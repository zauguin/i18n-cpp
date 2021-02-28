#pragma once

#include "../common/ast.hpp"
#include <boost/fusion/include/adapt_struct.hpp>

BOOST_FUSION_ADAPT_STRUCT(client::ast::Comment, kind, content)

BOOST_FUSION_ADAPT_STRUCT(client::ast::Message, comments, context, singular, plural, translation)
