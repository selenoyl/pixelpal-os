#!/bin/sh
set -eu

BASE_LIB="${PIXELPAL_LIB_ROOT:-/usr/lib/pixelpal}"
THEME_ROOT="${PIXELPAL_THEME_ROOT:-/usr/share/pixelpal/themes}"
CONFIG_ROOT="${PIXELPAL_CONFIG_ROOT:-/etc/pixelpal}"
GAMES_ROOT="${PIXELPAL_GAMES_ROOT:-/opt/pixelpal/games}"
SAVE_ROOT="${PIXELPAL_SAVE_ROOT:-/var/lib/pixelpal/saves}"
GAME_CONFIG_ROOT="${PIXELPAL_GAME_CONFIG_ROOT:-/var/lib/pixelpal/config}"
CACHE_ROOT="${PIXELPAL_CACHE_ROOT:-/var/cache/pixelpal/manifests}"
LOG_ROOT="${PIXELPAL_LOG_ROOT:-/var/log/pixelpal/games}"
RUN_STATUS_ROOT="${PIXELPAL_RUN_STATUS_ROOT:-/run/pixelpal/status}"
RUN_SESSION_ROOT="${PIXELPAL_RUN_SESSION_ROOT:-/run/pixelpal/session}"

install -d "${BASE_LIB}/launcher"
install -d "${BASE_LIB}/services"
install -d "${BASE_LIB}/sdk"
install -d "${THEME_ROOT}"
install -d "${CONFIG_ROOT}"
install -d "${GAMES_ROOT}"
install -d "${SAVE_ROOT}"
install -d "${GAME_CONFIG_ROOT}"
install -d "${CACHE_ROOT}"
install -d "${LOG_ROOT}"
install -d "${RUN_STATUS_ROOT}"
install -d "${RUN_SESSION_ROOT}"

