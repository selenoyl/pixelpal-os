# PixelPal OS Architecture

## Runtime contract

PixelPal OS is split into four concrete layers:

1. Arch Linux ARM base with `systemd`
2. Appliance services under `services/`
3. Launcher process under `launcher/`
4. One official game runtime surface in `sdk/`

## Launcher contract

The launcher owns:

- reading cached or installed manifests
- displaying available games
- launching exactly one game at a time through `pixelpal-run.sh`
- reading status snapshots from `/run/pixelpal/status`
- loading theme audio for menu music and UI sounds
- exposing reboot and shutdown actions

The launcher does not own:

- Wi-Fi scanning logic
- battery polling logic
- backlight control logic
- per-game save path management

## Service contract

Service scripts publish normalized state into files:

- `/run/pixelpal/status/power.json`
- `/run/pixelpal/status/wifi.json`
- `/run/pixelpal/status/audio.json`
- `/run/pixelpal/session/last-exit.json`

This keeps the surface simple for both the launcher and future SDK integrations.

## Game package contract

Every installed game lives under `/opt/pixelpal/games/<game-id>/` and includes:

- `manifest.toml`
- a runnable executable or wrapper named in `exec`
- an icon asset

Writable data never lives inside the package directory.

## Bring-up order

1. Run `pixelpal-preflight.sh`
2. Start `pixelpal-statusd.py`
3. Install a sample game with `pixelpal-install.py`
4. Run the launcher against the installed game root
5. Validate launch, exit, save paths, and logs

## Current hardware input contract

PixelPal OS currently standardizes on:

- D-pad
- `A`
- `B`
- `Start`
- `Select`

The SDK uses this smaller control surface as the baseline so games stay portable between Windows dev builds and the final handheld.

## Deferred identity and online hooks

Future platform work is expected to layer on:

- a device-bound username record stored in system config
- a simple friends list that can start as LAN-visible identities
- later migration from LAN-only middleware to internet-capable services
- OTA update plumbing for small release batches

None of those flows are active in v0/v1, but the architecture should keep them in the launcher/settings/network domain rather than inside individual games.
