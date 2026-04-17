"""
GodotLuau — Crear nuevo proyecto
=================================
Crea un proyecto de Godot 4 listo para usar con la arquitectura
completa de Roblox: servicios, movimiento y chat ya configurados.

Uso:  python nuevo_proyecto.py
Autor: PimpoliDev

FUNCIONES:
  - Elige entre juego 2D o 3D
  - Crea toda la estructura de carpetas
  - Copia la DLL compilada y los iconos
  - Genera la escena principal con todos los servicios
  - Incluye el script de movimiento correspondiente (Luau)
"""

import os
import sys
import shutil
import random
import string

ORIGEN = os.path.dirname(os.path.abspath(__file__))

# ─── Banner ────────────────────────────────────────────────────────
BANNER = """
╔══════════════════════════════════════════════════════╗
║         GodotLuau — by PimpoliDev                    ║
║          System Roblox for Godot                     ║
╚══════════════════════════════════════════════════════╝
"""

# ─── Generador de UID válido para Godot 4 ──────────────────────────
def uid_godot():
    chars = string.digits + string.ascii_lowercase
    return "uid://" + "".join(random.choices(chars, k=13))

# ─── Plantillas de archivos ────────────────────────────────────────

def project_godot(nombre):
    return f"""; Configuración del proyecto Godot 4
; Generado por GodotLuau — PimpoliDev

config_version=5

[application]

config/name="{nombre}"
config/description="Proyecto creado con GodotLuau"
run/main_scene="res://Escenas/RobloxGame.tscn"
config/features=PackedStringArray("4.6", "Forward Plus")
config/icon="res://icons/icon.svg"
"""

def gdextension(dll_path):
    release = dll_path.replace("template_debug", "template_release")
    return f"""[configuration]
entry_symbol = "luau_extension_init"
compatibility_minimum = "4.1"
reloadable = true

[libraries]
windows.debug.x86_64   = "{dll_path}"
windows.release.x86_64 = "{release}"
linux.debug.x86_64     = "{dll_path.replace('.dll', '.so')}"
linux.release.x86_64   = "{release.replace('.dll', '.so')}"
"""

def scene_3d(uid):
    # RobloxGame3D crea AUTOMÁTICAMENTE toda la jerarquía al abrir en el editor
    return f"""[gd_scene format=3 uid="{uid}"]

[node name="Game" type="RobloxGame3D"]
"""

def scene_2d(uid):
    # RobloxGame2D crea AUTOMÁTICAMENTE toda la jerarquía al abrir en el editor
    return f"""[gd_scene format=3 uid="{uid}"]

[node name="Game" type="RobloxGame2D"]
"""

def icon_svg():
    """Icono minimalista de GodotLuau"""
    return """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <rect width="64" height="64" rx="12" fill="#1a1a2e"/>
  <text x="32" y="44" font-family="monospace" font-size="28" font-weight="bold"
        text-anchor="middle" fill="#4fc3f7">GL</text>
  <text x="32" y="56" font-family="monospace" font-size="8"
        text-anchor="middle" fill="#546e7a">GodotLuau</text>
</svg>"""

def script_controller_3d():
    """Contenido del script de movimiento 3D para StarterPlayerScripts"""
    script_path = os.path.join(ORIGEN, "DefaultScripts", "PlayerController3D.lua")
    if os.path.exists(script_path):
        with open(script_path, "r", encoding="utf-8") as f:
            return f.read()
    # Fallback básico si no existe el archivo
    return """-- > GodotLuau — PimpoliDev
-- PlayerController3D — Sistema de movimiento 3D

-- MODO DE CAMARA: 1=Fija, 2=Suave, 3=Combinada
local CAMERA_MODE = 1

local Players = game:GetService("Players")
local player  = Players.LocalPlayer
if not player then return end

local humanoid = player:FindFirstChild("Humanoid")
if humanoid then
    humanoid.WalkSpeed = 16
    humanoid.JumpPower = 20
end

player.CameraMode = CAMERA_MODE
print("[GodotLuau] PlayerController3D listo.")
"""

def script_controller_2d():
    """Contenido del script de movimiento 2D para StarterPlayerScripts"""
    script_path = os.path.join(ORIGEN, "DefaultScripts", "PlayerController2D.lua")
    if os.path.exists(script_path):
        with open(script_path, "r", encoding="utf-8") as f:
            return f.read()
    return """-- > GodotLuau — PimpoliDev
-- PlayerController2D — Sistema de movimiento 2D

-- TIPO DE MOVIMIENTO: 0=Platformer, 1=TopDown
local MOVEMENT_TYPE = 0

-- MODO DE CAMARA: 1=Fija, 2=Suave, 3=Combinada
local CAMERA_MODE = 1

local Players = game:GetService("Players")
local player  = Players.LocalPlayer
if not player then return end

player.MovementType = MOVEMENT_TYPE
player.CameraMode   = CAMERA_MODE
print("[GodotLuau] PlayerController2D listo.")
"""

# ─── Lógica principal ───────────────────────────────────────────────

def elegir_opcion(pregunta, opciones):
    """Muestra un menú numerado y devuelve el índice elegido (0-based)"""
    while True:
        print(f"\n{pregunta}")
        for i, opt in enumerate(opciones, 1):
            print(f"  {i}. {opt}")
        resp = input("  Tu elección: ").strip()
        if resp.isdigit() and 1 <= int(resp) <= len(opciones):
            return int(resp) - 1
        print("  ⚠  Opción no válida. Intenta de nuevo.")

def crear_proyecto(destino, nombre, modo):
    is_2d = (modo == 1)
    modo_str = "2D" if is_2d else "3D"
    print(f"\n  Creando proyecto '{nombre}' ({modo_str}) en:")
    print(f"  {destino}\n")

    # 1. Estructura de carpetas
    # LocalScripts/ModuleScripts/ServerScripts: la extensión guarda aquí los .lua del editor
    carpetas = [
        "bin", "Escenas", "icons",
        "LocalScripts", "ServerScripts", "ModuleScripts",
        "Assets/Sounds", "Assets/Textures",
    ]
    for carpeta in carpetas:
        os.makedirs(os.path.join(destino, carpeta), exist_ok=True)
        print(f"  📁 {carpeta}/")

    # 2. Copiar DLL(s) compiladas
    bin_origen = os.path.join(ORIGEN, "bin")
    dll_nombre = None
    if os.path.isdir(bin_origen):
        for f in os.listdir(bin_origen):
            if f.endswith(".dll") or f.endswith(".so"):
                shutil.copy2(os.path.join(bin_origen, f),
                             os.path.join(destino, "bin", f))
                print(f"  ✔ bin/{f}")
                if f.endswith(".dll") and dll_nombre is None:
                    dll_nombre = f

    if not dll_nombre:
        print("\n  ERROR: No se encontró la DLL compilada en bin/")
        print("  Por favor compila el proyecto con: scons")
        sys.exit(1)

    # 3. Copiar iconos SVG
    icons_origen = os.path.join(ORIGEN, "icons")
    count_icons = 0
    if os.path.isdir(icons_origen):
        for f in os.listdir(icons_origen):
            if f.endswith(".svg"):
                shutil.copy2(os.path.join(icons_origen, f),
                             os.path.join(destino, "icons", f))
                count_icons += 1
        if count_icons > 0:
            print(f"  ✔ icons/ ({count_icons} iconos SVG)")

    # 4. Crear icono del proyecto
    with open(os.path.join(destino, "icons", "icon.svg"), "w", encoding="utf-8") as f:
        f.write(icon_svg())
    print("  ✔ icons/icon.svg")

    # 5. Generar archivos de configuración
    uid_escena = uid_godot()
    dll_path   = f"res://bin/{dll_nombre}"

    # project.godot
    with open(os.path.join(destino, "project.godot"), "w", encoding="utf-8") as f:
        f.write(project_godot(nombre))
    print("  ✔ project.godot")

    # godot_luau.gdextension
    with open(os.path.join(destino, "godot_luau.gdextension"), "w", encoding="utf-8") as f:
        f.write(gdextension(dll_path))
    print("  ✔ godot_luau.gdextension")

    # Escena principal
    escena_contenido = scene_2d(uid_escena) if is_2d else scene_3d(uid_escena)
    with open(os.path.join(destino, "Escenas", "RobloxGame.tscn"), "w", encoding="utf-8") as f:
        f.write(escena_contenido)
    print("  ✔ Escenas/RobloxGame.tscn")

    # NOTA: Los scripts de jugador (Health, PlayerModule, ControlModule, CameraModule, GameManager)
    # son creados AUTOMÁTICAMENTE por la extensión C++ al abrir el proyecto en Godot.
    # Solo creamos archivos de ejemplo adicionales aquí.

    # 6. ModuleScript de ejemplo adicional (Utils)
    module_script = """-- > GodotLuau — PimpoliDev
-- ModuleScript de ejemplo
-- Úsalo con: local MiModulo = require(game:GetService("ReplicatedStorage"):FindFirstChild("Utils"))

local Utils = {}

function Utils.saludar(nombre)
    print("Hola, " .. (nombre or "jugador") .. "!")
end

function Utils.sumar(a, b)
    return a + b
end

return Utils
"""
    with open(os.path.join(destino, "ModuleScripts", "Utils.lua"), "w", encoding="utf-8") as f:
        f.write(module_script)
    print("  ✔ ModuleScripts/Utils.lua")

    # ─── Mensaje final ───────────────────────────────────────────
    print(f"""
╔══════════════════════════════════════════════════════╗
║  ✅  Proyecto "{nombre}" listo!                      ║
║                                                      ║
║  Modo: {modo_str:<46}║
║                                                      ║
║  COMO ABRIRLO EN GODOT:                             ║
║  1. Abre Godot 4                                    ║
║  2. Clic en "Import" en el Project Manager          ║
║  3. Navega a esta carpeta → selecciona project.godot║
║  4. Clic en "Importar y Editar"                     ║
║  5. Abre Escenas/RobloxGame.tscn                    ║
║  6. ¡La estructura Roblox se crea automáticamente!  ║
║                                                      ║
║  ESTRUCTURA AUTO-GENERADA:                          ║
║  • Game ({'RobloxGame3D' if not is_2d else 'RobloxGame2D':28})       ║
║    ├── Workspace (con CurrentCamera)                ║
║    ├── Players / Lighting / Teams...                ║
║    ├── ServerScriptService                          ║
║    │   └── GameManager (ServerScript) ← editable   ║
║    └── StarterPlayer                                ║
║        ├── StarterCharacterScripts                  ║
║        │   ├── Health.lua  ← regen vida             ║
║        │   └── Animate.lua ← animaciones            ║
║        └── StarterPlayerScripts                     ║
║            ├── PlayerModule.lua ← loader            ║
║            └── Modules/                             ║
║                ├── ControlModule.lua ← velocidad    ║
║                └── CameraModule.lua ← cámara        ║
╚══════════════════════════════════════════════════════╝
  Ruta: {destino}
""")


# ─── Punto de entrada ───────────────────────────────────────────────
if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    print(BANNER)

    # Nombre del proyecto
    nombre = input("  Nombre del proyecto (ej: MiJuegoRoblox): ").strip()
    if not nombre:
        nombre = "MiJuegoRoblox"

    # Tipo de juego
    modo = elegir_opcion(
        "¿Qué tipo de juego vas a crear?",
        [
            "3D — Mundo tridimensional (como la mayoría de juegos de Roblox)",
            "2D — Juego en dos dimensiones (plataformas, RPG top-down, etc.)",
        ]
    )

    # Si es 2D, preguntar subtipo para informar al usuario
    if modo == 1:
        subtipo = elegir_opcion(
            "¿Qué estilo de movimiento 2D prefieres? (podrás cambiarlo después en el script)",
            [
                "Platformer — Con gravedad y salto (tipo Mario, Celeste)",
                "TopDown    — Sin gravedad, 8 direcciones (tipo Zelda, RPG)",
            ]
        )
        print(f"\n  Nota: El tipo se configura en StarterPlayerScripts/Modules/ControlModule.lua")
        print(f"  Variable MOVEMENT_TYPE = {subtipo}  ({['Platformer', 'TopDown'][subtipo]})")

    # Directorio de destino
    print("\n  ¿Dónde crear el proyecto?")
    print("  (Deja en blanco para usar el Escritorio)")
    destino_base = input("  Ruta: ").strip()

    if not destino_base:
        destino_base = os.path.join(os.path.expanduser("~"), "Desktop")

    destino = os.path.join(destino_base, nombre.replace(" ", "_"))

    # Verificar si ya existe
    if os.path.exists(destino) and os.listdir(destino):
        resp = input(f"\n  La carpeta ya existe y tiene archivos. ¿Sobreescribir? (s/n): ").strip().lower()
        if resp != "s":
            print("  Cancelado.")
            sys.exit(0)
        shutil.rmtree(destino)

    os.makedirs(destino, exist_ok=True)
    crear_proyecto(destino, nombre, modo)
