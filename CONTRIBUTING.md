# Contributing

## Scope

This repository is a working game/device project, not a framework. Changes should preserve the current direction:

- lightweight handheld runtime
- controller-first UX
- one consistent game/runtime pattern
- Pi Zero 2 W constraints

## Build expectations

Windows development uses MSYS2, MinGW-w64, CMake, Ninja, and SDL2.

Typical commands:

```sh
cmake --preset windows-debug
cmake --build --preset windows-debug
```

Linux development uses the `linux-debug` preset.

## Repo hygiene

- do not commit `build/`, `dist/`, `out/`, or other generated output
- keep sample games launchable through the staged `games/` layout
- prefer simple SDL/native code over adding heavy frameworks
- keep launcher and SDK contracts consistent across games

## Game additions

New games should follow the existing package pattern:

- `manifest.toml`
- executable target
- icon/splash assets
- use `libpixelpal` for input, save/config paths, and clean exit behavior

## Public repo note

There is currently no selected repository license. Do not assume third-party reuse permissions until a license file is added.
