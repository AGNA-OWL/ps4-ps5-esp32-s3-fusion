#!/usr/bin/env python3
"""Lightweight pre-flight checks for the fusion firmware project."""
from __future__ import annotations

import py_compile
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LITTLEFS_LIMIT = 0xCF0000

REQUIRED = [
    "platformio.ini",
    "partitions_16mb.csv",
    "README.md",
    "LICENSE",
    "src/main.cpp",
    "src/portal_common.cpp",
    "src/dns_portal.cpp",
    "src/http_portal.cpp",
    "src/https_portal.cpp",
    "src/server_certs.h",
    "templates/ps4_index.html",
    "templates/admin.html",
    "tools/stage_payloads.py",
    "tools/gen_certs.py",
]

REQUIRED_DATA = [
    "index.html.gz",
    "cache.appcache",
    "ps4/index.html.gz",
    "ps4/g2all/index.html.gz",
    "ps4/g2all/goldhen_2.4b18.14.bin.gz",
    "ps4/672goldhen/index.html.gz",
    "ps4/672goldhen/cache.manifest",
    "ps4/672goldhen/goldhen_2.4b18.14.bin.gz",
    "ps4/672goldhen/jb.js.gz",
    "ps4/style.css.gz",
    "ps4/img.jpg",
    "portal.html.gz",
    "status.html.gz",
    "admin.html.gz",
    "payloads/.gitkeep",
]


def main() -> int:
    failures: list[str] = []

    for rel in REQUIRED:
        if not (ROOT / rel).is_file():
            failures.append(f"missing: {rel}")

    for rel in REQUIRED_DATA:
        if not (ROOT / "data" / rel).is_file():
            failures.append(f"missing generated data: data/{rel}")

    for script in (ROOT / "tools").glob("*.py"):
        try:
            py_compile.compile(str(script), doraise=True)
        except py_compile.PyCompileError as exc:
            failures.append(f"python syntax error in {script.name}: {exc}")

    data = ROOT / "data"
    if data.is_dir():
        total = sum(p.stat().st_size for p in data.rglob("*") if p.is_file())
        if total > LITTLEFS_LIMIT:
            failures.append(
                f"LittleFS overflow: {total} bytes > {LITTLEFS_LIMIT} bytes"
            )
        print(f"data/: {total / 1024 / 1024:.2f} MiB")

    partition_text = (ROOT / "partitions_16mb.csv").read_text(encoding="utf-8")
    if "0x310000, 0xCF0000" not in partition_text:
        failures.append("partition table spiffs entry does not match LITTLEFS_LIMIT")

    if failures:
        print("\nFAILED:", file=sys.stderr)
        for item in failures:
            print(" -", item, file=sys.stderr)
        return 1

    print("All pre-flight checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
