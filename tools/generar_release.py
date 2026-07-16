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
    "LICENSE",
]
CARPETAS = [
    "addons/GodotLuauUpdater",
    "GodotLuau",   # TODO lo de la extension vive aqui (bin, icons, assets, shaders, DefaultScripts...)
]
EXTENSIONES_BIN = (".dll", ".so", ".dylib", ".lib", ".exp")
EXTENSIONES_TEXTO = (".cfg", ".gd", ".gdextension", ".txt", ".md", ".json", ".lua", ".uid")

# Binarios por plataforma (para armar zips por plataforma en el release).
BIN_POR_PLATAFORMA = {
    "windows": (".dll", ".lib", ".exp"),
    "linux":   (".so",),
    "macos":   (".dylib",),
}


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


def empaquetar(nombre_zip=NOMBRE_ZIP, only=None):
    print("=" * 54)
    print(f"  GodotLuau — Empaquetando {nombre_zip}"
          + (f" (solo {only})" if only else " (todas las plataformas)"))
    print("=" * 54)

    # Qué binarios incluir en /bin: todos, o solo los de una plataforma.
    bin_ok = BIN_POR_PLATAFORMA.get(only, EXTENSIONES_BIN) if only else EXTENSIONES_BIN

    with zipfile.ZipFile(nombre_zip, "w", zipfile.ZIP_DEFLATED) as zf:
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
                    if raiz_norm.endswith("/bin"):
                        if not archivo.endswith(EXTENSIONES_BIN):
                            continue
                        if only and not archivo.endswith(bin_ok):
                            continue   # binario de otra plataforma → fuera de este zip
                    ruta = os.path.join(raiz, archivo)
                    agregar_archivo(zf, ruta, ruta.replace("\\", "/"))
            print(f"✓  {carpeta}/")

    sha = hashlib.sha256(open(nombre_zip, "rb").read()).hexdigest()
    with open(nombre_zip + ".sha256", "w") as f:
        f.write(sha + "\n")

    tam = os.path.getsize(nombre_zip) / 1024.0
    print()
    print(f"  ¡Listo! {nombre_zip} ({tam:.0f} KB)")
    print(f"  SHA-256: {sha}")
    print("=" * 54)


if __name__ == "__main__":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except AttributeError:
        pass
    # Uso:
    #   python generar_release.py                 → GodotLuau.zip (todas las plataformas)
    #   python generar_release.py --only windows --out "Windows GodotLuau.zip"
    #   python generar_release.py --only linux   --out "Linux GodotLuau.zip"
    argv = sys.argv[1:]
    only = None
    out = NOMBRE_ZIP
    i = 0
    while i < len(argv):
        if argv[i] == "--only" and i + 1 < len(argv):
            only = argv[i + 1]; i += 2
        elif argv[i] == "--out" and i + 1 < len(argv):
            out = argv[i + 1]; i += 2
        else:
            i += 1
    empaquetar(out, only)
