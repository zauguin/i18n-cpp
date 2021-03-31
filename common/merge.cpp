#include "ast.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace {

using Message = client::ast::Message;

void presort_msgs(std::vector<Message> &msgs) {
  std::sort(msgs.begin(), msgs.end(), [](const auto &a, const auto &b) {
    std::strong_ordering order = a.context <=> b.context;
    if (order == 0) {
      order = a.singular <=> b.singular;
      if (order == 0) { order = a.extractedComments <=> b.extractedComments; }
    }
    return order < 0;
  });
}

void postsort_msgs(std::vector<Message> &msgs) {
  std::sort(msgs.begin(), msgs.end(), [](const auto &a, const auto &b) {
    std::strong_ordering order = a.extractedComments <=> b.extractedComments;
    [[unlikely]] if (order == 0) {
      order = a.context <=> b.context;
      if (order == 0) { order = a.singular <=> b.singular; }
    }
    return order < 0;
  });
}

void combine_elements(std::vector<Message> &msgs) {
  auto read_iter     = msgs.begin();
  auto write_iter    = msgs.begin();
  const auto old_end = msgs.end();

  while (read_iter != old_end) {
    Message &base = *read_iter;
    while (++read_iter != old_end && read_iter->context == base.context
           && read_iter->singular == base.singular) {
      [[unlikely]] if (read_iter->plural != base.plural || read_iter->flags != base.flags) {
        std::clog << "Unexpected plural or flags mismatch ignored.\n";
      }
      base.extractedComments.insert(base.extractedComments.end(),
                                    std::make_move_iterator(read_iter->extractedComments.begin()),
                                    std::make_move_iterator(read_iter->extractedComments.end()));
    }
    if (write_iter + 1 != read_iter) *write_iter = std::move(base);
    ++write_iter;
  }
  msgs.erase(write_iter, msgs.end());
}

void merge_msg(std::vector<Message> &msgs) {
  for (auto &msg : msgs) {
    std::sort(msg.extractedComments.begin(), msg.extractedComments.end());
    const auto front   = msg.extractedComments.begin();
    const auto old_end = msg.extractedComments.end();

    if (front != old_end) {
      auto read_iter = front + 1;
      while (read_iter != old_end) {
        auto &base = *read_iter;
        while (++read_iter != old_end && read_iter->first == base.first) {
          if (read_iter->second != base.second) {
            if (read_iter->second)
              base.second = std::move(read_iter->second);
            else
              std::clog << "Comment mismatch for identical reference. Second "
                           "comments will be dropped.\n";
          }
        }
        if (base.first) {
          if (front->first) {
            *front->first += ' ';
            *front->first += *base.first;
          } else
            front->first = std::move(base.first);
        }
        if (base.second) {
          if (front->second) {
            *front->second += '\n';
            *front->second += *base.second;
          } else
            front->second = std::move(base.second);
        }
      }
      msg.extractedComments.resize(front->first || front->second ? 1 : 0);
    }
  }
}

} // namespace

std::vector<Message> client::ast::merge_messages(std::vector<Message> messages) {
  presort_msgs(messages);
  combine_elements(messages);
  merge_msg(messages);
  postsort_msgs(messages);
  return messages;
}
