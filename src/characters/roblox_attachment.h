#ifndef ROBLOX_ATTACHMENT_H
#define ROBLOX_ATTACHMENT_H

// ════════════════════════════════════════════════════════════════════
//  Attachment — punto de anclaje local (estilo Roblox)
//  Marker3D: hereda de Node3D, así que Position/CFrame ya funcionan por el
//  index/newindex genérico. Las propiedades extra (Axis, Orientation,
//  WorldPosition…) se exponen por el puente genérico "_gl_bridge".
//  Los constraints modernos podrán anclarse a Attachments más adelante.
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/marker3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/vector3.hpp>

using namespace godot;

class Attachment : public Marker3D {
    GDCLASS(Attachment, Marker3D);

    Vector3 axis = Vector3(1, 0, 0);
    Vector3 secondary_axis = Vector3(0, 1, 0);
    bool visible_flag = false;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_Axis", "v"), &Attachment::set_axis);
        ClassDB::bind_method(D_METHOD("get_Axis"), &Attachment::get_axis);
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "Axis"), "set_Axis", "get_Axis");

        ClassDB::bind_method(D_METHOD("set_SecondaryAxis", "v"), &Attachment::set_secondary_axis);
        ClassDB::bind_method(D_METHOD("get_SecondaryAxis"), &Attachment::get_secondary_axis);
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "SecondaryAxis"), "set_SecondaryAxis", "get_SecondaryAxis");

        ClassDB::bind_method(D_METHOD("set_Visible", "b"), &Attachment::set_visible_flag);
        ClassDB::bind_method(D_METHOD("get_Visible"), &Attachment::get_visible_flag);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "Visible"), "set_Visible", "get_Visible");
    }

public:
    void set_axis(const Vector3 &v) { axis = v; }
    Vector3 get_axis() const { return axis; }
    void set_secondary_axis(const Vector3 &v) { secondary_axis = v; }
    Vector3 get_secondary_axis() const { return secondary_axis; }
    void set_visible_flag(bool b) { visible_flag = b; }
    bool get_visible_flag() const { return visible_flag; }

    Attachment() { set_meta("_gl_bridge", true); }
};

#endif // ROBLOX_ATTACHMENT_H
