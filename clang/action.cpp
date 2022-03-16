#include "action.h"

#include "../common/ast.hpp"

#include <algorithm>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/RawCommentList.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Type.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Parse/ParseDiagnostic.h>
#include <clang/Sema/Sema.h>
#include <clang/Sema/SemaConsumer.h>
#include <filesystem>
#include <fstream>
#include <llvm/ADT/APSInt.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <optional>
#include <utility>

namespace {

template <typename T>
using optional         = llvm::Optional<T>;
constexpr auto nullopt = llvm::None;
template <typename T>
auto make_optional(T value) {
  return llvm::Optional<T>(std::move(value));
}

template <typename Int, Int Empty = Int(-1)>
class OptionalInt {
  Int value = Empty;

 public:
  constexpr OptionalInt() = default;
  constexpr OptionalInt(decltype(llvm::None)) {}
  constexpr OptionalInt(const OptionalInt &val): value(val.value) {}
  constexpr OptionalInt(Int val): value(val) { assert(*this); }
  constexpr OptionalInt &operator=(OptionalInt val) {
    value = val.value;
    return *this;
  }
  constexpr OptionalInt &operator=(Int val) {
    value = val;
    return *this;
  }
  constexpr Int &operator*() { return value; }
  constexpr Int operator*() const { return value; }
  constexpr bool operator==(OptionalInt other) const { return value == other.value; }
  constexpr bool operator!=(OptionalInt other) const { return value != other.value; }
  constexpr bool operator<(OptionalInt other) const { return value < other.value; }
  constexpr bool operator<=(OptionalInt other) const { return value <= other.value; }
  constexpr bool operator>(OptionalInt other) const { return value > other.value; }
  constexpr bool operator>=(OptionalInt other) const { return value >= other.value; }
  explicit operator bool() { return value != Empty; }
};

using clang::SourceLocation;
using clang::SourceManager;
using llvm::dyn_cast;
using llvm::StringRef;

inline SourceLocation locToLineBegin(SourceLocation loc, SourceManager &sm) {
  loc = sm.getSpellingLoc(loc);
  return loc.getLocWithOffset(1 - sm.getSpellingColumnNumber(loc));
}

inline bool max_one_linebreak(clang::SourceRange range, SourceManager &sm) {
  StringRef text   = clang::Lexer::getSourceText(clang::CharSourceRange::getCharRange(range), sm,
                                                 clang::LangOptions());
  auto first_break = text.find_first_of("\n\r");
  // If there is no linebreak or only in the last char, then we can't have more
  // than one linebreak.
  if (first_break == StringRef::npos || first_break == text.size() - 1) return true;
  auto second_break = text.find_first_of("\n\r", first_break + 1);
  // If this is an instance of \n\r or \r\n, treat it as a single linebreak and
  // keep scanning for the second one.
  if (second_break == first_break + 1 && text[first_break] != text[second_break])
    second_break = text.find_first_of("\n\r", second_break + 1);
  return second_break == StringRef::npos;
}

// Returns false if unknown
bool evaluates_to_nullptr(const clang::Expr *expr, const clang::ASTContext &context) {
  clang::Expr::EvalResult result;
  return expr->EvaluateAsRValue(result, context) && result.Val.isNullPointer();
}

std::string locToString(SourceLocation loc, SourceManager &sm,
                        const optional<std::filesystem::path> &base_path) {
  auto location = sm.getPresumedLoc(loc);
  auto str = (llvm::Twine(location.getFilename()) + ":" + std::to_string(location.getLine())).str();
  if (base_path) str = std::filesystem::path(str).lexically_relative(*base_path).generic_string();
  return str;
}

auto compare_first = [](const auto &a, const auto &b) { return a.first < b.first; };

class CommentHandler : public clang::CommentHandler {
 public:
  CommentHandler(llvm::SmallVectorImpl<std::pair<SourceLocation, clang::RawComment>> &comments,
                 optional<std::string> filter):
      comments(&comments),
      filter(std::move(filter)) {}

 private:
  llvm::SmallVectorImpl<std::pair<SourceLocation, clang::RawComment>> *comments;
  bool HandleComment(clang::Preprocessor &preprocessor, clang::SourceRange comment) override {
    auto &sm = preprocessor.getSourceManager();
    clang::CommentOptions opts;
    opts.ParseAllComments = true;

    clang::RawComment raw_comment(sm, comment, opts, false);

    if (raw_comment.isInvalid() || raw_comment.isTrailingComment()) {
      previous = {};
      return false;
    }

    if (previous.isInvalid() && filter) {
      auto text = raw_comment.getFormattedText(sm, preprocessor.getDiagnostics());
      if (!StringRef(text).ltrim().startswith(*filter)) return false;
    }

    clang::Token tok;
    if (clang::Lexer::getRawToken(comment.getEnd(), tok, sm, preprocessor.getLangOpts(), true)) {
      fprintf(stderr, "Failed to retrieve token");
      return false;
    }
    if (tok.is(clang::tok::comment)
        && max_one_linebreak({comment.getEnd(), tok.getLocation()}, sm)) {
      if (previous.isInvalid()) previous = comment.getBegin();
    } else if (tok.isOneOf(clang::tok::eof, clang::tok::comment) || !tok.isAtStartOfLine()) {
      previous = {};
    } else {
      auto next_line_location = locToLineBegin(tok.getLocation(), sm);
      if (previous.isValid()) {
        raw_comment = clang::RawComment(sm, {previous, comment.getEnd()}, opts, true);
      }
      comments->emplace_back(next_line_location, raw_comment);
      previous = {};
    }
    return false;
  }

 private:
  SourceLocation previous;
  optional<std::string> filter;
};

class i18nVisitor : public clang::RecursiveASTVisitor<i18nVisitor> {
 public:
  using Entry = std::pair<std::optional<std::string>, client::ast::Message>;
  llvm::StringMap<Entry> entries;

  i18nVisitor(clang::Sema &sema_, clang::ASTContext &ctxt): sema(sema_), context(&ctxt) {}

  bool VisitRecordDecl(clang::RecordDecl *record) {
    // We only accept the attribute for the definition. (This ensures that we can look at the fields
    // without trouble)
    if (record->isTemplated() || !record->isCompleteDefinition()) return true;

    for (auto *attr : record->specific_attrs<clang::AnnotateAttr>()) {
      if (attr->getAnnotation() == "mfk::i18n") {
        clang::VarDecl *begin_context = nullptr, *end_context = nullptr, *begin_msgid = nullptr,
                       *end_msgid = nullptr, *begin_plural = nullptr, *end_plural = nullptr,
                       *begin_domain = nullptr, *end_domain = nullptr;
        for (auto *field : record->decls())
          if (auto *var = dyn_cast<clang::VarDecl>(field))
            for (auto *attr : var->specific_attrs<clang::AnnotateAttr>()) {
              auto annot = attr->getAnnotation();
              if (annot.consume_front("mfk::i18n::")) {
                if (annot == "context::begin")
                  begin_context = var;
                else if (annot == "context::end")
                  end_context = var;
                else if (annot == "singular::begin")
                  begin_msgid = var;
                else if (annot == "singular::end")
                  end_msgid = var;
                else if (annot == "plural::begin")
                  begin_plural = var;
                else if (annot == "plural::end")
                  end_plural = var;
                else if (annot == "domain::begin")
                  begin_domain = var;
                else if (annot == "domain::end")
                  end_domain = var;
                else {
                  printf("Unexpected i18n scoped annotation, skipping function\n");
                  return true;
                }
                break;
              }
            }
        auto msgid   = extract_string(begin_msgid, end_msgid),
             context = extract_string(begin_context, end_context),
             domain  = extract_string(begin_domain, end_domain),
             plural  = extract_string(begin_plural, end_plural);
        if (!domain || !context || !msgid || !plural) return true;
        if (!*msgid) {
          printf("msgid can not be NULL\n");
          return true;
        }
        types.insert({record->getTypeForDecl()->getUnqualifiedDesugaredType(),
                      &addEntry(*domain, *context, **msgid, *plural)});
        return true;
      }
    }

    // We only reach this point if the class does not have the attribute. But maybe a base class was
    // marked?
    if (auto *cxx_record = dyn_cast<clang::CXXRecordDecl>(record)) {
      for (clang::CXXBaseSpecifier &base : cxx_record->bases()) {
        auto base_iter = types.find(base.getType().getTypePtr()->getUnqualifiedDesugaredType());
        if (base_iter != types.end()) {
          types.insert(
              {record->getTypeForDecl()->getUnqualifiedDesugaredType(), base_iter->second});
          return true;
        }
      }
    }

    return true;
  }

  bool VisitFunctionDecl(clang::FunctionDecl *func) {
    if (func->isTemplated()) return true;

    for (auto *attr : func->specific_attrs<clang::AnnotateAttr>()) {
      if (attr->getAnnotation() == "mfk::i18n") {
        auto type = func->getReturnType().getTypePtr()->getUnqualifiedDesugaredType();
        /* if (types.contains(type)) { */ //< Would work in C++20, but we try
                                          // to minimize C++20 features
        if (auto iter = types.find(type); iter != types.end()) {
          functions.insert({func->getCanonicalDecl(), iter->second});
          return true;
        } else {
          // Look at the arguments
          OptionalInt<unsigned short> msgid, context, domain, plural;
          for (auto *param : func->parameters())
            for (auto *arg_attr : param->specific_attrs<clang::AnnotateAttr>()) {
              if (arg_attr->getAnnotation() == "mfk::i18n::context::begin")
                context = param->getFunctionScopeIndex();
              else if (arg_attr->getAnnotation() == "mfk::i18n::singular::begin")
                msgid = param->getFunctionScopeIndex();
              else if (arg_attr->getAnnotation() == "mfk::i18n::plural::begin")
                plural = param->getFunctionScopeIndex();
              else if (arg_attr->getAnnotation() == "mfk::i18n::domain::begin")
                domain = param->getFunctionScopeIndex();
              else
                continue;
              break;
            }
          if (!msgid) {
            fprintf(stderr, "Neither return value nor parameters marked\n");
            return true;
          }
          literalFunctions.insert(
              {func->getCanonicalDecl(),
               Indices{std::move(domain), std::move(context), *msgid, std::move(plural)}});
          return true;
        }
      }
    }
    return true;
  }

  bool VisitCallExpr(clang::CallExpr *call) {
    auto &sm    = context->getSourceManager();
    auto callee = call->getDirectCallee();
    if (!callee) return true;
    callee = callee->getCanonicalDecl();

    auto callee_iter = functions.find(callee);
    if (callee_iter != functions.end()) {
      locations.insert({locToLineBegin(call->getBeginLoc(), sm), callee_iter->second});
      return true;
    }

    auto callee_iter_literal = literalFunctions.find(callee);
    if (callee_iter_literal != literalFunctions.end()) {
      auto &indices = callee_iter_literal->second;
      auto msgid    = get_argument_string(call, indices.msgid),
           domain   = get_argument_string(call, indices.domain),
           context  = get_argument_string(call, indices.context),
           plural   = get_argument_string(call, indices.plural);
      if (!msgid || !domain || !context || !plural) return true;
      if (!*msgid) {
        fprintf(stderr, "Message ID can not be NULL");
        return true;
      }
      locations.insert({locToLineBegin(call->getBeginLoc(), sm),
                        &addEntry(*domain, *context, **msgid, *plural)});
      return true;
    }
    return true;
  }
  bool shouldVisitTemplateInstantiations() const { return true; }

 private:
  Entry &addEntry(std::optional<std::string> domain, std::optional<std::string> context,
                  std::string msgid, std::optional<std::string> plural) {
    // If context is present and ends with '\4' + msgid, then we strip the later part.
    // (This is mostly to catch gettext's macro implementation for handling the context.)
    // If no context is present but the msgid contains '\4', assume that someone tries to imlememnt
    // contexts manually and use the first part of the msgid as context
    if (context) {
      auto &ctxt = *context;
      if (ctxt.size() > msgid.size() && ctxt[ctxt.size() - msgid.size() - 1] == '\4'
          && std::equal(ctxt.end() - msgid.size(), ctxt.end(),
                        msgid.begin())) // ctxt.ends_with(msgid) if C++20 is available :dreamy:
        ctxt.resize(ctxt.size() - 1 - msgid.size());
    } else if (auto index = msgid.find_first_of('\4'); index != std::string::npos) {
      context.emplace(msgid, 1, index);
      msgid = msgid.substr(index + 1);
    }

    auto &entry = entries[((domain ? *domain + llvm::Twine('\1') : llvm::Twine())
                           + (context ? *context + llvm::Twine('\4') : llvm::Twine()) + msgid)
                              .str()];
    if (entry.second.singular.empty()) {
      entry.first           = std::move(domain);
      entry.second.context  = std::move(context);
      entry.second.singular = std::move(msgid);
      entry.second.plural   = std::move(plural);
    }
    return entry;
  }

  clang::Sema &sema;
  clang::ASTContext *context;
  clang::DiagnosticsEngine *diag = &context->getDiagnostics();

  struct Indices {
    OptionalInt<unsigned short> domain;
    OptionalInt<unsigned short> context;
    unsigned short msgid;
    OptionalInt<unsigned short> plural;
  };
  std::map<const clang::FunctionDecl *, Indices> literalFunctions;
  std::map<const clang::FunctionDecl *, Entry *> functions;
  std::map<const clang::Type *, Entry *> types;

 public:
  std::set<std::pair<SourceLocation, Entry *>> locations;

 private:
  optional<std::optional<std::string>> get_argument_string(clang::CallExpr *call,
                                                           OptionalInt<unsigned short> index) {
    if (!index) return ::make_optional(std::optional<std::string>());
    return extract_string(call->getArg(*index), nullptr);
  }

  optional<std::optional<std::string>> extract_string(clang::VarDecl *begin_decl,
                                                      clang::VarDecl *end_decl) {
    clang::DeclRefExpr *begin_expr = begin_decl
                                         ? sema.BuildDeclRefExpr(begin_decl, begin_decl->getType(),
                                                                 clang::VK_LValue, SourceLocation{})
                                         : nullptr;
    clang::DeclRefExpr *end_expr   = end_decl
                                         ? sema.BuildDeclRefExpr(end_decl, end_decl->getType(),
                                                                 clang::VK_LValue, SourceLocation{})
                                         : nullptr;
    return extract_string(begin_expr, end_expr);
  }
  optional<std::optional<std::string>> extract_string(clang::Expr *begin_expr,
                                                      clang::Expr *end_expr) {
    if (!begin_expr || evaluates_to_nullptr(begin_expr, *context))
      return ::make_optional(std::optional<std::string>());

    if (end_expr && evaluates_to_nullptr(end_expr, *context)) end_expr = nullptr;

    {
      auto type = begin_expr->getType().getCanonicalType().getTypePtr();
      if (type != str_type && type != u8str_type) {
        auto builder = diag->Report(begin_expr->getBeginLoc(), wrong_string_type);
        return nullopt;
      }
    }

    if (end_expr) {
      auto type = end_expr->getType().getCanonicalType().getTypePtr();
      if (type != str_type && type != u8str_type) {
        auto builder = diag->Report(end_expr->getBeginLoc(), wrong_string_type);
        return nullopt;
      }
    }

    auto subscript_expr = [&] {
      auto expr = sema.CreateBuiltinArraySubscriptExpr(begin_expr, {}, str_index, {});
      assert(!expr.isInvalid());
      return expr.get();
    }();

    std::string str;
    std::int64_t len;
    if (end_expr) {
      auto len_value = [&] {
        auto expr = sema.CreateBuiltinBinOp({}, clang::BO_Sub, end_expr, begin_expr);
        assert(!expr.isInvalid());
        return expr.get()->getIntegerConstantExpr(*context);
      }();

      if (!len_value.hasValue()) return nullopt;
      if (len_value.getValue().isNegative()) {
        auto builder = diag->Report(end_expr->getBeginLoc(), wrong_order);
        return nullopt;
      }

      len = len_value.getValue().getExtValue();
      str.reserve(len);
    } else
      len = -1;

    llvm::APSInt index(context->getTypeSize(context->getSizeType()));
    for (; len == -1 || index < len; ++index) {
      str_index->setValue(*context, index);
      auto byte_value = subscript_expr->getIntegerConstantExpr(*context);
      if (!byte_value.hasValue()) { return nullopt; }
      if (len == -1 && byte_value.getValue().isNullValue()) break;
      str.push_back(byte_value.getValue().getExtValue());
    }
    return std::make_optional(str);
  }

 private:
  const clang::Type *str_type =
      context->getPointerType(context->getConstType(context->CharTy)).getTypePtr();
  const clang::Type *u8str_type =
      context->getPointerType(context->getConstType(context->Char8Ty)).getTypePtr();
  clang::IntegerLiteral *str_index =
      clang::IntegerLiteral::Create(*context, llvm::APSInt::get(0), context->getSizeType(), {});

  const unsigned int wrong_string_type =
      diag->getCustomDiagID(clang::DiagnosticsEngine::Error,
                            "Field marked with i18n attribute does not have string type");

  const unsigned int wrong_order = diag->getCustomDiagID(
      clang::DiagnosticsEngine::Error, "End of string occured before beginning of string");
};

class SemaConsumer : public clang::SemaConsumer {
 protected:
  clang::Sema *sema;
  void InitializeSema(clang::Sema &sema) override { this->sema = &sema; }
  void ForgetSema() override { sema = nullptr; }
};

class i18nConsumer : public SemaConsumer {
 private:
  clang::CompilerInstance *ci;
  optional<std::string> domain_filter;
  bool empty_domain;
  optional<std::filesystem::path> base_path;
  optional<std::string> output;
  llvm::SmallVector<std::pair<SourceLocation, client::ast::Message>, 0> messages;
  llvm::SmallVector<std::pair<SourceLocation, clang::RawComment>, 0> comments;

  struct PragmaI18NHandler : public clang::PragmaHandler {
    explicit PragmaI18NHandler(i18nConsumer *consumer): PragmaHandler("i18n"), consumer(consumer) {}
    void HandlePragma(clang::Preprocessor &PP, clang::PragmaIntroducer intro,
                      clang::Token &i18nTok) {
      clang::Token tok;
      PP.LexUnexpandedToken(tok);
      auto info = tok.getIdentifierInfo();
      if (info && info->isStr("domain")) {
        PP.Lex(tok);
        if (tok.isNot(clang::tok::l_paren)) {
          PP.Diag(tok.getLocation(), clang::diag::warn_pragma_expected_lparen) << "mfk i18n domain";
          return;
        }
        std::string domain;
        if (!PP.LexStringLiteral(tok, domain, "#pragma mfk i18n domain", true)) return;
        if (tok.isNot(clang::tok::r_paren)) {
          PP.Diag(tok.getLocation(), clang::diag::warn_pragma_expected_rparen) << "mfk i18n domain";
          return;
        }
        if (consumer->empty_domain) {
          if (consumer->domain_filter) {
            if (*consumer->domain_filter != domain)
              ; // TODO: WARNING?
          } else
            consumer->domain_filter = domain;
        } else {
          if (consumer->domain_filter) {
            if (*consumer->domain_filter == domain) consumer->empty_domain = true;
          } else
            consumer->domain_filter = domain;
        }
      } else {
        PP.Diag(tok.getLocation(), clang::diag::warn_pragma_expected_identifier) << "mfk i18n";
        return;
      }
    }
    i18nConsumer *consumer;
  } pragmaHandler{this};

 public:
  CommentHandler handler;

  i18nConsumer(clang::CompilerInstance &ci, optional<std::string> domain_filter, bool empty_domain,
               optional<std::string> comment_filter, optional<std::filesystem::path> base_path,
               optional<std::string> output):
      ci(&ci),
      domain_filter(std::move(domain_filter)), empty_domain(empty_domain),
      base_path(std::move(base_path)), output(std::move(output)),
      handler(comments, std::move(comment_filter)) {
    ci.getPreprocessor().addCommentHandler(&handler);
    ci.getPreprocessor().AddPragmaHandler("mfk", &pragmaHandler);
  }
  ~i18nConsumer() { ci->getPreprocessor().RemovePragmaHandler("mfk", &pragmaHandler); }

  void HandleTranslationUnit(clang::ASTContext &context) override {
    i18nVisitor visitor(*sema, context);
    visitor.TraverseDecl(context.getTranslationUnitDecl());

    auto &sm       = ci->getSourceManager();
    auto &diag     = ci->getDiagnostics();
    auto main_file = sm.getFileEntryForID(sm.getMainFileID());

    auto comments_iter      = comments.begin();
    const auto comments_end = comments.end();
    std::stable_sort(comments_iter, comments_end, compare_first);

    for (auto &&[location, entry] : visitor.locations)
      if (match_domain(entry->first)) {
        auto [iter, after] = std::equal_range(comments_iter, comments_end,
                                              std::make_pair(location, std::ignore), compare_first);
        std::optional<std::string> combined;
        if (iter != after) { combined = iter->second.getFormattedText(sm, diag); }
        entry->second.extractedComments.emplace_back(locToString(location, sm, base_path),
                                                     combined);
      }

    std::ofstream stream;
    if (output)
      stream = std::ofstream(output->c_str());
    else {
      StringRef out_file = ci->getFrontendOpts().OutputFile;
      if (out_file.empty()) out_file = main_file->getName();
      if (out_file == "-") {
        std::cerr << "Unable to derive i18n output file name\n";
        return;
      }
      clang::SmallString<128> output_file = out_file;
      stream = std::ofstream((llvm::Twine(out_file) + ".poc").str().c_str());
    }

    for (auto &&val : visitor.entries)
      if (match_domain(val.getValue().first)) {
        auto &msg = val.getValue().second;
        msg.translation.resize(msg.plural ? 2 : 1);
        stream << msg << '\n';
      }
  }
  bool match_domain(const std::optional<std::string> &domain) {
    if (domain)
      return domain_filter && *domain == *domain_filter;
    else
      return empty_domain || !domain_filter;
  }
};

} // namespace

std::unique_ptr<clang::ASTConsumer> i18nAction::CreateASTConsumer(clang::CompilerInstance &ci,
                                                                  StringRef) {
  return std::make_unique<i18nConsumer>(ci, domain_filter, empty_domain, std::move(comment_filter),
                                        std::move(base_path), std::move(output));
}

bool i18nAction::ParseArgs(const std::vector<std::string> &args) {
  for (StringRef arg : args) {
    if (arg == "nodomain") {
      if (empty_domain) std::cerr << "Duplicate nodomain option ignored\n";
      empty_domain = true;
    } else if (arg.consume_front("domain=")) {
      if (domain_filter)
        std::cerr << "Duplicate domain option ignored\n";
      else
        domain_filter = arg.str();
    } else if (arg.consume_front("comment=")) {
      if (comment_filter)
        std::cerr << "Duplicate comment option ignored\n";
      else
        comment_filter = arg.str();
    } else if (arg.consume_front("basepath=")) {
      if (base_path)
        std::cerr << "Duplicate base path ignored\n";
      else
        base_path = std::filesystem::weakly_canonical(arg.str());
    } else if (arg.consume_front("o=")) {
      if (output)
        std::cerr << "Duplicate output path ignored\n";
      else
        output = arg.str();
    } else
      std::cerr << "Unknown plugin option passed\n";
  }
  return true;
}
