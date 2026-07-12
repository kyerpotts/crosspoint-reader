#include <gtest/gtest.h>

#include <GfxRenderer.h>
#include <SdCardFont.h>
#include <SdCardFontManager.h>
#include <SdCardFontRegistry.h>

namespace {

SdCardFontFamilyInfo family(const char* name, const char* path, const uint8_t pointSize = 16) {
  return SdCardFontFamilyInfo{.name = name, .file = {.path = path, .pointSize = pointSize}};
}

}  // namespace

TEST(SdCardFontSlotsTest, LoadingSecondaryPreservesPrimary) {
  GfxRenderer renderer;
  SdCardFontManager manager;
  const auto primary = family("Primary", "/fonts/primary.cpfont");
  const auto secondary = family("Mono", "/fonts/mono.cpfont");

  ASSERT_TRUE(manager.loadFamily(SdFontSlot::Primary, primary, renderer, 16, 0));
  const int primaryId = manager.getFontId(SdFontSlot::Primary, "Primary");
  ASSERT_NE(primaryId, 0);

  ASSERT_TRUE(manager.loadFamily(SdFontSlot::Secondary, secondary, renderer, 16, 0));
  EXPECT_EQ(manager.getFontId(SdFontSlot::Primary, "Primary"), primaryId);
  EXPECT_TRUE(renderer.hasSdFont(primaryId));
  EXPECT_NE(manager.getFontId(SdFontSlot::Secondary, "Mono"), 0);
}

TEST(SdCardFontSlotsTest, ReplacingPrimaryPreservesSecondary) {
  GfxRenderer renderer;
  SdCardFontManager manager;
  ASSERT_TRUE(manager.loadFamily(SdFontSlot::Primary, family("Primary", "/fonts/primary.cpfont"), renderer, 16, 0));
  ASSERT_TRUE(manager.loadFamily(SdFontSlot::Secondary, family("Mono", "/fonts/mono.cpfont"), renderer, 16, 0));
  const int secondaryId = manager.getFontId(SdFontSlot::Secondary, "Mono");

  ASSERT_TRUE(manager.loadFamily(SdFontSlot::Primary, family("Serif", "/fonts/serif.cpfont"), renderer, 16, 0));

  EXPECT_EQ(manager.getFontId(SdFontSlot::Secondary, "Mono"), secondaryId);
  EXPECT_TRUE(renderer.hasSdFont(secondaryId));
}

TEST(SdCardFontSlotsTest, UnloadingOneSlotLeavesOtherRegistered) {
  GfxRenderer renderer;
  SdCardFontManager manager;
  ASSERT_TRUE(manager.loadFamily(SdFontSlot::Primary, family("Primary", "/fonts/primary.cpfont"), renderer, 16, 0));
  ASSERT_TRUE(manager.loadFamily(SdFontSlot::Secondary, family("Mono", "/fonts/mono.cpfont"), renderer, 16, 0));
  const int secondaryId = manager.getFontId(SdFontSlot::Secondary, "Mono");

  manager.unloadSlot(SdFontSlot::Primary, renderer);

  EXPECT_FALSE(manager.isLoaded(SdFontSlot::Primary));
  EXPECT_TRUE(manager.isLoaded(SdFontSlot::Secondary));
  EXPECT_TRUE(renderer.hasSdFont(secondaryId));
}

TEST(SdCardFontSlotsTest, FailedSecondaryLoadLeavesPrimaryIntact) {
  GfxRenderer renderer;
  SdCardFontManager manager;
  ASSERT_TRUE(manager.loadFamily(SdFontSlot::Primary, family("Primary", "/fonts/primary.cpfont"), renderer, 16, 0));
  const int primaryId = manager.getFontId(SdFontSlot::Primary, "Primary");

  EXPECT_FALSE(manager.loadFamily(SdFontSlot::Secondary, family("Broken", "/fonts/fail.cpfont"), renderer, 16, 0));

  EXPECT_EQ(manager.getFontId(SdFontSlot::Primary, "Primary"), primaryId);
  EXPECT_TRUE(renderer.hasSdFont(primaryId));
  EXPECT_FALSE(manager.isLoaded(SdFontSlot::Secondary));
}

TEST(SdCardFontSlotsTest, UnloadAllReleasesPrimaryThenSecondary) {
  GfxRenderer renderer;
  SdCardFontManager manager;
  ASSERT_TRUE(manager.loadFamily(SdFontSlot::Primary, family("Primary", "/fonts/primary.cpfont"), renderer, 16, 0));
  ASSERT_TRUE(manager.loadFamily(SdFontSlot::Secondary, family("Mono", "/fonts/mono.cpfont"), renderer, 16, 0));
  const int primaryId = manager.getFontId(SdFontSlot::Primary, "Primary");
  const int secondaryId = manager.getFontId(SdFontSlot::Secondary, "Mono");

  manager.unloadAll(renderer);

  ASSERT_EQ(renderer.removalOrder.size(), 2u);
  EXPECT_EQ(renderer.removalOrder[0], primaryId);
  EXPECT_EQ(renderer.removalOrder[1], secondaryId);
  EXPECT_EQ(SdCardFont::liveCount, 0);
}
