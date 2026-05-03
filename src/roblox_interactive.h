#ifndef ROBLOX_INTERACTIVE_H
#define ROBLOX_INTERACTIVE_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/area3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/color.hpp>
#include <vector>
#include <cstring>

#include "lua.h"
#include "lualib.h"

using namespace godot;

// ────────────────────────────────────────────────────────────────────
//  ClickDetector — exact equivalent to Roblox ClickDetector
//  Usage from Luau:
//    detector.MouseClick:Connect(function(player) end)
//    detector.MouseHoverEnter:Connect(function(player) end)
////
//  ClickDetector — equivalente exacto a Roblox ClickDetector
//  Uso desde Luau:
//    detector.MouseClick:Connect(function(player) end)
//    detector.MouseHoverEnter:Connect(function(player) end)
// ────────────────────────────────────────────────────────────────────
class ClickDetector : public Area3D {
    GDCLASS(ClickDetector, Area3D);

public:
    struct LuaCallback { lua_State* main_L; int ref; bool active = true; };

    std::vector<LuaCallback> mouse_click_cbs;
    std::vector<LuaCallback> hover_enter_cbs;
    std::vector<LuaCallback> hover_leave_cbs;

    float max_activation_distance = 32.0f;
    bool  cursor_icon_change       = true;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_max_distance", "d"), &ClickDetector::set_max_distance);
        ClassDB::bind_method(D_METHOD("get_max_distance"),      &ClickDetector::get_max_distance);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "MaxActivationDistance", PROPERTY_HINT_RANGE, "0,1000,0.1"),
                     "set_max_distance", "get_max_distance");
        ADD_SIGNAL(MethodInfo("MouseClick",      PropertyInfo(Variant::OBJECT, "player")));
        ADD_SIGNAL(MethodInfo("MouseHoverEnter", PropertyInfo(Variant::OBJECT, "player")));
        ADD_SIGNAL(MethodInfo("MouseHoverLeave", PropertyInfo(Variant::OBJECT, "player")));
    }

public:
    void set_max_distance(float d) { max_activation_distance = d; }
    float get_max_distance() const { return max_activation_distance; }

    void add_click_cb(lua_State* L, int ref) {
        mouse_click_cbs.push_back({L, ref, true});
    }
    void add_hover_enter_cb(lua_State* L, int ref) {
        hover_enter_cbs.push_back({L, ref, true});
    }
    void add_hover_leave_cb(lua_State* L, int ref) {
        hover_leave_cbs.push_back({L, ref, true});
    }

    void remove_callbacks_for_state(lua_State* L) {
        for (auto& cb : mouse_click_cbs) if (cb.main_L == L) cb.active = false;
        for (auto& cb : hover_enter_cbs) if (cb.main_L == L) cb.active = false;
        for (auto& cb : hover_leave_cbs) if (cb.main_L == L) cb.active = false;
    }

    void fire_click(Node* player = nullptr) {
        emit_signal("MouseClick", player);
        _fire_list(mouse_click_cbs, player);
    }

    void fire_hover_enter(Node* player = nullptr) {
        emit_signal("MouseHoverEnter", player);
        _fire_list(hover_enter_cbs, player);
    }

    void fire_hover_leave(Node* player = nullptr) {
        emit_signal("MouseHoverLeave", player);
        _fire_list(hover_leave_cbs, player);
    }

private:
    void _fire_list(std::vector<LuaCallback>& list, Node* player) {
        for (int i = (int)list.size() - 1; i >= 0; --i) {
            auto& cb = list[i];
            if (!cb.active) { list.erase(list.begin() + i); continue; }
            lua_State* th = lua_newthread(cb.main_L);
            lua_rawgeti(cb.main_L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.main_L, -1)) {
                lua_xmove(cb.main_L, th, 1);
                if (player) lua_pushlightuserdata(th, (void*)player);
                else        lua_pushnil(th);
                lua_resume(th, nullptr, 1);
            } else lua_pop(cb.main_L, 1);
            lua_pop(cb.main_L, 1);
        }
    }
};

// ────────────────────────────────────────────────────────────────────
//  ProximityPrompt — equivalent to Roblox ProximityPrompt
//  Shows a keybind hint when nearby and fires Triggered
//  Usage from Luau:
//    prompt.ActionText = "Open Door"
//    prompt.KeyboardKeyCode = "E"
//    prompt.Triggered:Connect(function(player) end)
////
//  ProximityPrompt — equivalente a Roblox ProximityPrompt
//  Muestra un keybind al acercarse y dispara Triggered
//  Uso desde Luau:
//    prompt.ActionText = "Open Door"
//    prompt.KeyboardKeyCode = "E"
//    prompt.Triggered:Connect(function(player) end)
// ────────────────────────────────────────────────────────────────────
class ProximityPrompt : public Node3D {
    GDCLASS(ProximityPrompt, Node3D);

public:
    struct LuaCallback { lua_State* main_L; int ref; bool active = true; };

    std::vector<LuaCallback> triggered_cbs;
    std::vector<LuaCallback> hold_began_cbs;
    std::vector<LuaCallback> hold_ended_cbs;

private:
    String  action_text        = "Interact";
    String  object_text        = "";
    String  key_code           = "E";
    float   max_distance       = 10.0f;
    float   hold_duration      = 0.0f;
    bool    enabled            = true;
    bool    require_line_sight = false;
    float   ui_offset_y        = 0.0f;

    Area3D*          trigger_area  = nullptr;
    Label3D*         label_node    = nullptr;
    bool             player_near   = false;
    float            hold_elapsed  = 0.0f;
    bool             holding       = false;

    void _setup_area() {
        if (trigger_area) return;
        trigger_area = memnew(Area3D);
        trigger_area->set_name("__prompt_area__");
        CollisionShape3D* cs = memnew(CollisionShape3D);
        Ref<SphereShape3D> sp = Ref<SphereShape3D>(memnew(SphereShape3D));
        sp->set_radius(max_distance);
        cs->set_shape(sp);
        trigger_area->add_child(cs);
        add_child(trigger_area);

        trigger_area->connect("body_entered", callable_mp(this, &ProximityPrompt::_on_body_entered));
        trigger_area->connect("body_exited",  callable_mp(this, &ProximityPrompt::_on_body_exited));
    }

    void _setup_label() {
        if (label_node) return;
        label_node = memnew(Label3D);
        label_node->set_name("__prompt_label__");
        label_node->set_position(Vector3(0, 0.5f + ui_offset_y, 0));
        label_node->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
        label_node->set_visible(false);
        _update_label_text();
        add_child(label_node);
    }

    void _update_label_text() {
        if (!label_node) return;
        String txt = "[" + key_code + "] " + action_text;
        if (!object_text.is_empty()) txt += "\n" + object_text;
        label_node->set_text(txt);
    }

    void _on_body_entered(Node* body) {
        if (!enabled) return;
        player_near = true;
        if (label_node) label_node->set_visible(true);
    }

    void _on_body_exited(Node* body) {
        player_near = false;
        if (label_node) label_node->set_visible(false);
        holding = false;
        hold_elapsed = 0.0f;
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_action_text", "t"),   &ProximityPrompt::set_action_text);
        ClassDB::bind_method(D_METHOD("get_action_text"),         &ProximityPrompt::get_action_text);
        ClassDB::bind_method(D_METHOD("set_max_distance", "d"),   &ProximityPrompt::set_max_distance);
        ClassDB::bind_method(D_METHOD("get_max_distance"),         &ProximityPrompt::get_max_distance);
        ClassDB::bind_method(D_METHOD("set_hold_duration", "d"),  &ProximityPrompt::set_hold_duration);
        ClassDB::bind_method(D_METHOD("get_hold_duration"),        &ProximityPrompt::get_hold_duration);
        ClassDB::bind_method(D_METHOD("set_key_code", "k"),       &ProximityPrompt::set_key_code);
        ClassDB::bind_method(D_METHOD("get_key_code"),             &ProximityPrompt::get_key_code);
        ClassDB::bind_method(D_METHOD("set_enabled", "v"),        &ProximityPrompt::set_enabled);
        ClassDB::bind_method(D_METHOD("get_enabled"),              &ProximityPrompt::get_enabled);
        ClassDB::bind_method(D_METHOD("trigger"),                  &ProximityPrompt::trigger);

        ADD_PROPERTY(PropertyInfo(Variant::STRING, "ActionText"),  "set_action_text",  "get_action_text");
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "KeyboardKeyCode"), "set_key_code", "get_key_code");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "MaxActivationDistance", PROPERTY_HINT_RANGE, "0,100,0.1"), "set_max_distance", "get_max_distance");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "HoldDuration", PROPERTY_HINT_RANGE, "0,10,0.1"), "set_hold_duration", "get_hold_duration");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,   "Enabled"), "set_enabled", "get_enabled");

        ADD_SIGNAL(MethodInfo("Triggered",  PropertyInfo(Variant::OBJECT, "player")));
        ADD_SIGNAL(MethodInfo("HoldBegan",  PropertyInfo(Variant::OBJECT, "player")));
        ADD_SIGNAL(MethodInfo("HoldEnded",  PropertyInfo(Variant::OBJECT, "player")));
    }

public:
    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        _setup_area();
        _setup_label();
    }

    void set_action_text(const String& t)  { action_text = t; _update_label_text(); }
    String get_action_text() const         { return action_text; }
    void set_max_distance(float d)         { max_distance = d; }
    float get_max_distance() const         { return max_distance; }
    void set_hold_duration(float d)        { hold_duration = d; }
    float get_hold_duration() const        { return hold_duration; }
    void set_key_code(const String& k)     { key_code = k; _update_label_text(); }
    String get_key_code() const            { return key_code; }
    void set_enabled(bool v)               { enabled = v; if (label_node && !v) label_node->set_visible(false); }
    bool get_enabled() const               { return enabled; }

    void add_triggered_cb(lua_State* L, int ref)  { triggered_cbs.push_back({L, ref, true}); }
    void add_hold_began_cb(lua_State* L, int ref) { hold_began_cbs.push_back({L, ref, true}); }
    void add_hold_ended_cb(lua_State* L, int ref) { hold_ended_cbs.push_back({L, ref, true}); }

    void remove_callbacks_for_state(lua_State* L) {
        for (auto& cb : triggered_cbs)  if (cb.main_L == L) cb.active = false;
        for (auto& cb : hold_began_cbs) if (cb.main_L == L) cb.active = false;
        for (auto& cb : hold_ended_cbs) if (cb.main_L == L) cb.active = false;
    }

    void trigger(Node* player = nullptr) {
        if (!enabled) return;
        emit_signal("Triggered", player);
        _fire_list(triggered_cbs, player);
    }

private:
    void _fire_list(std::vector<LuaCallback>& list, Node* player) {
        for (int i = (int)list.size() - 1; i >= 0; --i) {
            auto& cb = list[i];
            if (!cb.active) { list.erase(list.begin() + i); continue; }
            lua_State* th = lua_newthread(cb.main_L);
            lua_rawgeti(cb.main_L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.main_L, -1)) {
                lua_xmove(cb.main_L, th, 1);
                if (player) lua_pushlightuserdata(th, (void*)player);
                else        lua_pushnil(th);
                lua_resume(th, nullptr, 1);
            } else lua_pop(cb.main_L, 1);
            lua_pop(cb.main_L, 1);
        }
    }
};

// ────────────────────────────────────────────────────────────────────
//  SpawnLocation — equivalent to Roblox SpawnLocation
//  Marks where the character will appear when the scene loads
////
//  SpawnLocation — equivalente a Roblox SpawnLocation
//  Marca donde el personaje aparecerá al cargar la escena
// ────────────────────────────────────────────────────────────────────
class SpawnLocation : public Node3D {
    GDCLASS(SpawnLocation, Node3D);

    bool    enabled          = true;
    float   duration         = 10.0f;
    bool    allow_team_change = false;
    String  team_color       = "Bright red";
    bool    neutral          = true;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_enabled", "v"),            &SpawnLocation::set_enabled);
        ClassDB::bind_method(D_METHOD("get_enabled"),                  &SpawnLocation::get_enabled);
        ClassDB::bind_method(D_METHOD("set_duration", "v"),           &SpawnLocation::set_duration);
        ClassDB::bind_method(D_METHOD("get_duration"),                 &SpawnLocation::get_duration);
        ClassDB::bind_method(D_METHOD("set_neutral", "v"),            &SpawnLocation::set_neutral);
        ClassDB::bind_method(D_METHOD("get_neutral"),                  &SpawnLocation::get_neutral);
        ClassDB::bind_method(D_METHOD("set_allow_team_change", "v"),  &SpawnLocation::set_allow_team_change);
        ClassDB::bind_method(D_METHOD("get_allow_team_change"),        &SpawnLocation::get_allow_team_change);

        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "Enabled"),          "set_enabled",           "get_enabled");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Duration",
                                  PROPERTY_HINT_RANGE, "0,60,0.1"),    "set_duration",          "get_duration");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "Neutral"),          "set_neutral",           "get_neutral");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "AllowTeamChangeOnTouch"), "set_allow_team_change", "get_allow_team_change");
    }

public:
    void set_enabled(bool v)           { enabled = v; }
    bool get_enabled() const           { return enabled; }
    void set_duration(float v)         { duration = v; }
    float get_duration() const         { return duration; }
    void set_neutral(bool v)           { neutral = v; }
    bool get_neutral() const           { return neutral; }
    void set_allow_team_change(bool v) { allow_team_change = v; }
    bool get_allow_team_change() const { return allow_team_change; }

    Vector3 get_spawn_position() const { return get_global_position() + Vector3(0, 3.0f, 0); }
};

#endif // ROBLOX_INTERACTIVE_H
