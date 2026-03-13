# PixelPal OS

PixelPal OS is a lightweight handheld gaming environment for the ROKNET Systems Pixel Pal. This repository implements the greenfield v0/v1 scaffold described in the project plan:

- a native launcher core
- a small C SDK for games
- service and systemd scaffolding for Arch Linux ARM
- install and cache tooling
- a sample game package

The current repo is intentionally boring and practical. It favors explicit files, shell/Python helpers, and a single runtime pattern over a deep framework.

## Repository layout

- `launcher/` native launcher core
- `sdk/` `libpixelpal` C runtime surface
- `services/` boot/session/service scripts and systemd units
- `tools/` package install and cache refresh tools
- `sample-games/` reference game package and source
- `themes/` placeholder for the default theme payload
- `docs/` architecture and deployment notes
- `packaging/` image and filesystem layout notes

## What is implemented

- Manifest schema and scanner for installed games
- Headless launcher loop for cataloging, launching, settings actions, and status reading
- `pixelpal-run.sh` runner with log capture and session state output
- `pixelpal-preflight.sh` writable directory bootstrap
- `pixelpal-statusd.py` status publisher for power, Wi-Fi, and volume
- `pixelpal-install.py` package installer
- `pixelpal-refresh-cache.py` manifest cache builder
- launcher menu audio support with theme-driven music and UI sound hooks
- `libpixelpal` SDK with normalized input mapping, save/config path handling, framebuffer defaults, and clean-exit support
- `Stonefall`, a Tetris-style falling-block game used for bring-up

## What still requires target hardware

- SPI display tuning and final render path choice
- GPIO and device tree mapping for the physical controls
- battery telemetry integration for the chosen fuel gauge or ADC
- ALSA mixer tuning for the selected audio hardware
- Wi-Fi helper commands matching the final Arch image
- replacement of the temporary text-mode launcher frontend with the final SDL2 fullscreen UI
- first-pass account binding, friends list UI, and OTA flows remain intentionally deferred

## Build notes

The code targets Linux on the Raspberry Pi Zero 2 W, but the launcher core and `Stonefall` are also set up to support Windows-side development and testing. This workspace does not include a local compiler or SDL2 development environment, so the repo is scaffolded for target-side bring-up rather than compiled here.

Expected target-side dependencies:

- CMake
- a C and C++ compiler
- SDL2 development headers and libraries
- Python 3 for service tooling

The repo also includes [CMakePresets.json](C:/Users/ryanr/Documents/New%20project/CMakePresets.json) with starter Linux and Windows debug presets for local development.

## Input model

The current platform contract matches the intended handheld controls:

- D-pad
- `A`
- `B`
- `Start`
- `Select`

Default keyboard mapping for development:

- arrows = D-pad
- `Z` or `C` = `A`
- `X` = `B`
- `Enter` = `Start`
- `Right Shift` or `Backspace` = `Select`
- `Esc` = immediate exit

The SDK also reserves a long-press `Start + Select` combo as the standard system exit path.

On Windows, the launcher will fall back to directly executing the game binary when the Linux shell runner is not present, which keeps local iteration simple.

## Theme audio layout

The default theme now reserves `themes/default/audio/` for launcher sounds.

- `menu_theme.wav` for looping menu music
- `menu_move.wav` for D-pad navigation
- `menu_confirm.wav` for selecting or launching
- `menu_back.wav` for cancel/back

The launcher reads those paths from [manifest.toml](C:/Users/ryanr/Documents/New%20project/themes/default/audio/manifest.toml), so later theme swaps only need to replace files or point the manifest at new ones.

## Deferred product hooks

The architecture now keeps room for:

- LAN multiplayer with a future friends list and device identity layer
- a simple account model where a username can be linked to one device until wipe
- OTA updates for limited-release hardware batches

Those capabilities are intentionally not implemented yet in this scaffold.
