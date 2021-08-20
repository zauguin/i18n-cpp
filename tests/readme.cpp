#include <catch2/catch_test_macros.hpp>
#include <i18n/simple.hpp>
#include <locale>
#include <iostream>

using namespace mfk::i18n::literals;

constexpr auto saved_string = "Some global string"_;
constexpr auto saved_string_plural = "We have {} global string(s)"_;

TEST_CASE("test compilation of code from README.md", "[readme]" ) {
  std::locale::global(std::locale("")); // Use the current system locale
  textdomain("my_test_textdomain");

  std::cout << "The first message.\n"_;
  std::cout << "We have {} message(s).\n"_(6);

  const char *open_file_label = "file|open"_; // Add some context to differentiate messages which are the same in english
  const char *open_door_label = "door|open"_; // Add some context to differentiate messages which are the same in english

  std::cout << "The first message.\n"_;
  std::cout << saved_string << '\n';
  std::cout << saved_string_plural(2) << '\n';

  std::cout << "The last message.\n"_;
}
