#pragma once

#include "clang/Tooling/Tooling.h"

#include <memory>

std::unique_ptr<clang::tooling::FrontendActionFactory> i18nAction();
