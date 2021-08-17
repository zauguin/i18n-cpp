#include <catch2/catch_test_macros.hpp>
#include <i18n/simple.hpp>
#include <string>

using namespace mfk::i18n::literals;
using namespace std::string_literals;

TEST_CASE("C locale shows identity", "[simple_i18n]" ) {
  std::locale::global(std::locale("C"));
  textdomain("testcases");

  SECTION("singular strings") {
    REQUIRE("Hello world!" == std::string("Hello world!"_));
  }

  SECTION("singular formatting") {
    REQUIRE("Hello Max!" == std::string("Hello {}!"_("Max")));
  }

  SECTION("plural strings") {
    REQUIRE("Hello world!" == std::string("Hello world(s)!"_[1]));
    REQUIRE("Hello worlds!" == std::string("Hello world(s)!"_[2]));
    REQUIRE("Hello earth!" == std::string("Hello (earth|planets)!"_[1]));
    REQUIRE("Hello planets!" == std::string("Hello (earth|planets)!"_[2]));
  }

  SECTION("plural strings formatting") {
    REQUIRE("I ate 1 apple." == std::string("I ate {} apple(s)."_(1)));
    REQUIRE("I ate 2 apples." == std::string("I ate {} apple(s)."_(2)));
    REQUIRE("I ate an apple." == std::string("I ate (an|{}) apple(s)."_(1)));
    REQUIRE("I ate 2 apples." == std::string("I ate (an|{}) apple(s)."_(2)));
  }
}
