#include "ast.hpp"

#include <algorithm>
#include <string>
#include <vector>
#include <optional>

namespace {

struct Msg {
  std::optional<std::string> context;
  std::string singular;
  // The first two were the actual keys, based on the following we merge
  std::optional<std::string> location;
  std::vector<std::string> extracted;
  // The following two should always be equal
  std::optional<std::string> plural;
  std::vector<std::string> flags;
};

std::vector<Msg> messages_to_msgs(std::vector<client::ast::Message> messages) {
  std::vector<Msg> msgs;
  msgs.reserve(messages.size());
  for (auto &&message: messages) {
    std::vector<std::string> extracted;
    std::vector<std::string> flags;
    std::optional<std::string> location;
    for (auto &&comment: message.comments) {
      using Kind = client::ast::Comment::Kind;
      switch (comment.kind) {
        case Kind::Translator:
          if (!message.singular.empty()) {
            std::clog << "Unexpected translator comment dropped\n";
            break;
          }
          [[fallthrough]];
        case Kind::Extracted:
          extracted.push_back(std::move(comment.content));
          break;
        case Kind::Flags:
          flags.push_back(std::move(comment.content));
          break;
        case Kind::Previous:
          std::clog << "Unexpected reference to previous string dropped\n";
          break;
        case Kind::Reference:
          if (location)
            std::clog << "Duplicate reference location dropped\n";
          else
            location = std::move(comment.content);
          break;
      }
    }
    msgs.push_back({
        std::move(message.context),
        std::move(message.singular),
        std::move(location),
        std::move(extracted),
        std::move(message.plural),
        std::move(flags),
    });
  }
  return msgs;
}

std::vector<client::ast::Message> msgs_to_messages(std::vector<Msg> msgs) {
  std::vector<client::ast::Message> messages;
  messages.reserve(msgs.size());
  for (auto &&msg: msgs) {
    client::ast::Message message{
      {}, {},
      std::move(msg.context),
      std::move(msg.singular),
      std::move(msg.plural),
      {}};
    using Kind = client::ast::Comment::Kind;
    for (auto &&str: msg.extracted)
      message.comments.push_back({{}, Kind::Extracted, std::move(str)});
    if (msg.location)
      message.comments.push_back({{}, Kind::Reference, std::move(*msg.location)});
    for (auto &&str: msg.flags)
      message.comments.push_back({{}, Kind::Flags, std::move(str)});
    messages.push_back(std::move(message));
  }
  return messages;
}

void presort_msgs(std::vector<Msg> &msgs) {
  std::sort(msgs.begin(), msgs.end(), [](const auto &a, const auto &b) {
    std::strong_ordering order = a.context <=> b.context;
    if (order == 0) {
      order = a.singular <=> b.singular;
      if (order == 0) {
        order = a.location <=> b.location;
      }
    }
    return order < 0;
  });
}

void postsort_msgs(std::vector<Msg> &msgs) {
  std::sort(msgs.begin(), msgs.end(), [](const auto &a, const auto &b) {
    std::strong_ordering order = a.location <=> b.location;
    if (order == 0) {
      order = a.context <=> b.context;
      if (order == 0) {
        order = a.singular <=> b.singular;
      }
    }
    return order < 0;
  });
}

std::vector<Msg>::iterator merge_identical(std::vector<Msg> &msgs) {
  auto read_iter = msgs.begin();
  auto write_iter = msgs.begin();
  const auto old_end = msgs.end();

  while (read_iter != old_end) {
    Msg &base = *read_iter;
    while (++read_iter != old_end
        && read_iter->context == base.context
        && read_iter->singular == base.singular
        && read_iter->location == base.location) {
      [[unlikely]]
      if (read_iter->plural != base.plural || read_iter->flags != base.flags)
        std::clog << "Unexpected plural or flags mismatch ignored.\n";
    }
    *write_iter++ = std::move(base);
  }
  return write_iter;
}

std::vector<Msg>::iterator merge_groups(std::vector<Msg> &msgs, std::vector<Msg>::iterator old_end) {
  auto read_iter = msgs.begin();
  auto write_iter = msgs.begin();

  while (read_iter != old_end) {
    Msg &base = *read_iter;
    while (++read_iter != old_end
        && read_iter->context == base.context
        && read_iter->singular == base.singular) {
      [[unlikely]]
      if (read_iter->plural != base.plural || read_iter->flags != base.flags)
        std::clog << "Unexpected plural or flags mismatch ignored.\n";
      assert(read_iter->location); // An empty optional compares as minimal element
      [[likely]]
      if (base.location)
        *base.location += std::move(*read_iter->location);
      else
        base.location = std::move(read_iter->location);
      std::move(read_iter->extracted.begin(), read_iter->extracted.end(), std::back_inserter(base.extracted));
    }
    *write_iter++ = std::move(base);
  }
  return write_iter;
}

}

std::vector<client::ast::Message> client::ast::merge_messages(std::vector<client::ast::Message> messages) {
  std::vector<Msg> msgs = messages_to_msgs(std::move(messages));
  presort_msgs(msgs);
  auto new_end = merge_identical(msgs);
  msgs.erase(merge_groups(msgs, merge_identical(msgs)));
  postsort_msgs(msgs);
  return msgs_to_messages(std::move(msgs));
}
