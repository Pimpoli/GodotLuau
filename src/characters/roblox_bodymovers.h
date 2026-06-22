#ifndef ROBLOX_BODYMOVERS_H
#define ROBLOX_BODYMOVERS_H

// ════════════════════════════════════════════════════════════════════
//  BodyMovers — equivalents to Roblox's BodyMovers
////
//  BodyMovers — equivalentes a los BodyMovers de Roblox
//
//  All must be a direct child of a RigidBody3D (RobloxPart).
//  They activate in _physics_process to move/rotate the parent part.
////
//  Todos deben ser hijo directo de un RigidBody3D (RobloxPart).
//  Se activan en _physics_process para mover/rotar la parte padre.
//
//  BodyVelocity        — Keeps the part at a target velocity / Mantiene la parte a una velocidad objetivo
//  BodyPosition        — Moves the part toward a target position (PD) / Mueve la parte hacia una posición objetivo (PD)
//  BodyForce           — Applies a constant force / Aplica una fuerza constante
//  BodyAngularVelocity — Maintains a target angular velocity / Mantiene una velocidad angular objetivo
//  BodyGyro            — Rotates the part toward a target orientation / Rota la parte hacia una orientación objetivo
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ────────────────────────────────────────────────────────────────────
//  BodyVelocity
// ────────────────────────────────────────────────────────────────────
class BodyVelocity : public Node {
    GDCLASS(BodyVelocity, Node);

    Vector3 velocity    = Vector3(0, 0, 0);
    Vector3 max_force   = Vector3(4e5f, 4e5f, 4e5f);
    float   p_value     = 10000.0f;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_velocity",  "v"), &BodyVelocity::set_velocity);
        ClassDB::bind_method(D_METHOD("get_velocity"),       &BodyVelocity::get_velocity);
        ClassDB::bind_method(D_METHOD("set_max_force", "v"), &BodyVelocity::set_max_force);
        ClassDB::bind_method(D_METHOD("get_max_force"),      &BodyVelocity::get_max_force);
        ClassDB::bind_method(D_METHOD("set_p", "p"),         &BodyVelocity::set_p);
        ClassDB::bind_method(D_METHOD("get_p"),              &BodyVelocity::get_p);

        ADD_GROUP("BodyVelocity", "");
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "Velocity"),  "set_velocity",  "get_velocity");
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "MaxForce"),  "set_max_force", "get_max_force");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,   "P", PROPERTY_HINT_RANGE, "1,100000,1"),
                     "set_p", "get_p");
    }

public:
    void    set_velocity(Vector3 v)  { velocity  = v; }
    Vector3 get_velocity() const     { return velocity; }
    void    set_max_force(Vector3 v) { max_force = v; }
    Vector3 get_max_force() const    { return max_force; }
    void    set_p(float p)           { p_value   = Math::max(p, 0.1f); }
    float   get_p() const            { return p_value; }

    void _physics_process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        RigidBody3D* body = Object::cast_to<RigidBody3D>(get_parent());
        if (!body) return;

        // With very large MaxForce: set velocity directly (default Roblox behavior)
        //// Con MaxForce muy grande: setear velocidad directo (comportamiento Roblox default)
        if (max_force.x > 1e5f && max_force.y > 1e5f && max_force.z > 1e5f) {
            body->set_linear_velocity(velocity);
        } else {
            Vector3 cur = body->get_linear_velocity();
            float   mass = body->get_mass();
            Vector3 force = (velocity - cur) * p_value * mass;
            force.x = Math::clamp(force.x, -max_force.x, max_force.x);
            force.y = Math::clamp(force.y, -max_force.y, max_force.y);
            force.z = Math::clamp(force.z, -max_force.z, max_force.z);
            body->apply_central_force(force);
        }
    }
};

// ────────────────────────────────────────────────────────────────────
//  BodyPosition
// ────────────────────────────────────────────────────────────────────
class BodyPosition : public Node {
    GDCLASS(BodyPosition, Node);

    Vector3 position  = Vector3(0, 0, 0);
    Vector3 max_force = Vector3(4e4f, 4e4f, 4e4f);
    float   p_value   = 10000.0f;
    float   d_value   = 1000.0f;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_position",  "v"), &BodyPosition::set_position_val);
        ClassDB::bind_method(D_METHOD("get_position"),       &BodyPosition::get_position_val);
        ClassDB::bind_method(D_METHOD("set_max_force", "v"), &BodyPosition::set_max_force);
        ClassDB::bind_method(D_METHOD("get_max_force"),      &BodyPosition::get_max_force);
        ClassDB::bind_method(D_METHOD("set_p", "p"),         &BodyPosition::set_p);
        ClassDB::bind_method(D_METHOD("get_p"),              &BodyPosition::get_p);
        ClassDB::bind_method(D_METHOD("set_d", "d"),         &BodyPosition::set_d);
        ClassDB::bind_method(D_METHOD("get_d"),              &BodyPosition::get_d);

        ADD_GROUP("BodyPosition", "");
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "Position"), "set_position", "get_position");
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "MaxForce"), "set_max_force","get_max_force");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "P", PROPERTY_HINT_RANGE,"1,100000,1"), "set_p","get_p");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "D", PROPERTY_HINT_RANGE,"0,10000,1"),  "set_d","get_d");
    }

public:
    void    set_position_val(Vector3 v) { position  = v; }
    Vector3 get_position_val() const    { return position; }
    void    set_max_force(Vector3 v)    { max_force = v; }
    Vector3 get_max_force() const       { return max_force; }
    void    set_p(float p)              { p_value   = Math::max(p, 0.1f); }
    float   get_p() const               { return p_value; }
    void    set_d(float d)              { d_value   = Math::max(d, 0.0f); }
    float   get_d() const               { return d_value; }

    void _physics_process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        RigidBody3D* body = Object::cast_to<RigidBody3D>(get_parent());
        if (!body) return;

        Vector3 pos_err = position - body->get_global_position();
        Vector3 vel     = body->get_linear_velocity();
        float   mass    = body->get_mass();

        // PD controller: F = P*err - D*vel
        Vector3 force = pos_err * p_value * mass - vel * d_value * mass;
        force.x = Math::clamp(force.x, -max_force.x, max_force.x);
        force.y = Math::clamp(force.y, -max_force.y, max_force.y);
        force.z = Math::clamp(force.z, -max_force.z, max_force.z);
        body->apply_central_force(force);
    }
};

// ────────────────────────────────────────────────────────────────────
//  BodyForce
// ────────────────────────────────────────────────────────────────────
class BodyForce : public Node {
    GDCLASS(BodyForce, Node);

    Vector3 force       = Vector3(0, 0, 0);
    bool    relative_to = false; // false=World, true=Part

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_force",       "v"), &BodyForce::set_force);
        ClassDB::bind_method(D_METHOD("get_force"),            &BodyForce::get_force);
        ClassDB::bind_method(D_METHOD("set_relative_to", "b"), &BodyForce::set_relative_to);
        ClassDB::bind_method(D_METHOD("get_relative_to"),      &BodyForce::get_relative_to);

        ADD_GROUP("BodyForce", "");
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "Force"),      "set_force","get_force");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,    "RelativeTo"), "set_relative_to","get_relative_to");
    }

public:
    void    set_force(Vector3 v)       { force       = v; }
    Vector3 get_force() const          { return force; }
    void    set_relative_to(bool b)    { relative_to = b; }
    bool    get_relative_to() const    { return relative_to; }

    void _physics_process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        RigidBody3D* body = Object::cast_to<RigidBody3D>(get_parent());
        if (!body) return;

        if (relative_to) {
            // Convert force from local space to world space
            //// Convertir la fuerza del espacio local al espacio global
            Vector3 world_force = body->get_global_transform().basis.xform(force);
            body->apply_central_force(world_force);
        } else {
            body->apply_central_force(force);
        }
    }
};

// ────────────────────────────────────────────────────────────────────
//  BodyAngularVelocity
// ────────────────────────────────────────────────────────────────────
class BodyAngularVelocity : public Node {
    GDCLASS(BodyAngularVelocity, Node);

    Vector3 angular_velocity = Vector3(0, 0, 0);
    Vector3 max_torque       = Vector3(4e4f, 4e4f, 4e4f);
    float   p_value          = 10000.0f;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_angular_velocity","v"), &BodyAngularVelocity::set_angular_velocity);
        ClassDB::bind_method(D_METHOD("get_angular_velocity"),     &BodyAngularVelocity::get_angular_velocity);
        ClassDB::bind_method(D_METHOD("set_max_torque","v"),       &BodyAngularVelocity::set_max_torque);
        ClassDB::bind_method(D_METHOD("get_max_torque"),           &BodyAngularVelocity::get_max_torque);
        ClassDB::bind_method(D_METHOD("set_p","p"),                &BodyAngularVelocity::set_p);
        ClassDB::bind_method(D_METHOD("get_p"),                    &BodyAngularVelocity::get_p);

        ADD_GROUP("BodyAngularVelocity", "");
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3,"AngularVelocity"), "set_angular_velocity","get_angular_velocity");
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3,"MaxTorque"),       "set_max_torque","get_max_torque");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"P",PROPERTY_HINT_RANGE,"1,100000,1"), "set_p","get_p");
    }

public:
    void    set_angular_velocity(Vector3 v) { angular_velocity = v; }
    Vector3 get_angular_velocity() const    { return angular_velocity; }
    void    set_max_torque(Vector3 v)       { max_torque = v; }
    Vector3 get_max_torque() const          { return max_torque; }
    void    set_p(float p)                  { p_value = Math::max(p, 0.1f); }
    float   get_p() const                   { return p_value; }

    void _physics_process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        RigidBody3D* body = Object::cast_to<RigidBody3D>(get_parent());
        if (!body) return;

        if (max_torque.x > 1e4f && max_torque.y > 1e4f && max_torque.z > 1e4f) {
            body->set_angular_velocity(angular_velocity);
        } else {
            Vector3 cur   = body->get_angular_velocity();
            float   mass  = body->get_mass();
            Vector3 torque = (angular_velocity - cur) * p_value * mass;
            torque.x = Math::clamp(torque.x, -max_torque.x, max_torque.x);
            torque.y = Math::clamp(torque.y, -max_torque.y, max_torque.y);
            torque.z = Math::clamp(torque.z, -max_torque.z, max_torque.z);
            body->apply_torque(torque);
        }
    }
};

// ────────────────────────────────────────────────────────────────────
//  BodyGyro — Rotates the part toward a target orientation
////
//  BodyGyro — Rota la parte hacia una orientación objetivo
// ────────────────────────────────────────────────────────────────────
class BodyGyro : public Node {
    GDCLASS(BodyGyro, Node);

    Transform3D cframe_target;  // Target rotation (only Basis is used) / Rotación objetivo (solo se usa Basis)
    Vector3     max_torque = Vector3(4e4f, 4e4f, 4e4f);
    float       p_value    = 10000.0f;
    float       d_value    = 1000.0f;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_max_torque","v"), &BodyGyro::set_max_torque);
        ClassDB::bind_method(D_METHOD("get_max_torque"),     &BodyGyro::get_max_torque);
        ClassDB::bind_method(D_METHOD("set_p","p"),          &BodyGyro::set_p);
        ClassDB::bind_method(D_METHOD("get_p"),              &BodyGyro::get_p);
        ClassDB::bind_method(D_METHOD("set_d","d"),          &BodyGyro::set_d);
        ClassDB::bind_method(D_METHOD("get_d"),              &BodyGyro::get_d);

        ADD_GROUP("BodyGyro", "");
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3,"MaxTorque"), "set_max_torque","get_max_torque");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"P",PROPERTY_HINT_RANGE,"1,100000,1"), "set_p","get_p");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"D",PROPERTY_HINT_RANGE,"0,10000,1"),  "set_d","get_d");
    }

public:
    void        set_max_torque(Vector3 v) { max_torque   = v; }
    Vector3     get_max_torque() const    { return max_torque; }
    void        set_p(float p)            { p_value = Math::max(p, 0.1f); }
    float       get_p() const             { return p_value; }
    void        set_d(float d)            { d_value = Math::max(d, 0.0f); }
    float       get_d() const             { return d_value; }
    void        set_cframe(Transform3D t) { cframe_target = t; }
    Transform3D get_cframe() const        { return cframe_target; }

    void _physics_process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        RigidBody3D* body = Object::cast_to<RigidBody3D>(get_parent());
        if (!body) return;

        // Compute the rotation error between current and target orientation
        //// Calcular el error de rotación entre la orientación actual y la objetivo
        Quaternion cur    = body->get_quaternion();
        Quaternion target = Quaternion(cframe_target.basis);
        Quaternion diff   = target * cur.inverse();

        // Ensure we take the shortest path
        //// Asegurar que tomamos el camino más corto
        if (diff.w < 0.0f) diff = Quaternion(-diff.x, -diff.y, -diff.z, -diff.w);

        float half_angle = Math::acos(Math::clamp((double)diff.w, -1.0, 1.0));
        float sin_half   = Math::sin(half_angle);
        Vector3 axis;
        if (sin_half > 0.0001f) axis = Vector3(diff.x, diff.y, diff.z) / sin_half;

        float   angle     = 2.0f * half_angle;
        Vector3 ang_vel   = body->get_angular_velocity();
        float   mass      = body->get_mass();
        Vector3 torque    = axis * angle * p_value * mass - ang_vel * d_value * mass;
        torque.x = Math::clamp(torque.x, -max_torque.x, max_torque.x);
        torque.y = Math::clamp(torque.y, -max_torque.y, max_torque.y);
        torque.z = Math::clamp(torque.z, -max_torque.z, max_torque.z);
        body->apply_torque(torque);
    }
};

#endif // ROBLOX_BODYMOVERS_H
