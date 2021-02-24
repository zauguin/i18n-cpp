#include "config.hpp"
#include "messages.ipp"

namespace client::parser {
  BOOST_SPIRIT_INSTANTIATE(message_type, iterator_type, context_type);
}
