#pragma once

#include <EpdFontFamily.h>

#include <cstdint>

struct GlyphDemandEntry {
  uint32_t codepoint = 0;
  uint8_t styleMask = 0;
};

class GlyphDemandCollector {
 public:
  GlyphDemandCollector(GlyphDemandEntry* entries, uint16_t capacity);

  bool add(uint32_t codepoint, EpdFontFamily::Style style);
  bool addUtf8(const char* text, EpdFontFamily::Style style);
  bool mergeFrom(const GlyphDemandCollector& other);
  void reset();

  [[nodiscard]] bool overflowed() const { return overflowed_; }
  [[nodiscard]] uint16_t size() const { return size_; }
  [[nodiscard]] const GlyphDemandEntry* entries() const { return entries_; }

  static constexpr uint8_t styleBit(const EpdFontFamily::Style style) {
    return static_cast<uint8_t>(1U << (static_cast<uint8_t>(style) & 0x03U));
  }

 private:
  GlyphDemandEntry* entries_ = nullptr;
  uint16_t capacity_ = 0;
  uint16_t size_ = 0;
  bool overflowed_ = false;
};
