#include "../lib/ast.hpp"

namespace client::ast {

using std::ostream;

static ostream &write_escaped(ostream &stream, const std::string str) {
  constexpr char hexdigit[17] = "0123456789abcdef";
  stream << '"';
  for (auto c: str)
    switch (c) {
      case '\\': stream << "\\\\"; break;
      case '"':  stream << "\\\""; break;
      case '\n': stream << "\\n"; break;
      case '\t': stream << "\\t"; break;
      case '\b': stream << "\\b"; break;
      case '\r': stream << "\\r"; break;
      case '\f': stream << "\\f"; break;
      case '\v': stream << "\\v"; break;
      case '\a': stream << "\\a"; break;
      default:
        if (std::isprint(c)) {
          stream << c;
        } else {
          stream << "\\x" << hexdigit[std::uint8_t(c) / 16]
                          << hexdigit[std::uint8_t(c) % 16];
        }
    }
  return stream << '"';
}

ostream &operator <<(ostream &stream, const Comment &comment) {
  stream << '#';
  switch (comment.kind) {
    case Comment::Kind::Translator: break;
    case Comment::Kind::Extracted: stream << '.'; break;
    case Comment::Kind::Reference: stream << ':'; break;
    case Comment::Kind::Flags: stream << ','; break;
    case Comment::Kind::Previous: stream << '|'; break;
  }
  return stream << ' ' << comment.content << '\n';
}

ostream &operator <<(ostream &stream, const Message &message) {
  for (auto &&comment: message.comments)
    stream << comment;
  if (message.context) {
    stream << "msgctxt ";
    write_escaped(stream, *message.context);
    stream << '\n';
  }
  stream << "msgid ";
  write_escaped(stream, message.singular);
  stream << '\n';
  if (message.plural) {
    stream << "msgid_plural ";
    write_escaped(stream, *message.plural);
    stream << '\n';
    int i = 0;
    for (auto &&translated: message.translation) {
      stream << "msgstr[" << i++ << "] ";
      write_escaped(stream, translated) << '\n';
    }
  } else {
    stream << "msgstr ";
    if (message.translation.empty())
      stream << "\"\"\n";
    else {
      if (message.translation.size() > 1)
        std::clog << "WARNING: Unexpected msgstr value dropped\n";
      write_escaped(stream, message.translation[0]) << '\n';
    }
  }
  return stream;
}

}
