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
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/input_event_screen_drag.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/string.hpp>
#include "gl_runtime.h"

using namespace godot;

// ════════════════════════════════════════════════════════════════════
//  RobloxPlayer — 3D player character, equivalent to Roblox Character
//
//  CAMERA MODES (CameraMode property):
//    1 = Fixed     — Camera follows the player instantly (no lag)
//    2 = Smooth    — Camera follows with a subtle elegant delay
//    3 = Combined  — Strong lag while moving, auto-centers when idle
////
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
    Node3D* pivot_h    = nullptr;  // Horizontal pivot (mouse X) / Pivote horizontal (mouse X)
    Node3D* pivot_v    = nullptr;  // Vertical pivot (mouse Y) / Pivote vertical (mouse Y)
    SpringArm3D* spring_arm = nullptr; // Prevents camera clipping through walls / Evita que la cámara atraviese paredes
    Camera3D* camera     = nullptr;

    float mouse_sensitivity = 0.0022f;   // v1.8.3: bajado de 0.003 (camara mas calmada)
    float target_zoom       = 10.0f;
    Vector2 rmb_restore_pos;             // donde estaba el cursor antes de rotar (Roblox)
    bool    was_fp = false;              // veniamos de primera persona

    // ── Fade del personaje al acercar la camara (shader de disolucion) ──
    Ref<ShaderMaterial> fade_mat;
    bool  fade_setup_done = false;
    bool  fade_applied    = false;     // override actualmente puesto en los meshes
    float last_dissolve   = -1.0f;     // cache: no marcar el material dirty cada frame
    std::vector<uint64_t> fade_meshes; // ObjectIDs de los MeshInstance3D del Character

    void _setup_fade() {
        if (fade_setup_done) return;
        Node* character = get_node_or_null("Character");
        if (!character) { fade_setup_done = true; return; }   // capsula: usa visibilidad
        ResourceLoader* rl = ResourceLoader::get_singleton();
        const String sh_path = "res://GodotLuau/shaders/character_fade.gdshader";
        if (!rl || !rl->exists(sh_path)) { fade_setup_done = true; return; }
        Ref<Shader> sh = rl->load(sh_path);
        if (sh.is_null()) { fade_setup_done = true; return; }
        fade_mat.instantiate();
        fade_mat->set_shader(sh);
        // El sampler NUNCA queda vacio: un descriptor set incompleto en
        // D3D12/Vulkan produce "Uniforms were never supplied for set ..."
        // (godot #120417 / #65732). Blanco 1x1 = neutro si no hay textura.
        Ref<Image> white = Image::create(1, 1, false, Image::FORMAT_RGBA8);
        white->fill(Color(1, 1, 1, 1));
        fade_mat->set_shader_parameter("albedo_tex", ImageTexture::create_from_image(white));
        // Recorrer las mallas del personaje: tomar su textura y registrarlas.
        // El override se pone recien cuando el fade se activa (_update_fade).
        bool any = false;
        _apply_fade_recursive(character, any);
        if (!any) fade_mat = Ref<ShaderMaterial>();
        fade_setup_done = true;
    }
    void _apply_fade_recursive(Node* n, bool& any) {
        if (MeshInstance3D* mi = Object::cast_to<MeshInstance3D>(n)) {
            // Textura del material original (el rig R6 comparte una sola)
            Ref<StandardMaterial3D> src = mi->get_active_material(0);
            if (src.is_valid()) {
                Ref<Texture2D> tex = src->get_texture(StandardMaterial3D::TEXTURE_ALBEDO);
                if (tex.is_valid()) fade_mat->set_shader_parameter("albedo_tex", tex);
            }
            fade_meshes.push_back((uint64_t)mi->get_instance_id());
            any = true;
        }
        for (int i = 0; i < n->get_child_count(); i++) _apply_fade_recursive(n->get_child(i), any);
    }
    // Aplica/actualiza el dissolve SOLO cuando cambia. Con la camara lejos
    // (el caso normal) el override ni existe: el personaje se dibuja con su
    // material estandar y el pipeline del shader custom no entra en juego.
    void _update_fade(float d) {
        if (Math::abs(d - last_dissolve) < 0.002f) return;
        last_dissolve = d;
        bool want = d > 0.001f;
        if (want != fade_applied) {
            fade_applied = want;
            for (uint64_t id : fade_meshes) {
                Object* o = ObjectDB::get_instance(id);
                if (MeshInstance3D* mi = Object::cast_to<MeshInstance3D>(o))
                    mi->set_material_override(want ? Ref<Material>(fade_mat) : Ref<Material>());
            }
        }
        if (want) fade_mat->set_shader_parameter("dissolve", d);
    }

    // ── Camera mode ────────────────────────────────────────────────
    //// ── Modo de cámara ──────────────────────────────────────────────
    int camera_mode = 1;
    bool lock_first_person = false;   // Enum.CameraMode.LockFirstPerson (1.15)

    // For mode 3: detect if the player is moving
    //// Para el modo 3: detectar si el jugador se está moviendo
    Vector3 last_position;
    float   idle_time = 0.0f;

    // ── Controles tactiles (movil) ──────────────────────────────────
    CanvasLayer* touch_ui    = nullptr;
    Button*      jump_btn    = nullptr;
    int          move_touch  = -1;     // indice del dedo del joystick de movimiento
    Vector2      move_origin;          // punto donde se apoyo el dedo del joystick
    int          look_touch  = -1;     // indice del dedo que rota la camara
    float        touch_look_sensitivity = 0.0038f;  // v1.8.3: bajado de 0.005
    // Emulacion de movil desde el editor (device "Mobile" en la barra Players):
    // los controles tactiles aparecen aunque no haya pantalla tactil y el MOUSE
    // hace de dedo (click izquierdo = tocar).
    bool         mobile_emu  = false;
    bool         mouse_move_drag = false;   // mouse "tocando" el joystick
    bool         mouse_look_drag = false;   // mouse "tocando" la zona de mirar
    Panel*       joy_base    = nullptr;     // joystick visual (base)
    Panel*       joy_knob    = nullptr;     // joystick visual (perilla)

    // ── Roblox-style player properties ─────────────────────────────
    //// ── Propiedades de jugador tipo Roblox ─────────────────────────
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
        ClassDB::bind_method(D_METHOD("_gl_is_first_person"),        &RobloxPlayer::_gl_is_first_person);
        ClassDB::bind_method(D_METHOD("_gl_camera_yaw"),             &RobloxPlayer::_gl_camera_yaw);
        ClassDB::bind_method(D_METHOD("set_lock_first_person","v"),  &RobloxPlayer::set_lock_first_person);
        ClassDB::bind_method(D_METHOD("get_lock_first_person"),      &RobloxPlayer::get_lock_first_person);

        ADD_GROUP("Camara","");
        ADD_PROPERTY(PropertyInfo(Variant::INT,"CameraMode",PROPERTY_HINT_ENUM,
            "Fija:1,Suave:2,Combinada:3"),
            "set_camera_mode","get_camera_mode");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"MouseSensitivity",PROPERTY_HINT_RANGE,"0.0001,0.01,0.0001"),
            "set_mouse_sensitivity","get_mouse_sensitivity");

        // ── Player ──────────────────────────────────────────────────
        //// ── Jugador ─────────────────────────────────────────────────
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
    // Para el Humanoid (1.15): en primera persona el cuerpo se pega al yaw de la
    // camara en vez de girar hacia el movimiento. OJO: camera_mode de esta clase
    // es el estilo de seguimiento (Fija/Suave/Combinada), NO el Enum.CameraMode
    // de Roblox; ese llega por set_lock_first_person.
    bool  _gl_is_first_person() const { return target_zoom < 0.6f || lock_first_person; }
    double _gl_camera_yaw() const { return pivot_h ? (double)pivot_h->get_rotation().y : 0.0; }
    // Enum.CameraMode.LockFirstPerson: fuerza y mantiene la primera persona.
    void set_lock_first_person(bool v) {
        lock_first_person = v;
        if (v) target_zoom = 0.5f;
        else if (target_zoom < 0.6f) target_zoom = 10.0f;
    }
    bool get_lock_first_person() const { return lock_first_person; }

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

        // Horizontal pivot: top-level so it doesn't inherit body rotation
        //// Pivote horizontal: top-level para no heredar rotación del cuerpo
        pivot_h = memnew(Node3D);
        pivot_h->set_name("CameraPivotH");
        pivot_h->set_as_top_level(true);
        add_child(pivot_h);

        // Vertical pivot: child of the horizontal one
        //// Pivote vertical: hijo del horizontal
        pivot_v = memnew(Node3D);
        pivot_v->set_name("CameraPivotV");
        pivot_h->add_child(pivot_v);

        // SpringArm: prevents the camera from clipping through geometry
        //// SpringArm: evita que la cámara corte geometría
        spring_arm = memnew(SpringArm3D);
        spring_arm->set_name("SpringArm");
        Ref<SphereShape3D> sphere; sphere.instantiate();
        sphere->set_radius(0.3f);
        spring_arm->set_shape(sphere);
        spring_arm->set_length(target_zoom);
        spring_arm->add_excluded_object(get_rid());
        pivot_v->add_child(spring_arm);

        // Camera at the end of the arm
        //// Cámara al final del brazo
        camera = memnew(Camera3D);
        camera->set_name("Camera3D");
        camera->set_current(true);
        spring_arm->add_child(camera);

        // ── Controles tactiles en pantalla ─────────────────────────────
        // Como el TouchGui de Roblox: aparecen solos en movil REAL (pantalla
        // tactil) o cuando el editor emula "Mobile" (barra Players). En la
        // emulacion el mouse hace de dedo. Joystick = mitad izquierda;
        // mirar = arrastrar en la derecha; salto abajo a la derecha.
        mobile_emu = (gl_emulated_device() == String("Mobile"));
        if (DisplayServer::get_singleton()->is_touchscreen_available() || mobile_emu) {
            gl_mobile().active = true;
            touch_ui = memnew(CanvasLayer);
            touch_ui->set_name("TouchControls");
            add_child(touch_ui);

            jump_btn = memnew(Button);
            jump_btn->set_name("JumpButton");
            jump_btn->set_text("Jump");
            // Ancla abajo-derecha con margen (boton de 120x120 px)
            jump_btn->set_anchor(SIDE_LEFT,   1.0f);
            jump_btn->set_anchor(SIDE_TOP,    1.0f);
            jump_btn->set_anchor(SIDE_RIGHT,  1.0f);
            jump_btn->set_anchor(SIDE_BOTTOM, 1.0f);
            jump_btn->set_offset(SIDE_LEFT,   -160.0f);
            jump_btn->set_offset(SIDE_TOP,    -160.0f);
            jump_btn->set_offset(SIDE_RIGHT,   -40.0f);
            jump_btn->set_offset(SIDE_BOTTOM,  -40.0f);
            touch_ui->add_child(jump_btn);

            // Joystick VISUAL (base + perilla) abajo-izquierda, como Roblox.
            // Es informativo: muestra hacia donde estas moviendo el "dedo".
            joy_base = memnew(Panel);
            joy_base->set_name("JoystickBase");
            _style_circle(joy_base, Color(1, 1, 1, 0.10f));
            joy_base->set_anchor(SIDE_TOP, 1.0f); joy_base->set_anchor(SIDE_BOTTOM, 1.0f);
            joy_base->set_offset(SIDE_LEFT, 40.0f);  joy_base->set_offset(SIDE_RIGHT, 200.0f);
            joy_base->set_offset(SIDE_TOP, -220.0f); joy_base->set_offset(SIDE_BOTTOM, -60.0f);
            joy_base->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
            touch_ui->add_child(joy_base);
            joy_knob = memnew(Panel);
            joy_knob->set_name("JoystickKnob");
            _style_circle(joy_knob, Color(1, 1, 1, 0.28f));
            joy_knob->set_size(Vector2(64, 64));
            joy_knob->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
            joy_base->add_child(joy_knob);
            _center_knob();
        } else {
            gl_mobile().active = false;
        }
    }

    // Circulo de UI (StyleBoxFlat con esquinas al maximo)
    void _style_circle(Panel* p, Color c) {
        Ref<StyleBoxFlat> sb; sb.instantiate();
        sb->set_bg_color(c);
        sb->set_corner_radius_all(999);
        p->add_theme_stylebox_override("panel", sb);
    }
    void _center_knob() {
        if (!joy_base || !joy_knob) return;
        joy_knob->set_position(joy_base->get_size() * 0.5f - joy_knob->get_size() * 0.5f);
    }
    // Refleja gl_mobile().move en la perilla del joystick visual
    void _update_knob() {
        if (!joy_base || !joy_knob) return;
        Vector2 v = gl_mobile().move;   // x derecha, y adelante
        Vector2 center = joy_base->get_size() * 0.5f - joy_knob->get_size() * 0.5f;
        float r = joy_base->get_size().x * 0.5f - joy_knob->get_size().x * 0.5f;
        joy_knob->set_position(center + Vector2(v.x, -v.y) * r);
    }

    // ── Input: Mouse ───────────────────────────────────────────────
    void _input(const Ref<InputEvent>& p_event) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        if (gl_freecam().active) return;   // Vista de Servidor: la cámara libre maneja el input
        if (!pivot_h || !pivot_v || !spring_arm) return;

        Input* input = Input::get_singleton();
        bool is_first_person = target_zoom < 0.6f;

        // ── Tactil: joystick de movimiento (mitad izquierda) + mirar (derecha) ─
        Vector2 screen = get_viewport()->get_visible_rect().size;

        // ── Emulacion de movil: el MOUSE hace de dedo (click izquierdo) ────
        if (mobile_emu) {
            Ref<InputEventMouseButton> emb = p_event;
            if (emb.is_valid() && emb->get_button_index() == MOUSE_BUTTON_LEFT) {
                Vector2 p = emb->get_position();
                if (emb->is_pressed()) {
                    bool in_jump = (p.x > screen.x - 170.0f) && (p.y > screen.y - 170.0f);
                    if (p.x < screen.x * 0.5f) { mouse_move_drag = true; move_origin = p; }
                    else if (!in_jump)          mouse_look_drag = true;
                } else {
                    mouse_move_drag = false; mouse_look_drag = false;
                    gl_mobile().move = Vector2();
                    _center_knob();
                }
                return;
            }
            Ref<InputEventMouseMotion> emm = p_event;
            if (emm.is_valid() && (mouse_move_drag || mouse_look_drag)) {
                if (mouse_move_drag) {
                    Vector2 d = emm->get_position() - move_origin;
                    Vector2 v = d / 110.0f;
                    if (v.length() > 1.0f) v = v.normalized();
                    gl_mobile().move = Vector2(v.x, -v.y);
                } else if (mouse_look_drag && pivot_h && pivot_v) {
                    Vector2 rel = emm->get_relative();
                    Vector3 rh = pivot_h->get_rotation();
                    rh.y -= rel.x * touch_look_sensitivity;
                    pivot_h->set_rotation(rh);
                    Vector3 rv = pivot_v->get_rotation();
                    rv.x = Math::clamp(rv.x - rel.y * touch_look_sensitivity, -1.4f, 1.4f);
                    pivot_v->set_rotation(rv);
                }
                return;
            }
        }

        Ref<InputEventScreenTouch> tt = p_event;
        if (tt.is_valid()) {
            Vector2 p = tt->get_position();
            int idx = tt->get_index();
            if (tt->is_pressed()) {
                bool in_jump = (p.x > screen.x - 170.0f) && (p.y > screen.y - 170.0f);
                if (p.x < screen.x * 0.5f && move_touch == -1) {
                    move_touch = idx; move_origin = p;
                } else if (!in_jump && look_touch == -1) {
                    look_touch = idx;
                }
            } else {
                if (idx == move_touch) { move_touch = -1; gl_mobile().move = Vector2(); }
                if (idx == look_touch) { look_touch = -1; }
            }
            return;
        }
        Ref<InputEventScreenDrag> td = p_event;
        if (td.is_valid()) {
            int idx = td->get_index();
            if (idx == move_touch) {
                Vector2 d = td->get_position() - move_origin;
                Vector2 v = d / 110.0f;                 // radio del joystick en px
                if (v.length() > 1.0f) v = v.normalized();
                gl_mobile().move = Vector2(v.x, -v.y);  // pantalla Y hacia abajo → adelante = -y
            } else if (idx == look_touch) {
                Vector2 rel = td->get_relative();
                Vector3 rh = pivot_h->get_rotation();
                rh.y -= rel.x * touch_look_sensitivity;
                pivot_h->set_rotation(rh);
                Vector3 rv = pivot_v->get_rotation();
                rv.x -= rel.y * touch_look_sensitivity;
                rv.x = Math::clamp(rv.x, -1.4f, 1.4f);
                pivot_v->set_rotation(rv);
            }
            return;
        }

        // Boton derecho — rotar la camara COMO ROBLOX: el cursor se oculta
        // mientras arrastras y al soltar REAPARECE EXACTAMENTE donde estaba
        // (nada de saltar al centro de la pantalla).
        if (p_event->is_class("InputEventMouseButton")) {
            Ref<InputEventMouseButton> mb = p_event;
            if (mb->get_button_index() == MOUSE_BUTTON_RIGHT) {
                if (mb->is_pressed()) {
                    rmb_restore_pos = get_viewport()->get_mouse_position();
                    input->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
                } else {
                    input->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
                    input->warp_mouse(rmb_restore_pos);
                }
            }
            // Rueda del mouse — zoom multiplicativo (pasos suaves como Roblox)
            if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_UP)
                target_zoom = Math::max(0.5f, target_zoom / 1.12f);
            if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN)
                target_zoom = Math::min(60.0f, target_zoom * 1.12f);
        }

        // Mouse movement — rotate camera
        //// Movimiento del mouse — rotar cámara
        Ref<InputEventMouseMotion> mm = p_event;
        if (mm.is_valid()) {
            bool mouse_captured = (input->get_mouse_mode() == Input::MOUSE_MODE_CAPTURED);
            if (mouse_captured || is_first_person) {
                if (is_first_person) input->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);

                // Horizontal rotation
                //// Rotación horizontal
                Vector3 rot_h = pivot_h->get_rotation();
                rot_h.y -= mm->get_relative().x * mouse_sensitivity;
                pivot_h->set_rotation(rot_h);

                // Vertical rotation (clamped to prevent over-rotation)
                //// Rotación vertical (limitada para no girar demasiado)
                Vector3 rot_v = pivot_v->get_rotation();
                rot_v.x -= mm->get_relative().y * mouse_sensitivity;
                rot_v.x = Math::clamp(rot_v.x, -1.4f, 1.4f);
                pivot_v->set_rotation(rot_v);

                // In 1st person the body rotates with the camera
                //// En 1ª persona el cuerpo rota con la cámara
                if (is_first_person) {
                    set_rotation(Vector3(0.0f, rot_h.y, 0.0f));
                }
            }
        }
    }

    // ── _process: camera and zoom ─────────────────────────────────
    //// ── _process: cámara y zoom ────────────────────────────────────
    void _process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        if (gl_freecam().active) return;   // Vista de Servidor: no seguir/rotar la cámara del jugador
        if (!pivot_h || !spring_arm) return;

        // ── Gamepad: stick derecho rota la camara ──────────────────
        Input* gp_input = Input::get_singleton();
        if (pivot_v) {
            Vector2 rs(gp_input->get_joy_axis(0, JOY_AXIS_RIGHT_X),
                       gp_input->get_joy_axis(0, JOY_AXIS_RIGHT_Y));
            if (rs.length() > 0.15f) {
                float gp = 1.8f * (float)delta;   // v1.8.3: bajado de 2.5 (camara mas calmada)
                Vector3 rh = pivot_h->get_rotation(); rh.y -= rs.x * gp; pivot_h->set_rotation(rh);
                Vector3 rv = pivot_v->get_rotation(); rv.x -= rs.y * gp;
                rv.x = Math::clamp(rv.x, -1.4f, 1.4f); pivot_v->set_rotation(rv);
            }
        }
        // Boton de salto tactil → estado compartido que lee el Humanoid
        if (jump_btn) gl_mobile().jump = jump_btn->is_pressed();
        // Joystick visual: la perilla sigue al movimiento actual
        _update_knob();

        // Pivot height (at eye level)
        //// Altura del pivot (a la altura de los ojos)
        Vector3 target_pos = get_global_position() + Vector3(0.0f, 1.5f, 0.0f);

        // ── Detect movement for mode 3 ────────────────────────────
        //// ── Detectar movimiento para modo 3 ──────────────────────
        Vector3 current_pos = get_global_position();
        bool is_moving = (current_pos - last_position).length_squared() > 0.001f;
        last_position = current_pos;
        if (is_moving) idle_time = 0.0f;
        else           idle_time += (float)delta;

        // ── Apply camera mode ─────────────────────────────────────
        //// ── Aplicar modo de cámara ────────────────────────────────
        switch (camera_mode) {
            case 1: {
                // MODE 1: Fixed — follows instantly
                //// MODO 1: Fija — sigue al instante
                pivot_h->set_global_position(target_pos);
                break;
            }
            case 2: {
                // MODE 2: Smooth — constant lerp (elegant delay)
                //// MODO 2: Suave — lerp constante (retraso elegante)
                Vector3 cur = pivot_h->get_global_position();
                float alpha = 1.0f - Math::pow(0.05f, (float)delta);
                pivot_h->set_global_position(cur.lerp(target_pos, alpha));
                break;
            }
            case 3: {
                // MODE 3: Combined — fast while moving, centers when idle
                // When idle_time > 0.5s, alpha increases to re-center faster
                //// MODO 3: Combinada — rápido al moverse, centra al detenerse
                //// Cuando idle_time > 0.5s, el alpha aumenta para centrar más rápido
                Vector3 cur = pivot_h->get_global_position();
                float base_alpha = is_moving ? 0.04f : 0.25f;
                float alpha = 1.0f - Math::pow(base_alpha, (float)delta);
                pivot_h->set_global_position(cur.lerp(target_pos, alpha));
                break;
            }
        }

        // Smooth zoom (same across all modes)
        //// Suavizado del zoom (igual en todos los modos)
        float current_len = spring_arm->get_length();
        spring_arm->set_length(
            Math::lerp(current_len, target_zoom, (float)Math::min(10.0 * delta, 1.0)));

        // Al salir de primera persona, liberar el mouse (si no estas rotando)
        bool fp_now = target_zoom < 0.6f;
        if (was_fp && !fp_now
            && gp_input->get_mouse_mode() == Input::MOUSE_MODE_CAPTURED
            && !gp_input->is_mouse_button_pressed(MOUSE_BUTTON_RIGHT)) {
            gp_input->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
        }
        was_fp = fp_now;

        // ── Fade del personaje al acercar la camara (como Roblox, pero con
        //    shader de disolucion: descarta pixeles en vez de mezclar alpha) ──
        if (!fade_setup_done) _setup_fade();
        if (fade_mat.is_valid()) {
            // De 2.5 hacia 0.5 de distancia el personaje se va disolviendo
            float d = 1.0f - Math::clamp((current_len - 0.5f) / 2.0f, 0.0f, 1.0f);
            _update_fade(d);
        }
        // Fallback capsula: ocultar el mesh en primera persona
        MeshInstance3D* mesh = Object::cast_to<MeshInstance3D>(get_node_or_null("Mesh"));
        if (mesh) {
            mesh->set_visible(target_zoom >= 0.6f);
        }
    }
};

#endif // ROBLOX_PLAYER_H