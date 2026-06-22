#ifndef GL_DEBUG_H
#define GL_DEBUG_H

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

// true si el Modo Debug de GodotLuau está activo.
// Los mensajes informativos internos del sistema solo se imprimen en Modo Debug;
// los errores se imprimen siempre.
static inline bool gl_debug_on() {
    return (bool)godot::ProjectSettings::get_singleton()->get_setting("godot_luau/debug_mode", false);
}

#define GL_DEBUG_PRINT(...) do { if (gl_debug_on()) godot::UtilityFunctions::print(__VA_ARGS__); } while (0)

// Mensaje en el idioma del sistema (EN/ES/PT) — para textos de UI/runtime.
// Los nombres de nodos y clases SIEMPRE quedan en inglés (los referencian los sistemas).
static inline godot::String gl_tr3(const char* en, const char* es, const char* pt) {
    godot::String loc = godot::OS::get_singleton()->get_locale().left(2);
    if (loc == godot::String("es")) return godot::String::utf8(es);
    if (loc == godot::String("pt")) return godot::String::utf8(pt);
    return godot::String::utf8(en);
}

#endif
