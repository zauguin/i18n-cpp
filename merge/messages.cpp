#include "messages.ipp"

#include "config.hpp"

namespace client::parser {
BOOST_SPIRIT_INSTANTIATE(message_type, iterator_type, context_type);
}
