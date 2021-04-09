#include "config.hpp"
#include "merge.hpp"
#include "messages.hpp"

#include <ctime>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <string>

std::string read_file(const char *filename) {
  std::ifstream file(filename);
  if (!file) throw "Unable to open file";
  std::ostringstream str;
  str << file.rdbuf();
  return str.str();
}

int main(int argc, char const *argv[]) {
  int errs = 0;

  std::string_view copyright = "THE PACKAGE'S COPYRIGHT HOLDER";
  std::string_view package   = "PACKAGE";
  std::string_view version   = "VERSION";
  std::string_view bugs_addr = "";
  std::unique_ptr<std::ostream> out_file;
  std::string timestamp(21, '\0');
  {
    std::time_t now = std::time(nullptr);
    timestamp.resize(
        std::strftime(timestamp.data(), timestamp.size() + 1, "%F %R%z", std::localtime(&now)));
  }

  int current_arg = 1;
  for (; argc != current_arg && *argv[current_arg] == '-'; ++current_arg) {
    std::string_view arg = argv[current_arg];
    if (arg == "--") {
      ++current_arg;
      break;
    } else if (arg.starts_with("--copyright="))
      copyright = arg.substr(12);
    else if (arg.starts_with("--package="))
      package = arg.substr(10);
    else if (arg.starts_with("--version="))
      version = arg.substr(10);
    else if (arg.starts_with("--msgid-bugs-address="))
      bugs_addr = arg.substr(21);
    else if (arg.starts_with("--output="))
      out_file = std::make_unique<std::ofstream>(arg.substr(9).data());
    else
      std::cerr << "Ignoring unknown option " << arg << '\n';
  }

  std::vector<client::ast::Message> messages;
  for (; argc != current_arg; ++current_arg) {
    std::string str = read_file(argv[current_arg]);
    auto iter       = str.cbegin();
    const auto end  = str.cend();
    client::parser::error_handler_type error_handler(iter, end, std::cerr, argv[current_arg]);
    auto parser =
        with<boost::spirit::x3::error_handler_tag>(std::ref(error_handler))[client::message()];
    while (iter != end) {
      client::ast::Message message;
      bool r = phrase_parse(iter, end, parser, boost::spirit::x3::ascii::space, message);
      if (!r) {
        ++errs;
        break;
      }
      messages.push_back(std::move(message));
    }
  }
  messages             = client::ast::merge_messages(std::move(messages));
  std::ostream &stream = out_file ? *out_file : std::cout;
  stream << fmt::format(
      R"(# SOME DESCRIPTIVE TITLE.
# Copyright (C) YEAR {0}
# This file is distributed under the same license as the {1} package.
# FIRST AUTHOR <EMAIL@ADDRESS>, YEAR.
#
#, fuzzy
msgid ""
msgstr ""
"Project-Id-Version: {1} {2}\n"
"Report-Msgid-Bugs-To: {3}\n"
"POT-Creation-Date: {4}\n"
"PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\n"
"Last-Translator: FULL NAME <EMAIL@ADDRESS>\n"
"Language-Team: LANGUAGE <LL@li.org>\n"
"Language: \n"
"MIME-Version: 1.0\n"
"Content-Type: text/plain; charset=CHARSET\n"
"Content-Transfer-Encoding: 8bit\n"
)",
      copyright, package, version, bugs_addr, timestamp);
  for (auto &&msg : messages)
    stream << '\n' << msg;
  return errs;
}
