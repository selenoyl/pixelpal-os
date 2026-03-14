# PixelPal RPG Engine

`pixelpal_rpg_engine` is a new shared SDK module for building full top-down RPGs on PixelPal without hardcoding every map, quest, and actor directly into a game executable.

## Why it exists

The repo had two strong pieces already:

- a playable Priory runtime with bespoke systems
- a Priory editor with map, NPC, quest, sprite, and warp authoring

What it did not have was an engine contract between them. The new engine module is that contract.

## Design direction

This engine deliberately borrows a few ideas from established tools:

- Godot scenes and resources: reusable authored chunks instead of one giant monolithic world file
- Tiled layers, object layers, and custom properties: explicit map structure and metadata
- Unity prefabs and ScriptableObject-style data assets: authored templates plus per-instance overrides in scenes

The result is still lightweight and SDL/C++ native, but the authoring model is no longer tied to one hardcoded game file.

Official references that informed the shape of this module:

- [Godot scenes and nodes](https://docs.godotengine.org/en/stable/getting_started/step_by_step/nodes_and_scenes.html)
- [Godot resources](https://docs.godotengine.org/en/stable/tutorials/scripting/resources.html)
- [Tiled layers](https://doc.mapeditor.org/en/stable/manual/layers/)
- [Tiled objects](https://doc.mapeditor.org/en/stable/manual/objects/)
- [Tiled custom properties](https://doc.mapeditor.org/en/stable/manual/custom-properties/)
- [Unity prefabs](https://docs.unity3d.com/Manual/Prefabs.html)
- [Unity ScriptableObject](https://docs.unity3d.com/6000.1/Documentation/Manual/class-ScriptableObject.html)

## Core model

The engine project format now supports:

- project metadata and starting area/warp
- tile definitions with walkability, opacity, default layer, tags, and properties
- layered areas
  - `GROUND`
  - `WALL`
  - `FRINGE`
  - `OBJECT`
  - `TRIGGER`
- pathway warps with bounds and target area/warp
- scene entities with optional archetype links
- entity archetypes/prefabs for NPCs, monsters, props, chests, shops, and crops
- dialogue graphs with nodes and choices
- quest stages with requirements and completion actions
- items, shops, and crops
- sprite assets
- reusable pattern stamps

## Backward compatibility

The loader is intentionally able to read the current Priory editor JSON shape and upgrade it in memory:

- old `tiles` become a `GROUND` layer
- old `wall_tiles` become a `WALL` layer
- old `npcs` and `monsters` become scene entities
- old quest requirements and rewards are mapped into staged engine quests

That means existing authored Priory content can be moved forward instead of thrown away.

## CLI workflow

Build:

```bat
tools\build-pixelpal-engine-cli-windows.cmd
```

Smoke test:

```bat
tools\smoke-test-pixelpal-engine.cmd
```

Emit a fresh starter project:

```bat
tools\run-pixelpal-engine-cli.cmd --emit-starter build\starter-engine-project.json
```

Validate a project:

```bat
tools\run-pixelpal-engine-cli.cmd --validate path\to\project.json
```

Inspect counts and validation:

```bat
tools\run-pixelpal-engine-cli.cmd --inspect path\to\project.json
```

Upgrade a legacy Priory editor project into engine format:

```bat
tools\run-pixelpal-engine-cli.cmd --upgrade old-project.json upgraded-project.json
```

## Current repo role

Right now this engine module provides:

- shared data structures
- JSON load/save
- validation
- summary/inspection helpers
- a starter project generator

The next practical step is to make the Priory runtime load engine projects directly and then migrate the editor onto the shared SDK types instead of duplicating its own project schema in `main.cpp`.

## Intended engine roadmap

The shared engine is meant to become the backbone for more than Priory:

- runtime map loading instead of hardcoded world assembly
- editor save/load against the shared engine schema
- entity archetypes for NPCs, monsters, shops, crops, and décor
- dialogue and quest graphs shared between runtime and tooling
- prefab-like scene stamps for faster authoring
- renderer/editor/runtime consistency around layers, solidity, warps, and metadata

That keeps PixelPal moving toward a compact handheld-friendly RPG engine rather than a pile of disconnected one-off systems.
