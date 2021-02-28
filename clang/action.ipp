#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Expr.h>
#include <clang/AST/RawCommentList.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/LLVM.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Sema/Sema.h>
#include <clang/Sema/SemaConsumer.h>
#include <llvm/ADT/APSInt.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>

#include <fstream>
#include <optional>
#include <string_view>

#include "../common/ast.hpp"
#include "action.h"

namespace {

using clang::SourceLocation;
using clang::SourceManager;
using llvm::dyn_cast;
using llvm::StringRef;

template <typename T>
using optional = llvm::Optional<T>;
constexpr auto nullopt = llvm::None;
template <typename T>
auto make_optional(T value) {
  return llvm::Optional<T>(std::move(value));
}

inline SourceLocation locToLineBegin(SourceLocation loc, SourceManager &sm) {
  return loc.getLocWithOffset(1 - sm.getSpellingColumnNumber(loc));
}

inline bool max_one_linebreak(clang::SourceRange range, SourceManager &sm) {
  StringRef text = clang::Lexer::getSourceText(
      clang::CharSourceRange::getCharRange(range), sm, clang::LangOptions());
  auto first_break = text.find_first_of("\n\r");
  // If there is no linebreak or only in the last char, then we can't have more
  // than one linebreak.
  if (first_break == StringRef::npos || first_break == text.size() - 1)
    return true;
  auto second_break = text.find_first_of("\n\r", first_break + 1);
  // If this is an instance of \n\r or \r\n, treat it as a single linebreak and
  // keep scanning for the second one.
  if (second_break == first_break + 1 &&
      text[first_break] != text[second_break])
    second_break = text.find_first_of("\n\r", second_break + 1);
  return second_break == StringRef::npos;
}

// Returns false if unknown
bool evaluates_to_nullptr(const clang::Expr *expr,
                          const clang::ASTContext &context) {
  clang::Expr::EvalResult result;
  return expr->EvaluateAsRValue(result, context) && result.Val.isNullPointer();
}

std::string locToString(SourceLocation loc, SourceManager &sm) {
  auto location = sm.getPresumedLoc(loc);
  return (llvm::Twine(location.getFilename()) + ":" +
          std::to_string(location.getLine()))
      .str();
}

auto compare_first = [](const auto &a, const auto &b) {
  return a.first < b.first;
};

class CommentHandler : public clang::CommentHandler {
 public:
  CommentHandler(
      llvm::SmallVectorImpl<std::pair<SourceLocation, clang::RawComment>>
          &comments)
      : comments(&comments) {}

 private:
  llvm::SmallVectorImpl<std::pair<SourceLocation, clang::RawComment>> *comments;
  bool HandleComment(clang::Preprocessor &preprocessor,
                     clang::SourceRange comment) override {
    auto &sm = preprocessor.getSourceManager();
    clang::CommentOptions opts;
    opts.ParseAllComments = true;

    clang::RawComment raw_comment(sm, comment, opts, false);

    if (raw_comment.isInvalid() || raw_comment.isTrailingComment()) {
      previous = {};
      return false;
    }

    if (previous.isInvalid()) {
      // TODO: Check for configurable marker
      bool marker_is_set = true;
      if (!marker_is_set) return false;
    }

    clang::Token tok;
    if (clang::Lexer::getRawToken(comment.getEnd(), tok, sm,
                                  preprocessor.getLangOpts(), true)) {
      fprintf(stderr, "Failed to retrieve token");
      return false;
    }
    if (tok.is(clang::tok::comment) &&
        max_one_linebreak({comment.getEnd(), tok.getLocation()}, sm)) {
      if (previous.isInvalid()) previous = comment.getBegin();
    } else if (tok.isOneOf(clang::tok::eof, clang::tok::comment) ||
               !tok.isAtStartOfLine()) {
      previous = {};
    } else {
      auto next_line_location = locToLineBegin(tok.getLocation(), sm);
      if (previous.isValid()) {
        raw_comment =
            clang::RawComment(sm, {previous, comment.getEnd()}, opts, true);
      }
      comments->emplace_back(next_line_location, raw_comment);
      previous = {};
    }
    return false;
  }

 private:
  SourceLocation previous;
};

class i18nVisitor : public clang::RecursiveASTVisitor<i18nVisitor> {
 public:
  i18nVisitor(clang::Sema &sema_, clang::ASTContext &ctxt)
      : sema(&sema_), context(&ctxt) {}

  bool VisitRecordDecl(clang::RecordDecl *record) {
    if (record->isTemplated()) return true;

    for (auto *attr : record->specific_attrs<clang::AnnotateAttr>()) {
      if (attr->getAnnotation() == "mfk::i18n") {
        clang::VarDecl *begin_context = nullptr, *end_context = nullptr,
                       *begin_msgid = nullptr, *end_msgid = nullptr,
                       *begin_plural = nullptr, *end_plural = nullptr,
                       *begin_domain = nullptr, *end_domain = nullptr;
        for (auto *field : record->decls())
          if (auto *var = dyn_cast<clang::VarDecl>(field))
            for (auto *attr : var->specific_attrs<clang::AnnotateAttr>()) {
              if (attr->getAnnotation() == "mfk::i18n::context::begin")
                begin_context = var;
              else if (attr->getAnnotation() == "mfk::i18n::context::end")
                end_context = var;
              else if (attr->getAnnotation() == "mfk::i18n::singular::begin")
                begin_msgid = var;
              else if (attr->getAnnotation() == "mfk::i18n::singular::end")
                end_msgid = var;
              else if (attr->getAnnotation() == "mfk::i18n::plural::begin")
                begin_plural = var;
              else if (attr->getAnnotation() == "mfk::i18n::plural::end")
                end_plural = var;
              else if (attr->getAnnotation() == "mfk::i18n::domain::begin")
                begin_domain = var;
              else if (attr->getAnnotation() == "mfk::i18n::domain::end")
                end_domain = var;
              else
                continue;
              break;
            }
        optional<std::optional<std::string>> msgid = extract_string(begin_msgid,
                                                                    end_msgid),
                                             context = extract_string(
                                                 begin_context, end_context),
                                             domain = extract_string(
                                                 begin_domain, end_domain),
                                             plural = extract_string(
                                                 begin_plural, end_plural);
        if (!domain || !context || !msgid || !plural) return true;
        if (!*msgid) {
          printf("msgid can not be NULL\n");
          return true;
        }
        types.insert({record->getTypeForDecl()->getUnqualifiedDesugaredType(),
                      {*domain, *context, **msgid, *plural}});
        break;
      }
    }
    return true;
  }

  bool VisitFunctionDecl(clang::FunctionDecl *func) {
    if (func->isTemplated()) return true;

    for (auto *attr : func->specific_attrs<clang::AnnotateAttr>()) {
      if (attr->getAnnotation() == "mfk::i18n") {
        auto type =
            func->getReturnType().getTypePtr()->getUnqualifiedDesugaredType();
        /* if (types.contains(type)) { */ //< Would work in C++20, but we try to minimize C++20 features
        if (types.find(type) != types.end()) {
          functions.insert({func->getCanonicalDecl(), type});
          return true;
        } else {
          // Look at the arguments
          optional<std::size_t> msgid, context, domain, plural;
          for (auto *param : func->parameters())
            for (auto *attr : param->specific_attrs<clang::AnnotateAttr>()) {
              if (attr->getAnnotation() == "mfk::i18n::context::begin")
                context = param->getFunctionScopeIndex();
              else if (attr->getAnnotation() == "mfk::i18n::singular::begin")
                msgid = param->getFunctionScopeIndex();
              else if (attr->getAnnotation() == "mfk::i18n::plural::begin")
                plural = param->getFunctionScopeIndex();
              else if (attr->getAnnotation() == "mfk::i18n::domain::begin")
                domain = param->getFunctionScopeIndex();
              else
                continue;
              break;
            }
          if (!msgid) {
            fprintf(stderr, "Neither return value nor parameters marked\n");
            return true;
          }
          literalFunctions.insert({func->getCanonicalDecl(),
                                   {std::move(domain), std::move(context),
                                    *msgid, std::move(plural)}});
          return true;
        }
      }
    }
    return true;
  }

  bool VisitCallExpr(clang::CallExpr *call) {
    auto &sm = context->getSourceManager();
    auto callee = call->getDirectCallee();
    if (!callee) return true;
    callee = callee->getCanonicalDecl();

    auto callee_iter = functions.find(callee);
    if (callee_iter != functions.end()) {
      locations.insert(
          {callee_iter->second, locToLineBegin(call->getBeginLoc(), sm)});
      return true;
    }

    auto callee_iter_literal = literalFunctions.find(callee);
    if (callee_iter_literal != literalFunctions.end()) {
      bool success = false;
      auto msgid = get_argument_string(
          call, make_optional(callee_iter_literal->second.msgid));
      if (!msgid) return true;
      if (!*msgid) {
        fprintf(stderr, "Message ID can not be NULL");
        return true;
      }
      auto domain =
          get_argument_string(call, callee_iter_literal->second.domain);
      if (!domain) return true;
      auto context =
          get_argument_string(call, callee_iter_literal->second.context);
      if (!context) return true;
      auto plural =
          get_argument_string(call, callee_iter_literal->second.plural);
      if (!plural) return true;
      literalLocations.insert({locToLineBegin(call->getBeginLoc(), sm),
                               {*domain, *context, **msgid, *plural}});
      return true;
    }
    return true;
  }
  bool shouldVisitTemplateInstantiations() const { return true; }

 private:
  clang::Sema *sema;
  clang::ASTContext *context;
  clang::DiagnosticsEngine *diag = &context->getDiagnostics();

 public:
  struct Entry {
    std::optional<std::string> domain;
    std::optional<std::string> context;
    std::string msgid;
    std::optional<std::string> plural;
  };
  std::map<const clang::Type *, Entry> types;
  std::map<const clang::FunctionDecl *, const clang::Type *> functions;
  std::set<std::pair<const clang::Type *, SourceLocation>> locations;

  struct Indices {
    optional<std::size_t> domain;
    optional<std::size_t> context;
    std::size_t msgid;
    optional<std::size_t> plural;
  };
  std::map<const clang::FunctionDecl *, Indices> literalFunctions;
  std::map<SourceLocation, Entry> literalLocations;

 private:
  optional<std::optional<std::string>> get_argument_string(
      clang::CallExpr *call, optional<std::size_t> index) {
    if (!index) return ::make_optional(std::optional<std::string>());
    return extract_string(call->getArg(*index), nullptr);
  }

  optional<std::optional<std::string>> extract_string(
      clang::VarDecl *begin_decl, clang::VarDecl *end_decl) {
    clang::DeclRefExpr *begin_expr =
        begin_decl ? sema->BuildDeclRefExpr(begin_decl, begin_decl->getType(),
                                            clang::VK_LValue, SourceLocation{})
                   : nullptr;
    clang::DeclRefExpr *end_expr =
        end_decl ? sema->BuildDeclRefExpr(end_decl, end_decl->getType(),
                                          clang::VK_LValue, SourceLocation{})
                 : nullptr;
    return extract_string(begin_expr, end_expr);
  }
  optional<std::optional<std::string>> extract_string(clang::Expr *begin_expr,
                                                      clang::Expr *end_expr) {
    if (!begin_expr || evaluates_to_nullptr(begin_expr, *context))
      return ::make_optional(std::optional<std::string>());

    if (end_expr && evaluates_to_nullptr(end_expr, *context))
      end_expr = nullptr;

    if (begin_expr->getType().getCanonicalType() != str_type) {
      auto builder = diag->Report(begin_expr->getBeginLoc(), wrong_string_type);
      return nullopt;
    }

    if (end_expr && end_expr->getType().getCanonicalType() != str_type) {
      auto builder = diag->Report(end_expr->getBeginLoc(), wrong_string_type);
      return nullopt;
    }

    auto subscript_expr = [&] {
      auto expr =
          sema->CreateBuiltinArraySubscriptExpr(begin_expr, {}, str_index, {});
      assert(!expr.isInvalid());
      return expr.get();
    }();

    std::string str;
    std::int64_t len;
    if (end_expr) {
      auto len_value = [&] {
        auto expr =
            sema->CreateBuiltinBinOp({}, clang::BO_Sub, end_expr, begin_expr);
        assert(!expr.isInvalid());
        return expr.get()->getIntegerConstantExpr(*context);
      }();

      if (!len_value.hasValue()) return nullopt;
      if (len_value.getValue().isNegative()) {
        auto builder = diag->Report(end_expr->getBeginLoc(), wrong_string_type);
        return nullopt;
      }

      len = len_value.getValue().getExtValue();
      str.reserve(len);
    } else
      len = -1;

    llvm::APSInt index(context->getTypeSize(context->getSizeType()));
    for (; index < len; ++index) {
      str_index->setValue(*context, index);
      auto byte_value = subscript_expr->getIntegerConstantExpr(*context);
      if (!byte_value.hasValue()) {
        return nullopt;
      }
      if (len == -1 && byte_value.getValue().isNullValue()) break;
      str.push_back(byte_value.getValue().getExtValue());
    }
    return std::make_optional(str);
  }

 private:
  clang::QualType str_type = context->getConstType(
      context->getPointerType(context->getConstType(context->CharTy)));
  clang::IntegerLiteral *str_index = clang::IntegerLiteral::Create(
      *context, llvm::APSInt::get(0), context->getSizeType(), {});

  const unsigned int wrong_string_type = diag->getCustomDiagID(
      clang::DiagnosticsEngine::Error,
      "Field marked with i18n attribute does not have string type");

  const unsigned int wrong_order =
      diag->getCustomDiagID(clang::DiagnosticsEngine::Error,
                            "End of string occured before beginning of string");
};

void print_results_and_comments(
    llvm::MutableArrayRef<std::pair<SourceLocation, client::ast::Message>>
        messages,
    llvm::MutableArrayRef<std::pair<SourceLocation, clang::RawComment>>
        comments,
    StringRef file, const SourceManager &sm, clang::DiagnosticsEngine &diag) {
  auto comments_iter = comments.begin();
  const auto comments_end = comments.end();
  std::stable_sort(comments_iter, comments_end, compare_first);
  auto messages_iter = messages.begin();
  const auto messages_end = messages.end();
  std::stable_sort(messages_iter, messages_end, compare_first);

  StringRef outputFile;
  if (file == "-") {
    // TODO: Print error since we have no place to write our output
    return;
  } else {
    clang::SmallString<128> buffer = file;
    llvm::sys::path::replace_extension(buffer, "poc");
    outputFile = buffer;
  }
  std::ofstream stream(outputFile.str().c_str());

  for (; messages_end != messages_iter; ++messages_iter) {
    auto reference = messages_iter->second.comments.back();
    messages_iter->second.comments.pop_back();
    auto [iter, after] = std::equal_range(comments_iter, comments_end,
                                          *messages_iter, compare_first);
    if (iter != after) {
      auto comment_text = iter->second.getFormattedText(sm, diag);
      llvm::SmallVector<StringRef> lines;
      StringRef(comment_text).split(lines, '\n');
      for (auto line : lines) {
        messages_iter->second.comments.push_back(
            {{}, client::ast::Comment::Kind::Extracted, line.str()});
      }
    }
    comments_iter = after;
    messages_iter->second.comments.push_back(reference);
    messages_iter->second.translation.resize(messages_iter->second.plural ? 2
                                                                          : 1);
    stream << messages_iter->second;
  }
}

class SemaConsumer : public clang::SemaConsumer {
 protected:
  clang::Sema *sema;
  void InitializeSema(clang::Sema &sema) override { this->sema = &sema; }
  void ForgetSema() override { sema = nullptr; }
};

class i18nConsumer : public SemaConsumer {
 private:
  clang::CompilerInstance *ci;
  optional<llvm::StringRef> domain_filter;
  bool empty_domain;
  llvm::SmallVector<std::pair<SourceLocation, client::ast::Message>, 0>
      messages;
  llvm::SmallVector<std::pair<SourceLocation, clang::RawComment>, 0> comments;

 public:
  CommentHandler handler{comments};

  i18nConsumer(clang::CompilerInstance &ci,
               optional<llvm::StringRef> domain_filter, bool empty_domain)
      : ci(&ci), domain_filter(domain_filter), empty_domain(empty_domain) {
    ci.getPreprocessor().addCommentHandler(&handler);
  }
  ~i18nConsumer() {
    auto &sm = ci->getSourceManager();
    auto main_id = sm.getMainFileID();
    auto main_file = sm.getFileEntryForID(main_id);

    print_results_and_comments(messages, comments, main_file->getName(), sm,
                               ci->getDiagnostics());
  }
  void HandleTranslationUnit(clang::ASTContext &context) override {
    i18nVisitor visitor(*sema, context);
    visitor.TraverseDecl(context.getTranslationUnitDecl());

    {
      auto loc_iter = visitor.locations.begin(),
           loc_end = visitor.locations.end();
      for (auto &&[type, entry] : visitor.types) {
        client::ast::Message message;
        message.singular = entry.msgid;
        message.plural = entry.plural;
        message.context = entry.context;
        if (loc_iter != loc_end && loc_iter->first == type) {
          message.comments.push_back(
              {{}, client::ast::Comment::Kind::Reference, {}});
          do {
            message.comments[0].content =
                locToString(loc_iter->second, context.getSourceManager());
            messages.push_back({loc_iter->second, message});
          } while (++loc_iter != loc_end && loc_iter->first == type);
        } else
          messages.push_back({{}, std::move(message)});
      }

      for (auto &&location : visitor.literalLocations)
        if (match_domain(location.second.domain)) {
          auto loc = locToString(location.first, context.getSourceManager());
          client::ast::Message message;
          message.singular = location.second.msgid;
          message.plural = location.second.plural;
          message.context = location.second.context;
          message.comments.push_back(
              {{}, client::ast::Comment::Kind::Reference, loc});
          messages.emplace_back(location.first, std::move(message));
        }
    }
  }
  bool match_domain(const std::optional<std::string> &domain) {
    if (!empty_domain && !domain_filter) return true;  // We don't filter
    if (domain)
      return domain_filter && *domain == *domain_filter;
    else
      return empty_domain;
  }
};

class i18nAction : public clang::PluginASTAction {
 public:
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance &ci, StringRef file) override {
    return std::make_unique<i18nConsumer>(ci, domain_filter, empty_domain);
  }

 protected:
  // It would be cleaner to move functionality from i18nConsumer to
  // Begin/EndSourceFileAction, but they don't work for plugins.
  /* bool BeginSourceFileAction(clang::CompilerInstance &) override {} */
  /* void EndSourceFileAction() override {} */
  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    for (llvm::StringRef arg : args) {
      if (arg == "nodomain") {
        if (empty_domain) std::cerr << "Duplicate nodomain option ignored\n";
        empty_domain = true;
      } else if (arg.consume_front("domain=")) {
        if (domain_filter)
          std::cerr << "Duplicate domain option ignored\n";
        else
          domain_filter = arg;
      } else
        std::cerr << "I hate users.\n";
    }
    return true;
  }
  PluginASTAction::ActionType getActionType() override {
    return AddAfterMainAction;
  }
  clang::CompilerInstance *ci;

 private:
  optional<llvm::StringRef> domain_filter;
  bool empty_domain;
};

}  // namespace
