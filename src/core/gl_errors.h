#ifndef GL_ERRORS_H
#define GL_ERRORS_H

// ════════════════════════════════════════════════════════════════════
//  Reporte de errores de scripts estilo Roblox (usado por TODOS los
//  puntos que resumen corrutinas: scripts, señales, remotes, tweens...)
//
//  La linea roja del error es identica a la de Roblox porque el
//  chunkname de cada script es su ruta completa:
//      ServerScriptService.Server:5: attempt to index nil with 'Name'
//  y debajo va el stack en azul, formato Roblox exacto:
//      Stack Begin
//      Script 'ServerScriptService.Server', Line 5 - function foo
//      Stack End
//  (Luau NO desenrolla el stack de una corrutina muerta por error,
//  asi que lua_debugtrace justo despues del resume da el stack real.)
// ════════════════════════════════════════════════════════════════════

#include "lua.h"
#include "lualib.h"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <vector>
#include <string>

static const char* GL_ERR_COLOR   = "#ff5449";   // rojo error Roblox
static const char* GL_STACK_COLOR = "#5db2ff";   // azul info Roblox
static const char* GL_WARN_COLOR  = "#ff9e2c";   // naranja warning Roblox

// Ruta completa estilo Roblox ("ServerScriptService.Server.Modulo").
// Se usa en GetFullName, en los mensajes de error y como chunkname de scripts.
static godot::String gl_instance_fullname(godot::Node* n) {
    std::vector<std::string> parts;
    godot::Node* cur = n;
    while (cur && cur->get_parent()) {
        parts.push_back(godot::String(cur->get_name()).utf8().get_data());
        cur = cur->get_parent();
    }
    // Excluir el nodo raiz de la escena (el "game"): Roblox no lo incluye
    if (parts.size() > 1) parts.pop_back();
    std::string result;
    for (int i = (int)parts.size() - 1; i >= 0; i--) {
        if (!result.empty()) result += ".";
        result += parts[i];
    }
    return godot::String(result.c_str());
}

// Convierte un traceback de lua_debugtrace ("chunk:12 function foo\n...")
// al bloque Roblox "Stack Begin / Script '...', Line N - function foo / Stack End".
static godot::String gl_format_roblox_stack(const godot::String& trace) {
    godot::String out = godot::String("[color=") + GL_STACK_COLOR + "]Stack Begin[/color]";
    godot::PackedStringArray lines = trace.split("\n", false);
    for (int i = 0; i < lines.size(); i++) {
        godot::String ln = lines[i];
        if (ln.begins_with("[C]")) continue;   // Roblox no muestra frames C
        godot::String fn;
        int fpos = ln.find(" function ");
        if (fpos >= 0) { fn = ln.substr(fpos + 10); ln = ln.substr(0, fpos); }
        int c = ln.rfind(":");
        godot::String chunk = ln, lnum;
        if (c >= 0) { chunk = ln.substr(0, c); lnum = ln.substr(c + 1); }
        godot::String row = godot::String("Script '") + chunk + "'";
        if (!lnum.is_empty()) row += godot::String(", Line ") + lnum;
        if (!fn.is_empty())   row += godot::String(" - function ") + fn;
        out += godot::String("\n[color=") + GL_STACK_COLOR + "]" + row + "[/color]";
    }
    out += godot::String("\n[color=") + GL_STACK_COLOR + "]Stack End[/color]";
    return out;
}

// Imprime el error que esta en el tope de la pila de `th` + el stack dado.
static void gl_report_script_error_with_trace(lua_State* th, const godot::String& trace) {
    const char* raw = lua_tostring(th, -1);
    godot::String msg = raw ? godot::String::utf8(raw) : godot::String("Unknown error");
    godot::UtilityFunctions::print_rich(godot::String("[color=") + GL_ERR_COLOR + "]" + msg + "[/color]");
    godot::UtilityFunctions::print_rich(gl_format_roblox_stack(trace));
}

// Version directa: usable tras lua_resume fallido (el stack sigue vivo).
static void gl_report_script_error(lua_State* th) {
    gl_report_script_error_with_trace(th, godot::String::utf8(lua_debugtrace(th)));
}

// Linea roja suelta (errores de sintaxis / require): sin stack.
static void gl_report_error_line(const godot::String& msg) {
    godot::UtilityFunctions::print_rich(godot::String("[color=") + GL_ERR_COLOR + "]" + msg + "[/color]");
}

// Para lua_pcall (que SI desenrolla el stack): message handler que captura el
// traceback EN el momento del error; leerlo despues con gl_pcall_trace().
// (static local, NO global: un String global se construiria al cargar la DLL,
// antes de que godot-cpp este inicializado, y revienta la carga con error 1114)
static godot::String& gl_pcall_trace() {
    static godot::String s;
    return s;
}
static int gl_trace_handler(lua_State* L) {
    gl_pcall_trace() = godot::String::utf8(lua_debugtrace(L));
    return 1;   // devuelve el mismo objeto de error
}

// Chequeo estandar tras lua_resume de un callback de usuario.
static void gl_check_resume(lua_State* th, int status) {
    if (status != LUA_OK && status != LUA_YIELD) gl_report_script_error(th);
}

#endif // GL_ERRORS_H
