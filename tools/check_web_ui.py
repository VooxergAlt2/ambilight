#!/usr/bin/env python3
"""Structural checks for the embedded Ambilight Web UI.

This intentionally avoids browser/runtime dependencies. It catches the class
of mistakes that are easy to introduce while editing the single-file page:
duplicate static IDs, broken page/navigation contracts, stale safety guards
and accidental removal of required UI actions.
"""

from __future__ import annotations

from collections import Counter
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src" / "network" / "WebUiService.cpp"

START = 'R"HTML('
END = ')HTML";'

REQUIRED_IDS = {
    "action",
    "brightness",
    "curve",
    "factory",
    "pageHome",
    "pageLed",
    "pageTof",
    "pageDiag",
    "pageSystem",
    "navHome",
    "navLed",
    "navTof",
    "navDiag",
    "navSystem",
    "tofGrid",
    "mapping",
    "pixelMask",
}

REQUIRED_TOKENS = {
    "function showPage(",
    "function applyCurve(",
    "function forgetWifi(",
    "function renderMap(",
    "function renderTofGrid(",
    "pendingActionId",
    "sourceLabel(",
    "gainPercent(",
}

FORBIDDEN_TOKENS = {
    "$('mapApply').disabled=s.output.brightness!==0",
    "s.output.brightness<=64",
    "Distance → gain Q12",
    "Clear NVS Wi-Fi",
}



# Void HTML elements never carry a matching close tag.
VOID_TAGS = {"input", "meta", "br", "img", "hr", "link"}


def check_tag_balance(html: str) -> None:
    """Lightweight open/close tag balance check.

    This is deliberately not an HTML parser: it only counts `<name` versus
    `</name` occurrences per tag. That is enough to catch a copy/paste or
    edit mistake (an unclosed <div>, a stray </section>) without pulling in
    an HTML parsing dependency. The embedded <script> body is excluded
    first, because JS comparison operators such as `i<ids.length` or
    `a<b` would otherwise be misread as tag opens.
    """

    script_start = html.find("<script>")
    script_close = html.find("</script>")

    if script_start < 0 or script_close < 0:
        fail("cannot isolate <script> body for tag-balance check")

    # Keep both the opening and closing <script> tags themselves (they are
    # real markup and already balance each other 1:1); drop only the JS
    # source between them, since it is full of bare `<` comparisons that a
    # tag-shaped regex would otherwise misread as markup.
    markup = (
        html[:script_start] +
        "<script></script>" +
        html[script_close + len("</script>"):]
    )

    opens: Counter[str] = Counter()
    closes: Counter[str] = Counter()

    for is_close, name in re.findall(
        r"<(/?)([a-zA-Z][a-zA-Z0-9]*)\b",
        markup,
    ):
        tag = name.lower()

        if is_close:
            closes[tag] += 1
        else:
            opens[tag] += 1

    mismatched = sorted(
        tag for tag in set(opens) | set(closes)
        if tag not in VOID_TAGS and opens[tag] != closes[tag]
    )

    if mismatched:
        fail(
            "unbalanced static HTML tag(s): " +
            ", ".join(
                f"{tag} (open={opens[tag]}, close={closes[tag]})"
                for tag in mismatched
            )
        )

    stray_void_closes = sorted(
        tag for tag in VOID_TAGS
        if closes[tag] > 0
    )

    if stray_void_closes:
        fail(
            "void element(s) with a stray close tag: " +
            ", ".join(stray_void_closes)
        )


def check_q12_gain_roundtrip() -> None:
    """Every integer Q12 gain (0..4096) must survive display and re-entry.

    Reimplements the exact two functions the page uses to move between the
    runtime Q12 contract and the user-facing percent field:

        gainPercent(q) = (q*100/4096).toFixed(3), trailing zeros trimmed
        applyCurve()   = Math.round(percent*4096/100)

    toFixed(3) and Math.round both operate on the IEEE-754 double already
    produced by q*100/4096; Python's `format(x, ".3f")` on the same double
    performs the same round-half-away-from-zero rounding for this value
    range, so this is a faithful, dependency-free stand-in for running the
    real JS engine. (Cross-checked once against an actual browser JS
    engine over all 4097 values during the Stage 44 audit: 0 mismatches.)
    """

    errors = []

    for q in range(0, 4097):
        percent_exact = q * 100 / 4096
        percent_displayed = float(
            format(percent_exact, ".3f")
        )

        q_back = round(
            percent_displayed * 4096 / 100
        )

        if q_back != q:
            errors.append(
                (q, percent_displayed, q_back)
            )

    if errors:
        fail(
            "Q12<->percent round-trip mismatch for "
            f"{len(errors)} value(s), first: {errors[0]}"
        )


def fail(message: str) -> None:
    print(f"Web UI check FAILED: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    text = SOURCE.read_text(encoding="utf-8")

    start = text.find(START)
    if start < 0:
        fail("embedded HTML start marker not found")

    start += len(START)
    end = text.find(END, start)
    if end < 0:
        fail("embedded HTML end marker not found")

    html = text[start:end]

    ids = re.findall(r'\bid="([^"]+)"', html)
    counts = Counter(ids)

    duplicates = sorted(
        name for name, count in counts.items()
        if count > 1
    )

    if duplicates:
        fail(
            "duplicate static DOM id(s): " +
            ", ".join(duplicates)
        )

    missing_ids = sorted(
        name for name in REQUIRED_IDS
        if counts[name] != 1
    )

    if missing_ids:
        fail(
            "missing required DOM id(s): " +
            ", ".join(missing_ids)
        )

    missing_tokens = sorted(
        token for token in REQUIRED_TOKENS
        if token not in html
    )

    if missing_tokens:
        fail(
            "missing required UI contract token(s): " +
            ", ".join(missing_tokens)
        )

    forbidden = sorted(
        token for token in FORBIDDEN_TOKENS
        if token in html
    )

    if forbidden:
        fail(
            "stale UI contract token(s) returned: " +
            ", ".join(forbidden)
        )

    for page in ("Home", "Led", "Tof", "Diag", "System"):
        page_id = f"page{page}"
        nav_id = f"nav{page}"

        if counts[page_id] != 1 or counts[nav_id] != 1:
            fail(
                f"navigation pair is incomplete: "
                f"{nav_id}/{page_id}"
            )

    if html.count("<script>") != 1 or html.count("</script>") != 1:
        fail("expected exactly one inline script")

    if html.count("<style>") != 1 or html.count("</style>") != 1:
        fail("expected exactly one inline style block")

    check_tag_balance(html)
    check_q12_gain_roundtrip()

    print(
        "Web UI check OK: "
        f"{len(ids)} static IDs, "
        f"{len(html)} embedded HTML characters, "
        "tag balance OK, Q12 round-trip OK (4097/4097)"
    )


if __name__ == "__main__":
    main()
