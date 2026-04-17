#ifndef ROBLOX_SERVICES_H
#define ROBLOX_SERVICES_H

#include "roblox_chat.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

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

// Controla el cielo, luz ambiente y efectos de iluminación del mundo
class Lighting : public Node {
    GDCLASS(Lighting, Node);

private:
    float brightness     = 1.0f;
    float clock_time     = 14.0f;  // Hora del día (0-24)
    Color ambient        = Color(0.5f, 0.5f, 0.5f);
    bool  global_shadows = true;
    float fog_density    = 0.0f;
    Color fog_color      = Color(0.75f, 0.85f, 1.0f);

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_brightness", "v"), &Lighting::set_brightness);
        ClassDB::bind_method(D_METHOD("get_brightness"),       &Lighting::get_brightness);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Brightness",
                                  PROPERTY_HINT_RANGE, "0,2,0.01"),
                     "set_brightness", "get_brightness");

        ClassDB::bind_method(D_METHOD("set_clock_time", "v"), &Lighting::set_clock_time);
        ClassDB::bind_method(D_METHOD("get_clock_time"),       &Lighting::get_clock_time);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ClockTime",
                                  PROPERTY_HINT_RANGE, "0,24,0.1"),
                     "set_clock_time", "get_clock_time");

        ClassDB::bind_method(D_METHOD("set_ambient", "c"), &Lighting::set_ambient);
        ClassDB::bind_method(D_METHOD("get_ambient"),       &Lighting::get_ambient);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR, "Ambient"),
                     "set_ambient", "get_ambient");

        ClassDB::bind_method(D_METHOD("set_global_shadows", "v"), &Lighting::set_global_shadows);
        ClassDB::bind_method(D_METHOD("get_global_shadows"),       &Lighting::get_global_shadows);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "GlobalShadows"),
                     "set_global_shadows", "get_global_shadows");

        ClassDB::bind_method(D_METHOD("set_fog_density", "v"), &Lighting::set_fog_density);
        ClassDB::bind_method(D_METHOD("get_fog_density"),       &Lighting::get_fog_density);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "FogDensity",
                                  PROPERTY_HINT_RANGE, "0,1,0.001"),
                     "set_fog_density", "get_fog_density");
    }

public:
    void set_brightness(float v)     { brightness = Math::clamp(v, 0.0f, 2.0f); }
    float get_brightness() const     { return brightness; }
    void set_clock_time(float v)     { clock_time = Math::fmod(v, 24.0f); }
    float get_clock_time() const     { return clock_time; }
    void set_ambient(Color c)        { ambient = c; }
    Color get_ambient() const        { return ambient; }
    void set_global_shadows(bool v)  { global_shadows = v; }
    bool get_global_shadows() const  { return global_shadows; }
    void set_fog_density(float v)    { fog_density = Math::clamp(v, 0.0f, 1.0f); }
    float get_fog_density() const    { return fog_density; }
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
