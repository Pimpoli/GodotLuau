# GodotLuau

**Sistema Roblox para Godot 4** — una GDExtension escrita en C++ que integra el intérprete [Luau](https://luau.org/) dentro de Godot y reproduce la arquitectura de Roblox: `game`, servicios (`Workspace`, `Players`, `Lighting`, `ReplicatedStorage`…), `LocalScript` / `ServerScript` / `ModuleScript`, `Humanoid`, la librería `task`, `RunService.Heartbeat`, GUI estilo Roblox, constraints, BodyMovers y mucho más. Escribes tu juego en **Luau**, igual que en Roblox Studio, pero corre sobre el motor de Godot.

*Roblox System for Godot 4 — a C++ GDExtension that embeds the Luau interpreter in Godot and mirrors the full Roblox architecture.*

**Autor:** PimpoliDev · **Repo:** https://github.com/Pimpoli/GodotLuau · **Versión:** v1.7.0

---

## Índice

- [¿Qué es GodotLuau?](#qué-es-godotluau)
- [Características](#características)
- [Empezar a usar](#empezar-a-usar)
- [Cómo funciona (arquitectura)](#cómo-funciona-arquitectura)
  - [El intérprete Luau embebido](#1-el-intérprete-luau-embebido)
  - [Tipos de script y cómo se ejecutan](#2-tipos-de-script-y-cómo-se-ejecutan)
  - [El árbol de servicios estilo Roblox](#3-el-árbol-de-servicios-estilo-roblox)
  - [La API de Luau disponible](#4-la-api-de-luau-disponible)
  - [Capa de seguridad de vida](#5-capa-de-seguridad-de-vida)
  - [Scripts con ID persistente y papelera](#6-scripts-con-id-persistente-y-papelera)
- [Catálogo de nodos y clases](#catálogo-de-nodos-y-clases)
- [Sistemas de jugador en Luau](#sistemas-de-jugador-en-luau)
- [Iluminación estilo Roblox](#iluminación-estilo-roblox)
- [Autocompletado e IA](#autocompletado-e-ia)
- [Panel de configuración](#panel-de-configuración)
- [Compilar desde el código fuente](#compilar-desde-el-código-fuente)
- [Estructura del repositorio](#estructura-del-repositorio)
- [Herramientas incluidas](#herramientas-incluidas)
- [Limitaciones conocidas](#limitaciones-conocidas)
- [Publicar una release (mantenedor)](#publicar-una-release-mantenedor)
- [Licencia y créditos](#licencia-y-créditos)

---

## ¿Qué es GodotLuau?

GodotLuau hace que Godot 4 hable **Luau** (el dialecto de Lua que usa Roblox) y trae consigo la jerarquía, los nodos y la API con los que ya trabaja cualquier desarrollador de Roblox. La idea es que puedas escribir lógica de juego con la mentalidad de Roblox —`game.Players.PlayerAdded`, `workspace.Parte.Touched`, `RemoteEvent:FireServer()`, `task.wait()`— pero aprovechando el motor, el renderizado y la exportación de Godot.

No es un emulador de Roblox ni se conecta a sus servidores: es una **reimplementación independiente** de su API más común sobre tecnología abierta (Godot + Luau, ambos de código abierto).

## Características

- **API de Roblox en Luau de verdad**, interpretada por el motor oficial Luau compilado dentro de la extensión.
- **Jerarquía de servicios automática:** al abrir una escena `RobloxGame` se construye sola toda la estructura (`Workspace`, `Players`, `Lighting`, `StarterPlayer/StarterPlayerScripts`, etc.).
- **Tres tipos de script** con el comportamiento de Roblox: `LocalScript` (cliente), `ServerScript` (servidor) y `ModuleScript` (`require`).
- **Autocompletado consciente de la escena** (como Roblox Studio), documentación al pasar el cursor y resaltado de errores de sintaxis, directamente en el editor de Godot.
- **Autocompletado con IA** opcional mediante modelos n-gram propios (familia *LuauIA*).
- **Iluminación fiel a Roblox** con `ClockTime`, ciclo día/noche, presets y calidad gráfica configurable.
- **Sistemas de jugador listos y editables** en Luau (movimiento, cámara, sprint, estamina, vida con regeneración, anti-exploit).
- **Capa de seguridad de memoria** que evita los *crashes* clásicos al destruir objetos en tiempo de ejecución.
- **Auto-updater** integrado que descarga e instala nuevas versiones desde GitHub con verificación SHA-256.
- **Multilingüe** (English / Español / Português) en todo el panel y los mensajes.

## Empezar a usar

> **Requisitos:** Godot **4.3 o superior** · Windows x86_64 (las DLLs precompiladas incluidas son solo de Windows; para otras plataformas, ver [Compilar desde el código fuente](#compilar-desde-el-código-fuente)).

La forma más fácil de crear un juego nuevo es el asistente:

```bash
python nuevo_proyecto.py
```

Te pregunta nombre, modo (3D o 2D) y carpeta, y genera un proyecto Godot completo y listo para abrir, con:

- La extensión compilada (`bin/`) y su archivo `godot_luau.gdextension`.
- Los iconos estilo Roblox para que los nodos se vean como en Studio.
- El addon **GodotLuau Config** (panel de ajustes + auto-updater) ya habilitado.
- Una escena raíz `RobloxGame` que **auto-genera toda la jerarquía de servicios al abrirla**.

Al abrir el proyecto verás la estructura familiar (`Workspace`, `Players`, `StarterPlayer/StarterPlayerScripts` con `PlayerModule`, `ControlModule`, `CameraModule`…) y podrás editar los `.lua` directamente en Godot. Pulsa **F5** para jugar.

## Cómo funciona (arquitectura)

GodotLuau es una **GDExtension** (una librería nativa en C++ que Godot carga al arrancar). Por dentro está organizada como un conjunto de cabeceras incluidas por dos unidades de compilación; todo el código vive en `src/`. Esto es lo que ocurre paso a paso.

### 1. El intérprete Luau embebido

Al iniciar, la extensión hace dos cosas (en `src/register_types.cpp`):

1. **Registra Luau como lenguaje de script** dentro de Godot (`Engine::register_script_language`), junto a un **cargador y guardador de archivos `.lua`** (`LuauLoader` / `LuauSaver`). Por eso Godot sabe abrir, editar y guardar `.lua` de forma nativa.
2. **Registra todas las clases** (nodos y servicios) estilo Roblox, cada una con su icono.

El intérprete es el **Luau oficial** (de luau-lang) compilado dentro de la propia DLL: la extensión incluye la VM, el compilador y el AST de Luau.

### 2. Tipos de script y cómo se ejecutan

Hay tres nodos de script, con el mismo significado que en Roblox:

| Nodo | Color | Contexto | Cuándo se ejecuta |
|---|---|---|---|
| `LocalScript` | Azul | **Cliente** (`LocalPlayer`, `UserInputService` disponibles) | Solo, al iniciar el juego |
| `ServerScript` | Naranja | **Servidor** (`UserInputService` bloqueado, `LocalPlayer` nil) | Solo, al iniciar el juego |
| `ModuleScript` | Morado | Neutro | **No corre solo**: se carga con `require()` y devuelve su valor |

Cuando empieza el juego (no en el editor), cada `LocalScript` y `ServerScript` **crea su propia máquina virtual de Luau** (`luaL_newstate` + librerías estándar). El cuerpo del archivo `.lua` **se ejecuta de arriba a abajo directamente**, al estilo de Roblox — **no** se usan callbacks `_ready` / `_process` como en GDScript. Para lógica continua se usan `RunService.Heartbeat:Connect(...)` o bucles con `task.wait()`.

Un `ModuleScript` no se ejecuta por su cuenta: cuando otro script hace `require(...)`, su código corre una vez y se devuelve (y cachea) el valor que retorne — normalmente una tabla con funciones.

### 3. El árbol de servicios estilo Roblox

El nodo raíz (`RobloxGame3D` o `RobloxGame2D`) **auto-construye** la jerarquía completa de servicios la primera vez que se abre la escena, de modo que `game` y sus servicios existen tal y como esperas:

```
game (RobloxGame3D)
├── Workspace            (mundo 3D; contiene SpawnLocation y RunService oculto)
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

Desde Luau accedes a ellos con `game:GetService("Players")` o directamente (`game.Workspace`, `workspace`, etc.), y navegas el árbol con `FindFirstChild`, `WaitForChild`, `GetChildren` o por nombre (`workspace.MiParte`).

### 4. La API de Luau disponible

Dentro de un script tienes el entorno habitual de Roblox:

- **Globales:** `game`, `workspace`, `script`, `require`, `shared`, además de las librerías estándar de Luau (`math`, `string`, `table`, `coroutine`, `os`, `utf8`, `bit32`…).
- **Salida:** `print(...)`, `warn(...)`.
- **Programación temporal:** `task.spawn`, `task.delay`, `task.defer`, `task.wait`, `task.cancel`, y los globales clásicos `wait`, `spawn`, `delay`, `tick`, `time`. Los hilos (corrutinas) los gestiona el nodo, así que `task.wait()` pausa de verdad y reanuda en su momento.
- **Tipos de dato** con sus operadores y `typeof` correcto: `Vector3`, `Vector2`, `CFrame`, `Color3`, `UDim`, `UDim2`, `BrickColor`, además de `Enum` e `Instance`.
- **Señales estilo Roblox:** `:Connect()`, `:Disconnect()`, `.Connected`, `:Wait()` en todo lo que emite eventos (`RunService.Heartbeat`, eventos de `Humanoid`, `ClickDetector`, `ProximityPrompt`, `UserInputService`, GUI, etc.).
- **Comunicación:** `RemoteEvent` (`FireServer`/`FireClient`/`OnServerEvent`/`OnClientEvent`), `RemoteFunction` y `BindableEvent`.
- **Identidad de clase:** `Instance.ClassName` y `:IsA(...)` fieles a la jerarquía de Roblox.

> El cuerpo del script corre con la mentalidad de Roblox. **No** se expone la API cruda de Godot (`Node.new`, `:add_child`, `:queue_free`, `get_tree()`…): se trabaja siempre por el lado Roblox.

### 5. Capa de seguridad de vida

Mezclar un lenguaje con recolección de basura (Luau) y objetos de motor que pueden destruirse en cualquier momento (nodos de Godot) es la receta clásica de los *use-after-free* (acceder a memoria ya liberada → *crash*). GodotLuau lo evita con una capa dedicada (`src/gl_runtime.h`):

- Los objetos de Luau **no guardan punteros crudos** a los nodos, sino su **`ObjectID`**. Al usarlos se resuelve el ID contra la base de objetos viva: si el nodo ya no existe, obtienes `nil` en vez de un *crash*.
- Hay un **registro de las VMs de Luau vivas**: antes de invocar cualquier *callback* se comprueba que su VM sigue activa, de modo que nunca se ejecuta código sobre un intérprete ya cerrado.

### 6. Scripts con ID persistente y papelera

Cada `LocalScript` / `ServerScript` / `ModuleScript` recibe un **`script_id` inmutable** (p. ej. `ServerScript_ID_3`) que da nombre a su archivo `.lua`. Esto permite un flujo cómodo y seguro:

- Al **borrar el nodo**, su `.lua` no se pierde: se mueve a la papelera `res://.luau_trash/`.
- Con **Ctrl+Z** vuelven a la vez el nodo y su script.
- La papelera **se purga sola a los 7 días**.

## Catálogo de nodos y clases

Todas estas clases se registran en el editor con su icono estilo Roblox (ver `src/register_types.cpp`):

- **Scripts:** `LocalScript`, `ServerScript`, `ModuleScript`.
- **Raíces de juego:** `RobloxGame3D`, `RobloxGame2D`, `RobloxTemplate`.
- **Personajes y física 3D:** `Humanoid`, `RobloxWorkspace`, `RobloxPlayer`, `RobloxPart`.
- **Personajes y física 2D:** `Humanoid2D`, `RobloxWorkspace2D`, `RobloxPlayer2D`.
- **Servicios:** `Players`, `Lighting`, `MaterialService`, `ReplicatedStorage`, `ReplicatedFirst`, `ServerStorage`, `ServerScriptService`, `StarterPlayer`, `StarterPlayerScripts`, `StarterCharacterScripts`, `StarterGui`, `StarterPack`, `Teams`, `SoundService`, `RunService`, `TextChatService`, `NetworkClient`, `CollectionService`, `UserInputService`, `TweenService`, `Folder`.
- **Comunicación:** `RemoteEventNode`, `RemoteFunctionNode`, `BindableEventNode`.
- **BodyMovers:** `BodyVelocity`, `BodyPosition`, `BodyForce`, `BodyAngularVelocity`, `BodyGyro`.
- **Constraints:** `WeldConstraint`, `HingeConstraint`, `BallSocketConstraint`, `RodConstraint`, `SpringConstraint`, `Motor6D`.
- **GUI:** `ScreenGui`, `RobloxFrame`, `RobloxTextLabel`, `RobloxTextButton`, `RobloxTextBox`, `RobloxImageLabel`, `RobloxScrollingFrame`, `BillboardGui`, `SurfaceGui`.
- **Iluminación / efectos:** `AtmosphereNode`, `LightingSkyNode`, `SunRaysNode`, `BloomEffect`, `BlurEffect`, `ColorCorrectionEffect`, `DepthOfFieldEffect`.
- **Interacción / herramientas:** `ClickDetector`, `ProximityPrompt`, `SpawnLocation`, `RobloxTool`, `Backpack`.
- **Sonido:** `RobloxSound`, `RobloxSoundGroup`.
- **Animación:** `AnimationTrack`, `AnimationObject`.
- **Chat:** `RobloxChat`.

## Sistemas de jugador en Luau

Los proyectos nuevos incluyen controladores de jugador **escritos en Luau y totalmente editables** (`DefaultScripts/PlayerController3D.lua` y `PlayerController2D.lua`), pensados para que los abras y los toquetees:

- Movimiento y cámara (modo fijo o suavizado).
- **Sprint** con aceleración suave y **estamina** opcional.
- Módulos por plataforma (PC / móvil / consola).
- **Vida** con regeneración fiel a Roblox (se reinicia al recibir daño).
- Un `GameManager` con `PlayerAdded` y validación anti-exploit.

## Iluminación estilo Roblox

El servicio `Lighting` reproduce el comportamiento de Roblox:

- `ClockTime`, `TimeOfDay` (`"18:30:00"`), `Brightness`, `Ambient` / `OutdoorAmbient`, niebla, `ColorShift_Top/Bottom`.
- `SetMinutesAfterMidnight()` / `GetMinutesAfterMidnight()`.
- **Ciclo día/noche automático** (`DayNightCycle` + `DayLengthMinutes`).
- **Presets** (Realistic, Cartoon, Anime, Sunset, Night…).
- `Technology` ajusta la calidad gráfica real de Godot: *Compatibility/Legacy* (rendimiento), *ShadowMap* (equilibrado), *Future* (SSAO + SSIL + Glow), *Voxel* (iluminación global).

## Autocompletado e IA

El editor de scripts ofrece sugerencias **conscientes del árbol de la escena** (como Roblox Studio): al escribir `workspace.` o `FindFirstChild("` aparecen los hijos *reales* de la escena con su clase, y las propiedades se resuelven según la clase real de cada nodo. La base de tipos proviene de las definiciones de la API de Roblox (`Scripts/globalTypes.d.luau`, `Scripts/DataTypes.json`).

Además, de forma opcional, hay **autocompletado con IA**: una familia de modelos n-gram propios llamada **LuauIA** (niveles *Mini / Medium / High / HighPRO*, de más rápido a más inteligente). Se entrenan con `entrenar_modelo.py` a partir del corpus en `corpus/` y se pueden descargar desde el panel.

## Panel de configuración

En la barra inferior del editor aparece **GodotLuau Config** (English / Español / Português), organizado en zonas: **Actualizaciones**, **IA y Autocompletado**, **Datos**, **Debug** y **Apariencia** (con control deslizante de tamaño de texto 0–100).

- **Salida de scripts (`print`/`warn`):** activada por defecto; desactívala para tener un Output limpio.
- **Autocompletado instantáneo** y **autocompletado personalizado** desde un JSON local o una URL.
- **Modo Debug:** muestra la salida interna de los scripts del sistema (PlayerModule, Health, Chat…) y mensajes del motor.
- **Actualizaciones:** comprueba la versión en GitHub, descarga la nueva DLL (verificando su **SHA-256**) y reinicia el editor.

## Compilar desde el código fuente

Solo necesitas compilar si quieres modificar la extensión o generar binarios para Linux/macOS.

Las dependencias `godot-cpp` y `luau` van como **submódulos de git** fijados a commits exactos. Ya hay un `.gitmodules`, así que basta con inicializarlos:

```bash
git clone https://github.com/Pimpoli/GodotLuau.git
cd GodotLuau
git submodule update --init --recursive
```

> Los submódulos quedan fijados a `godot-cpp` en el commit `d5cc777…` (rama 4.3) y `luau` en `9e2984f…` (release/712), que son los compatibles.

**Requisitos de build:** Python 3, [SCons](https://scons.org/) (`pip install scons`) y un compilador de C++ (MSVC en Windows; GCC/Clang en Linux/macOS).

```bash
scons platform=windows target=template_debug
scons platform=windows target=template_release
```

Las librerías quedan en `bin/`. En Linux/macOS usa `platform=linux` / `platform=macos` y añade las entradas correspondientes (`linux.debug.x86_64`, etc.) en `godot_luau.gdextension`.

> El `SConstruct` reactiva las excepciones de C++ (`-fexceptions` / `/EHsc`), porque godot-cpp las desactiva por defecto y la VM de Luau las necesita para compilar. No tienes que pasar nada extra: ya está contemplado.

## Estructura del repositorio

```
src/                       Código C++ de la extensión (la VM, los nodos y la API)
addons/GodotLuauUpdater/   Panel de configuración + auto-updater (GDScript)
DefaultScripts/            Controladores de jugador de ejemplo (Luau)
Scripts/                   Definiciones de tipos para el autocompletado
corpus/                    Corpus de entrenamiento de los modelos de IA
models/                    Modelos de IA LuauIA (los descarga el updater)
icons/                     Iconos SVG estilo Roblox
bin/                       Librerías compiladas (DLL)
godot-cpp/, luau/          Dependencias (submódulos; ver arriba)
```

## Herramientas incluidas

| Script | Para qué sirve |
|---|---|
| `nuevo_proyecto.py` | Crea un proyecto de juego nuevo listo para usar (3D o 2D) |
| `crear_plantilla.py` | Empaqueta `RobloxTemplate.zip` importable desde el Project Manager de Godot |
| `generar_release.py` | Regenera `GodotLuau.zip` + `GodotLuau.zip.sha256` para el auto-updater |
| `entrenar_modelo.py` | Entrena la familia `models/LuauIA-*.json` (modelos del Autocompletado con IA) |

## Limitaciones conocidas

GodotLuau implementa la *experiencia* de programar como en Roblox, pero no es Roblox. Ten en cuenta:

- **Sin replicación de red real:** `RemoteEvent`/`RemoteFunction`/`BindableEvent` funcionan dentro de **una sola máquina** (cliente y servidor conviven en el mismo proceso). `Players:GetPlayers()` devuelve un único jugador.
- **Cada script tiene su propia VM:** un `ModuleScript` requerido por dos scripts distintos **no comparte memoria** entre ellos (cada script lo carga en su propio intérprete).
- **`Destroy`** saca el nodo del árbol al instante, pero el objeto C++ puede no liberarse de inmediato en estos nodos creados por la extensión (no es un *crash* ni una fuga visible).
- **Plataformas:** las DLLs incluidas son solo de **Windows x86_64**. Para otras plataformas hay que compilar (ver arriba).

## Publicar una release (mantenedor)

1. Recompila ambas DLLs (debug y release).
2. Actualiza el archivo `Version` (p. ej. `v1.7.0`) y la versión en `addons/GodotLuauUpdater/plugin.cfg`. El banner lee la versión de `res://Version` en tiempo de ejecución, así que no hay que tocar C++.
3. Regenera el paquete del updater y su hash:
   ```bash
   python generar_release.py
   ```
4. Haz commit y push de `Version`, `GodotLuau.zip` **y** `GodotLuau.zip.sha256` juntos.

El auto-updater de los usuarios compara `Version`, descarga el ZIP y verifica el SHA-256 antes de instalar.

## Licencia y créditos

- **GodotLuau** — © PimpoliDev. *(El repositorio aún no incluye un archivo `LICENSE`; si vas a distribuir o contribuir, conviene añadir uno para dejar claros los términos de uso.)*
- **[Luau](https://github.com/luau-lang/luau)** — el intérprete embebido, licencia MIT (Roblox Corporation).
- **[godot-cpp](https://github.com/godotengine/godot-cpp)** — los bindings de C++ para Godot, licencia MIT.
- **[Godot Engine](https://godotengine.org/)** — el motor sobre el que corre todo, licencia MIT.
