#include <gtest/gtest.h>

#include "Epub/Epub/FontRole.h"

TEST(FontRenderContextTest, ResolvesConfiguredSecondaryFont) {
  FontRenderContext fonts{.primaryId = 10, .secondaryId = 20};

  EXPECT_EQ(fonts.resolve(FontRole::Primary), 10);
  EXPECT_EQ(fonts.resolve(FontRole::Secondary), 20);
}

TEST(FontRenderContextTest, FallsBackToPrimaryWhenSecondaryIsDisabled) {
  FontRenderContext fonts{.primaryId = 10, .secondaryId = 0};

  EXPECT_EQ(fonts.resolve(FontRole::Secondary), 10);
  EXPECT_FALSE(fonts.hasSecondary());
}

TEST(FontRenderContextTest, ReportsDistinctSecondaryOnly) {
  EXPECT_FALSE((FontRenderContext{.primaryId = 10, .secondaryId = 10}).hasSecondary());
  EXPECT_TRUE((FontRenderContext{.primaryId = 10, .secondaryId = 20}).hasSecondary());
}

TEST(FontRoleFlagsTest, PacksSecondaryRoleIntoWordFlags) {
  EXPECT_EQ(fontRoleFromWordFlags(0), FontRole::Primary);
  EXPECT_EQ(fontRoleFromWordFlags(0x01), FontRole::Primary);
  EXPECT_EQ(fontRoleFromWordFlags(SECONDARY_FONT_WORD_FLAG), FontRole::Secondary);
  EXPECT_EQ(fontRoleFromWordFlags(SECONDARY_FONT_WORD_FLAG | 0x01), FontRole::Secondary);
}
