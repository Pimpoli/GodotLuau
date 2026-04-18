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

using namespace godot;

// ════════════════════════════════════════════════════════════════════
//  ALMACENAMIENTO
//  Equivalentes directos a los servicios de Roblox
// ════════════════════════════════════════════════════════════════════

// Visible para cliente Y servidor — se usa para ModuleScripts y objetos compartidos
class ReplicatedStorage : public Node {
    GDCLASS(ReplicatedStorage, Node);
protected:
    static void _bind_methods() {}
};

// Solo visible para el servidor — contenido seguro no accesible al cliente
class ServerStorage : public Node {
    GDCLASS(ServerStorage, Node);
protected:
    static void _bind_methods() {}
};

// Se replica al cliente ANTES de que cargue el resto de la escena
// Ideal para pantallas de carga, assets críticos
class ReplicatedFirst : public Node {
    GDCLASS(ReplicatedFirst, Node);
protected:
    static void _bind_methods() {}
};

// ════════════════════════════════════════════════════════════════════
//  SCRIPTS DEL SERVIDOR
// ════════════════════════════════════════════════════════════════════

// Scripts que solo se ejecutan en el servidor (lógica de juego segura)
class ServerScriptService : public Node {
    GDCLASS(ServerScriptService, Node);
protected:
    static void _bind_methods() {}
};

// ════════════════════════════════════════════════════════════════════
//  STARTER PLAYER — Sistema de scripts y personaje del jugador
// ════════════════════════════════════════════════════════════════════

// Scripts que se clonan en cada personaje al spawnear
class StarterCharacterScripts : public Node {
    GDCLASS(StarterCharacterScripts, Node);
protected:
    static void _bind_methods() {}
};

// Scripts que se clonan en el jugador local al entrar
class StarterPlayerScripts : public Node {
    GDCLASS(StarterPlayerScripts, Node);
protected:
    static void _bind_methods() {}
};

// Plantilla del personaje. Si está vacía se usa el personaje predeterminado (cápsula)
class StarterCharacter : public Node {
    GDCLASS(StarterCharacter, Node);
protected:
    static void _bind_methods() {}
};

// Container principal del jugador — crea sus hijos automáticamente al añadirlo al editor
class StarterPlayer : public Node {
    GDCLASS(StarterPlayer, Node);
protected:
    static void _bind_methods() {}

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE && Engine::get_singleton()->is_editor_hint()) {
            Node* root = get_tree()->get_edited_scene_root();
            if (!root) return;

            // Auto-crear hijos si no existen
            auto make_child = [&](const String& name, Node* child) {
                if (!get_node_or_null(name)) {
                    child->set_name(name);
                    add_child(child);
                    child->set_owner(root);
                }
            };

            if (!get_node_or_null("StarterCharacterScripts")) make_child("StarterCharacterScripts", memnew(StarterCharacterScripts));
            if (!get_node_or_null("StarterPlayerScripts"))    make_child("StarterPlayerScripts",    memnew(StarterPlayerScripts));
            if (!get_node_or_null("StarterCharacter"))        make_child("StarterCharacter",        memnew(StarterCharacter));
        }
    }
};

// ════════════════════════════════════════════════════════════════════
//  STARTER GUI Y PACK
// ════════════════════════════════════════════════════════════════════

// ScreenGuis aquí se clonan al HUD de cada jugador
class StarterGui : public Node {
    GDCLASS(StarterGui, Node);
protected:
    static void _bind_methods() {}
};

// Tools / items del inventario (mochila) del jugador
class StarterPack : public Node {
    GDCLASS(StarterPack, Node);
protected:
    static void _bind_methods() {}
};

// ════════════════════════════════════════════════════════════════════
//  JUGADORES Y EQUIPOS
// ════════════════════════════════════════════════════════════════════

// Servicio de jugadores — acceso via game:GetService("Players")
class Players : public Node {
    GDCLASS(Players, Node);
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("get_local_player"), &Players::get_local_player);
        ClassDB::bind_method(D_METHOD("get_players"),      &Players::get_players);
        ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "LocalPlayer",
                                  PROPERTY_HINT_NODE_TYPE, "Node"),
                     "", "get_local_player");
    }

public:
    // Busca el personaje del jugador en Workspace (RobloxPlayer o RobloxPlayer2D)
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
};

// Gestión de equipos — los jugadores pueden pertenecer a equipos con colores
class Teams : public Node {
    GDCLASS(Teams, Node);
protected:
    static void _bind_methods() {}
};

// Folder está definido en folder.h (ya existía antes)

// ════════════════════════════════════════════════════════════════════
//  ILUMINACIÓN Y AMBIENTE
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

    void _apply_sun() {
        DirectionalLight3D* sun = _find_sun();
        if (!sun) return;
        float max_elev = 90.0f - Math::abs(geographic_latitude);
        if (max_elev < 10.0f) max_elev = 10.0f;
        float t = (clock_time - 6.0f) / 12.0f;
        float elev;
        if (t >= 0.0f && t <= 1.0f) {
            elev = Math::sin(t * (float)Math_PI) * max_elev;
        } else {
            float t2 = (clock_time < 6.0f) ? (clock_time / 6.0f)
                                            : ((clock_time - 18.0f) / 6.0f);
            elev = -(Math::sin(t2 * (float)Math_PI) * 30.0f);
        }
        float sun_y = 45.0f + geographic_latitude * 0.3f;
        sun->set_rotation_degrees(Vector3(-elev, sun_y, 0.0f));
        sun->set_shadow(global_shadows);
        sun->set_param(Light3D::PARAM_SHADOW_BLUR,  shadow_softness * 4.0f);
        sun->set_param(Light3D::PARAM_ENERGY,       brightness);
        sun->set_param(Light3D::PARAM_INDIRECT_ENERGY, env_diffuse_scale);
        sun->set_param(Light3D::PARAM_SPECULAR,     env_specular_scale * 0.5f);
    }

    void _apply_env() {
        Ref<Environment> env = _get_env();
        if (!env.is_valid()) return;
        env->set_ambient_light_color(ambient);
        env->set_ambient_light_energy(brightness * 0.4f);
        env->set_ambient_light_sky_contribution(env_diffuse_scale * 0.5f);
        env->set_tonemap_exposure(1.0f + exposure_comp * 0.5f);
        // Fog
        bool use_depth_fog = (fog_end < 99000.0f && fog_end > fog_start);
        bool use_exp_fog   = (fog_density > 0.001f);
        env->set_fog_enabled(use_depth_fog || use_exp_fog);
        if (use_depth_fog) {
            env->set_fog_mode(Environment::FOG_MODE_DEPTH);
            env->set_fog_depth_begin(fog_start);
            env->set_fog_depth_end(fog_end);
            env->set_fog_light_color(fog_color);
            env->set_fog_density(0.01f);
        } else if (use_exp_fog) {
            env->set_fog_mode(Environment::FOG_MODE_EXPONENTIAL);
            env->set_fog_density(fog_density * 0.08f);
            env->set_fog_light_color(fog_color);
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
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _apply_all();
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
    void set_fog_color(Color c)         { fog_color = c;                             if(is_inside_tree()) _apply_env(); }
    Color get_fog_color() const         { return fog_color; }
};

// ════════════════════════════════════════════════════════════════════
//  AUDIO, MATERIALES, RED
// ════════════════════════════════════════════════════════════════════

// Controla el volumen global, reverb, y efectos de sonido del mundo
class SoundService : public Node {
    GDCLASS(SoundService, Node);

private:
    float ambient_reverb = 0.0f;
    float doppler_scale  = 1.0f;
    float distance_factor = 1.0f;

protected:
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

// Gestiona los materiales de superficie del mundo
class MaterialService : public Node {
    GDCLASS(MaterialService, Node);
protected:
    static void _bind_methods() {}
};

// Gestión de la red para multijugador
class NetworkClient : public Node {
    GDCLASS(NetworkClient, Node);
protected:
    static void _bind_methods() {}
};

// ════════════════════════════════════════════════════════════════════
//  TEXT CHAT SERVICE — Chat funcional tipo Roblox
// ════════════════════════════════════════════════════════════════════

// En runtime crea la UI del chat. En Luau se accede via game:GetService("TextChatService")
class TextChatService : public Node {
    GDCLASS(TextChatService, Node);

private:
    RobloxChat* chat_window = nullptr;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("send_message", "player", "message"),
                             &TextChatService::send_message_from_luau);
        ClassDB::bind_method(D_METHOD("set_player_name", "name"),
                             &TextChatService::set_player_name);
    }

public:
    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;

        // Crear el sistema de chat como hijo
        chat_window = memnew(RobloxChat);
        chat_window->set_name("ChatWindow");
        add_child(chat_window);

        UtilityFunctions::print("[GodotLuau] TextChatService listo. Presiona '/' o 'Enter' para abrir el chat.");
    }

    // Enviar un mensaje (llamable desde Luau)
    void send_message_from_luau(String player, String message) {
        if (chat_window) {
            chat_window->send_message(player, message);
        }
    }

    // Cambiar nombre del jugador local
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
    };

    std::vector<LuaCallback> heartbeat_cbs;
    std::vector<LuaCallback> render_stepped_cbs;
    std::vector<LuaCallback> stepped_cbs;

    void add_heartbeat(lua_State* main_L, int ref)       { heartbeat_cbs.push_back({main_L, ref, true}); }
    void add_render_stepped(lua_State* main_L, int ref)  { render_stepped_cbs.push_back({main_L, ref, true}); }
    void add_stepped(lua_State* main_L, int ref)         { stepped_cbs.push_back({main_L, ref, true}); }

    void remove_by_state(lua_State* L) {
        for (auto& cb : heartbeat_cbs)      if (cb.main_L == L) cb.active = false;
        for (auto& cb : render_stepped_cbs) if (cb.main_L == L) cb.active = false;
        for (auto& cb : stepped_cbs)        if (cb.main_L == L) cb.active = false;
    }

protected:
    static void _bind_methods() {}

    static void fire_list(std::vector<LuaCallback>& list, double delta) {
        for (int i = (int)list.size() - 1; i >= 0; --i) {
            auto& cb = list[i];
            if (!cb.active) {
                list.erase(list.begin() + i);
                continue;
            }
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

public:
    void _process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        fire_list(heartbeat_cbs, delta);
        fire_list(render_stepped_cbs, delta);
    }

    void _physics_process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        fire_list(stepped_cbs, delta);
    }
};

#endif // ROBLOX_SERVICES_H
