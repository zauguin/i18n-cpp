#include "action.h"

#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"

std::unique_ptr<clang::tooling::FrontendActionFactory>
i18nActionFactory(std::vector<std::string> args) {
  class Factory : public clang::tooling::FrontendActionFactory {
    std::vector<std::string> option;

   public:
    Factory(std::vector<std::string> args): option(std::move(args)) {}
    std::unique_ptr<clang::FrontendAction> create() override {
      auto action = std::make_unique<i18nAction>();
      action->ParseArgs(option);
      return action;
    }
  };
  return std::make_unique<Factory>(args);
}

using namespace clang::tooling;
using namespace llvm;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static cl::OptionCategory i18nCategory("i18n options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...\n");

static cl::opt<bool> noDomain("nodomain", "Extract strings without domain");
static cl::opt<std::string> domain("domain", "Select domain to extract");
static cl::opt<std::string> comment("comment", "Prefix for extracted comments");

int main(int argc, const char **argv) {
  CommonOptionsParser OptionsParser(argc, argv, i18nCategory);
  ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
  std::vector<std::string> options;
  if (noDomain.getValue()) options.push_back("nodomain");
  if (domain.getNumOccurrences()) options.push_back("domain=" + domain.getValue());
  if (comment.getNumOccurrences()) options.push_back("comment=" + comment.getValue());
  return Tool.run(i18nActionFactory(std::move(options)).get());
}
