#!/usr/bin/env python

env = SConscript("godot-cpp/SConstruct")

# 0. Luau usa excepciones de C++ en su bucle de ejecucion (luau/VM/src/ldo.cpp).
#    godot-cpp compila con -fno-exceptions por defecto, lo que impide construir
#    Luau. Reactivamos las excepciones para TODA la extension para que el
#    proyecto compile sin tener que pasar flags extra en la linea de comandos.
def _enable_exceptions(e):
    for key in ("CCFLAGS", "CXXFLAGS", "CFLAGS"):
        if key in e:
            e[key] = [f for f in e[key] if str(f) != "-fno-exceptions"]
    if e.get("is_msvc", False):
        e.Append(CXXFLAGS=["/EHsc"])
    else:
        e.Append(CXXFLAGS=["-fexceptions"])
_enable_exceptions(env)

# 1. Rutas de inclusion para que SCons encuentre los encabezados
env.Append(CPPPATH=[
    "src/",
    "luau/VM/include",
    "luau/Compiler/include",
    "luau/Ast/include",
    "luau/Common/include"
])

# 2. Archivos de nuestra extensión (Tus scripts)
sources = Glob("src/*.cpp")

# 3. Archivos del "Cerebro" de Luau (Agregamos la carpeta src de Common)
sources += Glob("luau/VM/src/*.cpp")
sources += Glob("luau/Compiler/src/*.cpp")
sources += Glob("luau/Ast/src/*.cpp")
sources += Glob("luau/Common/src/*.cpp")

# 4. Forzamos el nombre de la DLL para que Godot la detecte
target_name = "bin/godot_luau" + env["suffix"] + env["SHLIBSUFFIX"]

library = env.SharedLibrary(target=target_name, source=sources)

Default(library)