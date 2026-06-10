# GodotLuau

**Sistema Roblox para Godot 4** — una GDExtension en C++ que integra el intérprete [Luau](https://luau.org/) en Godot, con la arquitectura completa de Roblox: `game`, servicios (`Workspace`, `Players`, `Lighting`, `ReplicatedStorage`…), `LocalScript` / `ServerScript` / `ModuleScript`, `Humanoid`, `task`, `RunService.Heartbeat`, GUI tipo Roblox, constraints, BodyMovers y mucho más.

*Roblox System for Godot 4 — a C++ GDExtension that embeds the Luau interpreter in Godot with the full Roblox architecture.*

**Autor:** PimpoliDev · **Repo:** https://github.com/Pimpoli/GodotLuau

---

## Requisitos

- **Godot 4.3 o superior** (las DLLs se compilan contra godot-cpp 4.3; `compatibility_minimum = "4.3"`)
- Windows x86_64 (las librerías precompiladas incluidas son solo Windows; para Linux/macOS ver [Compilar desde el código fuente](#compilar-desde-el-código-fuente))

## Empezar a usar

La forma más fácil de crear un juego nuevo:

```
python nuevo_proyecto.py
```

El asistente te pregunta nombre, modo (3D/2D) y carpeta, y genera un proyecto Godot completo con:
- La extensión compilada (`bin/`) y su `.gdextension`
- Los iconos estilo Roblox
- El addon **GodotLuau Config** (panel de ajustes + auto-updater) ya habilitado
- Una escena `RobloxGame` que **auto-genera toda la jerarquía de servicios al abrirla**

Al abrir el proyecto en Godot verás la estructura familiar: `Workspace`, `Players`, `StarterPlayer/StarterPlayerScripts` con `PlayerModule`, `ControlModule`, `CameraModule`, etc. Edita los `.lua` directamente en el editor de Godot con autocompletado, documentación al pasar el cursor y resaltado de errores de sintaxis.

## Panel de configuración

En la barra inferior del editor aparece **GodotLuau Config** (English/Español/Português), organizado en zonas: Actualizaciones, IA y Autocompletado, Datos, Debug y Apariencia (con control deslizante de tamaño de texto 0–100).

- **Salida de scripts (print/warn):** activada por defecto; desactívala si quieres un Output limpio.
- **Autocompletado instantáneo** y **autocompletado personalizado** desde JSON local o URL.
- **Actualizaciones:** comprueba la versión en GitHub, descarga y aplica la nueva DLL (con verificación de integridad SHA-256) y reinicia el editor.

## Características destacadas

- **Autocompletado consciente de la escena** (como Roblox Studio): al escribir `workspace.` o `FindFirstChild("` se sugieren los hijos *reales* del árbol con su clase; las propiedades se resuelven según la clase real del nodo.
- **Scripts con ID persistente:** cada LocalScript/ServerScript/ModuleScript recibe un `script_id` inmutable (ej. `ServerScript_ID_3`) que nombra su archivo `.lua`. Al **borrar el nodo se "borra" su script** (va a la papelera `res://.luau_trash/`) y con **Ctrl+Z vuelven los dos**. La papelera se purga sola a los 7 días.
- **Lighting estilo Roblox:** `ClockTime`, `TimeOfDay` ("18:30:00"), `Brightness`, `Ambient`/`OutdoorAmbient`, niebla, `ColorShift_Top/Bottom`, presets (Realistic, Cartoon, Anime, Sunset, Night…), `SetMinutesAfterMidnight()`, y **ciclo día/noche automático** (`DayNightCycle` + `DayLengthMinutes`). `Technology` ajusta la calidad real: Compatibility/Legacy (rendimiento), ShadowMap (equilibrado), Future (SSAO+SSIL+Glow), Voxel (GI global).
- **Sistemas de jugador en Luau editables:** sprint con aceleración suave, estamina opcional, módulos por plataforma (PC/Móvil/Consola), regeneración de vida fiel a Roblox (se reinicia al recibir daño) y GameManager con validación anti-exploit.

## Compilar desde el código fuente

Las dependencias `godot-cpp` y `luau` van como submódulos *sin* `.gitmodules`, así que hay que clonarlas a mano en los commits exactos:

```bash
git clone https://github.com/Pimpoli/GodotLuau.git
cd GodotLuau

git clone https://github.com/godotengine/godot-cpp.git godot-cpp
git -C godot-cpp checkout d5cc777a89d899665fb61f1650ef0dc0cf6488c4   # rama 4.3

git clone https://github.com/luau-lang/luau.git luau
git -C luau checkout 9e2984fd0334414a2ef62ceede0c06dab7574d55        # release/712
```

Requisitos de build: **Python 3**, **SCons** (`pip install scons`) y un compilador C++ (MSVC en Windows, GCC/Clang en Linux).

```bash
scons platform=windows target=template_debug
scons platform=windows target=template_release
```

Las librerías quedan en `bin/`. En Linux/macOS usa `platform=linux` / `platform=macos` y añade las entradas correspondientes en `godot_luau.gdextension` (`linux.debug.x86_64`, etc.).

## Publicar una release (mantenedor)

1. Recompila ambas DLLs (debug y release).
2. Actualiza el archivo `Version` (ej. `v1.4.5`).
3. Regenera el paquete del updater y su hash:
   ```
   python generar_release.py
   ```
4. Haz commit y push de `Version`, `GodotLuau.zip` **y** `GodotLuau.zip.sha256` juntos.

El auto-updater de los usuarios compara `Version`, descarga el ZIP y verifica el SHA-256 antes de instalar.

## Herramientas incluidas

| Script | Para qué sirve |
|---|---|
| `nuevo_proyecto.py` | Crea un proyecto de juego nuevo listo para usar (3D o 2D) |
| `crear_plantilla.py` | Empaqueta `RobloxTemplate.zip` importable desde el Project Manager de Godot |
| `generar_release.py` | Regenera `GodotLuau.zip` + `GodotLuau.zip.sha256` para el auto-updater |
| `entrenar_modelo.py` | Entrena `models/LuauGram-Plus.json` (modelo n-gram del Autocompletado con IA) a partir del corpus en `corpus/` y los templates |

## Estructura del repositorio

```
src/                  Código C++ de la extensión (~80 clases estilo Roblox)
addons/GodotLuauUpdater/  Panel de configuración + auto-updater (GDScript)
DefaultScripts/       Controladores de jugador de ejemplo (Luau)
Scripts/              Tipos y datos para el autocompletado
icons/                Iconos SVG estilo Roblox
bin/                  Librerías compiladas (DLL)
godot-cpp/, luau/     Dependencias (clonar a mano, ver arriba)
```
