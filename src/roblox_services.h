#ifndef ROBLOX_SERVICES_H
#define ROBLOX_SERVICES_H

#include "roblox_chat.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/classes/procedural_sky_material.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/window.hpp>

#include "lua.h"
#include "lualib.h"
#include <vector>
#include <algorithm>
#include <cstring>

// ── Shared resume queue: RunService → ScriptNodeBase ─────────────────
// RunService queues pending coroutine resumes here; ScriptNodeBase drains
// them each frame so it can update spawned_threads correctly.
struct LuauPendingResume {
    lua_State* thread;   // coroutine to resume
    lua_State* main_L;   // owning main state
    double     delta;    // dt argument to pass
    Node*      node_arg; // optional node (WaitForChild result); nullptr if not used
};
inline std::vector<LuauPendingResume>& get_pending_resumes() {
    static std::vector<LuauPendingResume> v;
    return v;
}

using namespace godot;

// ════════════════════════════════════════════════════════════════════
//  Service placement protection: in Roblox, services only exist under
//  the DataModel. These helpers enforce that in the editor: if someone
//  tries to add a service to an invalid parent, it is deleted and an
//  error is shown. At runtime there is no restriction (scene is saved).
////
//  Protección de servicios: en Roblox, los servicios solo existen bajo
//  el DataModel. Estos helpers lo garantizan en el editor: si alguien
//  intenta añadir un servicio a un nodo incorrecto, se elimina y muestra
//  un error. En runtime no hay restricción (la escena ya está guardada).
// ════════════════════════════════════════════════════════════════════

static inline bool _is_valid_game_parent(Node* p) {
    if (!p) return false;
    return p->is_class("RobloxDataModel") ||
           p->is_class("RobloxGame3D")    ||
           p->is_class("RobloxGame2D");
}

static inline bool _is_valid_starter_parent(Node* p) {
    if (!p) return false;
    return p->is_class("StarterPlayer");
}

// Returns false if the node was removed (invalid parent).
//// Retorna false si el nodo fue eliminado (padre inválido).
static inline bool _enforce_game_parent(Node* self) {
    if (!Engine::get_singleton()->is_editor_hint()) return true;
    if (_is_valid_game_parent(self->get_parent())) return true;
    UtilityFunctions::push_error(
        String("[GodotLuau] '") + self->get_class() +
        "' is a Roblox service and can only exist as a direct child of "
        "RobloxGame3D, RobloxGame2D or RobloxDataModel. "
        "Use the RobloxGame3D/2D root node to auto-generate the structure.");
    self->queue_free();
    return false;
}

static inline bool _enforce_starter_parent(Node* self) {
    if (!Engine::get_singleton()->is_editor_hint()) return true;
    if (_is_valid_starter_parent(self->get_parent())) return true;
    UtilityFunctions::push_error(
        String("[GodotLuau] '") + self->get_class() +
        "' can only be a direct child of StarterPlayer.");
    self->queue_free();
    return false;
}

// ════════════════════════════════════════════════════════════════════
//  STORAGE
//  Direct equivalents to Roblox services
////
//  ALMACENAMIENTO
//  Equivalentes directos a los servicios de Roblox
// ════════════════════════════════════════════════════════════════════

// Visible to client AND server — used for ModuleScripts and shared objects
//// Visible para cliente Y servidor — se usa para ModuleScripts y objetos compartidos
class ReplicatedStorage : public Node {
    GDCLASS(ReplicatedStorage, Node);
protected:
    static void _bind_methods() {}
    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _enforce_game_parent(this);
    }
};

// Server-only — secure content not accessible to the client
//// Solo visible para el servidor — contenido seguro no accesible al cliente
class ServerStorage : public Node {
    GDCLASS(ServerStorage, Node);
protected:
    static void _bind_methods() {}
    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _enforce_game_parent(this);
    }
};

// Replicated to client BEFORE the rest of the scene loads
// Ideal for loading screens and critical assets
//// Se replica al cliente ANTES de que cargue el resto de la escena
//// Ideal para pantallas de carga, assets críticos
class ReplicatedFirst : public Node {
    GDCLASS(ReplicatedFirst, Node);
protected:
    static void _bind_methods() {}
    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _enforce_game_parent(this);
    }
};

// ════════════════════════════════════════════════════════════════════
//  SERVER SCRIPTS
////  SCRIPTS DEL SERVIDOR
// ════════════════════════════════════════════════════════════════════

// Scripts that run server-side only (secure game logic)
//// Scripts que solo se ejecutan en el servidor (lógica de juego segura)
class ServerScriptService : public Node {
    GDCLASS(ServerScriptService, Node);
protected:
    static void _bind_methods() {}
    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _enforce_game_parent(this);
    }
};

// ════════════════════════════════════════════════════════════════════
//  STARTER PLAYER — Player scripts and character system
////  STARTER PLAYER — Sistema de scripts y personaje del jugador
// ════════════════════════════════════════════════════════════════════

// Scripts cloned onto each character when they spawn
//// Scripts que se clonan en cada personaje al spawnear
class StarterCharacterScripts : public Node {
    GDCLASS(StarterCharacterScripts, Node);
protected:
    static void _bind_methods() {}
    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _enforce_starter_parent(this);
    }
};

// Scripts cloned onto the local player when they join
//// Scripts que se clonan en el jugador local al entrar
class StarterPlayerScripts : public Node {
    GDCLASS(StarterPlayerScripts, Node);
protected:
    static void _bind_methods() {}
    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _enforce_starter_parent(this);
    }
};

// Main player container — auto-creates its children when added in the editor
//// Container principal del jugador — crea sus hijos automáticamente al añadirlo al editor
class StarterPlayer : public Node {
    GDCLASS(StarterPlayer, Node);
protected:
    static void _bind_methods() {}

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE && Engine::get_singleton()->is_editor_hint()) {
            if (!_enforce_game_parent(this)) return;
            Node* root = get_tree()->get_edited_scene_root();
            if (!root) return;

            // Auto-create children if they don't exist
            //// Auto-crear hijos si no existen
            auto make_child = [&](const String& name, Node* child) {
                if (!get_node_or_null(name)) {
                    child->set_name(name);
                    add_child(child);
                    child->set_owner(root);
                }
            };

            if (!get_node_or_null("StarterCharacterScripts")) make_child("StarterCharacterScripts", memnew(StarterCharacterScripts));
            if (!get_node_or_null("StarterPlayerScripts"))    make_child("StarterPlayerScripts",    memnew(StarterPlayerScripts));
        }
    }
};

// ════════════════════════════════════════════════════════════════════
//  STARTER GUI AND PACK
////  STARTER GUI Y PACK
// ════════════════════════════════════════════════════════════════════

// ScreenGuis here are cloned into each player's HUD
//// ScreenGuis aquí se clonan al HUD de cada jugador
class StarterGui : public Node {
    GDCLASS(StarterGui, Node);
protected:
    static void _bind_methods() {}
    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _enforce_game_parent(this);
    }
};

// Tools / inventory items (backpack) for the player
//// Tools / items del inventario (mochila) del jugador
class StarterPack : public Node {
    GDCLASS(StarterPack, Node);
protected:
    static void _bind_methods() {}
    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _enforce_game_parent(this);
    }
};

// ════════════════════════════════════════════════════════════════════
//  PLAYERS AND TEAMS
////  JUGADORES Y EQUIPOS
// ════════════════════════════════════════════════════════════════════

// Players service — accessed via game:GetService("Players")
//// Servicio de jugadores — acceso via game:GetService("Players")
class Players : public Node {
    GDCLASS(Players, Node);

public:
    struct LuaCallback { lua_State* main_L; int ref; bool active = true; };
    std::vector<LuaCallback> player_added_cbs;
    std::vector<LuaCallback> player_removing_cbs;
    int max_players = 50;

protected:
    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _enforce_game_parent(this);
    }
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("get_local_player"),     &Players::get_local_player);
        ClassDB::bind_method(D_METHOD("get_players"),          &Players::get_players);
        ClassDB::bind_method(D_METHOD("get_max_players"),      &Players::get_max_players);
        ClassDB::bind_method(D_METHOD("set_max_players", "v"), &Players::set_max_players);
        ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "LocalPlayer",
                                  PROPERTY_HINT_NODE_TYPE, "Node"),
                     "", "get_local_player");
        ADD_PROPERTY(PropertyInfo(Variant::INT, "MaxPlayers"), "set_max_players", "get_max_players");
    }

public:
    int  get_max_players() const { return max_players; }
    void set_max_players(int v)  { max_players = v; }

    void add_player_added_cb(lua_State* L, int ref)    { player_added_cbs.push_back({L,ref,true}); }
    void add_player_removing_cb(lua_State* L, int ref) { player_removing_cbs.push_back({L,ref,true}); }

    void fire_player_added(Node* player)    { _fire_player_event(player_added_cbs, player); }
    void fire_player_removing(Node* player) { _fire_player_event(player_removing_cbs, player); }

    // Find the player character in Workspace (RobloxPlayer or RobloxPlayer2D)
    //// Busca el personaje del jugador en Workspace (RobloxPlayer o RobloxPlayer2D)
    Node* get_local_player() const {
        if (!is_inside_tree()) return nullptr;
        Node* parent = get_parent(); // DataModel
        if (!parent) return nullptr;
        Node* ws = parent->get_node_or_null("Workspace");
        if (!ws) return nullptr;

        for (int i = 0; i < ws->get_child_count(); i++) {
            Node* child = ws->get_child(i);
            if (child && (child->is_class("RobloxPlayer") || child->is_class("RobloxPlayer2D")))
                return child;
        }
        return nullptr;
    }

    TypedArray<Node> get_players() const {
        TypedArray<Node> result;
        Node* lp = get_local_player();
        if (lp) result.push_back(lp);
        return result;
    }

private:
    struct _GOW { Node* node_ptr; };
    void _fire_player_event(std::vector<LuaCallback>& list, Node* player) {
        for (int i = (int)list.size()-1; i >= 0; --i) {
            auto& cb = list[i];
            if (!cb.active) { list.erase(list.begin()+i); continue; }
            lua_State* th = lua_newthread(cb.main_L);
            lua_rawgeti(cb.main_L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.main_L, -1)) {
                lua_xmove(cb.main_L, th, 1);
                if (player) {
                    _GOW* w = (_GOW*)lua_newuserdata(th, sizeof(_GOW));
                    w->node_ptr = player;
                    luaL_getmetatable(th, "GodotObject");
                    lua_setmetatable(th, -2);
                } else {
                    lua_pushnil(th);
                }
                lua_resume(th, nullptr, 1);
            } else lua_pop(cb.main_L, 1);
            lua_pop(cb.main_L, 1);
        }
    }
};

// Teams management — players can belong to colored teams
//// Gestión de equipos — los jugadores pueden pertenecer a equipos con colores
class Teams : public Node {
    GDCLASS(Teams, Node);
protected:
    static void _bind_methods() {}
    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _enforce_game_parent(this);
    }
};

// Folder is defined in folder.h (already existed before)
//// Folder está definido en folder.h (ya existía antes)

// ════════════════════════════════════════════════════════════════════
//  LIGHTING AND ENVIRONMENT
////  ILUMINACIÓN Y AMBIENTE
// ════════════════════════════════════════════════════════════════════

class Lighting : public Node {
    GDCLASS(Lighting, Node);

public:
    enum LightingPreset {
        PRESET_DEFAULT=0, PRESET_REALISTIC, PRESET_CARTOON,
        PRESET_ANIME, PRESET_FLAT, PRESET_RETRO,
        PRESET_FOGGY, PRESET_SUNSET, PRESET_NIGHT
    };

private:
    // ── Core ─────────────────────────────────────────────────────
    float brightness           = 2.0f;
    float clock_time           = 14.0f;
    float geographic_latitude  = 41.7f;
    Color ambient              = Color(0.4f, 0.4f, 0.4f);
    Color outdoor_ambient      = Color(0.5f, 0.5f, 0.5f);
    bool  global_shadows       = true;
    float shadow_softness      = 0.2f;
    // ── Environment ──────────────────────────────────────────────
    float env_diffuse_scale    = 1.0f;
    float env_specular_scale   = 1.0f;
    float exposure_comp        = 0.0f;
    // ── Fog ──────────────────────────────────────────────────────
    float fog_start            = 0.0f;
    float fog_end              = 100000.0f;
    float fog_density          = 0.0f;
    Color fog_color            = Color(0.75f, 0.85f, 1.0f);
    // ── Color shifts (Roblox ColorShift_Top / ColorShift_Bottom) ──
    Color color_shift_top      = Color(0, 0, 0);
    Color color_shift_bottom   = Color(0, 0, 0);
    // Technology: 0=Compatibility 1=Legacy 2=Future 3=ShadowMap 4=Voxel
    int   technology           = 3; // ShadowMap default (same as modern Roblox Studio / igual que Roblox Studio moderno)
    // ── Preset ───────────────────────────────────────────────────
    int lighting_preset        = PRESET_DEFAULT;

    // ── Tree helpers ─────────────────────────────────────────────
    WorldEnvironment* _find_we_r(Node* n) const {
        if (!n) return nullptr;
        WorldEnvironment* we = Object::cast_to<WorldEnvironment>(n);
        if (we) return we;
        for (int i = 0; i < n->get_child_count(); i++) {
            WorldEnvironment* f = _find_we_r(n->get_child(i));
            if (f) return f;
        }
        return nullptr;
    }
    WorldEnvironment* _find_world_env() const {
        if (!is_inside_tree()) return nullptr;
        return _find_we_r((Node*)get_tree()->get_root()); // <-- AÑADIDO (Node*)
    }
    Ref<Environment> _get_env() const {
        WorldEnvironment* we = _find_world_env();
        if (!we) return {};
        return we->get_environment();
    }
    DirectionalLight3D* _find_sun_r(Node* n) const {
        if (!n) return nullptr;
        DirectionalLight3D* d = Object::cast_to<DirectionalLight3D>(n);
        if (d) return d;
        for (int i = 0; i < n->get_child_count(); i++) {
            DirectionalLight3D* f = _find_sun_r(n->get_child(i));
            if (f) return f;
        }
        return nullptr;
    }
    DirectionalLight3D* _find_sun() const {
        if (!is_inside_tree()) return nullptr;
        return _find_sun_r((Node*)get_tree()->get_root()); // <-- AÑADIDO (Node*)
    }

    // Returns sun elevation in degrees. max_elev_out receives the latitude cap.
    float _compute_elev(float* max_elev_out = nullptr) const {
        float max_elev = 90.0f - Math::abs(geographic_latitude);
        if (max_elev < 10.0f) max_elev = 10.0f;
        if (max_elev_out) *max_elev_out = max_elev;
        float t = (clock_time - 6.0f) / 12.0f;
        if (t >= 0.0f && t <= 1.0f)
            return Math::sin(t * (float)Math_PI) * max_elev;
        float t2 = (clock_time < 6.0f) ? (clock_time / 6.0f)
                                        : ((clock_time - 18.0f) / 6.0f);
        return -(Math::sin(t2 * (float)Math_PI) * 30.0f);
    }

    void _apply_sun() {
        DirectionalLight3D* sun = _find_sun();
        if (!sun) return;
        float max_elev = 0.0f;
        float elev = _compute_elev(&max_elev);
        float sun_y = 45.0f + geographic_latitude * 0.3f;
        sun->set_rotation_degrees(Vector3(-elev, sun_y, 0.0f));
        sun->set_shadow(global_shadows);
        sun->set_param(Light3D::PARAM_SHADOW_BLUR, shadow_softness * 4.0f);

        // Energy: smooth S-curve, zero below horizon, peak at noon
        float elev_norm = elev / Math::max(max_elev, 10.0f); // -1..1
        float raw_e = Math::clamp(elev_norm * 2.0f + 0.5f, 0.0f, 1.0f);
        float sun_ene = raw_e * raw_e * (3.0f - 2.0f * raw_e); // smoothstep
        sun->set_param(Light3D::PARAM_ENERGY,          brightness * sun_ene);
        sun->set_param(Light3D::PARAM_INDIRECT_ENERGY, env_diffuse_scale * sun_ene);
        sun->set_param(Light3D::PARAM_SPECULAR,        env_specular_scale * 0.5f * sun_ene);

        // Color: white at noon, deep orange-red at horizon
        float horizon_f = 1.0f - Math::clamp(elev_norm * 3.0f + 0.5f, 0.0f, 1.0f);
        Color sun_col = Color(1.0f, 1.0f, 1.0f).lerp(Color(1.0f, 0.38f, 0.04f), horizon_f * 0.92f);
        sun->set_color(sun_col);
    }

    void _apply_env() {
        Ref<Environment> env = _get_env();
        if (!env.is_valid()) return;

        // Compute time-based factors from sun elevation
        float max_elev = 0.0f;
        float elev = _compute_elev(&max_elev);
        float elev_norm = elev / Math::max(max_elev, 10.0f);
        float t_day    = Math::clamp(elev_norm * 2.5f + 0.5f, 0.0f, 1.0f);
        float t_sunset = Math::max(0.0f, 1.0f - Math::abs(elev_norm) * 4.0f) * (elev_norm > -0.25f ? 1.0f : 0.0f);
        float t_night  = Math::clamp(-elev_norm * 3.0f, 0.0f, 1.0f);

        // Dynamic ambient: neutral day → warm sunset → cool night
        Color night_amb  = Color(0.04f, 0.05f, 0.10f);
        Color sunset_amb = Color(0.52f, 0.26f, 0.08f);
        Color dyn_amb    = ambient.lerp(sunset_amb, t_sunset * 0.65f).lerp(night_amb, t_night);

        env->set_ambient_light_color(dyn_amb);
        env->set_ambient_light_energy(brightness * (0.15f + t_day * 0.35f));
        env->set_ambient_light_sky_contribution(env_diffuse_scale * 0.5f);
        env->set_tonemap_exposure(1.0f + exposure_comp * 0.5f);

        // Dynamic fog color: neutral day → warm sunset → dark night
        Color night_fog  = Color(0.025f, 0.032f, 0.07f);
        Color sunset_fog = Color(0.92f, 0.42f, 0.10f);
        Color dyn_fog    = fog_color.lerp(sunset_fog, t_sunset * 0.55f).lerp(night_fog, t_night);

        bool use_depth_fog = (fog_end < 99000.0f && fog_end > fog_start);
        bool use_exp_fog   = (fog_density > 0.001f);
        env->set_fog_enabled(use_depth_fog || use_exp_fog);
        if (use_depth_fog) {
            env->set_fog_mode(Environment::FOG_MODE_DEPTH);
            env->set_fog_depth_begin(fog_start);
            env->set_fog_depth_end(fog_end);
            env->set_fog_light_color(dyn_fog);
            env->set_fog_density(0.01f);
        } else if (use_exp_fog) {
            env->set_fog_mode(Environment::FOG_MODE_EXPONENTIAL);
            env->set_fog_density(fog_density * 0.08f);
            env->set_fog_light_color(dyn_fog);
        }
    }

    void _apply_all() { _apply_sun(); _apply_env(); }

    void _apply_preset(int p) {
        switch (p) {
        case PRESET_REALISTIC:
            brightness=3.0f; shadow_softness=0.1f; env_diffuse_scale=1.0f;
            env_specular_scale=1.0f; exposure_comp=0.0f; fog_density=0.0f;
            ambient=Color(0.3f,0.3f,0.35f); outdoor_ambient=Color(0.5f,0.55f,0.6f);
            break;
        case PRESET_CARTOON:
            brightness=2.5f; shadow_softness=0.5f; env_diffuse_scale=1.2f;
            env_specular_scale=0.3f; exposure_comp=0.1f; fog_density=0.0f;
            ambient=Color(0.5f,0.5f,0.6f); outdoor_ambient=Color(0.6f,0.65f,0.8f);
            break;
        case PRESET_ANIME:
            brightness=2.8f; shadow_softness=0.0f; env_diffuse_scale=1.1f;
            env_specular_scale=0.5f; exposure_comp=0.2f; fog_density=0.0f;
            ambient=Color(0.45f,0.45f,0.55f); outdoor_ambient=Color(0.55f,0.6f,0.75f);
            break;
        case PRESET_FLAT:
            brightness=2.0f; shadow_softness=1.0f; env_diffuse_scale=2.0f;
            env_specular_scale=0.0f; exposure_comp=0.0f; fog_density=0.0f;
            ambient=Color(0.6f,0.6f,0.6f); global_shadows=false;
            break;
        case PRESET_RETRO:
            brightness=1.8f; shadow_softness=0.8f; env_diffuse_scale=0.8f;
            env_specular_scale=0.2f; exposure_comp=-0.2f; fog_density=0.02f;
            ambient=Color(0.35f,0.30f,0.25f); fog_color=Color(0.8f,0.75f,0.6f);
            break;
        case PRESET_FOGGY:
            brightness=1.5f; shadow_softness=0.4f; env_diffuse_scale=0.9f;
            env_specular_scale=0.4f; exposure_comp=-0.1f; fog_density=0.15f;
            fog_end=500.0f; ambient=Color(0.5f,0.52f,0.55f);
            fog_color=Color(0.75f,0.80f,0.85f);
            break;
        case PRESET_SUNSET:
            clock_time=18.5f; brightness=2.0f; shadow_softness=0.3f;
            env_diffuse_scale=1.0f; env_specular_scale=0.8f; exposure_comp=0.1f;
            fog_density=0.02f; ambient=Color(0.45f,0.30f,0.20f);
            fog_color=Color(1.0f,0.6f,0.3f);
            break;
        case PRESET_NIGHT:
            clock_time=22.0f; brightness=0.15f; shadow_softness=0.6f;
            env_diffuse_scale=0.3f; env_specular_scale=0.5f; exposure_comp=0.5f;
            fog_density=0.005f; ambient=Color(0.05f,0.06f,0.12f);
            fog_color=Color(0.05f,0.07f,0.15f);
            break;
        default: // PRESET_DEFAULT
            brightness=2.0f; clock_time=14.0f; shadow_softness=0.2f;
            env_diffuse_scale=1.0f; env_specular_scale=1.0f; exposure_comp=0.0f;
            fog_density=0.0f; fog_end=100000.0f; global_shadows=true;
            ambient=Color(0.4f,0.4f,0.4f); fog_color=Color(0.75f,0.85f,1.0f);
            break;
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_lighting_preset","v"), &Lighting::set_lighting_preset);
        ClassDB::bind_method(D_METHOD("get_lighting_preset"),     &Lighting::get_lighting_preset);
        ADD_PROPERTY(PropertyInfo(Variant::INT,"LightingPreset",PROPERTY_HINT_ENUM,
            "Default,Realistic,Cartoon,Anime,Flat,Retro,Foggy,Sunset,Night"),
            "set_lighting_preset","get_lighting_preset");

        ClassDB::bind_method(D_METHOD("set_brightness","v"), &Lighting::set_brightness);
        ClassDB::bind_method(D_METHOD("get_brightness"),     &Lighting::get_brightness);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Brightness",PROPERTY_HINT_RANGE,"0,4,0.01"),"set_brightness","get_brightness");

        ClassDB::bind_method(D_METHOD("set_clock_time","v"), &Lighting::set_clock_time);
        ClassDB::bind_method(D_METHOD("get_clock_time"),     &Lighting::get_clock_time);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"ClockTime",PROPERTY_HINT_RANGE,"0,24,0.1"),"set_clock_time","get_clock_time");

        ClassDB::bind_method(D_METHOD("set_geographic_latitude","v"), &Lighting::set_geographic_latitude);
        ClassDB::bind_method(D_METHOD("get_geographic_latitude"),     &Lighting::get_geographic_latitude);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"GeographicLatitude",PROPERTY_HINT_RANGE,"-90,90,0.1"),"set_geographic_latitude","get_geographic_latitude");

        ClassDB::bind_method(D_METHOD("set_ambient","c"), &Lighting::set_ambient);
        ClassDB::bind_method(D_METHOD("get_ambient"),     &Lighting::get_ambient);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"Ambient"),"set_ambient","get_ambient");

        ClassDB::bind_method(D_METHOD("set_outdoor_ambient","c"), &Lighting::set_outdoor_ambient);
        ClassDB::bind_method(D_METHOD("get_outdoor_ambient"),     &Lighting::get_outdoor_ambient);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"OutdoorAmbient"),"set_outdoor_ambient","get_outdoor_ambient");

        ClassDB::bind_method(D_METHOD("set_global_shadows","v"), &Lighting::set_global_shadows);
        ClassDB::bind_method(D_METHOD("get_global_shadows"),     &Lighting::get_global_shadows);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"GlobalShadows"),"set_global_shadows","get_global_shadows");

        ClassDB::bind_method(D_METHOD("set_shadow_softness","v"), &Lighting::set_shadow_softness);
        ClassDB::bind_method(D_METHOD("get_shadow_softness"),     &Lighting::get_shadow_softness);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"ShadowSoftness",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_shadow_softness","get_shadow_softness");

        ClassDB::bind_method(D_METHOD("set_env_diffuse_scale","v"), &Lighting::set_env_diffuse_scale);
        ClassDB::bind_method(D_METHOD("get_env_diffuse_scale"),     &Lighting::get_env_diffuse_scale);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"EnvironmentDiffuseScale",PROPERTY_HINT_RANGE,"0,2,0.01"),"set_env_diffuse_scale","get_env_diffuse_scale");

        ClassDB::bind_method(D_METHOD("set_env_specular_scale","v"), &Lighting::set_env_specular_scale);
        ClassDB::bind_method(D_METHOD("get_env_specular_scale"),     &Lighting::get_env_specular_scale);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"EnvironmentSpecularScale",PROPERTY_HINT_RANGE,"0,2,0.01"),"set_env_specular_scale","get_env_specular_scale");

        ClassDB::bind_method(D_METHOD("set_exposure_comp","v"), &Lighting::set_exposure_comp);
        ClassDB::bind_method(D_METHOD("get_exposure_comp"),     &Lighting::get_exposure_comp);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"ExposureCompensation",PROPERTY_HINT_RANGE,"-5,5,0.01"),"set_exposure_comp","get_exposure_comp");

        ClassDB::bind_method(D_METHOD("set_fog_start","v"), &Lighting::set_fog_start);
        ClassDB::bind_method(D_METHOD("get_fog_start"),     &Lighting::get_fog_start);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"FogStart",PROPERTY_HINT_RANGE,"0,10000,1"),"set_fog_start","get_fog_start");

        ClassDB::bind_method(D_METHOD("set_fog_end","v"), &Lighting::set_fog_end);
        ClassDB::bind_method(D_METHOD("get_fog_end"),     &Lighting::get_fog_end);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"FogEnd",PROPERTY_HINT_RANGE,"0,100000,1"),"set_fog_end","get_fog_end");

        ClassDB::bind_method(D_METHOD("set_fog_density","v"), &Lighting::set_fog_density);
        ClassDB::bind_method(D_METHOD("get_fog_density"),     &Lighting::get_fog_density);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"FogDensity",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_fog_density","get_fog_density");

        ClassDB::bind_method(D_METHOD("set_fog_color","c"), &Lighting::set_fog_color);
        ClassDB::bind_method(D_METHOD("get_fog_color"),     &Lighting::get_fog_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"FogColor"),"set_fog_color","get_fog_color");

        ClassDB::bind_method(D_METHOD("set_color_shift_top","c"),    &Lighting::set_color_shift_top);
        ClassDB::bind_method(D_METHOD("get_color_shift_top"),        &Lighting::get_color_shift_top);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"ColorShift_Top"),  "set_color_shift_top","get_color_shift_top");

        ClassDB::bind_method(D_METHOD("set_color_shift_bottom","c"), &Lighting::set_color_shift_bottom);
        ClassDB::bind_method(D_METHOD("get_color_shift_bottom"),     &Lighting::get_color_shift_bottom);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"ColorShift_Bottom"),"set_color_shift_bottom","get_color_shift_bottom");

        ClassDB::bind_method(D_METHOD("set_technology","t"), &Lighting::set_technology);
        ClassDB::bind_method(D_METHOD("get_technology"),     &Lighting::get_technology);
        ADD_PROPERTY(PropertyInfo(Variant::INT,"Technology",PROPERTY_HINT_ENUM,
            "Compatibility:0,Legacy:1,Future:2,ShadowMap:3,Voxel:4"),
            "set_technology","get_technology");
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            if (!_enforce_game_parent(this)) return;
            _apply_all();
        }
    }

public:
    void set_lighting_preset(int v) {
        lighting_preset = v;
        _apply_preset(v);
        if (is_inside_tree()) _apply_all();
    }
    int  get_lighting_preset() const    { return lighting_preset; }

    void set_brightness(float v)        { brightness = Math::clamp(v,0.0f,4.0f);    if(is_inside_tree()) _apply_all(); }
    float get_brightness() const        { return brightness; }
    void set_clock_time(float v)        { clock_time = Math::fmod(Math::abs(v),24.0f); if(is_inside_tree()) _apply_all(); }
    float get_clock_time() const        { return clock_time; }
    void set_geographic_latitude(float v){ geographic_latitude = Math::clamp(v,-90.0f,90.0f); if(is_inside_tree()) _apply_all(); }
    float get_geographic_latitude() const{ return geographic_latitude; }
    void set_ambient(Color c)           { ambient = c;                               if(is_inside_tree()) _apply_env(); }
    Color get_ambient() const           { return ambient; }
    void set_outdoor_ambient(Color c)   { outdoor_ambient = c;                       if(is_inside_tree()) _apply_env(); }
    Color get_outdoor_ambient() const   { return outdoor_ambient; }
    void set_global_shadows(bool v)     { global_shadows = v;                        if(is_inside_tree()) _apply_sun(); }
    bool get_global_shadows() const     { return global_shadows; }
    void set_shadow_softness(float v)   { shadow_softness = Math::clamp(v,0.0f,1.0f); if(is_inside_tree()) _apply_sun(); }
    float get_shadow_softness() const   { return shadow_softness; }
    void set_env_diffuse_scale(float v) { env_diffuse_scale = Math::clamp(v,0.0f,2.0f); if(is_inside_tree()) _apply_all(); }
    float get_env_diffuse_scale() const { return env_diffuse_scale; }
    void set_env_specular_scale(float v){ env_specular_scale = Math::clamp(v,0.0f,2.0f); if(is_inside_tree()) _apply_all(); }
    float get_env_specular_scale() const{ return env_specular_scale; }
    void set_exposure_comp(float v)     { exposure_comp = Math::clamp(v,-5.0f,5.0f); if(is_inside_tree()) _apply_env(); }
    float get_exposure_comp() const     { return exposure_comp; }
    void set_fog_start(float v)         { fog_start = v;                             if(is_inside_tree()) _apply_env(); }
    float get_fog_start() const         { return fog_start; }
    void set_fog_end(float v)           { fog_end = v;                               if(is_inside_tree()) _apply_env(); }
    float get_fog_end() const           { return fog_end; }
    void set_fog_density(float v)       { fog_density = Math::clamp(v,0.0f,1.0f);   if(is_inside_tree()) _apply_env(); }
    float get_fog_density() const       { return fog_density; }
    void set_fog_color(Color c)          { fog_color = c;                             if(is_inside_tree()) _apply_env(); }
    Color get_fog_color() const          { return fog_color; }
    void  set_color_shift_top(Color c)   { color_shift_top = c; }
    Color get_color_shift_top() const    { return color_shift_top; }
    void  set_color_shift_bottom(Color c){ color_shift_bottom = c; }
    Color get_color_shift_bottom() const { return color_shift_bottom; }
    void  set_technology(int t)          { technology = Math::clamp(t, 0, 4); }
    int   get_technology() const         { return technology; }
};

// ════════════════════════════════════════════════════════════════════
//  AUDIO, MATERIALS, NETWORK
////  AUDIO, MATERIALES, RED
// ════════════════════════════════════════════════════════════════════

// Controls global volume, reverb, and world sound effects
//// Controla el volumen global, reverb, y efectos de sonido del mundo
class SoundService : public Node {
    GDCLASS(SoundService, Node);

private:
    float ambient_reverb = 0.0f;
    float doppler_scale  = 1.0f;
    float distance_factor = 1.0f;

protected:
    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _enforce_game_parent(this);
    }
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_ambient_reverb", "v"), &SoundService::set_ambient_reverb);
        ClassDB::bind_method(D_METHOD("get_ambient_reverb"),       &SoundService::get_ambient_reverb);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "AmbientReverb",
                                  PROPERTY_HINT_RANGE, "0,10,0.1"),
                     "set_ambient_reverb", "get_ambient_reverb");
    }

public:
    void set_ambient_reverb(float v) { ambient_reverb = v; }
    float get_ambient_reverb() const { return ambient_reverb; }
};

// Manages world surface materials
//// Gestiona los materiales de superficie del mundo
class MaterialService : public Node {
    GDCLASS(MaterialService, Node);
protected:
    static void _bind_methods() {}
    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _enforce_game_parent(this);
    }
};

// Network management for multiplayer
//// Gestión de la red para multijugador
class NetworkClient : public Node {
    GDCLASS(NetworkClient, Node);
protected:
    static void _bind_methods() {}
    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _enforce_game_parent(this);
    }
};

// ════════════════════════════════════════════════════════════════════
//  TEXT CHAT SERVICE — Chat funcional tipo Roblox
// ════════════════════════════════════════════════════════════════════

// At runtime creates the chat UI. In Luau accessed via game:GetService("TextChatService")
//// En runtime crea la UI del chat. En Luau se accede via game:GetService("TextChatService")
class TextChatService : public Node {
    GDCLASS(TextChatService, Node);

private:
    RobloxChat* chat_window = nullptr;

protected:
    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _enforce_game_parent(this);
    }
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("send_message", "player", "message"),
                             &TextChatService::send_message_from_luau);
        ClassDB::bind_method(D_METHOD("set_player_name", "name"),
                             &TextChatService::set_player_name);
    }

public:
    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;

        // Create the chat system as a child
        //// Crear el sistema de chat como hijo
        chat_window = memnew(RobloxChat);
        chat_window->set_name("ChatWindow");
        add_child(chat_window);

        UtilityFunctions::print("[GodotLuau] TextChatService ready. Press '/' or 'Enter' to open the chat.");
    }

    // Send a message (callable from Luau)
    //// Enviar un mensaje (llamable desde Luau)
    void send_message_from_luau(String player, String message) {
        if (chat_window) {
            chat_window->send_message(player, message);
        }
    }

    // Change the local player name
    //// Cambiar nombre del jugador local
    void set_player_name(String name) {
        if (chat_window) {
            chat_window->set_player_name(name);
        }
    }

    RobloxChat* get_chat() const { return chat_window; }
};

// ════════════════════════════════════════════════════════════════════
//  RUN SERVICE — Motor de eventos por frame (equivalente 100% a Roblox)
//  Uso desde Luau:
//    local RS = game:GetService("RunService")
//    RS.Heartbeat:Connect(function(dt) ... end)
//    RS.RenderStepped:Connect(function(dt) ... end)
//    RS.Stepped:Connect(function(dt) ... end)
//    RS:IsRunning()  RS:IsClient()  RS:IsServer()
// ════════════════════════════════════════════════════════════════════
class RunService : public Node {
    GDCLASS(RunService, Node);

public:
    struct LuaCallback {
        lua_State* main_L;
        int        ref;
        bool       active = true;
        bool       once   = false;
    };

    // Coroutines waiting on the next signal fire (Signal:Wait())
    struct WaitCallback {
        lua_State* thread;
        lua_State* main_L;
    };

    std::vector<LuaCallback> heartbeat_cbs;
    std::vector<LuaCallback> render_stepped_cbs;
    std::vector<LuaCallback> stepped_cbs;

    std::vector<WaitCallback> heartbeat_wait_cbs;
    std::vector<WaitCallback> render_stepped_wait_cbs;
    std::vector<WaitCallback> stepped_wait_cbs;

    void add_heartbeat(lua_State* main_L, int ref, bool once = false)      { heartbeat_cbs.push_back({main_L, ref, true, once}); }
    void add_render_stepped(lua_State* main_L, int ref, bool once = false) { render_stepped_cbs.push_back({main_L, ref, true, once}); }
    void add_stepped(lua_State* main_L, int ref, bool once = false)        { stepped_cbs.push_back({main_L, ref, true, once}); }

    void add_heartbeat_wait(lua_State* th, lua_State* mL)      { heartbeat_wait_cbs.push_back({th, mL}); }
    void add_render_stepped_wait(lua_State* th, lua_State* mL) { render_stepped_wait_cbs.push_back({th, mL}); }
    void add_stepped_wait(lua_State* th, lua_State* mL)        { stepped_wait_cbs.push_back({th, mL}); }

    void remove_by_state(lua_State* L) {
        for (auto& cb : heartbeat_cbs)      if (cb.main_L == L) cb.active = false;
        for (auto& cb : render_stepped_cbs) if (cb.main_L == L) cb.active = false;
        for (auto& cb : stepped_cbs)        if (cb.main_L == L) cb.active = false;
        heartbeat_wait_cbs.erase(std::remove_if(heartbeat_wait_cbs.begin(), heartbeat_wait_cbs.end(),
            [L](const WaitCallback& w){ return w.main_L == L; }), heartbeat_wait_cbs.end());
        render_stepped_wait_cbs.erase(std::remove_if(render_stepped_wait_cbs.begin(), render_stepped_wait_cbs.end(),
            [L](const WaitCallback& w){ return w.main_L == L; }), render_stepped_wait_cbs.end());
        stepped_wait_cbs.erase(std::remove_if(stepped_wait_cbs.begin(), stepped_wait_cbs.end(),
            [L](const WaitCallback& w){ return w.main_L == L; }), stepped_wait_cbs.end());
    }

    void disconnect_by_ref(const char* ev, int ref) {
        auto deactivate = [ref](std::vector<LuaCallback>& list) {
            for (auto& cb : list) if (cb.ref == ref) { cb.active = false; return; }
        };
        if      (strcmp(ev, "Heartbeat")     == 0) deactivate(heartbeat_cbs);
        else if (strcmp(ev, "RenderStepped") == 0) deactivate(render_stepped_cbs);
        else if (strcmp(ev, "Stepped")       == 0) deactivate(stepped_cbs);
    }

protected:
    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _enforce_game_parent(this);
    }
    static void _bind_methods() {}

    static void fire_list(std::vector<LuaCallback>& list, double delta) {
        for (int i = (int)list.size() - 1; i >= 0; --i) {
            auto& cb = list[i];
            if (!cb.active) {
                list.erase(list.begin() + i);
                continue;
            }
            if (cb.once) cb.active = false;
            lua_State* thread = lua_newthread(cb.main_L);
            lua_rawgeti(cb.main_L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.main_L, -1)) {
                lua_xmove(cb.main_L, thread, 1);
                lua_pushnumber(thread, delta);
                int status = lua_resume(thread, nullptr, 1);
                if (status != LUA_OK && status != LUA_YIELD) {
                    UtilityFunctions::print("[RunService] Error: ", lua_tostring(thread, -1));
                }
            } else {
                lua_pop(cb.main_L, 1);
            }
            lua_pop(cb.main_L, 1);
        }
    }

    // Queue Signal:Wait() coroutines into the shared pending-resume list
    static void fire_wait_list(std::vector<WaitCallback>& list, double delta) {
        for (int i = (int)list.size() - 1; i >= 0; --i) {
            auto& wc = list[i];
            get_pending_resumes().push_back({wc.thread, wc.main_L, delta, nullptr});
            list.erase(list.begin() + i);
        }
    }

public:
    void _process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        fire_list(heartbeat_cbs, delta);
        fire_list(render_stepped_cbs, delta);
        fire_wait_list(heartbeat_wait_cbs, delta);
        fire_wait_list(render_stepped_wait_cbs, delta);
    }

    void _physics_process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        fire_list(stepped_cbs, delta);
        fire_wait_list(stepped_wait_cbs, delta);
    }
};

#endif // ROBLOX_SERVICES_H
