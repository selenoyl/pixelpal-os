#!/usr/bin/env python3

import argparse
from pathlib import Path


FIELDS = [
    "id",
    "name",
    "version",
    "exec",
    "icon",
    "splash",
    "sdk_version",
    "author",
    "description",
    "supports_network_lan",
    "min_os_version",
]


def parse_manifest(path: Path) -> dict:
    parsed = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if not line or line.startswith("[") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        parsed[key.strip()] = value.strip()
    return parsed


def main() -> int:
    parser = argparse.ArgumentParser(description="Refresh PixelPal manifest cache")
    parser.add_argument("--games-root", default="/opt/pixelpal/games")
    parser.add_argument("--cache-root", default="/var/cache/pixelpal/manifests")
    args = parser.parse_args()

    games_root = Path(args.games_root)
    cache_root = Path(args.cache_root)
    cache_root.mkdir(parents=True, exist_ok=True)

    for cache_file in cache_root.glob("*.cache"):
        cache_file.unlink()

    if not games_root.exists():
        return 0

    for child in sorted(games_root.iterdir()):
        manifest = child / "manifest.toml"
        if not manifest.is_file():
            continue

        parsed = parse_manifest(manifest)
        game_id = parsed.get("id", "").replace('"', "").strip()
        if not game_id:
            continue

        lines = [f'root_dir = "{child}"', f'manifest_path = "{manifest}"']
        for field in FIELDS:
            if field in parsed:
                lines.append(f"{field} = {parsed[field]}")

        (cache_root / f"{game_id}.cache").write_text(
            "\n".join(lines) + "\n",
            encoding="utf-8",
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

