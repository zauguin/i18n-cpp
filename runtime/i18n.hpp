#ifndef I18N_HPP
#define I18N_HPP

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <libintl.h>
/* #include <type_traits> */

#include "i18n/base.hpp"

#include <tuple>
#include <version>

#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907
  #include <format>
namespace fmtstd = std;
#else
  #include <fmt/format.h>
namespace fmtstd = fmt;
#endif

namespace mfk::i18n {

class SmallI18NStringCrossDomain {
 public:
  explicit constexpr SmallI18NStringCrossDomain(const char *domain, const char *msgid):
      domain(domain), msgid(msgid) {}
  operator const char *() {
    const char *translated = dgettext(domain, msgid);
    if (translated != msgid) return translated;
    translated = strchr(msgid, '\4');
    return translated ? translated + 1 : msgid;
  }
  operator std::string_view() { return static_cast<const char *>(*this); }

  template <typename... Args>
  auto operator()(Args &&...args) {
    return fmtstd::format(std::string_view(*this), std::forward<Args>(args)...);
  }

 protected:
  const char *domain;
  const char *msgid;
};

class I18NStringCrossDomain : public SmallI18NStringCrossDomain {
 public:
  explicit constexpr I18NStringCrossDomain(const char *domain, const char *msgid,
                                           const char *singular):
      SmallI18NStringCrossDomain(domain, msgid),
      singular(singular) {}
  operator const char *() {
    const char *translated = dgettext(domain, msgid);
    return translated == msgid ? singular : translated;
  }
  operator std::string_view() { return static_cast<const char *>(*this); }

  template <typename... Args>
  auto operator()(Args &&...args) {
    return fmtstd::format(std::string_view(*this), std::forward<Args>(args)...);
  }

 protected:
  const char *singular;
};

template <CompileTimeString Domain>
class SmallI18NString {
 public:
  explicit constexpr SmallI18NString(const char *msgid): msgid(msgid) {}
  operator SmallI18NStringCrossDomain() {
    return SmallI18NStringCrossDomain(Domain.begin(), msgid);
  }
  operator const char *() {
    if constexpr (Domain.length == -1) {
      const char *translated = gettext(msgid);
      if (translated != msgid) return translated;
      translated = strchr(msgid, '\4');
      return translated ? translated + 1 : msgid;
    } else
      return SmallI18NStringCrossDomain(*this);
  }

  template <typename... Args>
  auto operator()(Args &&...args) {
    return fmtstd::format(std::string_view(*this), std::forward<Args>(args)...);
  }

 protected:
  const char *msgid;
};

template <CompileTimeString Domain>
class I18NString : public SmallI18NString<Domain> {
 public:
  explicit constexpr I18NString(const char *msgid, const char *singular):
      SmallI18NString<Domain>(msgid), singular(singular) {}
  operator I18NStringCrossDomain() {
    return I18NStringCrossDomain(Domain.begin(), this->msgid, singular);
  }
  operator const char *() {
    if constexpr (Domain.length == -1) {
      const char *translated = gettext(this->msgid);
      return translated == this->msgid ? singular : translated;
    } else
      return I18NStringCrossDomain(*this);
  }
  operator std::string_view() { return static_cast<const char *>(*this); }

  template <typename... Args>
  auto operator()(Args &&...args) {
    return fmtstd::format(std::string_view(*this), std::forward<Args>(args)...);
  }

 protected:
  const char *singular;
};

class SmallI18NPluralStringCrossDomain {
 public:
  explicit constexpr SmallI18NPluralStringCrossDomain(const char *domain, const char *msgid):
      domain(domain), msgid(msgid) {}
  const char *operator[](unsigned long n) {
    const char *translated = dngettext(domain, msgid, msgid + std::strlen(msgid) + 1, n);
    if (translated != msgid) return translated;
    translated = strchr(msgid, '\4');
    return translated ? translated + 1 : msgid;
  }

  template <std::convertible_to<unsigned long> First, typename... Args>
  auto operator()(First &&first, Args &&...args) {
    return fmtstd::format((*this)[first], std::forward<First>(first), std::forward<Args>(args)...);
  }

 protected:
  const char *domain;
  const char *msgid;
};

class I18NPluralStringCrossDomain : public SmallI18NPluralStringCrossDomain {
 public:
  explicit constexpr I18NPluralStringCrossDomain(const char *domain, const char *msgid,
                                                 const char *singular, const char *plural):
      SmallI18NPluralStringCrossDomain(domain, msgid),
      singular(singular), plural(plural) {}
  const char *operator[](unsigned long n) {
    const char *translated = dngettext(domain, msgid, plural, n);
    return translated == msgid ? singular : translated;
  }

  template <std::convertible_to<unsigned long> First, typename... Args>
  auto operator()(First &&first, Args &&...args) {
    return fmtstd::format((*this)[first], std::forward<First>(first), std::forward<Args>(args)...);
  }

 protected:
  const char *singular;
  const char *plural;
};

template <CompileTimeString Domain>
class SmallI18NPluralString {
 public:
  explicit constexpr SmallI18NPluralString(const char *msgid): msgid(msgid) {}
  operator SmallI18NPluralStringCrossDomain() {
    return SmallI18NPluralStringCrossDomain(Domain.begin(), msgid);
  }
  const char *operator[](unsigned long n) {
    if constexpr (Domain.length == -1) {
      const char *translated = ngettext(msgid, msgid + std::strlen(msgid) + 1, n);
      if (translated != msgid) return translated;
      translated = strchr(msgid, '\4');
      return translated ? translated + 1 : msgid;
    } else
      return SmallI18NPluralStringCrossDomain(*this)[n];
  }

  template <std::convertible_to<unsigned long> First, typename... Args>
  auto operator()(First &&first, Args &&...args) {
    return fmtstd::format((*this)[first], std::forward<First>(first), std::forward<Args>(args)...);
  }

 protected:
  const char *msgid;
};

template <CompileTimeString Domain>
class I18NPluralString : public SmallI18NPluralString<Domain> {
 public:
  explicit constexpr I18NPluralString(const char *msgid, const char *singular, const char *plural):
      SmallI18NPluralString<Domain>(msgid), singular(singular), plural(plural) {}
  operator I18NPluralStringCrossDomain() {
    return I18NPluralStringCrossDomain(Domain.begin(), this->msgid, singular, plural);
  }
  const char *operator[](unsigned long n) {
    if constexpr (Domain.length == -1) {
      const char *translated = ngettext(this->msgid, plural, n);
      return translated == this->msgid ? singular : translated;
    } else
      return I18NPluralStringCrossDomain(*this)[n];
  }

  template <std::convertible_to<unsigned long> First, typename... Args>
  auto operator()(First &&first, Args &&...args) {
    return fmtstd::format((*this)[first], std::forward<First>(first), std::forward<Args>(args)...);
  }

 protected:
  const char *singular;
  const char *plural;
};

namespace detail {
// The context part uses quite simple escaping rules:
// Everything can be escaped by prepending a \ and the first
// unescaped | limits the context part.  All unescaped
// parens get discarded. Additionally a | counts as escaped
// if the last previous paren was a (. Especially this implies that | enclosed
// in parens are escaped if all parens are balanced.
template <typename Char>
constexpr std::tuple<const Char *, std::size_t> measure_context(const Char *const begin,
                                                                const Char *const end) {
  std::size_t count = 0;
  bool grouped      = false;
  for (auto iter = begin; end != iter; ++iter) {
    if (*iter == '\\' && iter + 1 != end)
      ++iter;
    else if (*iter == '(')
      grouped = true;
    else if (*iter == ')')
      grouped = false;
    else if (*iter == '|' && !grouped)
      return std::make_tuple(iter + 1, count);
    ++count;
  }
  return std::make_tuple(begin, -1);
}

// Here we take a shortcut: We don't have to track if a | is grouped, since we
// know the length we can just check if we need more characters
template <typename Char, std::size_t Length>
constexpr CompileTimeString<Char, Length> extract_context(const Char *iter) {
  if (Length == -1) return {};
  CompileTimeString<Char, Length> result;
  for (auto &c : result) {
    if (*iter == '\\') ++iter;
    c = *iter++;
  }
  *result.end() = '\0';
  return result;
}

// The main part uses more complicated escaping since we want to allow normal
// interpretation of parens as often as possible.
//
// PRECONDITION: !*end. Technically *end != 's' && *end != ')' is enough.
template <std::size_t Singular, std::size_t Plural, typename Char>
constexpr std::tuple<CompileTimeString<Char, Singular>, CompileTimeString<Char, Plural>>
extract_singular_plural(const Char *iter, const Char *const end) {
  std::tuple<CompileTimeString<Char, Singular>, CompileTimeString<Char, Plural>> result;
  Char *out_singular = std::get<0>(result).begin(), *out_plural = std::get<1>(result).begin();
  if (Plural == std::size_t(-1)) {
    while (end != iter) {
      if (*iter == '\\') ++iter;
      *out_singular++ = *iter++;
    }
    if (out_singular != std::get<0>(result).end()) throw "Assertion failed";
  }
  bool grouped = false;
  // The following two are only defined if grouped is true.
  Char *try_singular, *try_plural;
  for (; end != iter; ++iter) {
    if (grouped) {
      // If we find another '(', we treat this as a new group and assume that
      // the previous '(' should be written in both forms. Similar for a
      // potential '|'.
      if (try_plural && *iter == ')') {
        // The standard case. Just commit the try_... .
        grouped      = false;
        out_singular = try_singular;
        out_plural   = try_plural;
      } else if (!try_plural && *iter == '|') {
        try_plural = out_plural;
      } else if (*iter == '(' || *iter == '|' || *iter == ')') {
        // We found something unexpected. Revert the current try.
        std::copy_backward(out_singular, try_singular, try_singular + 1);
        *out_singular = '(';
        ++try_singular;
        if (try_plural) {
          *try_singular++ = '|';
          try_singular    = std::copy(out_plural, try_plural, try_singular);
          try_plural      = nullptr;
        }
        out_plural   = std::copy(out_singular, try_singular, out_plural);
        out_singular = try_singular;
        if (*iter != '(') {
          grouped         = false;
          *out_singular++ = *out_plural++ = *iter;
        }
      } else {
        if (*iter == '\\') ++iter;
        (try_plural ? *try_plural++ : *try_singular++) = *iter;
      }
    } else if (*iter == '(') {
      grouped      = true;
      try_singular = out_singular;
      if (iter[1] == 's' && iter[2] == ')')
        try_plural = out_plural;
      else
        try_plural = nullptr;
    } else {
      if (*iter == '\\') ++iter;
      *out_singular++ = *out_plural++ = *iter;
    }
  }
  return result;
}

// Now measure the corresponding lengths.
// Basically the same code, just reduced to counting
template <typename Char>
constexpr std::tuple<std::size_t, std::size_t> measure_singular_plural(const Char *iter,
                                                                       const Char *const end) {
  std::size_t singular = 0, plural = 0;
  bool grouped = false, has_plural = false;
  // The following two are only defined if grouped is true.
  std::size_t try_singular, try_plural;
  for (; end != iter; ++iter) {
    if (grouped) {
      // If we find another '(', we treat this as a new group and assume that
      // the previous '(' should be written in both forms. Similar for a
      // potential '|'.
      if (try_plural != static_cast<std::size_t>(-1) && *iter == ')') {
        // The standard case. Just commit the try_... .
        has_plural = true;
        grouped    = false;
        singular += try_singular;
        plural += try_plural;
      } else if (try_plural == static_cast<std::size_t>(-1) && *iter == '|') {
        try_plural = 0;
      } else if (*iter == '(' || *iter == '|' || *iter == ')') {
        // We found something unexpected. Revert the current try.
        ++try_singular;
        if (try_plural != static_cast<std::size_t>(-1)) {
          try_singular += 1 + try_plural;
          try_plural = -1;
        }
        plural += try_singular;
        singular += try_singular;
        if (*iter != '(') {
          grouped = false;
          ++singular, ++plural;
        }
      } else {
        if (*iter == '\\') ++iter;
        if (try_plural != static_cast<std::size_t>(-1))
          ++try_plural;
        else
          ++try_singular;
      }
    } else if (*iter == '(') {
      grouped      = true;
      try_singular = 0;
      if (iter[1] == 's' && iter[2] == ')')
        try_plural = 0;
      else
        try_plural = -1;
    } else
      ++singular, ++plural;
  }
  return std::make_tuple(singular, has_plural ? plural : std::size_t(-1));
}

} // namespace detail

template <CompileTimeString Str, CompileTimeString Domain = CompileTimeString<
                                     typename decltype(Str)::char_type, std::size_t(-1)>()>
constexpr auto build_I18NString() {
  // We are working in two steps:
  constexpr auto lengths = [] {
    struct {
      std::size_t context = -1, singular = 0, plural = -1;
      const char *start_main;
    } result;
    std::tie(result.start_main, result.context) = detail::measure_context(Str.begin(), Str.end());
    std::tie(result.singular, result.plural) =
        detail::measure_singular_plural(result.start_main, Str.end());
    return result;
  }();

  struct TripleString {
    CompileTimeString<char, lengths.context> context;
    CompileTimeString<char, lengths.singular> singular;
    CompileTimeString<char, lengths.plural> plural;
  };
  constexpr TripleString strings = [&] {
    TripleString result{};
    result.context = detail::extract_context<char, lengths.context>(Str.begin());
    std::tie(result.singular, result.plural) =
        detail::extract_singular_plural<lengths.singular, lengths.plural>(lengths.start_main,
                                                                          Str.end());
    return result;
  }();

  return CompileTimeI18NString<Domain, strings.context, strings.singular, strings.plural>();
}

} // namespace mfk::i18n

#endif
