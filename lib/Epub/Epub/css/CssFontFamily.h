#pragma once

#include <cstdint>
#include <string_view>

enum class CssGenericFontFamily : uint8_t { Unspecified = 0, Monospace = 1, Proportional = 2 };

namespace CssFontFamilyDetail {
constexpr char lowerAscii(const char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c; }

constexpr std::string_view trim(std::string_view value) {
  while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r' ||
                             value.front() == '\n' || value.front() == '\'' || value.front() == '"')) {
    value.remove_prefix(1);
  }
  while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' ||
                             value.back() == '\n' || value.back() == '\'' || value.back() == '"')) {
    value.remove_suffix(1);
  }
  return value;
}

constexpr bool equalsIgnoreAsciiCase(const std::string_view value, const std::string_view expectedLower) {
  if (value.size() != expectedLower.size()) {
    return false;
  }
  for (size_t i = 0; i < value.size(); ++i) {
    if (lowerAscii(value[i]) != expectedLower[i]) {
      return false;
    }
  }
  return true;
}
}  // namespace CssFontFamilyDetail

constexpr CssGenericFontFamily parseCssGenericFontFamily(const std::string_view value) {
  CssGenericFontFamily result = CssGenericFontFamily::Unspecified;
  size_t start = 0;
  for (size_t i = 0; i <= value.size(); ++i) {
    if (i != value.size() && value[i] != ',') {
      continue;
    }
    const auto family = CssFontFamilyDetail::trim(value.substr(start, i - start));
    if (CssFontFamilyDetail::equalsIgnoreAsciiCase(family, "monospace")) {
      result = CssGenericFontFamily::Monospace;
    } else if (CssFontFamilyDetail::equalsIgnoreAsciiCase(family, "serif") ||
               CssFontFamilyDetail::equalsIgnoreAsciiCase(family, "sans-serif") ||
               CssFontFamilyDetail::equalsIgnoreAsciiCase(family, "cursive") ||
               CssFontFamilyDetail::equalsIgnoreAsciiCase(family, "fantasy") ||
               CssFontFamilyDetail::equalsIgnoreAsciiCase(family, "system-ui")) {
      result = CssGenericFontFamily::Proportional;
    }
    start = i + 1;
  }
  return result;
}
