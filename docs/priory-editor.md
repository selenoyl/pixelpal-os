# Priory Editor

`Priory Editor` is a Windows-first desktop tool for building Priory maps, reusable tile stamps, NPC placements, doorway warps, sprite sheets, monsters, and quest chains outside the handheld runtime.

## What it does

- paint Priory tiles directly onto a map canvas
- create, resize, fill, and delete whole areas
- place and retarget door or road warps, including selecting a destination area and numbered destination pathway
- place NPCs with role, facing, dialogue, and solidity
- place monsters with HP, attack, aggression, and facing
- author sprite assets with per-direction, per-frame pixel editing
- author quest records, dialogue, requirements, rewards, and quest givers
- capture a rectangle from the map into a reusable stamp bank
- save and load editor projects as JSON
- preview linked neighboring areas while editing the current map

## Windows workflow

Build:

```bat
tools\build-priory-editor-windows.cmd
```

Run:

```bat
tools\run-priory-editor-windows.cmd
```

Smoke test:

```bat
tools\smoke-test-priory-editor.cmd
```

## Controls

- left click: paint or place/select the active tool item
- right click: erase tile or delete the active item
- shift + drag: capture a stamp selection
- mouse wheel over the map: zoom
- `[` / `]`: zoom out / in
- arrow keys: pan the map
- `1`: paint tool
- `2`: warp tool
- `3`: NPC tool
- `4`: monster tool
- `5`: quest tool
- `6`: sprite tool
- `Ctrl+S`: save project
- `Ctrl+Z`: undo
- `Ctrl+L`: load project
- `F11`: toggle fullscreen
- `G`: toggle grid
- `Delete`: remove the selected item for the active tool

## Project format

Projects are stored as JSON and contain:

- project name
- areas with tile rows
- warps with bounds, target area, target pathway, target coordinates, and facing
- NPC definitions, dialogue, and sprite overrides
- monster definitions
- quest records, stage lists, requirements, rewards, and giver ids
- sprite assets with four directions and two frames
- reusable stamp tiles

The editor starts with a seeded `Saint Catherine Arrival` project and defaults to saving into:

`sample-games/priory/editor-projects/saint-catherine-arrival.json`
