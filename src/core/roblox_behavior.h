#ifndef ROBLOX_BEHAVIOR_H
#define ROBLOX_BEHAVIOR_H

// ════════════════════════════════════════════════════════════════════
//  Clases nuevas con COMPORTAMIENTO real (no solo holders).
//  - UICorner    : redondea las esquinas del GUI padre
//  - UIListLayout: ordena los hijos Control del padre en lista
//  CornerRadius/Padding aceptan número o UDim (se usa el Offset) — ver el
//  puente en luau_api.h.
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/style_box_texture.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture2d.hpp>
#include <godot_cpp/classes/gpu_particles3d.hpp>
#include <godot_cpp/classes/particle_process_material.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/audio_effect_reverb.hpp>
#include <godot_cpp/classes/audio_effect_delay.hpp>
#include <godot_cpp/classes/audio_effect_eq6.hpp>
#include <godot_cpp/classes/audio_effect_distortion.hpp>
#include <godot_cpp/classes/audio_effect_compressor.hpp>
#include <godot_cpp/classes/audio_effect_pitch_shift.hpp>
#include <godot_cpp/classes/audio_effect_chorus.hpp>
#include <godot_cpp/classes/audio_listener3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include "roblox_sound.h"
#include "gl_ui_modifiers.h"   // UICorner/UIListLayout/UIPadding/... (semantica real de Roblox)

using namespace godot;

// ── ParticleEmitter ───────────────────────────────────────────────────
//  GPUParticles3D real con una configuración por defecto que SÍ emite.
//  Rate/Color/Size etc. se guardan por el puente; el mapeo fino queda "por
//  mejorar", pero ya se ven partículas.
class ParticleEmitter : public GPUParticles3D {
    GDCLASS(ParticleEmitter, GPUParticles3D);
    Ref<ParticleProcessMaterial> pm;
    Ref<StandardMaterial3D> mm;
    Color  pcolor = Color(1.0f, 0.8f, 0.3f, 0.9f);
    double speed  = 2.25;

    void _rebuild() {
        if (pm.is_null()) pm.instantiate();
        pm->set_direction(Vector3(0, 1, 0));
        pm->set_spread(25.0f);
        pm->set_gravity(Vector3(0, -1.5f, 0));
        pm->set_param_min(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, (float)(speed * 0.7));
        pm->set_param_max(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, (float)(speed * 1.3));
        pm->set_param_min(ParticleProcessMaterial::PARAM_SCALE, 0.5f);
        pm->set_param_max(ParticleProcessMaterial::PARAM_SCALE, 1.0f);
        set_process_material(pm);
        if (mm.is_null()) mm.instantiate();
        mm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
        mm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
        mm->set_billboard_mode(BaseMaterial3D::BILLBOARD_PARTICLES);
        mm->set_albedo(pcolor);
        Ref<QuadMesh> qm; qm.instantiate();
        qm->set_size(Vector2(0.3f, 0.3f));
        qm->set_material(mm);
        set_draw_pass_mesh(0, qm);
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_Rate", "v"),     &ParticleEmitter::set_Rate);
        ClassDB::bind_method(D_METHOD("get_Rate"),          &ParticleEmitter::get_Rate);
        ClassDB::bind_method(D_METHOD("set_Lifetime", "v"), &ParticleEmitter::set_Lifetime);
        ClassDB::bind_method(D_METHOD("get_Lifetime"),      &ParticleEmitter::get_Lifetime);
        ClassDB::bind_method(D_METHOD("set_Color", "v"),    &ParticleEmitter::set_Color);
        ClassDB::bind_method(D_METHOD("get_Color"),         &ParticleEmitter::get_Color);
        ClassDB::bind_method(D_METHOD("set_Speed", "v"),    &ParticleEmitter::set_Speed);
        ClassDB::bind_method(D_METHOD("get_Speed"),         &ParticleEmitter::get_Speed);
        ClassDB::bind_method(D_METHOD("set_Enabled", "v"),  &ParticleEmitter::set_Enabled);
        ClassDB::bind_method(D_METHOD("get_Enabled"),       &ParticleEmitter::get_Enabled);
        ADD_PROPERTY(PropertyInfo(Variant::INT,   "Rate"),     "set_Rate",     "get_Rate");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Lifetime"), "set_Lifetime", "get_Lifetime");
        ADD_PROPERTY(PropertyInfo(Variant::COLOR, "Color"),    "set_Color",    "get_Color");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Speed"),    "set_Speed",    "get_Speed");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "Enabled"),  "set_Enabled",  "get_Enabled");
    }

public:
    void   set_Rate(int v)       { set_amount(v > 0 ? v : 1); }
    int    get_Rate() const      { return get_amount(); }
    void   set_Lifetime(double v){ set_lifetime(v > 0 ? v : 0.1); }
    double get_Lifetime() const  { return get_lifetime(); }
    void   set_Color(Color c)    { pcolor = c; if (mm.is_valid()) mm->set_albedo(c); }
    Color  get_Color() const     { return pcolor; }
    void   set_Speed(double s)   {
        speed = s;
        if (pm.is_valid()) {
            pm->set_param_min(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, (float)(s * 0.7));
            pm->set_param_max(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, (float)(s * 1.3));
        }
    }
    double get_Speed() const     { return speed; }
    void   set_Enabled(bool e)   { set_emitting(e); }
    bool   get_Enabled() const   { return is_emitting(); }

    void _ready() override {
        _rebuild();
        set_amount(50);
        set_lifetime(2.0);
        set_emitting(true);
    }
    ParticleEmitter() { set_meta("_gl_bridge", true); }
};

// ── *SoundEffect ──────────────────────────────────────────────────────
//  Al entrar en escena, añade el AudioEffect de Godot correspondiente al bus
//  del SoundGroup padre (o Master). Efecto de audio REAL.
#define GL_SOUND_EFFECT(NAME, EFFECTTYPE)                                    \
    class NAME : public Node {                                               \
        GDCLASS(NAME, Node);                                                 \
    protected: static void _bind_methods() {}                               \
    public:                                                                   \
        NAME() { set_meta("_gl_bridge", true); }                            \
        void _ready() override {                                             \
            if (Engine::get_singleton()->is_editor_hint()) return;          \
            String bus = "Master";                                          \
            RobloxSoundGroup* g = Object::cast_to<RobloxSoundGroup>(get_parent()); \
            if (g) bus = g->get_group_name();                               \
            AudioServer* as = AudioServer::get_singleton();                 \
            int bi = as->get_bus_index(bus); if (bi < 0) bi = 0;            \
            Ref<EFFECTTYPE> fx; fx.instantiate();                          \
            if (fx.is_valid()) as->add_bus_effect(bi, fx);                 \
        }                                                                    \
    };
GL_SOUND_EFFECT(ReverbSoundEffect,     AudioEffectReverb)
GL_SOUND_EFFECT(EchoSoundEffect,       AudioEffectDelay)
GL_SOUND_EFFECT(EqualizerSoundEffect,  AudioEffectEQ6)
GL_SOUND_EFFECT(DistortionSoundEffect, AudioEffectDistortion)
GL_SOUND_EFFECT(CompressorSoundEffect, AudioEffectCompressor)
GL_SOUND_EFFECT(PitchShiftSoundEffect, AudioEffectPitchShift)
GL_SOUND_EFFECT(ChorusSoundEffect,     AudioEffectChorus)
#undef GL_SOUND_EFFECT

// ── Constraints modernos con física REAL ─────────────────────────────
//  Actúan sobre el RigidBody3D padre (el RobloxPart). Reemplazan a los
//  BodyMovers. El anclaje por Attachment queda "por mejorar".

// VectorForce: fuerza constante
class VectorForce : public Node3D {
    GDCLASS(VectorForce, Node3D);
    Vector3 force = Vector3(0, 0, 0);
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_Force", "v"), &VectorForce::set_Force);
        ClassDB::bind_method(D_METHOD("get_Force"),      &VectorForce::get_Force);
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "Force"), "set_Force", "get_Force");
    }
public:
    void set_Force(Vector3 v) { force = v; }
    Vector3 get_Force() const { return force; }
    void _physics_process(double) override {
        RigidBody3D* rb = Object::cast_to<RigidBody3D>(get_parent());
        if (rb) rb->apply_central_force(force);
    }
    VectorForce() { set_meta("_gl_bridge", true); set_physics_process(true); }
};

// Torque: par constante
class Torque : public Node3D {
    GDCLASS(Torque, Node3D);
    Vector3 torque = Vector3(0, 0, 0);
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_Torque", "v"), &Torque::set_Torque);
        ClassDB::bind_method(D_METHOD("get_Torque"),      &Torque::get_Torque);
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "Torque"), "set_Torque", "get_Torque");
    }
public:
    void set_Torque(Vector3 v) { torque = v; }
    Vector3 get_Torque() const { return torque; }
    void _physics_process(double) override {
        RigidBody3D* rb = Object::cast_to<RigidBody3D>(get_parent());
        if (rb) rb->apply_torque(torque);
    }
    Torque() { set_meta("_gl_bridge", true); set_physics_process(true); }
};

// LinearVelocity: mantiene una velocidad lineal
class LinearVelocity : public Node3D {
    GDCLASS(LinearVelocity, Node3D);
    Vector3 vel = Vector3(0, 0, 0);
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_VectorVelocity", "v"), &LinearVelocity::set_VectorVelocity);
        ClassDB::bind_method(D_METHOD("get_VectorVelocity"),      &LinearVelocity::get_VectorVelocity);
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "VectorVelocity"), "set_VectorVelocity", "get_VectorVelocity");
    }
public:
    void set_VectorVelocity(Vector3 v) { vel = v; }
    Vector3 get_VectorVelocity() const { return vel; }
    void _physics_process(double) override {
        RigidBody3D* rb = Object::cast_to<RigidBody3D>(get_parent());
        if (rb) rb->set_linear_velocity(vel);
    }
    LinearVelocity() { set_meta("_gl_bridge", true); set_physics_process(true); }
};

// AngularVelocity: mantiene una velocidad angular
class AngularVelocity : public Node3D {
    GDCLASS(AngularVelocity, Node3D);
    Vector3 av = Vector3(0, 0, 0);
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_AngularVelocity", "v"), &AngularVelocity::set_AngularVelocity);
        ClassDB::bind_method(D_METHOD("get_AngularVelocity"),      &AngularVelocity::get_AngularVelocity);
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "AngularVelocity"), "set_AngularVelocity", "get_AngularVelocity");
    }
public:
    void set_AngularVelocity(Vector3 v) { av = v; }
    Vector3 get_AngularVelocity() const { return av; }
    void _physics_process(double) override {
        RigidBody3D* rb = Object::cast_to<RigidBody3D>(get_parent());
        if (rb) rb->set_angular_velocity(av);
    }
    AngularVelocity() { set_meta("_gl_bridge", true); set_physics_process(true); }
};

// AlignPosition: muelle-amortiguador hacia una posición objetivo
class AlignPosition : public Node3D {
    GDCLASS(AlignPosition, Node3D);
    Vector3 target = Vector3(0, 0, 0);
    double  responsiveness = 10.0;
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_Position", "v"), &AlignPosition::set_Position);
        ClassDB::bind_method(D_METHOD("get_Position"),      &AlignPosition::get_Position);
        ClassDB::bind_method(D_METHOD("set_Responsiveness", "v"), &AlignPosition::set_Responsiveness);
        ClassDB::bind_method(D_METHOD("get_Responsiveness"),      &AlignPosition::get_Responsiveness);
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "Position"),       "set_Position",       "get_Position");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,   "Responsiveness"), "set_Responsiveness", "get_Responsiveness");
    }
public:
    void    set_Position(Vector3 v) { target = v; }
    Vector3 get_Position() const { return target; }
    void    set_Responsiveness(double v) { responsiveness = v > 0.1 ? v : 0.1; }
    double  get_Responsiveness() const { return responsiveness; }
    void _physics_process(double) override {
        RigidBody3D* rb = Object::cast_to<RigidBody3D>(get_parent());
        if (!rb) return;
        Vector3 pos = rb->get_global_position();
        Vector3 v   = rb->get_linear_velocity();
        Vector3 f = (target - pos) * (float)(responsiveness * 3.0) - v * (float)(responsiveness * 0.8);
        rb->apply_central_force(f);
    }
    AlignPosition() { set_meta("_gl_bridge", true); set_physics_process(true); }
};

// AlignOrientation: alinea la orientación del cuerpo a una objetivo (reemplaza BodyGyro)
class AlignOrientation : public Node3D {
    GDCLASS(AlignOrientation, Node3D);
    Vector3 orient_deg = Vector3(0, 0, 0);   // Orientación objetivo (grados)
    double  responsiveness = 10.0;
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_Orientation", "v"), &AlignOrientation::set_Orientation);
        ClassDB::bind_method(D_METHOD("get_Orientation"),      &AlignOrientation::get_Orientation);
        ClassDB::bind_method(D_METHOD("set_Responsiveness", "v"), &AlignOrientation::set_Responsiveness);
        ClassDB::bind_method(D_METHOD("get_Responsiveness"),      &AlignOrientation::get_Responsiveness);
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "Orientation"),    "set_Orientation",    "get_Orientation");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,   "Responsiveness"), "set_Responsiveness", "get_Responsiveness");
    }
public:
    void    set_Orientation(Vector3 v) { orient_deg = v; }
    Vector3 get_Orientation() const { return orient_deg; }
    void    set_Responsiveness(double v) { responsiveness = v > 0.1 ? v : 0.1; }
    double  get_Responsiveness() const { return responsiveness; }
    void _physics_process(double) override {
        RigidBody3D* rb = Object::cast_to<RigidBody3D>(get_parent());
        if (!rb) return;
        Basis cur = rb->get_global_transform().basis.orthonormalized();
        Basis tar = Basis::from_euler(orient_deg * (float)(Math_PI / 180.0));
        Vector3 rot = (tar * cur.inverse()).get_euler();  // vector de rotación de error
        Vector3 torque = rot * (float)(responsiveness * 3.0) - rb->get_angular_velocity() * (float)(responsiveness * 0.8);
        rb->apply_torque(torque);
    }
    AlignOrientation() { set_meta("_gl_bridge", true); set_physics_process(true); }
};

// LineForce: fuerza a lo largo de la línea hacia un punto objetivo
class LineForce : public Node3D {
    GDCLASS(LineForce, Node3D);
    double  magnitude = 0.0;
    Vector3 target = Vector3(0, 0, 0);
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_Magnitude", "v"), &LineForce::set_Magnitude);
        ClassDB::bind_method(D_METHOD("get_Magnitude"),      &LineForce::get_Magnitude);
        ClassDB::bind_method(D_METHOD("set_TargetPosition", "v"), &LineForce::set_TargetPosition);
        ClassDB::bind_method(D_METHOD("get_TargetPosition"),      &LineForce::get_TargetPosition);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,   "Magnitude"),      "set_Magnitude",      "get_Magnitude");
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "TargetPosition"), "set_TargetPosition", "get_TargetPosition");
    }
public:
    void    set_Magnitude(double v) { magnitude = v; }
    double  get_Magnitude() const { return magnitude; }
    void    set_TargetPosition(Vector3 v) { target = v; }
    Vector3 get_TargetPosition() const { return target; }
    void _physics_process(double) override {
        RigidBody3D* rb = Object::cast_to<RigidBody3D>(get_parent());
        if (!rb) return;
        Vector3 dir = target - rb->get_global_position();
        float len = dir.length();
        if (len > 1e-4f) rb->apply_central_force(dir / len * (float)magnitude);
    }
    LineForce() { set_meta("_gl_bridge", true); set_physics_process(true); }
};

// ── CanvasGroup: transparencia/color de grupo sobre el subárbol ──────
class RobloxCanvasGroup : public Control {
    GDCLASS(RobloxCanvasGroup, Control);
    double group_transparency = 0.0;
    Color  group_color = Color(1, 1, 1);
    void _apply() { set_modulate(Color(group_color.r, group_color.g, group_color.b, 1.0f - (float)group_transparency)); }
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_GroupTransparency", "v"), &RobloxCanvasGroup::set_GroupTransparency);
        ClassDB::bind_method(D_METHOD("get_GroupTransparency"),      &RobloxCanvasGroup::get_GroupTransparency);
        ClassDB::bind_method(D_METHOD("set_GroupColor3", "v"),       &RobloxCanvasGroup::set_GroupColor3);
        ClassDB::bind_method(D_METHOD("get_GroupColor3"),            &RobloxCanvasGroup::get_GroupColor3);
        ClassDB::bind_method(D_METHOD("_apply"), &RobloxCanvasGroup::_apply);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "GroupTransparency"), "set_GroupTransparency", "get_GroupTransparency");
        ADD_PROPERTY(PropertyInfo(Variant::COLOR, "GroupColor3"),       "set_GroupColor3",       "get_GroupColor3");
    }
public:
    void   set_GroupTransparency(double v) { group_transparency = v; _apply(); }
    double get_GroupTransparency() const { return group_transparency; }
    void   set_GroupColor3(Color c) { group_color = c; _apply(); }
    Color  get_GroupColor3() const { return group_color; }
    void _ready() override { _apply(); }
    RobloxCanvasGroup() { set_meta("_gl_bridge", true); }
};

// ── AudioListener: fija el listener 3D del audio ─────────────────────
class AudioListener : public Node3D {
    GDCLASS(AudioListener, Node3D);
    AudioListener3D* listener = nullptr;
protected:
    static void _bind_methods() {}
public:
    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        listener = memnew(AudioListener3D);
        add_child(listener);
        listener->make_current();
    }
    AudioListener() { set_meta("_gl_bridge", true); }
};

// ── FlangeSoundEffect (aprox. con AudioEffectChorus) ─────────────────
class FlangeSoundEffect : public Node {
    GDCLASS(FlangeSoundEffect, Node);
protected: static void _bind_methods() {}
public:
    FlangeSoundEffect() { set_meta("_gl_bridge", true); }
    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        String bus = "Master";
        RobloxSoundGroup* g = Object::cast_to<RobloxSoundGroup>(get_parent());
        if (g) bus = g->get_group_name();
        AudioServer* as = AudioServer::get_singleton();
        int bi = as->get_bus_index(bus); if (bi < 0) bi = 0;
        Ref<AudioEffectChorus> fx; fx.instantiate();
        if (fx.is_valid()) as->add_bus_effect(bi, fx);
    }
};

inline void gl_register_behavior_classes() {
    gl_register_ui_modifier_classes();
    ClassDB::register_class<RobloxCanvasGroup>();
    ClassDB::register_class<AudioListener>();
    ClassDB::register_class<FlangeSoundEffect>();
    ClassDB::register_class<ParticleEmitter>();
    ClassDB::register_class<ReverbSoundEffect>();
    ClassDB::register_class<EchoSoundEffect>();
    ClassDB::register_class<EqualizerSoundEffect>();
    ClassDB::register_class<DistortionSoundEffect>();
    ClassDB::register_class<CompressorSoundEffect>();
    ClassDB::register_class<PitchShiftSoundEffect>();
    ClassDB::register_class<ChorusSoundEffect>();
    ClassDB::register_class<VectorForce>();
    ClassDB::register_class<Torque>();
    ClassDB::register_class<LinearVelocity>();
    ClassDB::register_class<AngularVelocity>();
    ClassDB::register_class<AlignPosition>();
    ClassDB::register_class<AlignOrientation>();
    ClassDB::register_class<LineForce>();
}

#endif // ROBLOX_BEHAVIOR_H
