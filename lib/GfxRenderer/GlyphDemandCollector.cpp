#include "GlyphDemandCollector.h"

#include <Utf8.h>

GlyphDemandCollector::GlyphDemandCollector(GlyphDemandEntry* entries, const uint16_t capacity)
    : entries_(entries), capacity_(capacity), overflowed_(entries == nullptr || capacity == 0) {}

bool GlyphDemandCollector::add(const uint32_t codepoint, const EpdFontFamily::Style style) {
  if (overflowed_) {
    return false;
  }

  const uint8_t bit = styleBit(style);
  for (uint16_t i = 0; i < size_; ++i) {
    if (entries_[i].codepoint == codepoint) {
      entries_[i].styleMask = static_cast<uint8_t>(entries_[i].styleMask | bit);
      return true;
    }
  }

  if (size_ == capacity_) {
    overflowed_ = true;
    return false;
  }

  entries_[size_++] = GlyphDemandEntry{codepoint, bit};
  return true;
}

bool GlyphDemandCollector::addUtf8(const char* text, const EpdFontFamily::Style style) {
  if (!text || overflowed_) {
    return false;
  }

  const auto* cursor = reinterpret_cast<const unsigned char*>(text);
  while (*cursor) {
    const uint32_t codepoint = utf8NextCodepoint(&cursor);
    if (codepoint == 0) {
      break;
    }
    if (!add(codepoint, style)) {
      return false;
    }
  }
  return true;
}

void GlyphDemandCollector::reset() {
  size_ = 0;
  overflowed_ = entries_ == nullptr || capacity_ == 0;
}
