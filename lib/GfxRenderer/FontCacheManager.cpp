#include "FontCacheManager.h"

#include <FontDecompressor.h>
#include <GlyphDemandCollector.h>
#include <Logging.h>
#include <SdCardFont.h>
#include <Utf8.h>

#include <cstring>

namespace {

uint8_t encodeUtf8(const uint32_t codepoint, uint8_t out[4]) {
  if (codepoint <= 0x7F) {
    out[0] = static_cast<uint8_t>(codepoint);
    return 1;
  }
  if (codepoint <= 0x7FF) {
    out[0] = static_cast<uint8_t>(0xC0 | (codepoint >> 6));
    out[1] = static_cast<uint8_t>(0x80 | (codepoint & 0x3F));
    return 2;
  }
  if (codepoint <= 0xFFFF) {
    out[0] = static_cast<uint8_t>(0xE0 | (codepoint >> 12));
    out[1] = static_cast<uint8_t>(0x80 | ((codepoint >> 6) & 0x3F));
    out[2] = static_cast<uint8_t>(0x80 | (codepoint & 0x3F));
    return 3;
  }
  out[0] = static_cast<uint8_t>(0xF0 | (codepoint >> 18));
  out[1] = static_cast<uint8_t>(0x80 | ((codepoint >> 12) & 0x3F));
  out[2] = static_cast<uint8_t>(0x80 | ((codepoint >> 6) & 0x3F));
  out[3] = static_cast<uint8_t>(0x80 | (codepoint & 0x3F));
  return 4;
}

}  // namespace

FontCacheManager::FontCacheManager(const std::map<int, EpdFontFamily>& fontMap,
                                   const std::map<int, SdCardFont*>& sdCardFonts)
    : fontMap_(fontMap), sdCardFonts_(sdCardFonts) {}

void FontCacheManager::setFontDecompressor(FontDecompressor* d) { fontDecompressor_ = d; }

void FontCacheManager::clearCache() {
  if (fontDecompressor_) fontDecompressor_->clearCache();
  for (auto& [id, font] : sdCardFonts_) {
    font->clearCache();
  }
}

void FontCacheManager::clearCache(const int fontId) {
  if (const auto it = sdCardFonts_.find(fontId); it != sdCardFonts_.end()) {
    it->second->clearCache();
    return;
  }
  if (fontDecompressor_) fontDecompressor_->clearCache();
}

bool FontCacheManager::prewarmCache(int fontId, const char* utf8Text, uint8_t styleMask) {
  // SD card font prewarm path: prewarm all requested styles in one call
  auto it = sdCardFonts_.find(fontId);
  if (it != sdCardFonts_.end()) {
    int missed = it->second->prewarm(utf8Text, styleMask);
    if (missed > 0) {
      LOG_DBG("FCM", "prewarmCache(SD): %d glyph(s) not found (styleMask=0x%02X)", missed, styleMask);
    }
    return !it->second->lastPrewarmFailed();
  }

  // Standard compressed font prewarm path: loop over all requested styles
  if (!fontDecompressor_ || fontMap_.count(fontId) == 0) return false;

  // Reverse iteration is harmless now; the decompressor keeps one retained page slot per style.
  for (int8_t i = 3; i >= 0; i--) {
    if (!(styleMask & (1 << i))) continue;
    auto style = static_cast<EpdFontFamily::Style>(i);
    const EpdFontData* data = fontMap_.at(fontId).getData(style);
    if (!data || !data->groups) continue;
    int missed = fontDecompressor_->prewarmCache(data, utf8Text);
    if (missed > 0) {
      LOG_DBG("FCM", "prewarmCache: %d glyph(s) not cached for style %d", missed, i);
    }
  }
  return true;
}

bool FontCacheManager::prewarmDemand(const int fontId, const GlyphDemandEntry* entries, const uint16_t count,
                                     uint8_t* scratch, const size_t scratchBytes) {
  if (!entries || count == 0) return true;
  if (!scratch || scratchBytes < static_cast<size_t>(count) * sizeof(uint32_t) + 1) return false;

  if (const auto it = sdCardFonts_.find(fontId); it != sdCardFonts_.end()) {
    const int missed = it->second->prewarmDemand(entries, count, scratch, scratchBytes);
    if (missed > 0) {
      LOG_DBG("FCM", "prewarmDemand(SD): %d glyph(s) not found", missed);
    }
    return !it->second->lastPrewarmFailed();
  }

  if (!fontDecompressor_ || fontMap_.count(fontId) == 0) return false;
  for (uint8_t styleIndex = 0; styleIndex < 4; ++styleIndex) {
    const uint8_t styleBit = static_cast<uint8_t>(1U << styleIndex);
    uint16_t codepointCount = 0;
    for (uint16_t i = 0; i < count; ++i) {
      if ((entries[i].styleMask & styleBit) == 0) continue;
      memcpy(scratch + static_cast<size_t>(codepointCount) * sizeof(uint32_t), &entries[i].codepoint, sizeof(uint32_t));
      ++codepointCount;
    }
    if (codepointCount == 0) continue;

    uint8_t* write = scratch + scratchBytes - 1;
    *write = '\0';
    for (uint16_t i = codepointCount; i > 0; --i) {
      uint32_t codepoint = 0;
      memcpy(&codepoint, scratch + static_cast<size_t>(i - 1) * sizeof(uint32_t), sizeof(uint32_t));
      uint8_t encoded[4];
      const uint8_t encodedBytes = encodeUtf8(codepoint, encoded);
      write -= encodedBytes;
      memcpy(write, encoded, encodedBytes);
    }

    const auto style = static_cast<EpdFontFamily::Style>(styleIndex);
    const EpdFontData* data = fontMap_.at(fontId).getData(style);
    if (!data || !data->groups) continue;
    const int missed = fontDecompressor_->prewarmCache(data, reinterpret_cast<const char*>(write));
    if (missed > 0) {
      LOG_DBG("FCM", "prewarmDemand: %d glyph(s) not cached for style %u", missed, styleIndex);
    }
  }
  return true;
}

void FontCacheManager::logStats(const char* label) {
  if (fontDecompressor_) fontDecompressor_->logStats(label);
  for (auto& [id, font] : sdCardFonts_) {
    font->logStats(label);
  }
}

void FontCacheManager::resetStats() {
  if (fontDecompressor_) fontDecompressor_->resetStats();
  for (auto& [id, font] : sdCardFonts_) {
    font->resetStats();
  }
}
