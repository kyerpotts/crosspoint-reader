#pragma once

#include <cstddef>

struct FontFamilyName {
  static constexpr std::size_t CAPACITY = 64;

  char value[CAPACITY] = {};

  void assign(const char* source) {
    if (!source) {
      clear();
      return;
    }
    std::size_t i = 0;
    for (; i + 1 < CAPACITY && source[i] != '\0'; ++i) {
      value[i] = source[i];
    }
    value[i] = '\0';
  }

  void clear() { value[0] = '\0'; }
  bool empty() const { return value[0] == '\0'; }

  char& operator[](const std::size_t index) { return value[index]; }
  const char& operator[](const std::size_t index) const { return value[index]; }
};

static_assert(sizeof(FontFamilyName) == FontFamilyName::CAPACITY);
