#include "SdCardFontSystem.h"

#include <GfxRenderer.h>
#include <Logging.h>
#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"

void SdCardFontSystem::begin(GfxRenderer& renderer) {
  registry_.discover();

  SETTINGS.sdFontIdResolver = [](void* ctx, const char* familyName, uint8_t fontSizeEnum) -> int {
    return static_cast<SdCardFontSystem*>(ctx)->resolveFontId(SdFontSlot::Primary, familyName, fontSizeEnum);
  };
  SETTINGS.secondarySdFontIdResolver = [](void* ctx, const char* familyName, uint8_t fontSizeEnum) -> int {
    return static_cast<SdCardFontSystem*>(ctx)->resolveFontId(SdFontSlot::Secondary, familyName, fontSizeEnum);
  };
  SETTINGS.sdFontResolverCtx = this;
  ensureLoaded(renderer);

  LOG_DBG("SDFS", "SD font system ready (%d families discovered)", registry_.getFamilyCount());
}

void SdCardFontSystem::ensureLoaded(GfxRenderer& renderer) {
  const bool registryWasDirty = registryDirty_.exchange(false, std::memory_order_acquire);
  if (registryWasDirty) {
    LOG_DBG("SDFS", "Registry dirty — re-discovering fonts");
    registry_.discover();
  }

  const uint8_t targetPointSize = SETTINGS.getSdFontTargetPointSize();
  const uint8_t sizeStep = SETTINGS.fontSize;
  ensureSlotLoaded(renderer, SdFontSlot::Primary, SETTINGS.sdFontFamilyName, targetPointSize, sizeStep,
                   registryWasDirty);

  if (!SETTINGS.secondarySdFontFamilyName.empty() && SETTINGS.sdFontFamilyName[0] != '\0' &&
      std::strcmp(SETTINGS.secondarySdFontFamilyName.value, SETTINGS.sdFontFamilyName) == 0) {
    manager_.unloadSlot(SdFontSlot::Secondary, renderer);
    return;
  }
  ensureSlotLoaded(renderer, SdFontSlot::Secondary, SETTINGS.secondarySdFontFamilyName.value, targetPointSize,
                   sizeStep, registryWasDirty);
}

void SdCardFontSystem::ensureSlotLoaded(GfxRenderer& renderer, const SdFontSlot slot, char* wantedFamily,
                                        const uint8_t targetPointSize, const uint8_t sizeStep,
                                        const bool registryWasDirty) {
  const char* currentFamily = manager_.currentFamilyName(slot);
  if (wantedFamily[0] == '\0') {
    manager_.unloadSlot(slot, renderer);
    return;
  }

  const auto* family = registry_.findFamily(wantedFamily);
  if (!family) {
    LOG_DBG("SDFS", "SD font family not found for slot %u: %s (clearing)", static_cast<unsigned>(slot), wantedFamily);
    manager_.unloadSlot(slot, renderer);
    wantedFamily[0] = '\0';
    SETTINGS.saveToFile();
    return;
  }

  if (std::strcmp(currentFamily, wantedFamily) == 0) {
    const auto* wantedFile = family->selectFile(targetPointSize, sizeStep);
    const uint8_t wantedPt = wantedFile ? wantedFile->pointSize : 0;
    if (!registryWasDirty && wantedPt == manager_.currentPointSize(slot)) return;
    LOG_DBG("SDFS", "Reloading slot=%u %s: size %u -> %u", static_cast<unsigned>(slot), wantedFamily,
            manager_.currentPointSize(slot), wantedPt);
  }

  if (manager_.loadFamily(slot, *family, renderer, targetPointSize, sizeStep)) {
    LOG_DBG("SDFS", "Loaded SD font slot=%u family=%s", static_cast<unsigned>(slot), wantedFamily);
    return;
  }

  LOG_ERR("SDFS", "Failed to load SD font slot=%u family=%s (clearing)", static_cast<unsigned>(slot), wantedFamily);
  wantedFamily[0] = '\0';
  SETTINGS.saveToFile();
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
  const int slotId = manager_.getFontId(slot, familyName);
  if (slotId != 0 || slot != SdFontSlot::Secondary) return slotId;
  return manager_.getFontId(SdFontSlot::Primary, familyName);
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
