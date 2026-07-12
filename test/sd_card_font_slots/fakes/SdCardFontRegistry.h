#pragma once

#include <cstdint>
#include <string>

struct SdCardFontFileInfo {
  std::string path;
  uint8_t pointSize = 0;
};

struct SdCardFontFamilyInfo {
  std::string name;
  SdCardFontFileInfo file;

  const SdCardFontFileInfo* selectFile(uint8_t, uint8_t) const { return file.path.empty() ? nullptr : &file; }
};
