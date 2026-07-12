#include <gtest/gtest.h>

#include "Epub/Epub/parsers/ListState.h"

namespace {
constexpr const char* BULLET = "\xE2\x80\xA2";
}

TEST(ListStateTest, NumbersOrderedItemsAndRestartsNewLists) {
  ListState state;

  state.enterList(true);
  EXPECT_STREQ(state.enterItem(), "1.");
  state.consumePendingMarker();
  state.exitItem();
  EXPECT_STREQ(state.enterItem(), "2.");
  state.exitItem();
  state.exitList();

  state.enterList(true);
  EXPECT_STREQ(state.enterItem(), "1.");
}

TEST(ListStateTest, UsesBulletsForUnorderedItems) {
  ListState state;

  state.enterList(false);
  EXPECT_STREQ(state.enterItem(), BULLET);
  EXPECT_TRUE(state.hasPendingMarker());
  EXPECT_STREQ(state.pendingMarker(), BULLET);
}

TEST(ListStateTest, BuildsFixedWidthMarkerTokenForJustifiedLines) {
  ListState state;

  state.enterList(true);
  state.enterItem();
  EXPECT_STREQ(state.pendingMarker(), "1.");
  EXPECT_STREQ(state.pendingMarkerToken(), "1. ");

  state.consumePendingMarker();
  EXPECT_STREQ(state.pendingMarkerToken(), "");

  state.exitItem();
  state.exitList();
  state.enterList(false);
  state.enterItem();
  EXPECT_STREQ(state.pendingMarkerToken(), "\xE2\x80\xA2 ");
}

TEST(ListStateTest, RestoresOuterOrdinalAfterNestedList) {
  ListState state;

  state.enterList(true);
  EXPECT_STREQ(state.enterItem(), "1.");
  state.consumePendingMarker();

  state.enterList(false);
  EXPECT_STREQ(state.enterItem(), BULLET);
  state.consumePendingMarker();
  state.exitItem();
  state.exitList();

  state.exitItem();
  EXPECT_STREQ(state.enterItem(), "2.");
}

TEST(ListStateTest, RecoversAfterNestingBeyondTrackedDepth) {
  ListState state;

  for (int depth = 0; depth < 5; ++depth) {
    state.enterList((depth % 2) == 0);
  }
  EXPECT_STREQ(state.enterItem(), BULLET);
  state.consumePendingMarker();
  state.exitItem();
  for (int depth = 0; depth < 5; ++depth) {
    state.exitList();
  }

  state.enterList(true);
  EXPECT_STREQ(state.enterItem(), "1.");
}

TEST(ListStateTest, OverflowItemsDoNotAdvanceDeepestTrackedOrdinal) {
  ListState state;

  for (int depth = 0; depth < ListState::MAX_DEPTH; ++depth) {
    state.enterList(true);
  }
  state.enterList(true);
  EXPECT_STREQ(state.enterItem(), BULLET);
  state.exitItem();
  state.exitList();

  EXPECT_STREQ(state.enterItem(), "1.");
}

TEST(ListStateTest, DeepOverflowUnwindsWithoutPoppingTrackedContexts) {
  ListState state;

  for (int depth = 0; depth < ListState::MAX_DEPTH; ++depth) {
    state.enterList(true);
  }
  for (int depth = 0; depth < 300; ++depth) {
    state.enterList(false);
  }
  for (int depth = 0; depth < 300; ++depth) {
    state.exitList();
  }

  EXPECT_STREQ(state.enterItem(), "1.");
}

TEST(ListStateTest, GuardsMalformedExtraExits) {
  ListState state;

  state.exitItem();
  state.exitList();
  EXPECT_FALSE(state.inItem());
  EXPECT_FALSE(state.hasPendingMarker());

  state.enterList(true);
  EXPECT_STREQ(state.enterItem(), "1.");
}

TEST(ListStateTest, TracksNestedItemsAndConsumesMarkerOnce) {
  ListState state;

  state.enterList(false);
  state.enterItem();
  EXPECT_TRUE(state.inItem());
  EXPECT_TRUE(state.hasPendingMarker());

  state.consumePendingMarker();
  EXPECT_FALSE(state.hasPendingMarker());
  EXPECT_STREQ(state.pendingMarker(), "");

  state.enterItem();
  EXPECT_TRUE(state.inItem());
  state.exitItem();
  EXPECT_TRUE(state.inItem());
  state.exitItem();
  EXPECT_FALSE(state.inItem());
}

TEST(ListStateTest, DeepItemNestingUnwindsExactly) {
  ListState state;

  state.enterList(false);
  for (int depth = 0; depth < 300; ++depth) {
    state.enterItem();
    state.consumePendingMarker();
  }
  for (int depth = 0; depth < 299; ++depth) {
    state.exitItem();
  }
  EXPECT_TRUE(state.inItem());
  state.exitItem();
  EXPECT_FALSE(state.inItem());
}
