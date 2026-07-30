# GodotLuau

**Escribe juegos estilo Roblox en Luau y córrelos en Godot 4.**

GodotLuau es una GDExtension en C++ que mete el intérprete [Luau](https://luau.org/) dentro de Godot y reimplementa la arquitectura de Roblox encima: `game`, los servicios (`Workspace`, `Players`, `Lighting`, `ReplicatedStorage`…), `LocalScript` / `ServerScript` / `ModuleScript`, `Humanoid`, el avatar R6, los `RemoteEvent`, la librería `task`, la GUI estilo Roblox y red de verdad (LAN, internet y un coordinador de mundos estilo Roblox).

**Autor:** PimpoliDev · **Repo:** https://github.com/Pimpoli/GodotLuau · **Licencia:** MIT

### 🌐 Read this in English → **[README.md](README.md)** (same information, in English)

> **Aviso:** GodotLuau es un proyecto independiente. **No** está afiliado, avalado ni patrocinado por Roblox Corporation ni por la Godot Foundation. "Roblox" es una marca de Roblox Corporation y aquí se menciona solo para describir la compatibilidad de la API. GodotLuau no se conecta a los servidores de Roblox y no incluye assets de Roblox.

---

## Índice

- [Qué es (y qué no es)](#qué-es-y-qué-no-es)
- [Qué funciona hoy](#qué-funciona-hoy)
- [Instalación](#instalación)
- [Tu primer proyecto, paso a paso](#tu-primer-proyecto-paso-a-paso)
- [Escribir scripts en Luau](#escribir-scripts-en-luau)
- [Importar lugares de Roblox (.rbxl)](#importar-lugares-de-roblox-rbxl)
- [Modo online / multijugador](#modo-online--multijugador)
  - [Las cuatro formas de jugar juntos](#las-cuatro-formas-de-jugar-juntos)
  - [1. Prueba local desde el editor](#1-prueba-local-desde-el-editor)
  - [2. Servidor dedicado (LAN o internet)](#2-servidor-dedicado-lan-o-internet)
  - [3. Coordinador + mundos (matchmaking estilo Roblox)](#3-coordinador--mundos-matchmaking-estilo-roblox)
  - [4. Servidor con jugador desde Luau](#4-servidor-con-jugador-desde-luau)
  - [Referencia de argumentos](#referencia-de-argumentos)
  - [API de red para Luau](#api-de-red-para-luau)
  - [Qué se replica y qué no](#qué-se-replica-y-qué-no)
  - [Archivos que crea la red](#archivos-que-crea-la-red)
  - [Seguridad: léelo antes de publicar](#seguridad-léelo-antes-de-publicar)
  - [Cuando no conecta (diagnóstico)](#cuando-no-conecta-diagnóstico)
  - [Backend opcional (Nakama)](#backend-opcional-nakama)
- [Cómo funciona por dentro (arquitectura)](#cómo-funciona-por-dentro-arquitectura)
- [Catálogo de nodos y clases](#catálogo-de-nodos-y-clases)
- [Iluminación](#iluminación)
- [Autocompletado e IA](#autocompletado-e-ia)
- [Panel de configuración](#panel-de-configuración)
- [Compilar desde el código fuente](#compilar-desde-el-código-fuente)
- [Estructura del repositorio](#estructura-del-repositorio)
- [Herramientas incluidas](#herramientas-incluidas)
- [Limitaciones conocidas](#limitaciones-conocidas)
- [Publicar una versión (mantenedor)](#publicar-una-versión-mantenedor)
- [Licencia y créditos](#licencia-y-créditos)

---

## Qué es (y qué no es)

**Es** una reimplementación independiente de la parte más usada de la API de Roblox sobre dos tecnologías abiertas (Godot + Luau). Escribes la lógica como ya sabes —`game.Players.PlayerAdded`, `workspace.Part.Touched`, `RemoteEvent:FireServer()`, `task.wait()`— y Godot se encarga del render, la física, la exportación y las plataformas.

**No es:**

- un emulador ni un cliente de Roblox: nunca habla con los servidores de Roblox;
- una forma de correr tu juego de Roblox "tal cual": la API cubierta es grande pero no está completa;
- una forma de bajar assets de Roblox: las mallas, texturas y sonidos viven en los servidores de Roblox y no están dentro del archivo `.rbxl`.

## Qué funciona hoy

| Área | Estado |
|---|---|
| Intérprete Luau (Luau oficial, compilado dentro de la extensión) | ✅ funciona |
| Árbol de servicios, `LocalScript` / `ServerScript` / `ModuleScript`, `require` | ✅ funciona |
| Librería `task`, `RunService.Heartbeat`, señales estilo Roblox (`:Connect`, `:Wait`) | ✅ funciona |
| Tipos de dato (`Vector3`, `CFrame`, `Color3`, `UDim2`, `BrickColor`, `Enum`…) | ✅ funciona |
| Mensajes de error idénticos a los de Roblox, con bloque de stack | ✅ funciona |
| Avatar R6 + `StarterCharacter`, `Humanoid`, constraints, BodyMovers | ✅ funciona |
| GUI estilo Roblox, `BillboardGui`, `SurfaceGui`, menú de ajustes en juego (Esc) | ✅ funciona |
| Importar lugares `.rbxl` (formato binario) | ✅ funciona (los assets por id no se pueden bajar) |
| Prueba de multijugador local (N ventanas + Server View) | ✅ funciona |
| Red real: servidor dedicado, Direct Connect, `RemoteEvent` por red, replicación | ✅ funciona |
| Coordinador con mundos automáticos (matchmaking estilo Roblox) | ⚠️ funciona, solo en una máquina — ver [límites](#3-coordinador--mundos-matchmaking-estilo-roblox) |
| `HttpService` (HTTP/HTTPS real, bloqueante) | ✅ funciona |
| `DataStoreService` | ⚠️ solo archivos JSON locales, sin backend en la nube |
| Autenticación de servidor / anti-cheat / cuentas | ❌ no existe — ver [Seguridad](#seguridad-léelo-antes-de-publicar) |
| Binarios de macOS | ❌ hay que compilar desde el fuente |

## Instalación

**Requisitos**

- **Godot 4.3 o más nuevo** (`compatibility_minimum = "4.3"`; desarrollado contra Godot 4.6/4.7).
- **Windows x86_64** o **Linux x86_64**. El paquete de release trae los binarios de los dos; cualquier otra plataforma necesita [compilar desde el fuente](#compilar-desde-el-código-fuente).
- Python 3 solo si quieres usar el asistente de proyectos de `tools/`.

**Opción A — el asistente (lo recomendado para un juego nuevo)**

```bash
python tools/nuevo_proyecto.py
```

Pregunta nombre, modo (3D o 2D) y carpeta, y escribe un proyecto de Godot completo: la extensión compilada en `GodotLuau/bin/`, su `godot_luau.gdextension`, los iconos estilo Roblox, el addon **GodotLuau Config** ya activado y una escena raíz `RobloxGame`.

**Opción B — añadirlo a un proyecto que ya tienes**

1. Descarga el paquete de release de tu plataforma y descomprímelo **en la raíz del proyecto**. Trae `godot_luau.gdextension`, `GodotLuau/` (binarios, iconos, assets, shaders, scripts por defecto) y `addons/GodotLuauUpdater/`.
2. Abre el proyecto. Godot carga la extensión al arrancar (si ya estaba abierto, reinícialo una vez).
3. Activa **GodotLuau Config** en *Proyecto → Ajustes del proyecto → Plugins* si no está activo.
4. Pon un nodo **`RobloxGame3D`** (o `RobloxGame2D`) como raíz de la escena y guarda.

A partir de ahí el auto-updater integrado puede traer versiones nuevas de GitHub (verifica el SHA-256 antes de instalar).

## Tu primer proyecto, paso a paso

1. **Abre el proyecto.** El nodo raíz `RobloxGame3D` **construye solo toda la jerarquía de servicios** la primera vez que se abre la escena, así que ya ves `Workspace` (con un Baseplate con la cuadrícula tintable), `Players`, `Lighting`, `ServerScriptService`, `StarterPlayer/StarterPlayerScripts` y el resto.
2. **Añade un script.** Clic derecho en `ServerScriptService` → *Añadir nodo hijo* → `ServerScript`. El nodo crea su propio `.lua` en `res://GodotLuau/ServerScripts/`, editable dentro de Godot con los colores de Roblox Studio.
3. **Escribe Luau:**

   ```lua
   local Players = game:GetService("Players")

   Players.PlayerAdded:Connect(function(player)
       print(player.Name .. " entró")
   end)

   local part = Instance.new("Part")
   part.Size = Vector3.new(8, 1, 8)
   part.Position = Vector3.new(0, 10, 0)
   part.BrickColor = BrickColor.new("Bright red")
   part.Parent = workspace
   ```
4. **Pulsa F5.** El script corre de arriba abajo, igual que en Roblox.
5. **Pulsa Esc** en el juego para el menú estilo Roblox (lista de jugadores con ping, calidad gráfica, FPS máximos, contadores de FPS/ping, volumen, sensibilidad de cámara). Se guarda por jugador en `user://gl_settings_player<N>.cfg`.

## Escribir scripts en Luau

| Nodo | Color | Contexto | Cuándo corre |
|---|---|---|---|
| `LocalScript` | Azul | **Cliente** (`UserInputService` disponible) | Solo, al arrancar el juego |
| `ServerScript` | Naranja | **Servidor** (`UserInputService` bloqueado) | Solo, al arrancar el juego |
| `ModuleScript` | Morado | Neutro | **Nunca solo**: se carga con `require()` y devuelve su valor |

Lo importante, sobre todo si vienes de GDScript:

- **No hay `_ready` ni `_process`.** El cuerpo del `.lua` corre de arriba abajo. Para lógica continua usa `RunService.Heartbeat:Connect(...)` o un bucle con `task.wait()`.
- **Propiedad `Enabled`**, como en Studio: un script desactivado no corre; si lo reactivas en runtime, corre en ese momento.
- **Cada `LocalScript`/`ServerScript` tiene su propia VM de Luau.** Un `ModuleScript` requerido desde dos scripts **no** comparte estado entre ellos (esta es una diferencia real con Roblox).
- **Globales disponibles:** `game`, `workspace`, `script`, `require`, `shared`, las librerías estándar de Luau (`math`, `string`, `table`, `coroutine`, `os`, `utf8`, `bit32`…), `print`, `warn`, `task.*` y los clásicos `wait` / `spawn` / `delay` / `tick` / `time`.
- **Los errores se leen como los de Roblox:**

  ```
  ServerScriptService.Server:5: attempt to index nil with 'Name'
  Stack Begin
  Script 'ServerScriptService.Server', Line 5 - function doStuff
  Stack End
  ```

  Indexar algo que no existe lanza `X is not a valid member of …`; `WaitForChild` suspende la corrutina de verdad y avisa `Infinite yield possible on '…'` a los 5 segundos; `require` reproduce los mensajes exactos de Roblox. Ningún camino de callback falla en silencio.
- **La identidad de los scripts es estable.** Cada nodo de script tiene un `script_id` inmutable (por ejemplo `ServerScript_ID_3`) que da nombre a su `.lua`. Borrar el nodo manda el archivo a `res://.luau_trash/` (**Ctrl+Z** restaura nodo y archivo juntos; la basura se purga sola a los 7 días).

## Importar lugares de Roblox (.rbxl)

GodotLuau lee el **formato binario de lugares** de Roblox directamente, así que un lugar guardado desde Studio vuelve a aparecer como árbol completo en Godot.

**Cómo:** en Godot, **Proyecto → Herramientas → "Importar lugar de Roblox (.rbxl)"** y eliges tu `.rbxl`. **Solo 3D** — importar sobre un Workspace 2D se rechaza con un mensaje claro.

La importación va por lotes entre frames (~30 ms por frame) para que el editor no se congele, con una ventana de progreso que muestra la fase y qué está procesando (incluido `Script 45 de 202: MiScript`). Tres etapas: **Importando** (instancias, propiedades, jerarquía, referencias cruzadas) → **Comprobado** (se verifica que cada instancia del archivo existe y está en el árbol, así nada se salta en silencio) → **Recolocado** (el resultado se mezcla con la estructura Roblox que ya hay en tu escena: tu `Workspace`, `Players`, `Lighting`… **se reutilizan, no se duplican**).

Desde código:

```gdscript
var imp = RBXImporter.new()
var root = imp.ImportFile("C:/ruta/MiLugar.rbxl")   # bloqueante
print(imp.GetReport())
```

O paso a paso, para llevar tu propia barra de progreso:

```gdscript
imp.Begin(path)
imp.SetTarget(mi_nodo_game)        # opcional: mezclar con un Game que ya existe
while not imp.IsDone() and not imp.HasFailed():
    imp.Step()
    print(imp.GetPhaseName(), imp.GetProgress(), imp.GetCurrentItem())
```

**Qué entra:** el árbol de instancias completo tal como lo muestra el Explorer — nombres, padres, orden de hijos, tamaños, `CFrame`, colores (incluido `BrickColor` y el enum `Material`), layout de GUI, **el código Luau de cada script**, **tags de CollectionService**, **atributos de instancia** y referencias entre instancias como `PrimaryPart` o `Part0`/`Part1`. Las clases que GodotLuau todavía no implementa llegan como `RBXInstance`, conservando el nombre de clase, la jerarquía y todas las propiedades como metadatos — **no se descarta nada**, así que los scripts que recorren el árbol siguen encontrando lo que esperan.

**Qué no puede entrar:** mallas, texturas y sonidos **no están en el archivo**; son punteros `rbxassetid://` a los servidores de Roblox. El importador guarda todos los ids (en el nodo y en el informe) para que exportes el modelo desde Studio tú mismo. Los DataStores, gamepasses y la configuración del place también viven en la nube. `CustomPhysicalProperties` y el tipo nuevo `Content` se omiten. Los lugares XML (`.rbxlx`) todavía no se leen — guarda en `.rbxl` binario desde Studio.

> Leer tus propios archivos es interoperabilidad, lo mismo que hace [rbx-dom](https://github.com/rojo-rbx/rbx-dom).

---

# Modo online / multijugador

La red es **real** y va sobre la capa ENet (UDP) de Godot, así que es multiplataforma: un servidor de Linux acepta clientes de Windows y al revés, sin tocar nada. **No hay relay, ni login, ni servicio oficial de matchmaking**: tú alojas el servidor, como se hace con un servidor de Minecraft.

**Vocabulario que se usa abajo**

| Término | Qué es |
|---|---|
| **Cliente** | El juego de un jugador. No ejecuta `ServerScript`s: recibe el mundo del servidor por replicación, igual que en Roblox. |
| **Servidor con jugador (host)** | Una máquina que juega *y* sirve a la vez. |
| **Servidor dedicado** | Un servidor puro: corre tus `ServerScript`s y acepta jugadores, **sin personaje local** (como `minecraft_server.jar`). |
| **Coordinador** | Un servidor liviano que no aloja mundo; manda a cada jugador que llega a un **mundo** con espacio, y crea uno nuevo cuando están todos llenos. |
| **Mundo (instancia)** | Un proceso de servidor dedicado aparte, lanzado por el coordinador, en su propio puerto. |

## Las cuatro formas de jugar juntos

| | Para qué | Quién lo corre | Dirección |
|---|---|---|---|
| **1. Prueba local** | Desarrollar y depurar | El botón Play del editor | `127.0.0.1:25575` (automático) |
| **2. Servidor dedicado** | Jugar con amigos, un mundo | Tú (PC o VPS) | `tu-ip:25565` (tú eliges) |
| **3. Coordinador + mundos** | Muchos mundos en paralelo, estilo Roblox | Tú (PC o VPS) | `tu-ip:25565` horneado en el cliente |
| **4. Servidor con jugador** | Una partida rápida desde tu propia UI | Cualquier jugador, desde Luau | `net:StartServer(...)` |

---

## 1. Prueba local desde el editor

La barra del editor tiene un selector de **Players (1–8)** y un selector de **dispositivo** (PC / Mobile / Console / vista previa VR).

1. Pon **Players** en 2 o más.
2. Pulsa el botón **Play** nativo.

GodotLuau abre una ventana por jugador (`Player1`…`PlayerN`) **más una ventana Server View** — una cámara libre sobre el mundo (WASD mueve, clic derecho mira, la rueda cambia la velocidad, E/Espacio sube, Q/Ctrl baja). Las ventanas se acomodan en mosaico y se titulan solas. Posiciones, rotaciones, estados de animación y el chat se replican en tiempo real.

Detalles que importan mientras depuras:

- La prueba local siempre usa el puerto **25575** en `127.0.0.1`. Lo elige quien lo abre primero: si el puerto está ocupado, esa ventana entra como cliente, así que la sesión siempre se forma.
- La **ventana del servidor no tiene personaje** (igual que en Roblox Studio) y no se puede volver vista de cliente.
- Con 1 jugador sigues teniendo el botón Server View en el panel Game.
- Para que las ventanas cliente apunten a **otra máquina** en vez de a `127.0.0.1`, crea el archivo `res://.gl_host` con la IP de esa máquina como único contenido.
- Activa el **Modo Debug** en el panel GodotLuau Config para ver el log interno `[GodotLuau MP]` (roles, quién hostea, TX/RX).

## 2. Servidor dedicado (LAN o internet)

Este es el modo que quieres para "poner mi juego online". Un servidor dedicado es **tu propio juego exportado** arrancado con un argumento extra.

### Paso 1 — arrancar el servidor

Desde tu juego **exportado**:

```bat
MiJuego.exe --headless -- --glserver 25565
```

- `--headless` = sin ventana (opcional; quítalo para ver el mundo con la cámara libre).
- `--glserver 25565` = escucha en el puerto UDP **25565**, hasta **64** jugadores, **sin personaje local**.
- Fíjate en el `--` solo: todo lo que va después son argumentos *de usuario*, que es como Godot se los pasa a la extensión.

En Linux:

```bash
./MiJuego.x86_64 --headless -- --glserver 25565
```

En `GodotLuau/DedicatedServer/` hay lanzadores listos (`start_server.bat`, `start_server.sh`): cambia el nombre del ejecutable y doble clic. Esa carpeta tiene además su propia guía detallada en los dos idiomas:
**[README - Espanol.md](GodotLuau/DedicatedServer/README%20-%20Espanol.md)** · **[README - English.md](GodotLuau/DedicatedServer/README%20-%20English.md)**.

También puedes servir directamente desde el editor, que va bien para probar antes de exportar:

```bat
Godot_v4.x.exe --path "C:\ruta\a\tu\proyecto" --headless -- --glserver 25565
```

Cuando arranca verás:

```
[GodotLuau] SERVIDOR DEDICADO escuchando en el puerto 25565 (sin jugador local).
```

### Paso 2 — conectar un cliente

Por línea de comandos:

```bat
MiJuego.exe --glconnect 203.0.113.5:25565
```

(si omites `:puerto` usa 25565; el cliente reintenta unas 20 veces, una cada 1,5 s, así que puedes lanzarlo antes de que el servidor esté listo)

o desde tu propia UI, en Luau:

```lua
local net = game:GetService("NetworkService")
net:Connect("203.0.113.5", 25565)
print(net:GetConnectionState())   -- "Connecting" → "Connected"
```

o desde una lista de servidores guardada, estilo Minecraft:

```lua
local net = game:GetService("NetworkService")
net:AddServer("Mi VPS", "203.0.113.5", 25565)      -- se guarda en user://gl_servers.json
net:AddServer("PC de Ana", "192.168.1.20", 25565)
for _, s in ipairs(net:GetServers()) do print(s.Name, s.IP, s.Port) end
net:JoinServer("Mi VPS")
net:RemoveServer("PC de Ana")
```

### Paso 3 — elegir dónde se aloja

- **El mismo PC (prueba):** conéctate a `127.0.0.1:25565`.
- **Tu red local (amigos en casa):** corre el servidor en tu PC y dales tu IP local. `net:GetLocalIP()` te la dice (la primera IPv4 que no sea loopback).
- **Por internet, desde tu PC:** redirige el puerto **UDP 25565** en el router hacia la máquina que hostea, permítelo en el firewall del sistema y comparte tu IP pública. Si tu proveedor usa CGNAT el port forwarding no funcionará y necesitarás un VPS.
- **En un VPS:** sube el juego exportado (o el proyecto + un Godot headless) y corre el mismo comando; comparte la IP pública del VPS. En Linux hay que compilar el `.so` una vez en esa máquina — ver [compilar desde el fuente](#compilar-desde-el-código-fuente).

## 3. Coordinador + mundos (matchmaking estilo Roblox)

En vez de un solo mundo compartido, corre un **coordinador**: cuando entra un jugador, lo manda a un mundo con espacio, y si todos están llenos **crea uno nuevo**, respetando un máximo por mundo — igual que Roblox reparte gente entre servidores.

### El dueño enciende el coordinador y lo deja prendido

```bat
MiJuego.exe --headless -- --glhost 25565 --glmax 8
```

- `--glhost 25565` = coordinador en el puerto 25565 (él no aloja ningún mundo).
- `--glmax 8` = como máximo 8 jugadores por mundo.
- Los mundos se lanzan como **procesos aparte** en `puerto_del_coordinador + 1`, `+2`, `+3`… (o sea 25566, 25567, …) con `--glserver <puerto> --glinstance`.
- La primera vez crea `user://host.key` y la imprime. Lee [Seguridad](#seguridad-léelo-antes-de-publicar) antes de dar por hecho que eso protege algo.

### Los jugadores entran sin escribir ninguna IP

Antes de exportar, crea el archivo `res://gl_match.cfg` con una sola línea: la dirección de tu coordinador.

```
203.0.113.5:25565
```

Al abrir el juego exportado, se conecta solo al coordinador, este le asigna un mundo y el cliente se reconecta allí. Sin escribir IPs, como en Roblox.

> Dos avisos prácticos: `gl_match.cfg` solo se lee en una build **exportada** (en el editor se hace la prueba local), y Godot solo empaqueta los archivos que no son recursos si se lo dices, así que añade `*.cfg` en *Exportar → Recursos → "Filtros para exportar archivos que no son recursos"* o el archivo no estará dentro del `.pck`.

Para probar lo mismo sin exportar:

```bat
MiJuego.exe --glmatch 203.0.113.5:25565
```

Desde Luau:

```lua
local net = game:GetService("NetworkService")
net:StartHost(25565, 8)                       -- correr el coordinador (dueño)
net:JoinMatchmaking("203.0.113.5", 25565)     -- entrar a través de él (jugador)
net:GetHostKey()                              -- leer user://host.key ("" si no hay)
net:GenerateHostKey()                         -- leerla, creándola si falta
```

### Límites honestos de este modo

- **El coordinador y sus mundos tienen que estar en la misma máquina.** El coordinador descubre los mundos leyendo archivos de latido (`user://gl_inst_<puerto>.json`, refrescados una vez por segundo, se dan por muertos a los 6 s) y crea mundos nuevos lanzando un proceso local. No hay clúster de varias máquinas.
- **Los puertos de cada mundo también tienen que ser accesibles.** El cliente se redirige a la IP del coordinador pero al puerto del *mundo*, así que detrás de un router hay que redirigir el puerto del coordinador **y** el rango de puertos de mundos que esperes usar (25566, 25567, …).
- **`TeleportService` mueve a un jugador entre mundos** de la misma máquina, usando esos mismos archivos de latido. Sin coordinador corriendo no hay a dónde teletransportarse.
- Todos los mundos de una máquina **comparten `user://`**, así que comparten los archivos de `DataStoreService` (`user://ds_<Store>_<clave>.json`). Eso es lo que hace que un inventario compartido funcione entre mundos — y también significa que una escritura desde un cliente chocaría, razón por la que las llamadas a `DataStore` dan error en el cliente.

## 4. Servidor con jugador desde Luau

Una máquina que juega y sirve a la vez, controlada desde tus propios menús:

```lua
local net = game:GetService("NetworkService")

net:StartServer(25565, 32)      -- hostear conservando tu personaje (puerto y máximo son opcionales)
print("Dale esto a tus amigos:", net:GetLocalIP())

net:Connect("192.168.1.20", 25565)   -- o unirte a otro
net:Disconnect()                     -- volver a un jugador
```

El host es siempre el jugador 1. Los `UserId` los asigna el servidor por orden de entrada (1, 2, 3…) y se replican, así que todas las ventanas coinciden en quién es quién.

## Referencia de argumentos

Todo lo que va después del `--` solo son argumentos *de usuario*. Los valores mostrados son los que usa el código de verdad.

| Argumento | Rol | Por defecto |
|---|---|---|
| `--glserver <puerto>` | Arrancar como **servidor dedicado** en ese puerto (sin personaje local, máximo 64 jugadores) | — |
| `--glconnect <ip>[:<puerto>]` | Arrancar como **cliente** que se conecta a esa dirección | puerto `25565` |
| `--glhost <puerto>` | Arrancar como **coordinador** (`--glmax` fija los jugadores por mundo) | — |
| `--glmax <n>` | Máximo de jugadores por mundo | `8` |
| `--glinstance` | Marca este servidor dedicado como **mundo** de un coordinador (publica su latido) | apagado |
| `--glmatch <ip>[:<puerto>]` | Entrar **a través de un coordinador** (él te asigna un mundo) | puerto `25565` |
| `--glsecret <llave>` | Pasar la llave del dueño a un proceso servidor en vez de leer `user://host.key` | — |
| `--gldevice PC\|Mobile\|Console\|VR` | Dispositivo emulado (tamaño de ventana, controles táctiles, vista VR partida) | `PC` |
| `--glindex <n>` / `--glcount <n>` | Interno, lo usa la prueba local del editor (índice 1 = servidor, 2..N+1 = `Player1..N`) | — |

> ⚠️ **Cuidado con `--glhost`:** los comentarios del código también describen `--glhost <ip>` como forma de apuntar las ventanas cliente de la prueba local a otra máquina, pero `_auto_init` interpreta `--glhost` como **número de puerto** del coordinador, y la rama del coordinador se evalúa primero. Pasarle una IP ahí **no** hará lo que dice el comentario. Para el caso "apuntar la prueba a otro PC" usa el archivo `res://.gl_host`, y trata `--glhost` como "arranca el coordinador en este puerto".

## API de red para Luau

`local net = game:GetService("NetworkService")`

```lua
-- Hostear / unirse
net:StartServer(puerto?, maxJugadores?)   -- servidor con jugador (por defecto 25565, 32)
net:Connect(ipOHostname, puerto?)         -- unirse (los hostnames se resuelven por DNS)
net:Disconnect()                          -- volver a un jugador
net:StartHost(puerto?, maxPorMundo?)      -- coordinador (por defecto 25565, 8)
net:JoinMatchmaking(ip, puerto?)          -- unirse a través de un coordinador

-- Lista de servidores guardada (user://gl_servers.json)
net:AddServer(nombre, ip, puerto?)
net:RemoveServer(nombre)
net:GetServers()                          -- array de { Name, IP, Port }
net:JoinServer(nombre)

-- Estado
net:IsServer() / net:IsClient() / net:IsConnected()
net:IsDedicatedServer()
net:GetConnectionState()                  -- "Server" | "Connecting" | "Connected" | "Disconnected"
net:GetServerAddress()                    -- "ip:puerto" al que te conectaste ("" si host/un jugador)
net:GetLocalIP()                          -- tu IPv4 de LAN, para dársela a tus amigos
net:GetPeerId() / net:GetPlayerCount()
net:GetDevice() / net:GetPlayerIndex()
net:GetHostKey() / net:GenerateHostKey()

-- Señales (estilo Roblox)
net.PlayerConnected:Connect(function(peerId) end)
net.PlayerDisconnected:Connect(function(peerId) end)
net.Connected:Connect(function() end)
net.ConnectionFailed:Connect(function() end)
```

Para el juego normal casi no necesitas este servicio: `Players.PlayerAdded`, `RemoteEvent`, `RemoteFunction` y `Player.Character` ya funcionan por red.

## Qué se replica y qué no

**Se replica automáticamente**

- Posición, rotación y estado de animación de los jugadores, difundidos por su dueño (no se inventa suavizado en el que recibe).
- Los personajes remotos son **personajes de verdad**, no muñecos: las partes R6 conservan sus nombres de Roblox como hijas directas y tienen `Humanoid`, así que `hit.Parent:FindFirstChild("Humanoid")` funciona contra otros jugadores.
- **La salud es autoritativa del servidor:** si un `ServerScript` llama a `hum:TakeDamage(10)`, el servidor le manda el valor al dueño y este lo propaga a todos.
- **`RemoteEvent` y `RemoteFunction` por red**, confiables y no confiables, en los dos sentidos. El payload admite nil/bool/número/string, tablas array/diccionario/mixtas y anidadas, `Vector3`, `Color3`, todos los datatypes tipo tabla (`Vector2`, `CFrame`, `UDim`, `UDim2`, `Rect`, `Region3`, `Ray`, `NumberRange`, `BrickColor`, `NumberSequence`, `ColorSequence`, `PhysicalProperties`, `DateTime`) e Instances (por id de red estable, con la ruta como respaldo). Lo que no se puede enviar lanza un error de Luau en vez de llegar como `nil` en silencio. Los `EnumItem` viajan como su valor numérico.
- **Replicación de instancias**: crear, cambiar propiedad, cambiar de padre y destruir, con ids deterministas para los objetos que venían en el place.
- **Network ownership de las partes no ancladas**: la máquina autoridad simula y difunde transform + velocidad; las demás congelan su copia y la siguen.
- `UserId` secuenciales, equipos, `LoadCharacter`, `Kick` (con motivo), reloj sincronizado, ping de cada cliente legible desde el servidor, y el chat.
- `ServerScriptService` y `ServerStorage` se **vacían en el cliente**, así que un `LocalScript` no puede leerlos.

**No se replica / todavía no está**

- No hay relay ni NAT punch-through: el servidor tiene que ser accesible (LAN, port forwarding o VPS).
- No hay lista oficial de servidores, ni cuentas, ni lista de amigos.
- Vaciar `ServerStorage` en el cliente corta el acceso desde Luau, pero los datos siguen dentro del `.pck` del cliente. Esconderlos de verdad exige no empaquetarlos en la build de cliente, y eso es cosa del exportador.
- `DataStoreService` escribe archivos JSON locales; no hay persistencia en la nube todavía.

## Archivos que crea la red

| Archivo | Quién lo escribe | Para qué |
|---|---|---|
| `user://host.key` | El coordinador (se crea la primera vez) | La llave de 24 caracteres del dueño, impresa al arrancar |
| `user://gl_servers.json` | `net:AddServer` | Lista de servidores guardados (`name`, `ip`, `port`) |
| `user://gl_inst_<puerto>.json` | Cada mundo, una vez por segundo | Latido (`port`, `players`, marca de tiempo); caduca a los 6 s |
| `user://ds_<Store>_<clave>.json` | `DataStoreService` | Datos guardados del lado servidor |
| `user://gl_settings_player<N>.cfg` | Menú de ajustes | Ajustes de gráficos/audio/control por jugador |
| `res://gl_match.cfg` | **Tú**, antes de exportar | Dirección del coordinador para el auto-join |
| `res://.gl_host` | **Tú**, opcional | IP a la que deben apuntar las ventanas cliente de la prueba local |
| `res://.gl_mp_session`, `res://.gl_view_cmd` | El plugin del editor | Sesión de prueba local y toggle de Server View |

En Windows `user://` es normalmente `%APPDATA%\Godot\app_userdata\<nombre del proyecto>`; en Linux `~/.local/share/godot/app_userdata/<nombre del proyecto>`.

## Seguridad: léelo antes de publicar

Sin adornos, porque esta es la parte con la que la gente se hace daño:

- **`host.key` todavía no es autenticación.** El coordinador la crea si no existe, la imprime y se la pasa a los mundos que lanza — pero **ninguna conexión entrante se valida contra ella** en el código actual. No la trates como una contraseña. Lo único que impide de verdad es que el coordinador arranque si no puede crear/leer el archivo.
- **La protección real hoy es la dirección que horneas.** Los clientes construidos con `gl_match.cfg` solo se conectan a *tu* coordinador, así que un desconocido no puede llevarse a tus jugadores a un servidor modificado solo por encender uno. Lo que no impide es que alguien corra su propio servidor para sus propios jugadores.
- **En GodotLuau no hay login ni contraseñas en ninguna parte.** Nunca hagas un sistema que le pida a tus jugadores la contraseña de Roblox, ni ninguna contraseña, dentro de un juego: la extensión no tiene forma de guardarla ni de verificarla.
- **Un servidor dedicado (`--glserver`) no pide ninguna llave.** Cualquiera que tenga tu juego exportado puede levantar uno.
- **Confía en el servidor, no en el cliente.** Un cliente no ejecuta `ServerScript`s y no puede tocar `DataStore`, pero cualquier cliente puede mandar cualquier payload por `RemoteEvent`. Valida en el servidor, exactamente como harías en Roblox.

## Cuando no conecta (diagnóstico)

| Síntoma | Causa probable y arreglo |
|---|---|
| `No se pudo abrir el servidor dedicado en el puerto N` | El puerto ya está en uso (¿otro servidor sigue corriendo?) o bloqueado. Usa otro puerto o cierra el otro proceso. |
| El cliente se queda en `"Connecting"` para siempre | IP mal, servidor apagado, puerto UDP sin redirigir, o firewall. Prueba primero en `127.0.0.1`, luego LAN, luego internet. |
| Funciona en LAN pero no desde fuera | Redirige el puerto como **UDP** (no TCP), usa tu IP **pública** y comprueba si tu proveedor usa CGNAT. |
| `No se pudo resolver el host X` | El hostname no resuelve; usa una IP directamente. |
| Conecta al coordinador y luego nada | El puerto del mundo asignado no es accesible. Redirige también `puerto_coordinador + 1…N`. |
| El juego exportado no hace auto-join | `gl_match.cfg` no se empaquetó. Añade `*.cfg` al filtro de exportación de archivos que no son recursos. |
| No sé quién es servidor y quién cliente | Activa el **Modo Debug** en el panel GodotLuau Config y lee las líneas `[GodotLuau MP]`. |
| El servidor `--glserver` no muestra personaje | Es correcto: un servidor dedicado no tiene jugador local. |

## Backend opcional (Nakama)

En `backend/` hay un **stack local de Nakama + PostgreSQL** (`docker-compose.yml`) pensado como futuro backend central (cuentas, DataStore en la nube). Es **opcional y no está cableado a la extensión**: hoy `HttpService` puede hablar con él porque el HTTP/HTTPS es real, pero no existe un `NakamaService` y `DataStoreService` no va por ahí.

```bash
cd backend
docker compose up -d              # Nakama + Postgres en segundo plano
docker compose logs -f nakama
docker compose down               # parar (los datos quedan en el volumen)
```

| Qué | Valor |
|---|---|
| API HTTP (a la que llama `HttpService`) | `http://127.0.0.1:7350` |
| Consola de administración | `http://127.0.0.1:7351` (`admin` / `password`) |
| Clave de servidor | `defaultkey` |

Mira `backend/README.md` para la hoja de ruta. Esas credenciales son de desarrollo local: cámbialas antes de que esto toque una máquina real.

---

## Cómo funciona por dentro (arquitectura)

GodotLuau es una **GDExtension**: una librería nativa en C++ que Godot carga al arrancar. Todo el código vive en `src/`, agrupado por área.

**1. El intérprete embebido.** Al arrancar (`src/core/register_types.cpp`) la extensión registra Luau como lenguaje de scripting más un cargador/guardador de `.lua` (por eso Godot puede abrir, editar y guardar `.lua` de forma nativa), registra todas las clases estilo Roblox con su icono, y sube los límites de cuerpos de Jolt Physics — los 10.240 por defecto de Godot son muy pocos para mundos de voxels o tycoons. El intérprete es el **Luau oficial** (VM, compilador y AST) compilado dentro del binario.

**2. El árbol de servicios.** El nodo raíz (`RobloxGame3D` / `RobloxGame2D`) construye la jerarquía la primera vez que se abre la escena:

```
game (RobloxGame3D)
├── Workspace            (mundo 3D; Baseplate con la cuadrícula tintable de Studio)
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

Se accede con `game:GetService("Players")` o directamente (`game.Workspace`, `workspace`), y se navega con `FindFirstChild`, `WaitForChild`, `GetChildren` o por nombre.

**3. El avatar R6.** Por defecto los jugadores nacen con el **rig R6**: seis partes colgadas de pivotes en las posiciones clásicas de las articulaciones, así que las animaciones por código (péndulo al caminar con brazos y piernas opuestos, respiración en idle, brazos arriba en el aire) rotan como el avatar clásico. Las partes conservan sus nombres de Roblox (`"Left Arm"`, `"Torso"`, `"HumanoidRootPart"`…). Si pones cualquier modelo llamado **`StarterCharacter`** dentro de `StarterPlayer`, ese pasa a ser el personaje de todos. El desvanecido por cercanía de cámara usa un shader de disolución (descarta píxeles, no mezcla alfa), y al rotar la cámara con clic derecho el ratón vuelve a donde estaba.

**4. Capa de seguridad de vida de objetos.** Los objetos de Luau nunca guardan punteros crudos a nodos: guardan el `ObjectID` y lo resuelven contra la base de objetos viva en cada uso, así que un nodo destruido da `nil` en vez de un crash. Un registro de VMs de Luau vivas garantiza que ningún callback corra sobre un intérprete cerrado (`src/core/gl_runtime.h`).

## Catálogo de nodos y clases

Todas están registradas en el editor con su icono estilo Roblox:

- **Scripts:** `LocalScript`, `ServerScript`, `ModuleScript`.
- **Raíces de juego:** `RobloxGame3D`, `RobloxGame2D`, `RobloxTemplate`.
- **Personajes y física 3D:** `Humanoid`, `RobloxWorkspace`, `RobloxPlayer`, `RobloxPart`.
- **2D:** `Humanoid2D`, `RobloxWorkspace2D`, `RobloxPlayer2D`.
- **Servicios:** `Players`, `Lighting`, `MaterialService`, `ReplicatedStorage`, `ReplicatedFirst`, `ServerStorage`, `ServerScriptService`, `StarterPlayer`, `StarterPlayerScripts`, `StarterCharacterScripts`, `StarterGui`, `StarterPack`, `Teams`, `SoundService`, `RunService`, `TextChatService`, `NetworkClient`, `NetworkService`, `CollectionService`, `UserInputService`, `TweenService`, `Folder`.
- **Comunicación:** `RemoteEventNode`, `RemoteFunctionNode`, `BindableEventNode`.
- **BodyMovers:** `BodyVelocity`, `BodyPosition`, `BodyForce`, `BodyAngularVelocity`, `BodyGyro`.
- **Constraints:** `WeldConstraint`, `HingeConstraint`, `BallSocketConstraint`, `RodConstraint`, `SpringConstraint`, `Motor6D`, `Attachment`.
- **GUI:** `ScreenGui`, `RobloxFrame`, `RobloxTextLabel`, `RobloxTextButton`, `RobloxTextBox`, `RobloxImageLabel`, `RobloxScrollingFrame`, `BillboardGui`, `SurfaceGui`, más ayudantes (`UICorner`, `UIListLayout`, `UIStroke`, `UIGradient`…).
- **Iluminación / efectos:** `AtmosphereNode`, `LightingSkyNode`, `SunRaysNode`, `BloomEffect`, `BlurEffect`, `ColorCorrectionEffect`, `DepthOfFieldEffect`, `ParticleEmitter`, `Beam`, `Trail`, `Highlight`, `Smoke`, `Fire`, `Explosion`…
- **Interacción / herramientas:** `ClickDetector`, `ProximityPrompt`, `SpawnLocation`, `RobloxTool`, `Backpack`, `Seat`, `VehicleSeat`.
- **Sonido:** `RobloxSound`, `RobloxSoundGroup`, nodos de efectos de sonido.
- **Animación:** `AnimationTrack`, `AnimationObject`, `GLR6Animator`.
- **Chat:** `RobloxChat`.
- **Placeholders de lugares importados:** `RBXInstance`, `RBXModel`, `RBXMeshPart`, `RBXTexture`, `RBXDecal`, `RBXMesh`, `RBXBone`.

## Iluminación

- `ClockTime`, `TimeOfDay` (`"18:30:00"`), `Brightness`, `Ambient` / `OutdoorAmbient`, niebla, `ColorShift_Top/Bottom`.
- `SetMinutesAfterMidnight()` / `GetMinutesAfterMidnight()`, ciclo día/noche automático.
- Presets (Realistic, Cartoon, Anime, Sunset, Night…).
- `Technology` cambia la calidad gráfica real de Godot: *Compatibility/Legacy*, *ShadowMap*, *Future* (SSAO + SSIL + Glow), *Voxel* (GI).
- El entorno vive en **`GodotLuau/shaders/environment_roblox.tres`** — edítalo para cambiar el look de todas las escenas.

## Autocompletado e IA

El editor de scripts sugiere **según la escena**, como Roblox Studio: al escribir `workspace.` o `FindFirstChild("` lista los hijos *reales* de tu escena con su clase. Los tipos salen de las definiciones de la API de Roblox (`Scripts/globalTypes.d.luau`, `Scripts/DataTypes.json`). El Luau se colorea con la paleta de Studio (palabras clave rosas, builtins azules, strings verdes, números amarillos) y los errores de sintaxis se resaltan en línea.

También hay un **autocompletado con IA** opcional: una familia de modelos n-grama propios llamada **LuauIA** (*Mini / Medium / High / HighPRO*), entrenados con `tools/entrenar_modelo.py` desde el corpus de `corpus/`.

## Panel de configuración

Abajo en el editor: **GodotLuau Config** (English / Español / Português), organizado en **Updates**, **AI & Autocomplete**, **Data**, **Debug** y **Appearance**.

- **Salida de scripts (`print`/`warn`)** — activada por defecto.
- **Modo Debug** — muestra diagnósticos internos del motor, incluido el log del multijugador.
- **Updates** — comprueba la versión en GitHub, descarga el paquete, verifica su **SHA-256** y reinicia el editor.

## Compilar desde el código fuente

`godot-cpp` y `luau` son **submódulos de git** fijados a commits exactos:

```bash
git clone https://github.com/Pimpoli/GodotLuau.git
cd GodotLuau
git submodule update --init --recursive
```

Requisitos: Python 3, [SCons](https://scons.org/) (`pip install scons`) y un compilador C++ (MinGW/MSVC en Windows, GCC/Clang en Linux).

```bash
scons platform=windows target=template_debug
scons platform=windows target=template_release
# o: scons platform=linux target=template_debug / template_release
```

Las librerías salen en `GodotLuau/bin/`. Copia los `.so` resultantes al `GodotLuau/bin/` de tu proyecto en esa máquina Linux (junto a las `.dll`); `godot_luau.gdextension` ya apunta a los dos. Los binarios de Linux también los construye el workflow de GitHub Actions (`.github/workflows/build-linux.yml`) en cada push que toca `src/`.

> El `SConstruct` reactiva las excepciones de C++ (`-fexceptions` / `/EHsc`) porque godot-cpp las desactiva por defecto y la VM de Luau las necesita. No hay que pasar nada extra.

## Estructura del repositorio

```
src/                       Código C++ de la extensión, por área:
  core/                      Integración de la VM de Luau, lenguaje, registro, errores
  editor/                    Autocompletado + base de tipos (solo editor)
  importer/                  Lector de .rbxl y plugin de importación
  services/                  game, servicios, workspace, red (multijugador)
  characters/                jugadores, humanoids, partes, avatar R6, body movers
  gameplay/                  remotes, input, interacción, animación, tween, sonido
  ui/                        GUI, billboards, chat, efectos de luz, menú de ajustes
addons/GodotLuauUpdater/   Panel de ajustes + auto-updater + UX del editor (GDScript)
GodotLuau/                 Todo lo que la extensión distribuye:
  bin/                       Librerías compiladas (.dll / .so)
  icons/                     Iconos SVG estilo Roblox
  assets/                    Mallas del avatar R6, texturas (cuadrícula del baseplate)
  shaders/                   character_fade.gdshader, environment_roblox.tres
  DefaultScripts/            Controladores de jugador de ejemplo (Luau)
  DedicatedServer/           Lanzadores del servidor + las guías del servidor dedicado
  licenses/                  Licencias MIT de GodotLuau y del software embebido
backend/                   Stack local opcional de Nakama + Postgres (docker compose)
tools/                     Scripts de ayuda en Python
Scripts/                   Definiciones de tipos para el autocompletado
corpus/, models/           Corpus y modelos del autocompletado con IA
godot-cpp/, luau/          Dependencias (submódulos)
```

## Herramientas incluidas

| Script | Qué hace |
|---|---|
| `tools/nuevo_proyecto.py` | Crea un proyecto de juego nuevo listo para usar (3D o 2D) |
| `tools/crear_plantilla.py` | Empaqueta `RobloxTemplate.zip`, importable desde el gestor de proyectos de Godot |
| `tools/generar_release.py` | Regenera `GodotLuau.zip` + `.sha256` para el auto-updater |
| `tools/entrenar_modelo.py` | Entrena la familia `models/LuauIA-*.json` (autocompletado con IA) |

## Limitaciones conocidas

- **Una VM por script:** un `ModuleScript` requerido por dos scripts no comparte estado entre ellos.
- **La red la alojas tú:** no hay relay, ni NAT traversal, ni cuentas, ni lista oficial de servidores. Ver [Seguridad](#seguridad-léelo-antes-de-publicar).
- **El coordinador es de una sola máquina:** los mundos son procesos locales descubiertos por archivos de latido en `user://`.
- **`DataStoreService` son JSON locales**, no almacenamiento en la nube.
- **Cantidades muy grandes de partes** (decenas de miles) funcionan pero todavía no se agrupan; el clustering/instancing automático de partes ancladas es la siguiente pieza grande.
- **Plataformas:** los binarios precompilados son Windows x86_64 y Linux x86_64. macOS necesita compilar desde el fuente.
- **Los lugares `.rbxlx` (XML) no se leen**, y el contenido `rbxassetid://` no se puede descargar.

## Publicar una versión (mantenedor)

1. Recompilar las dos DLL de Windows (debug y release); hacer push de `src/` para que el workflow de Linux construya los `.so`, y ponerlos en `GodotLuau/bin/`.
2. Actualizar el archivo `Version` y la versión en `addons/GodotLuauUpdater/plugin.cfg`.
3. Regenerar el paquete del updater: `python tools/generar_release.py`
4. Hacer commit y push de `Version`, `GodotLuau.zip` **y** `GodotLuau.zip.sha256` juntos.

El auto-updater de los usuarios compara `Version`, descarga el ZIP y verifica el SHA-256 antes de instalar.

## Licencia y créditos

- **GodotLuau** — Licencia MIT, © 2026 PimpoliDev (ver `LICENSE`).
- **[Luau](https://github.com/luau-lang/luau)** — el intérprete embebido, MIT (Roblox Corporation).
- **[godot-cpp](https://github.com/godotengine/godot-cpp)** — los bindings de C++ para Godot, MIT.
- **[Godot Engine](https://godotengine.org/)** — el motor sobre el que corre todo, MIT.

Las copias de todas las licencias incluidas van dentro del paquete de release, en `GodotLuau/licenses/`.
