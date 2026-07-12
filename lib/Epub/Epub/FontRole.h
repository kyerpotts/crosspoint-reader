#pragma once

#include <cstdint>

enum class FontRole : uint8_t {
  Primary = 0,
  Secondary = 1,
};

inline constexpr uint8_t SECONDARY_FONT_WORD_FLAG = 0x04;

constexpr FontRole fontRoleFromWordFlags(const uint8_t flags) {
  return (flags & SECONDARY_FONT_WORD_FLAG) != 0 ? FontRole::Secondary : FontRole::Primary;
}

struct FontRenderContext {
  int primaryId = 0;
  int secondaryId = 0;

  [[nodiscard]] int resolve(const FontRole role) const {
    return role == FontRole::Secondary && secondaryId != 0 ? secondaryId : primaryId;
  }

  [[nodiscard]] bool hasSecondary() const { return secondaryId != 0 && secondaryId != primaryId; }
};
