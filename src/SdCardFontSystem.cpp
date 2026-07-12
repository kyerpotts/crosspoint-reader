#include "SdCardFontSystem.h"

#include <GfxRenderer.h>
#include <Logging.h>
#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"

void SdCardFontSystem::begin(GfxRenderer& renderer) {
  registry_.discover();

  // Register this system as the SD font ID resolver in settings.
  // Uses a static trampoline since CrossPointSettings stores a plain function pointer.
  SETTINGS.sdFontIdResolver = [](void* ctx, const char* familyName, uint8_t fontSizeEnum) -> int {
    return static_cast<SdCardFontSystem*>(ctx)->resolveFontId(familyName, fontSizeEnum);
  };
  SETTINGS.sdFontResolverCtx = this;

  // If user has a saved SD font selection, load it
  if (SETTINGS.sdFontFamilyName[0] != '\0') {
    const auto* family = registry_.findFamily(SETTINGS.sdFontFamilyName);
    if (family) {
      if (manager_.loadFamily(SdFontSlot::Primary, *family, renderer, SETTINGS.getSdFontTargetPointSize(),
                              SETTINGS.fontSize)) {
        LOG_DBG("SDFS", "Loaded SD card font family: %s", SETTINGS.sdFontFamilyName);
      } else {
        LOG_ERR("SDFS", "Failed to load SD font family: %s (clearing)", SETTINGS.sdFontFamilyName);
        SETTINGS.sdFontFamilyName[0] = '\0';
        SETTINGS.saveToFile();
      }
    } else {
      LOG_DBG("SDFS", "SD font family not found on card: %s (clearing)", SETTINGS.sdFontFamilyName);
      SETTINGS.sdFontFamilyName[0] = '\0';
      SETTINGS.saveToFile();
    }
  }

  LOG_DBG("SDFS", "SD font system ready (%d families discovered)", registry_.getFamilyCount());
}

void SdCardFontSystem::ensureLoaded(GfxRenderer& renderer) {
  // If the web server (or another task) installed/deleted fonts, re-discover.
  // Track whether we just re-discovered so we can force a reload below even
  // when the wanted family/size still maps to the same point size — the file
  // contents on disk may have changed (e.g. user re-uploaded a new build).
  const bool registryWasDirty = registryDirty_.exchange(false, std::memory_order_acquire);
  if (registryWasDirty) {
    LOG_DBG("SDFS", "Registry dirty — re-discovering fonts");
    registry_.discover();
  }

  const char* wantedFamily = SETTINGS.sdFontFamilyName;
  const char* currentFamily = manager_.currentFamilyName(SdFontSlot::Primary);
  const uint8_t targetPointSize = SETTINGS.getSdFontTargetPointSize();
  const uint8_t sizeStep = SETTINGS.fontSize;

  if (wantedFamily[0] == '\0') {
    manager_.unloadSlot(SdFontSlot::Primary, renderer);
    return;
  }

  // Reload if family changed OR if the user-selected size maps to a
  // different file than what's currently loaded OR if the registry was
  // just rediscovered (file may have been replaced on disk).
  const bool familyMatches = std::strcmp(currentFamily, wantedFamily) == 0;
  if (familyMatches) {
    const auto* family = registry_.findFamily(wantedFamily);
    if (!family) {
      LOG_DBG("SDFS", "SD font family disappeared: %s (clearing)", wantedFamily);
      manager_.unloadSlot(SdFontSlot::Primary, renderer);
      SETTINGS.sdFontFamilyName[0] = '\0';
      SETTINGS.saveToFile();
      return;
    }
    const auto* wantedFile = family->selectFile(targetPointSize, sizeStep);
    uint8_t wantedPt = wantedFile ? wantedFile->pointSize : 0;
    if (!registryWasDirty && wantedPt == manager_.currentPointSize(SdFontSlot::Primary)) return;
    LOG_DBG("SDFS", "Reloading %s: size %u -> %u (target %u step %u)%s", wantedFamily,
            manager_.currentPointSize(SdFontSlot::Primary), wantedPt, targetPointSize, sizeStep,
            registryWasDirty ? " [registry dirty]" : "");
  }

  if (currentFamily[0] != '\0') {
    manager_.unloadSlot(SdFontSlot::Primary, renderer);
  }

  const auto* family = registry_.findFamily(wantedFamily);
  if (family) {
    if (manager_.loadFamily(SdFontSlot::Primary, *family, renderer, targetPointSize, sizeStep)) {
      LOG_DBG("SDFS", "Loaded SD font family: %s", wantedFamily);
    } else {
      LOG_ERR("SDFS", "Failed to load SD font family: %s (clearing)", wantedFamily);
      SETTINGS.sdFontFamilyName[0] = '\0';
      SETTINGS.saveToFile();
    }
  } else {
    LOG_DBG("SDFS", "SD font family not found: %s (clearing)", wantedFamily);
    SETTINGS.sdFontFamilyName[0] = '\0';
    SETTINGS.saveToFile();
  }
}

void SdCardFontSystem::releaseLoadedFont(GfxRenderer& renderer) {
  if (!manager_.isLoaded(SdFontSlot::Primary) && !manager_.isLoaded(SdFontSlot::Secondary)) return;

  char primaryFamily[64] = {};
  char secondaryFamily[64] = {};
  std::snprintf(primaryFamily, sizeof(primaryFamily), "%s", manager_.currentFamilyName(SdFontSlot::Primary));
  std::snprintf(secondaryFamily, sizeof(secondaryFamily), "%s", manager_.currentFamilyName(SdFontSlot::Secondary));
  manager_.unloadAll(renderer);
  LOG_DBG("SDFS", "Released SD fonts before low-memory operation: primary=%s secondary=%s", primaryFamily,
          secondaryFamily);
}

void SdCardFontSystem::releaseForNetwork(GfxRenderer& renderer) {
  releaseLoadedFont(renderer);

  const int familyCount = registry_.getFamilyCount();
  if (familyCount == 0) return;

  registry_.clear();
  registryDirty_.store(true, std::memory_order_release);
  LOG_DBG("SDFS", "Released SD font registry before network operation (%d families)", familyCount);
}

int SdCardFontSystem::resolveFontId(const char* familyName, uint8_t fontSizeEnum) const {
  return resolveFontId(SdFontSlot::Primary, familyName, fontSizeEnum);
}

int SdCardFontSystem::resolveFontId(const SdFontSlot slot, const char* familyName,
                                    uint8_t /*fontSizeEnum*/) const {
  return manager_.getFontId(slot, familyName);
}

bool SdCardFontSystem::changeReaderFontSize(const bool larger) {
  refreshIfDirty();

  if (SETTINGS.sdFontFamilyName[0] != '\0') {
    const auto* family = registry_.findFamily(SETTINGS.sdFontFamilyName);
    if (family) {
      const auto sizes = family->availableSizes();
      if (sizes.size() > 1) {
        uint8_t current = SETTINGS.fontSize < sizes.size() ? SETTINGS.fontSize : static_cast<uint8_t>(sizes.size() - 1);
        if (larger) {
          current = static_cast<uint8_t>((current + 1) % sizes.size());
        } else {
          current = current == 0 ? static_cast<uint8_t>(sizes.size() - 1) : static_cast<uint8_t>(current - 1);
        }
        SETTINGS.fontSize = current;
        return true;
      }
    }
  }

  return SETTINGS.changeReaderFontSize(larger);
}
