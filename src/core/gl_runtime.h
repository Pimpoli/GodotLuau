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
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "lua.h"
#include "lualib.h"

#include <unordered_set>
#include <vector>
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

// Devuelve CUALQUIER estado Luau vivo (para operaciones que solo necesitan una VM
// válida, como aplicar una propiedad replicada). nullptr si no hay ninguno.
static inline lua_State* gl_any_state() {
    std::unordered_set<lua_State*>& s = gl_alive_states();
    return s.empty() ? nullptr : *s.begin();
}

// ── Replicación automática (1.14.5) ──────────────────────────────────────────
//  `inline` (NO `static inline`) = UNA sola instancia compartida entre TUs.
//  Rol de red GLOBAL espejo del NetworkService (0=single, 1=servidor/host,
//  2=cliente). Lo actualiza el NetworkService. Permite que el hook de escritura
//  de propiedades descarte en O(1) cuando NO somos el servidor de una sesión de
//  red → cero coste en single-player.
inline int& gl_net_role() { static int r = 0; return r; }
//  ObjectID del NetworkService vivo (para resolverlo sin recorrer el árbol).
inline uint64_t& gl_net_service_id() { static uint64_t id = 0; return id; }
//  UserId secuencial e INMUTABLE del jugador local, asignado por el servidor por
//  orden de entrada (host=1, luego 2,3,...). 0 = sin asignar → el Player local
//  usa 1 (single-player). El servidor lo pone en start_server; un cliente lo
//  recibe por RPC (_gl_peer_uid) al conectarse.
inline int64_t& gl_local_user_id() { static int64_t v = 0; return v; }

//  Prefijo de meta del shadow de replicación: valor Lua-encodeado por propiedad.
static const char* GL_REP_SHADOW = "_glrep_";

//  ── JobId (1.14.16) ──────────────────────────────────────────────────────
//  Identificador ÚNICO de esta instancia de servidor. Antes `game.JobId`
//  devolvía "" siempre. Se genera una vez, la primera vez que se pide.
//  Como Roblox: sin sesión de red (equivalente a Studio) sigue siendo "".
inline String& gl_job_id() { static String id; return id; }

//  ── TeleportData (1.14.17) ───────────────────────────────────────────────
//  Datos que viajan con el jugador al cambiar de mundo. Van en memoria porque
//  el PROCESO del cliente sobrevive a la reconexión: solo se suelta el servidor
//  viejo y se entra al nuevo, así que al llegar siguen aquí.
inline String& gl_teleport_data()   { static String d;  return d; }
inline bool&   gl_teleport_arrived(){ static bool b = false; return b; }
inline String gl_ensure_job_id() {
    String& id = gl_job_id();
    if (!id.is_empty()) return id;
    auto hex = [](uint64_t v, int len) {
        String s = String::num_uint64(v, 16);
        while (s.length() < len) s = String("0") + s;
        return s.substr(s.length() - len, len);
    };
    uint64_t t  = (uint64_t)Time::get_singleton()->get_unix_time_from_system();
    uint64_t u  = (uint64_t)Time::get_singleton()->get_ticks_usec();
    uint64_t r1 = (uint64_t)UtilityFunctions::randi();
    uint64_t r2 = (uint64_t)UtilityFunctions::randi();
    id = hex(t, 8) + "-" + hex(r1 & 0xFFFF, 4) + "-4" + hex((r1 >> 16) & 0xFFF, 3)
       + "-" + hex(0x8000 | (r2 & 0x3FFF), 4) + "-" + hex((u << 16) ^ r2, 12);
    return id;
}

//  ── netId DETERMINISTA para los nodos del PLACE (1.14.10) ────────────────
//  Los netId de runtime los asigna el SERVIDOR con un contador y se anuncian por
//  RPC; los nodos que vienen en el place nunca pasan por ahí, así que no tenían
//  id y nada suyo se podía sincronizar (ni física ni props). Como TODAS las
//  ventanas cargan el MISMO place, la ruta absoluta ya identifica al nodo igual
//  en todas: hasheándola (FNV-1a) cada máquina llega al MISMO id sin negociar.
//  El bit 62 marca "estático" y mantiene ese rango separado del contador de
//  runtime (que empieza en 1), así que los dos tipos de id nunca chocan.
static const uint64_t GL_STATIC_ID_BIT = 0x4000000000000000ULL;
inline uint64_t gl_static_netid(const String& path) {
    CharString cs = path.utf8();
    const char* p = cs.get_data();
    uint64_t h = 1469598103934665603ULL;   // FNV-1a offset basis
    for (int i = 0; p && p[i]; i++) { h ^= (uint8_t)p[i]; h *= 1099511628211ULL; }
    return (h & 0x3FFFFFFFFFFFFFFFULL) | GL_STATIC_ID_BIT;
}
inline bool gl_is_static_netid(uint64_t id) { return (id & GL_STATIC_ID_BIT) != 0; }

//  ── Qué nodo se reporta en Touched (1.14.11) ─────────────────────────────
//  En Roblox el Touched entrega una PARTE del personaje, y por eso el idioma
//  universal es `hit.Parent:FindFirstChild("Humanoid")`. Aquí el colisionador
//  es el personaje ENTERO (lleva una sola cápsula), así que reportarlo tal cual
//  dejaba `hit.Parent` = Workspace y ese idioma no entraba NUNCA. Se reporta su
//  HumanoidRootPart (o Torso), cuyo Parent sí es el personaje.
inline Node* gl_touch_reported_node(Node* body) {
    if (!body) return nullptr;
    if (!body->get_node_or_null(NodePath("Humanoid"))) return body;   // no es un personaje
    if (Node* p = body->get_node_or_null(NodePath("HumanoidRootPart"))) return p;
    if (Node* t = body->get_node_or_null(NodePath("Torso"))) return t;
    return body;
}

//  Reloj sincronizado (1.14.8). El servidor es la referencia; el cliente estima
//  su desfase por round-trip (NetworkService._time_req/_time_res). En servidor y
//  single-player los offsets son 0 → devuelven el reloj local.
inline double& gl_server_unix_offset() { static double v = 0.0; return v; }   // Δ a sumar al unix local para = unix del server
inline double& gl_dgt_offset()         { static double v = 0.0; return v; }   // Δ para DistributedGameTime
//  GetServerTimeNow(): hora unix (epoch) del servidor, en segundos con decimales.
inline double gl_server_time_now() {
    return Time::get_singleton()->get_unix_time_from_system() + gl_server_unix_offset();
}
//  DistributedGameTime: segundos que lleva corriendo el servidor, sincronizado.
inline double gl_distributed_game_time() {
    return (double)Time::get_singleton()->get_ticks_usec() / 1000000.0 + gl_dgt_offset();
}
//  Hook para APLICAR una propiedad replicada en el cliente reentrando al
//  __newindex real. Lo define luau_api.h (que ve godot_object_newindex/wrap_node/
//  gl_net_decode) y lo instala el setup por-VM; el NetworkService (que no puede
//  ver luau_api.h por el orden de includes) lo llama por puntero.
typedef void (*GLApplyPropFn)(Node* target, const String& key, const Variant& enc);
inline GLApplyPropFn& gl_apply_prop_hook() { static GLApplyPropFn f = nullptr; return f; }
// Hook para registrar en el ScriptNodeBase un hilo que cede esperando la
// respuesta de un RemoteFunction por red. luau_api.h (que NO ve ScriptNodeBase,
// se incluye antes) lo llama; luau_script.h lo instala tras definir la clase.
typedef void (*GLAddInvokeWaitFn)(Node* sn, lua_State* th, lua_State* main_L, int ref, int64_t call_id, double timeout);
inline GLAddInvokeWaitFn& gl_add_invoke_wait_hook() { static GLAddInvokeWaitFn f = nullptr; return f; }

// ── Modelo server-authoritative (1.14.5) ─────────────────────────────────────
//  ¿Esta ventana arrancó como CLIENTE de una sesión de red? Se decide por los
//  args del launcher (fiable al arranque, antes de que corran los scripts). Un
//  cliente NO ejecuta ServerScripts: recibe el mundo del servidor por
//  replicación, igual que Roblox. La ventana nativa/servidor/single devuelve
//  false → ejecuta todo.
static inline bool gl_startup_is_client() {
    PackedStringArray a = OS::get_singleton()->get_cmdline_user_args();
    for (int i = 0; i < a.size(); i++) {
        String s = a[i];
        if (s == String("--glconnect") || s == String("--glmatch")) return true;
        if (s == String("--glindex") && i + 1 < a.size() && String(a[i + 1]).to_int() >= 2) return true;
    }
    return false;
}
//  ¿Esta ventana debe APLAZAR sus ServerScripts hasta CONFIRMAR que es servidor
//  (gl_net_role()==1)? El rol de servidor se fija tarde (start_server, vía
//  _auto_init diferido), así que si un ServerScript corriera antes, el mundo que
//  crea no se replicaría. Devuelve true para: cliente seguro; servidor por args
//  (--glserver/--glinstance/--glindex 1); y la ventana NATIVA con sesión MP
//  FRESCA (será el host del test local). Single-player → false (corre todo ya).
//  El guard de frescura (age<45s) evita romper single-player con una sesión vieja.
static inline bool gl_startup_defer_server() {
    if (gl_startup_is_client()) return true;
    PackedStringArray a = OS::get_singleton()->get_cmdline_user_args();
    for (int i = 0; i < a.size(); i++) {
        String s = a[i];
        if (s == String("--glserver") || s == String("--glinstance")) return true;
        if (s == String("--glindex") && i + 1 < a.size() && String(a[i + 1]).to_int() == 1) return true;
    }
    Ref<FileAccess> f = FileAccess::open("res://.gl_mp_session", FileAccess::READ);
    if (f.is_valid()) {
        PackedStringArray p = String(f->get_as_text()).strip_edges().split("|");
        f->close();
        if (p.size() >= 3) {
            int c = String(p[0]).to_int();
            double age = Time::get_singleton()->get_unix_time_from_system() - (double)String(p[2]).to_float();
            if (c >= 2 && age >= 0.0 && age < 45.0) return true;   // sesión MP fresca → esta nativa será el servidor
        }
    }
    return false;
}
//  ServerScripts APLAZADOS en una ventana cliente (guardan ObjectID). Si esa
//  ventana se promueve a host de respaldo (gl_net_role()→1), se ejecutan
//  entonces — así el fallback de host del test local sigue funcionando.
inline std::vector<uint64_t>& gl_deferred_server_scripts() { static std::vector<uint64_t> v; return v; }
static inline void gl_run_deferred_server_scripts() {
    std::vector<uint64_t>& dl = gl_deferred_server_scripts();
    std::vector<uint64_t> copy = dl;   // copiar: iniciar_corrutina puede tocar la lista
    dl.clear();
    for (uint64_t oid : copy) {
        Object* o = ObjectDB::get_instance(oid);
        if (Node* n = Object::cast_to<Node>(o)) n->call("iniciar_corrutina");
    }
}

// ── BindToClose: callbacks a ejecutar al cerrar el juego (guardar datos, etc.) ─
//  game:BindToClose(fn) registra fn aquí junto a su VM (main_L). Cada script
//  ejecuta SUS propios callbacks justo antes de cerrar su VM (EXIT_TREE), así
//  corren sobre una VM viva y en orden correcto — igual que el BindToClose de
//  Roblox al parar el servidor (best-effort: si el callback cede, no se reanuda).
struct GLCloseCb { lua_State* main_L; int ref; };
static inline std::vector<GLCloseCb>& gl_close_cbs() { static std::vector<GLCloseCb> v; return v; }
static inline void gl_add_close_cb(lua_State* main_L, int ref) {
    if (main_L) gl_close_cbs().push_back({ main_L, ref });
}
// Ejecuta y elimina los callbacks cuyo main_L coincide. Llamar ANTES de cerrar L.
static inline void gl_run_close_cbs_for(lua_State* main_L) {
    if (!main_L) return;
    std::vector<GLCloseCb>& v = gl_close_cbs();
    for (int i = 0; i < (int)v.size(); ) {
        if (v[i].main_L == main_L) {
            if (gl_state_alive(main_L)) {
                lua_State* th = lua_newthread(main_L);
                lua_rawgeti(main_L, LUA_REGISTRYINDEX, v[i].ref);
                if (lua_isfunction(main_L, -1)) {
                    lua_xmove(main_L, th, 1);
                    lua_resume(th, nullptr, 0);   // síncrono; si cede, no se reanuda
                } else lua_pop(main_L, 1);
                lua_pop(main_L, 1);               // quita el hilo
                lua_unref(main_L, v[i].ref);
            }
            v.erase(v.begin() + i);
        } else ++i;
    }
}

#endif // GL_RUNTIME_H
