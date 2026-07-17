#ifndef HUMANOID_H
#define HUMANOID_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/classes/kinematic_collision3d.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <vector>

#include "lua.h"
#include "lualib.h"
#include "gl_errors.h"
#include "gl_runtime.h"   // gl_state_alive

using namespace godot;

// ════════════════════════════════════════════════════════════════════
//  Humanoid — Movement and health system for 3D characters
//
//  Humanoid is the character's "engine". It handles:
//  - Movement physics (WASD + camera)
//  - Health and damage
//  - Signals (Died, HealthChanged)
//  - Collision detection with RobloxParts (Touched event)
//
//  Properties editable from Luau:
//    humanoid.WalkSpeed  = 20    -- Normal speed
//    humanoid.JumpPower  = 20    -- Jump force
//    humanoid.Health     = 100   -- Current health
////
//  Humanoid — Sistema de movimiento y salud del personaje 3D
//
//  El Humanoid es el "motor" del personaje. Maneja:
//  - Física de movimiento (WASD + cámara)
//  - Salud y daño
//  - Señales (Died, HealthChanged)
//  - Detección de colisiones con RobloxParts (evento Touched)
//
//  Propiedades modificables desde Luau:
//    humanoid.WalkSpeed  = 20    -- Velocidad normal
//    humanoid.JumpPower  = 20    -- Fuerza del salto
//    humanoid.Health     = 100   -- Salud actual
// ════════════════════════════════════════════════════════════════════
class Humanoid : public Node {
    GDCLASS(Humanoid, Node);

public:
    // Callback for Luau signals (Died, HealthChanged)
    //// Callback para señales Luau (Died, HealthChanged)
    struct LuaCallback {
        lua_State* main_L;
        int ref;
        bool active = true;
    };

    std::vector<LuaCallback> died_cbs;
    std::vector<LuaCallback> health_changed_cbs;
    std::vector<LuaCallback> state_changed_cbs;

    void add_died_callback(lua_State* main_L, int ref) {
        died_cbs.push_back({main_L, ref, true});
    }
    void add_health_changed_callback(lua_State* main_L, int ref) {
        health_changed_cbs.push_back({main_L, ref, true});
    }
    void add_state_changed_callback(lua_State* main_L, int ref) {
        state_changed_cbs.push_back({main_L, ref, true});
    }
    void remove_callbacks_for_state(lua_State* L) {
        for (auto& cb : died_cbs)           if (cb.main_L == L) cb.active = false;
        for (auto& cb : health_changed_cbs) if (cb.main_L == L) cb.active = false;
        for (auto& cb : state_changed_cbs)  if (cb.main_L == L) cb.active = false;
    }
    void _gl_disconnect(int ref) {
        for (auto& cb : died_cbs)           if (cb.ref == ref) cb.active = false;
        for (auto& cb : health_changed_cbs) if (cb.ref == ref) cb.active = false;
        for (auto& cb : state_changed_cbs)  if (cb.ref == ref) cb.active = false;
    }

    static void fire_lua_died(std::vector<LuaCallback>& cbs) {
        for (int i = (int)cbs.size() - 1; i >= 0; i--) {
            auto& cb = cbs[i];
            if (!cb.active || !gl_state_alive(cb.main_L)) { cbs.erase(cbs.begin() + i); continue; }
            lua_State* thread = lua_newthread(cb.main_L);
            lua_rawgeti(cb.main_L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.main_L, -1)) {
                lua_xmove(cb.main_L, thread, 1);
                gl_check_resume(thread, lua_resume(thread, nullptr, 0));
            } else { lua_pop(cb.main_L, 1); }
            lua_pop(cb.main_L, 1);
        }
    }
    static void fire_lua_state_changed(std::vector<LuaCallback>& cbs, int new_state, int old_state) {
        for (int i = (int)cbs.size() - 1; i >= 0; i--) {
            auto& cb = cbs[i];
            if (!cb.active || !gl_state_alive(cb.main_L)) { cbs.erase(cbs.begin() + i); continue; }
            lua_State* thread = lua_newthread(cb.main_L);
            lua_rawgeti(cb.main_L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.main_L, -1)) {
                lua_xmove(cb.main_L, thread, 1);
                lua_pushnumber(thread, new_state);
                lua_pushnumber(thread, old_state);
                gl_check_resume(thread, lua_resume(thread, nullptr, 2));
            } else { lua_pop(cb.main_L, 1); }
            lua_pop(cb.main_L, 1);
        }
    }

    static void fire_lua_health_changed(std::vector<LuaCallback>& cbs, float new_hp, float old_hp) {
        for (int i = (int)cbs.size() - 1; i >= 0; i--) {
            auto& cb = cbs[i];
            if (!cb.active || !gl_state_alive(cb.main_L)) { cbs.erase(cbs.begin() + i); continue; }
            lua_State* thread = lua_newthread(cb.main_L);
            lua_rawgeti(cb.main_L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.main_L, -1)) {
                lua_xmove(cb.main_L, thread, 1);
                lua_pushnumber(thread, new_hp);
                lua_pushnumber(thread, old_hp);
                gl_check_resume(thread, lua_resume(thread, nullptr, 2));
            } else { lua_pop(cb.main_L, 1); }
            lua_pop(cb.main_L, 1);
        }
    }

private:
    float  health                 = 100.0f;
    float  max_health             = 100.0f;
    float  walk_speed             = 16.0f;
    float  jump_power             = 20.0f;
    float  jump_height            = 7.2f;
    float  hip_height             = 0.0f;
    float  gravity                = 45.0f;
    bool   is_dead                = false;
    bool   auto_rotate            = true;
    // Suavidad del giro del cuerpo hacia la direccion de movimiento (AutoRotate).
    // Mayor = gira mas rapido; menor = mas suave. Independiente de los FPS.
    // v1.8.3: bajado de 9 a 5 (giro claramente mas suave, no brusco).
    float  turn_speed             = 11.0f;   // giro rapido hacia el movimiento, como Roblox
    bool   auto_jump_enabled      = true;
    float  name_display_distance  = 100.0f;
    float  health_display_distance= 100.0f;
    // 0 = R6, 1 = R15
    int    rig_type               = 1;
    bool   evaluate_state_machine = true;
    bool   break_joints_on_death  = true;
    bool   requires_neck          = true;
    String display_name;

    // ── HumanoidStateType (mirror of Enum.HumanoidStateType in Luau) ─
    //// ── HumanoidStateType (espejo de Enum.HumanoidStateType en Luau) ─
    // 0=None 1=Running 2=Jumping 3=Landed 4=Swimming 5=Climbing 6=Seated
    // 7=Dead 8=Freefall 9=Flying 10=GettingUp 11=FallingDown 12=Ragdoll
    // 13=StrafingNoWalk 14=Frozen
    int     current_state = 1;
    int     forced_state  = -1;   // -1 = automatic / automático
    bool    was_on_floor  = true;
    float   landed_timer  = 0.0f;
    bool    body_configured = false;   // config del CharacterBody3D una sola vez
    Vector3 move_direction;
    Vector3 npc_move_dir;         // Non-zero = NPC/scripted movement, overrides player input

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("_gl_disconnect", "ref"), &Humanoid::_gl_disconnect);
        ClassDB::bind_method(D_METHOD("set_health", "h"),       &Humanoid::set_health);
        ClassDB::bind_method(D_METHOD("get_health"),             &Humanoid::get_health);
        ClassDB::bind_method(D_METHOD("set_max_health", "h"),   &Humanoid::set_max_health);
        ClassDB::bind_method(D_METHOD("get_max_health"),         &Humanoid::get_max_health);
        ClassDB::bind_method(D_METHOD("set_walk_speed", "s"),   &Humanoid::set_walk_speed);
        ClassDB::bind_method(D_METHOD("get_walk_speed"),         &Humanoid::get_walk_speed);
        ClassDB::bind_method(D_METHOD("set_jump_power", "p"),   &Humanoid::set_jump_power);
        ClassDB::bind_method(D_METHOD("get_jump_power"),         &Humanoid::get_jump_power);
        ClassDB::bind_method(D_METHOD("set_gravity_val", "g"),  &Humanoid::set_gravity_val);
        ClassDB::bind_method(D_METHOD("get_gravity_val"),        &Humanoid::get_gravity_val);
        ClassDB::bind_method(D_METHOD("take_damage", "amount"), &Humanoid::take_damage);
        ClassDB::bind_method(D_METHOD("heal", "amount"),         &Humanoid::heal);

        ADD_GROUP("Salud", "");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Health",
                                  PROPERTY_HINT_RANGE, "0,1000,0.1"),
                     "set_health", "get_health");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "MaxHealth",
                                  PROPERTY_HINT_RANGE, "1,1000,1"),
                     "set_max_health", "get_max_health");

        ClassDB::bind_method(D_METHOD("set_jump_height","h"),  &Humanoid::set_jump_height);
        ClassDB::bind_method(D_METHOD("get_jump_height"),      &Humanoid::get_jump_height);
        ClassDB::bind_method(D_METHOD("set_hip_height","h"),   &Humanoid::set_hip_height);
        ClassDB::bind_method(D_METHOD("get_hip_height"),       &Humanoid::get_hip_height);

        ADD_GROUP("Movimiento", "");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "WalkSpeed",
                                  PROPERTY_HINT_RANGE, "0,100,0.1"),
                     "set_walk_speed", "get_walk_speed");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "JumpPower",
                                  PROPERTY_HINT_RANGE, "0,200,0.1"),
                     "set_jump_power", "get_jump_power");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "JumpHeight",
                                  PROPERTY_HINT_RANGE, "0,100,0.1"),
                     "set_jump_height", "get_jump_height");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "HipHeight",
                                  PROPERTY_HINT_RANGE, "0,10,0.01"),
                     "set_hip_height", "get_hip_height");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Gravity",
                                  PROPERTY_HINT_RANGE, "0,200,0.1"),
                     "set_gravity_val", "get_gravity_val");

        ADD_GROUP("Personaje", "");
        ClassDB::bind_method(D_METHOD("set_auto_rotate","b"),              &Humanoid::set_auto_rotate);
        ClassDB::bind_method(D_METHOD("get_auto_rotate"),                  &Humanoid::get_auto_rotate);
        ClassDB::bind_method(D_METHOD("set_auto_jump_enabled","b"),        &Humanoid::set_auto_jump_enabled);
        ClassDB::bind_method(D_METHOD("get_auto_jump_enabled"),            &Humanoid::get_auto_jump_enabled);
        ClassDB::bind_method(D_METHOD("set_display_name","n"),             &Humanoid::set_display_name);
        ClassDB::bind_method(D_METHOD("get_display_name"),                 &Humanoid::get_display_name);
        ClassDB::bind_method(D_METHOD("set_name_display_distance","d"),    &Humanoid::set_name_display_distance);
        ClassDB::bind_method(D_METHOD("get_name_display_distance"),        &Humanoid::get_name_display_distance);
        ClassDB::bind_method(D_METHOD("set_health_display_distance","d"),  &Humanoid::set_health_display_distance);
        ClassDB::bind_method(D_METHOD("get_health_display_distance"),      &Humanoid::get_health_display_distance);
        ClassDB::bind_method(D_METHOD("set_rig_type","t"),                 &Humanoid::set_rig_type);
        ClassDB::bind_method(D_METHOD("get_rig_type"),                     &Humanoid::get_rig_type);
        ClassDB::bind_method(D_METHOD("set_evaluate_state_machine","b"),   &Humanoid::set_evaluate_state_machine);
        ClassDB::bind_method(D_METHOD("get_evaluate_state_machine"),       &Humanoid::get_evaluate_state_machine);
        ClassDB::bind_method(D_METHOD("set_break_joints_on_death","b"),    &Humanoid::set_break_joints_on_death);
        ClassDB::bind_method(D_METHOD("get_break_joints_on_death"),        &Humanoid::get_break_joints_on_death);
        ClassDB::bind_method(D_METHOD("set_requires_neck","b"),            &Humanoid::set_requires_neck);
        ClassDB::bind_method(D_METHOD("get_requires_neck"),                &Humanoid::get_requires_neck);

        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "AutoRotate"),            "set_auto_rotate","get_auto_rotate");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "AutoJumpEnabled"),       "set_auto_jump_enabled","get_auto_jump_enabled");
        ADD_PROPERTY(PropertyInfo(Variant::STRING,"DisplayName"),           "set_display_name","get_display_name");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "NameDisplayDistance",    PROPERTY_HINT_RANGE,"0,1000,1"), "set_name_display_distance","get_name_display_distance");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "HealthDisplayDistance",  PROPERTY_HINT_RANGE,"0,1000,1"), "set_health_display_distance","get_health_display_distance");
        ADD_PROPERTY(PropertyInfo(Variant::INT,   "RigType",                PROPERTY_HINT_ENUM,"R6:0,R15:1"),"set_rig_type","get_rig_type");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "EvaluateStateMachine"),  "set_evaluate_state_machine","get_evaluate_state_machine");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "BreakJointsOnDeath"),    "set_break_joints_on_death","get_break_joints_on_death");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "RequiresNeck"),          "set_requires_neck","get_requires_neck");

        ClassDB::bind_method(D_METHOD("get_state"),              &Humanoid::get_state);
        ClassDB::bind_method(D_METHOD("change_state","s"),       &Humanoid::change_state);
        ClassDB::bind_method(D_METHOD("get_move_direction"),     &Humanoid::get_move_direction);
        ClassDB::bind_method(D_METHOD("set_npc_move_dir","dir"), &Humanoid::set_npc_move_dir);
        ClassDB::bind_method(D_METHOD("get_npc_move_dir"),       &Humanoid::get_npc_move_dir);
        ClassDB::bind_method(D_METHOD("stop_npc_movement"),      &Humanoid::stop_npc_movement);

        // Roblox-style signals
        //// Señales tipo Roblox
        ADD_SIGNAL(MethodInfo("Died"));
        ADD_SIGNAL(MethodInfo("HealthChanged",
            PropertyInfo(Variant::FLOAT, "new_health"),
            PropertyInfo(Variant::FLOAT, "old_health")));
        ADD_SIGNAL(MethodInfo("StateChanged",
            PropertyInfo(Variant::INT, "new_state"),
            PropertyInfo(Variant::INT, "old_state")));
    }

public:
    // ── Health ─────────────────────────────────────────────────────
    //// ── Salud ──────────────────────────────────────────────────────
    void set_health(float h) {
        if (is_dead) return;
        float old = health;
        health = Math::clamp(h, 0.0f, max_health);

        if (old != health) {
            emit_signal("HealthChanged", health, old);
            fire_lua_health_changed(health_changed_cbs, health, old);
        }

        if (health <= 0.0f) {
            is_dead = true;
            _set_state_internal(7); // Dead
            emit_signal("Died");
            fire_lua_died(died_cbs);
        }
    }
    float get_health() const { return health; }

    void set_max_health(float h) {
        max_health = Math::max(h, 1.0f);
        health = Math::min(health, max_health);
    }
    float get_max_health() const { return max_health; }

    // Equivalent to Roblox Humanoid:TakeDamage()
    //// Equivalente a Humanoid:TakeDamage() de Roblox
    void take_damage(float amount) {
        set_health(health - amount);
    }

    // Restore health
    //// Curar salud
    void heal(float amount) {
        if (is_dead) return;
        set_health(health + amount);
    }

    bool is_character_dead() const { return is_dead; }

    // ── Humanoid State ─────────────────────────────────────────────
    //// ── Estado del Humanoid ────────────────────────────────────────
    int     get_state() const          { return current_state; }
    Vector3 get_move_direction() const { return move_direction; }
    void    set_npc_move_dir(Vector3 dir) { npc_move_dir = dir; }
    Vector3 get_npc_move_dir() const      { return npc_move_dir; }
    void    stop_npc_movement()           { npc_move_dir = Vector3(); }

    void change_state(int s) {
        if (s < 0 || s > 14) return;
        // Physics states (Running, Jumping, Freefall, Landed) remain automatic
        // unless the user forces them explicitly
        //// Los estados de física (Running, Jumping, Freefall, Landed) siguen siendo automáticos
        //// excepto si el usuario los fuerza explícitamente
        forced_state = (s == 1 || s == 2 || s == 3 || s == 8) ? -1 : s;
        _set_state_internal(s);
    }

    // ── Movement ───────────────────────────────────────────────────
    //// ── Movimiento ─────────────────────────────────────────────────
    void  set_walk_speed(float s)   { walk_speed = Math::max(s, 0.0f); }
    float get_walk_speed() const    { return walk_speed; }
    void  set_jump_power(float p)   { jump_power = Math::max(p, 0.0f); }
    float get_jump_power() const    { return jump_power; }
    void  set_jump_height(float h)  { jump_height = Math::max(h, 0.0f); }
    float get_jump_height() const   { return jump_height; }
    void  set_hip_height(float h)   { hip_height = Math::max(h, 0.0f); }
    float get_hip_height() const    { return hip_height; }
    void  set_gravity_val(float g)  { gravity = Math::max(g, 0.0f); }
    float get_gravity_val() const   { return gravity; }

    // ── Character ──────────────────────────────────────────────────
    //// ── Personaje ──────────────────────────────────────────────────
    void   set_auto_rotate(bool b)              { auto_rotate = b; }
    bool   get_auto_rotate() const              { return auto_rotate; }
    void   set_auto_jump_enabled(bool b)        { auto_jump_enabled = b; }
    bool   get_auto_jump_enabled() const        { return auto_jump_enabled; }
    void   set_display_name(String n)           { display_name = n; }
    String get_display_name() const             { return display_name; }
    void   set_name_display_distance(float d)   { name_display_distance = Math::max(d, 0.0f); }
    float  get_name_display_distance() const    { return name_display_distance; }
    void   set_health_display_distance(float d) { health_display_distance = Math::max(d, 0.0f); }
    float  get_health_display_distance() const  { return health_display_distance; }
    void   set_rig_type(int t)                  { rig_type = Math::clamp(t, 0, 1); }
    int    get_rig_type() const                 { return rig_type; }
    void   set_evaluate_state_machine(bool b)   { evaluate_state_machine = b; }
    bool   get_evaluate_state_machine() const   { return evaluate_state_machine; }
    void   set_break_joints_on_death(bool b)    { break_joints_on_death = b; }
    bool   get_break_joints_on_death() const    { return break_joints_on_death; }
    void   set_requires_neck(bool b)            { requires_neck = b; }
    bool   get_requires_neck() const            { return requires_neck; }

    void _set_state_internal(int new_state) {
        if (current_state == new_state) return;
        int old = current_state;
        current_state = new_state;
        emit_signal("StateChanged", new_state, old);
        fire_lua_state_changed(state_changed_cbs, new_state, old);
    }

    // ── _physics_process: all movement logic ───────────────────────
    //// ── _physics_process: toda la lógica de movimiento ─────────────
    void _physics_process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint() || is_dead) return;

        CharacterBody3D* body = Object::cast_to<CharacterBody3D>(get_parent());
        if (!body) return;

        // Config del cuerpo una sola vez: pegado al suelo (no rebota en rampas/
        // escalones) y angulo de suelo amplio, como un personaje de Roblox.
        if (!body_configured) {
            body->set_up_direction(Vector3(0, 1, 0));
            body->set_floor_snap_length(0.5f);
            body->set_floor_stop_on_slope_enabled(true);
            body->set_floor_max_angle((float)Math::deg_to_rad(50.0));
            body_configured = true;
        }

        Input* input = Input::get_singleton();
        Vector3 velocity = body->get_velocity();

        // ── 1. Gravity ─────────────────────────────────────────────
        //// ── 1. Gravedad ────────────────────────────────────────────
        if (!body->is_on_floor()) {
            velocity.y -= gravity * (float)delta;
        }

        // ── 2. Jump (teclado + gamepad A + boton tactil) ───────────
        //// ── 2. Salto (teclado + gamepad A + botón táctil) ──────────
        // Vista de Servidor activa: la cámara libre se queda con el input
        // (WASD/salto no mueven al personaje, como en Roblox Studio).
        bool freecam = gl_freecam().active;
        bool jump_pressed = !freecam && (input->is_key_pressed(KEY_SPACE)
            || input->is_joy_button_pressed(0, JOY_BUTTON_A)
            || (gl_mobile().active && gl_mobile().jump));
        if (jump_pressed && body->is_on_floor()) {
            velocity.y = jump_power;
        }

        // ── 3. Direction based on active camera ────────────────────
        //// ── 3. Dirección según la cámara activa ───────────────────
        Camera3D* cam = get_viewport()->get_camera_3d();
        if (cam) {
            Basis cam_basis = cam->get_global_transform().basis;
            // forward: camera -Z axis, projected onto the ground
            //// forward: eje -Z de la cámara, proyectado al suelo
            Vector3 forward = -cam_basis.get_column(2);
            Vector3 right   =  cam_basis.get_column(0);
            forward.y = 0.0f;
            right.y   = 0.0f;
            forward   = forward.normalized();
            right     = right.normalized();

            // Input de movimiento UNIFICADO cross-device: teclado, gamepad y
            // joystick tactil alimentan el mismo vector (como el moveVector del
            // ControlModule de Roblox). Asi el personaje se mueve en PC, movil
            // y consola sin cambiar nada.
            float input_forward = 0.0f;
            float input_right   = 0.0f;
            if (!freecam) {
                // Teclado (WASD + flechas)
                if (input->is_key_pressed(KEY_W) || input->is_key_pressed(KEY_UP))    input_forward += 1.0f;
                if (input->is_key_pressed(KEY_S) || input->is_key_pressed(KEY_DOWN))  input_forward -= 1.0f;
                if (input->is_key_pressed(KEY_D) || input->is_key_pressed(KEY_RIGHT)) input_right   += 1.0f;
                if (input->is_key_pressed(KEY_A) || input->is_key_pressed(KEY_LEFT))  input_right   -= 1.0f;
                // Gamepad: stick izquierdo (zona muerta para evitar deriva)
                {
                    Vector2 ls(input->get_joy_axis(0, JOY_AXIS_LEFT_X),
                               input->get_joy_axis(0, JOY_AXIS_LEFT_Y));
                    if (ls.length() > 0.2f) { input_right += ls.x; input_forward -= ls.y; }
                }
                // Tactil: joystick virtual en pantalla (lo escribe el RobloxPlayer)
                if (gl_mobile().active) {
                    Vector2 tv = gl_mobile().move;
                    if (tv.length() > 0.05f) { input_right += tv.x; input_forward += tv.y; }
                }
            }
            input_forward = Math::clamp(input_forward, -1.0f, 1.0f);
            input_right   = Math::clamp(input_right,   -1.0f, 1.0f);

            Vector3 move_dir = forward * input_forward + right * input_right;

            // NPC/scripted movement overrides player input when set
            if (npc_move_dir.length_squared() > 0.01f) {
                move_dir = npc_move_dir.normalized();
                move_dir.y = 0;
            }

            // Velocidad horizontal OBJETIVO (0 si no hay input).
            Vector3 target_h(0.0f, 0.0f, 0.0f);
            if (move_dir.length_squared() > 0.01f) {
                move_dir = move_dir.normalized();
                // Sprint: Shift (teclado) o click del stick izquierdo (gamepad)
                bool sprint = input->is_key_pressed(KEY_SHIFT)
                    || input->is_joy_button_pressed(0, JOY_BUTTON_LEFT_STICK);
                float speed_mult = sprint ? 1.5f : 1.0f;
                target_h = move_dir * (walk_speed * speed_mult);

                // AutoRotate: girar el cuerpo hacia la direccion del movimiento,
                // como Roblox. Suavizado exponencial: independiente de los FPS y
                // SIN sobre-girar (el factor nunca pasa de 1). Antes usaba
                // 16*delta, que con FPS bajos podia pasar de 1 y "latigar".
                if (auto_rotate) {
                    float target_angle = (float)Math::atan2((double)-move_dir.x, (double)-move_dir.z);
                    Vector3 cur_rot = body->get_rotation();
                    float t = 1.0f - Math::exp(-turn_speed * (float)delta);
                    cur_rot.y = (float)Math::lerp_angle(
                        (double)cur_rot.y, (double)target_angle, (double)t);
                    body->set_rotation(cur_rot);
                }
            }

            // Aceleracion/desaceleracion suave hacia la velocidad objetivo, estilo
            // Roblox: arranque y frenada rapidos pero sin tirones, y DELTA-correcto
            // (antes la frenada dependia de los FPS). Menos control en el aire.
            float accel = body->is_on_floor() ? 110.0f : 45.0f;
            velocity.x = (float)Math::move_toward((double)velocity.x, (double)target_h.x, (double)(accel * delta));
            velocity.z = (float)Math::move_toward((double)velocity.z, (double)target_h.z, (double)(accel * delta));
        }

        body->set_velocity(velocity);
        body->move_and_slide();

        // ── 4. Detect collisions with RobloxParts (Touched event) ──
        //// ── 4. Detectar colisiones con RobloxParts (evento Touched) ─
        for (int i = 0; i < body->get_slide_collision_count(); i++) {
            Ref<KinematicCollision3D> col = body->get_slide_collision(i);
            if (col.is_valid()) {
                Node* collider = Object::cast_to<Node>(col->get_collider());
                if (collider && collider->has_signal("Touched")) {
                    // Se reporta una PARTE del personaje, no el personaje: así
                    // `hit.Parent:FindFirstChild("Humanoid")` funciona (1.14.11).
                    collider->emit_signal("Touched", gl_touch_reported_node(body));
                }
            }
        }

        // ── 5. State machine (HumanoidStateType) ───────────────────
        //// ── 5. Máquina de estados (HumanoidStateType) ─────────────
        if (evaluate_state_machine) {
            bool on_floor = body->is_on_floor();
            Vector3 horiz = Vector3(velocity.x, 0.0f, velocity.z);
            move_direction = horiz.length_squared() > 0.01f ? horiz.normalized() : Vector3();

            if (forced_state >= 0) {
                _set_state_internal(forced_state);
            } else if (on_floor) {
                if (!was_on_floor && (current_state == 8 || current_state == 2)) {
                    // Just landed
                    //// Acaba de aterrizar
                    _set_state_internal(3); // Landed
                    landed_timer = 0.12f;
                } else if (landed_timer > 0.0f) {
                    landed_timer -= (float)delta;
                    if (landed_timer <= 0.0f) _set_state_internal(1); // Running
                } else {
                    _set_state_internal(1); // Running (también cuando está quieto en suelo)
                }
            } else {
                // En el aire
                if (velocity.y > 0.1f) _set_state_internal(2); // Jumping
                else                    _set_state_internal(8); // Freefall
            }
            was_on_floor = on_floor;
        }

        // Animacion de PRUEBA por codigo (placeholder): SOLO si el toggle de
        // Debug 'debug_test_animation' esta activo. Anima la malla visual, no
        // la fisica; sirve para ver el muñeco "vivo" sin animaciones reales aun.
        _drive_animation(body, velocity, delta);
    }

    // ── Animacion del personaje: R6Animator (avatar por partes) o fallback ──
    void _drive_animation(CharacterBody3D* body, const Vector3& velocity, double delta) {
        // Desde 1.14.5.3 el personaje está APLANADO: el nodo "Character" del rig
        // desaparece y el animador queda como hijo DIRECTO. Buscar solo en
        // "Character/R6Animator" dejó las animaciones R6 del jugador local
        // MUERTAS (caían al fallback) sin que nada avisara. Se mira primero el
        // camino aplanado y se conserva el viejo por compatibilidad.
        Node* anim = body ? body->get_node_or_null(NodePath("R6Animator")) : nullptr;
        if (!anim) {
            Node* character = body ? body->get_node_or_null(NodePath("Character")) : nullptr;
            anim = character ? character->get_node_or_null(NodePath("R6Animator")) : nullptr;
        }
        if (anim) {
            Vector3 horiz(velocity.x, 0.0f, velocity.z);
            double speed01 = Math::clamp((double)horiz.length() / (double)Math::max(walk_speed, 0.1f), 0.0, 1.0);
            anim->call("set_move_state", speed01, !body->is_on_floor());
            return;
        }
        _test_animate(body, velocity, delta);
    }

    // ── Animacion de prueba: como el avatar es una malla unica sin huesos,
    //    animamos su transform (rebote al estar quieto/caminar + giro hacia
    //    la direccion de movimiento). Se restaura sola al apagar el toggle.
    float anim_time     = 0.0f;
    float anim_base_y   = 0.0f;
    bool  anim_base_set = false;

    void _test_animate(CharacterBody3D* body, const Vector3& velocity, double delta) {
        Node3D* mesh = body ? Object::cast_to<Node3D>(body->get_node_or_null(NodePath("Character"))) : nullptr;
        if (!mesh) return;
        bool on = (bool)ProjectSettings::get_singleton()->get_setting("godot_luau/debug_test_animation", false);
        if (!on) {
            if (anim_base_set) {
                Vector3 p = mesh->get_position(); p.y = anim_base_y;
                mesh->set_position(p); mesh->set_rotation(Vector3());
                anim_base_set = false;
            }
            return;
        }
        if (!anim_base_set) { anim_base_y = mesh->get_position().y; anim_base_set = true; }
        anim_time += (float)delta;
        Vector3 horiz(velocity.x, 0.0f, velocity.z);
        bool moving = horiz.length() > 0.5f;
        float freq = moving ? 9.0f : 2.2f;
        float amp  = moving ? 0.12f : 0.04f;
        Vector3 p = mesh->get_position();
        p.y = anim_base_y + Math::abs(Math::sin(anim_time * freq)) * amp;
        mesh->set_position(p);
        float yaw  = moving ? Math::atan2(velocity.x, velocity.z) : mesh->get_rotation().y;
        float sway = moving ? Math::sin(anim_time * freq) * 0.08f : 0.0f;
        mesh->set_rotation(Vector3(0.0f, yaw, sway));
    }
};

#endif // HUMANOID_H
