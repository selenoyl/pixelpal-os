#!/usr/bin/env python3

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


def parse_manifest(path: Path) -> dict:
    parsed = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if not line or line.startswith("[") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        parsed[key.strip()] = value.strip().strip('"')
    return parsed


def copy_game_package(source: Path, destination_root: Path) -> Path:
    manifest_path = source / "manifest.toml"
    if not manifest_path.is_file():
        raise SystemExit(f"missing manifest: {manifest_path}")

    manifest = parse_manifest(manifest_path)
    game_id = manifest.get("id", "")
    if not game_id:
        raise SystemExit("manifest missing id")

    target = destination_root / game_id
    if target.exists():
        shutil.rmtree(target)

    shutil.copytree(source, target)
    exec_relative = manifest.get("exec", "")
    if exec_relative:
        executable = target / exec_relative
        if executable.exists():
            executable.chmod(executable.stat().st_mode | 0o111)
    return target


def ensure_mutable_dirs(game_id: str, saves_root: Path, config_root: Path) -> None:
    (saves_root / game_id).mkdir(parents=True, exist_ok=True)
    (config_root / game_id).mkdir(parents=True, exist_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="Install a PixelPal game package")
    parser.add_argument("source", help="Path to an unpacked game folder")
    parser.add_argument("--games-root", default="/opt/pixelpal/games")
    parser.add_argument("--saves-root", default="/var/lib/pixelpal/saves")
    parser.add_argument("--config-root", default="/var/lib/pixelpal/config")
    parser.add_argument("--cache-root", default="/var/cache/pixelpal/manifests")
    args = parser.parse_args()

    source = Path(args.source).resolve()
    games_root = Path(args.games_root)
    saves_root = Path(args.saves_root)
    config_root = Path(args.config_root)
    cache_root = Path(args.cache_root)

    games_root.mkdir(parents=True, exist_ok=True)
    saves_root.mkdir(parents=True, exist_ok=True)
    config_root.mkdir(parents=True, exist_ok=True)
    cache_root.mkdir(parents=True, exist_ok=True)

    target = copy_game_package(source, games_root)
    manifest = parse_manifest(target / "manifest.toml")
    ensure_mutable_dirs(manifest["id"], saves_root, config_root)

    refresh_script = Path(__file__).with_name("pixelpal-refresh-cache.py")
    if refresh_script.is_file():
        subprocess.run(
            [
                sys.executable,
                str(refresh_script),
                "--games-root",
                str(games_root),
                "--cache-root",
                str(cache_root),
            ],
            check=False,
        )

    print(f"Installed {manifest['id']} to {target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
