# i18n++
I18N for C++20 based on libintl/gettext

## Usecase
When internationalizing C++ projects one of the most common approaches is to use the `gettext` family of function from `libintl`.
A typical example for using it would be

```cpp
#include <libintl.h>
#include <locale>
#include <cstdio>

#define _(x) gettext(x)
#define _n(singular, plural, n) ngettext(singular, plural, n)
#define _p(context, msg) pgettext(context, msg)
#define N_(x) x

constexpr auto saved_string = N_("Some global string"); // This will be translated later, so we already have to mark it with a macro to get it extracted.
                                                        // Let's just hope that we never need a plural version for this.

int main () {
  std::locale::global(std::locale("")); // Use the current system locale
  textdomain("my_test_textdomain");

  std::printf(_("The first message."));
  std::printf(_n("We have %i message.", "We have %i messages.", 6), 6);

  const char *open_file_label = pgettext("file", "open"); // Add some context to differentiate  messages which are the same in english
  const char *open_door_label = pgettext("door", "open"); // Add some context to differentiate  messages which are the same in english
  std::printf(_(saved_string));

  std::printf(_("The last message."));
}
```

Then the strings can be extracted using `xgettext` which scans the files to find all uses of your macros.

While this works, it's for from ideal in a modern C++ project:

 - It uses macros, often even macros which carry meaning which isn't directly visible in the program.
 - Using plural forms requires the text to be duplicated, even though in almost all cases the english version in the code uses predictable plural forms.
 - The syntax `_("...")` looks weird in C++.
 - String literals have to extracted with `xgettext` and therefore can only use syntax supported by `xgettext`.
 - Strings with context and/or plural can't easily be stored to be translated later.
 - It can not be used directly in `std::format` (or `fmt::format`) since the string is no longer a constant expression. (Only relevant after P2216, but this change is applied for C++20 already since it's a defect report)

So we propse an alternative:

```c++
#include <i18n/simple.hpp>
#include <locale>
#include <iostream>

using namespace mfk::i18n::literals;

constexpr auto saved_string = "Some global string"_;
constexpr auto saved_string_plural = "We have {} global string(s)"_;

int main () {
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
```

The usage is simple:

 - By default the `simple.hpp` header uses `operator ""_`, so translatable strings can be marked by adding a `_`.
   If you prefer e.g. `_i18n` or another name, it is easy to adapt `simple.hpp` for your project.
 - A context can be added to any string by prepending the context followed by `|`.
 - The result of the operator can be stored as an untranslated string but it can also be used in most places where ordinary strings are used
   since it's convertible to `const char *` and `std::string_view`. These conversions automatically trigger the translation.
 - Plural forms build with by appending a `s` can be written using `(s)` and automatically get expanded to the full forms at compile time.
 - Irregular plural forms can be written using `(singular form|plural form)`, e.g. `{} (person|people)`.
 - If the string uses the characters `|()\` in situations which might get interpreted as one these things, they can be escaped by prepending backslash.
   In string literals, this normally means that two backslashes have to be added (`"The following has no special meaning: \\(hello \\| world\\)"`)
 - Plural strings get translated by passing an integer in square brackets (`"message(s)"[5]`) or using the `std::format` integration.
 - All messages can be followed by argument lists in regular parentheses to automatically pass them to `std::format`. Then the first parameter is used to select the form plural form if applicable.
   This automatically falls back to `libfmt` if `std::format` is not available.
   Compile-time type checking is done based on the untranslated forms.

Additionally a clang plugin is provided to extract the untranslated strings into a `.pot` file during compilation.

## Getting started

First compile the `clang++` plugin:
```
git clone https://github.com/zauguin/i18n-cpp i18n++
cd i18n++
git submodule update --init
CXX=clang++ cmake -B build -G Ninja
cmake --build build
sudo cmake --build build -t install
```
The file can then be compiled with

    clang++ -c filename.cpp -fplugin=i18n-clang.so -Xclang -plugin-arg-i18n -Xclang nodomain

This generates a `filename.o.poc` (**PO** **c**omponent) in addition to the `filename.o` object.
All the `.poc` files in your program can be merged into a `.pot` file using

    i18n-merge-pot --package="Awesome project" --version=1.0.0 --output=awesome.pot *.poc

This `.pot` file can then be handled as if it had been generated with `xgettext`.

See [the example directory](example/CMakeLists.txt) for an example how to integrate this into a CMake project.
