#include <compare>
#include <i18n/simple.hpp>
#include <initializer_list>
#include <iostream>
#include <random>

using namespace mfk::i18n::literals;

class dev_seed {
 public:
  using result_type = std::uint32_t;

 private:
  std::random_device rddev;
  std::uniform_int_distribution<result_type> dist;

 public:
  dev_seed() = default;
  template <typename Iter>
  [[maybe_unused]] dev_seed(Iter, Iter) {}
  [[maybe_unused]] dev_seed(std::initializer_list<result_type>) {}
  template <typename Iter>
  void generate(Iter begin, const Iter end) {
    while (begin != end)
      *begin++ = dist(rddev);
  }
  [[nodiscard]] static std::size_t size() { return 0; }
  template <typename Iter>
  static void param(Iter) {}
};

#pragma mfk i18n domain("guessinggame")

int main(int, char const *[]) {
  std::locale::global(std::locale(""));
  textdomain("guessinggame");
  constexpr auto welcome = "Welcome to the guessing game\n";
  std::cout << gettext(welcome);
  auto rng = []() {
    auto seed = dev_seed();
    return std::mt19937(seed);
  }();
  std::uniform_int_distribution<int> dist(1, 1023);
  std::string answer;
  do {
    const auto secret_number = dist(rng);
    for (int i = 1; true; ++i) {
      std::cout << "Please guess a number between 1 and 1023: "_;
      int guess;
      std::cin >> guess;
      if (!std::cin) return -1;
      auto comp = guess <=> secret_number;
      if (comp == std::strong_ordering::less)
        std::cout << "You guessed too low\n"_;
      else if (comp == std::strong_ordering::greater)
        std::cout << "You guessed too high\n"_;
      else if (comp == std::strong_ordering::equal) {
        std::cout << "That's right! You only needed {} guess(|es) to find the number.\n"_(i);
        break;
      }
    }
    std::cout << "Do you want to play again? (yes/no) "_;
    std::cin >> answer;
  } while (answer == static_cast<const char *>("play again|yes"_));

  if (answer != static_cast<const char *>("play again|no"_))
    std::cerr << "ERROR: Expected \"yes\" or \"no\", but got \"{}\".\n"_(answer);
  return 0;
}
