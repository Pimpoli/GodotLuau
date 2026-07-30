# GodotLuau

**Write Roblox-style games in Luau, run them on Godot 4.**

GodotLuau is a C++ GDExtension that embeds the [Luau](https://luau.org/) interpreter inside Godot and reimplements the Roblox architecture on top of it: `game`, the services (`Workspace`, `Players`, `Lighting`, `ReplicatedStorage`…), `LocalScript` / `ServerScript` / `ModuleScript`, `Humanoid`, the R6 avatar, `RemoteEvent`s, the `task` library, Roblox-style GUI, and real networking (LAN, Internet and a Roblox-style world coordinator).

**Author:** PimpoliDev · **Repo:** https://github.com/Pimpoli/GodotLuau · **License:** MIT

### 🌐 Read this in Spanish → **[README - Espanol.md](README%20-%20Espanol.md)** (misma información, en español)

> **Disclaimer:** GodotLuau is an independent project. It is **not** affiliated with, endorsed by, or sponsored by Roblox Corporation or the Godot Foundation. "Roblox" is a trademark of Roblox Corporation and is referenced here only to describe API compatibility. GodotLuau does not connect to Roblox servers and ships no Roblox assets.

---

## Table of contents

- [What it is (and what it is not)](#what-it-is-and-what-it-is-not)
- [What works today](#what-works-today)
- [Install](#install)
- [Your first project, step by step](#your-first-project-step-by-step)
- [Writing scripts in Luau](#writing-scripts-in-luau)
- [Importing Roblox places (.rbxl)](#importing-roblox-places-rbxl)
- [Online mode / multiplayer](#online-mode--multiplayer)
  - [The four ways to play together](#the-four-ways-to-play-together)
  - [1. Local test from the editor](#1-local-test-from-the-editor)
  - [2. Dedicated server (LAN or Internet)](#2-dedicated-server-lan-or-internet)
  - [3. Coordinator + worlds (Roblox-style matchmaking)](#3-coordinator--worlds-roblox-style-matchmaking)
  - [4. Listen server from Luau](#4-listen-server-from-luau)
  - [Command-line reference](#command-line-reference)
  - [Networking API for Luau](#networking-api-for-luau)
  - [What replicates and what does not](#what-replicates-and-what-does-not)
  - [Files the network layer creates](#files-the-network-layer-creates)
  - [Security: read this before you publish](#security-read-this-before-you-publish)
  - [Troubleshooting a connection](#troubleshooting-a-connection)
  - [Optional backend (Nakama)](#optional-backend-nakama)
- [How it works (architecture)](#how-it-works-architecture)
- [Node and class catalog](#node-and-class-catalog)
- [Lighting](#lighting)
- [Autocomplete and AI](#autocomplete-and-ai)
- [Configuration panel](#configuration-panel)
- [Building from source](#building-from-source)
- [Repository structure](#repository-structure)
- [Included tools](#included-tools)
- [Known limitations](#known-limitations)
- [Publishing a release (maintainer)](#publishing-a-release-maintainer)
- [License and credits](#license-and-credits)

---

## What it is (and what it is not)

**It is** an independent reimplementation of the most common Roblox API on top of two open technologies (Godot + Luau). You write game logic the way you already do — `game.Players.PlayerAdded`, `workspace.Part.Touched`, `RemoteEvent:FireServer()`, `task.wait()` — and Godot handles rendering, physics, export and platforms.

**It is not:**

- a Roblox emulator or a Roblox client — it never contacts Roblox servers;
- a way to run your Roblox game "as is" — the API surface is large but not complete;
- a way to download Roblox assets — meshes, textures and sounds live on Roblox's servers and are not inside a `.rbxl` file.

## What works today

| Area | State |
|---|---|
| Luau interpreter (official Luau, compiled into the extension) | ✅ working |
| Service tree, `LocalScript` / `ServerScript` / `ModuleScript`, `require` | ✅ working |
| `task` library, `RunService.Heartbeat`, Roblox-style signals (`:Connect`, `:Wait`) | ✅ working |
| Data types (`Vector3`, `CFrame`, `Color3`, `UDim2`, `BrickColor`, `Enum`…) | ✅ working |
| Roblox-exact error messages and stack blocks | ✅ working |
| R6 avatar + `StarterCharacter`, `Humanoid`, constraints, BodyMovers | ✅ working |
| Roblox-style GUI, `BillboardGui`, `SurfaceGui`, in-game settings menu (Esc) | ✅ working |
| `.rbxl` place import (binary format) | ✅ working (assets by id are not downloadable) |
| Local multiplayer test (N windows + Server View) | ✅ working |
| Real networking: dedicated server, Direct Connect, networked `RemoteEvent`s, replication | ✅ working |
| Coordinator with automatic worlds (Roblox-style matchmaking) | ⚠️ working, single machine only — see [limits](#3-coordinator--worlds-roblox-style-matchmaking) |
| `HttpService` (real HTTP/HTTPS, blocking) | ✅ working |
| `DataStoreService` | ⚠️ local JSON files only, no cloud backend yet |
| Server authentication / anti-cheat / accounts | ❌ does not exist — see [Security](#security-read-this-before-you-publish) |
| macOS binaries | ❌ build from source |

## Install

**Requirements**

- **Godot 4.3 or newer** (`compatibility_minimum = "4.3"`; developed against Godot 4.6/4.7).
- **Windows x86_64** or **Linux x86_64**. Precompiled binaries for both ship in the release package; anything else needs [Building from source](#building-from-source).
- Python 3 only if you want to use the project wizard in `tools/`.

**Option A — the wizard (recommended for a brand new game)**

```bash
python tools/nuevo_proyecto.py
```

It asks for a name, a mode (3D or 2D) and a folder, and writes a complete Godot project: the compiled extension in `GodotLuau/bin/`, `godot_luau.gdextension`, the Roblox-style icons, the **GodotLuau Config** addon already enabled, and a root `RobloxGame` scene.

**Option B — add it to a project you already have**

1. Download the release package for your platform and unzip it **into your project root**. It contains `godot_luau.gdextension`, `GodotLuau/` (binaries, icons, assets, shaders, default scripts) and `addons/GodotLuauUpdater/`.
2. Open the project. Godot loads the extension at startup (restart it once if it was already open).
3. Enable **GodotLuau Config** in *Project → Project Settings → Plugins* if it is not on already.
4. Add a **`RobloxGame3D`** (or `RobloxGame2D`) node as the scene root and save it.

From then on the built-in auto-updater can pull newer versions from GitHub (it verifies the SHA-256 before installing).

## Your first project, step by step

1. **Open the project.** The root `RobloxGame3D` node **auto-builds the whole service hierarchy** the first time the scene is opened, so you immediately see `Workspace` (with a tintable grid Baseplate), `Players`, `Lighting`, `ServerScriptService`, `StarterPlayer/StarterPlayerScripts`, and the rest.
2. **Add a script.** Right-click `ServerScriptService` → *Add Child Node* → `ServerScript`. The node creates its own `.lua` file under `res://GodotLuau/ServerScripts/`, editable inside Godot with Roblox Studio syntax colors.
3. **Write Luau in it:**

   ```lua
   local Players = game:GetService("Players")

   Players.PlayerAdded:Connect(function(player)
       print(player.Name .. " joined")
   end)

   local part = Instance.new("Part")
   part.Size = Vector3.new(8, 1, 8)
   part.Position = Vector3.new(0, 10, 0)
   part.BrickColor = BrickColor.new("Bright red")
   part.Parent = workspace
   ```
4. **Press F5.** The script runs top to bottom, exactly like in Roblox.
5. **Press Esc** in game for the Roblox-style menu (players list with ping, graphics quality, max FPS, FPS/ping overlays, volume, camera sensitivity). Settings are saved per player in `user://gl_settings_player<N>.cfg`.

## Writing scripts in Luau

| Node | Colour | Context | When it runs |
|---|---|---|---|
| `LocalScript` | Blue | **Client** (`UserInputService` available) | On its own, when the game starts |
| `ServerScript` | Orange | **Server** (`UserInputService` blocked) | On its own, when the game starts |
| `ModuleScript` | Purple | Neutral | **Never on its own** — loaded with `require()`, returns its value |

Key differences from GDScript, and things worth knowing:

- **No `_ready` / `_process`.** The body of the `.lua` file runs top to bottom. For continuous logic use `RunService.Heartbeat:Connect(...)` or a loop with `task.wait()`.
- **`Enabled` property**, like in Studio: a disabled script does not run; re-enabling it at runtime runs it then.
- **Each `LocalScript`/`ServerScript` gets its own Luau VM.** A `ModuleScript` required from two different scripts does **not** share state between them (this is a real divergence from Roblox).
- **Available globals:** `game`, `workspace`, `script`, `require`, `shared`, plus the Luau standard libraries (`math`, `string`, `table`, `coroutine`, `os`, `utf8`, `bit32`…), `print`, `warn`, `task.*` and the classic `wait` / `spawn` / `delay` / `tick` / `time`.
- **Errors read like Roblox errors:**

  ```
  ServerScriptService.Server:5: attempt to index nil with 'Name'
  Stack Begin
  Script 'ServerScriptService.Server', Line 5 - function doStuff
  Stack End
  ```

  Indexing something that does not exist raises `X is not a valid member of …`; `WaitForChild` really suspends the coroutine and warns `Infinite yield possible on '…'` after 5 seconds; `require` reproduces Roblox's exact messages. No callback path fails silently.
- **Script identity is stable.** Every script node owns an immutable `script_id` (e.g. `ServerScript_ID_3`) that names its `.lua` file. Deleting the node moves the file to `res://.luau_trash/` (**Ctrl+Z** restores node and file together; the trash auto-purges after 7 days).

## Importing Roblox places (.rbxl)

GodotLuau reads Roblox's **binary place format** directly, so a place you saved from Studio comes back as a full tree in Godot.

**How:** in Godot, **Project → Tools → "Importar lugar de Roblox (.rbxl)"**, then pick your `.rbxl`. **3D only** — importing into a 2D Workspace is rejected with a clear message.

The import runs in batches across frames (~30 ms per frame) so the editor stays responsive, with a progress window showing the phase and the current item (including `Script 45 of 202: MyScript`). Three stages: **Importing** (instances, properties, hierarchy, cross-references) → **Comprobado / checked** (every instance in the file is verified to exist and be inside the tree, so nothing is silently skipped) → **Recolocado / placed** (the result is merged into the Roblox structure already in your scene: your existing `Workspace`, `Players`, `Lighting`… are **reused, not duplicated**).

From code:

```gdscript
var imp = RBXImporter.new()
var root = imp.ImportFile("C:/path/MyPlace.rbxl")   # blocking
print(imp.GetReport())
```

Or step by step, to drive your own progress bar:

```gdscript
imp.Begin(path)
imp.SetTarget(my_game_node)        # optional: merge into an existing Game
while not imp.IsDone() and not imp.HasFailed():
    imp.Step()
    print(imp.GetPhaseName(), imp.GetProgress(), imp.GetCurrentItem())
```

**What comes in:** the full instance tree as the Explorer shows it — names, parents, child order, sizes, `CFrame` transforms, colors (including `BrickColor` and the `Material` enum), GUI layout, **the Luau source of every script**, **CollectionService tags**, **instance attributes**, and cross-instance references such as `PrimaryPart` or `Part0`/`Part1`. Classes GodotLuau does not implement yet arrive as `RBXInstance`, keeping the class name, hierarchy and every property as metadata — **nothing is dropped**, so scripts that walk the tree still find what they expect.

**What cannot come in:** meshes, textures and sounds are **not stored in the file**; they are `rbxassetid://` pointers to Roblox's servers. The importer keeps every id (on the node and in the report) so you can export the model from Studio yourself. DataStores, gamepasses and place configuration live in the cloud too. `CustomPhysicalProperties` and the new `Content` type are skipped. XML places (`.rbxlx`) are not read yet — save as binary `.rbxl` in Studio.

> Reading your own files is interoperability, the same thing [rbx-dom](https://github.com/rojo-rbx/rbx-dom) does.

---

# Online mode / multiplayer

Networking is **real** and runs on Godot's ENet (UDP) layer, so it is cross-platform: a Linux server accepts Windows clients and vice versa, with no changes. There is **no relay, no login and no official matchmaking service** — you host it yourself, the way a Minecraft server works.

**Vocabulary used below**

| Term | Meaning |
|---|---|
| **Client** | A player's game. Does not run `ServerScript`s: it receives the world from the server by replication, like Roblox. |
| **Listen server (host)** | One machine that plays *and* serves at the same time. |
| **Dedicated server** | A pure server: runs your `ServerScript`s and accepts players, with **no local character** (like `minecraft_server.jar`). |
| **Coordinator** | A lightweight server that does not host a world; it sends each arriving player to a **world** with room, and starts a new world when all are full. |
| **World (instance)** | A separate dedicated-server process launched by the coordinator, on its own port. |

## The four ways to play together

| | Use it for | Who runs it | Address |
|---|---|---|---|
| **1. Local test** | Developing and debugging | The editor's Play button | `127.0.0.1:25575` (automatic) |
| **2. Dedicated server** | Playing with friends, one world | You (PC or VPS) | `your-ip:25565` (you choose) |
| **3. Coordinator + worlds** | Many parallel worlds, Roblox-style | You (PC or VPS) | `your-ip:25565` baked into the client |
| **4. Listen server** | A quick game from inside your own UI | Any player, from Luau | `net:StartServer(...)` |

---

## 1. Local test from the editor

The editor toolbar has a **Players selector (1–8)** and a **device selector** (PC / Mobile / Console / VR preview).

1. Set **Players** to 2 or more.
2. Press the native **Play** button.

GodotLuau launches one window per player (`Player1`…`PlayerN`) **plus a Server View window** — a free camera over the world (WASD to move, right-click to look, wheel to change speed, E/Space up, Q/Ctrl down). The windows are tiled and titled automatically. Positions, rotations, animation states and the chat replicate in real time.

Details that matter while debugging:

- The local test always uses port **25575** on `127.0.0.1`. It is chosen by whoever opens it first: if the port is taken, that window joins as a client instead, so a session always forms.
- The **server window has no character** (exactly like Roblox Studio) and it cannot be switched back to a client view.
- With 1 player you still get the Server View toggle in the Game panel.
- To point the client windows at **another machine** instead of `127.0.0.1`, create a file `res://.gl_host` whose only content is that machine's IP.
- Turn on **Debug mode** in the GodotLuau Config panel to see the internal `[GodotLuau MP]` log (roles, who hosts, TX/RX).

## 2. Dedicated server (LAN or Internet)

This is the mode you want for "put my game online". A dedicated server is **your own exported game** started with one extra argument.

### Step 1 — start the server

From your **exported** game:

```bat
MiJuego.exe --headless -- --glserver 25565
```

- `--headless` = no window (optional; drop it to watch the world with the free camera).
- `--glserver 25565` = listen on UDP port **25565**, up to **64** players, **no local character**.
- Note the lone `--`: everything after it are *user* arguments, which is how Godot passes them to the extension.

On Linux:

```bash
./MiJuego.x86_64 --headless -- --glserver 25565
```

Ready-made launchers ship in `GodotLuau/DedicatedServer/` (`start_server.bat`, `start_server.sh`) — edit the executable name and double-click. That folder also has its own detailed guide in both languages:
**[README - English.md](GodotLuau/DedicatedServer/README%20-%20English.md)** · **[README - Espanol.md](GodotLuau/DedicatedServer/README%20-%20Espanol.md)**.

You can also serve straight from the editor, which is handy for testing before you export:

```bat
Godot_v4.x.exe --path "C:\path\to\your\project" --headless -- --glserver 25565
```

When it comes up you get:

```
[GodotLuau] SERVIDOR DEDICADO escuchando en el puerto 25565 (sin jugador local).
```

### Step 2 — connect a client

Either by command line:

```bat
MiJuego.exe --glconnect 203.0.113.5:25565
```

(if you omit `:port` it defaults to 25565; the client retries about 20 times, once every 1.5 s, so you can launch it before the server is fully up)

or from your own UI, in Luau:

```lua
local net = game:GetService("NetworkService")
net:Connect("203.0.113.5", 25565)
print(net:GetConnectionState())   -- "Connecting" → "Connected"
```

or from a saved server list, Minecraft-style:

```lua
local net = game:GetService("NetworkService")
net:AddServer("My VPS", "203.0.113.5", 25565)      -- saved in user://gl_servers.json
net:AddServer("Ana's PC", "192.168.1.20", 25565)
for _, s in ipairs(net:GetServers()) do print(s.Name, s.IP, s.Port) end
net:JoinServer("My VPS")
net:RemoveServer("Ana's PC")
```

### Step 3 — choose where it lives

- **Same PC (test):** connect to `127.0.0.1:25565`.
- **Your LAN (friends at home):** run the server on your PC and give them your local IP. `net:GetLocalIP()` prints it (first non-loopback IPv4).
- **Over the Internet, from your PC:** forward **UDP 25565** in your router to the machine that hosts, allow the port in the OS firewall, and share your public IP. Your ISP may use CGNAT, in which case port forwarding will not work and you need a VPS.
- **On a VPS:** upload the exported game (or the project + a headless Godot build) and run the same command; share the VPS public IP. On Linux you must build the `.so` once on that machine — see [Building from source](#building-from-source).

## 3. Coordinator + worlds (Roblox-style matchmaking)

Instead of one shared world, run a **coordinator**: when a player arrives it redirects them to a world with room, and if every world is full it **starts a new one**, respecting a maximum per world — the way Roblox spreads players across servers.

### The owner starts the coordinator and leaves it running

```bat
MiJuego.exe --headless -- --glhost 25565 --glmax 8
```

- `--glhost 25565` = coordinator on port 25565 (it hosts no world of its own).
- `--glmax 8` = at most 8 players per world.
- Worlds are spawned as **separate processes** on `coordinator_port + 1`, `+2`, `+3`… (so 25566, 25567, …) with `--glserver <port> --glinstance`.
- The first run creates `user://host.key` and prints it. Read [Security](#security-read-this-before-you-publish) before assuming it protects anything.

### Players join without typing an IP

Before exporting, create a file `res://gl_match.cfg` whose single line is your coordinator's address:

```
203.0.113.5:25565
```

When the exported game opens, it connects to the coordinator on its own, gets assigned a world, and reconnects there. No IP prompt, like Roblox.

> Two practical notes: `gl_match.cfg` is only read in an **exported** build (in the editor you get the local test instead), and Godot only ships non-resource files it is told to, so add `*.cfg` to *Export → Resources → "Filters to export non-resource files"* or the file will not be inside your `.pck`.

To test the same thing without exporting:

```bat
MiJuego.exe --glmatch 203.0.113.5:25565
```

From Luau:

```lua
local net = game:GetService("NetworkService")
net:StartHost(25565, 8)                       -- run the coordinator (owner)
net:JoinMatchmaking("203.0.113.5", 25565)     -- join through it (player)
net:GetHostKey()                              -- read user://host.key ("" if none)
net:GenerateHostKey()                         -- read it, creating it if missing
```

### Honest limits of this mode

- **The coordinator and its worlds must live on the same machine.** The coordinator discovers worlds by reading heartbeat files (`user://gl_inst_<port>.json`, refreshed once per second, considered dead after 6 s) and starts new worlds with a local process. There is no multi-machine cluster.
- **Every world port must be reachable too.** A client is redirected to the coordinator's IP on the *world's* port, so behind a router you must forward the coordinator port **and** the range of world ports you expect to use (25566, 25567, …).
- **`TeleportService` moves a player between worlds** of the same machine, reusing those same heartbeat files. Without a coordinator running there is nowhere to teleport to.
- All worlds on one machine **share `user://`**, so they share `DataStoreService` files (`user://ds_<Store>_<key>.json`). That is what makes a shared inventory work across worlds — and also means a stray write from a client would collide, which is why `DataStore` calls error out on clients.

## 4. Listen server from Luau

One machine that plays and serves at the same time, driven from your own menus:

```lua
local net = game:GetService("NetworkService")

net:StartServer(25565, 32)      -- host, keeping your local character (port and max are optional)
print("Tell your friends:", net:GetLocalIP())

net:Connect("192.168.1.20", 25565)   -- or join someone else
net:Disconnect()                     -- back to single player
```

The host is always player 1. `UserId`s are assigned by the server in join order (1, 2, 3…) and replicated, so every window agrees on who is who.

## Command-line reference

Everything after the lone `--` is a *user* argument. Values shown are what the code actually defaults to.

| Argument | Role | Default |
|---|---|---|
| `--glserver <port>` | Start as a **dedicated server** on that port (no local character, max 64 players) | — |
| `--glconnect <ip>[:<port>]` | Start as a **client** connecting to that address | port `25565` |
| `--glhost <port>` | Start as a **coordinator** (`--glmax` sets players per world) | — |
| `--glmax <n>` | Maximum players per world | `8` |
| `--glinstance` | Marks this dedicated server as a **world** of a coordinator (publishes its heartbeat) | off |
| `--glmatch <ip>[:<port>]` | Join **through a coordinator** (it assigns you a world) | port `25565` |
| `--glsecret <key>` | Pass the owner key to a server process instead of reading `user://host.key` | — |
| `--gldevice PC\|Mobile\|Console\|VR` | Emulated device (window size, touch controls, VR split preview) | `PC` |
| `--glindex <n>` / `--glcount <n>` | Internal, used by the editor's local test (index 1 = server, 2..N+1 = `Player1..N`) | — |

> ⚠️ **Careful with `--glhost`:** the code comments also describe `--glhost <ip>` as a way to point the local test's client windows at another machine, but `_auto_init` parses `--glhost` as a **port number** for the coordinator, and the coordinator branch is evaluated first. Passing an IP there will *not* do what the comment says. Use the file `res://.gl_host` for the "point the test at another PC" case, and treat `--glhost` as "start the coordinator on this port".

## Networking API for Luau

`local net = game:GetService("NetworkService")`

```lua
-- Hosting / joining
net:StartServer(port?, maxPlayers?)      -- listen server (default 25565, 32)
net:Connect(ipOrHostname, port?)         -- join (hostnames are resolved by DNS)
net:Disconnect()                          -- back to single player
net:StartHost(port?, maxPerWorld?)        -- coordinator (default 25565, 8)
net:JoinMatchmaking(ip, port?)            -- join through a coordinator

-- Saved server list (user://gl_servers.json)
net:AddServer(name, ip, port?)
net:RemoveServer(name)
net:GetServers()                          -- array of { Name, IP, Port }
net:JoinServer(name)

-- State
net:IsServer() / net:IsClient() / net:IsConnected()
net:IsDedicatedServer()
net:GetConnectionState()                  -- "Server" | "Connecting" | "Connected" | "Disconnected"
net:GetServerAddress()                    -- "ip:port" you connected to ("" if host/single)
net:GetLocalIP()                          -- your LAN IPv4, to hand to friends
net:GetPeerId() / net:GetPlayerCount()
net:GetDevice() / net:GetPlayerIndex()
net:GetHostKey() / net:GenerateHostKey()

-- Signals (Roblox style)
net.PlayerConnected:Connect(function(peerId) end)
net.PlayerDisconnected:Connect(function(peerId) end)
net.Connected:Connect(function() end)
net.ConnectionFailed:Connect(function() end)
```

For normal gameplay you rarely need this service: `Players.PlayerAdded`, `RemoteEvent`, `RemoteFunction` and `Player.Character` already work across the network.

## What replicates and what does not

**Replicated automatically**

- Player positions, rotations and animation state, broadcast by their owner (no smoothing is invented on the receiving side).
- Remote characters are **real characters**, not dummies: R6 parts keep their Roblox names as direct children, and they have a `Humanoid`, so `hit.Parent:FindFirstChild("Humanoid")` works against other players.
- **Health is server-authoritative:** if a `ServerScript` calls `hum:TakeDamage(10)`, the server pushes the value to the owner and it propagates to everyone.
- **`RemoteEvent` and `RemoteFunction` over the wire**, reliable and unreliable, both directions. Payloads support nil/bool/number/string, nested array/dictionary/mixed tables, `Vector3`, `Color3`, every table-like datatype (`Vector2`, `CFrame`, `UDim`, `UDim2`, `Rect`, `Region3`, `Ray`, `NumberRange`, `BrickColor`, `NumberSequence`, `ColorSequence`, `PhysicalProperties`, `DateTime`) and Instances (by stable network id, with path as a fallback). Anything unsendable raises a Luau error instead of silently arriving as `nil`. `EnumItem`s travel as their numeric value.
- **Instance replication**: create, property change, reparent and destroy, with ids that are deterministic for objects that came with the place.
- **Network ownership for unanchored parts**: the authority simulates and broadcasts transform + velocity; everyone else freezes their copy and follows.
- Sequential `UserId`s, teams, `LoadCharacter`, `Kick` (with reason), a synchronized clock, per-client ping readable from the server, and the chat.
- `ServerScriptService` and `ServerStorage` are **emptied on clients**, so a `LocalScript` cannot read them.

**Not replicated / not there yet**

- No relay or NAT punch-through: the server must be reachable (LAN, port forwarding or VPS).
- No official server browser, accounts, or friend list.
- Emptying `ServerStorage` on the client cuts Luau access, but the data is still inside the client's `.pck`. Truly hiding it means not packing it into the client build, which the exporter would have to do.
- `DataStoreService` writes local JSON files; there is no cloud persistence yet.

## Files the network layer creates

| File | Who writes it | What for |
|---|---|---|
| `user://host.key` | Coordinator (created on first run) | The owner's 24-character key, printed at startup |
| `user://gl_servers.json` | `net:AddServer` | Saved server list (`name`, `ip`, `port`) |
| `user://gl_inst_<port>.json` | Each world, once per second | Heartbeat (`port`, `players`, timestamp); stale after 6 s |
| `user://ds_<Store>_<key>.json` | `DataStoreService` | Server-side saved data |
| `user://gl_settings_player<N>.cfg` | Settings menu | Per-player graphics/audio/input settings |
| `res://gl_match.cfg` | **You**, before exporting | Coordinator address for automatic join |
| `res://.gl_host` | **You**, optional | IP the local test's client windows should target |
| `res://.gl_mp_session`, `res://.gl_view_cmd` | The editor plugin | Local test session and Server View toggle |

On Windows `user://` is normally `%APPDATA%\Godot\app_userdata\<project name>`; on Linux `~/.local/share/godot/app_userdata/<project name>`.

## Security: read this before you publish

Being blunt, because this is the part that gets people hurt:

- **`host.key` is not authentication yet.** The coordinator creates it if it does not exist, prints it, and passes it to the worlds it spawns — but **no incoming connection is validated against it** anywhere in the current code. Do not treat it as a password. The only thing it actually gates is the coordinator refusing to start if it cannot create/read the file.
- **The real protection today is the address you ship.** Clients built with `gl_match.cfg` only ever connect to *your* coordinator, so a stranger cannot lure your players onto a modified server just by running one. Nothing stops someone from running their own server for their own players, though.
- **There is no login and no password anywhere in GodotLuau.** Never write a system that asks your players for a Roblox password, or any password, inside a game — the extension has no way to store or verify one.
- **A dedicated server (`--glserver`) requires no key at all.** Anyone who has your exported game can host one.
- **Trust the server, not the client.** A client cannot run `ServerScript`s and cannot touch `DataStore`, but any client can send any `RemoteEvent` payload. Validate on the server side, exactly as you would in Roblox.

## Troubleshooting a connection

| Symptom | Likely cause and fix |
|---|---|
| `No se pudo abrir el servidor dedicado en el puerto N` | The port is already in use (another server still running?) or blocked. Pick another port or close the other process. |
| Client stays on `"Connecting"` forever | Wrong IP, server not up, UDP port not forwarded, or firewall. Test on `127.0.0.1` first, then LAN, then Internet. |
| Works on LAN but not from outside | Forward the port as **UDP** (not TCP), use your **public** IP, and check whether your ISP uses CGNAT. |
| `No se pudo resolver el host X` | The hostname does not resolve; use a plain IP. |
| Coordinator connects, then nothing | The assigned world's port is not reachable. Forward `coordinator_port + 1…N` too. |
| Exported game does not auto-join | `gl_match.cfg` was not packed. Add `*.cfg` to the export filter for non-resource files. |
| I cannot tell who is server or client | Turn on **Debug mode** in the GodotLuau Config panel and read the `[GodotLuau MP]` lines. |
| `--glserver` server shows no character | That is correct: a dedicated server has no local player. |

## Optional backend (Nakama)

`backend/` holds a **local Nakama + PostgreSQL stack** (`docker-compose.yml`) intended as the future central backend (accounts, cloud DataStore). It is **optional and not wired into the extension**: today `HttpService` can talk to it because HTTP/HTTPS is real, but there is no `NakamaService` and `DataStoreService` does not route to it.

```bash
cd backend
docker compose up -d              # Nakama + Postgres in the background
docker compose logs -f nakama
docker compose down               # stop (data kept in the volume)
```

| What | Value |
|---|---|
| HTTP API (what `HttpService` calls) | `http://127.0.0.1:7350` |
| Admin console | `http://127.0.0.1:7351` (`admin` / `password`) |
| Server key | `defaultkey` |

See `backend/README.md` for the roadmap. The local defaults above are development credentials — change them before any of this touches a real machine.

---

## How it works (architecture)

GodotLuau is a **GDExtension**: a native C++ library Godot loads at startup. All code lives under `src/`, grouped by area.

**1. The embedded interpreter.** On startup (`src/core/register_types.cpp`) the extension registers Luau as a scripting language plus a `.lua` loader/saver (which is why Godot can open, edit and save `.lua` natively), registers every Roblox-style class with its icon, and raises the Jolt Physics body limits — Godot's default 10,240 bodies is far too low for voxel or tycoon worlds. The interpreter is the **official Luau** (VM, compiler and AST) compiled into the binary.

**2. The service tree.** The root node (`RobloxGame3D` / `RobloxGame2D`) auto-builds the hierarchy the first time the scene opens:

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

Reach them with `game:GetService("Players")` or directly (`game.Workspace`, `workspace`), and navigate with `FindFirstChild`, `WaitForChild`, `GetChildren` or by name.

**3. The R6 avatar.** By default players spawn as the **R6 rig**: six parts hung from pivots at the classic joint positions, so the code-driven animations (walk pendulum with opposite arms and legs, idle breathing, airborne arms-up) rotate like the classic avatar. Parts keep their Roblox names (`"Left Arm"`, `"Torso"`, `"HumanoidRootPart"`…). Drop any model named **`StarterCharacter`** under `StarterPlayer` and it becomes everyone's character instead. Camera-proximity fade uses a dissolve shader (pixels discarded, not alpha-blended), and right-click camera rotation restores the mouse to where it was.

**4. Lifetime safety.** Luau objects never hold raw pointers to nodes — they hold the `ObjectID` and resolve it against the live object database on each use, so a destroyed node yields `nil` instead of a crash. A registry of live Luau VMs guarantees callbacks never run on a closed interpreter (`src/core/gl_runtime.h`).

## Node and class catalog

All of these are registered in the editor with their Roblox-style icon:

- **Scripts:** `LocalScript`, `ServerScript`, `ModuleScript`.
- **Game roots:** `RobloxGame3D`, `RobloxGame2D`, `RobloxTemplate`.
- **3D characters and physics:** `Humanoid`, `RobloxWorkspace`, `RobloxPlayer`, `RobloxPart`.
- **2D:** `Humanoid2D`, `RobloxWorkspace2D`, `RobloxPlayer2D`.
- **Services:** `Players`, `Lighting`, `MaterialService`, `ReplicatedStorage`, `ReplicatedFirst`, `ServerStorage`, `ServerScriptService`, `StarterPlayer`, `StarterPlayerScripts`, `StarterCharacterScripts`, `StarterGui`, `StarterPack`, `Teams`, `SoundService`, `RunService`, `TextChatService`, `NetworkClient`, `NetworkService`, `CollectionService`, `UserInputService`, `TweenService`, `Folder`.
- **Communication:** `RemoteEventNode`, `RemoteFunctionNode`, `BindableEventNode`.
- **BodyMovers:** `BodyVelocity`, `BodyPosition`, `BodyForce`, `BodyAngularVelocity`, `BodyGyro`.
- **Constraints:** `WeldConstraint`, `HingeConstraint`, `BallSocketConstraint`, `RodConstraint`, `SpringConstraint`, `Motor6D`, `Attachment`.
- **GUI:** `ScreenGui`, `RobloxFrame`, `RobloxTextLabel`, `RobloxTextButton`, `RobloxTextBox`, `RobloxImageLabel`, `RobloxScrollingFrame`, `BillboardGui`, `SurfaceGui`, plus helpers (`UICorner`, `UIListLayout`, `UIStroke`, `UIGradient`…).
- **Lighting / effects:** `AtmosphereNode`, `LightingSkyNode`, `SunRaysNode`, `BloomEffect`, `BlurEffect`, `ColorCorrectionEffect`, `DepthOfFieldEffect`, `ParticleEmitter`, `Beam`, `Trail`, `Highlight`, `Smoke`, `Fire`, `Explosion`…
- **Interaction / tools:** `ClickDetector`, `ProximityPrompt`, `SpawnLocation`, `RobloxTool`, `Backpack`, `Seat`, `VehicleSeat`.
- **Sound:** `RobloxSound`, `RobloxSoundGroup`, sound effect nodes.
- **Animation:** `AnimationTrack`, `AnimationObject`, `GLR6Animator`.
- **Chat:** `RobloxChat`.
- **Imported-place placeholders:** `RBXInstance`, `RBXModel`, `RBXMeshPart`, `RBXTexture`, `RBXDecal`, `RBXMesh`, `RBXBone`.

## Lighting

- `ClockTime`, `TimeOfDay` (`"18:30:00"`), `Brightness`, `Ambient` / `OutdoorAmbient`, fog, `ColorShift_Top/Bottom`.
- `SetMinutesAfterMidnight()` / `GetMinutesAfterMidnight()`, automatic day/night cycle.
- Presets (Realistic, Cartoon, Anime, Sunset, Night…).
- `Technology` drives Godot's real graphics quality: *Compatibility/Legacy*, *ShadowMap*, *Future* (SSAO + SSIL + Glow), *Voxel* (GI).
- The environment lives in **`GodotLuau/shaders/environment_roblox.tres`** — edit it to change the look of every scene.

## Autocomplete and AI

The script editor gives **scene-aware** suggestions like Roblox Studio: typing `workspace.` or `FindFirstChild("` lists the real children of your scene with their class. Types come from the Roblox API definitions in `Scripts/globalTypes.d.luau` and `Scripts/DataTypes.json`. Luau is coloured with the Studio palette (pink keywords, blue builtins, green strings, yellow numbers), and syntax errors are highlighted inline.

There is also an optional **AI autocomplete**: a family of custom n-gram models called **LuauIA** (*Mini / Medium / High / HighPRO*), trained with `tools/entrenar_modelo.py` from the corpus in `corpus/`.

## Configuration panel

At the bottom of the editor: **GodotLuau Config** (English / Español / Português), organized into **Updates**, **AI & Autocomplete**, **Data**, **Debug** and **Appearance**.

- **Script output (`print`/`warn`)** — on by default.
- **Debug mode** — shows internal engine diagnostics, including the multiplayer log.
- **Updates** — checks the version on GitHub, downloads the package, verifies its **SHA-256**, restarts the editor.

## Building from source

`godot-cpp` and `luau` are **git submodules** pinned to exact commits:

```bash
git clone https://github.com/Pimpoli/GodotLuau.git
cd GodotLuau
git submodule update --init --recursive
```

Requirements: Python 3, [SCons](https://scons.org/) (`pip install scons`) and a C++ compiler (MinGW/MSVC on Windows, GCC/Clang on Linux).

```bash
scons platform=windows target=template_debug
scons platform=windows target=template_release
# or: scons platform=linux target=template_debug / template_release
```

Libraries land in `GodotLuau/bin/`. Copy the resulting `.so` files into the `GodotLuau/bin/` of your project on that Linux machine (next to the `.dll`s); `godot_luau.gdextension` already points at both. Linux binaries are also built by the GitHub Actions workflow (`.github/workflows/build-linux.yml`) on every push touching `src/`.

> `SConstruct` re-enables C++ exceptions (`-fexceptions` / `/EHsc`) because godot-cpp disables them by default and the Luau VM needs them. Nothing extra to pass.

## Repository structure

```
src/                       Extension C++ code, by area:
  core/                      Luau VM integration, language, registration, errors
  editor/                    Autocomplete + type database (editor only)
  importer/                  .rbxl reader and import plugin
  services/                  game, services, workspace, network (multiplayer)
  characters/                players, humanoids, parts, R6 avatar, body movers
  gameplay/                  remotes, input, interaction, animation, tween, sound
  ui/                        GUI, billboards, chat, lighting effects, settings menu
addons/GodotLuauUpdater/   Settings panel + auto-updater + editor UX (GDScript)
GodotLuau/                 Everything the extension ships:
  bin/                       Compiled libraries (.dll / .so)
  icons/                     Roblox-style SVG icons
  assets/                    R6 avatar meshes, textures (baseplate grid)
  shaders/                   character_fade.gdshader, environment_roblox.tres
  DefaultScripts/            Example player controllers (Luau)
  DedicatedServer/           Server launchers + the dedicated-server guides
  licenses/                  MIT licenses of GodotLuau and embedded software
backend/                   Optional local Nakama + Postgres stack (docker compose)
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
| `tools/generar_release.py` | Regenerates `GodotLuau.zip` + `.sha256` for the auto-updater |
| `tools/entrenar_modelo.py` | Trains the `models/LuauIA-*.json` family (AI autocomplete) |

## Known limitations

- **One VM per script:** a `ModuleScript` required by two scripts does not share state between them.
- **Self-hosted networking only:** no relay, no NAT traversal, no accounts, no official server browser. See [Security](#security-read-this-before-you-publish).
- **The coordinator is single-machine:** worlds are local processes discovered through `user://` heartbeat files.
- **`DataStoreService` is local JSON**, not cloud storage.
- **Very large part counts** (tens of thousands) work but are not batched yet; automatic clustering/instancing of anchored parts is the next big piece.
- **Platforms:** precompiled binaries are Windows x86_64 and Linux x86_64. macOS needs a source build.
- **`.rbxlx` (XML) places are not read**, and `rbxassetid://` content cannot be downloaded.

## Publishing a release (maintainer)

1. Rebuild both Windows DLLs (debug and release); push `src/` so the Linux workflow builds the `.so` files, and place them in `GodotLuau/bin/`.
2. Update the `Version` file and the version in `addons/GodotLuauUpdater/plugin.cfg`.
3. Regenerate the updater package: `python tools/generar_release.py`
4. Commit and push `Version`, `GodotLuau.zip` **and** `GodotLuau.zip.sha256` together.

The users' auto-updater compares `Version`, downloads the ZIP and verifies the SHA-256 before installing.

## License and credits

- **GodotLuau** — MIT License, © 2026 PimpoliDev (see `LICENSE`).
- **[Luau](https://github.com/luau-lang/luau)** — the embedded interpreter, MIT (Roblox Corporation).
- **[godot-cpp](https://github.com/godotengine/godot-cpp)** — C++ bindings for Godot, MIT.
- **[Godot Engine](https://godotengine.org/)** — the engine everything runs on, MIT.

Copies of every bundled license ship in the release package under `GodotLuau/licenses/`.
