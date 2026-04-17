#ifndef ROBLOX_PART_H
#define ROBLOX_PART_H

#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

// Necesario para interactuar con la máquina virtual de Luau
#include "lua.h"
#include "lualib.h"

using namespace godot;

class RobloxPart : public RigidBody3D {
    GDCLASS(RobloxPart, RigidBody3D);

private:
    MeshInstance3D* mesh_instance;
    CollisionShape3D* collision_shape;
    Ref<StandardMaterial3D> material;
    Ref<BoxMesh> box_mesh;
    Ref<BoxShape3D> box_shape;

    // Propiedades clásicas de Roblox
    bool anchored = true;
    Color color = Color(0.63f, 0.64f, 0.65f); // Medium stone grey
    Vector3 size = Vector3(4, 1, 2);
    float transparency = 0.0;
    bool can_collide = true;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_anchored", "a"), &RobloxPart::set_anchored);
        ClassDB::bind_method(D_METHOD("get_anchored"), &RobloxPart::get_anchored);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "Anchored"), "set_anchored", "get_anchored");

        ClassDB::bind_method(D_METHOD("set_color", "c"), &RobloxPart::set_color);
        ClassDB::bind_method(D_METHOD("get_color"), &RobloxPart::get_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR, "Color"), "set_color", "get_color");

        ClassDB::bind_method(D_METHOD("set_size", "s"), &RobloxPart::set_size);
        ClassDB::bind_method(D_METHOD("get_size"), &RobloxPart::get_size);
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "Size"), "set_size", "get_size");

        ClassDB::bind_method(D_METHOD("set_transparency", "t"), &RobloxPart::set_transparency);
        ClassDB::bind_method(D_METHOD("get_transparency"), &RobloxPart::get_transparency);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Transparency", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_transparency", "get_transparency");

        ClassDB::bind_method(D_METHOD("set_can_collide", "c"), &RobloxPart::set_can_collide);
        ClassDB::bind_method(D_METHOD("get_can_collide"), &RobloxPart::get_can_collide);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "CanCollide"), "set_can_collide", "get_can_collide");

        // --- SEÑALES ---
        ADD_SIGNAL(MethodInfo("Touched", PropertyInfo(Variant::OBJECT, "hit_node")));
        
        ClassDB::bind_method(D_METHOD("_on_body_entered", "body"), &RobloxPart::_on_body_entered);
        
        // ¡MAGIA! Renombramos la función para engañar a la caché y forzar 3 argumentos
        ClassDB::bind_method(D_METHOD("_on_luau_part_touched", "hit_node", "luau_ref", "lua_state_ptr"), &RobloxPart::_on_luau_part_touched);
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            // Construcción automática de la geometría si no existe
            if (!mesh_instance) {
                mesh_instance = memnew(MeshInstance3D);
                box_mesh.instantiate();
                material.instantiate();
                
                box_mesh->set_size(size);
                material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
                material->set_albedo(Color(color.r, color.g, color.b, 1.0f - transparency));
                
                mesh_instance->set_mesh(box_mesh);
                mesh_instance->set_material_override(material);
                add_child(mesh_instance);

                collision_shape = memnew(CollisionShape3D);
                box_shape.instantiate();
                box_shape->set_size(size);
                collision_shape->set_shape(box_shape);
                add_child(collision_shape);

                // Configuración de físicas
                set_freeze_enabled(anchored);
                set_contact_monitor(true);
                set_max_contacts_reported(5);
                
                // Conectar detección de colisión nativa
                connect("body_entered", Callable(this, "_on_body_entered"));
            }
        }
    }

public:
    RobloxPart() {
        mesh_instance = nullptr;
        collision_shape = nullptr;
    }

    // Detector de colisión de Godot
    void _on_body_entered(Node* body) {
        if (body && body != this) {
            emit_signal("Touched", body);
        }
    }

    // --- ¡EL PUENTE ACTIVO 3.0! ---
    void _on_luau_part_touched(Node* p_hit, int p_ref, int64_t ptr_L) {
        lua_State* L = (lua_State*)ptr_L;
        if (!L || !p_hit) return;

        lua_getref(L, p_ref); 
        
        if (lua_isfunction(L, -1)) {
            // Envolver el objeto que chocó
            struct GodotObjectWrapper { Node* node_ptr; };
            GodotObjectWrapper* wrap = (GodotObjectWrapper*)lua_newuserdata(L, sizeof(GodotObjectWrapper));
            wrap->node_ptr = p_hit;
            luaL_getmetatable(L, "GodotObject"); 
            lua_setmetatable(L, -2);
            
            // Disparamos la función en Luau
            if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                UtilityFunctions::print("[Luau Error en Touched] ", lua_tostring(L, -1));
                lua_pop(L, 1); // Limpiar error
            }
        } else {
            lua_pop(L, 1); // Limpiar si no era función
        }
    }

    // --- GETTERS Y SETTERS ---
    void set_anchored(bool p_anchored) { anchored = p_anchored; set_freeze_enabled(anchored); }
    bool get_anchored() const { return anchored; }
    void set_color(Color p_color) { color = p_color; if (material.is_valid()) material->set_albedo(Color(color.r, color.g, color.b, 1.0f - transparency)); }
    Color get_color() const { return color; }
    void set_size(Vector3 p_size) { size = p_size; if (box_mesh.is_valid()) box_mesh->set_size(size); if (box_shape.is_valid()) box_shape->set_size(size); }
    Vector3 get_size() const { return size; }
    void set_transparency(float p_transparency) { transparency = p_transparency; if (material.is_valid()) material->set_albedo(Color(color.r, color.g, color.b, 1.0f - transparency)); }
    float get_transparency() const { return transparency; }
    void set_can_collide(bool p_can_collide) { can_collide = p_can_collide; if (collision_shape) collision_shape->set_disabled(!can_collide); }
    bool get_can_collide() const { return can_collide; }
};

#endif