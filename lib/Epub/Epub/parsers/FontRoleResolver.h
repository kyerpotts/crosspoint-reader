#pragma once

#include <cstring>

#include "../FontRole.h"
#include "../css/CssStyle.h"


inline FontRole resolveElementFontRole(const char* tag, const FontRole inherited,
                                       const CssGenericFontFamily genericFamily,
                                       const bool publisherStylesEnabled) {
  if (tag && (std::strcmp(tag, "code") == 0 || std::strcmp(tag, "kbd") == 0 ||
              std::strcmp(tag, "samp") == 0 || std::strcmp(tag, "tt") == 0 || std::strcmp(tag, "pre") == 0)) {
    return FontRole::Secondary;
  }
  if (!publisherStylesEnabled) {
    return inherited;
  }
  if (genericFamily == CssGenericFontFamily::Monospace) {
    return FontRole::Secondary;
  }
  if (genericFamily == CssGenericFontFamily::Proportional) {
    return FontRole::Primary;
  }
  return inherited;
}
