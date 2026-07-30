# GodotLuau — Guía del servidor dedicado (Español)

> English: **[README - English.md](README%20-%20English.md)** · Portada del proyecto: **[../../README - Espanol.md](../../README%20-%20Espanol.md)**

En esta carpeta están los lanzadores para correr **tu juego como servidor**:

| Archivo | Qué es |
|---|---|
| `start_server.bat` | Lanzador de Windows — cambia el nombre del `.exe` y doble clic |
| `start_server.sh` | Lanzador de Linux — `bash start_server.sh` |
| `README - Espanol.md` | Esta guía |
| `README - English.md` | La misma guía en inglés |

---

## Índice

- [Qué es un servidor dedicado](#qué-es-un-servidor-dedicado)
- [Requisitos](#requisitos)
- [Arranque rápido (5 minutos, un mundo)](#arranque-rápido-5-minutos-un-mundo)
- [Cómo se conecta un cliente](#cómo-se-conecta-un-cliente)
- [Puertos: qué abrir y por qué](#puertos-qué-abrir-y-por-qué)
- [Dónde alojarlo](#dónde-alojarlo)
- [Mundos automáticos con matchmaking (coordinador)](#mundos-automáticos-con-matchmaking-coordinador)
- [Lista de servidores guardados](#lista-de-servidores-guardados)
- [Dónde se guardan los datos](#dónde-se-guardan-los-datos)
- [Correr en Linux](#correr-en-linux)
- [Referencia completa de argumentos](#referencia-completa-de-argumentos)
- [API de red para Luau](#api-de-red-para-luau)
- [Cuando no conecta](#cuando-no-conecta)
- [Seguridad: qué está protegido y qué no](#seguridad-qué-está-protegido-y-qué-no)

---

## Qué es un servidor dedicado

Un **servidor dedicado** corre tu juego como servidor puro: ejecuta tus `ServerScript`s y acepta jugadores, **sin personaje local** — igual que `minecraft_server.jar`. En la máquina del servidor no juega nadie; los jugadores son los clientes que se conectan.

Para qué lo quieres:

- El mundo sigue vivo aunque tú cierres tu juego.
- Una sola autoridad para todos: tus `ServerScript`s deciden la salud, los drops y los datos guardados.
- Los clientes **no** ejecutan `ServerScript`s y no pueden leer `ServerStorage` / `ServerScriptService`, así que la lógica del juego se queda de tu lado.
- Lo alojas donde quieras: tu PC, otra PC de tu red, o un VPS.

En qué se diferencia de los otros modos:

| Modo | ¿Personaje local? | Se arranca con |
|---|---|---|
| Un jugador | Sí | Solo abrir el juego |
| Servidor con jugador (el host juega) | Sí | `net:StartServer(puerto, max)` desde Luau |
| **Servidor dedicado** | **No** | `--glserver <puerto>` |
| Coordinador (sin mundo propio) | No | `--glhost <puerto>` |

## Requisitos

- Tu juego **exportado** para la plataforma que va a alojarlo (Windows `.exe` o Linux `.x86_64`). También puedes servir con una instalación de Godot más la carpeta del proyecto, que va bien mientras pruebas.
- El binario de GodotLuau de esa plataforma dentro de `GodotLuau/bin/`. El paquete de release trae las `.dll` de Windows; para Linux se compila el `.so` una vez — ver [Correr en Linux](#correr-en-linux).
- Un puerto **UDP** libre. Los ejemplos usan **25565** (nada obliga a ese número; es la convención heredada de Minecraft).
- No hace falta base de datos, ni sistema de cuentas, ni ningún servicio externo. La red es ENet (UDP), que ya viene en Godot.

## Arranque rápido (5 minutos, un mundo)

**1. Arranca el servidor**

Desde tu juego exportado en Windows:

```bat
MiJuego.exe --headless -- --glserver 25565
```

En Linux:

```bash
./MiJuego.x86_64 --headless -- --glserver 25565
```

Tres cosas en las que fijarse:

- `--headless` corre sin ventana. **Quítalo** si quieres ver el mundo con la cámara libre (WASD mueve, clic derecho mira, la rueda cambia la velocidad, E/Espacio sube, Q/Ctrl baja).
- El `--` solo importa: todo lo que va después son argumentos *de usuario*, que es como Godot se los pasa a la extensión. Sin él, Godot intenta interpretar `--glserver` y falla.
- `--glserver 25565` significa: escuchar en UDP 25565, aceptar hasta **64** jugadores y quitar el personaje local.

Si funcionó, la consola imprime:

```
[GodotLuau] SERVIDOR DEDICADO escuchando en el puerto 25565 (sin jugador local).
```

Si el puerto estaba ocupado o bloqueado, sale esto en su lugar:

```
[GodotLuau] No se pudo abrir el servidor dedicado en el puerto 25565
```

**2. O usa el lanzador de esta carpeta**

Abre `start_server.bat`, cambia `set GAME=MiJuego.exe` por el nombre de tu ejecutable (y `set PORT=25565` si quieres otro puerto), guarda y doble clic. `start_server.sh` es lo mismo para Linux.

**3. O sirve desde el editor, sin exportar**

```bat
Godot_v4.x.exe --path "C:\ruta\a\tu\proyecto" --headless -- --glserver 25565
```

## Cómo se conecta un cliente

**Direct Connect por línea de comandos** (sin escribir código Lua):

```bat
MiJuego.exe --glconnect 203.0.113.5:25565
```

- Si omites `:puerto` usa **25565**.
- También acepta un hostname (`--glconnect miservidor.duckdns.org:25565`); se resuelve por DNS antes de conectar.
- El cliente reintenta unas **20 veces, una cada 1,5 s**, así que puedes lanzarlo antes de que el servidor esté del todo listo.

**Desde tu propio menú, en Luau:**

```lua
local net = game:GetService("NetworkService")

net:Connect("203.0.113.5", 25565)
print(net:GetConnectionState())      -- "Connecting" → "Connected"
print(net:GetServerAddress())        -- "203.0.113.5:25565"

net:Disconnect()                     -- volver a un jugador
```

**Sin escribir ninguna IP:** mira [Mundos automáticos con matchmaking](#mundos-automáticos-con-matchmaking-coordinador) — horneas la dirección de tu coordinador en la build y el jugador solo abre el juego.

Una vez conectado, el cliente se comporta como un cliente de Roblox: recibe el mundo por replicación, `Players.PlayerAdded` se dispara en el servidor, `RemoteEvent` / `RemoteFunction` funcionan en los dos sentidos, los demás jugadores aparecen como personajes de verdad (partes R6 con sus nombres de Roblox y un `Humanoid`), y la salud es autoritativa del servidor.

## Puertos: qué abrir y por qué

| Puerto | Quién lo usa | Protocolo |
|---|---|---|
| El que le pasas a `--glserver` (`25565` en todos los ejemplos) | Servidor dedicado ↔ clientes | **UDP** |
| El que le pasas a `--glhost` (`25565` por convención) | Coordinador ↔ clientes | **UDP** |
| `puerto del coordinador + 1`, `+2`, `+3`… (`25566`, `25567`…) | Cada mundo que lanza el coordinador ↔ clientes | **UDP** |
| `25575` | La prueba de multijugador local del editor, solo en `127.0.0.1` | UDP |

ENet es **UDP**. Si tu router o firewall te pregunta el protocolo, elige UDP (o los dos) — redirigir solo TCP no funcionará nunca y sin avisar. Y si usas el coordinador, recuerda que a los clientes se los redirige al puerto de un **mundo**, así que hay que abrir también ese rango.

## Dónde alojarlo

### En tu propia PC, para tu red local

Corre el servidor y dales a tus amigos tu **IP local**:

```lua
local net = game:GetService("NetworkService")
print("Conéctense a:", net:GetLocalIP())   -- primera IPv4 que no es loopback, p.ej. 192.168.1.35
```

Ellos se conectan con `--glconnect 192.168.1.35:25565`. No hay que abrir nada, pero todos tienen que estar en la misma red.

### En tu propia PC, por internet

1. Dale a la PC que hostea una IP local fija (reserva DHCP en el router).
2. Redirige el puerto **UDP 25565** del router hacia esa IP local.
3. Permite el puerto en el firewall del sistema (en Windows normalmente lo pregunta la primera vez — di sí para redes privadas *y* públicas si quieres gente de fuera).
4. Comparte tu IP **pública** (cualquier web tipo "cuál es mi IP" te la dice).

Dos muros habituales: tu IP pública cambia con el tiempo (usa un nombre de DNS dinámico) y muchos proveedores te ponen detrás de **CGNAT**, donde el port forwarding no puede funcionar. Si el puerto está bien redirigido y aun así nadie entra desde fuera, CGNAT es la causa típica — y la solución es un VPS.

### En otra PC de tu casa

Igual que arriba pero dejas tu máquina libre. Copia el juego exportado a esa PC, corre el lanzador ahí y conéctate a su IP local.

### En un VPS (la opción fiable)

1. Alquila cualquier VPS Linux pequeño (un mundo con pocos jugadores necesita muy poco).
2. Sube la build de Linux exportada, o el proyecto más un Godot headless. En los dos casos el `.so` de GodotLuau para Linux tiene que estar en `GodotLuau/bin/` — ver [Correr en Linux](#correr-en-linux).
3. Abre el UDP 25565 en el firewall/grupo de seguridad del proveedor **y** en el firewall del VPS (`ufw allow 25565/udp` en Ubuntu, por ejemplo).
4. Corre el mismo comando. Usa `screen`, `tmux` o un servicio systemd para que sobreviva cuando cierres la sesión.
5. Comparte la IP pública del VPS.

**El cross-play Windows ↔ Linux funciona directo** — ENet es multiplataforma, así que un servidor de Linux acepta clientes de Windows y al revés, sin cambiar nada.

## Mundos automáticos con matchmaking (coordinador)

En vez de un solo mundo compartido, puedes correr un **coordinador**: cuando entra un jugador lo manda a un mundo con espacio, y si todos están llenos **crea uno nuevo**, respetando un máximo por mundo — como Roblox reparte gente entre servidores. Cada mundo es un proceso aparte, totalmente independiente.

### El dueño lo enciende y lo deja prendido

```bat
MiJuego.exe --headless -- --glhost 25565 --glmax 8
```

- `--glhost 25565` — coordinador en el puerto 25565. Él no aloja ningún mundo y no tiene personaje.
- `--glmax 8` — como máximo 8 jugadores por mundo.
- Los mundos se lanzan como procesos aparte en `25566`, `25567`, … con `--glserver <puerto> --glinstance`.
- La primera vez crea `user://host.key` (una llave de 24 caracteres) y la imprime. Lee [Seguridad](#seguridad-qué-está-protegido-y-qué-no) antes de dar por hecho que eso protege algo.

En consola se ve algo así:

```
[GodotLuau] COORDINADOR activo en el puerto 25565 (máx 8 por mundo). host.key = ...
[GodotLuau] Coordinador: nuevo MUNDO en el puerto 25566 (pid 12345)
```

### Los jugadores entran sin que se les pida una IP

Antes de exportar, crea el archivo `res://gl_match.cfg` con una sola línea: la dirección de tu coordinador.

```
203.0.113.5:25565
```

Al abrir el juego exportado se conecta al coordinador, recibe la asignación de un mundo y se reconecta ahí solo (reintenta hasta 40 veces, una cada 0,6 s, porque un mundo recién lanzado tarda un momento en abrir su puerto).

Dos cosas con las que la gente se tropieza:

- `gl_match.cfg` se lee **solo en una build exportada**. En el editor lo que se hace es la prueba de multijugador local.
- Godot no empaqueta cualquier archivo que no sea un recurso. Añade `*.cfg` en *Exportar → Recursos → "Filtros para exportar archivos que no son recursos"*, o el archivo no estará dentro del `.pck` y no habrá auto-join.

Para probarlo sin exportar:

```bat
MiJuego.exe --glmatch 203.0.113.5:25565
```

### Límites honestos

- **El coordinador y sus mundos tienen que estar en la misma máquina.** El coordinador encuentra los mundos leyendo archivos de latido (`user://gl_inst_<puerto>.json`, escritos una vez por segundo, se dan por muertos a los 6 s) y crea los nuevos como procesos locales. No hay clúster de varias máquinas.
- **Todos los puertos de mundo tienen que ser accesibles**, no solo el del coordinador (ver [Puertos](#puertos-qué-abrir-y-por-qué)).
- **`TeleportService` mueve jugadores entre mundos** de esa misma máquina usando esos mismos archivos de latido. Sin coordinador corriendo no hay a dónde teletransportarse.

## Lista de servidores guardados

Los clientes pueden guardar una lista de servidores como la de Minecraft, desde Luau:

```lua
local net = game:GetService("NetworkService")

net:AddServer("Mi VPS", "203.0.113.5", 25565)
net:AddServer("PC de Ana", "192.168.1.20", 25565)

for _, s in ipairs(net:GetServers()) do
    print(s.Name, s.IP, s.Port)
end

net:JoinServer("Mi VPS")          -- conectar por nombre
net:RemoveServer("PC de Ana")
```

La lista se guarda en `user://gl_servers.json` como `[{ "name", "ip", "port" }]`. Si añades un nombre que ya existe, se reemplaza.

## Dónde se guardan los datos

Todo lo que el servidor persiste va a la carpeta `user://` de Godot **en la máquina del servidor**:

| Archivo | Qué guarda |
|---|---|
| `user://ds_<Store>_<clave>.json` | Datos de `DataStoreService` — un JSON por store y clave |
| `user://host.key` | La llave del dueño que crea el coordinador |
| `user://gl_servers.json` | La lista de servidores guardados (lado cliente) |
| `user://gl_inst_<puerto>.json` | Latido de cada mundo: puerto, número de jugadores, marca de tiempo |
| `user://gl_settings_player<N>.cfg` | Ajustes por jugador del menú de Esc |

Ubicación típica de `user://`: `%APPDATA%\Godot\app_userdata\<nombre del proyecto>` en Windows, `~/.local/share/godot/app_userdata/<nombre del proyecto>` en Linux.

Cosas que conviene saber antes de confiarle datos:

- **`DataStoreService` es solo del servidor.** Llamarlo desde un cliente lanza `… can only be called from the server (DataStore is not accessible to clients)`. Es a propósito: todas las ventanas de un proyecto comparten `user://`, así que una escritura del cliente pisaría los archivos del servidor.
- **`GetAsync` lee siempre el archivo**, no una caché en memoria vieja, así que un mundo que arranca después ve lo que otro mundo acaba de guardar.
- **Todos los mundos de una máquina comparten esta carpeta**, que es justo lo que hace que un inventario compartido entre mundos funcione.
- **Esto es JSON local, no almacenamiento en la nube.** Haz tus propias copias de seguridad. En esta versión no hay DataStore en la nube.

## Correr en Linux

GodotLuau es una extensión C++, así que cada sistema necesita su propio binario. El paquete de release trae las `.dll` de **Windows**. Para **Linux**, compila el `.so` una sola vez en esa máquina Linux:

```bash
git clone --recursive https://github.com/Pimpoli/GodotLuau.git
cd GodotLuau
sudo apt install scons g++ python3-dev      # si no los tienes
scons platform=linux target=template_debug
scons platform=linux target=template_release
# genera GodotLuau/bin/godot_luau.linux.template_debug.x86_64.so (+ release)
```

Copia esos `.so` al `GodotLuau/bin/` de **tu** proyecto en Linux (junto a las `.dll`) y Godot cargará la extensión ahí; el `godot_luau.gdextension` ya apunta a ellos. Después arranca el servidor con `bash start_server.sh` o con el comando `--glserver` de arriba.

## Referencia completa de argumentos

Todo lo que va después del `--` solo son argumentos *de usuario*. Los valores por defecto son los que usa el código de verdad.

| Argumento | Qué hace | Por defecto |
|---|---|---|
| `--glserver <puerto>` | Correr como **servidor dedicado** en ese puerto (sin personaje local, máximo 64 jugadores) | — |
| `--glconnect <ip>[:<puerto>]` | Correr como **cliente** que se conecta a esa dirección | puerto `25565` |
| `--glhost <puerto>` | Correr como **coordinador** | — |
| `--glmax <n>` | Máximo de jugadores por mundo | `8` |
| `--glinstance` | Marca este servidor dedicado como **mundo** de un coordinador (publica su latido) | apagado |
| `--glmatch <ip>[:<puerto>]` | Entrar **a través de un coordinador**, que te asigna un mundo | puerto `25565` |
| `--glsecret <llave>` | Darle la llave del dueño a un proceso servidor en vez de leer `user://host.key` | — |
| `--gldevice PC\|Mobile\|Console\|VR` | Dispositivo emulado (tamaño de ventana, controles táctiles, vista VR partida) | `PC` |
| `--glindex <n>` / `--glcount <n>` | Interno: la prueba local del editor (índice 1 = servidor, 2..N+1 = `Player1..N`) | — |

> ⚠️ **`--glhost` recibe un PUERTO, no una IP.** Algunos comentarios del código describen `--glhost <ip>` como forma de apuntar las ventanas cliente de la prueba local a otra máquina, pero el argumento se interpreta como el puerto del coordinador y esa rama corre primero. Para "apuntar la prueba local a otra PC", usa el archivo `res://.gl_host` con la IP de esa máquina.

Los flags propios de Godot siguen valiendo, y van **antes** del `--`: `--headless` (sin ventana), `--path <carpeta>` (correr una carpeta de proyecto), `--verbose`.

## API de red para Luau

```lua
local net = game:GetService("NetworkService")

-- Hostear / unirse
net:StartServer(puerto?, maxJugadores?)  -- servidor con jugador, el host conserva personaje (25565, 32)
net:Connect(ipOHostname, puerto?)        -- unirse a un servidor (25565)
net:JoinServer(nombre)                   -- unirse a un servidor guardado por nombre
net:Disconnect()                         -- volver a un jugador
net:StartHost(puerto?, maxPorMundo?)     -- coordinador (25565, 8)
net:JoinMatchmaking(ip, puerto?)         -- entrar a través de un coordinador (25565)

-- Lista de servidores
net:AddServer(nombre, ip, puerto?) / net:RemoveServer(nombre) / net:GetServers()

-- Estado
net:IsServer() / net:IsClient() / net:IsConnected()
net:IsDedicatedServer()                  -- true si este proceso es un servidor dedicado
net:GetConnectionState()                 -- "Server" | "Connecting" | "Connected" | "Disconnected"
net:GetServerAddress()                   -- "ip:puerto" al que te conectaste ("" si host/un jugador)
net:GetLocalIP()                         -- tu IPv4 de LAN, para dársela a tus amigos
net:GetPeerId() / net:GetPlayerCount() / net:GetPlayerIndex() / net:GetDevice()
net:GetHostKey() / net:GenerateHostKey()

-- Señales
net.PlayerConnected:Connect(function(peerId) end)
net.PlayerDisconnected:Connect(function(peerId) end)
net.Connected:Connect(function() end)
net.ConnectionFailed:Connect(function() end)
```

Un patrón útil para un script de servidor que debe comportarse distinto cuando es dedicado:

```lua
local net = game:GetService("NetworkService")

if net:IsDedicatedServer() then
    print("Servidor dedicado arriba — aquí no hay jugador local")
end
```

## Cuando no conecta

Ve de dentro hacia fuera: primero `127.0.0.1`, luego LAN, luego internet. Cada paso descarta una familia entera de problemas.

| Síntoma | Causa probable y arreglo |
|---|---|
| `No se pudo abrir el servidor dedicado en el puerto N` | El puerto ya está en uso (¿otro servidor sigue corriendo?) o bloqueado. Cambia de puerto o cierra el otro proceso. |
| El servidor arranca pero no se ve ningún personaje | Es correcto: un servidor dedicado no tiene jugador local. Quita `--headless` para mirar con la cámara libre. |
| El cliente se queda en `"Connecting"` | IP/puerto mal, servidor apagado, UDP sin redirigir, o firewall. Prueba `127.0.0.1` en la máquina del servidor para demostrar que el servidor en sí está bien. |
| `No se pudo resolver el host X` | El hostname no resuelve. Usa una IP directamente. |
| `No se pudo conectar a X` | La dirección no es alcanzable — IP mal, puerto mal, o nadie escuchando. |
| Funciona en LAN pero no desde fuera | Redirige el puerto como **UDP**, usa la IP **pública**, comprueba si hay CGNAT. |
| Conecta al coordinador y se queda colgado | El puerto del mundo asignado no es accesible. Redirige `puerto del coordinador + 1…N`. A los ~24 s de reintentos sale además `No se pudo entrar al mundo asignado por el coordinador.` |
| El juego exportado no hace auto-join | `gl_match.cfg` no se empaquetó. Añade `*.cfg` al filtro de exportación de archivos que no son recursos. |
| Godot se queja de `--glserver` | Te faltó el `--` solo antes de los argumentos de GodotLuau. |
| No sé quién es servidor y quién cliente | Activa el **Modo Debug** en el panel GodotLuau Config y lee las líneas `[GodotLuau MP]` (roles, quién hostea, TX/RX). |
| Los jugadores se ven teletransportándose | Revisa los FPS del servidor y la calidad de la conexión; las posiciones se aplican tal cual llegan, sin suavizado. |

## Seguridad: qué está protegido y qué no

Lee esto antes de abrir un puerto a internet.

- **`host.key` todavía no es autenticación.** El coordinador la crea si falta, la imprime y se la pasa a los mundos que lanza — pero **ninguna conexión entrante se valida contra ella** en el código actual. Lo único que impide de verdad es que el coordinador arranque si el archivo no se puede crear o leer. No la trates como una contraseña.
- **Un servidor dedicado no pide ninguna llave.** Cualquiera que tenga tu juego exportado puede correr `--glserver`.
- **Lo que sí protege a tus jugadores es la dirección que distribuyes.** Un cliente construido con `gl_match.cfg` solo se conecta a *tu* coordinador, así que nadie puede llevarse a tus jugadores a un servidor modificado solo por encender uno.
- **Nunca le pidas una contraseña a un jugador dentro del juego.** GodotLuau no tiene sistema de cuentas, ni guarda contraseñas, ni tiene forma de verificarlas. Si un "servidor" pide la contraseña de Roblox es una estafa — y tú tampoco deberías construir eso.
- **Valida en el servidor.** Los clientes no ejecutan `ServerScript`s y no pueden tocar `DataStore`, pero cualquier cliente puede mandar cualquier payload por `RemoteEvent` con cualquier valor. Compruébalo en el servidor, igual que harías en Roblox.
- **`ServerStorage` / `ServerScriptService` se vacían en el cliente**, así que Luau ahí no puede leerlos. Aviso honesto: los datos siguen dentro del `.pck` del cliente; esconderlos de verdad exige no empaquetarlos en la build de cliente, y eso lo tendría que hacer el exportador.
- **Haz copias de `user://`.** Los datos de los jugadores son archivos JSON planos en la máquina del servidor, sin replicación ni instantáneas.
