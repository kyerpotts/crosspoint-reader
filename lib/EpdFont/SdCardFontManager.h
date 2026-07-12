#pragma once

#include <cstddef>
#include <cstdint>

class GfxRenderer;
class SdCardFont;
struct SdCardFontFamilyInfo;

enum class SdFontSlot : uint8_t { Primary = 0, Secondary = 1 };

class SdCardFontManager {
 public:
  SdCardFontManager() = default;
  ~SdCardFontManager();
  SdCardFontManager(const SdCardFontManager&) = delete;
  SdCardFontManager& operator=(const SdCardFontManager&) = delete;

  // Loads one selected family size into the requested independent slot. A
  // failure leaves the other slot untouched.
  bool loadFamily(SdFontSlot slot, const SdCardFontFamilyInfo& family, GfxRenderer& renderer,
                  uint8_t targetPointSize, uint8_t sizeStep);

  void unloadSlot(SdFontSlot slot, GfxRenderer& renderer);
  void unloadAll(GfxRenderer& renderer);

  int getFontId(SdFontSlot slot, const char* familyName) const;
  const char* currentFamilyName(SdFontSlot slot) const;
  uint8_t currentPointSize(SdFontSlot slot) const;
  bool isLoaded(SdFontSlot slot) const;

 private:
  struct LoadedFontSlot {
    SdCardFont* font = nullptr;
    int fontId = 0;
    uint8_t pointSize = 0;
    char familyName[64] = {};
  };

  static constexpr std::size_t SLOT_COUNT = 2;
  static std::size_t slotIndex(SdFontSlot slot) { return static_cast<std::size_t>(slot); }
  static int computeFontId(uint32_t contentHash, const char* familyName, uint8_t pointSize);

  LoadedFontSlot slots_[SLOT_COUNT];
};
