#include <clang/AST/ASTContext.h>
#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/Basic/AttributeCommonInfo.h>
#include <clang/Sema/ParsedAttr.h>
#include <clang/Sema/Sema.h>
#include <clang/Sema/SemaDiagnostic.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Attributes.h>
#include <string>
#include <utility>

namespace {

using clang::isa, clang::dyn_cast;

constexpr struct {
  const char *spelling;
  const char *annotation;
} Mapping[]{{"mfk::i18n", "mfk::i18n"},
            {"mfk::i18n_domain_begin", "mfk::i18n::domain::begin"},
            {"mfk::i18n_domain_end", "mfk::i18n::domain::end"},
            {"mfk::i18n_context_begin", "mfk::i18n::context::begin"},
            {"mfk::i18n_context_end", "mfk::i18n::context::end"},
            {"mfk::i18n_singular_begin", "mfk::i18n::singular::begin"},
            {"mfk::i18n_singular_end", "mfk::i18n::singular::end"},
            {"mfk::i18n_plural_begin", "mfk::i18n::plural::begin"},
            {"mfk::i18n_plural_end", "mfk::i18n::plural::end"}};

template <int Index>
struct AttrInfo : public clang::ParsedAttrInfo {
  static constexpr Spelling OurSpelling = {clang::ParsedAttr::AS_CXX11, Mapping[Index].spelling};
  AttrInfo() { Spellings = OurSpelling; }

  bool diagAppertainsToDecl(clang::Sema &sema, const clang::ParsedAttr &attr,
                            const clang::Decl *declaration) const override {
    if constexpr (Index == 0) {
      if (isa<clang::FunctionDecl>(declaration) || isa<clang::RecordDecl>(declaration)) return true;
      sema.Diag(attr.getLoc(), clang::diag::warn_attribute_wrong_decl_type_str)
          << attr << "functions or classes";
      return false;
    } else {
      if (auto *decl = dyn_cast<clang::VarDecl>(declaration))
        if (isa<clang::ParmVarDecl>(decl) || (decl->isStaticDataMember() && decl->isConstexpr()))
          return true;
      sema.Diag(attr.getLoc(), clang::diag::warn_attribute_wrong_decl_type_str)
          << attr << "static constexpr data members or function arguments";
      return false;
    }
  }

  AttrHandling handleDeclAttribute(clang::Sema &sema, clang::Decl *declaration,
                                   const clang::ParsedAttr &attr) const override {
    declaration->addAttr(clang::AnnotateAttr::Create(sema.Context, Mapping[Index].annotation,
                                                     nullptr, 0, attr.getRange()));
    return AttributeApplied;
  }
};

template <typename>
struct Register;

template <>
struct Register<std::index_sequence<>> {};

template <std::size_t I, std::size_t... Is>
struct Register<std::index_sequence<I, Is...>> : Register<std::index_sequence<Is...>> {
  [[maybe_unused]] clang::ParsedAttrInfoRegistry::Add<AttrInfo<I>> X{Mapping[I].spelling, "Attribute for i18n"};
};

[[maybe_unused]] Register<std::make_index_sequence<sizeof(Mapping) / sizeof(*Mapping)>> registrations;

} // namespace
