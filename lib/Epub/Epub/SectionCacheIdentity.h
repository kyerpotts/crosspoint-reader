#pragma once

#include <cstdint>

inline constexpr uint8_t SECTION_FILE_VERSION = 46;

struct SectionCacheIdentity {
  uint8_t version;
  int primaryFontId;
  int secondaryFontId;

  constexpr bool matches(const int expectedPrimaryFontId, const int expectedSecondaryFontId) const {
    return version == SECTION_FILE_VERSION && primaryFontId == expectedPrimaryFontId &&
           secondaryFontId == expectedSecondaryFontId;
  }
};
