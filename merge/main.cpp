#include "../lib/ast.hpp"
#include "messages.hpp"
#include "config.hpp"

#include <iostream>
#include <fstream>
#include <string>

std::string read_file(const char *filename) {
  std::ifstream file(filename);
  if (!file)
    throw "Unable to open file";
  std::ostringstream str;
  str << file.rdbuf();
  return str.str();
}

int main(int argc, char const* argv[])
{
  int errs = 0;
  std::vector<client::ast::Message> messages;
  for (int i = 1; argc != i; ++i) {
    std::string str = read_file(argv[i]);
    auto iter = str.cbegin();
    const auto end = str.cend();
    client::parser::error_handler_type error_handler(iter, end, std::cerr, argv[i]);
    auto parser = with<boost::spirit::x3::error_handler_tag>(std::ref(error_handler))[client::message()];
    while(iter != end) {
      client::ast::Message message;
      bool r = phrase_parse(iter, end, parser, boost::spirit::x3::ascii::space, message);
      if (!r) {
        ++errs;
        break;
      }
      messages.push_back(std::move(message));
    }
  }
  return errs;
}
