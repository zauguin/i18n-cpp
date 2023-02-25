#pragma once

#include "clang/Tooling/Tooling.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class i18nAction : public clang::PluginASTAction {
 public:
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &,
                                                        llvm::StringRef) override;
  bool ParseArgs(const std::vector<std::string> &args);

 protected:
  // It would be cleaner to move functionality from i18nConsumer to
  // Begin/EndSourceFileAction, but they don't work for plugins.
  /* bool BeginSourceFileAction(clang::CompilerInstance &) override {} */
  /* void EndSourceFileAction() override {} */
  bool ParseArgs(const clang::CompilerInstance &, const std::vector<std::string> &args) override {
    return ParseArgs(args);
  }
  PluginASTAction::ActionType getActionType() override { return AddBeforeMainAction; }

 private:
  std::optional<std::string> domain_filter;
  bool empty_domain = false;
  std::optional<std::string> comment_filter;
  std::optional<std::filesystem::path> base_path;
  std::optional<std::string> output;
};
