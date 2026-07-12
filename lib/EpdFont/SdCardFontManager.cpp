#include "SdCardFontManager.h"

#include <EpdFontFamily.h>
#include <GfxRenderer.h>
#include <Logging.h>
#include <SdCardFont.h>
#include <SdCardFontRegistry.h>

#include <cstdio>
#include <cstring>
#include <new>

SdCardFontManager::~SdCardFontManager() {
  for (auto& slot : slots_) {
    delete slot.font;
  }
}

// FNV-1a continuation: seeds with contentHash, then hashes family name + point size.
// Produces a deterministic ID that changes when the font file contents change.
int SdCardFontManager::computeFontId(uint32_t contentHash, const char* familyName, uint8_t pointSize) {
  static constexpr uint32_t FNV_PRIME = 16777619u;
  uint32_t hash = contentHash;
  while (*familyName) {
    hash ^= static_cast<uint8_t>(*familyName++);
    hash *= FNV_PRIME;
  }
  hash ^= pointSize;
  hash *= FNV_PRIME;
  const int id = static_cast<int>(hash);
  return id != 0 ? id : 1;
}

bool SdCardFontManager::loadFamily(const SdFontSlot slot, const SdCardFontFamilyInfo& family, GfxRenderer& renderer,
                                   const uint8_t targetPointSize, const uint8_t sizeStep) {
  const SdCardFontFileInfo* selected = family.selectFile(targetPointSize, sizeStep);
  if (!selected) {
    LOG_ERR("SDMGR", "Family %s has no files to load", family.name.c_str());
    return false;
  }

  auto* candidate = new (std::nothrow) SdCardFont();
  if (!candidate) {
    LOG_ERR("SDMGR", "Failed to allocate SdCardFont for %s", selected->path.c_str());
    return false;
  }
  if (!candidate->load(selected->path.c_str())) {
    LOG_ERR("SDMGR", "Failed to load %s", selected->path.c_str());
    delete candidate;
    return false;
  }

  const int candidateId = computeFontId(candidate->contentHash(), family.name.c_str(), selected->pointSize);
  LoadedFontSlot& target = slots_[slotIndex(slot)];
  const bool replacingSameId = target.font && target.fontId == candidateId;
  if (renderer.getFontMap().count(candidateId) != 0 && !replacingSameId) {
    LOG_ERR("SDMGR", "Font ID %d collides with an existing font, skipping %s", candidateId,
            selected->path.c_str());
    delete candidate;
    return false;
  }

  unloadSlot(slot, renderer);
  renderer.registerSdCardFont(candidateId, candidate);
  renderer.insertFont(candidateId,
                      EpdFontFamily(candidate->getEpdFont(0), candidate->getEpdFont(1), candidate->getEpdFont(2),
                                    candidate->getEpdFont(3)));

  target.font = candidate;
  target.fontId = candidateId;
  target.pointSize = selected->pointSize;
  std::snprintf(target.familyName, sizeof(target.familyName), "%s", family.name.c_str());

  LOG_DBG("SDMGR", "Loaded slot=%u %s size=%u id=%d styles=%u (target=%u step=%u)",
          static_cast<unsigned>(slot), selected->path.c_str(), selected->pointSize, candidateId,
          candidate->styleCount(), targetPointSize, sizeStep);
  return true;
}

void SdCardFontManager::unloadSlot(const SdFontSlot slot, GfxRenderer& renderer) {
  LoadedFontSlot& target = slots_[slotIndex(slot)];
  if (!target.font) return;

  renderer.removeFont(target.fontId);
  delete target.font;
  target = {};
}

void SdCardFontManager::unloadAll(GfxRenderer& renderer) {
  unloadSlot(SdFontSlot::Primary, renderer);
  unloadSlot(SdFontSlot::Secondary, renderer);
}

int SdCardFontManager::getFontId(const SdFontSlot slot, const char* familyName) const {
  const LoadedFontSlot& target = slots_[slotIndex(slot)];
  if (!target.font || !familyName || std::strcmp(target.familyName, familyName) != 0) return 0;
  return target.fontId;
}

const char* SdCardFontManager::currentFamilyName(const SdFontSlot slot) const {
  return slots_[slotIndex(slot)].familyName;
}

uint8_t SdCardFontManager::currentPointSize(const SdFontSlot slot) const {
  return slots_[slotIndex(slot)].pointSize;
}

bool SdCardFontManager::isLoaded(const SdFontSlot slot) const { return slots_[slotIndex(slot)].font != nullptr; }
