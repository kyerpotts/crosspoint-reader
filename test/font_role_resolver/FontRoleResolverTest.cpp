#include <gtest/gtest.h>

#include "Epub/Epub/parsers/FontRoleResolver.h"

TEST(FontRoleResolverTest, SemanticCodeElementsSelectSecondary) {
  for (const char* tag : {"code", "kbd", "samp", "tt", "pre"}) {
    EXPECT_EQ(resolveElementFontRole(tag, FontRole::Primary, CssGenericFontFamily::Unspecified, true),
              FontRole::Secondary)
        << tag;
  }
}

TEST(FontRoleResolverTest, UnrelatedElementsInheritRole) {
  EXPECT_EQ(resolveElementFontRole("span", FontRole::Secondary, CssGenericFontFamily::Unspecified, true),
            FontRole::Secondary);
  EXPECT_EQ(resolveElementFontRole("span", FontRole::Primary, CssGenericFontFamily::Unspecified, true),
            FontRole::Primary);
}

TEST(FontRoleResolverTest, CssGenericFamilyOverridesOnlyWhenPublisherStylesEnabled) {
  EXPECT_EQ(resolveElementFontRole("span", FontRole::Primary, CssGenericFontFamily::Monospace, true),
            FontRole::Secondary);
  EXPECT_EQ(resolveElementFontRole("span", FontRole::Secondary, CssGenericFontFamily::Proportional, true),
            FontRole::Primary);
  EXPECT_EQ(resolveElementFontRole("span", FontRole::Primary, CssGenericFontFamily::Monospace, false),
            FontRole::Primary);
}

TEST(FontRoleResolverTest, ParsesGenericFontFamilyWithoutAllocatingNames) {
  EXPECT_EQ(parseCssGenericFontFamily("monospace"), CssGenericFontFamily::Monospace);
  EXPECT_EQ(parseCssGenericFontFamily("'Courier New', monospace"), CssGenericFontFamily::Monospace);
  EXPECT_EQ(parseCssGenericFontFamily("Arial, sans-serif"), CssGenericFontFamily::Proportional);
  EXPECT_EQ(parseCssGenericFontFamily("serif"), CssGenericFontFamily::Proportional);
  EXPECT_EQ(parseCssGenericFontFamily("'Custom Family'"), CssGenericFontFamily::Unspecified);
}
