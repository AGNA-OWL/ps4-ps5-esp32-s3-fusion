#!/usr/bin/env python3
"""Stage third-party exploit hosts into data/ for the ESP32-S3 fusion firmware.

This script does NOT vendor exploit code into git. It clones small shallow
checkouts into .cache/ and copies only the static files needed by the portal.
Run it before `pio run -t uploadfs`.

Sources:
  PS5 autoloader : percdotdev/ps5-webkit-autoloader-esp32 (host/packed)
  PS4 PSPulse    : psx8/psx8.github.io
  PS4 Afterglow  : well0nez/afterglow-web
  PS4 multi-FW   : GamerHack/GamerHack.github.io (g2all auto-detect host)
  PS4 9.00 USB   : doj/ESP32-Server-900u (exfathax.h only)
"""
from __future__ import annotations

import argparse
import gzip
import json
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CACHE = ROOT / ".cache"
DEFAULT_DATA = ROOT / "data"

LITTLEFS_LIMIT = 0xCF0000  # Must match the spiffs partition in partitions_16mb.csv

REPOS = {
    "ps5": (
        "https://github.com/percdotdev/ps5-webkit-autoloader-esp32.git",
        "ps5-autoloader",
    ),
    "pspulse": (
        "https://github.com/psx8/psx8.github.io.git",
        "psx8",
    ),
    "afterglow": (
        "https://github.com/well0nez/afterglow-web.git",
        "afterglow-web",
    ),
    "gamerhack": (
        "https://github.com/GamerHack/GamerHack.github.io.git",
        "gamerhack",
    ),
    "exfathax": (
        "https://github.com/doj/ESP32-Server-900u.git",
        "ps4-900u",
    ),
}

PSX8_DIRS = ["505", "672", "700", "900", "1300", "1352", "css"]
PSX8_FILES = ["index.html", "navigation.manifest", "pspulse.png", "README.md"]

GZIP_SUFFIXES = {
    ".html", ".htm", ".js", ".mjs", ".css", ".json", ".svg",
    ".txt", ".lua", ".elf", ".bin",
}


def run(cmd: list[str], cwd: Path | None = None) -> None:
    print("+", " ".join(str(c) for c in cmd), flush=True)
    subprocess.run(cmd, cwd=cwd, check=True)


def repo_commit(path: Path) -> str:
    result = subprocess.run(
        ["git", "-C", str(path), "rev-parse", "--short", "HEAD"],
        capture_output=True, text=True, check=False,
    )
    return result.stdout.strip() or "unknown"


def ensure_repo(cache: Path, key: str, update: bool) -> Path:
    url, name = REPOS[key]
    dest = cache / name
    if not dest.exists():
        run(["git", "clone", "--depth", "1", url, str(dest)])
    elif update:
        run(["git", "-C", str(dest), "fetch", "--depth", "1", "origin", "HEAD"])
        run(["git", "-C", str(dest), "reset", "--hard", "FETCH_HEAD"])
    if not dest.is_dir():
        raise SystemExit(f"repository staging failed: {dest}")
    return dest


def copy_tree(src: Path, dst: Path) -> None:
    if not src.exists():
        raise SystemExit(f"missing upstream path: {src}")
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(src, dst, dirs_exist_ok=True)


def stage_ps5(ps5: Path, data: Path) -> None:
    packed = ps5 / "host" / "packed"
    copy_tree(packed, data)


def stage_pspulse(psx8: Path, data: Path) -> None:
    target = data / "ps4" / "pspulse"
    target.mkdir(parents=True, exist_ok=True)
    for name in PSX8_DIRS:
        src = psx8 / name
        if src.is_dir():
            copy_tree(src, target / name)
    for name in PSX8_FILES:
        src = psx8 / name
        if src.is_file():
            shutil.copy2(src, target / name)


def stage_afterglow(afterglow: Path, data: Path) -> None:
    target = data / "ps4" / "afterglow"
    target.mkdir(parents=True, exist_ok=True)
    skip = {".git", ".gitignore", ".nojekyll", "serve.sh", "make-manifest.sh"}
    for item in afterglow.iterdir():
        if item.name in skip or item.name == ".git":
            continue
        if item.is_dir():
            copy_tree(item, target / item.name)
        elif item.is_file():
            shutil.copy2(item, target / item.name)


def stage_gamerhack(gamerhack: Path, data: Path) -> None:
    """Stage GamerHack's g2all auto-detect host as the unified PS4 entry.

    g2all reads the PS4 firmware from navigator.userAgent and loads the right
    exploit chain (5.05, 7.00-8.52, 9.00-9.60, 10.00-11.02, 11.50-14.02).
    Its manifests use Windows backslashes and reference ../style.css and
    ../img.jpg, so normalize paths and stage those two files in data/ps4/.
    """
    src = gamerhack / "g2all"
    if not src.is_dir():
        raise SystemExit(f"missing g2all path: {src}")
    target = data / "ps4" / "g2all"
    copy_tree(src, target)

    # Manifests must use forward slashes for the HTTP server.
    for manifest in target.glob("*.manifest"):
        text = manifest.read_text(encoding="utf-8", errors="replace")
        manifest.write_text(text.replace("\\", "/"), encoding="utf-8")

    # g2all pages reference ../style.css and ../img.jpg from inside g2all/.
    for name in ("style.css", "img.jpg"):
        upstream = gamerhack / name
        if upstream.is_file():
            shutil.copy2(upstream, data / "ps4" / name)


def stage_templates(data: Path) -> None:
    templates = ROOT / "templates"
    (data / "ps4").mkdir(parents=True, exist_ok=True)
    # /ps4/ is our own landing page; it detects the console and forwards to
    # /ps4/g2all/index.html (relative resources there stay intact).
    shutil.copy2(templates / "ps4_index.html", data / "ps4" / "index.html")
    shutil.copy2(templates / "admin.html", data / "admin.html")
    shutil.copy2(templates / "status.html", data / "status.html")
    shutil.copy2(templates / "portal.html", data / "portal.html")


def stage_exfathax(ps4_900u: Path) -> None:
    src = ps4_900u / "ESP32_Server_900u" / "exfathax.h"
    if not src.is_file():
        raise SystemExit(f"missing exfathax source: {src}")
    header = (
        "// Generated by tools/stage_payloads.py from doj/ESP32-Server-900u.\n"
        "// Kept out of git; see README for attribution and license notes.\n"
        + src.read_text(encoding="utf-8")
    )
    (ROOT / "src" / "exfathax.h").write_text(header, encoding="utf-8")


def gzip_static_files(data: Path) -> int:
    count = 0
    for path in sorted(data.rglob("*")):
        if not path.is_file() or path.name == ".gitkeep":
            continue
        if path.suffix.lower() in {".gz", ".appcache", ".manifest", ".cache"}:
            continue
        if path.suffix.lower() not in GZIP_SUFFIXES:
            continue
        gz_path = path.with_name(path.name + ".gz")
        if gz_path.exists():
            gz_path.unlink()
        with path.open("rb") as source, gz_path.open("wb") as raw:
            with gzip.GzipFile(fileobj=raw, mode="wb", compresslevel=9, mtime=0) as out:
                shutil.copyfileobj(source, out)
        # The firmware serves the .gz copy transparently; keeping both would
        # double the LittleFS footprint.
        path.unlink()
        count += 1
    return count


def write_build_info(data: Path, repos: dict[str, Path]) -> None:
    info = {
        "generated": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "target": "esp32-s3-n16r8",
        "sources": {key: repo_commit(path) for key, path in repos.items()},
    }
    (data / "build_info.json").write_text(
        json.dumps(info, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )


def report_size(data: Path) -> None:
    total = sum(p.stat().st_size for p in data.rglob("*") if p.is_file())
    mib = total / 1024 / 1024
    limit_mib = LITTLEFS_LIMIT / 1024 / 1024
    print(f"\nLittleFS payload size: {mib:.2f} MiB / {limit_mib:.2f} MiB")
    if total > LITTLEFS_LIMIT:
        print(
            "ERROR: staged data exceeds the LittleFS partition. "
            "Remove unused firmware directories or enlarge the spiffs entry.",
            file=sys.stderr,
        )
        raise SystemExit(1)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, default=DEFAULT_CACHE)
    parser.add_argument("--data", type=Path, default=DEFAULT_DATA)
    parser.add_argument("--update", action="store_true", help="fetch latest upstream commits")
    parser.add_argument("--no-gzip", action="store_true")
    args = parser.parse_args()

    cache = args.cache.resolve()
    data = args.data.resolve()
    cache.mkdir(parents=True, exist_ok=True)
    if data.exists():
        for item in data.iterdir():
            if item.name != ".gitkeep":
                if item.is_dir():
                    shutil.rmtree(item)
                else:
                    item.unlink()
    data.mkdir(parents=True, exist_ok=True)

    repos = {key: ensure_repo(cache, key, args.update) for key in REPOS}

    stage_ps5(repos["ps5"], data)
    stage_pspulse(repos["pspulse"], data)
    stage_afterglow(repos["afterglow"], data)
    stage_gamerhack(repos["gamerhack"], data)
    stage_templates(data)
    (data / "payloads").mkdir(parents=True, exist_ok=True)
    (data / "payloads" / ".gitkeep").write_text("", encoding="utf-8")
    stage_exfathax(repos["exfathax"])
    write_build_info(data, repos)

    if not args.no_gzip:
        count = gzip_static_files(data)
        print(f"gzip'd {count} static files")

    report_size(data)
    print("\nStaging complete. Next: pio run -t uploadfs && pio run -t upload")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
