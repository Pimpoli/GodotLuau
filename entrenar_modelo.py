"""
GodotLuau — Entrenador de la familia de modelos LuauIA
========================================================
Entrena un modelo n-gram completo (bigramas + trigramas + 4-gramas) y lo
"destila" en 4 niveles podando por peso, como una familia de modelos real:

    LuauIA-Mini      ~10.000 parámetros   (el más rápido)
    LuauIA-Medium    ~25.000 parámetros
    LuauIA-High      ~62.500 parámetros
    LuauIA-HighPRO   ~156.250 parámetros  (el más inteligente)

Corpus:
  - corpus/*.lua                 (código idiomático curado — peso x10)
  - DefaultScripts/*.lua         (controladores reales — peso x10)
  - templates de src/luau_script.h (peso x10)
  - Scripts/globalTypes.d.luau   (definiciones de la API Roblox — peso x1)

Uso:
    python entrenar_modelo.py
"""
import os
import re
import sys
import json
import glob

NIVELES = [
    ("LuauIA-Mini",     10000),
    ("LuauIA-Medium",   25000),
    ("LuauIA-High",     62500),
    ("LuauIA-HighPRO", 156250),
]
MAX_NEXT     = 40   # máximo de sugerencias por contexto
CORPUS_BOOST = 10   # el código idiomático pesa más que las definiciones de API

TOKEN_RE    = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
TEMPLATE_RE = re.compile(r'R"LUAU\((.*?)\)LUAU"', re.DOTALL)

# Tokens de definiciones de tipos que no aportan al autocompletado de código
SKIP_TOKENS = {"declare", "export", "extends", "typeof", "keyof", "index"}
BAD_NEXT    = {"number", "boolean", "any", "unknown", "never", "void", "T"}


def tokenizar(codigo: str):
    tokens = []
    for linea in codigo.split("\n"):
        idx = linea.find("--")
        if idx != -1:
            linea = linea[:idx]
        for tok in TOKEN_RE.findall(linea):
            if tok in SKIP_TOKENS:
                tokens.append("\\n")  # corta los n-gramas en ese punto
            else:
                tokens.append(tok)
        tokens.append("\\n")
    return tokens


def acumular(tablas, toks, boost):
    bigrams, trigrams, fourgrams = tablas
    n = len(toks)
    for i in range(n - 1):
        a, b = toks[i], toks[i + 1]
        if a == "\\n" or b == "\\n" or b in BAD_NEXT:
            continue
        bigrams.setdefault(a, {})
        bigrams[a][b] = bigrams[a].get(b, 0) + boost
        if i + 2 < n:
            c = toks[i + 2]
            if c != "\\n" and c not in BAD_NEXT:
                k = a + " " + b
                trigrams.setdefault(k, {})
                trigrams[k][c] = trigrams[k].get(c, 0) + boost
                if i + 3 < n:
                    d = toks[i + 3]
                    if d != "\\n" and d not in BAD_NEXT:
                        k4 = a + " " + b + " " + c
                        fourgrams.setdefault(k4, {})
                        fourgrams[k4][d] = fourgrams[k4].get(d, 0) + boost


CLASS_RE  = re.compile(r"declare class (\w+)")
PROP_RE   = re.compile(r"^\s+(\w+):\s*(.+?)\s*$")
FUNC_RE   = re.compile(r"^\s+function (\w+)\s*\(")


def sintetizar_desde_globaltypes(texto: str) -> str:
    """Genera código idiomático sintético desde las definiciones de la API:
    por cada propiedad/método/evento real crea las líneas que un usuario
    escribiría. Esto multiplica el corpus con patrones de altísima calidad."""
    lineas = []
    clase = None
    instancia = None
    for linea in texto.split("\n"):
        m = CLASS_RE.search(linea)
        if m:
            clase = m.group(1)
            instancia = clase[0].lower() + clase[1:]
            lineas.append("local %s = Instance.new(\"%s\")" % (instancia, clase))
            continue
        if not clase:
            continue
        fm = FUNC_RE.match(linea)
        if fm:
            metodo = fm.group(1)
            if not metodo.startswith("_"):
                lineas.append("%s:%s()" % (instancia, metodo))
                lineas.append("local resultado = %s:%s()" % (instancia, metodo))
            continue
        pm = PROP_RE.match(linea)
        if pm:
            prop, tipo = pm.group(1), pm.group(2)
            if prop.startswith("_"):
                continue
            if "RBXScriptSignal" in tipo:
                lineas.append("%s.%s:Connect(function()" % (instancia, prop))
                lineas.append("end)")
            else:
                lineas.append("local valor = %s.%s" % (instancia, prop))
                lineas.append("%s.%s = valor" % (instancia, prop))
    return "\n".join(lineas)


def recolectar():
    """Devuelve [(texto, boost), ...]"""
    fuentes = []
    for patron in ("corpus/*.lua", "DefaultScripts/*.lua"):
        for ruta in glob.glob(patron):
            with open(ruta, "r", encoding="utf-8") as f:
                fuentes.append((f.read(), CORPUS_BOOST))
    header = os.path.join("src", "luau_script.h")
    if os.path.exists(header):
        with open(header, "r", encoding="utf-8") as f:
            for bloque in TEMPLATE_RE.findall(f.read()):
                fuentes.append((bloque, CORPUS_BOOST))
    # stdlib Luau embebida: código Lua real del runtime (señales, clases, etc.)
    stdlib = os.path.join("src", "luau_stdlib.h")
    if os.path.exists(stdlib):
        with open(stdlib, "r", encoding="utf-8") as f:
            for bloque in TEMPLATE_RE.findall(f.read()):
                fuentes.append((bloque, 5))
    gtypes = os.path.join("Scripts", "globalTypes.d.luau")
    if os.path.exists(gtypes):
        with open(gtypes, "r", encoding="utf-8") as f:
            contenido = f.read()
        fuentes.append((contenido, 1))
        # Corpus sintético: uso idiomático de toda la API (clase por clase)
        fuentes.append((sintetizar_desde_globaltypes(contenido), 2))
    # Base de datos de la API (nombres de clases/propiedades/métodos)
    dtypes = os.path.join("Scripts", "DataTypes.json")
    if os.path.exists(dtypes):
        with open(dtypes, "r", encoding="utf-8") as f:
            fuentes.append((f.read(), 1))
    return fuentes


def entrenar():
    fuentes = recolectar()
    if not fuentes:
        print("✗ Sin corpus para entrenar.")
        sys.exit(1)

    bigrams, trigrams, fourgrams = {}, {}, {}
    for texto, boost in fuentes:
        acumular((bigrams, trigrams, fourgrams), tokenizar(texto), boost)

    # Recortar cada contexto a sus MAX_NEXT mejores y aplanar todo en una
    # lista (orden, contexto, siguiente, peso) para poder podar por niveles
    entradas = []
    for orden, tabla in (("bigrams", bigrams), ("trigrams", trigrams), ("fourgrams", fourgrams)):
        for ctx, nexts in tabla.items():
            top = sorted(nexts.items(), key=lambda kv: -kv[1])[:MAX_NEXT]
            for nxt, peso in top:
                entradas.append((orden, ctx, nxt, peso))

    total = len(entradas)
    print(f"Modelo completo: {total:,} parámetros disponibles")
    entradas.sort(key=lambda e: -e[3])  # mejores pesos primero

    os.makedirs("models", exist_ok=True)
    print("=" * 58)
    for nombre, objetivo in NIVELES:
        sel = entradas[:min(objetivo, total)]
        modelo = {"name": nombre, "params": len(sel),
                  "bigrams": {}, "trigrams": {}, "fourgrams": {}}
        for orden, ctx, nxt, peso in sel:
            modelo[orden].setdefault(ctx, {})[nxt] = peso
        ruta = os.path.join("models", nombre + ".json")
        with open(ruta, "w", encoding="utf-8") as f:
            json.dump(modelo, f, ensure_ascii=False, separators=(",", ":"))
        tam = os.path.getsize(ruta) / 1024.0
        print(f"  {nombre:<18} {len(sel):>8,} parámetros   {tam:>8.1f} KB")
    print("=" * 58)


if __name__ == "__main__":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except AttributeError:
        pass
    entrenar()
