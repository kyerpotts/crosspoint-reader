#pragma once

#include <EpdFontFamily.h>

#include <map>
#include <vector>

class SdCardFont;

class GfxRenderer {
 public:
  const std::map<int, EpdFontFamily>& getFontMap() const { return fontMap_; }
  void registerSdCardFont(int fontId, SdCardFont* font) { sdFonts_[fontId] = font; }
  void insertFont(int fontId, EpdFontFamily font) { fontMap_.emplace(fontId, font); }
  void removeFont(int fontId) {
    removalOrder.push_back(fontId);
    fontMap_.erase(fontId);
    sdFonts_.erase(fontId);
  }

  bool hasSdFont(int fontId) const { return sdFonts_.count(fontId) != 0; }

  std::vector<int> removalOrder;

 private:
  std::map<int, EpdFontFamily> fontMap_;
  std::map<int, SdCardFont*> sdFonts_;
};
