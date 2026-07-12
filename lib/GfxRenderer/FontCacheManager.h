#pragma once

#include <EpdFontFamily.h>

#include <cstddef>
#include <cstdint>
#include <map>

struct GlyphDemandEntry;

class FontDecompressor;
class SdCardFont;

class FontCacheManager {
 public:
  FontCacheManager(const std::map<int, EpdFontFamily>& fontMap, const std::map<int, SdCardFont*>& sdCardFonts);

  void setFontDecompressor(FontDecompressor* d);

  void clearCache();
  void clearCache(int fontId);
  bool prewarmCache(int fontId, const char* utf8Text, uint8_t styleMask = 0x0F);
  bool prewarmDemand(int fontId, const GlyphDemandEntry* entries, uint16_t count, uint8_t* scratch,
                     size_t scratchBytes);
  void logStats(const char* label = "render");
  void resetStats();

  // The FontDecompressor pointer, needed by GfxRenderer::getGlyphBitmap()
  FontDecompressor* getDecompressor() const { return fontDecompressor_; }

 private:
  const std::map<int, EpdFontFamily>& fontMap_;
  const std::map<int, SdCardFont*>& sdCardFonts_;
  FontDecompressor* fontDecompressor_ = nullptr;
};
