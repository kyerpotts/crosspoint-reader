#pragma once

#include <cstdint>
#include <cstdio>
#include <limits>

class ListState {
 public:
  static constexpr uint8_t MAX_DEPTH = 4;
  static constexpr uint8_t MAX_MARKER_LENGTH = 8;

  void enterList(const bool ordered) {
    if (overflowDepth_ > 0 || depth_ == MAX_DEPTH) {
      if (overflowDepth_ < std::numeric_limits<uint32_t>::max()) {
        ++overflowDepth_;
      }
      return;
    }

    contexts_[depth_++] = {ordered, 1};
  }

  void exitList() {
    if (overflowDepth_ > 0) {
      --overflowDepth_;
    } else if (depth_ > 0) {
      --depth_;
    }
  }

  const char* enterItem() {
    if (itemDepth_ < std::numeric_limits<uint32_t>::max()) {
      ++itemDepth_;
    }

    consumePendingMarker();

    if (overflowDepth_ == 0 && depth_ > 0 && contexts_[depth_ - 1].ordered) {
      auto& ordinal = contexts_[depth_ - 1].nextOrdinal;
      std::snprintf(pendingMarker_, sizeof(pendingMarker_), "%u.", static_cast<unsigned>(ordinal));
      if (ordinal < std::numeric_limits<uint16_t>::max()) {
        ++ordinal;
      }
    } else {
      std::snprintf(pendingMarker_, sizeof(pendingMarker_), "%s", "\xE2\x80\xA2");
    }
    hasPendingMarker_ = true;
    return pendingMarker_;
  }

  void exitItem() {
    if (itemDepth_ == 0) {
      return;
    }
    consumePendingMarker();
    --itemDepth_;
  }

  // Marker pointers remain valid until the next marker or consume operation.
  [[nodiscard]] bool inItem() const { return itemDepth_ > 0; }
  [[nodiscard]] bool hasPendingMarker() const { return hasPendingMarker_; }
  [[nodiscard]] const char* pendingMarker() const { return pendingMarker_; }

  void consumePendingMarker() {
    pendingMarker_[0] = '\0';
    hasPendingMarker_ = false;
  }

 private:
  struct Context {
    bool ordered = false;
    uint16_t nextOrdinal = 1;
  };

  Context contexts_[MAX_DEPTH] = {};
  uint8_t depth_ = 0;
  uint32_t overflowDepth_ = 0;
  uint32_t itemDepth_ = 0;
  char pendingMarker_[MAX_MARKER_LENGTH] = {};
  bool hasPendingMarker_ = false;
};
