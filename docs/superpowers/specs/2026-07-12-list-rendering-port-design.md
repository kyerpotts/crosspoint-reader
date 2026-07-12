# List Rendering Port Design

## Goal

Port the list-rendering behavior from `crosspoint-reader` branch `fix/list-rendering-clean` into CrossInk branch `feature/list-alignment`, adapted to CrossInk's newer EPUB parser and constrained to the cases in the source branch's `test_list_rendering.epub` fixture.

## Scope

The port covers:

- unordered-list bullet markers;
- ordered-list numeric markers starting at `1.` for each list;
- nested ordered and unordered lists through four tracked levels;
- safe parser-state recovery when nesting exceeds four levels;
- markers kept with the first non-whitespace item content;
- `<li><p>...</p></li>` and multi-paragraph list items;
- hanging indents so wrapped lines align with item text rather than the marker;
- default indentation for list containers without publisher left margin or padding;
- bounded vertical spacing within list items;
- deterministic generation of the source branch's list-rendering EPUB fixture.

The port does not add semantics absent from the fixture, including `<ol start>`, `<li value>`, alternate marker styles, or right-to-left list-marker placement.

## Existing CrossInk Constraints

CrossInk already emits one eager bullet per `<li>` in `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp`, but it does not distinguish ordered lists, track nesting contexts, or provide a true hanging indent. The parser has also diverged from the source project with low-memory aborts, table buffering, strikethrough and background styles, forced paragraph indents, publisher page markers, and additional ancestor-stack handling. Replacing parser files or cherry-picking the source commit would risk losing these behaviors.

The implementation must preserve the ESP32-C3 memory constraints: no dynamic allocation for list tracking, no new per-token heap churn, and no large stack objects. List state will use fixed-size parser members.

## Architecture

### List context

`ChapterHtmlSlimParser` will own a fixed array of four list contexts. Each context records whether its list is ordered and the next ordinal. Separate counters track active stored contexts, overflow nesting, and active list-item depth. A fixed marker buffer stores the pending marker without allocating.

Opening `<ul>` or `<ol>` pushes a context when capacity remains; deeper containers increment an overflow counter. Closing a list first consumes overflow depth, then pops a stored context. This keeps outer numbering intact after an over-depth nested list.

### Marker lifecycle

Opening `<li>` creates either `•` or the current ordered ordinal and advances that context's ordinal. The marker remains pending until the parser sees the first non-whitespace character. This prevents formatting whitespace and paragraph wrappers from producing a marker-only block.

Only the first content block in an item receives the marker and hanging indent. Later paragraphs remain list-item content but do not receive another marker. Closing `<li>` clears any unconsumed pending marker and item-local indent state.

### Block styling

`ul` and `ol` become block elements so their styles participate in CrossInk's existing `BlockStyle` stack. A one-em left padding is supplied only when publisher CSS specifies neither left margin nor left padding. Publisher-provided horizontal styling remains authoritative.

List-item and nested paragraph top/bottom margins and padding are capped at one quarter em to prevent browser-oriented CSS defaults from creating excessive e-reader whitespace. The first marked block receives a negative text indent equal to marker width plus one space. Continuation paragraphs default to zero text indent unless publisher CSS explicitly defines one. CrossInk's forced paragraph-indent option must not override these list-specific rules.

### Line positioning

`ParsedText::extractLine` currently clamps every negative x coordinate to zero. A hanging marker needs an LTR first-line coordinate left of the continuation-text origin. A local coordinate resolver will permit negative x only for LTR blocks with a negative first-line indent; all other paths retain clamping. The resolver will be used consistently by reordered, RTL, and ordinary LTR line-position branches.

### Fixture

The deterministic Python generator and generated `test/epubs/test_list_rendering.epub` from the source branch will be ported. The fixture defines the accepted behavior: plain and paragraph-wrapped unordered and ordered items, multiple paragraphs, list restart, mixed nesting through depth four, depth-five overflow recovery, long wrapped lines, and mixed ordered/unordered children.

## Error and Resource Handling

The feature adds no heap allocation. The list-context array and marker buffer have compile-time bounds. Over-depth nesting logs once per encountered overflow container and continues with safe fallback marker behavior based on the deepest stored context. Closing malformed or over-depth structures must not underflow counters.

All existing low-memory early returns, table fallbacks, style-stack cleanup, and ancestor-stack behavior remain intact. No SDK or storage API is added.

## Verification

1. Run the fixture generator twice and verify byte-identical output.
2. Before implementing list behavior, run the generated fixture through the current simulator build to establish the existing behavior and exercise the regression case.
3. Build the `simulator` PlatformIO environment after implementation.
4. Run `scripts/run_simulator_smoke_test.py` with `test/epubs/test_list_rendering.epub` and enough page turns to traverse all fixture chapters.
5. Inspect the fixture in the simulator or on hardware for marker type, numbering, nesting, hanging indentation, paragraph behavior, overflow recovery, and spacing.
6. On X4/X3 hardware, clear the fixture's `.crosspoint/epub_<hash>/` cache before testing so stale rendered pages cannot mask parser changes.

The simulator smoke test proves that the fixture parses, renders, and pages without crashes or hangs. Visual alignment remains a manual acceptance check because this repository has no parser-layout assertion harness.

## Tech-Debt Review

After the fixture works, review only the introduced list-rendering code. Consolidate repeated tag and marker-state checks when doing so reduces branching, keep state transitions explicit, remove narration comments, verify every fixed-size bound and counter transition, and confirm no allocation or unrelated refactor entered the diff. Add one human-readable entry under `CHANGELOG.md`'s Unreleased Fixed section.
