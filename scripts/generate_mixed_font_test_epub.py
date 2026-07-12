#!/usr/bin/env python3

from pathlib import Path
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile, ZipInfo


OUTPUT = Path(__file__).resolve().parents[1] / "test" / "epubs" / "test_mixed_font_rendering.epub"
LONG_OUTPUT = Path(__file__).resolve().parents[1] / "test" / "epubs" / "test_mixed_font_rendering_long.epub"
TIMESTAMP = (2026, 7, 12, 0, 0, 0)


def entry(name: str, content: str, compression: int = ZIP_DEFLATED) -> tuple[ZipInfo, bytes]:
    info = ZipInfo(name, TIMESTAMP)
    info.compress_type = compression
    info.create_system = 0
    return info, content.encode("utf-8")


STYLE = """
body { font-size: 1em; line-height: 1.25; }
h1 { font-size: 1.35em; }
.note { font-size: 0.85em; }
.mono { font-family: monospace; }
pre { white-space: pre-wrap; margin: 0.5em 0; }
table { width: 100%; border-collapse: collapse; }
td, th { border: 1px solid #000; padding: 0.2em; }
"""

PROSE = """
<h1>Prose control</h1>
<p>This chapter contains ordinary prose only. It is repeated so warm forward and backward page turns measure the zero-overhead secondary-font path without code tokens.</p>
<p>Reliable performance measurements need enough words to wrap across many lines. The quick brown fox jumps over the lazy dog while readers compare cache behavior, line spacing, punctuation, and paragraph flow.</p>
<p>This chapter contains ordinary prose only. It is repeated so warm forward and backward page turns measure the zero-overhead secondary-font path without code tokens.</p>
<p>Reliable performance measurements need enough words to wrap across many lines. The quick brown fox jumps over the lazy dog while readers compare cache behavior, line spacing, punctuation, and paragraph flow.</p>
"""

INLINE = """
<h1>Semantic inline code</h1>
<p>Call <code>renderPage(cache, glyphs)</code>, press <kbd>Ctrl+C</kbd>, inspect <samp>cache hit: 97%</samp>, and compare the legacy <tt>fixed_width_name</tt> element.</p>
<p>Styles remain independent: <code><b>const</b> value = <i>compute()</i>;</code> and <u><code>underlined_identifier</code></u> must retain one baseline and wrap naturally with surrounding prose.</p>
<p>Long identifiers test line breaking: <code>secondaryMonospaceGlyphDemandCollectorWithBoundedStorage</code> continues after prose without becoming a separate block.</p>
"""

PREFORMATTED = """
<h1>Preformatted blocks</h1>
<pre><code>template &lt;typename T&gt;
T clamp_value(T value, T low, T high) {
    return value &lt; low ? low : (value &gt; high ? high : value);
}</code></pre>
<pre>for (uint32_t i = 0; i &lt; glyphCount; ++i) {
    cache.prewarm(codepoints[i]);
}</pre>
<p class="note">Both blocks must use the selected secondary family without clipping braces, underscores, punctuation, or indentation.</p>
"""

CSS_MONO = """
<h1>CSS generic monospace</h1>
<p>The next spans use publisher CSS rather than semantic tags.</p>
<p><span class="mono">font-family: monospace; --flag=value; /books/source.cpp</span> returns to proportional prose after the closing span.</p>
<div class="mono"><b>bold_css_code()</b> and <i>italic_css_code()</i> share the selected secondary family.</div>
"""

UNICODE_BIDI = """
<h1>Unicode and direction stress</h1>
<p>Prose retains Unicode: café, naïve, Ελληνικά, Русский, 日本語, and العربية.</p>
<p>Mixed content: العربية <code>printf("value=%d", value);</code> نهاية and 日本語 <code>std::vector&lt;int&gt;</code> 続き.</p>
<p><code>ASCII_Code_1234 += 0x2A;</code> must not receive bionic splitting or altered identifiers.</p>
"""

TABLE = """
<h1>Tables and cache stress</h1>
<table>
<tr><th>Operation</th><th>Example</th></tr>
<tr><td>Allocate</td><td><code>makeUniqueNoThrow&lt;uint8_t[]&gt;(size)</code></td></tr>
<tr><td>Measure</td><td><code>renderer.getTextAdvanceX(fontId, text)</code></td></tr>
<tr><td>Log</td><td><code>LOG_DBG("FONT", "glyphs=%u", count)</code></td></tr>
</table>
<p><code><b>bold</b> <i>italic</i> <b><i>boldItalic</i></b> regular</code> forces exact per-style demand without prewarming every codepoint in every style.</p>
<p>Repeat: <code>cache.clear(); cache.prewarm(); cache.render();</code></p>
<p>Repeat: <code>cache.clear(); cache.prewarm(); cache.render();</code></p>
"""

CHAPTERS = [PROSE, INLINE, PREFORMATTED, CSS_MONO, UNICODE_BIDI, TABLE]
TITLES = ["Prose control", "Semantic inline", "Preformatted", "CSS monospace", "Unicode and BiDi", "Tables and stress"]


def repeated_sections(title: str, content: str, count: int) -> str:
    return "".join(f"<h2>{title} {index}</h2>{content}" for index in range(1, count + 1))


LONG_PROSE = repeated_sections(
    "Prose measurement section",
    """
<p>This is proportional prose with no code roles. Forward and backward page turns through this chapter isolate the secondary font's zero-demand path. The wording stays stable so repeated measurements exercise equivalent wrapping and cache behavior.</p>
<p>Reliable measurements need ordinary punctuation, several sentence lengths, and enough text to fill the screen. The quick brown fox jumps over the lazy dog while the reader advances through a deterministic stream of paragraphs.</p>
""",
    20,
)

LONG_INLINE = repeated_sections(
    "Inline mixed-font section",
    """
<p>Call <code>renderPage(cache, glyphs)</code>, press <kbd>Ctrl+C</kbd>, inspect <samp>cache hit: 97%</samp>, and compare <tt>fixed_width_name</tt> before returning to proportional prose.</p>
<p>Long tokens such as <code>secondaryMonospaceGlyphDemandCollectorWithBoundedStorage</code> test wrapping. Styled tokens <code><b>const</b> value = <i>compute()</i>;</code> retain their weight, slant, baseline, and font role.</p>
<p><span class="mono">font-family: monospace; --flag=value; /books/source.cpp</span> exercises the CSS generic family alongside semantic elements.</p>
""",
    20,
)

LONG_BLOCKS = repeated_sections(
    "Code block section",
    """
<pre><code>template &lt;typename T&gt;
T clamp_value(T value, T low, T high) {
    return value &lt; low ? low : (value &gt; high ? high : value);
}

for (uint32_t i = 0; i &lt; glyphCount; ++i) {
    cache.prewarm(codepoints[i]);
}</code></pre>
<p>Proportional text after each block verifies that the font role ends at the closing element.</p>
""",
    20,
)

LONG_CHAPTERS = [LONG_PROSE, LONG_INLINE, LONG_BLOCKS]
LONG_TITLES = ["Long prose control", "Long inline mixed fonts", "Long preformatted blocks"]


def xhtml(title: str, body: str) -> str:
    return f'''<?xml version="1.0" encoding="utf-8"?>
<html xmlns="http://www.w3.org/1999/xhtml">
<head><title>{title}</title><style>{STYLE}</style></head>
<body>{body}</body>
</html>
'''


def build_epub(
    output: Path,
    identifier: str,
    title: str,
    titles: list[str],
    chapters: list[str],
) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    manifest = []
    spine = []
    nav = []
    files: list[tuple[ZipInfo, bytes]] = []
    for index, (chapter_title, body) in enumerate(zip(titles, chapters), start=1):
        name = f"chapter{index}.xhtml"
        item_id = f"ch{index}"
        manifest.append(f'<item id="{item_id}" href="{name}" media-type="application/xhtml+xml"/>')
        spine.append(f'<itemref idref="{item_id}"/>')
        nav.append(f'<li><a href="{name}">{chapter_title}</a></li>')
        files.append(entry(f"EPUB/{name}", xhtml(chapter_title, body)))

    container = '''<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
<rootfiles><rootfile full-path="EPUB/package.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>
'''
    package = f'''<?xml version="1.0" encoding="utf-8"?>
<package version="3.0" unique-identifier="book-id" xmlns="http://www.idpf.org/2007/opf">
<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
<dc:identifier id="book-id">{identifier}</dc:identifier>
<dc:title>{title}</dc:title>
<dc:language>en</dc:language>
</metadata>
<manifest>{''.join(manifest)}<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/></manifest>
<spine>{''.join(spine)}</spine>
</package>
'''
    navigation = f'''<?xml version="1.0" encoding="utf-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
<head><title>Contents</title></head><body><nav epub:type="toc"><ol>{''.join(nav)}</ol></nav></body>
</html>
'''

    with ZipFile(output, "w") as archive:
        for info, data in [
            entry("mimetype", "application/epub+zip", ZIP_STORED),
            entry("META-INF/container.xml", container),
            entry("EPUB/package.opf", package),
            entry("EPUB/nav.xhtml", navigation),
            *files,
        ]:
            archive.writestr(info, data)
    print(f"Generated {output}")


def build() -> None:
    build_epub(
        OUTPUT,
        "crossink-mixed-font-rendering",
        "Mixed Font Rendering Tests",
        TITLES,
        CHAPTERS,
    )
    build_epub(
        LONG_OUTPUT,
        "crossink-mixed-font-rendering-long",
        "Long Mixed Font Rendering Tests",
        LONG_TITLES,
        LONG_CHAPTERS,
    )


if __name__ == "__main__":
    build()
