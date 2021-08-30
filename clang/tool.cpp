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

namespace {
// Apply a custom category to all command-line options so that they are the
// only ones displayed.
cl::OptionCategory i18nCategory("i18n options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
[[maybe_unused]] cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
[[maybe_unused]] cl::extrahelp MoreHelp(
    "\nExtract messages marked for i18n from source files.\n"
    "Pass a number of source files as arguments to extract i18n strings\n"
    "from them and store them in corresponding .poc files. These can then be\n"
    "merged using merge_pot into a .pot file for the current project.\n\n"
    " --domain <textdomain> is used to restrict extraction to strings with an explicit "
    "textdomain.\n\n"
    "    If your project explicitly specifies the text domain for every\n"
"string, this can be used to restrict the extraction to one specific domain.\n"
    "This option can only be used once, if you want to extract messages for multiple\n"
    "domains the tool as to be called multiple times. It can be combined with\n"
    "--nodomain though. This is mostly useful if  the domain is also the globally\n"
    "set textdomain in your application.\n\n"
    " --nodomain Extract strings with no explicit textdomain (This is most likely what you want)\n\n"
    "    By default, all strings are extracted. If this option is specified,\n"
    "    only strings with no explicit textdomain will be extracted. As long as\n"
    "    you are aware that this will hide all other messages you probably want\n"
    "    to set this option.\n\n"
    " --comment <prefix> Restrict comment extraction to comments starting with <prefix>\n\n"
    "    Comments on the line preceding the i18n string get added to\n"
    "    the .pot file to help translators. Use this option to restrict\n"
    "    this to comments explicitly marked for this purpose,\n"
    "    e.g. by adding a \"I18N: \" prefix (corresponding to \"--comment I18N:\")\n\n"
    " --basepath <path> Set a basepath for all filename references in the .poc file.\n\n"
    "    In order for merge_pot to work properly all .poc files to be merged\n"
    "    must have been generated with the same --basepath. If this option\n"
    "    is not provided, all paths will be absolute.");

cl::opt<bool> noDomain("nodomain", cl::desc("Extract strings without domain"),
                       cl::cat(i18nCategory));
cl::opt<std::string> domain("domain", cl::desc("Select domain to extract"), cl::cat(i18nCategory));
cl::opt<std::string> comment("comment", cl::desc("Prefix for extracted comments"),
                             cl::cat(i18nCategory));
cl::opt<std::string> basepath("basepath", cl::desc("Base path for reference locations"),
                              cl::cat(i18nCategory));
} // namespace

int main(int argc, const char **argv) {
  CommonOptionsParser OptionsParser(argc, argv, i18nCategory);
  ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
  std::vector<std::string> options;
  if (noDomain.getValue()) options.push_back("nodomain");
  if (domain.getNumOccurrences()) options.push_back("domain=" + domain.getValue());
  if (comment.getNumOccurrences()) options.push_back("comment=" + comment.getValue());
  if (basepath.getNumOccurrences()) options.push_back("basepath=" + basepath.getValue());
  return Tool.run(i18nActionFactory(std::move(options)).get());
}
