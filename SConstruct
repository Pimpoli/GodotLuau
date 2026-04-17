#!/usr/bin/env python

env = SConscript("godot-cpp/SConstruct")

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