#include "messages.hpp"

#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>
#include <cstring>

namespace client {
namespace parser {

namespace x3    = boost::spirit::x3;
namespace ascii = x3::ascii;

using x3::uint_, x3::lit, x3::double_, x3::lexeme, x3::char_;

struct error_handler {
  template <typename Iterator, typename Exception, typename Context>
  x3::error_handler_result on_error(const Iterator &, const Iterator &, const Exception &ex,
                                    const Context &context) {
    auto &error_handler = x3::get<x3::error_handler_tag>(context).get();
    error_handler(ex.where(), "Error! Expecting " + ex.which() + " here:");
    return x3::error_handler_result::fail;
  }
  /* template<typename T, typename Iterator, typename Context> */
  /* void on_success(const Iterator &begin, const Iterator &end, T &ast, const Context &context) {
   */
  /*   auto &position_cache = x3::get<error_handler_tag>(context).get(); */
  /*   position_cache.annotate(ast, begin, end); */
  /* } */
};

struct quoted_string_class {
  template <typename Iterator, typename Exception, typename Context>
  x3::error_handler_result on_error(Iterator &first, const Iterator &last, const Exception &ex,
                                    const Context &context) {
    auto &error_handler = x3::get<x3::error_handler_tag>(context).get();
    if (ex.where() != last) {
      if (*ex.where() == '\n' || *ex.where() == '\r') {
        parse(first, last, x3::eol);
        error_handler(ex.where(), "Unexpected end of line in string literal at");
        return x3::error_handler_result::fail;
      } else
        std::cerr << "Encountered " << *ex.where() << '\n';
    }
    error_handler(ex.where(), "Error! Expecting " + ex.which() + " here:");
    return x3::error_handler_result::fail;
  }
  /* template<typename T, typename Iterator, typename Context> */
  /* void on_success(const Iterator &begin, const Iterator &end, T &ast, const Context &context) {
   */
  /*   auto &position_cache = x3::get<error_handler_tag>(context).get(); */
  /*   position_cache.annotate(ast, begin, end); */
  /* } */
};

struct comment_kind_ : x3::symbols<ast::Comment::Kind> {
  comment_kind_() {
    using Kind = ast::Comment::Kind;
    add(".", Kind::Extracted)(":", Kind::Reference)(",", Kind::Flags)("|", Kind::Previous);
  }
};

struct control_sequence_ : x3::symbols<char> {
  control_sequence_() {
    add("n", '\n')("t", '\t')(
        "b", '\b')("r", '\r')("f", '\f')("v", '\v')("a", '\a')("\\", '\\')("\"", '"');
  }
};

const auto any_control_sequence = '\\' > (control_sequence_()
                                          | ('x' > x3::uint_parser<unsigned char, 16, 2, 2>())
                                          | x3::uint_parser<unsigned char, 8, 1, 3>());

const auto comment_kind = comment_kind_() | x3::attr(ast::Comment::Kind::Translator);

struct message_class : error_handler {};
struct extracted_comment_class : x3::annotate_on_success {};

const x3::rule<quoted_string_class, std::string> quoted_string    = "quoted_string";
const x3::rule<struct quoted_strings, std::string> quoted_strings = "quoted_strings";
const x3::rule<extracted_comment_class, std::pair<std::string, std::optional<std::string>>>
    extracted_comment                                 = "extracted_comment";
const x3::rule<message_class, ast::Message> message   = "message";
const x3::rule<struct raw_line, std::string> raw_line = "raw_line";

const auto &raw_line_def = lexeme[*(char_ - x3::eol) >> x3::eol];

auto append                   = [](auto &context) { _val(context) += _attr(context); };
const auto quoted_string_def  = lexeme['"' >> *(any_control_sequence | (char_ - '"' - '\n')) > '"'];
const auto quoted_strings_def = +quoted_string[append];
auto add_extracted            = [](auto &context) {
  for (auto &&s : _attr(context))
    if (_val(context).second) {
      *_val(context).second += '\n';
      *_val(context).second += s;
    } else
      _val(context).second = s;
};
auto add_reference = [](auto &context) {
  _val(context).first = at_c<1>(_attr(context));
  for (auto &&s : at_c<0>(_attr(context)))
    if (_val(context).second) {
      *_val(context).second += '\n';
      *_val(context).second += s;
    } else
      _val(context).second = s;
};
const auto extracted_comment_def =
    (*lexeme["#. " > raw_line] >> lexeme["#: " > raw_line])[add_reference]
    | (+lexeme["#. " > raw_line])[add_extracted];

const auto message_def = *lexeme["# " > raw_line] > *extracted_comment > -lexeme["#, " > raw_line]
                         > -("msgctxt" > quoted_strings) > "msgid" > quoted_strings
                         > -("msgid_plural" > quoted_strings)
                         > +("msgstr" > -x3::omit['[' > uint_ > ']'] > quoted_strings);

BOOST_SPIRIT_DEFINE(raw_line, quoted_string, quoted_strings, extracted_comment);
BOOST_SPIRIT_DEFINE(message);

} // namespace parser
parser::message_type message() {
  return parser::message;
}

} // namespace client
