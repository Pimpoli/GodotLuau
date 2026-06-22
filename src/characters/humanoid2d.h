#ifndef HUMANOID2D_H
#define HUMANOID2D_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

using namespace godot;

// ════════════════════════════════════════════════════════════════════
//  Humanoid2D — Health system for 2D characters
//
//  Physical movement is implemented directly in RobloxPlayer2D.
//  This node handles health, signals, and properties configurable from Luau.
////
//  Humanoid2D — Sistema de salud para personajes 2D
//
//  El movimiento físico está implementado directamente en RobloxPlayer2D.
//  Este nodo maneja salud, señales y propiedades configurables desde Luau.
// ════════════════════════════════════════════════════════════════════
class Humanoid2D : public Node {
    GDCLASS(Humanoid2D, Node);

private:
    float health     = 100.0f;
    float max_health = 100.0f;
    bool  is_dead    = false;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_health", "h"),     &Humanoid2D::set_health);
        ClassDB::bind_method(D_METHOD("get_health"),           &Humanoid2D::get_health);
        ClassDB::bind_method(D_METHOD("set_max_health", "h"), &Humanoid2D::set_max_health);
        ClassDB::bind_method(D_METHOD("get_max_health"),       &Humanoid2D::get_max_health);
        ClassDB::bind_method(D_METHOD("take_damage", "amount"), &Humanoid2D::take_damage);
        ClassDB::bind_method(D_METHOD("heal", "amount"),        &Humanoid2D::heal);

        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Health",    PROPERTY_HINT_RANGE, "0,1000,0.1"),
                     "set_health",     "get_health");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "MaxHealth", PROPERTY_HINT_RANGE, "1,1000,1"),
                     "set_max_health", "get_max_health");

        // Signal equivalent to Roblox: Humanoid.Died
        //// Señal equivalente a Roblox: Humanoid.Died
        ADD_SIGNAL(MethodInfo("Died"));
        ADD_SIGNAL(MethodInfo("HealthChanged",
            PropertyInfo(Variant::FLOAT, "new_health"),
            PropertyInfo(Variant::FLOAT, "old_health")));
    }

public:
    void set_health(float h) {
        float old = health;
        health = Math::clamp(h, 0.0f, max_health);

        if (old != health) {
            emit_signal("HealthChanged", health, old);
        }

        if (health <= 0.0f && !is_dead) {
            is_dead = true;
            emit_signal("Died");
        }
    }
    float get_health() const { return health; }

    void set_max_health(float h) {
        max_health = Math::max(h, 1.0f);
        health = Math::min(health, max_health);
    }
    float get_max_health() const { return max_health; }

    void take_damage(float amount) {
        if (is_dead) return;
        set_health(health - amount);
    }

    void heal(float amount) {
        if (is_dead) return;
        set_health(health + amount);
    }
};

#endif // HUMANOID2D_H
