#ifndef I18N_SIMPLE_HPP
#define I18N_SIMPLE_HPP

#include "../i18n.hpp"

extern "C" {
[[mfk::i18n]] extern char *gettext([[mfk::i18n_singular_begin]] const char *);
[[mfk::i18n]] extern char *dgettext([[mfk::i18n_domain_begin]] const char *,
                                    [[mfk::i18n_singular_begin]] const char *);
[[mfk::i18n]] extern char *ngettext([[mfk::i18n_singular_begin]] const char *,
                                    [[mfk::i18n_plural_begin]] const char *, unsigned long);
[[mfk::i18n]] extern char *dngettext([[mfk::i18n_domain_begin]] const char *,
                                     [[mfk::i18n_singular_begin]] const char *,
                                     [[mfk::i18n_plural_begin]] const char *, unsigned long);
}

namespace mfk::i18n::inline literals {
template <CompileTimeString str>
[[mfk::i18n]] constexpr auto operator""_() {
  return build_I18NString<str>();
}
} // namespace mfk::i18n::inline literals

#endif
