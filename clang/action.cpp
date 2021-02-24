#include "action.ipp"

std::unique_ptr<clang::tooling::FrontendActionFactory> i18nAction() {
  return clang::tooling::newFrontendActionFactory<class i18nAction>();
}
