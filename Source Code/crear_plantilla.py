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
import textwrap


# ─────────────────────────────────────────────────────────────────────
#  CONTENIDO DE LA ESCENA PRINCIPAL
#  Un solo nodo RobloxDataModel — al abrirlo en Godot crea todos
#  los servicios automáticamente.
# ─────────────────────────────────────────────────────────────────────
ESCENA_PRINCIPAL = textwrap.dedent("""\
    [gd_scene format=3 uid="uid://roblox_game_template"]

    [node name="RobloxGame" type="RobloxDataModel"]
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
""")


def generar_gdextension(dll_path: str) -> str:
    return textwrap.dedent(f"""\
        [configuration]
        entry_symbol = "luau_extension_init"
        compatibility_minimum = 4.1

        [libraries]
        windows.debug.x86_64   = "{dll_path}"
        windows.release.x86_64 = "{dll_path.replace('template_debug', 'template_release')}"
    """)


def empaquetar_plantilla():
    nombre_zip = "RobloxTemplate.zip"
    print("=" * 54)
    print("  GodotLuau — Empaquetando plantilla de proyecto")
    print("=" * 54)

    # 1. Buscar la DLL compilada
    dlls = glob.glob("bin/*.dll")
    if not dlls:
        print("⚠  No se encontró DLL en 'bin/'. Compila primero con: python -m SCons platform=windows target=template_debug")
        return
    dll_path = "res://" + dlls[0].replace("\\", "/")
    print(f"✓  DLL encontrada: {dlls[0]}")

    gdext_content = generar_gdextension(dll_path)

    try:
        with zipfile.ZipFile(nombre_zip, "w", zipfile.ZIP_DEFLATED) as zf:

            # ── Archivos generados en memoria ────────────────────────
            zf.writestr("godot_luau.gdextension",         gdext_content)
            zf.writestr("project.godot",                  PROJECT_GODOT)
            zf.writestr("Escenas/RobloxGame.tscn",        ESCENA_PRINCIPAL)
            print("✓  Archivos de configuración incluidos")

            # ── DLL compilada ────────────────────────────────────────
            for dll in dlls:
                zf.write(dll, arcname=dll)
                print(f"✓  {dll}")

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
                        zf.write(ruta, arcname=ruta)
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
