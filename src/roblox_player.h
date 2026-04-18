#ifndef ROBLOX_PLAYER_H
#define ROBLOX_PLAYER_H

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/spring_arm3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

// ════════════════════════════════════════════════════════════════════
//  RobloxPlayer — Personaje 3D del jugador, equivalente al Character de Roblox
//
//  MODOS DE CÁMARA (propiedad CameraMode):
//    1 = Fija       — La cámara sigue al jugador al instante (sin retraso)
//    2 = Suave      — La cámara sigue con un pequeño retraso elegante
//    3 = Combinada  — Retraso fuerte al moverse, se centra sola al detenerse
// ════════════════════════════════════════════════════════════════════
class RobloxPlayer : public CharacterBody3D {
    GDCLASS(RobloxPlayer, CharacterBody3D);

private:
    Node3D* pivot_h    = nullptr;  // Pivote horizontal (mouse X)
    Node3D* pivot_v    = nullptr;  // Pivote vertical (mouse Y)
    SpringArm3D* spring_arm = nullptr; // Evita que la cámara atraviese paredes
    Camera3D* camera     = nullptr;

    float mouse_sensitivity = 0.003f;
    float target_zoom       = 10.0f;

    // ── Modo de cámara ──────────────────────────────────────────────
    int camera_mode = 1;

    // Para el modo 3: detectar si el jugador se está moviendo
    Vector3 last_position;
    float   idle_time = 0.0f;

    // ── Propiedades de jugador tipo Roblox ─────────────────────────
    int    user_id                   = 0;
    String display_name;
    int    account_age               = 0;
    Color  team_color                = Color(1,1,1);
    bool   auto_jump_enabled         = true;
    // 0=Zoom, 1=Invisicam
    int    dev_camera_occlusion_mode = 0;
    // 0=KeyboardMouse, 1=ClickToMove, 2=Scriptable
    int    dev_computer_movement_mode= 0;
    // 0=Thumbstick, 1=DynamicThumbstick, 2=ClickToMove, 3=Scriptable
    int    dev_touch_movement_mode   = 0;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_camera_mode", "mode"), &RobloxPlayer::set_camera_mode);
        ClassDB::bind_method(D_METHOD("get_camera_mode"),         &RobloxPlayer::get_camera_mode);
        ClassDB::bind_method(D_METHOD("set_mouse_sensitivity", "s"), &RobloxPlayer::set_mouse_sensitivity);
        ClassDB::bind_method(D_METHOD("get_mouse_sensitivity"),      &RobloxPlayer::get_mouse_sensitivity);

        ADD_GROUP("Camara","");
        ADD_PROPERTY(PropertyInfo(Variant::INT,"CameraMode",PROPERTY_HINT_ENUM,
            "Fija:1,Suave:2,Combinada:3"),
            "set_camera_mode","get_camera_mode");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"MouseSensitivity",PROPERTY_HINT_RANGE,"0.0001,0.01,0.0001"),
            "set_mouse_sensitivity","get_mouse_sensitivity");

        // ── Jugador ─────────────────────────────────────────────────
        ADD_GROUP("Jugador","");
        ClassDB::bind_method(D_METHOD("set_user_id","id"),                    &RobloxPlayer::set_user_id);
        ClassDB::bind_method(D_METHOD("get_user_id"),                         &RobloxPlayer::get_user_id);
        ClassDB::bind_method(D_METHOD("set_display_name","n"),                &RobloxPlayer::set_display_name);
        ClassDB::bind_method(D_METHOD("get_display_name"),                    &RobloxPlayer::get_display_name);
        ClassDB::bind_method(D_METHOD("set_account_age","a"),                 &RobloxPlayer::set_account_age);
        ClassDB::bind_method(D_METHOD("get_account_age"),                     &RobloxPlayer::get_account_age);
        ClassDB::bind_method(D_METHOD("set_team_color","c"),                  &RobloxPlayer::set_team_color);
        ClassDB::bind_method(D_METHOD("get_team_color"),                      &RobloxPlayer::get_team_color);
        ClassDB::bind_method(D_METHOD("set_auto_jump_enabled","b"),           &RobloxPlayer::set_auto_jump_enabled);
        ClassDB::bind_method(D_METHOD("get_auto_jump_enabled"),               &RobloxPlayer::get_auto_jump_enabled);
        ClassDB::bind_method(D_METHOD("set_dev_camera_occlusion_mode","m"),   &RobloxPlayer::set_dev_camera_occlusion_mode);
        ClassDB::bind_method(D_METHOD("get_dev_camera_occlusion_mode"),       &RobloxPlayer::get_dev_camera_occlusion_mode);
        ClassDB::bind_method(D_METHOD("set_dev_computer_movement_mode","m"),  &RobloxPlayer::set_dev_computer_movement_mode);
        ClassDB::bind_method(D_METHOD("get_dev_computer_movement_mode"),      &RobloxPlayer::get_dev_computer_movement_mode);
        ClassDB::bind_method(D_METHOD("set_dev_touch_movement_mode","m"),     &RobloxPlayer::set_dev_touch_movement_mode);
        ClassDB::bind_method(D_METHOD("get_dev_touch_movement_mode"),         &RobloxPlayer::get_dev_touch_movement_mode);

        ADD_PROPERTY(PropertyInfo(Variant::INT,   "UserId"),               "set_user_id","get_user_id");
        ADD_PROPERTY(PropertyInfo(Variant::STRING,"DisplayName"),          "set_display_name","get_display_name");
        ADD_PROPERTY(PropertyInfo(Variant::INT,   "AccountAge"),           "set_account_age","get_account_age");
        ADD_PROPERTY(PropertyInfo(Variant::COLOR, "TeamColor"),            "set_team_color","get_team_color");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "AutoJumpEnabled"),      "set_auto_jump_enabled","get_auto_jump_enabled");
        ADD_PROPERTY(PropertyInfo(Variant::INT,"DevCameraOcclusionMode",PROPERTY_HINT_ENUM,"Zoom:0,Invisicam:1"),
            "set_dev_camera_occlusion_mode","get_dev_camera_occlusion_mode");
        ADD_PROPERTY(PropertyInfo(Variant::INT,"DevComputerMovementMode",PROPERTY_HINT_ENUM,
            "KeyboardMouse:0,ClickToMove:1,Scriptable:2"),
            "set_dev_computer_movement_mode","get_dev_computer_movement_mode");
        ADD_PROPERTY(PropertyInfo(Variant::INT,"DevTouchMovementMode",PROPERTY_HINT_ENUM,
            "Thumbstick:0,DynamicThumbstick:1,ClickToMove:2,Scriptable:3"),
            "set_dev_touch_movement_mode","get_dev_touch_movement_mode");
    }

public:
    // ── Camera / Mouse ─────────────────────────────────────────────
    void  set_camera_mode(int m)       { camera_mode = Math::clamp(m, 1, 3); }
    int   get_camera_mode() const      { return camera_mode; }
    void  set_mouse_sensitivity(float s){ mouse_sensitivity = s; }
    float get_mouse_sensitivity() const{ return mouse_sensitivity; }

    // ── Player identity ────────────────────────────────────────────
    void   set_user_id(int id)                  { user_id = id; }
    int    get_user_id() const                  { return user_id; }
    void   set_display_name(String n)           { display_name = n; }
    String get_display_name() const             { return display_name; }
    void   set_account_age(int a)               { account_age = a; }
    int    get_account_age() const              { return account_age; }
    void   set_team_color(Color c)              { team_color = c; }
    Color  get_team_color() const               { return team_color; }
    void   set_auto_jump_enabled(bool b)        { auto_jump_enabled = b; }
    bool   get_auto_jump_enabled() const        { return auto_jump_enabled; }
    void   set_dev_camera_occlusion_mode(int m) { dev_camera_occlusion_mode = Math::clamp(m,0,1); }
    int    get_dev_camera_occlusion_mode() const{ return dev_camera_occlusion_mode; }
    void   set_dev_computer_movement_mode(int m){ dev_computer_movement_mode = Math::clamp(m,0,2); }
    int    get_dev_computer_movement_mode() const{ return dev_computer_movement_mode; }
    void   set_dev_touch_movement_mode(int m)   { dev_touch_movement_mode = Math::clamp(m,0,3); }
    int    get_dev_touch_movement_mode() const  { return dev_touch_movement_mode; }

    // ── _ready ─────────────────────────────────────────────────────
    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;

        last_position = get_global_position();

        // Pivote horizontal: top-level para no heredar rotación del cuerpo
        pivot_h = memnew(Node3D);
        pivot_h->set_name("CameraPivotH");
        pivot_h->set_as_top_level(true);
        add_child(pivot_h);

        // Pivote vertical: hijo del horizontal
        pivot_v = memnew(Node3D);
        pivot_v->set_name("CameraPivotV");
        pivot_h->add_child(pivot_v);

        // SpringArm: evita que la cámara corte geometría
        spring_arm = memnew(SpringArm3D);
        spring_arm->set_name("SpringArm");
        Ref<SphereShape3D> sphere; sphere.instantiate();
        sphere->set_radius(0.3f);
        spring_arm->set_shape(sphere);
        spring_arm->set_length(target_zoom);
        spring_arm->add_excluded_object(get_rid());
        pivot_v->add_child(spring_arm);

        // Cámara al final del brazo
        camera = memnew(Camera3D);
        camera->set_name("Camera3D");
        camera->set_current(true);
        spring_arm->add_child(camera);
    }

    // ── Input: Mouse ───────────────────────────────────────────────
    void _input(const Ref<InputEvent>& p_event) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        if (!pivot_h || !pivot_v || !spring_arm) return;

        Input* input = Input::get_singleton();
        bool is_first_person = target_zoom < 0.6f;

        // Botón derecho — captura el mouse para rotar la cámara
        if (p_event->is_class("InputEventMouseButton")) {
            Ref<InputEventMouseButton> mb = p_event;
            if (mb->get_button_index() == MOUSE_BUTTON_RIGHT) {
                input->set_mouse_mode(
                    mb->is_pressed() ? Input::MOUSE_MODE_CAPTURED
                                     : Input::MOUSE_MODE_VISIBLE);
            }
            // Rueda del mouse — zoom
            if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_UP)
                target_zoom = Math::max(0.0f, target_zoom - 1.5f);
            if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN)
                target_zoom = Math::min(30.0f, target_zoom + 1.5f);
        }

        // Movimiento del mouse — rotar cámara
        Ref<InputEventMouseMotion> mm = p_event;
        if (mm.is_valid()) {
            bool mouse_captured = (input->get_mouse_mode() == Input::MOUSE_MODE_CAPTURED);
            if (mouse_captured || is_first_person) {
                if (is_first_person) input->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);

                // Rotación horizontal
                Vector3 rot_h = pivot_h->get_rotation();
                rot_h.y -= mm->get_relative().x * mouse_sensitivity;
                pivot_h->set_rotation(rot_h);

                // Rotación vertical (limitada para no girar demasiado)
                Vector3 rot_v = pivot_v->get_rotation();
                rot_v.x -= mm->get_relative().y * mouse_sensitivity;
                rot_v.x = Math::clamp(rot_v.x, -1.4f, 1.4f);
                pivot_v->set_rotation(rot_v);

                // En 1ª persona el cuerpo rota con la cámara
                if (is_first_person) {
                    set_rotation(Vector3(0.0f, rot_h.y, 0.0f));
                }
            }
        }
    }

    // ── _process: cámara y zoom ────────────────────────────────────
    void _process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        if (!pivot_h || !spring_arm) return;

        // Altura del pivot (a la altura de los ojos)
        Vector3 target_pos = get_global_position() + Vector3(0.0f, 1.5f, 0.0f);

        // ── Detectar movimiento para modo 3 ──────────────────────
        Vector3 current_pos = get_global_position();
        bool is_moving = (current_pos - last_position).length_squared() > 0.001f;
        last_position = current_pos;
        if (is_moving) idle_time = 0.0f;
        else           idle_time += (float)delta;

        // ── Aplicar modo de cámara ────────────────────────────────
        switch (camera_mode) {
            case 1: {
                // MODO 1: Fija — sigue al instante
                pivot_h->set_global_position(target_pos);
                break;
            }
            case 2: {
                // MODO 2: Suave — lerp constante (retraso elegante)
                Vector3 cur = pivot_h->get_global_position();
                float alpha = 1.0f - Math::pow(0.05f, (float)delta);
                pivot_h->set_global_position(cur.lerp(target_pos, alpha));
                break;
            }
            case 3: {
                // MODO 3: Combinada — rápido al moverse, centra al detenerse
                // Cuando idle_time > 0.5s, el alpha aumenta para centrar más rápido
                Vector3 cur = pivot_h->get_global_position();
                float base_alpha = is_moving ? 0.04f : 0.25f;
                float alpha = 1.0f - Math::pow(base_alpha, (float)delta);
                pivot_h->set_global_position(cur.lerp(target_pos, alpha));
                break;
            }
        }

        // Suavizado del zoom (igual en todos los modos)
        float current_len = spring_arm->get_length();
        spring_arm->set_length(
            Math::lerp(current_len, target_zoom, (float)Math::min(10.0 * delta, 1.0)));

        // Ocultar el mesh en primera persona
        MeshInstance3D* mesh = Object::cast_to<MeshInstance3D>(get_node_or_null("Mesh"));
        if (mesh) {
            mesh->set_visible(target_zoom >= 0.6f);
        }
    }
};

#endif // ROBLOX_PLAYER_H