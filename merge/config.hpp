#pragma once

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>
#include <boost/spirit/home/x3/char/char_class.hpp>
#include <boost/spirit/home/x3/core/parse.hpp>
#include <string>

namespace client::parser {
  namespace x3 = boost::spirit::x3;

  using iterator_type = std::string::const_iterator;
  using phrase_context_type = x3::phrase_parse_context<x3::ascii::space_type>::type;
  using error_handler_type = x3::error_handler<iterator_type>;

  using context_type = x3::context<x3::error_handler_tag, std::reference_wrapper<error_handler_type>, phrase_context_type>;
}
