"""
GodotLuau — Generador de plantilla de proyecto
================================================
Genera el archivo RobloxTemplate.zip que Godot puede importar
directamente como un proyecto nuevo listo para usar.

Uso:
    python crear_plantilla.py
"""
import os
import zipfile
import glob
import random
import string
import textwrap


# ─────────────────────────────────────────────────────────────────────
#  UID válido para Godot 4 (13 caracteres alfanuméricos en minúscula)
# ─────────────────────────────────────────────────────────────────────
def uid_godot():
    chars = string.digits + string.ascii_lowercase
    return "uid://" + "".join(random.choices(chars, k=13))


# ─────────────────────────────────────────────────────────────────────
#  CONTENIDO DE LA ESCENA PRINCIPAL
#  Un solo nodo RobloxTemplate — al abrirlo en Godot crea todos
#  los servicios automáticamente.
# ─────────────────────────────────────────────────────────────────────
def escena_principal() -> str:
    return textwrap.dedent(f"""\
        [gd_scene format=3 uid="{uid_godot()}"]

        [node name="RobloxGame" type="RobloxTemplate"]
        WorkspaceMode = 0
    """)

# ─────────────────────────────────────────────────────────────────────
#  project.godot de la plantilla
# ─────────────────────────────────────────────────────────────────────
PROJECT_GODOT = textwrap.dedent("""\
    ; Engine configuration file.
    ; Generado por GodotLuau

    config_version=5

    [application]

    config/name="Mi Juego Roblox"
    run/main_scene="res://Escenas/RobloxGame.tscn"
    config/features=PackedStringArray("4.6")

    [editor_plugins]

    enabled=PackedStringArray("res://addons/GodotLuauUpdater/plugin.cfg")
""")


def generar_gdextension(dll_debug: str, dll_release: str) -> str:
    return textwrap.dedent(f"""\
        [configuration]
        entry_symbol = "luau_extension_init"
        compatibility_minimum = "4.3"

        [libraries]
        windows.debug.x86_64   = "{dll_debug}"
        windows.release.x86_64 = "{dll_release}"
    """)


def empaquetar_plantilla():
    nombre_zip = "RobloxTemplate.zip"
    print("=" * 54)
    print("  GodotLuau — Empaquetando plantilla de proyecto")
    print("=" * 54)

    # 1. Buscar la DLL compilada (debug y release por separado, sin depender del orden)
    dlls = glob.glob("bin/*.dll")
    if not dlls:
        print("⚠  No se encontró DLL en 'bin/'. Compila primero con: python -m SCons platform=windows target=template_debug")
        return
    debug   = next((d for d in dlls if "template_debug"   in d), None)
    release = next((d for d in dlls if "template_release" in d), None)
    # Si solo existe una, se usa para ambas entradas
    debug   = debug   or release or dlls[0]
    release = release or debug
    dll_debug   = "res://" + debug.replace("\\", "/")
    dll_release = "res://" + release.replace("\\", "/")
    print(f"✓  DLL debug:   {debug}")
    print(f"✓  DLL release: {release}")

    gdext_content = generar_gdextension(dll_debug, dll_release)

    try:
        with zipfile.ZipFile(nombre_zip, "w", zipfile.ZIP_DEFLATED) as zf:

            # ── Archivos generados en memoria ────────────────────────
            zf.writestr("godot_luau.gdextension",         gdext_content)
            zf.writestr("project.godot",                  PROJECT_GODOT)
            zf.writestr("Escenas/RobloxGame.tscn",        escena_principal())
            print("✓  Archivos de configuración incluidos")

            # ── DLL compilada ────────────────────────────────────────
            for dll in dlls:
                zf.write(dll, arcname=dll.replace("\\", "/"))
                print(f"✓  {dll}")

            # ── Addon (panel de config + updater) y Version ──────────
            if os.path.isdir("addons/GodotLuauUpdater"):
                for raiz, _, archivos in os.walk("addons/GodotLuauUpdater"):
                    for archivo in archivos:
                        ruta = os.path.join(raiz, archivo)
                        zf.write(ruta, arcname=ruta.replace("\\", "/"))
                print("✓  addons/GodotLuauUpdater/")
            if os.path.exists("Version"):
                zf.write("Version", arcname="Version")
                print("✓  Version")

            # ── Carpetas de scripts Luau (siempre vacías en la plantilla) ──
            # NO incluimos scripts del proyecto fuente, solo las carpetas
            carpetas_luau = [
                "Scripts/",
                "LocalScripts/",
                "ServerScripts/",
                "ModuleScripts/",
            ]
            for carpeta in carpetas_luau:
                zf.writestr(carpeta + ".gitkeep", "")
                print(f"✓  {carpeta} (vacía)")

            # ── Iconos personalizados si existen ─────────────────────
            if os.path.isdir("icons"):
                for raiz, _, archivos in os.walk("icons"):
                    for archivo in archivos:
                        ruta = os.path.join(raiz, archivo)
                        # arcname con '/' (en Windows os.path.join usa '\\', que
                        # produce rutas inválidas dentro del ZIP)
                        zf.write(ruta, arcname=ruta.replace("\\", "/"))
                print("✓  icons/")

        print()
        print("=" * 54)
        print(f"  ¡Listo! Plantilla guardada en: {nombre_zip}")
        print()
        print("  Cómo usarla en Godot:")
        print("  1. Abre Godot 4")
        print("  2. Project Manager → Import")
        print("  3. Selecciona este ZIP")
        print("  4. Crea el proyecto en una carpeta nueva")
        print("  5. Abre Escenas/RobloxGame.tscn")
        print("  6. ¡Todos los servicios de Roblox aparecen solos!")
        print("=" * 54)

    except Exception as e:
        print(f"✗  Error: {e}")


if __name__ == "__main__":
    empaquetar_plantilla()
