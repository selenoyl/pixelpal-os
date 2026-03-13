# PixelPal Filesystem Layout

## Immutable payload

- `/usr/lib/pixelpal/launcher/pixelpal-launcher`
- `/usr/lib/pixelpal/services/pixelpal-preflight.sh`
- `/usr/lib/pixelpal/services/pixelpal-run.sh`
- `/usr/lib/pixelpal/services/pixelpal-statusd.py`
- `/usr/lib/pixelpal/sdk/libpixelpal.a`

## Mutable payload

- `/var/cache/pixelpal/manifests/*.cache`
- `/var/lib/pixelpal/saves/<game-id>/`
- `/var/lib/pixelpal/config/<game-id>/`
- `/var/log/pixelpal/games/<game-id>.log`
- `/run/pixelpal/status/*.json`
- `/run/pixelpal/session/*.json`

## Install flow

1. Copy game package to `/opt/pixelpal/games/<game-id>/`
2. Create save and config directories
3. Refresh the manifest cache
4. Restart the launcher if desired

## Deferred system data

Reserved future mutable data locations:

- `/var/lib/pixelpal/device-profile/`
- `/var/lib/pixelpal/accounts/`
- `/var/lib/pixelpal/friends/`
- `/var/lib/pixelpal/update-state/`
