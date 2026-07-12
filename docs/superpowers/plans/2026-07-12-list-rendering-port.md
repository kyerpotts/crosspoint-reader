# List Rendering Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the list-rendering behavior exercised by CrossPoint's `test_list_rendering.epub` fixture into CrossInk without regressing CrossInk-specific parser behavior.

**Architecture:** Add a fixed-storage `ListState` value type to isolate ordered/unordered nesting and marker lifecycle, then integrate it with CrossInk's existing HTML block-style parser. Use a shared coordinate resolver in `ParsedText` so LTR hanging markers may extend left of continuation text while all unrelated negative positions remain clamped.

**Tech Stack:** C++20 host tests with GoogleTest, ESP32-C3 firmware C++, Expat callbacks, PlatformIO simulator, deterministic Python ZIP generation.

---

## File Structure

- Create `lib/Epub/Epub/parsers/ListState.h`: allocation-free list nesting, numbering, pending-marker, and overflow state.
- Create `test/list_state/ListStateTest.cpp`: observable state-transition tests.
- Create `test/list_state/CMakeLists.txt`: host-test target registration.
- Modify `test/CMakeLists.txt`: include the list-state test suite.
- Modify `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.h`: replace the eager-bullet depth field with `ListState` and marker-indent metadata.
- Modify `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp`: integrate list containers, deferred markers, hanging indents, paragraph handling, and spacing with current CrossInk parser paths.
- Modify `lib/Epub/Epub/ParsedText.cpp`: consistently resolve negative first-line x positions for LTR hanging indents.
- Create `scripts/generate_list_rendering_test_epub.py`: deterministic fixture generator ported from CrossPoint.
- Create `test/epubs/test_list_rendering.epub`: generated acceptance fixture.

### Task 1: Fixed-Storage List State

**Files:**
- Create: `lib/Epub/Epub/parsers/ListState.h`
- Create: `test/list_state/ListStateTest.cpp`
- Create: `test/list_state/CMakeLists.txt`
- Modify: `test/CMakeLists.txt:41-44`

- [ ] **Step 1: Register the new host-test suite**

Add `add_subdirectory(list_state)` to `test/CMakeLists.txt`. Define a `list_state_test` executable linked to `GTest::gtest_main` and `crosspoint_test_common`, then register it with `gtest_discover_tests`.

- [ ] **Step 2: Write failing state-transition tests**

Test these contracts against the wished-for `ListState` API:

```cpp
TEST(ListStateTest, NumbersOrderedItemsAndRestartsNewLists) {
  ListState state;
  state.enterList(true);
  EXPECT_STREQ(state.enterItem(), "1.");
  state.exitItem();
  EXPECT_STREQ(state.enterItem(), "2.");
  state.exitItem();
  state.exitList();
  state.enterList(true);
  EXPECT_STREQ(state.enterItem(), "1.");
}

TEST(ListStateTest, RestoresOuterOrdinalAfterNestedList) {
  ListState state;
  state.enterList(true);
  EXPECT_STREQ(state.enterItem(), "1.");
  state.enterList(false);
  EXPECT_STREQ(state.enterItem(), "\xE2\x80\xA2");
  state.exitItem();
  state.exitList();
  state.exitItem();
  EXPECT_STREQ(state.enterItem(), "2.");
}

TEST(ListStateTest, RecoversAfterNestingBeyondTrackedDepth) {
  ListState state;
  for (int depth = 0; depth < 5; ++depth) state.enterList((depth % 2) == 0);
  EXPECT_STREQ(state.enterItem(), "\xE2\x80\xA2");
  EXPECT_TRUE(state.hasPendingMarker());
  state.consumePendingMarker();
  for (int depth = 0; depth < 5; ++depth) state.exitList();
  state.enterList(true);
  EXPECT_STREQ(state.enterItem(), "1.");
}
```

Also cover unordered markers, empty/malformed exits without underflow, item-depth tracking, and pending-marker consumption.

- [ ] **Step 3: Run tests and verify RED**

Run:

```sh
pio run -t unit-tests
```

Expected: compilation fails because `ListState.h` and its API do not exist.

- [ ] **Step 4: Implement minimal fixed-storage state**

Implement `ListState` as a header-only value type using:

```cpp
static constexpr uint8_t MAX_DEPTH = 4;
static constexpr uint8_t MAX_MARKER_LENGTH = 8;
struct Context { bool ordered; uint16_t nextOrdinal; };
Context contexts_[MAX_DEPTH]{};
uint8_t depth_ = 0;
uint8_t overflowDepth_ = 0;
uint8_t itemDepth_ = 0;
char pendingMarker_[MAX_MARKER_LENGTH]{};
bool hasPendingMarker_ = false;
```

Use `snprintf` for ordinals, saturating guards for counters, no dynamic allocation, and narrow accessors needed by the parser. `enterItem()` must create a marker from the deepest tracked context, advance only an ordered context, and mark it pending. `exitItem()` clears pending state and decrements item depth. `exitList()` consumes overflow depth before popping stored contexts.

- [ ] **Step 5: Run tests and verify GREEN**

Run `pio run -t unit-tests`. Expected: all host suites pass.

### Task 2: Deterministic Acceptance Fixture

**Files:**
- Create: `scripts/generate_list_rendering_test_epub.py`
- Create: `test/epubs/test_list_rendering.epub`

- [ ] **Step 1: Port the source generator**

Copy the deterministic generator from `/home/squidmilk/Work/oss/crosspoint-reader/scripts/generate_list_rendering_test_epub.py` without changing fixture semantics. Keep fixed `ZipInfo` timestamps and the first uncompressed `mimetype` entry.

- [ ] **Step 2: Generate twice and verify reproducibility**

Run the generator, record `sha256sum test/epubs/test_list_rendering.epub`, run it again, and record the checksum again. Expected: identical SHA-256 values.

- [ ] **Step 3: Establish the pre-port simulator baseline**

Run:

```sh
python scripts/run_simulator_smoke_test.py \
  --book test/epubs/test_list_rendering.epub \
  --page-turns 40
```

Expected: the existing parser completes without a crash, while visual behavior remains the known bullet-only/non-hanging baseline.

### Task 3: Parser Integration and Hanging Layout

**Files:**
- Modify: `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.h:24-172`
- Modify: `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp:25-47,805-823,1409-1521,1740-1900,1968-2050`
- Modify: `lib/Epub/Epub/ParsedText.cpp:904-1018`

- [ ] **Step 1: Replace eager-bullet state with `ListState`**

Include `ListState.h`, replace `pendingListMarkerDepth` with a `ListState` member plus parser-depth and marker-indent metadata. Keep all state fixed-size.

- [ ] **Step 2: Track list container and item transitions**

Treat `ul` and `ol` as block tags. On element start, enter list contexts before block-style calculation and enter items before marker styling. On end, clear item state and pop list context with guarded transitions. Preserve `xpathListItemIndex`, skip handling, table handling, and `ancestorStack_` updates.

- [ ] **Step 3: Integrate list block styles**

For list containers with no publisher left margin or padding, add one em of left padding. Cap list-item and list-paragraph vertical margin/padding at one quarter em. The marked block receives a negative indent equal to marker advance plus one space; later paragraphs receive zero implicit indent unless publisher CSS defines one. List-specific indent rules override `forceParagraphIndents` only while inside a list item.

- [ ] **Step 4: Defer marker emission until content**

Do not add a marker at `<li>` start. Ignore leading whitespace while a marker is pending, then add the pending marker immediately before the first non-whitespace token using the current background style. Consume the pending marker exactly once. Closing an empty item clears it.

- [ ] **Step 5: Permit LTR hanging coordinates**

Add one local non-capturing resolver in `ParsedText::extractLine`:

```cpp
const auto resolveLineX = [firstLineIndent, isRtl = blockStyle.isRtl](const int x) -> int16_t {
  if (!isRtl && firstLineIndent < 0) return static_cast<int16_t>(x);
  return static_cast<int16_t>(std::max(0, x));
};
```

Use it for every `lineXPos.push_back` branch. Do not change spacing, BiDi ordering, bionic/guide metadata, or line-break calculations.

- [ ] **Step 6: Build and smoke-test the fixture**

Run:

```sh
pio run -e simulator
python scripts/run_simulator_smoke_test.py \
  --no-build \
  --book test/epubs/test_list_rendering.epub \
  --page-turns 40
```

Expected: build succeeds and smoke output contains `Simulator smoke test passed` with no crash signature.

### Task 4: Requested Tech-Debt Sweep

**Files:**
- Review only files changed in Tasks 1-3.

- [ ] **Step 1: Review state invariants**

Check all list opens/closes, empty items, paragraph wrappers, overflow nesting, malformed extra closes, and marker consumption. Consolidate repeated tag predicates or marker cleanup only where it removes real duplication.

- [ ] **Step 2: Review embedded constraints**

Confirm no bare `new`, heap-backed list container, per-token allocation, avoidable copy, large stack object, raw SDK call, hardcoded screen geometry, or unrelated parser refactor was introduced.

- [ ] **Step 3: Review comments and diff scope**

Keep comments that state marker or overflow invariants; remove narration. Ensure the fixture, list behavior, and hanging-coordinate fix are the only behavioral changes.

- [ ] **Step 4: Re-run focused verification**

Run:

```sh
pio run -t unit-tests
pio run -e simulator
python scripts/run_simulator_smoke_test.py \
  --no-build \
  --book test/epubs/test_list_rendering.epub \
  --page-turns 40
```

Expected: all host tests pass, simulator builds, and fixture smoke passes without crash signatures.

- [ ] **Step 5: Record hardware verification**

Clear the fixture's `.crosspoint/epub_<hash>/` cache, open it on device, and inspect every chapter for marker type, numbering, nested indentation, hanging continuation alignment, multi-paragraph items, vertical spacing, and recovery after depth-five overflow.
