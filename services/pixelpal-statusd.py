#!/usr/bin/env python3

import json
import os
import shutil
import subprocess
import time


STATUS_ROOT = os.environ.get("PIXELPAL_RUN_STATUS_ROOT", "/run/pixelpal/status")
POLL_SECONDS = float(os.environ.get("PIXELPAL_STATUS_INTERVAL", "5"))
CRITICAL_BATTERY_PERCENT = int(os.environ.get("PIXELPAL_CRITICAL_BATTERY_PERCENT", "5"))
LOW_BATTERY_PERCENT = int(os.environ.get("PIXELPAL_LOW_BATTERY_PERCENT", "15"))


def ensure_dir(path: str) -> None:
    os.makedirs(path, exist_ok=True)


def write_json(name: str, payload: dict) -> None:
    ensure_dir(STATUS_ROOT)
    path = os.path.join(STATUS_ROOT, name)
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, sort_keys=True)


def read_int_from_file(path: str) -> int:
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return int(handle.read().strip())
    except (OSError, ValueError):
        return -1


def detect_power() -> dict:
    power = {
        "battery_percent": -1,
        "charging": False,
        "level": "unknown",
    }

    mock_percent = os.environ.get("PIXELPAL_MOCK_BATTERY_PERCENT")
    mock_charging = os.environ.get("PIXELPAL_MOCK_CHARGING")
    if mock_percent is not None:
        try:
            power["battery_percent"] = int(mock_percent)
        except ValueError:
            pass
        power["charging"] = mock_charging == "1"
    else:
        capacity_path = "/sys/class/power_supply/BAT0/capacity"
        status_path = "/sys/class/power_supply/BAT0/status"
        capacity = read_int_from_file(capacity_path)
        power["battery_percent"] = capacity
        try:
            with open(status_path, "r", encoding="utf-8") as handle:
                power["charging"] = handle.read().strip().lower() == "charging"
        except OSError:
            pass

    if power["battery_percent"] >= 0:
        if power["battery_percent"] <= CRITICAL_BATTERY_PERCENT:
            power["level"] = "critical"
        elif power["battery_percent"] <= LOW_BATTERY_PERCENT:
            power["level"] = "low"
        else:
            power["level"] = "ok"

    return power


def detect_wifi() -> dict:
    payload = {
        "connected": False,
        "ssid": "",
    }

    mock_ssid = os.environ.get("PIXELPAL_MOCK_WIFI_SSID")
    if mock_ssid:
        payload["connected"] = True
        payload["ssid"] = mock_ssid
        return payload

    if shutil.which("iwgetid") is None:
        return payload

    try:
        completed = subprocess.run(
            ["iwgetid", "-r"],
            capture_output=True,
            text=True,
            check=False,
        )
        ssid = completed.stdout.strip()
        if ssid:
            payload["connected"] = True
            payload["ssid"] = ssid
    except OSError:
        pass

    return payload


def detect_audio() -> dict:
    payload = {
        "volume_percent": -1,
    }

    mock_volume = os.environ.get("PIXELPAL_MOCK_VOLUME_PERCENT")
    if mock_volume is not None:
        try:
            payload["volume_percent"] = int(mock_volume)
        except ValueError:
            pass
        return payload

    if shutil.which("amixer") is None:
        return payload

    try:
        completed = subprocess.run(
            ["amixer", "get", "Master"],
            capture_output=True,
            text=True,
            check=False,
        )
        for line in completed.stdout.splitlines():
            if "%" in line and "[" in line:
                start = line.find("[")
                end = line.find("%", start)
                if start != -1 and end != -1:
                    payload["volume_percent"] = int(line[start + 1 : end])
                    break
    except (OSError, ValueError):
        pass

    return payload


def maybe_shutdown_for_critical_battery(power: dict) -> None:
    if power.get("level") != "critical" or power.get("charging"):
        return

    if os.environ.get("PIXELPAL_DISABLE_CRITICAL_SHUTDOWN", "0") == "1":
        return

    if shutil.which("systemctl") is None:
        return

    subprocess.run(["systemctl", "poweroff"], check=False)


def main() -> None:
    while True:
        power = detect_power()
        wifi = detect_wifi()
        audio = detect_audio()

        write_json("power.json", power)
        write_json("wifi.json", wifi)
        write_json("audio.json", audio)

        maybe_shutdown_for_critical_battery(power)
        time.sleep(POLL_SECONDS)


if __name__ == "__main__":
    main()

