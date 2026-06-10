"""
GodotLuau — Generador del paquete de release
=============================================
Regenera GodotLuau.zip (el paquete que descarga el auto-updater)
y su hash GodotLuau.zip.sha256 para la verificación de integridad.

Ejecutar SIEMPRE después de recompilar las DLLs y antes de hacer push:
    python generar_release.py
"""
import os
import hashlib
import zipfile

NOMBRE_ZIP = "GodotLuau.zip"

# Contenido del paquete de release (lo que instala el updater en res://)
ARCHIVOS_SUELTOS = [
    "godot_luau.gdextension",
    "godot_luau.gdextension.uid",
]
CARPETAS = [
    "addons/GodotLuauUpdater",
    "bin",
    "DefaultScripts",
    "icons",
]
EXTENSIONES_BIN = (".dll", ".so", ".dylib", ".lib", ".exp")


def empaquetar():
    print("=" * 54)
    print("  GodotLuau — Empaquetando release del updater")
    print("=" * 54)

    with zipfile.ZipFile(NOMBRE_ZIP, "w", zipfile.ZIP_DEFLATED) as zf:
        for archivo in ARCHIVOS_SUELTOS:
            if os.path.exists(archivo):
                zf.write(archivo, arcname=archivo)
                print(f"✓  {archivo}")

        for carpeta in CARPETAS:
            if not os.path.isdir(carpeta):
                print(f"⚠  No existe: {carpeta}/ (omitida)")
                continue
            for raiz, _, archivos in os.walk(carpeta):
                for archivo in archivos:
                    if carpeta == "bin" and not archivo.endswith(EXTENSIONES_BIN):
                        continue
                    ruta = os.path.join(raiz, archivo)
                    zf.write(ruta, arcname=ruta.replace("\\", "/"))
            print(f"✓  {carpeta}/")

    sha = hashlib.sha256(open(NOMBRE_ZIP, "rb").read()).hexdigest()
    with open(NOMBRE_ZIP + ".sha256", "w") as f:
        f.write(sha + "\n")

    tam = os.path.getsize(NOMBRE_ZIP) / 1024.0
    print()
    print(f"  ¡Listo! {NOMBRE_ZIP} ({tam:.0f} KB)")
    print(f"  SHA-256: {sha}")
    print("  Recuerda subir AMBOS archivos (.zip y .zip.sha256) en el mismo commit.")
    print("=" * 54)


if __name__ == "__main__":
    empaquetar()
