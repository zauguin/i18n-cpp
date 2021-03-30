#pragma once
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <string>
#include <vector>
#include <optional>
#include <iostream>

namespace client::ast {

using boost::spirit::x3::position_tagged;

struct Comment: position_tagged {
  enum struct Kind {
    Translator,
    Extracted,
    Reference,
    Flags,
    Previous,
  };
  Kind kind;
  std::string content;
};

struct Message: position_tagged {
  std::vector<std::string> translatorComments;
  std::vector<std::pair<std::optional<std::string>, std::optional<std::string>>> extractedComments;
  std::optional<std::string> flags;
  std::optional<std::string> context;
  std::string singular;
  std::optional<std::string> plural;
  std::vector<std::string> translation;
};

std::ostream &operator <<(std::ostream&, const Comment&);
std::ostream &operator <<(std::ostream&, const Message&);

std::vector<client::ast::Message> merge_messages(std::vector<client::ast::Message> messages);

}
