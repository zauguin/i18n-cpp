#ifndef I18N_BASE_HPP
#define I18N_BASE_HPP

#include <algorithm>
#include <cassert>
#include <cstdint>

namespace mfk::i18n {

// Char should be char, char8_t, char16_t or char32_t.
template <typename Char, std::size_t Length>
struct CompileTimeString {
  constexpr CompileTimeString() = default;
  constexpr CompileTimeString(const Char (&literal)[Length + 1]) {
    for (int i = 0; i != Length + 1; ++i)
      str[i] = literal[i];
    if (std::is_constant_evaluated()) {
      if (*end()) throw "Initializer for CompileTimeString should be NULL-terminated";
    } else {
      assert(!*end());
    }
  }
  constexpr Char *begin() { return str; }
  constexpr const Char *begin() const { return str; }
  constexpr Char *end() { return str + Length; }
  constexpr const Char *end() const { return str + Length; }

  using char_type                     = Char;
  Char str[Length + 1]                = {0};
  static constexpr std::size_t length = Length;
};

/* We use CompileTimeString<T, -1> to indicate non-existance */
template <typename Char>
struct CompileTimeString<Char, std::size_t(-1)> {
  constexpr CompileTimeString() = default;
  constexpr CompileTimeString(std::nullptr_t) {}

  constexpr Char *begin() { return nullptr; }
  constexpr const Char *begin() const { return nullptr; }
  constexpr Char *end() { return nullptr; }
  constexpr const Char *end() const { return nullptr; }

  using char_type                     = Char;
  static constexpr std::size_t length = -1;
  static constexpr char *str          = nullptr;
};

template <typename Char, std::size_t Length>
CompileTimeString(const Char (&)[Length]) -> CompileTimeString<Char, Length - 1>;

namespace detail {
// If one string does not exists, return the other (with no separator)
template <typename Char, std::size_t Length1, std::size_t Length2>
constexpr CompileTimeString<Char, Length1 + 1 + Length2>
join_with_separator(CompileTimeString<Char, Length1> str1, Char sep,
                    CompileTimeString<Char, Length2> str2) {
  if constexpr (Length1 == -1) return str2;
  if constexpr (Length2 == -1) return str1;
  CompileTimeString<Char, Length1 + 1 + Length2> result;
  auto result_iter = std::copy(str1.begin(), str1.end(), result.begin());
  *result_iter++   = sep;
  result_iter      = std::copy(str2.begin(), str2.end(), result_iter);
  *result_iter     = '\0';
  /* assert(result_iter == result.end()); */
  return result;
}

} // namespace detail

template <CompileTimeString Domain = CompileTimeString<char, std::size_t(-1)>()>
class I18NString;
template <CompileTimeString Domain = CompileTimeString<char, std::size_t(-1)>()>
class I18NPluralString;

template <CompileTimeString Domain, CompileTimeString Context, CompileTimeString Msgid,
          CompileTimeString Plural>
class [[mfk::i18n]] CompileTimeI18NString :
    public std::conditional_t<Plural.length == -1, I18NString<Domain>, I18NPluralString<Domain>> {
  using Base =
      std::conditional_t<Plural.length == -1, I18NString<Domain>, I18NPluralString<Domain>>;
  static_assert(Msgid.length != -1, "Singular string required");
  static constexpr auto idStorage =
      detail::join_with_separator(detail::join_with_separator(Context, '\4', Msgid), '\0', Plural);

 public:
  constexpr CompileTimeI18NString() requires(Plural.length == -1):
      Base(idStorage.begin(), singular_begin) {}
  constexpr CompileTimeI18NString() requires(Plural.length != -1):
      Base(idStorage.begin(), singular_begin, plural_begin) {}

 private:
  static constexpr const char *domain_begin [[mfk::i18n_domain_begin]] =
      Domain.length == -1 ? nullptr : Domain.str;
  static constexpr const char *domain_end [[mfk::i18n_domain_end]] =
      domain_begin ? domain_begin + Domain.length : nullptr;
  static constexpr const char *context_begin [[mfk::i18n_context_begin]] =
      Context.length == -1 ? nullptr : idStorage.str;
  static constexpr const char *context_end [[mfk::i18n_context_end]] =
      context_begin ? context_begin + Context.length : nullptr;
  static constexpr const char *singular_begin [[mfk::i18n_singular_begin]] =
      idStorage.str + (Context.length + 1); // context_end+1 or idStorage.str
  static constexpr const char *singular_end [[mfk::i18n_singular_end]] =
      singular_begin + Msgid.length;
  static constexpr const char *plural_begin [[mfk::i18n_plural_begin]] =
      Plural.length == -1 ? nullptr : singular_end + 1;
  static constexpr const char *plural_end [[mfk::i18n_plural_end]] =
      plural_begin ? plural_begin + Plural.length : nullptr;
};

} // namespace mfk::i18n

#endif
