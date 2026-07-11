#ifndef GL_RUNTIME_H
#define GL_RUNTIME_H

// ════════════════════════════════════════════════════════════════════
//  Seguridad de vida para el puente Luau <-> Godot
//  ----------------------------------------------------------------
//  1) Registro de estados Luau vivos: cada ScriptNodeBase registra su
//     lua_State principal al crearlo y lo da de baja ANTES de cerrarlo.
//     Cualquier despacho de señal/callback comprueba gl_state_alive()
//     para no invocar refs sobre una VM ya cerrada (use-after-free al
//     destruir o recargar un script).
//
//  2) Referencias a nodos por ObjectID: el wrapper de Lua guarda el
//     ObjectID del nodo, NO un puntero crudo. gow_node() resuelve y
//     valida contra ObjectDB, devolviendo nullptr si el nodo fue
//     destruido (igual que Roblox: acceder a un objeto destruido es
//     seguro y no crashea).
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include "lua.h"
#include "lualib.h"

#include <unordered_set>
#include <cstdint>

using namespace godot;

// ── Estado de input cross-device compartido ──────────────────────────────────
//  La UI tactil (joystick + boton de salto, creada por RobloxPlayer cuando hay
//  pantalla tactil) ESCRIBE aqui; el Humanoid LEE de aqui para mover el
//  personaje. Asi el movimiento funciona en PC, movil y consola con un solo
//  camino, igual que el moveVector del ControlModule de Roblox.
//  `inline` (enlace externo) = una sola instancia compartida entre TUs.
struct GLMobileInput {
    Vector2 move;            // joystick virtual: x = derecha, y = adelante (-1..1)
    bool    jump   = false;  // boton de salto en pantalla mantenido
    bool    active = false;  // hay controles tactiles en pantalla
};
inline GLMobileInput& gl_mobile() { static GLMobileInput s; return s; }

// ── Vista de Servidor (cámara libre de debug, como en Roblox Studio) ─────────
//  Cuando está activa, el Humanoid y el RobloxPlayer IGNORAN el input: WASD y
//  el mouse mueven la cámara libre, no al personaje. La activa el botón
//  "Server View" que crea el NetworkService en sesiones multijugador.
struct GLFreecamState {
    bool active = false;
};
inline GLFreecamState& gl_freecam() { static GLFreecamState s; return s; }

// ── Dispositivo emulado (PC / Mobile / Console / VR) ─────────────────────────
//  Elegido en la barra "Players" del editor. Llega por --gldevice (ventanas
//  cliente) o por res://.gl_mp_session "count|device|stamp" (ventana nativa).
//  Lo leen RobloxPlayer (controles tactiles en Mobile) y NetworkService
//  (tamano de ventana, vista VR, etc.).
static inline godot::String gl_emulated_device() {
    godot::PackedStringArray a = godot::OS::get_singleton()->get_cmdline_user_args();
    for (int i = 0; i < a.size(); i++)
        if (godot::String(a[i]) == "--gldevice" && i + 1 < a.size()) return a[i + 1];
    godot::Ref<godot::FileAccess> f = godot::FileAccess::open("res://.gl_mp_session", godot::FileAccess::READ);
    if (f.is_valid()) {
        godot::PackedStringArray parts = godot::String(f->get_as_text()).strip_edges().split("|");
        f->close();
        if (parts.size() >= 2) return parts[1];
    }
    return godot::String("PC");
}

// ── Wrapper de instancia: ObjectID en lugar de Node* crudo ───────────────────
struct GodotObjectWrapper {
    uint64_t obj_id;   // ObjectID del nodo (0 = ninguno)
};

// Resuelve el nodo validando contra ObjectDB. nullptr si fue destruido.
static inline Node* gow_node(const GodotObjectWrapper* w) {
    if (!w || w->obj_id == 0) return nullptr;
    Object* o = ObjectDB::get_instance(w->obj_id);
    return o ? Object::cast_to<Node>(o) : nullptr;
}

static inline void gow_set(GodotObjectWrapper* w, Node* n) {
    if (w) w->obj_id = n ? (uint64_t)n->get_instance_id() : 0;
}

// ── Registro global de estados Luau vivos ────────────────────────────────────
static inline std::unordered_set<lua_State*>& gl_alive_states() {
    static std::unordered_set<lua_State*> s;
    return s;
}
static inline void gl_register_state(lua_State* L)   { if (L) gl_alive_states().insert(L); }
static inline void gl_unregister_state(lua_State* L) { if (L) gl_alive_states().erase(L); }
static inline bool gl_state_alive(lua_State* L)      { return L && gl_alive_states().count(L) != 0; }

// Normaliza cualquier hilo Luau a su estado principal (el que registramos).
// SOLO debe llamarse mientras L sigue vivo (p.ej. al conectar el callback).
static inline lua_State* gl_main_of(lua_State* L) { return L ? lua_mainthread(L) : nullptr; }

#endif // GL_RUNTIME_H
