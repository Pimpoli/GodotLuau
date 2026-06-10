"""
GodotLuau — Entrenador de modelos n-gram para el Autocompletado con IA
=======================================================================
Genera models/LuauGram-Plus.json a partir del corpus Luau del proyecto:
  - DefaultScripts/*.lua
  - Los templates Luau embebidos en src/luau_script.h
  - (opcional) cualquier .lua extra en la carpeta corpus/

El modelo resultante contiene bigramas Y trigramas con pesos:
  dado el/los token(s) anteriores → siguientes más probables.

Uso:
    python entrenar_modelo.py
"""
import os
import re
import sys
import json
import glob

NOMBRE   = "LuauGram-Plus"
SALIDA   = os.path.join("models", "LuauGram-Plus.json")
MIN_PESO = 1      # peso mínimo para conservar una transición
MAX_NEXT = 12     # máximo de sugerencias por contexto

TOKEN_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
TEMPLATE_RE = re.compile(r'R"LUAU\((.*?)\)LUAU"', re.DOTALL)


def tokenizar(codigo: str):
    """Tokens identificador de cada línea (sin comentarios)."""
    tokens = []
    for linea in codigo.split("\n"):
        # quitar comentarios de línea
        idx = linea.find("--")
        if idx != -1:
            linea = linea[:idx]
        tokens.extend(TOKEN_RE.findall(linea))
        tokens.append("\\n")  # separador de línea: evita n-gramas entre líneas
    return tokens


def recolectar_corpus():
    fuentes = []
    for ruta in glob.glob("DefaultScripts/*.lua"):
        with open(ruta, "r", encoding="utf-8") as f:
            fuentes.append(f.read())
    for ruta in glob.glob("corpus/*.lua"):
        with open(ruta, "r", encoding="utf-8") as f:
            fuentes.append(f.read())
    # Templates Luau embebidos en el C++
    header = os.path.join("src", "luau_script.h")
    if os.path.exists(header):
        with open(header, "r", encoding="utf-8") as f:
            for bloque in TEMPLATE_RE.findall(f.read()):
                fuentes.append(bloque)
    return fuentes


def entrenar():
    fuentes = recolectar_corpus()
    if not fuentes:
        print("✗ No se encontró corpus (.lua) para entrenar.")
        sys.exit(1)

    bigrams = {}
    trigrams = {}
    for codigo in fuentes:
        toks = tokenizar(codigo)
        for i in range(len(toks) - 1):
            a, b = toks[i], toks[i + 1]
            if a == "\\n" or b == "\\n":
                continue
            bigrams.setdefault(a, {})
            bigrams[a][b] = bigrams[a].get(b, 0) + 1
            if i + 2 < len(toks):
                c = toks[i + 2]
                if c == "\\n":
                    continue
                clave = a + " " + b
                trigrams.setdefault(clave, {})
                trigrams[clave][c] = trigrams[clave].get(c, 0) + 1

    def podar(tabla):
        limpia = {}
        for ctx, nexts in tabla.items():
            filtrados = {k: v for k, v in nexts.items() if v >= MIN_PESO}
            if not filtrados:
                continue
            top = dict(sorted(filtrados.items(), key=lambda kv: -kv[1])[:MAX_NEXT])
            limpia[ctx] = top
        return limpia

    bigrams = podar(bigrams)
    trigrams = podar(trigrams)

    parametros = sum(len(v) for v in bigrams.values()) + sum(len(v) for v in trigrams.values())

    modelo = {
        "name": NOMBRE,
        "params": parametros,
        "bigrams": bigrams,
        "trigrams": trigrams,
    }

    os.makedirs("models", exist_ok=True)
    with open(SALIDA, "w", encoding="utf-8") as f:
        json.dump(modelo, f, ensure_ascii=False, separators=(",", ":"))

    tam = os.path.getsize(SALIDA) / 1024.0
    print("=" * 54)
    print(f"  Modelo entrenado: {NOMBRE}")
    print(f"  Parámetros (pesos): {parametros:,}")
    print(f"  Contextos bigram:   {len(bigrams):,}")
    print(f"  Contextos trigram:  {len(trigrams):,}")
    print(f"  Archivo: {SALIDA} ({tam:.1f} KB)")
    print("=" * 54)


if __name__ == "__main__":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except AttributeError:
        pass
    entrenar()
