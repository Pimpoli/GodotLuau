# Servidores de GodotLuau (dedicado + mundos automáticos estilo Roblox)

Un servidor dedicado corre TU juego como servidor puro: ejecuta tus
ServerScripts y acepta jugadores, **sin jugador local** (igual que
`minecraft_server.jar`). Se aloja donde tú quieras: tu PC, otra PC, o un VPS.

## ⭐ Mundos automáticos con matchmaking (lo más fácil, estilo Roblox)

En vez de un solo mundo, corre un **COORDINADOR**: cuando un jugador entra, lo
manda solo a un **mundo con espacio**, y si todos están llenos **crea un mundo
nuevo** (respetando el máximo por mundo, como Roblox). Cada mundo es un proceso
aparte → totalmente independiente.

**El dueño lo enciende (en su PC o VPS) y lo deja prendido:**
```
MiJuego.exe --headless -- --glhost 25565 --glmax 8
```
- `--glhost 25565` = coordinador en el puerto 25565.
- `--glmax 8`      = máximo 8 jugadores por mundo.
- La **primera vez genera `host.key`** (una llave secreta) y la imprime. Esa
  llave es lo que hace que **solo tú puedas prender servidores** — guárdala; los
  jugadores no la tienen.

**Los jugadores entran solos.** Antes de exportar tu juego, crea el archivo
`res://gl_match.cfg` con una sola línea = la dirección de tu coordinador:
```
TU_IP:25565
```
Al abrir el juego exportado, el jugador se conecta solo al coordinador y este lo
mete en un mundo con gente. Sin escribir IPs, como Roblox. (Para probar sin
exportar: `MiJuego.exe --glmatch TU_IP:25565`.)

### ¿Por qué solo el dueño puede hostear? (seguridad)
- Los clientes se conectan **solo** a la dirección que tú horneaste en
  `gl_match.cfg` (tu coordinador). Nunca a un servidor cualquiera → nadie puede
  poner un "servidor modificado" que robe cuentas/contraseñas, porque los
  jugadores jamás se conectan ahí.
- Prender un coordinador/mundo requiere la `host.key`, que vive solo en tu
  máquina servidora (no va en el build del cliente). Sin ella, `--glhost` no
  arranca. Si quieres, puedes pasarla a otra PC de confianza con `--glsecret`.

---

## Un solo mundo (servidor dedicado simple)

## Cómo hostear

### Opción A — desde tu juego EXPORTADO
1. Exporta tu juego a Windows (`MiJuego.exe`).
2. Ejecuta:
   ```
   MiJuego.exe --headless -- --glserver 25565
   ```
   - `--headless`  = sin ventana (opcional; quítalo para ver el mundo con cámara libre).
   - `--glserver 25565` = hospeda en el puerto 25565.
3. Los jugadores se unen con `--glconnect TU_IP:25565` o desde tu propia UI con
   `net:Connect("TU_IP", 25565)`.

Edita `start_server.bat` (cambia el nombre del .exe) y haz doble clic.

### Opción B — desde el editor de Godot (para probar)
```
Godot_v4.x.exe --path "C:\ruta\a\tu\proyecto" --headless -- --glserver 25565
```

## Elegir dónde se aloja
- **Tu PC (LAN):** usa tu IP local. En Lua: `local ip = net:GetLocalIP()` te la dice.
  Los amigos de tu red se conectan a esa IP.
- **Por internet:** abre/redirige el puerto (port forwarding) en tu router hacia
  el PC que hostea, y comparte tu IP pública.
- **VPS:** sube el .exe (o el proyecto + Godot headless) al VPS y corre el mismo
  comando; comparte la IP pública del VPS.

## Lista de servidores (cambiar de server fácil)
Desde Luau puedes guardar y cambiar de servidor como en la lista de Minecraft:
```lua
local net = game:GetService("NetworkService")
net:AddServer("Mi VPS", "203.0.113.5", 25565)  -- guardar
net:AddServer("PC de Ana", "192.168.1.20", 25565)
for _, s in net:GetServers() do print(s.Name, s.IP, s.Port) end
net:JoinServer("Mi VPS")        -- conectar por nombre
net:RemoveServer("PC de Ana")   -- borrar
```
La lista se guarda en `user://gl_servers.json`.

## API de red útil
```lua
net:StartServer(25565)          -- hospedar en tu PC (con jugador)
net:Connect("IP", 25565)        -- unirse por IP o hostname
net:JoinServer("nombre")        -- unirse a un servidor guardado
net:Disconnect()                -- volver a un jugador
net:GetConnectionState()        -- "Server"/"Connecting"/"Connected"/"Disconnected"
net:GetServerAddress()          -- "ip:puerto" al que te conectaste
net:GetLocalIP()                -- tu IP LAN (para dársela a tus amigos)
net:IsDedicatedServer()         -- true si esta instancia es un server dedicado
```
