#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
  echo "usage: pixelpal-run.sh <game-root>" >&2
  exit 2
fi

GAME_ROOT="$1"
MANIFEST_PATH="${GAME_ROOT}/manifest.toml"
SAVE_ROOT="${PIXELPAL_SAVE_ROOT:-/var/lib/pixelpal/saves}"
CONFIG_ROOT="${PIXELPAL_GAME_CONFIG_ROOT:-/var/lib/pixelpal/config}"
LOG_ROOT="${PIXELPAL_LOG_ROOT:-/var/log/pixelpal/games}"
SESSION_ROOT="${PIXELPAL_RUN_SESSION_ROOT:-/run/pixelpal/session}"

if [ ! -f "${MANIFEST_PATH}" ]; then
  echo "missing manifest: ${MANIFEST_PATH}" >&2
  exit 3
fi

GAME_ID="$(awk -F= '/^id[[:space:]]*=/{gsub(/[ "]/, "", $2); print $2}' "${MANIFEST_PATH}")"
EXEC_RELATIVE="$(awk -F= '/^exec[[:space:]]*=/{gsub(/^ +| +$/, "", $2); gsub(/"/, "", $2); print $2}' "${MANIFEST_PATH}")"

if [ -z "${GAME_ID}" ] || [ -z "${EXEC_RELATIVE}" ]; then
  echo "manifest missing id or exec: ${MANIFEST_PATH}" >&2
  exit 4
fi

EXECUTABLE="${GAME_ROOT}/${EXEC_RELATIVE}"
LOG_PATH="${LOG_ROOT}/${GAME_ID}.log"

install -d "${SAVE_ROOT}/${GAME_ID}"
install -d "${CONFIG_ROOT}/${GAME_ID}"
install -d "${LOG_ROOT}"
install -d "${SESSION_ROOT}"

if [ ! -x "${EXECUTABLE}" ]; then
  chmod +x "${EXECUTABLE}"
fi

export PIXELPAL_GAME_ID="${GAME_ID}"
export PIXELPAL_SAVE_ROOT="${SAVE_ROOT}"
export PIXELPAL_GAME_CONFIG_ROOT="${CONFIG_ROOT}"

set +e
"${EXECUTABLE}" >>"${LOG_PATH}" 2>&1
EXIT_CODE="$?"
set -e

cat >"${SESSION_ROOT}/last-exit.json" <<EOF
{
  "game_id": "${GAME_ID}",
  "exit_code": ${EXIT_CODE}
}
EOF

exit "${EXIT_CODE}"
