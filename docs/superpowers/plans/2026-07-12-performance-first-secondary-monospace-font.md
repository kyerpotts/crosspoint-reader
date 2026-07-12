# Performance-First Secondary Monospace Font Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an independently selectable SD-card monospace font for semantic/CSS code content while keeping prose pages on the existing fast path and limiting mixed-font render overhead to 10% on X3 hardware.

**Architecture:** First replace the fake scan render with direct, bounded glyph-demand collection and prove the single-font path is non-regressed. Then carry one `FontRole` bit through parser, layout, packed text lines, cache serialization, dual SD ownership, exact per-role prewarm, and one-pass rendering.

**Tech Stack:** ESP32-C3 C++20, Expat EPUB parser, packed `TextBlock` arenas, `ScratchWorkspace`, SD `.cpfont` files, CMake/GoogleTest host tests, PlatformIO `tiny`, simulator framebuffer capture, X3 timing and heap logs.

---

## File Structure

### New focused units

- `lib/Epub/Epub/FontRole.h`: exhaustive primary/secondary role and `FontRenderContext` resolver.
- `lib/GfxRenderer/GlyphDemandCollector.h`: bounded codepoint/style demand collector over caller-owned scratch.
- `lib/GfxRenderer/GlyphDemandCollector.cpp`: deduplication, role partitioning, overflow behavior.
- `lib/Epub/Epub/parsers/FontRoleResolver.h`: semantic-tag and CSS generic-family role resolution.
- `test/font_role/FontRoleTest.cpp`: render-context fallback and role-flag contracts.
- `test/glyph_demand_collector/GlyphDemandCollectorTest.cpp`: exact per-role/per-style demand and overflow tests.
- `test/font_role_resolver/FontRoleResolverTest.cpp`: semantic/CSS activation and inheritance tests.
- `scripts/generate_mixed_font_test_epub.py`: deterministic technical-book fixture.
- `test/epubs/test_mixed_font_rendering.epub`: generated fixture.
- `scripts/profile_mixed_font_render.py`: parse serial timing/cache/heap logs and enforce median budgets.

### Existing units modified

- `lib/Epub/Epub/ParsedText.{h,cpp}`: token role vectors and role-aware measurement/layout.
- `lib/Epub/Epub/blocks/TextBlock.{h,cpp}`: packed role bit, direct demand collection, role-aware render.
- `lib/Epub/Epub/Page.{h,cpp}`: direct text-demand traversal and two-font render context.
- `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.{h,cpp}`: fixed-depth inherited font-role state.
- `lib/Epub/Epub/Section.{h,cpp}`: cache version and secondary font ID.
- `lib/GfxRenderer/FontCacheManager.{h,cpp}`: demand-based exact prewarm; remove fake scan ownership.
- `lib/EpdFont/SdCardFont.{h,cpp}`: exact codepoint/style demand prewarm entry point.
- `lib/EpdFont/SdCardFontManager.{h,cpp}`: independent primary and secondary slots.
- `src/SdCardFontSystem.{h,cpp}`: role-specific lifecycle, resolver, and low-memory release.
- `src/CrossPointSettings.{h,cpp}`, `src/JsonSettingsIO.cpp`, and `src/SettingsList.h`: independent persisted secondary family and dynamic setting row.
- `src/activities/settings/FontSelectionActivity.{h,cpp}`: configurable primary/secondary selection target and disabled secondary choice.
- `src/activities/SettingsActivity.cpp` and `src/activities/reader/ReaderOptionsActivity.cpp`: dispatch the existing selector for the secondary row.
- `src/activities/reader/EpubReaderActivity.{h,cpp}` and `EpubReaderMenuActivity.cpp`: direct prewarm/render plus existing per-book layout snapshot integration.
- `lib/I18n/translations/english.yaml` and generated `lib/I18n/*`: translated menu labels.
- `test/CMakeLists.txt`: register focused host suites.

## Task 1: Deterministic Fixture and Baseline

**Files:**
- Create: `scripts/generate_mixed_font_test_epub.py`
- Create: `test/epubs/test_mixed_font_rendering.epub`
- Create: `scripts/profile_mixed_font_render.py`

- [ ] **Step 1: Generate a deterministic technical EPUB fixture**

Create chapters covering ordinary prose, inline `<code>`, `<kbd>`, `<samp>`, `<tt>`, `<pre>`, CSS `font-family: monospace`, nested bold/italic/code, Unicode prose beside ASCII code, long identifiers, punctuation, tables, BiDi prose, and repeated equivalent pages. Use fixed ZIP timestamps and store `mimetype` first and uncompressed.

- [ ] **Step 2: Verify fixture reproducibility**

Run the generator twice and compare:

```sh
python scripts/generate_mixed_font_test_epub.py
sha256sum test/epubs/test_mixed_font_rendering.epub
python scripts/generate_mixed_font_test_epub.py
sha256sum test/epubs/test_mixed_font_rendering.epub
```

Expected: identical SHA-256 values.

- [ ] **Step 3: Add the timing-log analyzer before changing rendering**

Implement a Python parser that accepts repeated log lines containing `prewarm=`, `bw_render=`, SD seek/read counts, free heap, and largest alloc. It computes medians and exits nonzero when prose overhead exceeds 2%, mixed overhead exceeds 10%, any overflow read is recorded, or largest-free-block falls below the supplied baseline floor.

Example contract:

```python
result = compare_runs(control_lines, candidate_lines)
assert result.median_cpu_overhead_percent <= 10.0
assert result.overflow_reads == 0
```

- [ ] **Step 4: Write and run analyzer tests**

Feed fixed control/candidate logs where 9.8% passes, 10.1% fails, prose 2.1% fails, and any overflow read fails. Run:

```sh
python -m unittest scripts/tests/test_profile_mixed_font_render.py -v
```

Expected: all analyzer boundary tests pass.

- [ ] **Step 5: Record X3 baseline**

Build and flash `tiny`, clear the fixture cache, render at least 20 repeated warm page turns, and save primary-only `prewarm`, `bw_render`, SD-read, heap, and largest-block data under `local://mixed-font-baseline.json` for implementation comparison. Do not commit device-specific measurements.

- [ ] **Step 6: Commit the fixture and analyzer**

```sh
git add scripts/generate_mixed_font_test_epub.py scripts/profile_mixed_font_render.py \
  scripts/tests/test_profile_mixed_font_render.py test/epubs/test_mixed_font_rendering.epub
git commit -m "test: add mixed font rendering benchmark"
```

## Task 2: Bounded Glyph-Demand Collector

**Files:**
- Create: `lib/GfxRenderer/GlyphDemandCollector.h`
- Create: `lib/GfxRenderer/GlyphDemandCollector.cpp`
- Create: `test/glyph_demand_collector/GlyphDemandCollectorTest.cpp`
- Create: `test/glyph_demand_collector/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`

- [ ] **Step 1: Write failing collector tests**

Define the wished-for contract:

```cpp
GlyphDemandEntry storage[8]{};
GlyphDemandCollector demand(storage, std::size(storage));
EXPECT_TRUE(demand.add(U'a', EpdFontFamily::REGULAR));
EXPECT_TRUE(demand.add(U'a', EpdFontFamily::BOLD));
ASSERT_EQ(demand.size(), 1U);
EXPECT_EQ(demand.entries()[0].codepoint, U'a');
EXPECT_EQ(demand.entries()[0].styleMask,
          styleBit(EpdFontFamily::REGULAR) | styleBit(EpdFontFamily::BOLD));
```

Also test UTF-8 traversal, replacement/space/hyphen demand, deterministic insertion order, exact-capacity success, first-overflow failure, sticky overflow, and reset/reuse without allocation.

- [ ] **Step 2: Run tests and verify RED**

```sh
cmake -S test -B build/test -DCMAKE_BUILD_TYPE=Release
cmake --build build/test --target GlyphDemandCollectorTest
```

Expected: compilation fails because the collector does not exist.

- [ ] **Step 3: Implement the bounded collector**

Use caller-owned contiguous storage:

```cpp
struct GlyphDemandEntry {
  uint32_t codepoint;
  uint8_t styleMask;
};

class GlyphDemandCollector {
 public:
  GlyphDemandCollector(GlyphDemandEntry* entries, uint16_t capacity);
  bool add(uint32_t codepoint, EpdFontFamily::Style style);
  bool addUtf8(const char* text, EpdFontFamily::Style style);
  void reset();
  bool overflowed() const;
  uint16_t size() const;
  const GlyphDemandEntry* entries() const;
};
```

Use bounded linear deduplication; typical pages contain far fewer than 512 unique codepoints, and the fixed upper bound avoids heap/hash-table overhead. Merge style bits for duplicate codepoints. Never silently discard overflow.

- [ ] **Step 4: Run tests and verify GREEN**

```sh
cmake --build build/test --target GlyphDemandCollectorTest
ctest --test-dir build/test -R GlyphDemandCollector --output-on-failure
```

Expected: all collector tests pass.

- [ ] **Step 5: Commit the collector**

```sh
git add lib/GfxRenderer/GlyphDemandCollector.* test/glyph_demand_collector test/CMakeLists.txt
git commit -m "feat: add bounded glyph demand collector"
```

## Task 3: Replace the Fake Scan Render for One Font

**Files:**
- Modify: `lib/Epub/Epub/blocks/TextBlock.{h,cpp}`
- Modify: `lib/Epub/Epub/Page.{h,cpp}`
- Modify: `lib/GfxRenderer/FontCacheManager.{h,cpp}`
- Modify: `lib/EpdFont/SdCardFont.{h,cpp}`
- Modify: `src/activities/reader/EpubReaderActivity.cpp:4238-4252`
- Test: `test/glyph_demand_collector/GlyphDemandCollectorTest.cpp`

- [ ] **Step 1: Write a failing direct-page-collection test**

Construct a `TextBlock` containing repeated regular and bold words, place it in a `Page`, collect demand, and assert one codepoint entry with merged style bits. Assert an image-only page produces empty demand.

- [ ] **Step 2: Verify RED**

Build the focused target. Expected: failure because `TextBlock::collectGlyphDemand` and `Page::collectGlyphDemand` do not exist.

- [ ] **Step 3: Add direct traversal**

Add non-rendering traversal methods:

```cpp
bool TextBlock::collectGlyphDemand(GlyphDemandCollector& demand) const;
bool PageLine::collectGlyphDemand(GlyphDemandCollector& demand) const;
bool PageTableFragment::collectGlyphDemand(GlyphDemandCollector& demand) const;
bool Page::collectGlyphDemand(GlyphDemandCollector& demand) const;
```

Images and horizontal rules contribute no glyphs. Table fragments traverse every serialized cell line. Include inserted hyphens, guide dots, and bionic primary suffix styles exactly as drawing requires.

- [ ] **Step 4: Add exact-demand prewarm APIs**

Add `SdCardFont::prewarmDemand(const GlyphDemandEntry*, uint16_t)` and a corresponding built-in decompressor path. Load only styles requested for each codepoint; do not expand one page-wide style mask across all codepoints.

- [ ] **Step 5: Replace `PrewarmScope` in the reader**

Acquire enough `ScratchWorkspace` for `SdCardFont::MAX_PAGE_GLYPHS` demand entries, collect directly, prewarm once, release scratch, then call `page->renderText` once. Retain the current scan path only behind a temporary debug comparison switch during this task; remove it before commit.

- [ ] **Step 6: Verify behavior and performance**

Run focused collector tests, build `tiny`, run simulator framebuffer comparison against the fixture, and rerun the prose X3 baseline. Required: identical framebuffer hashes for primary-only pages, no render slowdown above 2%, and no new heap fragmentation.

- [ ] **Step 7: Commit the single-font refactor**

```sh
git add lib/Epub/Epub/blocks/TextBlock.* lib/Epub/Epub/Page.* \
  lib/GfxRenderer/FontCacheManager.* lib/EpdFont/SdCardFont.* \
  src/activities/reader/EpubReaderActivity.cpp test/glyph_demand_collector
git commit -m "perf: collect page glyph demand without scan render"
```

## Task 4: Font Role and Packed Line Contract

**Files:**
- Create: `lib/Epub/Epub/FontRole.h`
- Create: `test/font_role/FontRoleTest.cpp`
- Create: `test/font_role/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`
- Modify: `lib/Epub/Epub/ParsedText.{h,cpp}`
- Modify: `lib/Epub/Epub/blocks/TextBlock.{h,cpp}`

- [ ] **Step 1: Write failing role and packing tests**

Test:

```cpp
FontRenderContext fonts{.primaryId = 10, .secondaryId = 20};
EXPECT_EQ(fonts.resolve(FontRole::Primary), 10);
EXPECT_EQ(fonts.resolve(FontRole::Secondary), 20);
fonts.secondaryId = 0;
EXPECT_EQ(fonts.resolve(FontRole::Secondary), 10);
```

Construct primary-only and mixed `TextBlock`s. Assert primary-only blocks omit `wordFlags`, mixed blocks set `WORD_FLAG_SECONDARY_FONT` only on secondary tokens, and serialization round-trips roles.

- [ ] **Step 2: Verify RED**

Build `FontRoleTest`. Expected: missing role/context and flag APIs.

- [ ] **Step 3: Implement the role contract**

```cpp
enum class FontRole : uint8_t { Primary = 0, Secondary = 1 };
struct FontRenderContext {
  int primaryId;
  int secondaryId;
  int resolve(FontRole role) const {
    return role == FontRole::Secondary && secondaryId != 0 ? secondaryId : primaryId;
  }
};
```

Add `WORD_FLAG_SECONDARY_FONT = 0x04` and a `wordFontRole(i)` accessor. Preserve the optional flags arena: create it only when any existing flag or secondary token occurs.

- [ ] **Step 4: Carry roles through `ParsedText`**

Extend `addWord` with a required/defaulted `FontRole` at the end, then add role vectors beside every word/style vector and every reorder/line scratch vector. Every insertion, erase, split, BiDi reorder, hyphen insertion, and line extraction must update role in lockstep.

- [ ] **Step 5: Run tests and verify GREEN**

Run `FontRoleTest`, existing differential rounding, hyphenation evaluation, and list-state tests. Expected: all focused suites pass.

- [ ] **Step 6: Commit role metadata**

```sh
git add lib/Epub/Epub/FontRole.h lib/Epub/Epub/ParsedText.* \
  lib/Epub/Epub/blocks/TextBlock.* test/font_role test/CMakeLists.txt
git commit -m "feat: preserve font role in epub text lines"
```

## Task 5: Semantic and CSS Role Resolution

**Files:**
- Create: `lib/Epub/Epub/parsers/FontRoleResolver.h`
- Create: `test/font_role_resolver/FontRoleResolverTest.cpp`
- Create: `test/font_role_resolver/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`
- Modify: `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.{h,cpp}`
- Modify: CSS declaration structures that currently expose `font-family`

- [ ] **Step 1: Write failing resolver tests**

Cover every semantic tag, mixed case normalization already performed by Expat, CSS generic monospace with publisher styles enabled, CSS ignored when disabled, nested inheritance, nested explicit proportional generic restoration, and unrelated tags preserving inherited role.

- [ ] **Step 2: Verify RED**

Build `FontRoleResolverTest`. Expected: resolver missing.

- [ ] **Step 3: Implement pure resolver**

Expose:

```cpp
FontRole resolveElementFontRole(const char* tag, FontRole inherited,
                                CssGenericFontFamily genericFamily,
                                bool publisherStylesEnabled);
```

Use exact tag comparisons for `code`, `kbd`, `samp`, `tt`, and `pre`. Generic monospace selects secondary only when publisher styles are honored.

- [ ] **Step 4: Integrate fixed-depth parser state**

Store role with the parser's existing inherited inline state rather than a new heap stack. Pass the resolved role into every `ParsedText::addWord`, including list markers, whitespace/background tokens, inserted punctuation, and table-cell parsing. Synthetic reader-generated markers remain primary unless they are inside semantic code content.

- [ ] **Step 5: Verify fixture parsing**

Run resolver tests, build `tiny`, clear the fixture section cache, and inspect parser logs/serialized lines to confirm expected role counts.

- [ ] **Step 6: Commit parser activation**

```sh
git add lib/Epub/Epub/parsers/FontRoleResolver.h \
  lib/Epub/Epub/parsers/ChapterHtmlSlimParser.* test/font_role_resolver test/CMakeLists.txt
git commit -m "feat: detect semantic monospace epub content"
```

## Task 6: Mixed-Font Measurement and Rendering

**Files:**
- Modify: `lib/Epub/Epub/ParsedText.{h,cpp}`
- Modify: `lib/Epub/Epub/blocks/TextBlock.{h,cpp}`
- Modify: `lib/Epub/Epub/Page.{h,cpp}`
- Test: `test/font_role/FontRoleTest.cpp`

- [ ] **Step 1: Write failing mixed-metric tests**

Use a fake metric provider where primary `a` is 5px, secondary `a` is 9px, primary ascender is 10px, and secondary ascender is 14px. Assert mixed token x positions, line break boundary, max line metrics, underline width, and role-aware demand separation.

- [ ] **Step 2: Verify RED**

Expected: layout uses one font ID and produces primary-only widths.

- [ ] **Step 3: Make every measurement role-aware**

Replace scalar `fontId` parameters inside mixed text layout with `const FontRenderContext&`. Resolve the role for word width, bionic prefix/suffix, soft hyphen, kerning/natural gap across role boundaries, guide dots, and inserted hyphens. For adjacent tokens with different roles, use zero cross-family kerning and each family's own advances.

- [ ] **Step 4: Make line height role-aware**

Compute the maximum ascender and descender among fonts represented on the line. Store or apply those metrics at page-line placement without adding per-token vertical arrays.

- [ ] **Step 5: Render once with per-token IDs**

`TextBlock::render` resolves one font ID per token and reuses it for text, background width, underline, strikethrough, superscript/subscript, and guide dots. Secondary tokens bypass bionic split rendering and render their original token exactly.

- [ ] **Step 6: Verify GREEN and visual output**

Run role/metric tests, build `tiny`, and capture every mixed-font fixture page in the simulator. Confirm inline baseline, code-block wrapping, style preservation, and no clipping.

- [ ] **Step 7: Commit mixed rendering**

```sh
git add lib/Epub/Epub/ParsedText.* lib/Epub/Epub/blocks/TextBlock.* \
  lib/Epub/Epub/Page.* test/font_role
git commit -m "feat: lay out mixed epub fonts in one pass"
```

## Task 7: Section Cache Identity

**Files:**
- Modify: `lib/Epub/Epub/Section.{h,cpp}`
- Modify: all section create/load call sites in `src/activities/reader/EpubReaderActivity.cpp`
- Test: add a focused section-header test target

- [ ] **Step 1: Write failing cache identity tests**

Create a header with primary ID 10 and secondary ID 20. Assert load accepts `(10,20)`, rejects `(11,20)`, rejects `(10,21)`, and rejects the previous section version.

- [ ] **Step 2: Verify RED**

Expected: section header contains only primary ID.

- [ ] **Step 3: Bump and extend the cache header**

Increment `SECTION_FILE_VERSION`, add `secondaryFontId` beside `fontId`, update `HEADER_SIZE`, write/read validation, create/load signatures, and every caller. Use `0` for disabled/fallback secondary.

- [ ] **Step 4: Verify GREEN**

Run the section-header target and clear/rebuild the fixture cache. Expected: old cache rejected once, then subsequent opens load the new cache.

- [ ] **Step 5: Commit cache identity**

```sh
git add lib/Epub/Epub/Section.* src/activities/reader/EpubReaderActivity.cpp test/section_font_identity
git commit -m "feat: include secondary font in epub cache identity"
```

## Task 8: Independent Dual SD Font Slots

**Files:**
- Modify: `lib/EpdFont/SdCardFontManager.{h,cpp}`
- Modify: `src/SdCardFontSystem.{h,cpp}`
- Create: `test/sd_card_font_slots/SdCardFontSlotsTest.cpp`
- Create: `test/sd_card_font_slots/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`

- [ ] **Step 1: Write failing two-slot lifecycle tests**

Use fake load/unload hooks to verify loading secondary does not unload primary, changing primary preserves secondary, unloading one removes only its renderer ID, low-memory/network release visits both in deterministic order, and failed secondary load leaves primary intact.

- [ ] **Step 2: Verify RED**

Expected: current manager unloads its one family before every load.

- [ ] **Step 3: Implement explicit slots**

```cpp
enum class SdFontSlot : uint8_t { Primary = 0, Secondary = 1 };
struct LoadedFontSlot {
  SdCardFont* font = nullptr;
  int fontId = 0;
  uint8_t pointSize = 0;
  char familyName[64] = {};
};
```

Use `new (std::nothrow)` once per selected slot, null-check and log. Slot operations load, register, resolve, release, and unload without touching the other slot. Close every local `HalFile` before moving to the next slot.

- [ ] **Step 4: Add role-specific `SdCardFontSystem` resolution**

Expose primary and secondary resolution through one context callback or two explicit methods. `ensureLoaded` reconciles both settings before section create/render. `releaseForNetwork` and low-memory release handle both slots and clear the registry once.

- [ ] **Step 5: Verify GREEN and memory**

Run slot tests and build `tiny`. On X3, load two SD families and record free/max-alloc heap before load, after load, after prewarm, and after unload. Confirm both IDs are registered and no file concurrency error occurs.

- [ ] **Step 6: Commit dual-slot ownership**

```sh
git add lib/EpdFont/SdCardFontManager.* src/SdCardFontSystem.* \
  test/sd_card_font_slots test/CMakeLists.txt
git commit -m "feat: manage independent reader sd font slots"
```

## Task 9: Settings and Existing-Menu UI

**Files:**
- Modify: `src/CrossPointSettings.{h,cpp}`
- Modify: `src/JsonSettingsIO.cpp`
- Modify: `src/SettingsList.h`
- Modify: `src/activities/settings/FontSelectionActivity.{h,cpp}`
- Modify: `src/activities/SettingsActivity.cpp`
- Modify: `src/activities/reader/ReaderOptionsActivity.cpp`
- Modify: `src/activities/reader/EpubReaderActivity.{h,cpp}`
- Modify: `src/activities/reader/EpubReaderMenuActivity.cpp`
- Modify: `lib/I18n/translations/english.yaml`
- Regenerate: `lib/I18n/*`
- Test: add focused settings migration/normalization tests

- [ ] **Step 1: Write failing settings tests**

Test default disabled value, independent primary/secondary persistence, bounded family-name copy, missing-family clearing, and settings-version migration from the current schema.

- [ ] **Step 2: Verify RED**

Expected: no secondary family field or setting row.

- [ ] **Step 3: Add persisted secondary selection**

Add a fixed-size `secondarySdFontFamilyName` field and bump the settings version through the existing migration mechanism. Empty string means disabled. Persist it through binary settings, JSON import/export, and the reader's existing per-book layout snapshot so custom book settings restore both font choices together.

- [ ] **Step 4: Add the formatting-menu row**

Add “Monospace font” through `SettingsList.h` to the existing reader formatting category. Extend `FontSelectionActivity` with an explicit primary/secondary target; secondary selection lists “Disabled” first and SD families after it, while primary behavior remains unchanged. Dispatch that target from `SettingsActivity` and `ReaderOptionsActivity`. On change, invalidate section identity and request the normal reader rebuild; do not add a new activity.

- [ ] **Step 5: Add and generate translations**

Add English YAML keys for the row and disabled choice, then run:

```sh
python scripts/gen_i18n.py
```

Do not edit generated i18n headers directly.

- [ ] **Step 6: Verify settings behavior**

Run focused settings tests, build `tiny`, open the formatting menu on X3, select two distinct SD families, reboot, and confirm both selections persist.

- [ ] **Step 7: Commit settings and UI**

```sh
git add src/CrossPointSettings.* src/JsonSettingsIO.cpp src/SettingsList.h \
  src/activities/settings/FontSelectionActivity.* src/activities/SettingsActivity.cpp \
  src/activities/reader/ReaderOptionsActivity.cpp src/activities/reader/EpubReaderActivity.* \
  src/activities/reader/EpubReaderMenuActivity.cpp lib/I18n/translations/english.yaml lib/I18n
git commit -m "feat: select a secondary reader monospace font"
```

## Task 10: Role-Separated Exact Prewarm

**Files:**
- Modify: `lib/GfxRenderer/FontCacheManager.{h,cpp}`
- Modify: `lib/EpdFont/SdCardFont.{h,cpp}`
- Modify: `lib/Epub/Epub/blocks/TextBlock.cpp`
- Modify: `lib/Epub/Epub/Page.cpp`
- Modify: `src/activities/reader/EpubReaderActivity.cpp`
- Test: extend collector and dual-slot suites

- [ ] **Step 1: Write failing two-font demand tests**

Build a mixed page and assert primary and secondary collectors contain only their tokens/styles, prose pages report no secondary demand, each participating font receives one prewarm call, and secondary prewarm failure resolves the page render context back to primary before drawing.

- [ ] **Step 2: Verify RED**

Expected: direct collector has one font destination.

- [ ] **Step 3: Partition one scratch lease by role**

Acquire bounded scratch for two `MAX_PAGE_GLYPHS` demand partitions. `TextBlock::collectGlyphDemand` resolves each token role into the correct collector. Do not allocate collector storage on the heap or stack.

- [ ] **Step 4: Prewarm participating fonts sequentially**

Prewarm primary first, close its SD file, then prewarm secondary. Retain both prepared caches. Skip secondary entirely when its collector is empty. On demand overflow/OOM/insufficient largest block, set `secondaryId = 0`, clear secondary prepared cache, and render once with primary fallback.

- [ ] **Step 5: Remove old scan-render machinery**

Delete `ScanMode`, `recordText`, `scanText_`, `scanFontId_`, `PrewarmScope`, and reader call sites after every direct-collector path passes. Leave no compatibility alias or deprecated path.

- [ ] **Step 6: Verify focused correctness and heap behavior**

Run all new host suites, `pio run -e tiny`, simulator framebuffer capture, and X3 cache/heap logs. Confirm prose pages make zero secondary calls and mixed pages make exactly one prewarm per role.

- [ ] **Step 7: Commit integrated prewarm**

```sh
git add lib/GfxRenderer/FontCacheManager.* lib/EpdFont/SdCardFont.* \
  lib/Epub/Epub/blocks/TextBlock.cpp lib/Epub/Epub/Page.cpp \
  src/activities/reader/EpubReaderActivity.cpp test
git commit -m "perf: prewarm mixed reader fonts by exact demand"
```

## Task 11: Performance and Hardware Acceptance Gate

**Files:**
- No production changes unless a measured failure first receives a regression test.

- [ ] **Step 1: Run host and firmware verification**

```sh
cmake -S test -B build/test -DCMAKE_BUILD_TYPE=Release
cmake --build build/test
ctest --test-dir build/test --output-on-failure -j
pio run -e tiny
```

Expected: every host target passes; hardware build succeeds within flash/RAM limits.

- [ ] **Step 2: Verify deterministic simulator output**

Run the mixed fixture through the simulator, capture all pages, and compare primary-only pages against pre-feature hashes. Visually inspect semantic tags, CSS monospace, inline baseline, wrapping, mixed styles, tables, Unicode, and clipping.

- [ ] **Step 3: Measure X3 prose fast path**

Collect at least 20 warm turns with secondary disabled and configured-but-unused. Run the analyzer. Required: at most 2% median difference and zero secondary SD operations.

- [ ] **Step 4: Measure built-in plus SD mixed pages**

Collect at least 20 warm turns. Required: at most 10% median `prewarm + bw_render` overhead, no overflow reads, stable largest free block.

- [ ] **Step 5: Measure dual-SD mixed pages**

Collect at least 20 warm turns with distinct primary and secondary SD families. Required: at most 10% overhead, one prewarm per role, no low-memory fallback, no per-glyph SD reads, and stable largest free block.

- [ ] **Step 6: Run cache stress**

Turn forward/back repeatedly through mixed styles, long identifiers, Unicode, tables, and images. Required: no latency growth, panic, stale cache, handle concurrency error, or fragmentation trend.

- [ ] **Step 7: Measure firmware size**

Run the repository firmware-size history tool and record flash/RAM deltas against `feature/list-alignment`. The `tiny` image must remain under its configured flash limit.

- [ ] **Step 8: Enforce the gate**

If any performance scenario fails, do not call the feature complete and do not merge. Add a failing benchmark/regression test for the measured cause, optimize the source, and rerun all scenarios. Do not enable per-glyph fallback to force a pass.
