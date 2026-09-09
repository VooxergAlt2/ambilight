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
    "function actionSequenceAfter(",
    "function tofOperationalStatus(",
    "function spatialNumber(",
    "wifiOpen",
}

FORBIDDEN_TOKENS = {
    "$('mapApply').disabled=s.output.brightness!==0",
    "s.output.brightness<=64",
    "Distance → gain Q12",
    "Clear NVS Wi-Fi",
    "function f1(",
    "post('/api/wifi',$('ssid').value+'|'+p",
}



# Void HTML elements never carry a matching close tag.
VOID_TAGS = {"input", "meta", "br", "img", "hr", "link"}


def check_tag_balance(html: str) -> None:
    """Validate static HTML nesting without adding an HTML dependency.

    The embedded JavaScript body is removed first because comparison operators
    can look tag-shaped to a lightweight tokenizer. Void elements are ignored;
    every other closing tag must match the most recently opened tag.
    """

    script_start = html.find("<script>")
    script_close = html.find("</script>")

    if script_start < 0 or script_close < 0:
        fail("cannot isolate <script> body for tag-nesting check")

    markup = (
        html[:script_start]
        + "<script></script>"
        + html[script_close + len("</script>"):]
    )

    stack: list[str] = []

    for match in re.finditer(
        r"<(/?)([a-zA-Z][a-zA-Z0-9]*)\b[^>]*>",
        markup,
    ):
        is_close = bool(match.group(1))
        tag = match.group(2).lower()

        if tag in VOID_TAGS:
            if is_close:
                fail(f"void element has a stray close tag: {tag}")
            continue

        if not is_close:
            stack.append(tag)
            continue

        if not stack:
            fail(f"stray closing tag: {tag}")

        expected = stack.pop()

        if expected != tag:
            fail(
                "mis-nested static HTML: "
                f"expected </{expected}> before </{tag}>"
            )

    if stack:
        fail(
            "unclosed static HTML tag(s): "
            + ", ".join(stack)
        )

def check_q12_gain_roundtrip() -> None:
    """Prove that the 3-decimal percent editor preserves all Q12 values.

    For q in 0..4096, q*100/4096 is exactly representable as a binary float
    because the reduced denominator is a power of two. JavaScript toFixed(3)
    therefore rounds the exact positive value to the nearest 0.001 percent,
    with half values rounded upward. Math.round() is modelled the same way
    using integer arithmetic, avoiding Python's different tie-breaking rules.
    """

    errors = []

    for q in range(0, 4097):
        numerator = q * 100_000
        displayed_milli_percent = (
            numerator * 2 + 4096
        ) // (2 * 4096)

        reconstruction_numerator = (
            displayed_milli_percent * 4096
        )
        q_back = (
            reconstruction_numerator * 2 + 100_000
        ) // (2 * 100_000)

        if q_back != q:
            errors.append(
                (
                    q,
                    displayed_milli_percent,
                    q_back,
                )
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
