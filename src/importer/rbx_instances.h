#ifndef RBX_INSTANCES_H
#define RBX_INSTANCES_H

// Clases que existen en Roblox pero todavia no en GodotLuau. Sin ellas un place
// real pierde miles de objetos: Model, MeshPart, Texture, Decal, Bone y las
// mallas especiales.
//
// La mas importante es RBXInstance: representa CUALQUIER clase sin equivalente
// nativo, de modo que ninguna instancia se pierde y el arbol queda igual al de
// Roblox. Asi los scripts que hacen FindFirstChild/WaitForChild siguen
// encontrando lo que buscan.

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "roblox_part.h"   // RobloxPart, base de RBXMeshPart

using namespace godot;

// ── Instancia generica ──────────────────────────────────────────────
class RBXInstance : public Node {
    GDCLASS(RBXInstance, Node);

    String rbx_class_name = "Instance";

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_rbx_class_name", "n"), &RBXInstance::set_rbx_class_name);
        ClassDB::bind_method(D_METHOD("get_rbx_class_name"),      &RBXInstance::get_rbx_class_name);
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "ClassName"),
                     "set_rbx_class_name", "get_rbx_class_name");
    }

public:
    void set_rbx_class_name(const String &n) { rbx_class_name = n; }
    String get_rbx_class_name() const { return rbx_class_name; }
};

// ── Model ───────────────────────────────────────────────────────────
class RBXModel : public Node3D {
    GDCLASS(RBXModel, Node3D);

    NodePath primary_part;
    Transform3D world_pivot;
    float scale_factor = 1.0f;

    void _collect_aabb(Node *n, AABB &box, bool &first) const {
        RobloxPart *p = Object::cast_to<RobloxPart>(n);
        if (p) {
            AABB b(p->get_global_position() - p->get_size() * 0.5f, p->get_size());
            if (first) { box = b; first = false; }
            else box = box.merge(b);
        }
        for (int k = 0; k < n->get_child_count(); k++)
            _collect_aabb(n->get_child(k), box, first);
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_primary_part", "p"), &RBXModel::set_primary_part);
        ClassDB::bind_method(D_METHOD("get_primary_part"),      &RBXModel::get_primary_part);
        ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "PrimaryPart"),
                     "set_primary_part", "get_primary_part");

        ClassDB::bind_method(D_METHOD("set_world_pivot", "t"), &RBXModel::set_world_pivot);
        ClassDB::bind_method(D_METHOD("get_world_pivot"),      &RBXModel::get_world_pivot);
        ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "WorldPivot"),
                     "set_world_pivot", "get_world_pivot");

        ClassDB::bind_method(D_METHOD("set_scale_factor", "f"), &RBXModel::set_scale_factor);
        ClassDB::bind_method(D_METHOD("get_scale_factor"),      &RBXModel::get_scale_factor);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ScaleFactor"),
                     "set_scale_factor", "get_scale_factor");

        ClassDB::bind_method(D_METHOD("GetPivot"),           &RBXModel::get_pivot);
        ClassDB::bind_method(D_METHOD("PivotTo", "t"),       &RBXModel::pivot_to);
        ClassDB::bind_method(D_METHOD("MoveTo", "pos"),      &RBXModel::move_to);
        ClassDB::bind_method(D_METHOD("GetBoundingBox"),     &RBXModel::get_bounding_box);
        ClassDB::bind_method(D_METHOD("GetExtentsSize"),     &RBXModel::get_extents_size);
    }

public:
    void set_primary_part(const NodePath &p) { primary_part = p; }
    NodePath get_primary_part() const { return primary_part; }
    void set_world_pivot(const Transform3D &t) { world_pivot = t; }
    Transform3D get_world_pivot() const { return world_pivot; }
    void set_scale_factor(float f) { scale_factor = f; }
    float get_scale_factor() const { return scale_factor; }

    // Si hay PrimaryPart el pivote es el suyo, como en Roblox.
    Transform3D get_pivot() const {
        if (!primary_part.is_empty()) {
            Node3D *pp = Object::cast_to<Node3D>(get_node_or_null(primary_part));
            if (pp) return pp->get_global_transform();
        }
        return world_pivot;
    }

    void pivot_to(const Transform3D &t) {
        Transform3D cur = get_pivot();
        Transform3D delta = t * cur.affine_inverse();
        set_global_transform(delta * get_global_transform());
        world_pivot = t;
    }

    void move_to(const Vector3 &pos) {
        Transform3D t = get_pivot();
        t.origin = pos;
        pivot_to(t);
    }

    AABB get_bounding_box() const {
        AABB box; bool first = true;
        for (int k = 0; k < get_child_count(); k++)
            _collect_aabb(get_child(k), box, first);
        return box;
    }

    Vector3 get_extents_size() const { return get_bounding_box().size; }
};

// ── MeshPart ────────────────────────────────────────────────────────
// Hereda toda la fisica y el material de RobloxPart. El mesh real vive en la
// nube de Roblox y no se puede descargar, asi que se conserva el id y la part
// se queda con su caja hasta que el usuario enchufe su propio modelo.
class RBXMeshPart : public RobloxPart {
    GDCLASS(RBXMeshPart, RobloxPart);

    String mesh_id, texture_id;
    Vector3 mesh_size = Vector3(1, 1, 1);
    int render_fidelity = 0;
    int collision_fidelity = 0;
    bool double_sided = false;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_mesh_id", "s"), &RBXMeshPart::set_mesh_id);
        ClassDB::bind_method(D_METHOD("get_mesh_id"),      &RBXMeshPart::get_mesh_id);
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "MeshId"), "set_mesh_id", "get_mesh_id");

        ClassDB::bind_method(D_METHOD("set_texture_id", "s"), &RBXMeshPart::set_texture_id);
        ClassDB::bind_method(D_METHOD("get_texture_id"),      &RBXMeshPart::get_texture_id);
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "TextureId"), "set_texture_id", "get_texture_id");

        ClassDB::bind_method(D_METHOD("set_mesh_size", "v"), &RBXMeshPart::set_mesh_size);
        ClassDB::bind_method(D_METHOD("get_mesh_size"),      &RBXMeshPart::get_mesh_size);
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "MeshSize"), "set_mesh_size", "get_mesh_size");

        ClassDB::bind_method(D_METHOD("set_render_fidelity", "i"), &RBXMeshPart::set_render_fidelity);
        ClassDB::bind_method(D_METHOD("get_render_fidelity"),      &RBXMeshPart::get_render_fidelity);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "RenderFidelity"),
                     "set_render_fidelity", "get_render_fidelity");

        ClassDB::bind_method(D_METHOD("set_collision_fidelity", "i"), &RBXMeshPart::set_collision_fidelity);
        ClassDB::bind_method(D_METHOD("get_collision_fidelity"),      &RBXMeshPart::get_collision_fidelity);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "CollisionFidelity"),
                     "set_collision_fidelity", "get_collision_fidelity");

        ClassDB::bind_method(D_METHOD("set_double_sided", "b"), &RBXMeshPart::set_double_sided);
        ClassDB::bind_method(D_METHOD("get_double_sided"),      &RBXMeshPart::get_double_sided);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "DoubleSided"),
                     "set_double_sided", "get_double_sided");
    }

public:
    void set_mesh_id(const String &s) {
        mesh_id = s;
        // El asset no es descargable: dejamos constancia para el reporte.
        if (!s.is_empty()) set_meta("__rbx_missing_asset", s);
    }
    String get_mesh_id() const { return mesh_id; }
    void set_texture_id(const String &s) { texture_id = s; }
    String get_texture_id() const { return texture_id; }
    void set_mesh_size(const Vector3 &v) { mesh_size = v; }
    Vector3 get_mesh_size() const { return mesh_size; }
    void set_render_fidelity(int i) { render_fidelity = i; }
    int get_render_fidelity() const { return render_fidelity; }
    void set_collision_fidelity(int i) { collision_fidelity = i; }
    int get_collision_fidelity() const { return collision_fidelity; }
    void set_double_sided(bool b) { double_sided = b; }
    bool get_double_sided() const { return double_sided; }
};

// ── Texture (tileada sobre una cara) ────────────────────────────────
class RBXTexture : public Node3D {
    GDCLASS(RBXTexture, Node3D);

    String texture;
    int face = 5;                 // Front
    float studs_u = 2.0f, studs_v = 2.0f;
    float offset_u = 0.0f, offset_v = 0.0f;
    float transparency = 0.0f;
    Color color = Color(1, 1, 1);

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_texture_str", "s"), &RBXTexture::set_texture_str);
        ClassDB::bind_method(D_METHOD("get_texture_str"),      &RBXTexture::get_texture_str);
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "Texture"), "set_texture_str", "get_texture_str");

        ClassDB::bind_method(D_METHOD("set_face", "i"), &RBXTexture::set_face);
        ClassDB::bind_method(D_METHOD("get_face"),      &RBXTexture::get_face);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "Face", PROPERTY_HINT_ENUM,
                     "Right,Top,Back,Left,Bottom,Front"), "set_face", "get_face");

        ClassDB::bind_method(D_METHOD("set_studs_u", "f"), &RBXTexture::set_studs_u);
        ClassDB::bind_method(D_METHOD("get_studs_u"),      &RBXTexture::get_studs_u);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "StudsPerTileU"), "set_studs_u", "get_studs_u");

        ClassDB::bind_method(D_METHOD("set_studs_v", "f"), &RBXTexture::set_studs_v);
        ClassDB::bind_method(D_METHOD("get_studs_v"),      &RBXTexture::get_studs_v);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "StudsPerTileV"), "set_studs_v", "get_studs_v");

        ClassDB::bind_method(D_METHOD("set_offset_u", "f"), &RBXTexture::set_offset_u);
        ClassDB::bind_method(D_METHOD("get_offset_u"),      &RBXTexture::get_offset_u);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "OffsetStudsU"), "set_offset_u", "get_offset_u");

        ClassDB::bind_method(D_METHOD("set_offset_v", "f"), &RBXTexture::set_offset_v);
        ClassDB::bind_method(D_METHOD("get_offset_v"),      &RBXTexture::get_offset_v);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "OffsetStudsV"), "set_offset_v", "get_offset_v");

        ClassDB::bind_method(D_METHOD("set_transparency", "f"), &RBXTexture::set_transparency);
        ClassDB::bind_method(D_METHOD("get_transparency"),      &RBXTexture::get_transparency);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Transparency"),
                     "set_transparency", "get_transparency");

        ClassDB::bind_method(D_METHOD("set_color3", "c"), &RBXTexture::set_color3);
        ClassDB::bind_method(D_METHOD("get_color3"),      &RBXTexture::get_color3);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR, "Color3"), "set_color3", "get_color3");
    }

public:
    void set_texture_str(const String &s) {
        texture = s;
        if (!s.is_empty()) set_meta("__rbx_missing_asset", s);
    }
    String get_texture_str() const { return texture; }
    void set_face(int i) { face = i; }
    int get_face() const { return face; }
    void set_studs_u(float f) { studs_u = f; }
    float get_studs_u() const { return studs_u; }
    void set_studs_v(float f) { studs_v = f; }
    float get_studs_v() const { return studs_v; }
    void set_offset_u(float f) { offset_u = f; }
    float get_offset_u() const { return offset_u; }
    void set_offset_v(float f) { offset_v = f; }
    float get_offset_v() const { return offset_v; }
    void set_transparency(float f) { transparency = f; }
    float get_transparency() const { return transparency; }
    void set_color3(const Color &c) { color = c; }
    Color get_color3() const { return color; }
};

// ── Decal (sin tiling) ──────────────────────────────────────────────
class RBXDecal : public Node3D {
    GDCLASS(RBXDecal, Node3D);

    String texture;
    int face = 5;
    float transparency = 0.0f;
    Color color = Color(1, 1, 1);
    int z_index = 0;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_texture_str", "s"), &RBXDecal::set_texture_str);
        ClassDB::bind_method(D_METHOD("get_texture_str"),      &RBXDecal::get_texture_str);
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "Texture"), "set_texture_str", "get_texture_str");

        ClassDB::bind_method(D_METHOD("set_face", "i"), &RBXDecal::set_face);
        ClassDB::bind_method(D_METHOD("get_face"),      &RBXDecal::get_face);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "Face", PROPERTY_HINT_ENUM,
                     "Right,Top,Back,Left,Bottom,Front"), "set_face", "get_face");

        ClassDB::bind_method(D_METHOD("set_transparency", "f"), &RBXDecal::set_transparency);
        ClassDB::bind_method(D_METHOD("get_transparency"),      &RBXDecal::get_transparency);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Transparency"),
                     "set_transparency", "get_transparency");

        ClassDB::bind_method(D_METHOD("set_color3", "c"), &RBXDecal::set_color3);
        ClassDB::bind_method(D_METHOD("get_color3"),      &RBXDecal::get_color3);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR, "Color3"), "set_color3", "get_color3");

        ClassDB::bind_method(D_METHOD("set_z_index", "i"), &RBXDecal::set_z_index);
        ClassDB::bind_method(D_METHOD("get_z_index"),      &RBXDecal::get_z_index);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "ZIndex"), "set_z_index", "get_z_index");
    }

public:
    void set_texture_str(const String &s) {
        texture = s;
        if (!s.is_empty()) set_meta("__rbx_missing_asset", s);
    }
    String get_texture_str() const { return texture; }
    void set_face(int i) { face = i; }
    int get_face() const { return face; }
    void set_transparency(float f) { transparency = f; }
    float get_transparency() const { return transparency; }
    void set_color3(const Color &c) { color = c; }
    Color get_color3() const { return color; }
    void set_z_index(int i) { z_index = i; }
    int get_z_index() const { return z_index; }
};

// ── SpecialMesh / CylinderMesh / BlockMesh ──────────────────────────
// En Roblox estas mallas modifican la forma de la Part que las contiene, asi
// que al entrar al arbol se lo aplicamos al padre.
class RBXMesh : public Node3D {
    GDCLASS(RBXMesh, Node3D);

    int mesh_type = 6;            // Brick
    String mesh_id, texture_id;
    Vector3 mesh_scale = Vector3(1, 1, 1);
    Vector3 offset;
    Vector3 vertex_color = Vector3(1, 1, 1);

    void _apply_to_parent() {
        RobloxPart *p = Object::cast_to<RobloxPart>(get_parent());
        if (!p) return;
        switch (mesh_type) {
            case 3: p->set_roblox_shape(1); break;   // Sphere -> Ball
            case 4: p->set_roblox_shape(2); break;   // Cylinder
            case 2: p->set_roblox_shape(3); break;   // Wedge
            default: break;                          // FileMesh/Head/Torso: se deja la caja
        }
        if (mesh_scale != Vector3(1, 1, 1))
            p->set_size(p->get_size() * mesh_scale);
    }

protected:
    void _notification(int what) {
        if (what == NOTIFICATION_READY) _apply_to_parent();
    }

    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_mesh_type", "i"), &RBXMesh::set_mesh_type);
        ClassDB::bind_method(D_METHOD("get_mesh_type"),      &RBXMesh::get_mesh_type);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "MeshType", PROPERTY_HINT_ENUM,
                     "Head,Torso,Wedge,Sphere,Cylinder,FileMesh,Brick"),
                     "set_mesh_type", "get_mesh_type");

        ClassDB::bind_method(D_METHOD("set_mesh_id", "s"), &RBXMesh::set_mesh_id);
        ClassDB::bind_method(D_METHOD("get_mesh_id"),      &RBXMesh::get_mesh_id);
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "MeshId"), "set_mesh_id", "get_mesh_id");

        ClassDB::bind_method(D_METHOD("set_texture_id", "s"), &RBXMesh::set_texture_id);
        ClassDB::bind_method(D_METHOD("get_texture_id"),      &RBXMesh::get_texture_id);
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "TextureId"), "set_texture_id", "get_texture_id");

        ClassDB::bind_method(D_METHOD("set_mesh_scale", "v"), &RBXMesh::set_mesh_scale);
        ClassDB::bind_method(D_METHOD("get_mesh_scale"),      &RBXMesh::get_mesh_scale);
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "Scale"), "set_mesh_scale", "get_mesh_scale");

        ClassDB::bind_method(D_METHOD("set_offset", "v"), &RBXMesh::set_offset);
        ClassDB::bind_method(D_METHOD("get_offset"),      &RBXMesh::get_offset);
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "Offset"), "set_offset", "get_offset");

        ClassDB::bind_method(D_METHOD("set_vertex_color", "v"), &RBXMesh::set_vertex_color);
        ClassDB::bind_method(D_METHOD("get_vertex_color"),      &RBXMesh::get_vertex_color);
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "VertexColor"),
                     "set_vertex_color", "get_vertex_color");
    }

public:
    void set_mesh_type(int i) { mesh_type = i; }
    int get_mesh_type() const { return mesh_type; }
    void set_mesh_id(const String &s) {
        mesh_id = s;
        if (!s.is_empty()) set_meta("__rbx_missing_asset", s);
    }
    String get_mesh_id() const { return mesh_id; }
    void set_texture_id(const String &s) { texture_id = s; }
    String get_texture_id() const { return texture_id; }
    void set_mesh_scale(const Vector3 &v) { mesh_scale = v; }
    Vector3 get_mesh_scale() const { return mesh_scale; }
    void set_offset(const Vector3 &v) { offset = v; }
    Vector3 get_offset() const { return offset; }
    void set_vertex_color(const Vector3 &v) { vertex_color = v; }
    Vector3 get_vertex_color() const { return vertex_color; }
};

// ── Bone (esqueleto de MeshPart skinneado) ──────────────────────────
class RBXBone : public Node3D {
    GDCLASS(RBXBone, Node3D);

    Transform3D bone_transform;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_bone_transform", "t"), &RBXBone::set_bone_transform);
        ClassDB::bind_method(D_METHOD("get_bone_transform"),      &RBXBone::get_bone_transform);
        ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "Transform"),
                     "set_bone_transform", "get_bone_transform");
    }

public:
    void set_bone_transform(const Transform3D &t) { bone_transform = t; }
    Transform3D get_bone_transform() const { return bone_transform; }
};

#endif // RBX_INSTANCES_H
