#pragma once

#include "ast_adapted.hpp"

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>

namespace client {
namespace parser {

using quoted_string_type = boost::spirit::x3::rule<struct quoted_string_class, std::string>;
using message_type = boost::spirit::x3::rule<struct message_class, ast::Message>;
BOOST_SPIRIT_DECLARE(message_type);

}

parser::message_type message();
}
