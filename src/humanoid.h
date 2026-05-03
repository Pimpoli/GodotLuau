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
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <vector>

#include "lua.h"
#include "lualib.h"

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

    static void fire_lua_died(std::vector<LuaCallback>& cbs) {
        for (int i = (int)cbs.size() - 1; i >= 0; i--) {
            auto& cb = cbs[i];
            if (!cb.active) { cbs.erase(cbs.begin() + i); continue; }
            lua_State* thread = lua_newthread(cb.main_L);
            lua_rawgeti(cb.main_L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.main_L, -1)) {
                lua_xmove(cb.main_L, thread, 1);
                int st = lua_resume(thread, nullptr, 0);
                if (st != LUA_OK && st != LUA_YIELD)
                    UtilityFunctions::print("[Humanoid.Died] Error: ", lua_tostring(thread, -1));
            } else { lua_pop(cb.main_L, 1); }
            lua_pop(cb.main_L, 1);
        }
    }
    static void fire_lua_state_changed(std::vector<LuaCallback>& cbs, int new_state, int old_state) {
        for (int i = (int)cbs.size() - 1; i >= 0; i--) {
            auto& cb = cbs[i];
            if (!cb.active) { cbs.erase(cbs.begin() + i); continue; }
            lua_State* thread = lua_newthread(cb.main_L);
            lua_rawgeti(cb.main_L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.main_L, -1)) {
                lua_xmove(cb.main_L, thread, 1);
                lua_pushnumber(thread, new_state);
                lua_pushnumber(thread, old_state);
                int st = lua_resume(thread, nullptr, 2);
                if (st != LUA_OK && st != LUA_YIELD)
                    UtilityFunctions::print("[Humanoid.StateChanged] Error: ", lua_tostring(thread, -1));
            } else { lua_pop(cb.main_L, 1); }
            lua_pop(cb.main_L, 1);
        }
    }

    static void fire_lua_health_changed(std::vector<LuaCallback>& cbs, float new_hp, float old_hp) {
        for (int i = (int)cbs.size() - 1; i >= 0; i--) {
            auto& cb = cbs[i];
            if (!cb.active) { cbs.erase(cbs.begin() + i); continue; }
            lua_State* thread = lua_newthread(cb.main_L);
            lua_rawgeti(cb.main_L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.main_L, -1)) {
                lua_xmove(cb.main_L, thread, 1);
                lua_pushnumber(thread, new_hp);
                lua_pushnumber(thread, old_hp);
                int st = lua_resume(thread, nullptr, 2);
                if (st != LUA_OK && st != LUA_YIELD)
                    UtilityFunctions::print("[Humanoid.HealthChanged] Error: ", lua_tostring(thread, -1));
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
    Vector3 move_direction;
    Vector3 npc_move_dir;         // Non-zero = NPC/scripted movement, overrides player input

protected:
    static void _bind_methods() {
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

        Input* input = Input::get_singleton();
        Vector3 velocity = body->get_velocity();

        // ── 1. Gravity ─────────────────────────────────────────────
        //// ── 1. Gravedad ────────────────────────────────────────────
        if (!body->is_on_floor()) {
            velocity.y -= gravity * (float)delta;
        }

        // ── 2. Jump ────────────────────────────────────────────────
        //// ── 2. Salto ───────────────────────────────────────────────
        if (input->is_key_pressed(KEY_SPACE) && body->is_on_floor()) {
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

            float input_forward = 0.0f;
            float input_right   = 0.0f;
            if (input->is_key_pressed(KEY_W)) input_forward += 1.0f;
            if (input->is_key_pressed(KEY_S)) input_forward -= 1.0f;
            if (input->is_key_pressed(KEY_D)) input_right   += 1.0f;
            if (input->is_key_pressed(KEY_A)) input_right   -= 1.0f;

            Vector3 move_dir = forward * input_forward + right * input_right;

            // NPC/scripted movement overrides player input when set
            if (npc_move_dir.length_squared() > 0.01f) {
                move_dir = npc_move_dir.normalized();
                move_dir.y = 0;
            }

            if (move_dir.length_squared() > 0.01f) {
                move_dir = move_dir.normalized();

                // Sprint with Shift
                //// Correr con Shift
                float speed_mult = input->is_key_pressed(KEY_SHIFT) ? 1.5f : 1.0f;
                velocity.x = move_dir.x * walk_speed * speed_mult;
                velocity.z = move_dir.z * walk_speed * speed_mult;

                // Rotate the body toward the movement direction
                //// Rotar el cuerpo hacia la dirección del movimiento
                if (auto_rotate) {
                    float target_angle = (float)Math::atan2((double)-move_dir.x, (double)-move_dir.z);
                    Vector3 cur_rot = body->get_rotation();
                    cur_rot.y = (float)Math::lerp_angle(
                        (double)cur_rot.y, (double)target_angle, 12.0 * delta);
                    body->set_rotation(cur_rot);
                }
            } else {
                // Smooth deceleration when keys are released
                //// Desaceleración suave al soltar las teclas
                velocity.x = (float)Math::move_toward(
                    (double)velocity.x, 0.0, (double)walk_speed * 0.25);
                velocity.z = (float)Math::move_toward(
                    (double)velocity.z, 0.0, (double)walk_speed * 0.25);
            }
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
                    collider->emit_signal("Touched", body);
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
    }
};

#endif // HUMANOID_H
