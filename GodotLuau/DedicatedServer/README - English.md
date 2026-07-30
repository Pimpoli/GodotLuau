# GodotLuau — Dedicated server guide (English)

> Español: **[README - Espanol.md](README%20-%20Espanol.md)** · Project front page: **[../../README.md](../../README.md)**

This folder contains the launchers for running **your game as a server**:

| File | What it is |
|---|---|
| `start_server.bat` | Windows launcher — edit the `.exe` name and double-click |
| `start_server.sh` | Linux launcher — `bash start_server.sh` |
| `README - English.md` | This guide |
| `README - Espanol.md` | The same guide in Spanish |

---

## Table of contents

- [What a dedicated server is](#what-a-dedicated-server-is)
- [Requirements](#requirements)
- [Quick start (5 minutes, one world)](#quick-start-5-minutes-one-world)
- [How a client connects](#how-a-client-connects)
- [Ports: what to open and why](#ports-what-to-open-and-why)
- [Where to host it](#where-to-host-it)
- [Automatic worlds with matchmaking (coordinator)](#automatic-worlds-with-matchmaking-coordinator)
- [Saved server list](#saved-server-list)
- [Where the data lives](#where-the-data-lives)
- [Running on Linux](#running-on-linux)
- [Full argument reference](#full-argument-reference)
- [Networking API for Luau](#networking-api-for-luau)
- [When it does not connect](#when-it-does-not-connect)
- [Security: what is and is not protected](#security-what-is-and-is-not-protected)

---

## What a dedicated server is

A **dedicated server** runs your game as a pure server: it executes your `ServerScript`s and accepts players, **with no local character** — exactly like `minecraft_server.jar`. Nobody plays on the server machine; the players are the clients that connect to it.

Why you want one:

- The world keeps running when you close your own game.
- One authority for everyone: your `ServerScript`s decide health, drops, saved data.
- Clients do **not** run `ServerScript`s and cannot read `ServerStorage` / `ServerScriptService`, so game logic stays on your side.
- You can host it wherever you like: your PC, another PC on your network, or a VPS.

How it differs from the other modes:

| Mode | Local character? | Started with |
|---|---|---|
| Single player | Yes | Just open the game |
| Listen server (host plays) | Yes | `net:StartServer(port, max)` from Luau |
| **Dedicated server** | **No** | `--glserver <port>` |
| Coordinator (no world of its own) | No | `--glhost <port>` |

## Requirements

- Your game **exported** for the platform that will host it (Windows `.exe` or Linux `.x86_64`). You can also serve straight from a Godot install plus the project folder, which is handy while testing.
- The GodotLuau binary for that platform inside `GodotLuau/bin/`. The release package ships Windows `.dll`s; for Linux you build the `.so` once — see [Running on Linux](#running-on-linux).
- One free **UDP** port. The examples use **25565** (nothing forces that number; it is just the convention borrowed from Minecraft).
- No database, no account system and no external service is required. The network layer is ENet (UDP) built into Godot.

## Quick start (5 minutes, one world)

**1. Start the server**

From your exported game on Windows:

```bat
MiJuego.exe --headless -- --glserver 25565
```

On Linux:

```bash
./MiJuego.x86_64 --headless -- --glserver 25565
```

Three things to notice:

- `--headless` runs with no window. **Drop it** if you want to watch the world with the free camera (WASD to move, right-click to look, wheel for speed, E/Space up, Q/Ctrl down).
- The lone `--` matters: everything after it are *user* arguments, which is how Godot hands them to the extension. Without it Godot itself tries to interpret `--glserver` and fails.
- `--glserver 25565` means: listen on UDP 25565, accept up to **64** players, remove the local character.

If it worked, the console prints:

```
[GodotLuau] SERVIDOR DEDICADO escuchando en el puerto 25565 (sin jugador local).
```

If the port was busy or blocked you get instead:

```
[GodotLuau] No se pudo abrir el servidor dedicado en el puerto 25565
```

**2. Or use the launcher in this folder**

Open `start_server.bat`, change `set GAME=MiJuego.exe` to your executable's name (and `set PORT=25565` if you want another port), save, double-click. `start_server.sh` is the same for Linux.

**3. Or serve from the editor, without exporting**

```bat
Godot_v4.x.exe --path "C:\path\to\your\project" --headless -- --glserver 25565
```

## How a client connects

**Direct Connect by command line** (no Lua code needed):

```bat
MiJuego.exe --glconnect 203.0.113.5:25565
```

- If you omit `:port` it defaults to **25565**.
- A hostname works too (`--glconnect myserver.duckdns.org:25565`); it is resolved by DNS before connecting.
- The client retries about **20 times, once every 1.5 s**, so you can launch it before the server is fully up.

**From your own menu, in Luau:**

```lua
local net = game:GetService("NetworkService")

net:Connect("203.0.113.5", 25565)
print(net:GetConnectionState())      -- "Connecting" → "Connected"
print(net:GetServerAddress())        -- "203.0.113.5:25565"

net:Disconnect()                     -- back to single player
```

**With no IP typed at all:** see [Automatic worlds with matchmaking](#automatic-worlds-with-matchmaking-coordinator) — you bake your coordinator's address into the build and the player just opens the game.

Once connected, the client behaves like a Roblox client: it receives the world by replication, `Players.PlayerAdded` fires on the server, `RemoteEvent` / `RemoteFunction` work in both directions, other players appear as real characters (R6 parts with their Roblox names and a `Humanoid`), and health is server-authoritative.

## Ports: what to open and why

| Port | Who uses it | Protocol |
|---|---|---|
| The one you pass to `--glserver` (`25565` in every example) | Dedicated server ↔ clients | **UDP** |
| The one you pass to `--glhost` (`25565` by convention) | Coordinator ↔ clients | **UDP** |
| `coordinator port + 1`, `+2`, `+3`… (`25566`, `25567`…) | Each world spawned by the coordinator ↔ clients | **UDP** |
| `25575` | The editor's local multiplayer test, on `127.0.0.1` only | UDP |

ENet is **UDP**. If your router or firewall asks for a protocol, choose UDP (or both) — forwarding TCP only will silently never work. And if you use the coordinator, remember clients get redirected to a **world** port, so forward that range too.

## Where to host it

### On your own PC, for your local network

Run the server and give your friends your **local IP**:

```lua
local net = game:GetService("NetworkService")
print("Connect to:", net:GetLocalIP())   -- first non-loopback IPv4, e.g. 192.168.1.35
```

They connect with `--glconnect 192.168.1.35:25565`. Nothing to forward, but everyone must be on the same network.

### On your own PC, over the Internet

1. Give the hosting PC a fixed local IP (DHCP reservation in your router).
2. Forward **UDP 25565** in the router to that local IP.
3. Allow the port in the OS firewall (on Windows the first run usually asks — say yes for private *and* public if you want outside players).
4. Share your **public** IP (whatismyip-style sites show it).

Two common walls: your public IP changes over time (use a dynamic-DNS name), and many ISPs put you behind **CGNAT**, where port forwarding cannot work at all. If a correctly forwarded port still refuses connections from outside, CGNAT is the usual reason — a VPS is the fix.

### On another PC in your house

Same as above but you keep your machine free. Copy the exported game to that PC, run the launcher there, and connect to its local IP.

### On a VPS (the reliable option)

1. Rent any small Linux VPS (a world with a handful of players needs very little).
2. Upload either the exported Linux build, or the project plus a headless Godot build. In both cases the Linux `.so` of GodotLuau must be in `GodotLuau/bin/` — see [Running on Linux](#running-on-linux).
3. Open UDP 25565 in the provider's firewall/security group **and** in the VPS firewall (`ufw allow 25565/udp` on Ubuntu, for instance).
4. Run the same command. Use `screen`, `tmux` or a systemd unit so it survives you logging out.
5. Share the VPS public IP.

**Cross-play Windows ↔ Linux works out of the box** — ENet is cross-platform, so a Linux server accepts Windows clients and the other way round with no changes.

## Automatic worlds with matchmaking (coordinator)

Instead of a single shared world, you can run a **coordinator**: when a player arrives it sends them to a world that has room, and if all the worlds are full it **starts a new one**, respecting a maximum per world — the way Roblox spreads players across servers. Each world is a separate process, fully independent.

### The owner starts it and leaves it running

```bat
MiJuego.exe --headless -- --glhost 25565 --glmax 8
```

- `--glhost 25565` — coordinator on port 25565. It hosts no world itself and has no character.
- `--glmax 8` — at most 8 players per world.
- Worlds are launched as separate processes on `25566`, `25567`, … with `--glserver <port> --glinstance`.
- The first run creates `user://host.key` (a 24-character key) and prints it. Read [Security](#security-what-is-and-is-not-protected) before assuming it protects anything.

Console output looks like:

```
[GodotLuau] COORDINADOR activo en el puerto 25565 (máx 8 por mundo). host.key = ...
[GodotLuau] Coordinador: nuevo MUNDO en el puerto 25566 (pid 12345)
```

### Players join with no IP prompt

Before exporting, create a file `res://gl_match.cfg` whose only line is your coordinator's address:

```
203.0.113.5:25565
```

When the exported game opens it connects to the coordinator, receives a world assignment, and reconnects to that world by itself (it retries up to 40 times, once every 0.6 s, because a freshly spawned world takes a moment to open its port).

Two things that trip people up:

- `gl_match.cfg` is read **only in an exported build**. In the editor you get the local multiplayer test instead.
- Godot does not pack arbitrary non-resource files. Add `*.cfg` to *Export → Resources → "Filters to export non-resource files"*, or the file will not be inside your `.pck` and nothing will auto-join.

To try it without exporting:

```bat
MiJuego.exe --glmatch 203.0.113.5:25565
```

### Honest limits

- **Coordinator and worlds must be on the same machine.** The coordinator finds worlds by reading heartbeat files (`user://gl_inst_<port>.json`, written once per second, treated as dead after 6 s) and starts new ones as local processes. There is no multi-machine cluster.
- **Every world port must be reachable**, not just the coordinator's (see [Ports](#ports-what-to-open-and-why)).
- **`TeleportService` moves players between worlds** of that same machine using those same heartbeat files. With no coordinator running there is nowhere to teleport to.

## Saved server list

Clients can keep a server list like Minecraft's, from Luau:

```lua
local net = game:GetService("NetworkService")

net:AddServer("My VPS", "203.0.113.5", 25565)
net:AddServer("Ana's PC", "192.168.1.20", 25565)

for _, s in ipairs(net:GetServers()) do
    print(s.Name, s.IP, s.Port)
end

net:JoinServer("My VPS")          -- connect by name
net:RemoveServer("Ana's PC")
```

The list is stored in `user://gl_servers.json` as `[{ "name", "ip", "port" }]`. Adding a name that already exists replaces it.

## Where the data lives

Everything the server persists goes into Godot's `user://` folder on the **server machine**:

| File | What it holds |
|---|---|
| `user://ds_<Store>_<key>.json` | `DataStoreService` data — one JSON file per store and key |
| `user://host.key` | The owner key created by the coordinator |
| `user://gl_servers.json` | The saved server list (client side) |
| `user://gl_inst_<port>.json` | Heartbeat of each world: port, player count, timestamp |
| `user://gl_settings_player<N>.cfg` | Per-player settings from the Esc menu |

Typical location of `user://`: `%APPDATA%\Godot\app_userdata\<project name>` on Windows, `~/.local/share/godot/app_userdata/<project name>` on Linux.

Things worth knowing before you rely on this:

- **`DataStoreService` is server-only.** Calling it from a client raises `… can only be called from the server (DataStore is not accessible to clients)`. That is deliberate: all windows of one project share `user://`, so a client write would stomp the server's files.
- **`GetAsync` always reads the file**, not a stale in-memory cache, so a world started later sees what another world just saved.
- **Every world on one machine shares this folder**, which is what makes a shared inventory across worlds work.
- **This is local JSON, not cloud storage.** Back the folder up yourself. There is no cloud DataStore in this version.

## Running on Linux

GodotLuau is a C++ extension, so each operating system needs its own binary. The release package ships the **Windows** `.dll`s. For Linux, build the `.so` once on that Linux machine:

```bash
git clone --recursive https://github.com/Pimpoli/GodotLuau.git
cd GodotLuau
sudo apt install scons g++ python3-dev      # if you do not have them
scons platform=linux target=template_debug
scons platform=linux target=template_release
# produces GodotLuau/bin/godot_luau.linux.template_debug.x86_64.so (+ release)
```

Copy those `.so` files into the `GodotLuau/bin/` of **your** project on Linux (next to the `.dll`s) and Godot will load the extension there; `godot_luau.gdextension` already points at them. Then start the server with `bash start_server.sh` or the `--glserver` command above.

## Full argument reference

Everything after the lone `--` is a *user* argument. Defaults below are what the code actually uses.

| Argument | What it does | Default |
|---|---|---|
| `--glserver <port>` | Run as a **dedicated server** on that port (no local character, max 64 players) | — |
| `--glconnect <ip>[:<port>]` | Run as a **client** connecting to that address | port `25565` |
| `--glhost <port>` | Run as a **coordinator** | — |
| `--glmax <n>` | Maximum players per world | `8` |
| `--glinstance` | Marks this dedicated server as a **world** of a coordinator (publishes its heartbeat) | off |
| `--glmatch <ip>[:<port>]` | Join **through a coordinator**, which assigns you a world | port `25565` |
| `--glsecret <key>` | Hand the owner key to a server process instead of reading `user://host.key` | — |
| `--gldevice PC\|Mobile\|Console\|VR` | Emulated device (window size, touch controls, VR split preview) | `PC` |
| `--glindex <n>` / `--glcount <n>` | Internal: the editor's local test (index 1 = server, 2..N+1 = `Player1..N`) | — |

> ⚠️ **`--glhost` takes a PORT, not an IP.** Some code comments describe `--glhost <ip>` as a way to point the local test's client windows at another machine, but the argument is parsed as the coordinator's port and that branch runs first. For "point the local test at another PC", use the file `res://.gl_host` containing that machine's IP.

Godot's own flags still apply, and go **before** the `--`: `--headless` (no window), `--path <folder>` (run a project folder), `--verbose`.

## Networking API for Luau

```lua
local net = game:GetService("NetworkService")

-- Hosting / joining
net:StartServer(port?, maxPlayers?)      -- listen server, host keeps a character (25565, 32)
net:Connect(ipOrHostname, port?)         -- join a server (25565)
net:JoinServer(name)                     -- join a saved server by name
net:Disconnect()                          -- back to single player
net:StartHost(port?, maxPerWorld?)        -- coordinator (25565, 8)
net:JoinMatchmaking(ip, port?)            -- join through a coordinator (25565)

-- Server list
net:AddServer(name, ip, port?) / net:RemoveServer(name) / net:GetServers()

-- State
net:IsServer() / net:IsClient() / net:IsConnected()
net:IsDedicatedServer()                   -- true if this process is a dedicated server
net:GetConnectionState()                  -- "Server" | "Connecting" | "Connected" | "Disconnected"
net:GetServerAddress()                    -- "ip:port" you connected to ("" if host/single)
net:GetLocalIP()                          -- your LAN IPv4, to hand to friends
net:GetPeerId() / net:GetPlayerCount() / net:GetPlayerIndex() / net:GetDevice()
net:GetHostKey() / net:GenerateHostKey()

-- Signals
net.PlayerConnected:Connect(function(peerId) end)
net.PlayerDisconnected:Connect(function(peerId) end)
net.Connected:Connect(function() end)
net.ConnectionFailed:Connect(function() end)
```

A useful pattern for a server script that must behave differently when dedicated:

```lua
local net = game:GetService("NetworkService")

if net:IsDedicatedServer() then
    print("Dedicated server up — no local player here")
end
```

## When it does not connect

Work outwards: `127.0.0.1` first, then LAN, then the Internet. Each step rules out a whole class of problem.

| Symptom | Likely cause and fix |
|---|---|
| `No se pudo abrir el servidor dedicado en el puerto N` | The port is already in use (another server still running?) or blocked. Change the port or close the other process. |
| Server starts but shows no character | Correct behaviour: a dedicated server has no local player. Drop `--headless` to watch with the free camera. |
| Client stuck on `"Connecting"` | Wrong IP/port, server not up, UDP not forwarded, or firewall. Try `127.0.0.1` on the server machine to prove the server itself is fine. |
| `No se pudo resolver el host X` | The hostname does not resolve. Use a plain IP. |
| `No se pudo conectar a X` | The address is unreachable — bad IP, wrong port, or nothing listening. |
| Works on LAN, not from outside | Forward the port as **UDP**, use the **public** IP, check for CGNAT. |
| Coordinator connects, then it hangs | The assigned world's port is not reachable. Forward `coordinator port + 1…N`. Also `No se pudo entrar al mundo asignado por el coordinador.` appears after ~24 s of retries. |
| Exported game does not auto-join | `gl_match.cfg` was not packed. Add `*.cfg` to the export filter for non-resource files. |
| Godot complains about `--glserver` | You forgot the lone `--` before the GodotLuau arguments. |
| I cannot tell who is server and who is client | Turn on **Debug mode** in the GodotLuau Config panel and read the `[GodotLuau MP]` lines (roles, hosting, TX/RX). |
| Players see each other teleporting | Check the server's frame rate and the network path; positions are applied exactly as received, without smoothing. |

## Security: what is and is not protected

Read this before opening a port to the Internet.

- **`host.key` is not authentication yet.** The coordinator creates it if missing, prints it, and passes it to the worlds it spawns — but **no incoming connection is validated against it** anywhere in the current code. The only thing it really gates is the coordinator refusing to start when the file cannot be created or read. Do not treat it as a password.
- **A dedicated server needs no key at all.** Anyone with your exported game can run `--glserver`.
- **What does protect your players is the address you ship.** A client built with `gl_match.cfg` only ever connects to *your* coordinator, so nobody can pull your players onto a modified server just by starting one.
- **Never ask players for a password inside the game.** GodotLuau has no account system, no password storage and no way to verify one. If a "server" ever asks for a Roblox password, it is a scam — and you should not build one either.
- **Validate on the server.** Clients cannot run `ServerScript`s and cannot touch `DataStore`, but any client can send any `RemoteEvent` payload with any values. Check them on the server side, exactly as you would in Roblox.
- **`ServerStorage` / `ServerScriptService` are emptied on clients**, so Luau there cannot read them. Honest caveat: the data is still inside the client's `.pck`; truly hiding it means not packing it into the client build, which the exporter would have to handle.
- **Back up `user://`.** Player data is plain JSON files on the server machine, with no replication or snapshots.
