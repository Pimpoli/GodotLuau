#ifndef GL_DEBUG_H
#define GL_DEBUG_H

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

// true si el Modo Debug de GodotLuau está activo.
// Los mensajes informativos internos del sistema solo se imprimen en Modo Debug;
// los errores se imprimen siempre.
static inline bool gl_debug_on() {
    return (bool)godot::ProjectSettings::get_singleton()->get_setting("godot_luau/debug_mode", false);
}

#define GL_DEBUG_PRINT(...) do { if (gl_debug_on()) godot::UtilityFunctions::print(__VA_ARGS__); } while (0)

#endif
