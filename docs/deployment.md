# Deployment Notes

## On-device directories

- `/usr/lib/pixelpal/launcher`
- `/usr/lib/pixelpal/services`
- `/usr/lib/pixelpal/sdk`
- `/usr/share/pixelpal/themes`
- `/etc/pixelpal`
- `/opt/pixelpal/games`
- `/var/lib/pixelpal/saves`
- `/var/lib/pixelpal/config`
- `/var/cache/pixelpal/manifests`
- `/var/log/pixelpal/games`
- `/run/pixelpal/status`
- `/run/pixelpal/session`

## Suggested first bring-up

1. Copy service scripts into `/usr/lib/pixelpal/services/`
2. Install the systemd units from `services/systemd/`
3. Build and copy `pixelpal-launcher` and `libpixelpal`
4. Install the Stonefall sample game into `/opt/pixelpal/games/stonefall`
5. Run `pixelpal-refresh-cache.py`
6. Enable `pixelpal.target` as the default boot target or start it manually for bring-up

## Service ordering

- `pixelpal-preflight.service` should run before status and launcher services
- `pixelpal-statusd.service` should run before the launcher
- `pixelpal-launcher.service` should be restarted automatically on crash
- `pixelpal.target` is the appliance target that replaces a desktop session

## Windows development

For local Windows iteration:

1. Build `stonefall` and `pixelpal-launcher` with SDL2 available to CMake.
2. Run the launcher without the Linux runner path, or point it at a local game tree.
3. Let the SDK use its Windows default save/config directories under `LOCALAPPDATA`.

This is a development path only. Appliance boot, system services, safe shutdown, and hardware polling still belong to Linux on the Pi.
