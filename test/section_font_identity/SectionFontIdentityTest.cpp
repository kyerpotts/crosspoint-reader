#include <gtest/gtest.h>

#include "Epub/Epub/SectionCacheIdentity.h"

TEST(SectionFontIdentityTest, AcceptsMatchingPrimaryAndSecondaryFonts) {
  const SectionCacheIdentity identity{SECTION_FILE_VERSION, 10, 20};

  EXPECT_TRUE(identity.matches(10, 20));
}

TEST(SectionFontIdentityTest, RejectsChangedPrimaryFont) {
  const SectionCacheIdentity identity{SECTION_FILE_VERSION, 10, 20};

  EXPECT_FALSE(identity.matches(11, 20));
}

TEST(SectionFontIdentityTest, RejectsChangedSecondaryFont) {
  const SectionCacheIdentity identity{SECTION_FILE_VERSION, 10, 20};

  EXPECT_FALSE(identity.matches(10, 21));
}

TEST(SectionFontIdentityTest, RejectsPreviousCacheVersion) {
  const SectionCacheIdentity identity{static_cast<uint8_t>(SECTION_FILE_VERSION - 1), 10, 20};

  EXPECT_FALSE(identity.matches(10, 20));
}
