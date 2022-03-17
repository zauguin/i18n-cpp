#include <catch2/catch_test_macros.hpp>
#include <i18n/simple.hpp>
#include <string>
#include <version>

#ifdef __cpp_lib_math_constants
#include <numbers>

using std::numbers::pi;
#else
double pi = 3.14159265358979;
#endif

using namespace mfk::i18n::literals;
using namespace std::string_literals;

TEST_CASE("C locale shows identity", "[simple_i18n]") {
  std::locale::global(std::locale("C"));
  textdomain("testcases");

  SECTION("singular strings") {
    // If this test fails, something went terribly wrong
    // L10N: Please don't mess up the translation.
    REQUIRE("Hello world!" == std::string("Hello world!"_));
  }

  SECTION("singular formatting") {
    REQUIRE("Hello Max!" == std::string("Hello {}!"_("Max")));
    /*
     * L10N: If you translate this into a language used by pedantic people, feel free to
     * increase the precision.
     *
     * But don't set it higher then 14, otherwise `double` might not be enough.
     */
    REQUIRE("pi is 3.1416." == std::string("pi is {:.4Lf}."_(pi)));
  }

  SECTION("plural strings") {
    REQUIRE("Hello planet!" == std::string("Hello planet(s)!"_[1]));
    REQUIRE("Hello planets!" == std::string("Hello planet(s)!"_[2]));
    REQUIRE("Hello person!" == std::string("Hello (person|people)!"_[1]));
    REQUIRE("Hello people!" == std::string("Hello (person|people)!"_[2]));
  }

  SECTION("plural strings formatting") {
    REQUIRE("I ate 1 apple." == std::string("I ate {} apple(s)."_(1)));
    REQUIRE("I ate 2 apples." == std::string("I ate {} apple(s)."_(2)));
    REQUIRE("I ate an apple." == std::string("I ate (an|{}) apple(s)."_(1)));
    REQUIRE("I ate 2 apples." == std::string("I ate (an|{}) apple(s)."_(2)));
  }
}

TEST_CASE("de locale provides translations", "[translations]") {
  std::locale::global(std::locale("de_DE.UTF-8"));
  bindtextdomain("testcases", TEST_SOURCE_DIR);
  textdomain("testcases");

  SECTION("singular strings") { REQUIRE("Hallo Welt!" == std::string("Hello world!"_)); }

  SECTION("singular formatting") {
    REQUIRE("Hallo Max!" == std::string("Hello {}!"_("Max")));
    REQUIRE("Pi ist 3,1416." == std::string("pi is {:.4Lf}."_(pi)));
  }

  SECTION("plural strings") {
    REQUIRE("Hallo Planet!" == std::string("Hello planet(s)!"_[1]));
    REQUIRE("Hallo Planeten!" == std::string("Hello planet(s)!"_[2]));
    REQUIRE("Hallo Person!" == std::string("Hello (person|people)!"_[1]));
    REQUIRE("Hallo Personen!" == std::string("Hello (person|people)!"_[2]));
  }

  SECTION("plural strings formatting") {
    REQUIRE("Ich habe 1 Apfel gegessen." == std::string("I ate {} apple(s)."_(1)));
    REQUIRE("Ich habe 2 Äpfel gegessen." == std::string("I ate {} apple(s)."_(2)));
    REQUIRE("Ich habe einen Apfel gegessen." == std::string("I ate (an|{}) apple(s)."_(1)));
    REQUIRE("Ich habe 2 Äpfel gegessen." == std::string("I ate (an|{}) apple(s)."_(2)));
  }
}
