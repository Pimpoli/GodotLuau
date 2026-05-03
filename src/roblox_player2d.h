#ifndef ROBLOX_PLAYER2D_H
#define ROBLOX_PLAYER2D_H

#include <godot_cpp/classes/character_body2d.hpp>
#include <godot_cpp/classes/camera2d.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>
#include <godot_cpp/classes/capsule_shape2d.hpp>
#include <godot_cpp/classes/rectangle_shape2d.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/math.hpp>

using namespace godot;

// ════════════════════════════════════════════════════════════════════
//  RobloxPlayer2D — 2D player character
//
//  MOVEMENT TYPE (MovementType property):
//    0 = Platformer — With gravity physics, the character can fall and jump
//    1 = TopDown    — No gravity, moves in 8 directions (RPG/Zelda style)
//
//  2D CAMERA MODES (CameraMode property):
//    1 = Fixed    — Camera follows the player instantly
//    2 = Smooth   — Camera follows with a small delay
//    3 = Combined — Delay while moving, centers when idle
////
//  RobloxPlayer2D — Personaje del jugador para juegos 2D
//
//  TIPO DE MOVIMIENTO (propiedad MovementType):
//    0 = Platformer — Con física de gravedad, el personaje puede caer y saltar
//    1 = TopDown    — Sin gravedad, se mueve en 8 direcciones (RPG/Zelda style)
//
//  MODOS DE CÁMARA 2D (propiedad CameraMode):
//    1 = Fija       — La cámara sigue al jugador al instante
//    2 = Suave      — La cámara sigue con un pequeño retraso
//    3 = Combinada  — Retraso al moverse, centra al detenerse
// ════════════════════════════════════════════════════════════════════
class RobloxPlayer2D : public CharacterBody2D {
    GDCLASS(RobloxPlayer2D, CharacterBody2D);

private:
    Camera2D* camera = nullptr;

    // 0 = Platformer, 1 = TopDown
    int movement_type = 0;

    // 1 = Fixed, 2 = Smooth, 3 = Combined / Fija, Suave, Combinada
    int camera_mode = 1;

    // Movement speeds / Velocidades del movimiento
    float walk_speed  = 200.0f;
    float run_speed   = 300.0f;
    float jump_power  = -500.0f;  // Negative because in 2D Y grows downward / Negativo porque en 2D Y crece hacia abajo
    float gravity     = 980.0f;

    Vector2 last_position;
    float   idle_time = 0.0f;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_movement_type", "t"), &RobloxPlayer2D::set_movement_type);
        ClassDB::bind_method(D_METHOD("get_movement_type"),       &RobloxPlayer2D::get_movement_type);
        ClassDB::bind_method(D_METHOD("set_camera_mode", "m"),   &RobloxPlayer2D::set_camera_mode);
        ClassDB::bind_method(D_METHOD("get_camera_mode"),         &RobloxPlayer2D::get_camera_mode);
        ClassDB::bind_method(D_METHOD("set_walk_speed", "s"),    &RobloxPlayer2D::set_walk_speed);
        ClassDB::bind_method(D_METHOD("get_walk_speed"),          &RobloxPlayer2D::get_walk_speed);
        ClassDB::bind_method(D_METHOD("set_jump_power", "p"),    &RobloxPlayer2D::set_jump_power);
        ClassDB::bind_method(D_METHOD("get_jump_power"),          &RobloxPlayer2D::get_jump_power);
        ClassDB::bind_method(D_METHOD("set_gravity", "g"),       &RobloxPlayer2D::set_gravity_value);
        ClassDB::bind_method(D_METHOD("get_gravity"),             &RobloxPlayer2D::get_gravity_value);

        ADD_GROUP("Movimiento 2D", "");
        ADD_PROPERTY(
            PropertyInfo(Variant::INT, "MovementType",
                         PROPERTY_HINT_ENUM, "0:Platformer (con gravedad),1:TopDown (sin gravedad)"),
            "set_movement_type", "get_movement_type");
        ADD_PROPERTY(
            PropertyInfo(Variant::FLOAT, "WalkSpeed", PROPERTY_HINT_RANGE, "10,1000,1"),
            "set_walk_speed", "get_walk_speed");
        ADD_PROPERTY(
            PropertyInfo(Variant::FLOAT, "JumpPower", PROPERTY_HINT_RANGE, "-2000,-50,10"),
            "set_jump_power", "get_jump_power");
        ADD_PROPERTY(
            PropertyInfo(Variant::FLOAT, "Gravity2D", PROPERTY_HINT_RANGE, "0,3000,10"),
            "set_gravity", "get_gravity");

        ADD_GROUP("Camara 2D", "");
        ADD_PROPERTY(
            PropertyInfo(Variant::INT, "CameraMode",
                         PROPERTY_HINT_ENUM,
                         "1:Fija (instantanea),2:Suave (con retraso),3:Combinada (retraso y centra)"),
            "set_camera_mode", "get_camera_mode");
    }

public:
    // ── Getters / Setters ──────────────────────────────────────────
    void set_movement_type(int t) { movement_type = Math::clamp(t, 0, 1); }
    int  get_movement_type() const { return movement_type; }
    void set_camera_mode(int m) {
        camera_mode = Math::clamp(m, 1, 3);
        apply_camera_mode();
    }
    int  get_camera_mode() const { return camera_mode; }
    void set_walk_speed(float s) { walk_speed = s; }
    float get_walk_speed() const { return walk_speed; }
    void set_jump_power(float p) { jump_power = p; }
    float get_jump_power() const { return jump_power; }
    void set_gravity_value(float g) { gravity = g; }
    float get_gravity_value() const { return gravity; }

    void apply_camera_mode() {
        if (!camera) return;
        switch (camera_mode) {
            case 1:
                // Fixed: no smoothing / Fija: sin suavizado
                camera->set_position_smoothing_enabled(false);
                break;
            case 2:
                // Smooth: moderate smoothing / Suave: suavizado moderado
                camera->set_position_smoothing_enabled(true);
                camera->set_position_smoothing_speed(8.0f);
                break;
            case 3:
                // Combined: soft smoothing (complemented by logic in _process) / Combinada: suavizado suave (se complementa con la lógica en _process)
                camera->set_position_smoothing_enabled(true);
                camera->set_position_smoothing_speed(4.0f);
                break;
        }
    }

    // ── _ready ─────────────────────────────────────────────────────
    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;

        last_position = get_global_position();

        // Character body: simple rectangle / Cuerpo del personaje: rectángulo simple
        CollisionShape2D* col = memnew(CollisionShape2D);
        Ref<CapsuleShape2D> shape; shape.instantiate();
        shape->set_radius(16.0f);
        shape->set_height(32.0f);
        col->set_shape(shape);
        add_child(col);

        // Visual representation: colored rectangle / Representación visual: rectángulo de color
        ColorRect* body_rect = memnew(ColorRect);
        body_rect->set_name("BodyRect");
        body_rect->set_size(Vector2(32.0f, 64.0f));
        body_rect->set_position(Vector2(-16.0f, -64.0f));
        body_rect->set_color(Color(0.24f, 0.49f, 0.83f)); // Roblox blue / Azul Roblox
        add_child(body_rect);

        // 2D camera / Cámara 2D
        camera = memnew(Camera2D);
        camera->set_name("Camera2D");
        camera->set_enabled(true);
        add_child(camera);

        apply_camera_mode();
    }

    // ── _physics_process: movement ────────────────────────────────
    //// ── _physics_process: movimiento ──────────────────────────────
    void _physics_process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint()) return;

        Vector2 vel = get_velocity();
        Input* inp = Input::get_singleton();

        if (movement_type == 0) {
            // ── PLATFORMER ──────────────────────────────────────
            // Gravity / Gravedad
            if (!is_on_floor()) {
                vel.y += gravity * (float)delta;
            }

            // Jump / Salto
            if (inp->is_key_pressed(KEY_SPACE) && is_on_floor()) {
                vel.y = jump_power;
            }
            // Also accept W and up arrow to jump / También aceptar W y flecha arriba para saltar
            if ((inp->is_key_pressed(KEY_W) || inp->is_key_pressed(KEY_UP)) && is_on_floor()) {
                vel.y = jump_power;
            }

            // Horizontal movement / Movimiento horizontal
            float h = 0.0f;
            if (inp->is_key_pressed(KEY_D) || inp->is_key_pressed(KEY_RIGHT)) h += 1.0f;
            if (inp->is_key_pressed(KEY_A) || inp->is_key_pressed(KEY_LEFT))  h -= 1.0f;

            bool running = inp->is_key_pressed(KEY_SHIFT);
            float speed = running ? run_speed : walk_speed;

            if (Math::abs(h) > 0.0f) {
                vel.x = h * speed;
            } else {
                // Smooth deceleration / Desaceleración suave
                vel.x = (float)Math::move_toward((double)vel.x, 0.0, (double)speed * 0.3);
            }

        } else {
            // ── TOPDOWN ─────────────────────────────────────────
            float h = 0.0f, v = 0.0f;
            if (inp->is_key_pressed(KEY_D) || inp->is_key_pressed(KEY_RIGHT)) h += 1.0f;
            if (inp->is_key_pressed(KEY_A) || inp->is_key_pressed(KEY_LEFT))  h -= 1.0f;
            if (inp->is_key_pressed(KEY_S) || inp->is_key_pressed(KEY_DOWN))  v += 1.0f;
            if (inp->is_key_pressed(KEY_W) || inp->is_key_pressed(KEY_UP))    v -= 1.0f;

            Vector2 dir = Vector2(h, v);
            if (dir.length_squared() > 0.01f) dir = dir.normalized();

            bool running = inp->is_key_pressed(KEY_SHIFT);
            float speed = running ? run_speed : walk_speed;

            if (dir.length_squared() > 0.01f) {
                vel = dir * speed;
            } else {
                vel = vel.lerp(Vector2(), (float)Math::min(15.0 * delta, 1.0));
            }
        }

        set_velocity(vel);
        move_and_slide();
    }

    // ── _process: camera_mode 3 adjustment ───────────────────────
    //// ── _process: ajuste del camera_mode 3 ────────────────────────
    void _process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint() || !camera) return;

        // In mode 3 we change the smoothing speed dynamically / En modo 3 cambiamos la velocidad de suavizado dinámicamente
        if (camera_mode == 3) {
            Vector2 cur = get_global_position();
            bool moving = (cur - last_position).length_squared() > 1.0f;
            last_position = cur;

            if (moving) idle_time = 0.0f;
            else        idle_time += (float)delta;

            // Slower while moving, faster to center when idle / Más lento al moverse, más rápido para centrar al detenerse
            float target_speed = (idle_time > 0.3f) ? 10.0f : 3.0f;
            float cur_speed = camera->get_position_smoothing_speed();
            camera->set_position_smoothing_speed(
                (float)Math::lerp((double)cur_speed, (double)target_speed, 5.0 * delta));
        }
    }
};

#endif // ROBLOX_PLAYER2D_H
