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

    print(
        "Web UI check OK: "
        f"{len(ids)} static IDs, "
        f"{len(html)} embedded HTML characters"
    )


if __name__ == "__main__":
    main()
