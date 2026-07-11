# GodotLuau

**Roblox-style system for Godot 4** — a C++ GDExtension that embeds the [Luau](https://luau.org/) interpreter inside Godot and mirrors the Roblox architecture: `game`, services (`Workspace`, `Players`, `Lighting`, `ReplicatedStorage`…), `LocalScript` / `ServerScript` / `ModuleScript`, `Humanoid`, the R6 avatar with its classic animations, local multiplayer with a Server View, the `task` library, `RunService.Heartbeat`, Roblox-style GUI, in-game settings menu, constraints, BodyMovers and much more. You write your game in **Luau**, just like in Roblox Studio, but it runs on the Godot engine.

**Author:** PimpoliDev · **Repo:** https://github.com/Pimpoli/GodotLuau · **License:** MIT

> **Disclaimer:** GodotLuau is an independent project. It is **not** affiliated with, endorsed by, or sponsored by Roblox Corporation or the Godot Foundation. "Roblox" is a trademark of Roblox Corporation and is referenced here only to describe API compatibility. GodotLuau does not connect to Roblox servers and does not use Roblox assets.

---

## Table of contents

- [What is GodotLuau?](#what-is-godotluau)
- [Features](#features)
- [Getting started](#getting-started)
- [How it works (architecture)](#how-it-works-architecture)
- [The R6 avatar and StarterCharacter](#the-r6-avatar-and-startercharacter)
- [Local multiplayer](#local-multiplayer)
- [Roblox-style error messages](#roblox-style-error-messages)
- [In-game settings menu](#in-game-settings-menu)
- [Node and class catalog](#node-and-class-catalog)
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
- **Three script types** with Roblox behavior: `LocalScript` (client), `ServerScript` (server) and `ModuleScript` (`require`), each with an **`Enabled`** property (True/False) like in Studio.
- **Roblox-exact error messages:** runtime errors print the red error line plus the blue `Stack Begin / Script '…', Line N / Stack End` block; indexing a missing member raises `X is not a valid member of …`; a real `WaitForChild` that suspends the coroutine, warns `Infinite yield possible on '…'` after 5 seconds, and supports timeouts; `require` reproduces Roblox's exact messages.
- **R6 avatar** ("classic" blocky rig) with code-driven classic animations (walk pendulum, idle sway, jump arms-up), plus **StarterCharacter** support: drop a model named `StarterCharacter` under `StarterPlayer` and it becomes everyone's character.
- **Local multiplayer like Studio's "Local Server":** launch N player windows plus a free-camera **Server View** window, with real-time replicated positions, animations and a networked chat.
- **In-game settings menu** (Esc), Roblox-style: tabs, players list, ping, graphics quality, max FPS, FPS/ping overlays, volume, camera sensitivity — saved per player.
- **Luau syntax colors** in the script editor with the Roblox Studio palette, scene-aware autocomplete, hover documentation and syntax-error highlighting.
- **Physics tuned for Roblox-scale worlds:** Jolt Physics body limits are raised automatically (Godot's default 10,240 bodies is far too low for voxel/tycoon worlds) and `Touched` contact monitoring activates per-part only when a script actually connects it.
- **Roblox-faithful lighting** with `ClockTime`, day/night cycle, presets and configurable graphics quality; editable environment file in `GodotLuau/shaders/`.
- **Ready-made, editable player systems** in Luau (movement, camera, sprint, stamina, health regeneration, anti-exploit).
- **Memory-safety layer** that prevents the classic crashes when objects are destroyed at runtime.
- **Built-in auto-updater** that downloads and installs new versions from GitHub with SHA-256 verification.
- **Multilingual** UI (English / Español / Português), English by default.

## Getting started

> **Requirements:** Godot **4.3 or newer** · **Windows x86_64** or **Linux x86_64** (precompiled binaries for both are included in the release ZIP; for other platforms see [Building from source](#building-from-source)).

The easiest way to create a new game is the wizard:

```bash
python tools/nuevo_proyecto.py
```

It asks for a name, a mode (3D or 2D) and a folder, and generates a complete Godot project ready to open, with:

- The compiled extension (`GodotLuau/bin/`) and its `godot_luau.gdextension` file.
- The Roblox-style icons so nodes look like they do in Studio.
- The **GodotLuau Config** addon (settings panel + auto-updater) already enabled.
- A root `RobloxGame` scene that **auto-generates the entire service hierarchy when opened**.

When you open the project you'll see the familiar structure (`Workspace` with its tintable grid Baseplate, `Players`, `StarterPlayer/StarterPlayerScripts`…) and you can edit the `.lua` files directly in Godot with Roblox-style syntax colors. Press **F5** to play.

## How it works (architecture)

GodotLuau is a **GDExtension** (a native C++ library that Godot loads at startup). All the code lives under `src/`, organized by area and included by two translation units.

### 1. The embedded Luau interpreter

On startup the extension (in `src/core/register_types.cpp`):

1. **Registers Luau as a scripting language** inside Godot, along with a **`.lua` file loader and saver**. That's why Godot can open, edit and save `.lua` files natively.
2. **Registers all the classes** (nodes and services) in the Roblox style, each with its icon.
3. **Raises the Jolt Physics limits** so Roblox-scale worlds (tens of thousands of parts) keep their collisions.

The interpreter is the **official Luau** (from luau-lang) compiled into the binary itself: the extension bundles the Luau VM, compiler and AST.

### 2. Script types and how they run

| Node | Color | Context | When it runs |
|---|---|---|---|
| `LocalScript` | Blue | **Client** (`UserInputService` available) | On its own, when the game starts |
| `ServerScript` | Orange | **Server** (`UserInputService` blocked) | On its own, when the game starts |
| `ModuleScript` | Purple | Neutral | **Does not run on its own**: loaded via `require()` and returns its value |

When the game starts, each `LocalScript` and `ServerScript` **creates its own Luau virtual machine**. The body of the `.lua` file **runs top to bottom directly**, Roblox-style — there are **no** `_ready` / `_process` callbacks like in GDScript. For continuous logic you use `RunService.Heartbeat:Connect(...)` or loops with `task.wait()`. Scripts have an **`Enabled`** property: disabled scripts don't run, and re-enabling one at runtime runs it then.

### 3. The Roblox-style service tree

The root node (`RobloxGame3D` or `RobloxGame2D`) **auto-builds** the full service hierarchy the first time the scene is opened:

```
game (RobloxGame3D)
├── Workspace            (3D world; Baseplate with tintable Studio grid)
├── Players
├── Lighting             (Sky, Atmosphere, Bloom, Blur, DepthOfField, SunRays…)
├── ReplicatedFirst
├── ReplicatedStorage
├── ServerStorage
├── ServerScriptService
├── StarterGui
├── StarterPack
├── StarterPlayer
│   ├── StarterPlayerScripts
│   └── StarterCharacterScripts
├── Teams
└── SoundService
```

From Luau you access them with `game:GetService("Players")` or directly (`game.Workspace`, `workspace`), and you navigate the tree with `FindFirstChild`, `WaitForChild`, `GetChildren` or by name (`workspace.MyPart`).

### 4. The available Luau API

- **Globals:** `game`, `workspace`, `script`, `require`, `shared`, plus the Luau standard libraries (`math`, `string`, `table`, `coroutine`, `os`, `utf8`, `bit32`…).
- **Output:** `print(...)`, `warn(...)`, with Roblox-style error reporting (see below).
- **Scheduling:** `task.spawn`, `task.delay`, `task.defer`, `task.wait`, `task.cancel`, and the classic globals `wait`, `spawn`, `delay`, `tick`, `time`. Threads are managed per node, so `task.wait()` actually pauses and resumes at the right time.
- **Data types** with their operators and correct `typeof`: `Vector3`, `Vector2`, `CFrame`, `Color3`, `UDim`, `UDim2`, `BrickColor`, plus `Enum` (including `Enum.Material`) and `Instance`.
- **Roblox-style signals:** `:Connect()`, `:Disconnect()`, `.Connected`, `:Wait()` on everything that emits events.
- **Communication:** `RemoteEvent`, `RemoteFunction` and `BindableEvent`.
- **Class identity:** `Instance.ClassName` and `:IsA(...)` faithful to the Roblox hierarchy.

### 5. Lifetime safety layer

Luau objects **do not hold raw pointers** to nodes; they hold their **`ObjectID`**, resolved against the live object database on each use — if the node is gone you get `nil`, not a crash. A **registry of live Luau VMs** guarantees callbacks never run on a closed interpreter (`src/core/gl_runtime.h`).

### 6. Persistent script IDs and trash

Every script node gets an **immutable `script_id`** (e.g. `ServerScript_ID_3`) that names its `.lua` file under `res://GodotLuau/<Class>s/`. Deleting the node (or any ancestor) moves the affected `.lua` files to `res://.luau_trash/`; **Ctrl+Z** restores node and script together; the trash auto-purges after 7 days.

## The R6 avatar and StarterCharacter

By default every player spawns as the **R6 rig**: six body parts hung from pivots placed at the classic joint positions, so the code-driven animations (walk pendulum with opposite arms/legs, idle breathing, airborne arms-up) rotate exactly like the classic avatar. Scripts can read the parts by their Roblox names (`"Left Arm"`, `"Torso"`, `"HumanoidRootPart"`…).

To use your own character, place any model named **`StarterCharacter`** under `StarterPlayer`: it is cloned as the character for every player, exactly like in Roblox.

The camera-proximity fade uses a **dissolve shader** (pixels are discarded, not alpha-blended) that only kicks in when the camera gets close, and right-clicking to rotate the camera restores the mouse to its original position — the details that make movement feel like Roblox.

## Local multiplayer

The editor toolbar has a **player-count selector (1–8) and a device selector** (PC / Mobile / Console / VR preview). Press Play and GodotLuau launches N player windows **plus a Server View window** — a free camera over the world, with a toggle in the Game panel to switch between server and client perspective. Positions, rotations and animation states replicate in real time, and the chat is networked across all windows. Windows are tiled and titled automatically (`Player1`, `Player2`, … `Server View`).

## Roblox-style error messages

Script failures print **exactly** what Roblox would print:

```
ServerScriptService.Server:5: attempt to index nil with 'Name'
Stack Begin
Script 'ServerScriptService.Server', Line 5 - function doStuff
Stack End
```

- Indexing a member that doesn't exist raises `NoExiste is not a valid member of Workspace "Workspace"` (use `FindFirstChild` to probe safely, like in Roblox).
- `WaitForChild` really suspends the coroutine until the child appears; with no timeout it warns **`Infinite yield possible on 'Parent:WaitForChild("Name")'`** (with stack) after 5 seconds; with a timeout it returns `nil` when it expires.
- `require` reproduces Roblox's messages: `Attempted to call require with invalid argument(s).`, `Module code did not return exactly one value`, `Requested module experienced an error while loading` — and the module's own error is reported with its inner stack.
- Every callback path (signals, remotes, `RunService`, tweens, GUI, `Touched`, input…) reports errors this way; nothing dies silently.

## In-game settings menu

Press **Esc** (or the mobile button / gamepad Start) for the Roblox-style menu: **Players** list with ping, **Settings** (graphics quality, max FPS, Show FPS / Show Ping overlays, volume, camera sensitivity) and **Help**. Settings persist **per player** (`user://gl_settings_player<N>.cfg`), and leaving asks for confirmation.

## Node and class catalog

All these classes are registered in the editor with their Roblox-style icon:

- **Scripts:** `LocalScript`, `ServerScript`, `ModuleScript`.
- **Game roots:** `RobloxGame3D`, `RobloxGame2D`, `RobloxTemplate`.
- **3D characters and physics:** `Humanoid`, `RobloxWorkspace`, `RobloxPlayer`, `RobloxPart`.
- **2D:** `Humanoid2D`, `RobloxWorkspace2D`, `RobloxPlayer2D`.
- **Services:** `Players`, `Lighting`, `MaterialService`, `ReplicatedStorage`, `ReplicatedFirst`, `ServerStorage`, `ServerScriptService`, `StarterPlayer`, `StarterPlayerScripts`, `StarterCharacterScripts`, `StarterGui`, `StarterPack`, `Teams`, `SoundService`, `RunService`, `TextChatService`, `NetworkClient`, `NetworkService`, `CollectionService`, `UserInputService`, `TweenService`, `Folder`.
- **Communication:** `RemoteEventNode`, `RemoteFunctionNode`, `BindableEventNode`.
- **BodyMovers:** `BodyVelocity`, `BodyPosition`, `BodyForce`, `BodyAngularVelocity`, `BodyGyro`.
- **Constraints:** `WeldConstraint`, `HingeConstraint`, `BallSocketConstraint`, `RodConstraint`, `SpringConstraint`, `Motor6D`, `Attachment`.
- **GUI:** `ScreenGui`, `RobloxFrame`, `RobloxTextLabel`, `RobloxTextButton`, `RobloxTextBox`, `RobloxImageLabel`, `RobloxScrollingFrame`, `BillboardGui`, `SurfaceGui`, plus UI helpers (`UICorner`, `UIListLayout`, `UIStroke`, `UIGradient`…).
- **Lighting / effects:** `AtmosphereNode`, `LightingSkyNode`, `SunRaysNode`, `BloomEffect`, `BlurEffect`, `ColorCorrectionEffect`, `DepthOfFieldEffect`, `ParticleEmitter`, `Beam`, `Trail`, `Highlight`, `Smoke`, `Fire`, `Explosion`…
- **Interaction / tools:** `ClickDetector`, `ProximityPrompt`, `SpawnLocation`, `RobloxTool`, `Backpack`, `Seat`, `VehicleSeat`.
- **Sound:** `RobloxSound`, `RobloxSoundGroup`, sound effect nodes.
- **Animation:** `AnimationTrack`, `AnimationObject`, `GLR6Animator`.
- **Chat:** `RobloxChat`.

## Roblox-style lighting

- `ClockTime`, `TimeOfDay` (`"18:30:00"`), `Brightness`, `Ambient` / `OutdoorAmbient`, fog, `ColorShift_Top/Bottom`.
- `SetMinutesAfterMidnight()` / `GetMinutesAfterMidnight()`, **automatic day/night cycle**.
- **Presets** (Realistic, Cartoon, Anime, Sunset, Night…).
- `Technology` adjusts Godot's real graphics quality: *Compatibility/Legacy*, *ShadowMap*, *Future* (SSAO + SSIL + Glow), *Voxel* (GI).
- The environment lives in **`GodotLuau/shaders/environment_roblox.tres`** — edit it to customize the look of every scene.

## Autocomplete and AI

The script editor offers **scene-aware** suggestions (like Roblox Studio): typing `workspace.` or `FindFirstChild("` shows the *real* children of the scene with their class. The type base comes from the Roblox API definitions (`Scripts/globalTypes.d.luau`, `Scripts/DataTypes.json`). Luau code is colored with the **Roblox Studio palette** (pink keywords, blue builtins, green strings, yellow numbers).

There is also an optional **AI autocomplete**: a family of custom n-gram models called **LuauIA** (levels *Mini / Medium / High / HighPRO*), trained with `tools/entrenar_modelo.py` from the corpus in `corpus/`.

## Configuration panel

At the bottom of the editor you'll find **GodotLuau Config** (English / Español / Português), organized into zones: **Updates**, **AI & Autocomplete**, **Data**, **Debug** and **Appearance**.

- **Script output (`print`/`warn`):** on by default.
- **Debug mode:** shows internal output from system scripts and engine diagnostics (multiplayer roles, TX/RX…).
- **Updates:** checks the version on GitHub, downloads the new package (verifying its **SHA-256**) and restarts the editor.

## Building from source

The `godot-cpp` and `luau` dependencies are **git submodules** pinned to exact commits:

```bash
git clone https://github.com/Pimpoli/GodotLuau.git
cd GodotLuau
git submodule update --init --recursive
```

**Build requirements:** Python 3, [SCons](https://scons.org/) (`pip install scons`) and a C++ compiler (MinGW/MSVC on Windows; GCC/Clang on Linux).

```bash
scons platform=windows target=template_debug
scons platform=windows target=template_release
# or: scons platform=linux target=template_debug / template_release
```

The libraries land in `GodotLuau/bin/`. Linux binaries are also built automatically by the GitHub Actions workflow (`.github/workflows/build-linux.yml`) on every push that touches `src/`.

> The `SConstruct` re-enables C++ exceptions (`-fexceptions` / `/EHsc`), because godot-cpp disables them by default and the Luau VM needs them to compile. You don't have to pass anything extra.

## Repository structure

```
src/                       Extension C++ code, organized by area:
  core/                      Luau VM integration, language, registration, error reporting
  editor/                    Autocomplete + type database (editor-only)
  services/                  game, services, workspace, network (multiplayer)
  characters/                players, humanoids, parts, avatar R6, body movers
  gameplay/                  remotes, input, interaction, animation, tween, sound
  ui/                        GUI, billboards, chat, lighting effects, settings menu
addons/GodotLuauUpdater/   Settings panel + auto-updater + editor UX (GDScript)
GodotLuau/                 Everything the extension ships:
  bin/                       Compiled libraries (.dll / .so)
  icons/                     Roblox-style SVG icons
  assets/                    Avatar R6 meshes, textures (baseplate grid)
  shaders/                   character_fade.gdshader, environment_roblox.tres
  DefaultScripts/            Example player controllers (Luau)
  licenses/                  MIT licenses of GodotLuau and embedded software
tools/                     Python helper scripts
Scripts/                   Type definitions for autocomplete
corpus/, models/           AI autocomplete corpus and models
godot-cpp/, luau/          Dependencies (submodules)
```

## Included tools

| Script | What it does |
|---|---|
| `tools/nuevo_proyecto.py` | Creates a ready-to-use new game project (3D or 2D) |
| `tools/crear_plantilla.py` | Packages `RobloxTemplate.zip`, importable from Godot's Project Manager |
| `tools/generar_release.py` | Regenerates `GodotLuau.zip` + `GodotLuau.zip.sha256` for the auto-updater |
| `tools/entrenar_modelo.py` | Trains the `models/LuauIA-*.json` family (AI autocomplete) |

## Known limitations

- **Multiplayer is local (same machine):** the N windows + Server View replicate over localhost. Internet servers (relay/matchmaking) are planned.
- **Each script has its own VM:** a `ModuleScript` required by two different scripts does not share memory between them.
- **Very large part counts** (tens of thousands) work but are not yet batched: automatic clustering/instancing of anchored parts is the next major feature.
- **Platforms:** precompiled binaries are **Windows x86_64** and **Linux x86_64**. macOS requires building from source.

## Publishing a release (maintainer)

1. Rebuild both Windows DLLs (debug and release); push `src/` changes so the Linux workflow builds the `.so` files, and place them in `GodotLuau/bin/`.
2. Update the `Version` file and the version in `addons/GodotLuauUpdater/plugin.cfg`.
3. Regenerate the updater package: `python tools/generar_release.py`
4. Commit and push `Version`, `GodotLuau.zip` **and** `GodotLuau.zip.sha256` together.

The users' auto-updater compares `Version`, downloads the ZIP and verifies the SHA-256 before installing.

## License and credits

- **GodotLuau** — MIT License, © 2026 PimpoliDev (see `LICENSE`).
- **[Luau](https://github.com/luau-lang/luau)** — the embedded interpreter, MIT license (Roblox Corporation).
- **[godot-cpp](https://github.com/godotengine/godot-cpp)** — the C++ bindings for Godot, MIT license.
- **[Godot Engine](https://godotengine.org/)** — the engine everything runs on, MIT license.

Copies of all bundled licenses ship inside the release package under `GodotLuau/licenses/`.
