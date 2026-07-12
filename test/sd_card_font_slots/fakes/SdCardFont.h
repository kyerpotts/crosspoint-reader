#pragma once

#include <cstdint>
#include <cstring>

class SdCardFont {
 public:
  SdCardFont() { ++liveCount; }
  ~SdCardFont() { --liveCount; }

  bool load(const char* path) {
    if (std::strstr(path, "fail") != nullptr) return false;
    hash_ = 2166136261u;
    while (*path) {
      hash_ ^= static_cast<uint8_t>(*path++);
      hash_ *= 16777619u;
    }
    return true;
  }

  uint32_t contentHash() const { return hash_; }
  const void* getEpdFont(int) const { return nullptr; }
  uint8_t styleCount() const { return 4; }

  inline static int liveCount = 0;

 private:
  uint32_t hash_ = 0;
};
