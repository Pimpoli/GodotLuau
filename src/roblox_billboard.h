#ifndef ROBLOX_BILLBOARD_H
#define ROBLOX_BILLBOARD_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/classes/node.hpp>
#include <vector>

#include "lua.h"
#include "lualib.h"
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

using namespace godot;

// ────────────────────────────────────────────────────────────────────
//  BillboardGui — 3D GUI that always faces the camera (above a Part)
//  Implemented with Label3D in Billboard mode for maximum compatibility
////
//  BillboardGui — GUI 3D que siempre mira a la cámara (sobre un Part)
//  Implementado con Label3D en modo Billboard para máxima compatibilidad
// ────────────────────────────────────────────────────────────────────
class BillboardGui : public Node3D {
    GDCLASS(BillboardGui, Node3D);

    bool    enabled           = true;
    bool    always_on_top     = false;
    float   size_x            = 4.0f;
    float   size_y            = 2.0f;
    Vector3 studies_offset    = Vector3();
    float   max_distance      = 100.0f;
    float   min_distance      = 0.0f;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_enabled", "v"),       &BillboardGui::set_enabled);
        ClassDB::bind_method(D_METHOD("get_enabled"),             &BillboardGui::get_enabled);
        ClassDB::bind_method(D_METHOD("set_size_x", "v"),        &BillboardGui::set_size_x);
        ClassDB::bind_method(D_METHOD("get_size_x"),              &BillboardGui::get_size_x);
        ClassDB::bind_method(D_METHOD("set_size_y", "v"),        &BillboardGui::set_size_y);
        ClassDB::bind_method(D_METHOD("get_size_y"),              &BillboardGui::get_size_y);
        ClassDB::bind_method(D_METHOD("set_always_on_top", "v"), &BillboardGui::set_always_on_top);
        ClassDB::bind_method(D_METHOD("get_always_on_top"),       &BillboardGui::get_always_on_top);
        ClassDB::bind_method(D_METHOD("set_max_distance", "v"),  &BillboardGui::set_max_distance);
        ClassDB::bind_method(D_METHOD("get_max_distance"),        &BillboardGui::get_max_distance);

        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "Enabled"),      "set_enabled",       "get_enabled");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "AlwaysOnTop"),  "set_always_on_top", "get_always_on_top");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "SizeX",  PROPERTY_HINT_RANGE, "0.1,50,0.1"), "set_size_x", "get_size_x");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "SizeY",  PROPERTY_HINT_RANGE, "0.1,50,0.1"), "set_size_y", "get_size_y");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "MaxDistance", PROPERTY_HINT_RANGE, "0,500,1"), "set_max_distance", "get_max_distance");
    }

public:
    void set_enabled(bool v)        { enabled = v; set_visible(v); }
    bool get_enabled() const        { return enabled; }
    void set_size_x(float v)        { size_x = v; }
    float get_size_x() const        { return size_x; }
    void set_size_y(float v)        { size_y = v; }
    float get_size_y() const        { return size_y; }
    void set_always_on_top(bool v)  { always_on_top = v; }
    bool get_always_on_top() const  { return always_on_top; }
    void set_max_distance(float v)  { max_distance = v; }
    float get_max_distance() const  { return max_distance; }

    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        // All child Label3D/etc. placed as children from Lua will be
        // billboard-mode by default when added to BillboardGui
        for (int i = 0; i < get_child_count(); i++) {
            Label3D* lbl = Object::cast_to<Label3D>(get_child(i));
            if (lbl) lbl->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
        }
    }
};

// ────────────────────────────────────────────────────────────────────
//  SurfaceGui — GUI rendered on the surface of a Part
//  Uses a SubViewport projected onto a quad of the Part
////
//  SurfaceGui — GUI renderizada en la superficie de un Part
//  Usa SubViewport proyectado sobre un cuadrilátero del Part
// ────────────────────────────────────────────────────────────────────
class SurfaceGui : public Node3D {
    GDCLASS(SurfaceGui, Node3D);

    bool    enabled        = true;
    bool    always_on_top  = false;
    int     pixels_per_stud = 20;
    String  adornee        = "";
    int     face           = 1;  // Front=1, Back=2, Left=3, Right=4, Top=5, Bottom=6
    float   brightness     = 1.0f;
    float   light_influence = 1.0f;
    bool    active         = true;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_enabled", "v"),           &SurfaceGui::set_enabled);
        ClassDB::bind_method(D_METHOD("get_enabled"),                 &SurfaceGui::get_enabled);
        ClassDB::bind_method(D_METHOD("set_pixels_per_stud", "v"),   &SurfaceGui::set_pixels_per_stud);
        ClassDB::bind_method(D_METHOD("get_pixels_per_stud"),         &SurfaceGui::get_pixels_per_stud);
        ClassDB::bind_method(D_METHOD("set_face", "v"),              &SurfaceGui::set_face);
        ClassDB::bind_method(D_METHOD("get_face"),                    &SurfaceGui::get_face);
        ClassDB::bind_method(D_METHOD("set_brightness", "v"),        &SurfaceGui::set_brightness);
        ClassDB::bind_method(D_METHOD("get_brightness"),              &SurfaceGui::get_brightness);

        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "Enabled"),       "set_enabled",         "get_enabled");
        ADD_PROPERTY(PropertyInfo(Variant::INT,   "PixelsPerStuds", PROPERTY_HINT_RANGE, "1,200,1"), "set_pixels_per_stud", "get_pixels_per_stud");
        ADD_PROPERTY(PropertyInfo(Variant::INT,   "Face",           PROPERTY_HINT_RANGE, "0,5,1"),   "set_face",            "get_face");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Brightness",     PROPERTY_HINT_RANGE, "0,2,0.01"), "set_brightness",     "get_brightness");
    }

public:
    void set_enabled(bool v)         { enabled = v; set_visible(v); }
    bool get_enabled() const         { return enabled; }
    void set_pixels_per_stud(int v)  { pixels_per_stud = v; }
    int  get_pixels_per_stud() const { return pixels_per_stud; }
    void set_face(int v)             { face = v; }
    int  get_face() const            { return face; }
    void set_brightness(float v)     { brightness = v; }
    float get_brightness() const     { return brightness; }
};

// ────────────────────────────────────────────────────────────────────
//  Motor6D — motorized joint from Roblox (for animations)
//  Controls the transform between two Parts
////
//  Motor6D — junta motorizada de Roblox (para animaciones)
//  Controla la transformación entre dos Parts
// ────────────────────────────────────────────────────────────────────
class Motor6D : public Node {
    GDCLASS(Motor6D, Node);

    Node*       part0         = nullptr;
    Node*       part1         = nullptr;
    Transform3D c0;
    Transform3D c1;
    float       max_velocity  = 9999.0f;
    float       current_angle = 0.0f;
    float       desired_angle = 0.0f;
    bool        enabled       = true;
    String      part0_path    = "";
    String      part1_path    = "";

    Transform3D c0_inv;
    bool        c0_inv_valid = false;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_part0_path", "p"), &Motor6D::set_part0_path);
        ClassDB::bind_method(D_METHOD("get_part0_path"),       &Motor6D::get_part0_path);
        ClassDB::bind_method(D_METHOD("set_part1_path", "p"), &Motor6D::set_part1_path);
        ClassDB::bind_method(D_METHOD("get_part1_path"),       &Motor6D::get_part1_path);
        ClassDB::bind_method(D_METHOD("set_enabled", "v"),    &Motor6D::set_enabled);
        ClassDB::bind_method(D_METHOD("get_enabled"),          &Motor6D::get_enabled);
        ClassDB::bind_method(D_METHOD("set_max_velocity", "v"), &Motor6D::set_max_velocity);
        ClassDB::bind_method(D_METHOD("get_max_velocity"),      &Motor6D::get_max_velocity);
        ClassDB::bind_method(D_METHOD("set_desired_angle", "v"), &Motor6D::set_desired_angle);
        ClassDB::bind_method(D_METHOD("get_desired_angle"),      &Motor6D::get_desired_angle);
        ClassDB::bind_method(D_METHOD("set_c0", "t"),          &Motor6D::set_c0);
        ClassDB::bind_method(D_METHOD("get_c0"),               &Motor6D::get_c0);
        ClassDB::bind_method(D_METHOD("set_c1", "t"),          &Motor6D::set_c1);
        ClassDB::bind_method(D_METHOD("get_c1"),               &Motor6D::get_c1);

        ADD_PROPERTY(PropertyInfo(Variant::STRING, "Part0"), "set_part0_path", "get_part0_path");
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "Part1"), "set_part1_path", "get_part1_path");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,   "Enabled"), "set_enabled", "get_enabled");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "MaxVelocity", PROPERTY_HINT_RANGE, "0,100,0.1"), "set_max_velocity", "get_max_velocity");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "DesiredAngle"), "set_desired_angle", "get_desired_angle");
        ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "C0"), "set_c0", "get_c0");
        ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "C1"), "set_c1", "get_c1");
    }

public:
    void set_part0_path(const String& p) { part0_path = p; _resolve_parts(); }
    String get_part0_path() const        { return part0_path; }
    void set_part1_path(const String& p) { part1_path = p; _resolve_parts(); }
    String get_part1_path() const        { return part1_path; }
    void set_enabled(bool v)             { enabled = v; }
    bool get_enabled() const             { return enabled; }
    void set_max_velocity(float v)       { max_velocity = v; }
    float get_max_velocity() const       { return max_velocity; }
    void set_desired_angle(float v)      { desired_angle = v; }
    float get_desired_angle() const      { return desired_angle; }
    void set_c0(const Transform3D& t)    { c0 = t; c0_inv = t.inverse(); c0_inv_valid = true; }
    Transform3D get_c0() const           { return c0; }
    void set_c1(const Transform3D& t)    { c1 = t; }
    Transform3D get_c1() const           { return c1; }

    void set_part0(Node* n) { part0 = n; }
    void set_part1(Node* n) { part1 = n; }
    Node* get_part0() const { return part0; }
    Node* get_part1() const { return part1; }

    void _ready() override {
        _resolve_parts();
        set_physics_process(enabled && part0 && part1);
    }

    void _resolve_parts() {
        if (!is_inside_tree()) return;
        if (!part0_path.is_empty()) {
            Node* n = get_node_or_null(NodePath(part0_path));
            if (n) part0 = n;
        }
        if (!part1_path.is_empty()) {
            Node* n = get_node_or_null(NodePath(part1_path));
            if (n) part1 = n;
        }
    }

    void _physics_process(double delta) override {
        if (!enabled || !part0 || !part1) return;
        Node3D* n0 = Object::cast_to<Node3D>(part0);
        Node3D* n1 = Object::cast_to<Node3D>(part1);
        if (!n0 || !n1) return;

        // Move current angle toward desired_angle
        //// Mover ángulo actual hacia desired_angle
        float diff = desired_angle - current_angle;
        float step = Math::clamp(diff, -(float)delta * max_velocity, (float)delta * max_velocity);
        current_angle += step;

        // Apply transform: Part1.GlobalTransform = Part0.GlobalTransform * C0 * rotation * C1.inverse
        //// Aplicar la transformación: Part1.GlobalTransform = Part0.GlobalTransform * C0 * rotation * C1.inverse
        Transform3D rot_t;
        rot_t.basis = Basis(Vector3(0, 1, 0), current_angle); // Y rotation by default / rotación en Y por defecto
        Transform3D world = n0->get_global_transform() * c0 * rot_t * c1.inverse();
        n1->set_global_transform(world);
    }
};

// ────────────────────────────────────────────────────────────────────
//  Tool — equippable inventory item
////
//  Tool — herramienta equippable del inventario
// ────────────────────────────────────────────────────────────────────
class RobloxTool : public Node3D {
    GDCLASS(RobloxTool, Node3D);

public:
    struct LuaCallback { lua_State* main_L; int ref; bool active = true; };

    std::vector<LuaCallback> equipped_cbs;
    std::vector<LuaCallback> unequipped_cbs;
    std::vector<LuaCallback> activated_cbs;
    std::vector<LuaCallback> deactivated_cbs;

private:
    String  tool_tip         = "";
    bool    requires_handle  = true;
    bool    can_be_dropped   = true;
    bool    manually_activated = false;
    bool    grip_forward     = true;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_tool_tip", "v"),           &RobloxTool::set_tool_tip);
        ClassDB::bind_method(D_METHOD("get_tool_tip"),                  &RobloxTool::get_tool_tip);
        ClassDB::bind_method(D_METHOD("set_can_be_dropped", "v"),     &RobloxTool::set_can_be_dropped);
        ClassDB::bind_method(D_METHOD("get_can_be_dropped"),           &RobloxTool::get_can_be_dropped);
        ClassDB::bind_method(D_METHOD("set_requires_handle", "v"),    &RobloxTool::set_requires_handle);
        ClassDB::bind_method(D_METHOD("get_requires_handle"),          &RobloxTool::get_requires_handle);
        ClassDB::bind_method(D_METHOD("equip"),   &RobloxTool::equip);
        ClassDB::bind_method(D_METHOD("unequip"), &RobloxTool::unequip);
        ClassDB::bind_method(D_METHOD("activate"), &RobloxTool::activate);

        ADD_PROPERTY(PropertyInfo(Variant::STRING, "ToolTip"),        "set_tool_tip",       "get_tool_tip");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,   "CanBeDropped"),   "set_can_be_dropped", "get_can_be_dropped");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,   "RequiresHandle"), "set_requires_handle","get_requires_handle");

        ADD_SIGNAL(MethodInfo("Equipped",     PropertyInfo(Variant::OBJECT, "mouse")));
        ADD_SIGNAL(MethodInfo("Unequipped"));
        ADD_SIGNAL(MethodInfo("Activated"));
        ADD_SIGNAL(MethodInfo("Deactivated"));
    }

public:
    void set_tool_tip(const String& v)      { tool_tip = v; }
    String get_tool_tip() const             { return tool_tip; }
    void set_can_be_dropped(bool v)         { can_be_dropped = v; }
    bool get_can_be_dropped() const         { return can_be_dropped; }
    void set_requires_handle(bool v)        { requires_handle = v; }
    bool get_requires_handle() const        { return requires_handle; }

    void add_equipped_cb(lua_State* L, int ref)     { equipped_cbs.push_back({L,ref,true}); }
    void add_unequipped_cb(lua_State* L, int ref)   { unequipped_cbs.push_back({L,ref,true}); }
    void add_activated_cb(lua_State* L, int ref)    { activated_cbs.push_back({L,ref,true}); }
    void add_deactivated_cb(lua_State* L, int ref)  { deactivated_cbs.push_back({L,ref,true}); }

    void equip(Node* player = nullptr) {
        emit_signal("Equipped", player);
        _fire(equipped_cbs, player);
    }
    void unequip() {
        emit_signal("Unequipped");
        _fire_noarg(unequipped_cbs);
    }
    void activate() {
        emit_signal("Activated");
        _fire_noarg(activated_cbs);
    }
    void deactivate() {
        emit_signal("Deactivated");
        _fire_noarg(deactivated_cbs);
    }

private:
    void _fire(std::vector<LuaCallback>& list, Node* arg) {
        for (int i=(int)list.size()-1; i>=0; --i) {
            auto& cb=list[i];
            if (!cb.active){list.erase(list.begin()+i);continue;}
            lua_State* th=lua_newthread(cb.main_L);
            lua_rawgeti(cb.main_L,LUA_REGISTRYINDEX,cb.ref);
            if(lua_isfunction(cb.main_L,-1)){
                lua_xmove(cb.main_L,th,1);
                if(arg) lua_pushlightuserdata(th,(void*)arg); else lua_pushnil(th);
                lua_resume(th,nullptr,1);
            } else lua_pop(cb.main_L,1);
            lua_pop(cb.main_L,1);
        }
    }
    void _fire_noarg(std::vector<LuaCallback>& list) {
        for (int i=(int)list.size()-1; i>=0; --i) {
            auto& cb=list[i];
            if (!cb.active){list.erase(list.begin()+i);continue;}
            lua_State* th=lua_newthread(cb.main_L);
            lua_rawgeti(cb.main_L,LUA_REGISTRYINDEX,cb.ref);
            if(lua_isfunction(cb.main_L,-1)){lua_xmove(cb.main_L,th,1);lua_resume(th,nullptr,0);}
            else lua_pop(cb.main_L,1);
            lua_pop(cb.main_L,1);
        }
    }
};

// ────────────────────────────────────────────────────────────────────
//  Backpack — player inventory container
////
//  Backpack — inventario del jugador
// ────────────────────────────────────────────────────────────────────
class Backpack : public Node {
    GDCLASS(Backpack, Node);

protected:
    static void _bind_methods() {}
};

#endif // ROBLOX_BILLBOARD_H
