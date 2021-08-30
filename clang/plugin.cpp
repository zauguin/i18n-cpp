#include "action.h"

#include <clang/Frontend/FrontendPluginRegistry.h>

[[maybe_unused]] static clang::FrontendPluginRegistry::Add<i18nAction>
    registration("i18n", "Extract key strings for internationalization");
