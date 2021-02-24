#include <clang/Frontend/FrontendPluginRegistry.h>

#include "action.ipp"

static clang::FrontendPluginRegistry::Add<class i18nAction> registration(
    "i18n", "Extract key strings for internationalization");
