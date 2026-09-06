#!/usr/bin/env python3
"""Validate the Ambilight 16 MiB ESP32-C6 partition table."""

from __future__ import annotations

import csv
from pathlib import Path
import sys

FLASH_BYTES = 16 * 1024 * 1024
APP_SLOT_BYTES = 7 * 1024 * 1024
APP_ALIGNMENT = 0x10000
DATA_ALIGNMENT = 0x1000


def parse_int(text: str) -> int:
    value = text.strip()
    if not value:
        raise ValueError("empty numeric field")
    upper = value.upper()
    if upper.endswith("K"):
        return int(upper[:-1], 0) * 1024
    if upper.endswith("M"):
        return int(upper[:-1], 0) * 1024 * 1024
    return int(value, 0)


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent
    path = repo_root / "partitions" / "ambilight_16mb_ota.csv"

    rows: list[dict[str, int | str]] = []

    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.reader(
            line for line in handle
            if line.strip() and not line.lstrip().startswith("#")
        )

        for raw in reader:
            if len(raw) < 5:
                raise ValueError(f"invalid row: {raw!r}")

            name = raw[0].strip()
            ptype = raw[1].strip()
            subtype = raw[2].strip()
            offset = parse_int(raw[3])
            size = parse_int(raw[4])

            if size <= 0:
                raise ValueError(f"{name}: size must be positive")

            alignment = APP_ALIGNMENT if ptype == "app" else DATA_ALIGNMENT
            if offset % alignment:
                raise ValueError(
                    f"{name}: offset 0x{offset:X} is not aligned to 0x{alignment:X}"
                )

            end = offset + size
            if end > FLASH_BYTES:
                raise ValueError(
                    f"{name}: end 0x{end:X} exceeds 16 MiB flash"
                )

            rows.append(
                {
                    "name": name,
                    "type": ptype,
                    "subtype": subtype,
                    "offset": offset,
                    "size": size,
                    "end": end,
                }
            )

    by_name = {str(row["name"]): row for row in rows}

    for required in ("nvs", "otadata", "app0", "app1", "storage", "coredump"):
        if required not in by_name:
            raise ValueError(f"missing required partition: {required}")

    ordered = sorted(rows, key=lambda row: int(row["offset"]))
    for previous, current in zip(ordered, ordered[1:]):
        if int(previous["end"]) > int(current["offset"]):
            raise ValueError(
                f"overlap: {previous['name']} ends at 0x{int(previous['end']):X}, "
                f"{current['name']} starts at 0x{int(current['offset']):X}"
            )

    if int(by_name["nvs"]["offset"]) != 0x9000:
        raise ValueError("nvs must start at 0x9000")

    if int(by_name["otadata"]["size"]) != 0x2000:
        raise ValueError("otadata must be exactly 8 KiB")

    for name in ("app0", "app1"):
        row = by_name[name]
        if row["type"] != "app":
            raise ValueError(f"{name} must be an app partition")
        if int(row["size"]) != APP_SLOT_BYTES:
            raise ValueError(f"{name} must be exactly 7 MiB")

    if int(by_name["app0"]["offset"]) != 0x10000:
        raise ValueError("app0 must start at 0x10000")

    if int(by_name["coredump"]["end"]) != FLASH_BYTES:
        raise ValueError("coredump must terminate exactly at the 16 MiB boundary")

    print(
        "partition_ok=1 "
        f"flash={FLASH_BYTES} "
        f"app_slot={APP_SLOT_BYTES} "
        f"partitions={len(rows)}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"partition_ok=0 error={exc}", file=sys.stderr)
        raise SystemExit(1)
