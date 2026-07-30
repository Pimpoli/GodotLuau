"""
GodotLuau — Generador del header de plantillas Luau
====================================================
Convierte los .luau de GodotLuau/DefaultScripts/ en el header C++
src/core/luau_templates_gen.h (las constantes LUAU_TEMPLATE_*), para que la
DLL siga siendo autocontenida y funcione aunque la carpeta no exista.

Los .luau son la FUENTE DE LA VERDAD: se editan con autocompletado como
cualquier script normal y en tiempo de ejecucion el motor los lee de disco
(carga en caliente, ver gl_luau_template en src/core/luau_script.h). El header
solo es la copia embebida de reserva.

Ejecutar SIEMPRE tras editar un .luau y ANTES de compilar:
    python tools/generar_plantillas_luau.py
"""
import os
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CARPETA = os.path.join(RAIZ, "GodotLuau", "DefaultScripts")
SALIDA = os.path.join(RAIZ, "src", "core", "luau_templates_gen.h")

# ─────────────────────────────────────────────────────────────────────
#  Mapa plantilla → constante C++
#  La RUTA espeja donde cuelga el script en el arbol de Roblox (lo que ve
#  el usuario en el explorer), y el NOMBRE de la constante es el que ya
#  usaba el codigo: NO renombrar ninguno de los dos sin tocar
#  gl_builtin_template() en src/core/luau_script.h.
# ─────────────────────────────────────────────────────────────────────
PLANTILLAS = [
    ("StarterPlayer/StarterCharacterScripts/Health.luau",                        "LUAU_TEMPLATE_HEALTH"),
    ("StarterPlayer/StarterCharacterScripts/Animate.luau",                       "LUAU_TEMPLATE_ANIMATE"),
    ("StarterPlayer/StarterPlayerScripts/PlayerModule.luau",                     "LUAU_TEMPLATE_PLAYER_MODULE"),
    ("StarterPlayer/StarterPlayerScripts/Modules/ControlModule.luau",            "LUAU_TEMPLATE_CONTROL_MODULE"),
    ("StarterPlayer/StarterPlayerScripts/Modules/ControlModule/PCModule.luau",   "LUAU_TEMPLATE_PC_MODULE"),
    ("StarterPlayer/StarterPlayerScripts/Modules/ControlModule/MobileModule.luau", "LUAU_TEMPLATE_MOBILE_MODULE"),
    ("StarterPlayer/StarterPlayerScripts/Modules/ControlModule/ConsoleModule.luau", "LUAU_TEMPLATE_CONSOLE_MODULE"),
    ("StarterPlayer/StarterPlayerScripts/Modules/ChatModule.luau",               "LUAU_TEMPLATE_CHAT_MODULE"),
    ("StarterPlayer/StarterPlayerScripts/Modules/CameraModule.luau",             "LUAU_TEMPLATE_CAMERA_MODULE"),
    ("StarterPlayer/StarterPlayerScripts/Modules/SettingsModule.luau",           "LUAU_TEMPLATE_SETTINGS_MODULE"),
    ("StarterPlayer/StarterPlayerScripts/Modules/Menu.luau",                     "LUAU_TEMPLATE_MENU"),
    ("StarterPlayer/StarterPlayerScripts/Modules/Menu/MenuUi.luau",              "LUAU_TEMPLATE_MENU_UI"),
    ("StarterPlayer/StarterPlayerScripts/Modules/Menu/Settings.luau",            "LUAU_TEMPLATE_MENU_SETTINGS"),
    ("StarterPlayer/StarterPlayerScripts/Modules/Menu/Players.luau",             "LUAU_TEMPLATE_MENU_PLAYERS"),
    ("ServerScriptService/GameManager.luau",                                     "LUAU_TEMPLATE_GAME_MANAGER"),
]

# Delimitadores candidatos para el raw string. Si el .luau contiene la
# secuencia )LUAU" el literal se cerraria antes de tiempo → probamos el
# siguiente hasta encontrar uno que no aparezca en el codigo.
DELIMITADORES = ["LUAU", "LUAU_X", "LUAU_XX", "GL_LUAU_RAW"]

CABECERA = """#ifndef GL_LUAU_TEMPLATES_GEN_H
#define GL_LUAU_TEMPLATES_GEN_H

// ════════════════════════════════════════════════════════════════════
//  GENERADO AUTOMATICAMENTE — NO EDITAR A MANO
//
//  Este header lo escribe tools/generar_plantillas_luau.py a partir de
//  los archivos .luau de GodotLuau/DefaultScripts/. Edita el .luau, no
//  esto: cualquier cambio aqui se pierde al regenerar.
//
//  Por que existe: las plantillas viven como .luau de verdad (con
//  autocompletado y faciles de encontrar), pero la DLL tiene que seguir
//  funcionando sin esa carpeta (juego exportado, instalacion parcial).
//  Estas constantes son esa copia embebida de reserva; en ejecucion
//  gl_luau_template() prefiere el archivo de disco si existe.
//
//  Tras tocar un .luau:  python tools/generar_plantillas_luau.py
// ════════════════════════════════════════════════════════════════════
"""


def elegir_delimitador(codigo, rel):
    """Devuelve un delimitador de raw string que el codigo no cierre por accidente."""
    for delim in DELIMITADORES:
        if (")" + delim + '"') not in codigo:
            return delim
    raise SystemExit(
        "✗  %s contiene todas las secuencias de cierre probadas; anade otro\n"
        "   delimitador a DELIMITADORES en tools/generar_plantillas_luau.py" % rel)


def leer_plantilla(rel):
    """Lee un .luau como texto con saltos de linea normalizados a \\n."""
    ruta = os.path.join(CARPETA, rel.replace("/", os.sep))
    if not os.path.isfile(ruta):
        raise SystemExit("✗  Falta el archivo: GodotLuau/DefaultScripts/%s" % rel)
    with open(ruta, "rb") as f:
        datos = f.read()
    if datos.startswith(b"\xef\xbb\xbf"):
        datos = datos[3:]   # el BOM acabaria dentro del codigo Luau
    codigo = datos.decode("utf-8").replace("\r\n", "\n").replace("\r", "\n")
    if not codigo.strip():
        raise SystemExit("✗  Plantilla vacia: GodotLuau/DefaultScripts/%s" % rel)
    if not codigo.endswith("\n"):
        codigo += "\n"      # el )LUAU"; tiene que quedar al principio de linea
    return codigo


def avisar_huerfanos():
    """Avisa de .luau presentes en la carpeta que nadie mapea a una constante."""
    conocidos = set(rel.replace("/", os.sep) for rel, _ in PLANTILLAS)
    for raiz, _, archivos in os.walk(CARPETA):
        for archivo in archivos:
            if not archivo.endswith(".luau"):
                continue
            rel = os.path.relpath(os.path.join(raiz, archivo), CARPETA)
            if rel not in conocidos:
                print("⚠  %s no esta en PLANTILLAS: no entra en la DLL" % rel.replace(os.sep, "/"))


def generar():
    print("=" * 60)
    print("  GodotLuau — Plantillas .luau → luau_templates_gen.h")
    print("=" * 60)

    partes = [CABECERA]
    total = 0
    for rel, constante in PLANTILLAS:
        codigo = leer_plantilla(rel)
        delim = elegir_delimitador(codigo, rel)
        # El literal arranca con un salto de linea justo despues del R"LUAU(
        # para que el codigo empiece en su propia linea: asi el valor de la
        # constante es identico al que tenian los literales escritos a mano.
        partes.append("\n// ── %s ──\n" % rel)
        partes.append('static const char* %s_PATH = "%s";\n' % (constante, rel))
        partes.append('static const char* %s = R"%s(\n%s)%s";\n' % (constante, delim, codigo, delim))
        total += len(codigo.encode("utf-8"))
        print("✓  %-46s → %s" % (rel, constante))

    partes.append("\n#endif // GL_LUAU_TEMPLATES_GEN_H\n")
    texto = "".join(partes)

    # Se escribe con CRLF como el resto de src/ (el compilador convierte los
    # saltos de los raw strings a \n, igual que hacia con los literales viejos).
    with open(SALIDA, "wb") as f:
        f.write(texto.replace("\n", "\r\n").encode("utf-8"))

    avisar_huerfanos()
    print()
    print("  %d plantillas, %d bytes de Luau" % (len(PLANTILLAS), total))
    print("  Escrito: src/core/luau_templates_gen.h")
    print("=" * 60)


if __name__ == "__main__":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except AttributeError:
        pass
    generar()
