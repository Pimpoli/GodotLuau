#ifndef ROBLOX_CONSTRAINTS_H
#define ROBLOX_CONSTRAINTS_H

// ════════════════════════════════════════════════════════════════════
//  Constraints — equivalents to Roblox's Constraints
////
//  Constraints — equivalentes a los Constraints de Roblox
//
//  WeldConstraint       — Welds two parts together (no movement) / Suelda dos partes juntas (sin movimiento)
//  HingeConstraint      — Hinge between two parts (one free axis) / Bisagra entre dos partes (un eje libre)
//  BallSocketConstraint — Ball joint (multiple axes) / Rótula (múltiples ejes)
//  RodConstraint        — Fixed distance between two parts / Distancia fija entre dos partes
//  SpringConstraint     — Spring between two parts / Resorte entre dos partes
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/classes/hinge_joint3d.hpp>
#include <godot_cpp/classes/generic6_dof_joint3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ────────────────────────────────────────────────────────────────────
//  WeldConstraint — Keeps Part1 glued to Part0
////
//  WeldConstraint — Mantiene Part1 pegado a Part0
// ────────────────────────────────────────────────────────────────────
class WeldConstraint : public Node {
    GDCLASS(WeldConstraint, Node);

    Node*       part0              = nullptr;
    Node*       part1              = nullptr;
    bool        enabled            = true;
    Transform3D relative_transform;   // Computed in _ready / Calculado en _ready
    bool        transform_captured = false;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_part0","n"),   &WeldConstraint::set_part0);
        ClassDB::bind_method(D_METHOD("get_part0"),        &WeldConstraint::get_part0);
        ClassDB::bind_method(D_METHOD("set_part1","n"),   &WeldConstraint::set_part1);
        ClassDB::bind_method(D_METHOD("get_part1"),        &WeldConstraint::get_part1);
        ClassDB::bind_method(D_METHOD("set_enabled","b"), &WeldConstraint::set_enabled);
        ClassDB::bind_method(D_METHOD("get_enabled"),      &WeldConstraint::get_enabled);

        ADD_GROUP("WeldConstraint","");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "Enabled"), "set_enabled","get_enabled");
    }

public:
    void  set_part0(Node* n)   { part0 = n; transform_captured = false; }
    Node* get_part0() const    { return part0; }
    void  set_part1(Node* n)   { part1 = n; transform_captured = false; }
    Node* get_part1() const    { return part1; }
    void  set_enabled(bool b)  { enabled = b; }
    bool  get_enabled() const  { return enabled; }

    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        _capture_relative_transform();
    }

    void _capture_relative_transform() {
        if (!part0 || !part1) return;
        Node3D* n0 = Object::cast_to<Node3D>(part0);
        Node3D* n1 = Object::cast_to<Node3D>(part1);
        if (!n0 || !n1) return;
        // relative = part0_inv * part1
        relative_transform  = n0->get_global_transform().inverse() * n1->get_global_transform();
        transform_captured  = true;

        // Freeze Part1 so it has no independent dynamics
        //// Congelar Part1 para que no tenga dinámica propia
        RigidBody3D* rb1 = Object::cast_to<RigidBody3D>(part1);
        if (rb1) {
            rb1->set_freeze_enabled(true);
            rb1->set_freeze_mode(RigidBody3D::FREEZE_MODE_KINEMATIC);
        }
    }

    void _physics_process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint() || !enabled) return;
        if (!part0 || !part1) return;
        if (!transform_captured) { _capture_relative_transform(); return; }

        Node3D* n0 = Object::cast_to<Node3D>(part0);
        Node3D* n1 = Object::cast_to<Node3D>(part1);
        if (!n0 || !n1) return;

        // Apply the stored relative transform
        //// Aplicar la transformación relativa guardada
        n1->set_global_transform(n0->get_global_transform() * relative_transform);

        // Cancel residual velocities
        //// Anular velocidades residuales
        RigidBody3D* rb1 = Object::cast_to<RigidBody3D>(part1);
        if (rb1 && !rb1->is_freeze_enabled()) {
            rb1->set_linear_velocity(Vector3());
            rb1->set_angular_velocity(Vector3());
        }
    }
};

// ────────────────────────────────────────────────────────────────────
//  HingeConstraint — Hinge (one free rotation axis)
////
//  HingeConstraint — Bisagra (un eje de rotación libre)
// ────────────────────────────────────────────────────────────────────
class HingeConstraint : public Node {
    GDCLASS(HingeConstraint, Node);

    Node*  part0         = nullptr;
    Node*  part1         = nullptr;
    bool   enabled       = true;
    bool   limits_enabled = false;
    float  upper_angle   = 45.0f;
    float  lower_angle   = -45.0f;
    bool   motor_enabled = false;
    float  motor_max_torque   = 1000.0f;
    float  motor_target_velocity = 0.0f;
    float  current_angle = 0.0f;

    HingeJoint3D* godot_joint = nullptr;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_part0","n"),               &HingeConstraint::set_part0);
        ClassDB::bind_method(D_METHOD("get_part0"),                    &HingeConstraint::get_part0);
        ClassDB::bind_method(D_METHOD("set_part1","n"),               &HingeConstraint::set_part1);
        ClassDB::bind_method(D_METHOD("get_part1"),                    &HingeConstraint::get_part1);
        ClassDB::bind_method(D_METHOD("set_enabled","b"),             &HingeConstraint::set_enabled);
        ClassDB::bind_method(D_METHOD("get_enabled"),                  &HingeConstraint::get_enabled);
        ClassDB::bind_method(D_METHOD("set_limits_enabled","b"),      &HingeConstraint::set_limits_enabled);
        ClassDB::bind_method(D_METHOD("get_limits_enabled"),           &HingeConstraint::get_limits_enabled);
        ClassDB::bind_method(D_METHOD("set_upper_angle","a"),         &HingeConstraint::set_upper_angle);
        ClassDB::bind_method(D_METHOD("get_upper_angle"),              &HingeConstraint::get_upper_angle);
        ClassDB::bind_method(D_METHOD("set_lower_angle","a"),         &HingeConstraint::set_lower_angle);
        ClassDB::bind_method(D_METHOD("get_lower_angle"),              &HingeConstraint::get_lower_angle);
        ClassDB::bind_method(D_METHOD("set_motor_enabled","b"),       &HingeConstraint::set_motor_enabled);
        ClassDB::bind_method(D_METHOD("get_motor_enabled"),            &HingeConstraint::get_motor_enabled);
        ClassDB::bind_method(D_METHOD("set_motor_max_torque","t"),    &HingeConstraint::set_motor_max_torque);
        ClassDB::bind_method(D_METHOD("get_motor_max_torque"),         &HingeConstraint::get_motor_max_torque);
        ClassDB::bind_method(D_METHOD("set_motor_target_velocity","v"),&HingeConstraint::set_motor_target_velocity);
        ClassDB::bind_method(D_METHOD("get_motor_target_velocity"),    &HingeConstraint::get_motor_target_velocity);
        ClassDB::bind_method(D_METHOD("get_current_angle"),            &HingeConstraint::get_current_angle);

        ADD_GROUP("HingeConstraint","");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "Enabled"),          "set_enabled","get_enabled");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "LimitsEnabled"),    "set_limits_enabled","get_limits_enabled");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "UpperAngle",        PROPERTY_HINT_RANGE,"-180,180,0.1"), "set_upper_angle","get_upper_angle");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "LowerAngle",        PROPERTY_HINT_RANGE,"-180,180,0.1"), "set_lower_angle","get_lower_angle");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "MotorEnabled"),     "set_motor_enabled","get_motor_enabled");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "MotorMaxTorque",    PROPERTY_HINT_RANGE,"0,100000,1"),   "set_motor_max_torque","get_motor_max_torque");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "MotorTargetVelocity",PROPERTY_HINT_RANGE,"-500,500,0.1"),"set_motor_target_velocity","get_motor_target_velocity");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "CurrentAngle"),     "","get_current_angle");
    }

public:
    void  set_part0(Node* n)                    { part0 = n; }
    Node* get_part0() const                     { return part0; }
    void  set_part1(Node* n)                    { part1 = n; }
    Node* get_part1() const                     { return part1; }
    void  set_enabled(bool b)                   { enabled = b; _update_joint(); }
    bool  get_enabled() const                   { return enabled; }
    void  set_limits_enabled(bool b)            { limits_enabled = b; _update_joint(); }
    bool  get_limits_enabled() const            { return limits_enabled; }
    void  set_upper_angle(float a)              { upper_angle = a; _update_joint(); }
    float get_upper_angle() const               { return upper_angle; }
    void  set_lower_angle(float a)              { lower_angle = a; _update_joint(); }
    float get_lower_angle() const               { return lower_angle; }
    void  set_motor_enabled(bool b)             { motor_enabled = b; _update_joint(); }
    bool  get_motor_enabled() const             { return motor_enabled; }
    void  set_motor_max_torque(float t)         { motor_max_torque = t; _update_joint(); }
    float get_motor_max_torque() const          { return motor_max_torque; }
    void  set_motor_target_velocity(float v)    { motor_target_velocity = v; _update_joint(); }
    float get_motor_target_velocity() const     { return motor_target_velocity; }
    float get_current_angle() const             { return current_angle; }

    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        _build_joint();
    }

    void _build_joint() {
        if (!part0 || !part1 || !is_inside_tree()) return;

        Node3D* n0 = Object::cast_to<Node3D>(part0);
        Node3D* n1 = Object::cast_to<Node3D>(part1);
        if (!n0 || !n1) return;

        godot_joint = memnew(HingeJoint3D);
        // Place the joint at the midpoint between Part0 and Part1
        //// Posicionar el joint en el punto medio entre Part0 y Part1
        Vector3 mid = (n0->get_global_position() + n1->get_global_position()) * 0.5f;
        godot_joint->set_global_position(mid);
        godot_joint->set_node_a(n0->get_path());
        godot_joint->set_node_b(n1->get_path());
        add_child(godot_joint);
        _update_joint();
    }

    void _update_joint() {
        if (!godot_joint) return;
        if (limits_enabled) {
            godot_joint->set_flag(HingeJoint3D::FLAG_USE_LIMIT, true);
            godot_joint->set_param(HingeJoint3D::PARAM_LIMIT_UPPER, Math::deg_to_rad((double)upper_angle));
            godot_joint->set_param(HingeJoint3D::PARAM_LIMIT_LOWER, Math::deg_to_rad((double)lower_angle));
        } else {
            godot_joint->set_flag(HingeJoint3D::FLAG_USE_LIMIT, false);
        }
        if (motor_enabled) {
            godot_joint->set_flag(HingeJoint3D::FLAG_ENABLE_MOTOR, true);
            godot_joint->set_param(HingeJoint3D::PARAM_MOTOR_MAX_IMPULSE, motor_max_torque * 0.016f);
            godot_joint->set_param(HingeJoint3D::PARAM_MOTOR_TARGET_VELOCITY, Math::deg_to_rad((double)motor_target_velocity));
        } else {
            godot_joint->set_flag(HingeJoint3D::FLAG_ENABLE_MOTOR, false);
        }
    }
};

// ────────────────────────────────────────────────────────────────────
//  BallSocketConstraint — Spherical ball joint
////
//  BallSocketConstraint — Rótula esférica
// ────────────────────────────────────────────────────────────────────
class BallSocketConstraint : public Node {
    GDCLASS(BallSocketConstraint, Node);

    Node* part0  = nullptr;
    Node* part1  = nullptr;
    bool  enabled = true;
    bool  limits_enabled = false;
    float upper_angle    = 45.0f;
    float twist_upper    = 180.0f;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_part0","n"),          &BallSocketConstraint::set_part0);
        ClassDB::bind_method(D_METHOD("get_part0"),               &BallSocketConstraint::get_part0);
        ClassDB::bind_method(D_METHOD("set_part1","n"),          &BallSocketConstraint::set_part1);
        ClassDB::bind_method(D_METHOD("get_part1"),               &BallSocketConstraint::get_part1);
        ClassDB::bind_method(D_METHOD("set_enabled","b"),        &BallSocketConstraint::set_enabled);
        ClassDB::bind_method(D_METHOD("get_enabled"),             &BallSocketConstraint::get_enabled);
        ClassDB::bind_method(D_METHOD("set_limits_enabled","b"), &BallSocketConstraint::set_limits_enabled);
        ClassDB::bind_method(D_METHOD("get_limits_enabled"),      &BallSocketConstraint::get_limits_enabled);
        ClassDB::bind_method(D_METHOD("set_upper_angle","a"),    &BallSocketConstraint::set_upper_angle);
        ClassDB::bind_method(D_METHOD("get_upper_angle"),         &BallSocketConstraint::get_upper_angle);

        ADD_GROUP("BallSocketConstraint","");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "Enabled"),       "set_enabled","get_enabled");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "LimitsEnabled"), "set_limits_enabled","get_limits_enabled");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "UpperAngle",     PROPERTY_HINT_RANGE,"0,180,0.1"), "set_upper_angle","get_upper_angle");
    }

public:
    void  set_part0(Node* n)           { part0 = n; }
    Node* get_part0() const            { return part0; }
    void  set_part1(Node* n)           { part1 = n; }
    Node* get_part1() const            { return part1; }
    void  set_enabled(bool b)          { enabled = b; }
    bool  get_enabled() const          { return enabled; }
    void  set_limits_enabled(bool b)   { limits_enabled = b; }
    bool  get_limits_enabled() const   { return limits_enabled; }
    void  set_upper_angle(float a)     { upper_angle = a; }
    float get_upper_angle() const      { return upper_angle; }

    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint() || !part0 || !part1) return;
        Node3D* n0 = Object::cast_to<Node3D>(part0);
        Node3D* n1 = Object::cast_to<Node3D>(part1);
        if (!n0 || !n1) return;

        Generic6DOFJoint3D* j = memnew(Generic6DOFJoint3D);
        Vector3 mid = (n0->get_global_position() + n1->get_global_position()) * 0.5f;
        j->set_global_position(mid);
        j->set_node_a(n0->get_path());
        j->set_node_b(n1->get_path());
        // Lock translation, free rotation (sphere)
        //// Bloquear translación, liberar rotación (esfera)
        j->set_flag_x(Generic6DOFJoint3D::FLAG_ENABLE_LINEAR_LIMIT, true);
        j->set_flag_y(Generic6DOFJoint3D::FLAG_ENABLE_LINEAR_LIMIT, true);
        j->set_flag_z(Generic6DOFJoint3D::FLAG_ENABLE_LINEAR_LIMIT, true);
        j->set_flag_x(Generic6DOFJoint3D::FLAG_ENABLE_ANGULAR_LIMIT, limits_enabled);
        j->set_flag_y(Generic6DOFJoint3D::FLAG_ENABLE_ANGULAR_LIMIT, limits_enabled);
        j->set_flag_z(Generic6DOFJoint3D::FLAG_ENABLE_ANGULAR_LIMIT, limits_enabled);
        if (limits_enabled) {
            float rad = Math::deg_to_rad((double)upper_angle);
            j->set_param_x(Generic6DOFJoint3D::PARAM_ANGULAR_UPPER_LIMIT,  rad);
            j->set_param_x(Generic6DOFJoint3D::PARAM_ANGULAR_LOWER_LIMIT, -rad);
            j->set_param_y(Generic6DOFJoint3D::PARAM_ANGULAR_UPPER_LIMIT,  rad);
            j->set_param_y(Generic6DOFJoint3D::PARAM_ANGULAR_LOWER_LIMIT, -rad);
            j->set_param_z(Generic6DOFJoint3D::PARAM_ANGULAR_UPPER_LIMIT,  rad);
            j->set_param_z(Generic6DOFJoint3D::PARAM_ANGULAR_LOWER_LIMIT, -rad);
        }
        add_child(j);
    }
};

// ────────────────────────────────────────────────────────────────────
//  RodConstraint — Keeps a fixed distance between two parts
////
//  RodConstraint — Mantiene distancia fija entre dos partes
// ────────────────────────────────────────────────────────────────────
class RodConstraint : public Node {
    GDCLASS(RodConstraint, Node);

    Node* part0   = nullptr;
    Node* part1   = nullptr;
    bool  enabled = true;
    float length  = 5.0f;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_part0","n"),   &RodConstraint::set_part0);
        ClassDB::bind_method(D_METHOD("get_part0"),        &RodConstraint::get_part0);
        ClassDB::bind_method(D_METHOD("set_part1","n"),   &RodConstraint::set_part1);
        ClassDB::bind_method(D_METHOD("get_part1"),        &RodConstraint::get_part1);
        ClassDB::bind_method(D_METHOD("set_enabled","b"), &RodConstraint::set_enabled);
        ClassDB::bind_method(D_METHOD("get_enabled"),      &RodConstraint::get_enabled);
        ClassDB::bind_method(D_METHOD("set_length","l"),  &RodConstraint::set_length);
        ClassDB::bind_method(D_METHOD("get_length"),       &RodConstraint::get_length);

        ADD_GROUP("RodConstraint","");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "Enabled"), "set_enabled","get_enabled");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Length", PROPERTY_HINT_RANGE,"0,1000,0.01"), "set_length","get_length");
    }

public:
    void  set_part0(Node* n)    { part0 = n; }
    Node* get_part0() const     { return part0; }
    void  set_part1(Node* n)    { part1 = n; }
    Node* get_part1() const     { return part1; }
    void  set_enabled(bool b)   { enabled = b; }
    bool  get_enabled() const   { return enabled; }
    void  set_length(float l)   { length = Math::max(l, 0.01f); }
    float get_length() const    { return length; }

    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint() || !part0 || !part1) return;
        // Auto-compute initial length if it is 0
        //// Auto-calcular longitud inicial si es 0
        Node3D* n0 = Object::cast_to<Node3D>(part0);
        Node3D* n1 = Object::cast_to<Node3D>(part1);
        if (n0 && n1 && length <= 0.01f) {
            length = n0->get_global_position().distance_to(n1->get_global_position());
        }
    }

    void _physics_process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint() || !enabled || !part0 || !part1) return;
        Node3D* n0 = Object::cast_to<Node3D>(part0);
        Node3D* n1 = Object::cast_to<Node3D>(part1);
        if (!n0 || !n1) return;

        Vector3 p0 = n0->get_global_position();
        Vector3 p1 = n1->get_global_position();
        float   d  = p0.distance_to(p1);
        if (d < 0.001f) return;

        float   diff  = d - length;
        Vector3 dir   = (p1 - p0).normalized();
        Vector3 corr  = dir * diff * 0.5f;

        RigidBody3D* rb0 = Object::cast_to<RigidBody3D>(part0);
        RigidBody3D* rb1 = Object::cast_to<RigidBody3D>(part1);

        if (rb0 && !rb0->is_freeze_enabled()) n0->set_global_position(p0 + corr);
        if (rb1 && !rb1->is_freeze_enabled()) n1->set_global_position(p1 - corr);
    }
};

// ────────────────────────────────────────────────────────────────────
//  SpringConstraint — Spring between two parts
////
//  SpringConstraint — Resorte entre dos partes
// ────────────────────────────────────────────────────────────────────
class SpringConstraint : public Node {
    GDCLASS(SpringConstraint, Node);

    Node* part0       = nullptr;
    Node* part1       = nullptr;
    bool  enabled     = true;
    float free_length = 5.0f;
    float stiffness   = 200.0f;
    float damping     = 20.0f;
    float max_length  = 10.0f;
    float min_length  = 0.0f;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_part0","n"),       &SpringConstraint::set_part0);
        ClassDB::bind_method(D_METHOD("get_part0"),            &SpringConstraint::get_part0);
        ClassDB::bind_method(D_METHOD("set_part1","n"),       &SpringConstraint::set_part1);
        ClassDB::bind_method(D_METHOD("get_part1"),            &SpringConstraint::get_part1);
        ClassDB::bind_method(D_METHOD("set_enabled","b"),     &SpringConstraint::set_enabled);
        ClassDB::bind_method(D_METHOD("get_enabled"),          &SpringConstraint::get_enabled);
        ClassDB::bind_method(D_METHOD("set_free_length","l"), &SpringConstraint::set_free_length);
        ClassDB::bind_method(D_METHOD("get_free_length"),      &SpringConstraint::get_free_length);
        ClassDB::bind_method(D_METHOD("set_stiffness","s"),   &SpringConstraint::set_stiffness);
        ClassDB::bind_method(D_METHOD("get_stiffness"),        &SpringConstraint::get_stiffness);
        ClassDB::bind_method(D_METHOD("set_damping","d"),     &SpringConstraint::set_damping);
        ClassDB::bind_method(D_METHOD("get_damping"),          &SpringConstraint::get_damping);

        ADD_GROUP("SpringConstraint","");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "Enabled"),    "set_enabled","get_enabled");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "FreeLength",  PROPERTY_HINT_RANGE,"0,100,0.01"),    "set_free_length","get_free_length");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Stiffness",   PROPERTY_HINT_RANGE,"0,100000,1"),    "set_stiffness","get_stiffness");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Damping",     PROPERTY_HINT_RANGE,"0,10000,0.1"),   "set_damping","get_damping");
    }

public:
    void  set_part0(Node* n)         { part0 = n; }
    Node* get_part0() const          { return part0; }
    void  set_part1(Node* n)         { part1 = n; }
    Node* get_part1() const          { return part1; }
    void  set_enabled(bool b)        { enabled = b; }
    bool  get_enabled() const        { return enabled; }
    void  set_free_length(float l)   { free_length = Math::max(l, 0.0f); }
    float get_free_length() const    { return free_length; }
    void  set_stiffness(float s)     { stiffness = Math::max(s, 0.0f); }
    float get_stiffness() const      { return stiffness; }
    void  set_damping(float d)       { damping = Math::max(d, 0.0f); }
    float get_damping() const        { return damping; }

    void _physics_process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint() || !enabled || !part0 || !part1) return;
        Node3D* n0 = Object::cast_to<Node3D>(part0);
        Node3D* n1 = Object::cast_to<Node3D>(part1);
        if (!n0 || !n1) return;

        Vector3 p0  = n0->get_global_position();
        Vector3 p1  = n1->get_global_position();
        float   d   = p0.distance_to(p1);
        if (d < 0.001f) return;

        Vector3 dir  = (p1 - p0).normalized();
        float ext    = d - free_length;            // extension / extensión
        Vector3 spring_force = dir * ext * stiffness;

        // Relative velocities for damping
        //// Velocidades relativas para amortiguamiento
        RigidBody3D* rb0 = Object::cast_to<RigidBody3D>(part0);
        RigidBody3D* rb1 = Object::cast_to<RigidBody3D>(part1);

        Vector3 rel_vel;
        if (rb1) rel_vel += rb1->get_linear_velocity();
        if (rb0) rel_vel -= rb0->get_linear_velocity();
        float damp_scalar = rel_vel.dot(dir);
        Vector3 damp_force = dir * damp_scalar * damping;

        Vector3 total = spring_force + damp_force;

        if (rb0 && !rb0->is_freeze_enabled()) rb0->apply_central_force( total);
        if (rb1 && !rb1->is_freeze_enabled()) rb1->apply_central_force(-total);
    }
};

#endif // ROBLOX_CONSTRAINTS_H
