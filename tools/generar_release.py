"""
GodotLuau — Generador del paquete de release
=============================================
Regenera GodotLuau.zip (el paquete que descarga el auto-updater)
y su hash GodotLuau.zip.sha256 para la verificación de integridad.

Ejecutar SIEMPRE después de recompilar las DLLs y antes de hacer push:
    python generar_release.py
"""
import os
import sys
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
    "GodotLuau",   # TODO lo de la extension vive aqui (bin, icons, assets, shaders, DefaultScripts...)
]
EXTENSIONES_BIN = (".dll", ".so", ".dylib", ".lib", ".exp")
EXTENSIONES_TEXTO = (".cfg", ".gd", ".gdextension", ".txt", ".md", ".json", ".lua", ".uid")


def agregar_archivo(zf, ruta, arcname):
    """Agrega un archivo al zip quitando el BOM UTF-8 de los archivos de texto.
    Godot no tolera BOM en .cfg/.gdextension (el plugin deja de cargar)."""
    if ruta.lower().endswith(EXTENSIONES_TEXTO):
        with open(ruta, "rb") as f:
            data = f.read()
        if data.startswith(b"\xef\xbb\xbf"):
            data = data[3:]
            # tambien arreglar el archivo original en disco
            with open(ruta, "wb") as f:
                f.write(data)
            print(f"⚠  BOM eliminado: {arcname}")
        zf.writestr(arcname, data)
    else:
        zf.write(ruta, arcname=arcname)


def empaquetar():
    print("=" * 54)
    print("  GodotLuau — Empaquetando release del updater")
    print("=" * 54)

    with zipfile.ZipFile(NOMBRE_ZIP, "w", zipfile.ZIP_DEFLATED) as zf:
        for archivo in ARCHIVOS_SUELTOS:
            if os.path.exists(archivo):
                agregar_archivo(zf, archivo, archivo)
                print(f"✓  {archivo}")

        for carpeta in CARPETAS:
            if not os.path.isdir(carpeta):
                print(f"⚠  No existe: {carpeta}/ (omitida)")
                continue
            for raiz, _, archivos in os.walk(carpeta):
                for archivo in archivos:
                    raiz_norm = raiz.replace("\\", "/")
                    if raiz_norm.endswith("/bin") and not archivo.endswith(EXTENSIONES_BIN):
                        continue
                    ruta = os.path.join(raiz, archivo)
                    agregar_archivo(zf, ruta, ruta.replace("\\", "/"))
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
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except AttributeError:
        pass
    empaquetar()
