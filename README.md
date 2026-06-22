# GodotLuau

**Roblox-style system for Godot 4** — a C++ GDExtension that embeds the [Luau](https://luau.org/) interpreter inside Godot and mirrors the Roblox architecture: `game`, services (`Workspace`, `Players`, `Lighting`, `ReplicatedStorage`…), `LocalScript` / `ServerScript` / `ModuleScript`, `Humanoid`, the `task` library, `RunService.Heartbeat`, Roblox-style GUI, constraints, BodyMovers and much more. You write your game in **Luau**, just like in Roblox Studio, but it runs on the Godot engine.

**Author:** PimpoliDev · **Repo:** https://github.com/Pimpoli/GodotLuau · **Version:** v1.7.0

---

## Table of contents

- [What is GodotLuau?](#what-is-godotluau)
- [Features](#features)
- [Getting started](#getting-started)
- [How it works (architecture)](#how-it-works-architecture)
  - [The embedded Luau interpreter](#1-the-embedded-luau-interpreter)
  - [Script types and how they run](#2-script-types-and-how-they-run)
  - [The Roblox-style service tree](#3-the-roblox-style-service-tree)
  - [The available Luau API](#4-the-available-luau-api)
  - [Lifetime safety layer](#5-lifetime-safety-layer)
  - [Persistent script IDs and trash](#6-persistent-script-ids-and-trash)
- [Node and class catalog](#node-and-class-catalog)
- [Player systems in Luau](#player-systems-in-luau)
- [Roblox-style lighting](#roblox-style-lighting)
- [Autocomplete and AI](#autocomplete-and-ai)
- [Configuration panel](#configuration-panel)
- [Building from source](#building-from-source)
- [Repository structure](#repository-structure)
- [Included tools](#included-tools)
- [Known limitations](#known-limitations)
- [Publishing a release (maintainer)](#publishing-a-release-maintainer)
- [License and credits](#license-and-credits)

---

## What is GodotLuau?

GodotLuau makes Godot 4 speak **Luau** (the Lua dialect used by Roblox) and brings along the hierarchy, nodes and API any Roblox developer already knows. The idea is that you can write game logic with a Roblox mindset —`game.Players.PlayerAdded`, `workspace.Part.Touched`, `RemoteEvent:FireServer()`, `task.wait()`— while taking advantage of Godot's engine, rendering and export pipeline.

It is **not** a Roblox emulator and it does not connect to Roblox servers: it is an **independent reimplementation** of its most common API on top of open technology (Godot + Luau, both open source).

## Features

- **Real Roblox API in Luau**, interpreted by the official Luau engine compiled inside the extension.
- **Automatic service hierarchy:** opening a `RobloxGame` scene builds the whole structure for you (`Workspace`, `Players`, `Lighting`, `StarterPlayer/StarterPlayerScripts`, etc.).
- **Three script types** with Roblox behavior: `LocalScript` (client), `ServerScript` (server) and `ModuleScript` (`require`).
- **Scene-aware autocomplete** (like Roblox Studio), hover documentation and syntax-error highlighting, right in the Godot editor.
- **Optional AI autocomplete** powered by custom n-gram models (the *LuauIA* family).
- **Roblox-faithful lighting** with `ClockTime`, day/night cycle, presets and configurable graphics quality.
- **Ready-made, editable player systems** in Luau (movement, camera, sprint, stamina, health regeneration, anti-exploit).
- **Memory-safety layer** that prevents the classic crashes when objects are destroyed at runtime.
- **Built-in auto-updater** that downloads and installs new versions from GitHub with SHA-256 verification.
- **Multilingual** UI (English / Español / Português), English by default.

## Getting started

> **Requirements:** Godot **4.3 or newer** · Windows x86_64 (the precompiled DLLs included are Windows-only; for other platforms see [Building from source](#building-from-source)).

The easiest way to create a new game is the wizard:

```bash
python tools/nuevo_proyecto.py
```

It asks for a name, a mode (3D or 2D) and a folder, and generates a complete Godot project ready to open, with:

- The compiled extension (`bin/`) and its `godot_luau.gdextension` file.
- The Roblox-style icons so nodes look like they do in Studio.
- The **GodotLuau Config** addon (settings panel + auto-updater) already enabled.
- A root `RobloxGame` scene that **auto-generates the entire service hierarchy when opened**.

When you open the project you'll see the familiar structure (`Workspace`, `Players`, `StarterPlayer/StarterPlayerScripts` with `PlayerModule`, `ControlModule`, `CameraModule`…) and you can edit the `.lua` files directly in Godot. Press **F5** to play.

## How it works (architecture)

GodotLuau is a **GDExtension** (a native C++ library that Godot loads at startup). Internally it is organized as a set of headers included by two translation units; all the code lives under `src/`. Here is what happens, step by step.

### 1. The embedded Luau interpreter

On startup the extension does two things (in `src/core/register_types.cpp`):

1. **Registers Luau as a scripting language** inside Godot (`Engine::register_script_language`), along with a **`.lua` file loader and saver** (`LuauLoader` / `LuauSaver`). That's why Godot can open, edit and save `.lua` files natively.
2. **Registers all the classes** (nodes and services) in the Roblox style, each with its icon.

The interpreter is the **official Luau** (from luau-lang) compiled into the DLL itself: the extension bundles the Luau VM, compiler and AST.

### 2. Script types and how they run

There are three script nodes, with the same meaning as in Roblox:

| Node | Color | Context | When it runs |
|---|---|---|---|
| `LocalScript` | Blue | **Client** (`LocalPlayer`, `UserInputService` available) | On its own, when the game starts |
| `ServerScript` | Orange | **Server** (`UserInputService` blocked, `LocalPlayer` is nil) | On its own, when the game starts |
| `ModuleScript` | Purple | Neutral | **Does not run on its own**: loaded via `require()` and returns its value |

When the game starts (not in the editor), each `LocalScript` and `ServerScript` **creates its own Luau virtual machine** (`luaL_newstate` + standard libraries). The body of the `.lua` file **runs top to bottom directly**, Roblox-style — there are **no** `_ready` / `_process` callbacks like in GDScript. For continuous logic you use `RunService.Heartbeat:Connect(...)` or loops with `task.wait()`.

A `ModuleScript` does not run by itself: when another script calls `require(...)`, its code runs once and the value it returns (typically a table of functions) is returned and cached.

### 3. The Roblox-style service tree

The root node (`RobloxGame3D` or `RobloxGame2D`) **auto-builds** the full service hierarchy the first time the scene is opened, so `game` and its services exist exactly as you expect:

```
game (RobloxGame3D)
├── Workspace            (3D world; contains SpawnLocation and a hidden RunService)
├── Players
├── Lighting
├── ReplicatedFirst
├── ReplicatedStorage
├── ServerStorage
├── ServerScriptService
├── StarterGui
├── StarterPack
├── StarterPlayer
│   ├── StarterPlayerScripts   (PlayerModule, ControlModule, CameraModule…)
│   └── StarterCharacterScripts
├── Teams
└── SoundService
```

From Luau you access them with `game:GetService("Players")` or directly (`game.Workspace`, `workspace`, etc.), and you navigate the tree with `FindFirstChild`, `WaitForChild`, `GetChildren` or by name (`workspace.MyPart`).

### 4. The available Luau API

Inside a script you get the usual Roblox environment:

- **Globals:** `game`, `workspace`, `script`, `require`, `shared`, plus the Luau standard libraries (`math`, `string`, `table`, `coroutine`, `os`, `utf8`, `bit32`…).
- **Output:** `print(...)`, `warn(...)`.
- **Scheduling:** `task.spawn`, `task.delay`, `task.defer`, `task.wait`, `task.cancel`, and the classic globals `wait`, `spawn`, `delay`, `tick`, `time`. Threads (coroutines) are managed by the node, so `task.wait()` actually pauses and resumes at the right time.
- **Data types** with their operators and correct `typeof`: `Vector3`, `Vector2`, `CFrame`, `Color3`, `UDim`, `UDim2`, `BrickColor`, plus `Enum` and `Instance`.
- **Roblox-style signals:** `:Connect()`, `:Disconnect()`, `.Connected`, `:Wait()` on everything that emits events (`RunService.Heartbeat`, `Humanoid` events, `ClickDetector`, `ProximityPrompt`, `UserInputService`, GUI, etc.).
- **Communication:** `RemoteEvent` (`FireServer`/`FireClient`/`OnServerEvent`/`OnClientEvent`), `RemoteFunction` and `BindableEvent`.
- **Class identity:** `Instance.ClassName` and `:IsA(...)` faithful to the Roblox hierarchy.

> The script body runs with a Roblox mindset. The raw Godot API (`Node.new`, `:add_child`, `:queue_free`, `get_tree()`…) is **not** exposed: you always work the Roblox way.

### 5. Lifetime safety layer

Mixing a garbage-collected language (Luau) with engine objects that can be destroyed at any time (Godot nodes) is the classic recipe for *use-after-free* (accessing freed memory → crash). GodotLuau avoids this with a dedicated layer (`src/core/gl_runtime.h`):

- Luau objects **do not hold raw pointers** to nodes; they hold their **`ObjectID`**. When used, the ID is resolved against the live object database: if the node no longer exists you get `nil` instead of a crash.
- There is a **registry of live Luau VMs**: before invoking any callback it checks that its VM is still active, so code is never run on an already-closed interpreter.

### 6. Persistent script IDs and trash

Every `LocalScript` / `ServerScript` / `ModuleScript` gets an **immutable `script_id`** (e.g. `ServerScript_ID_3`) that names its `.lua` file. This enables a convenient, safe workflow:

- When you **delete the node**, its `.lua` is not lost: it is moved to the `res://.luau_trash/` folder.
- **Ctrl+Z** brings back both the node and its script at once.
- The trash **auto-purges after 7 days**.

## Node and class catalog

All these classes are registered in the editor with their Roblox-style icon (see `src/core/register_types.cpp`):

- **Scripts:** `LocalScript`, `ServerScript`, `ModuleScript`.
- **Game roots:** `RobloxGame3D`, `RobloxGame2D`, `RobloxTemplate`.
- **3D characters and physics:** `Humanoid`, `RobloxWorkspace`, `RobloxPlayer`, `RobloxPart`.
- **2D characters and physics:** `Humanoid2D`, `RobloxWorkspace2D`, `RobloxPlayer2D`.
- **Services:** `Players`, `Lighting`, `MaterialService`, `ReplicatedStorage`, `ReplicatedFirst`, `ServerStorage`, `ServerScriptService`, `StarterPlayer`, `StarterPlayerScripts`, `StarterCharacterScripts`, `StarterGui`, `StarterPack`, `Teams`, `SoundService`, `RunService`, `TextChatService`, `NetworkClient`, `CollectionService`, `UserInputService`, `TweenService`, `Folder`.
- **Communication:** `RemoteEventNode`, `RemoteFunctionNode`, `BindableEventNode`.
- **BodyMovers:** `BodyVelocity`, `BodyPosition`, `BodyForce`, `BodyAngularVelocity`, `BodyGyro`.
- **Constraints:** `WeldConstraint`, `HingeConstraint`, `BallSocketConstraint`, `RodConstraint`, `SpringConstraint`, `Motor6D`.
- **GUI:** `ScreenGui`, `RobloxFrame`, `RobloxTextLabel`, `RobloxTextButton`, `RobloxTextBox`, `RobloxImageLabel`, `RobloxScrollingFrame`, `BillboardGui`, `SurfaceGui`.
- **Lighting / effects:** `AtmosphereNode`, `LightingSkyNode`, `SunRaysNode`, `BloomEffect`, `BlurEffect`, `ColorCorrectionEffect`, `DepthOfFieldEffect`.
- **Interaction / tools:** `ClickDetector`, `ProximityPrompt`, `SpawnLocation`, `RobloxTool`, `Backpack`.
- **Sound:** `RobloxSound`, `RobloxSoundGroup`.
- **Animation:** `AnimationTrack`, `AnimationObject`.
- **Chat:** `RobloxChat`.

## Player systems in Luau

New projects include player controllers **written in Luau and fully editable** (`DefaultScripts/PlayerController3D.lua` and `PlayerController2D.lua`), meant for you to open and tweak:

- Movement and camera (fixed or smoothed mode).
- **Sprint** with smooth acceleration and optional **stamina**.
- Per-platform modules (PC / mobile / console).
- **Health** with Roblox-faithful regeneration (resets when taking damage).
- A `GameManager` with `PlayerAdded` and anti-exploit validation.

## Roblox-style lighting

The `Lighting` service mirrors Roblox behavior:

- `ClockTime`, `TimeOfDay` (`"18:30:00"`), `Brightness`, `Ambient` / `OutdoorAmbient`, fog, `ColorShift_Top/Bottom`.
- `SetMinutesAfterMidnight()` / `GetMinutesAfterMidnight()`.
- **Automatic day/night cycle** (`DayNightCycle` + `DayLengthMinutes`).
- **Presets** (Realistic, Cartoon, Anime, Sunset, Night…).
- `Technology` adjusts Godot's real graphics quality: *Compatibility/Legacy* (performance), *ShadowMap* (balanced), *Future* (SSAO + SSIL + Glow), *Voxel* (global illumination).

## Autocomplete and AI

The script editor offers **scene-aware** suggestions (like Roblox Studio): typing `workspace.` or `FindFirstChild("` shows the *real* children of the scene with their class, and properties are resolved by each node's actual class. The type base comes from the Roblox API definitions (`Scripts/globalTypes.d.luau`, `Scripts/DataTypes.json`).

In addition, there is an optional **AI autocomplete**: a family of custom n-gram models called **LuauIA** (levels *Mini / Medium / High / HighPRO*, from fastest to smartest). They are trained with `tools/entrenar_modelo.py` from the corpus in `corpus/` and can be downloaded from the panel.

## Configuration panel

At the bottom of the editor you'll find **GodotLuau Config** (English / Español / Português, **English by default**), organized into zones: **Updates**, **AI & Autocomplete**, **Data**, **Debug** and **Appearance** (with a 0–100 text-size slider).

- **Script output (`print`/`warn`):** on by default; turn it off for a clean Output.
- **Instant autocomplete** and **custom autocomplete** from a local JSON or a URL.
- **Debug mode:** shows internal output from system scripts (PlayerModule, Health, Chat…) and engine messages.
- **Updates:** checks the version on GitHub, downloads the new DLL (verifying its **SHA-256**) and restarts the editor.

## Building from source

You only need to build if you want to modify the extension or produce binaries for Linux/macOS.

The `godot-cpp` and `luau` dependencies are **git submodules** pinned to exact commits. A `.gitmodules` is already provided, so initializing them is enough:

```bash
git clone https://github.com/Pimpoli/GodotLuau.git
cd GodotLuau
git submodule update --init --recursive
```

> The submodules are pinned to `godot-cpp` at commit `d5cc777…` (branch 4.3) and `luau` at `9e2984f…` (release/712), which are the compatible ones.

**Build requirements:** Python 3, [SCons](https://scons.org/) (`pip install scons`) and a C++ compiler (MSVC on Windows; GCC/Clang on Linux/macOS).

```bash
scons platform=windows target=template_debug
scons platform=windows target=template_release
```

The libraries land in `bin/`. On Linux/macOS use `platform=linux` / `platform=macos` and add the matching entries (`linux.debug.x86_64`, etc.) in `godot_luau.gdextension`.

> The `SConstruct` re-enables C++ exceptions (`-fexceptions` / `/EHsc`), because godot-cpp disables them by default and the Luau VM needs them to compile. You don't have to pass anything extra: it's already handled.

## Repository structure

```
src/                       Extension C++ code, organized by area:
  core/                      Luau VM integration, language, registration, runtime safety
  editor/                    Autocomplete + type database (editor-only)
  services/                  game, services, workspace, folders
  characters/                players, humanoids, parts, physics, body movers, constraints
  gameplay/                  remotes, input, interaction, animation, tween, sound
  ui/                        GUI, billboards, chat, lighting effects
addons/GodotLuauUpdater/   Settings panel + auto-updater (GDScript)
assets/                    Project assets (avatar models, branding)
tools/                     Python helper scripts
DefaultScripts/            Example player controllers (Luau)
Scripts/                   Type definitions for autocomplete
corpus/                    Training corpus for the AI models
models/                    LuauIA AI models (downloaded by the updater)
icons/                     Roblox-style SVG icons
bin/                       Compiled libraries (DLL)
godot-cpp/, luau/          Dependencies (submodules; see above)
```

## Included tools

| Script | What it does |
|---|---|
| `tools/nuevo_proyecto.py` | Creates a ready-to-use new game project (3D or 2D) |
| `tools/crear_plantilla.py` | Packages `RobloxTemplate.zip`, importable from Godot's Project Manager |
| `tools/generar_release.py` | Regenerates `GodotLuau.zip` + `GodotLuau.zip.sha256` for the auto-updater |
| `tools/entrenar_modelo.py` | Trains the `models/LuauIA-*.json` family (the AI autocomplete models) |

> Run the tools from the repository root, e.g. `python tools/nuevo_proyecto.py`.

## Known limitations

GodotLuau implements the *experience* of programming like in Roblox, but it is not Roblox. Keep in mind:

- **No real network replication (yet):** `RemoteEvent`/`RemoteFunction`/`BindableEvent` work within a **single machine** (client and server live in the same process). `Players:GetPlayers()` returns a single player.
- **Each script has its own VM:** a `ModuleScript` required by two different scripts **does not share memory** between them (each script loads it in its own interpreter).
- **`Destroy`** removes the node from the tree immediately, but the C++ object may not be freed right away for these extension-created nodes (not a crash or a visible leak).
- **Platforms:** the included DLLs are **Windows x86_64** only. For other platforms you have to build (see above).

## Publishing a release (maintainer)

1. Rebuild both DLLs (debug and release).
2. Update the `Version` file (e.g. `v1.7.0`) and the version in `addons/GodotLuauUpdater/plugin.cfg`. The banner reads the version from `res://Version` at runtime, so no C++ change is needed.
3. Regenerate the updater package and its hash:
   ```bash
   python tools/generar_release.py
   ```
4. Commit and push `Version`, `GodotLuau.zip` **and** `GodotLuau.zip.sha256` together.

The users' auto-updater compares `Version`, downloads the ZIP and verifies the SHA-256 before installing.

## License and credits

- **GodotLuau** — © PimpoliDev. *(The repository does not yet include a `LICENSE` file; if you plan to distribute or contribute, adding one is recommended to make the terms of use clear.)*
- **[Luau](https://github.com/luau-lang/luau)** — the embedded interpreter, MIT license (Roblox Corporation).
- **[godot-cpp](https://github.com/godotengine/godot-cpp)** — the C++ bindings for Godot, MIT license.
- **[Godot Engine](https://godotengine.org/)** — the engine everything runs on, MIT license.
