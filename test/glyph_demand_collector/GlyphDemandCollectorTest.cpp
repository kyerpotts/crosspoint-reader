#include <gtest/gtest.h>

#include <array>

#include "GfxRenderer/GlyphDemandCollector.h"

TEST(GlyphDemandCollectorTest, MergesStylesForDuplicateCodepoints) {
  std::array<GlyphDemandEntry, 8> storage{};
  GlyphDemandCollector demand(storage.data(), storage.size());

  EXPECT_TRUE(demand.add(U'a', EpdFontFamily::REGULAR));
  EXPECT_TRUE(demand.add(U'a', EpdFontFamily::BOLD));

  ASSERT_EQ(demand.size(), 1U);
  EXPECT_EQ(demand.entries()[0].codepoint, U'a');
  EXPECT_EQ(demand.entries()[0].styleMask, GlyphDemandCollector::styleBit(EpdFontFamily::REGULAR) |
                                               GlyphDemandCollector::styleBit(EpdFontFamily::BOLD));
}

TEST(GlyphDemandCollectorTest, DecodesUtf8AndKeepsInsertionOrder) {
  std::array<GlyphDemandEntry, 8> storage{};
  GlyphDemandCollector demand(storage.data(), storage.size());

  EXPECT_TRUE(demand.addUtf8("A\xC3\xA9\xE6\x97\xA5", EpdFontFamily::ITALIC));

  ASSERT_EQ(demand.size(), 3U);
  EXPECT_EQ(demand.entries()[0].codepoint, U'A');
  EXPECT_EQ(demand.entries()[1].codepoint, U'é');
  EXPECT_EQ(demand.entries()[2].codepoint, U'日');
  EXPECT_EQ(demand.entries()[2].styleMask, GlyphDemandCollector::styleBit(EpdFontFamily::ITALIC));
}

TEST(GlyphDemandCollectorTest, PreservesBaseStyleWhenDecorationBitsArePresent) {
  std::array<GlyphDemandEntry, 4> storage{};
  GlyphDemandCollector demand(storage.data(), storage.size());
  const auto decoratedBold = static_cast<EpdFontFamily::Style>(EpdFontFamily::BOLD | EpdFontFamily::UNDERLINE);

  ASSERT_TRUE(demand.add(U'x', decoratedBold));

  EXPECT_EQ(demand.entries()[0].styleMask, GlyphDemandCollector::styleBit(EpdFontFamily::BOLD));
}

TEST(GlyphDemandCollectorTest, FillsExactCapacityThenReportsStickyOverflow) {
  std::array<GlyphDemandEntry, 2> storage{};
  GlyphDemandCollector demand(storage.data(), storage.size());

  EXPECT_TRUE(demand.add(U'a', EpdFontFamily::REGULAR));
  EXPECT_TRUE(demand.add(U'b', EpdFontFamily::REGULAR));
  EXPECT_FALSE(demand.overflowed());
  EXPECT_FALSE(demand.add(U'c', EpdFontFamily::REGULAR));
  EXPECT_TRUE(demand.overflowed());
  EXPECT_FALSE(demand.add(U'd', EpdFontFamily::REGULAR));
  EXPECT_EQ(demand.size(), 2U);
}

TEST(GlyphDemandCollectorTest, ResetReusesCallerStorage) {
  std::array<GlyphDemandEntry, 2> storage{};
  GlyphDemandCollector demand(storage.data(), storage.size());
  ASSERT_TRUE(demand.add(U'a', EpdFontFamily::REGULAR));
  ASSERT_FALSE(demand.addUtf8("bc", EpdFontFamily::REGULAR));

  demand.reset();

  EXPECT_EQ(demand.size(), 0U);
  EXPECT_FALSE(demand.overflowed());
  EXPECT_TRUE(demand.add(U'z', EpdFontFamily::BOLD_ITALIC));
  EXPECT_EQ(demand.entries(), storage.data());
}

TEST(GlyphDemandCollectorTest, RejectsInvalidStorageWithoutWriting) {
  GlyphDemandCollector nullDemand(nullptr, 4);
  std::array<GlyphDemandEntry, 1> storage{};
  GlyphDemandCollector zeroDemand(storage.data(), 0);

  EXPECT_FALSE(nullDemand.add(U'a', EpdFontFamily::REGULAR));
  EXPECT_TRUE(nullDemand.overflowed());
  EXPECT_FALSE(zeroDemand.add(U'a', EpdFontFamily::REGULAR));
  EXPECT_TRUE(zeroDemand.overflowed());
}

TEST(GlyphDemandCollectorTest, MergesFallbackDemandWithoutLosingStyles) {
  std::array<GlyphDemandEntry, 8> primaryStorage{};
  std::array<GlyphDemandEntry, 8> secondaryStorage{};
  GlyphDemandCollector primary(primaryStorage.data(), primaryStorage.size());
  GlyphDemandCollector secondary(secondaryStorage.data(), secondaryStorage.size());
  ASSERT_TRUE(primary.add(U'a', EpdFontFamily::REGULAR));
  ASSERT_TRUE(secondary.add(U'b', EpdFontFamily::BOLD));
  ASSERT_TRUE(secondary.add(U'a', EpdFontFamily::ITALIC));

  ASSERT_TRUE(primary.mergeFrom(secondary));

  ASSERT_EQ(primary.size(), 2U);
  EXPECT_EQ(primary.entries()[0].styleMask, GlyphDemandCollector::styleBit(EpdFontFamily::REGULAR) |
                                                GlyphDemandCollector::styleBit(EpdFontFamily::ITALIC));
  EXPECT_EQ(primary.entries()[1].codepoint, U'b');
  EXPECT_EQ(primary.entries()[1].styleMask, GlyphDemandCollector::styleBit(EpdFontFamily::BOLD));
}
