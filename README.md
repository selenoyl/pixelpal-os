# PixelPal OS

PixelPal OS is a lightweight handheld gaming environment for the ROKNET Systems Pixel Pal, a custom Raspberry Pi Zero 2 W portable console. The project uses Arch Linux ARM as the base OS and adds a focused console-style layer on top: fullscreen launcher, simple runtime contract for games, hardware services, and a small set of bundled reference games.

This repository is the working source tree for the launcher, SDK, platform services, and bundled test games.

## Project status

Current direction:

- Arch Linux ARM appliance layer for Raspberry Pi Zero 2 W
- Windows build/test workflow for fast iteration during development
- SDL-based launcher and SDL-based native games
- single-player first, LAN multiplayer later

Current bundled games:

- `BotByte` - maze chase game
- `Priory` - top-down pilgrimage RPG chapter set across a priory, market town, and wharf
- `Slinkbit` - snake-style cable game
- `Stonefall` - falling-block puzzle game

Shelved for now:

- `Grand Prix` - experimental kart racer prototype, removed from the default build and staged game list until the design is revisited

## Repository layout

- `launcher/` - PixelPal launcher source
- `sdk/` - `libpixelpal` runtime API for games
- `services/` - service scripts and `systemd` units for the Linux target
- `tools/` - install/cache helpers
- `sample-games/` - bundled sample games and prototypes
- `themes/` - default theme assets, palette, and menu audio layout
- `docs/` - architecture and deployment notes
- `cmake/` - build metadata helpers

## What exists now

- controller-first launcher flow
- Windows-friendly local development path
- Linux-oriented service scaffolding for the handheld target
- game manifest scanning and staging
- menu music and UI sound hooks
- save/config path handling through `libpixelpal`
- sample game packaging format aligned with future removable media support

## Build

### Windows

Recommended toolchain:

- MSYS2
- MinGW-w64 GCC
- CMake
- Ninja
- SDL2

Configured preset:

```sh
cmake --preset windows-debug
cmake --build --preset windows-debug
```

### Linux

Configured preset:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
```

The Linux target is the real product target. Windows exists to speed up iteration on launcher and game development before deploying to the handheld hardware.

## Runtime targets

PixelPal OS is intended to run on:

- Raspberry Pi Zero 2 W
- Arch Linux ARM
- small SPI display
- D-pad
- `A`
- `B`
- `Start`
- `Select`

The user-facing goal is a console appliance, not a general desktop Linux environment.

## Input model

Current platform controls:

- D-pad
- `A`
- `B`
- `Start`
- `Select`

Current Windows keyboard test mapping:

- arrow keys = D-pad
- `A` key = `A`
- `B` key = `B`
- `Enter` = `Start`
- `Space` = `Select`
- `Esc` = immediate exit

The platform exit combo remains `Start + Select`.

## Theme audio layout

The default theme audio contract is under `themes/default/audio/`:

- `menu_theme.wav`
- `menu_move.wav`
- `menu_confirm.wav`
- `menu_back.wav`

These are the launcher fallback audio slots for menu music and button/navigation sounds.

## Cartridge direction

Bundled games currently live in the OS build tree, but the packaging format is being kept aligned with future removable cartridge-style media. The long-term direction is for games to live on external storage such as SD or USB-like cartridge devices, with PixelPal scanning and launching them through the same manifest/runtime contract.

## Docs

- [Architecture](docs/architecture.md)
- [Deployment](docs/deployment.md)

## GitHub notes

This repo is structurally ready for GitHub. One product-level decision is still intentionally left open:

- no license file has been selected yet

If you publish without a license, the default is effectively all rights reserved. If you want an open-source release, the next step is choosing a license explicitly.
