#include <gtest/gtest.h>

#include <cstring>

#include "FontFamilyName.h"

TEST(FontFamilyNameTest, DefaultsToDisabled) {
  const FontFamilyName secondary;

  EXPECT_TRUE(secondary.empty());
  EXPECT_EQ(secondary[0], '\0');
}

TEST(FontFamilyNameTest, StoresPrimaryAndSecondaryIndependently) {
  FontFamilyName primary;
  FontFamilyName secondary;

  primary.assign("Literata");
  secondary.assign("JetBrains Mono");
  EXPECT_STREQ(primary.value, "Literata");
  EXPECT_STREQ(secondary.value, "JetBrains Mono");
}

TEST(FontFamilyNameTest, BoundsAndTerminatesLongNames) {
  char oversized[96];
  std::memset(oversized, 'x', sizeof(oversized));
  oversized[sizeof(oversized) - 1] = '\0';
  FontFamilyName name;

  name.assign(oversized);

  EXPECT_EQ(std::strlen(name.value), FontFamilyName::CAPACITY - 1);
  EXPECT_EQ(name[FontFamilyName::CAPACITY - 1], '\0');
}

TEST(FontFamilyNameTest, ClearingMissingFamilyDisablesSelection) {
  FontFamilyName secondary;
  secondary.assign("Missing Mono");

  secondary.clear();

  EXPECT_TRUE(secondary.empty());
}
