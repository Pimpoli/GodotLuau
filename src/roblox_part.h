#ifndef ROBLOX_PART_H
#define ROBLOX_PART_H

#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/physics_material.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "lua.h"
#include "lualib.h"

using namespace godot;

class RobloxPart : public RigidBody3D {
    GDCLASS(RobloxPart, RigidBody3D);

private:
    MeshInstance3D* mesh_instance   = nullptr;
    CollisionShape3D* collision_shape = nullptr;
    Ref<StandardMaterial3D> material;
    Ref<PhysicsMaterial>    phys_mat;

    // Active shape meshes (only one set of refs valid at a time)
    Ref<BoxMesh>         box_mesh;
    Ref<BoxShape3D>      box_shape;
    Ref<SphereMesh>      sphere_mesh;
    Ref<SphereShape3D>   sphere_shape;
    Ref<CylinderMesh>    cyl_mesh;
    Ref<CylinderShape3D> cyl_shape;

    // ── BasePart core properties ──────────────────────────────────
    bool    anchored      = true;
    Color   color         = Color(0.63f, 0.64f, 0.65f);
    Vector3 size          = Vector3(4, 1, 2);
    float   transparency  = 0.0f;
    bool    can_collide   = true;
    bool    cast_shadow   = true;
    bool    can_touch     = true;
    bool    can_query     = true;
    bool    locked        = false;
    bool    massless      = false;
    float   reflectance   = 0.0f;
    // Shape: 0=Block 1=Ball 2=Cylinder 3=Wedge 4=CornerWedge
    int     roblox_shape  = 0;
    // Material: 0=Plastic 1=SmoothPlastic 2=Neon 3=Wood 4=WoodPlanks
    //           5=Marble 6=Slate 7=Concrete 8=Granite 9=Brick
    //           10=Metal 11=CorrodedMetal 12=DiamondPlate 13=Foil
    //           14=Grass 15=Ice 16=Glass 17=Sand 18=Fabric
    //           19=Rock 20=Snow 21=Cobblestone 22=Pebble
    int     roblox_material = 0;

    // Surface types per face (0=Smooth 1=Glue 2=Weld 3=Studs 4=Inlet 5=Universal 6=Hinge 7=Motor)
    int top_surface    = 0;
    int bottom_surface = 0;
    int front_surface  = 0;
    int back_surface   = 0;
    int left_surface   = 0;
    int right_surface  = 0;

    // Physical material properties
    float density           = 0.7f;
    float friction_val      = 0.3f;
    float elasticity        = 0.0f;
    float friction_weight   = 1.0f;
    float elasticity_weight = 1.0f;

    // ── Internal helpers ──────────────────────────────────────────
    void _update_albedo() {
        if (!material.is_valid()) return;
        material->set_albedo(Color(color.r, color.g, color.b, 1.0f - transparency));
    }

    void _apply_material_visual() {
        if (!material.is_valid()) return;
        // Reset to neutral
        material->set_metallic(0.0f);
        material->set_roughness(0.8f);
        // CORRECCIÓN 1: Usar set_feature para deshabilitar la emisión en Godot 4
        material->set_feature(BaseMaterial3D::FEATURE_EMISSION, false);
        material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);

        switch (roblox_material) {
            case 0:  material->set_roughness(0.8f);  break; // Plastic
            case 1:  material->set_roughness(0.25f); break; // SmoothPlastic
            case 2:  // Neon — self-illuminated
                // CORRECCIÓN 2: Usar set_feature para habilitar la emisión
                material->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
                material->set_emission(Color(color.r, color.g, color.b));
                material->set_emission_energy_multiplier(2.0f);
                material->set_roughness(0.5f);
                break;
            case 3:  material->set_roughness(0.85f); break; // Wood
            case 4:  material->set_roughness(0.80f); break; // WoodPlanks
            case 5:  material->set_roughness(0.20f); break; // Marble
            case 6:  material->set_roughness(0.90f); break; // Slate
            case 7:  material->set_roughness(0.95f); break; // Concrete
            case 8:  material->set_roughness(0.90f); break; // Granite
            case 9:  material->set_roughness(0.95f); break; // Brick
            case 10: // Metal
                material->set_metallic(0.9f);
                material->set_roughness(0.35f);
                break;
            case 11: // CorrodedMetal
                material->set_metallic(0.4f);
                material->set_roughness(0.90f);
                break;
            case 12: // DiamondPlate
                material->set_metallic(1.0f);
                material->set_roughness(0.10f);
                break;
            case 13: // Foil
                material->set_metallic(1.0f);
                material->set_roughness(0.05f);
                break;
            case 14: material->set_roughness(0.95f); break; // Grass
            case 15: // Ice
                material->set_roughness(0.05f);
                material->set_metallic(0.05f);
                // CORRECCIÓN 3: Reemplazar TRANSPARENCY_GLASS por TRANSPARENCY_ALPHA
                material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
                break;
            case 16: // Glass
                material->set_roughness(0.0f);
                material->set_metallic(0.0f);
                // CORRECCIÓN 4: Reemplazar TRANSPARENCY_GLASS por TRANSPARENCY_ALPHA
                material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
                break;
            case 17: material->set_roughness(1.00f); break; // Sand
            case 18: material->set_roughness(0.95f); break; // Fabric
            case 19: material->set_roughness(0.95f); break; // Rock
            case 20: material->set_roughness(0.90f); break; // Snow
            case 21: material->set_roughness(0.95f); break; // Cobblestone
            case 22: material->set_roughness(0.85f); break; // Pebble
            default: material->set_roughness(0.8f);  break;
        }
        material->set_specular(reflectance);
        _update_albedo();
    }

    void _apply_shape_internal(int s) {
        if (!mesh_instance || !collision_shape) return;

        // Release all existing shape refs
        box_mesh    = Ref<BoxMesh>();
        box_shape   = Ref<BoxShape3D>();
        sphere_mesh = Ref<SphereMesh>();
        sphere_shape= Ref<SphereShape3D>();
        cyl_mesh    = Ref<CylinderMesh>();
        cyl_shape   = Ref<CylinderShape3D>();

        float r = Math::min(size.x, size.z) * 0.5f;

        switch (s) {
            case 1: { // Ball
                sphere_mesh.instantiate();
                sphere_mesh->set_radius(r);
                sphere_mesh->set_height(r * 2.0f);
                mesh_instance->set_mesh(sphere_mesh);
                sphere_shape.instantiate();
                sphere_shape->set_radius(r);
                collision_shape->set_shape(sphere_shape);
                break;
            }
            case 2: { // Cylinder
                cyl_mesh.instantiate();
                cyl_mesh->set_top_radius(r);
                cyl_mesh->set_bottom_radius(r);
                cyl_mesh->set_height(size.y);
                mesh_instance->set_mesh(cyl_mesh);
                cyl_shape.instantiate();
                cyl_shape->set_radius(r);
                cyl_shape->set_height(size.y);
                collision_shape->set_shape(cyl_shape);
                break;
            }
            default: { // Block / Wedge / CornerWedge
                box_mesh.instantiate();
                box_mesh->set_size(size);
                mesh_instance->set_mesh(box_mesh);
                box_shape.instantiate();
                box_shape->set_size(size);
                collision_shape->set_shape(box_shape);
                break;
            }
        }
    }

    void _update_shape_dims() {
        float r = Math::min(size.x, size.z) * 0.5f;
        if (box_mesh.is_valid())    box_mesh->set_size(size);
        if (box_shape.is_valid())   box_shape->set_size(size);
        if (sphere_mesh.is_valid()) { sphere_mesh->set_radius(r); sphere_mesh->set_height(r * 2.0f); }
        if (sphere_shape.is_valid()) sphere_shape->set_radius(r);
        if (cyl_mesh.is_valid()) {
            cyl_mesh->set_top_radius(r); cyl_mesh->set_bottom_radius(r); cyl_mesh->set_height(size.y);
        }
        if (cyl_shape.is_valid()) { cyl_shape->set_radius(r); cyl_shape->set_height(size.y); }
        _update_mass_from_density();
    }

    void _update_mass_from_density() {
        if (massless) { set_mass(0.001f); return; }
        float vol = size.x * size.y * size.z;
        set_mass(Math::max(density * vol, 0.001f));
    }

    void _update_phys_mat() {
        if (!phys_mat.is_valid()) return;
        phys_mat->set_friction(friction_val);
        phys_mat->set_bounce(elasticity);
    }

protected:
    static void _bind_methods() {
        // ── Anchored ────────────────────────────────────────────────
        ClassDB::bind_method(D_METHOD("set_anchored","a"), &RobloxPart::set_anchored);
        ClassDB::bind_method(D_METHOD("get_anchored"),     &RobloxPart::get_anchored);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"Anchored"),"set_anchored","get_anchored");

        // ── Color / BrickColor ──────────────────────────────────────
        ClassDB::bind_method(D_METHOD("set_color","c"), &RobloxPart::set_color);
        ClassDB::bind_method(D_METHOD("get_color"),     &RobloxPart::get_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"Color"),"set_color","get_color");
        ClassDB::bind_method(D_METHOD("set_brick_color","c"), &RobloxPart::set_color);
        ClassDB::bind_method(D_METHOD("get_brick_color"),     &RobloxPart::get_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"BrickColor"),"set_brick_color","get_brick_color");

        // ── Size ────────────────────────────────────────────────────
        ClassDB::bind_method(D_METHOD("set_size","s"), &RobloxPart::set_size);
        ClassDB::bind_method(D_METHOD("get_size"),     &RobloxPart::get_size);
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR3,"Size"),"set_size","get_size");

        // ── Transparency / Reflectance ──────────────────────────────
        ClassDB::bind_method(D_METHOD("set_transparency","t"), &RobloxPart::set_transparency);
        ClassDB::bind_method(D_METHOD("get_transparency"),     &RobloxPart::get_transparency);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Transparency",PROPERTY_HINT_RANGE,"0,1,0.01"),"set_transparency","get_transparency");

        ClassDB::bind_method(D_METHOD("set_reflectance","r"), &RobloxPart::set_reflectance);
        ClassDB::bind_method(D_METHOD("get_reflectance"),     &RobloxPart::get_reflectance);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Reflectance",PROPERTY_HINT_RANGE,"0,1,0.01"),"set_reflectance","get_reflectance");

        // ── Shape ───────────────────────────────────────────────────
        ClassDB::bind_method(D_METHOD("set_roblox_shape","s"), &RobloxPart::set_roblox_shape);
        ClassDB::bind_method(D_METHOD("get_roblox_shape"),     &RobloxPart::get_roblox_shape);
        ADD_PROPERTY(PropertyInfo(Variant::INT,"Shape",PROPERTY_HINT_ENUM,
            "Block:0,Ball:1,Cylinder:2,Wedge:3,CornerWedge:4"),
            "set_roblox_shape","get_roblox_shape");

        // ── Material ────────────────────────────────────────────────
        ClassDB::bind_method(D_METHOD("set_roblox_material","m"), &RobloxPart::set_roblox_material);
        ClassDB::bind_method(D_METHOD("get_roblox_material"),     &RobloxPart::get_roblox_material);
        ADD_PROPERTY(PropertyInfo(Variant::INT,"Material",PROPERTY_HINT_ENUM,
            "Plastic:0,SmoothPlastic:1,Neon:2,Wood:3,WoodPlanks:4,"
            "Marble:5,Slate:6,Concrete:7,Granite:8,Brick:9,"
            "Metal:10,CorrodedMetal:11,DiamondPlate:12,Foil:13,"
            "Grass:14,Ice:15,Glass:16,Sand:17,Fabric:18,"
            "Rock:19,Snow:20,Cobblestone:21,Pebble:22"),
            "set_roblox_material","get_roblox_material");

        // ── Collision flags ─────────────────────────────────────────
        ClassDB::bind_method(D_METHOD("set_can_collide","c"), &RobloxPart::set_can_collide);
        ClassDB::bind_method(D_METHOD("get_can_collide"),     &RobloxPart::get_can_collide);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"CanCollide"),"set_can_collide","get_can_collide");

        ClassDB::bind_method(D_METHOD("set_can_touch","c"), &RobloxPart::set_can_touch);
        ClassDB::bind_method(D_METHOD("get_can_touch"),     &RobloxPart::get_can_touch);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"CanTouch"),"set_can_touch","get_can_touch");

        ClassDB::bind_method(D_METHOD("set_can_query","c"), &RobloxPart::set_can_query);
        ClassDB::bind_method(D_METHOD("get_can_query"),     &RobloxPart::get_can_query);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"CanQuery"),"set_can_query","get_can_query");

        // ── Rendering flags ─────────────────────────────────────────
        ClassDB::bind_method(D_METHOD("set_cast_shadow","b"), &RobloxPart::set_cast_shadow);
        ClassDB::bind_method(D_METHOD("get_cast_shadow"),     &RobloxPart::get_cast_shadow);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"CastShadow"),"set_cast_shadow","get_cast_shadow");

        ClassDB::bind_method(D_METHOD("set_locked","b"), &RobloxPart::set_locked);
        ClassDB::bind_method(D_METHOD("get_locked"),     &RobloxPart::get_locked);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"Locked"),"set_locked","get_locked");

        ClassDB::bind_method(D_METHOD("set_massless","b"), &RobloxPart::set_massless);
        ClassDB::bind_method(D_METHOD("get_massless"),     &RobloxPart::get_massless);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"Massless"),"set_massless","get_massless");

        // ── Physical properties ─────────────────────────────────────
        ADD_GROUP("Physics","");
        ClassDB::bind_method(D_METHOD("set_density","d"),          &RobloxPart::set_density);
        ClassDB::bind_method(D_METHOD("get_density"),              &RobloxPart::get_density);
        ClassDB::bind_method(D_METHOD("set_friction_val","f"),     &RobloxPart::set_friction_val);
        ClassDB::bind_method(D_METHOD("get_friction_val"),         &RobloxPart::get_friction_val);
        ClassDB::bind_method(D_METHOD("set_elasticity","e"),       &RobloxPart::set_elasticity);
        ClassDB::bind_method(D_METHOD("get_elasticity"),           &RobloxPart::get_elasticity);
        ClassDB::bind_method(D_METHOD("set_friction_weight","w"),  &RobloxPart::set_friction_weight);
        ClassDB::bind_method(D_METHOD("get_friction_weight"),      &RobloxPart::get_friction_weight);
        ClassDB::bind_method(D_METHOD("set_elasticity_weight","w"),&RobloxPart::set_elasticity_weight);
        ClassDB::bind_method(D_METHOD("get_elasticity_weight"),    &RobloxPart::get_elasticity_weight);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Density",PROPERTY_HINT_RANGE,"0.001,100,0.001"),"set_density","get_density");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Friction",PROPERTY_HINT_RANGE,"0,2,0.01"),      "set_friction_val","get_friction_val");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Elasticity",PROPERTY_HINT_RANGE,"0,1,0.01"),    "set_elasticity","get_elasticity");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"FrictionWeight",PROPERTY_HINT_RANGE,"0,100,0.01"),"set_friction_weight","get_friction_weight");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"ElasticityWeight",PROPERTY_HINT_RANGE,"0,100,0.01"),"set_elasticity_weight","get_elasticity_weight");

        // ── Surface types ───────────────────────────────────────────
        ADD_GROUP("Surfaces","");
        static const char* SURF = "Smooth:0,Glue:1,Weld:2,Studs:3,Inlet:4,Universal:5,Hinge:6,Motor:7";
        ClassDB::bind_method(D_METHOD("set_top_surface","s"),    &RobloxPart::set_top_surface);
        ClassDB::bind_method(D_METHOD("get_top_surface"),        &RobloxPart::get_top_surface);
        ClassDB::bind_method(D_METHOD("set_bottom_surface","s"), &RobloxPart::set_bottom_surface);
        ClassDB::bind_method(D_METHOD("get_bottom_surface"),     &RobloxPart::get_bottom_surface);
        ClassDB::bind_method(D_METHOD("set_front_surface","s"),  &RobloxPart::set_front_surface);
        ClassDB::bind_method(D_METHOD("get_front_surface"),      &RobloxPart::get_front_surface);
        ClassDB::bind_method(D_METHOD("set_back_surface","s"),   &RobloxPart::set_back_surface);
        ClassDB::bind_method(D_METHOD("get_back_surface"),       &RobloxPart::get_back_surface);
        ClassDB::bind_method(D_METHOD("set_left_surface","s"),   &RobloxPart::set_left_surface);
        ClassDB::bind_method(D_METHOD("get_left_surface"),       &RobloxPart::get_left_surface);
        ClassDB::bind_method(D_METHOD("set_right_surface","s"),  &RobloxPart::set_right_surface);
        ClassDB::bind_method(D_METHOD("get_right_surface"),      &RobloxPart::get_right_surface);
        ADD_PROPERTY(PropertyInfo(Variant::INT,"TopSurface",PROPERTY_HINT_ENUM,SURF),    "set_top_surface","get_top_surface");
        ADD_PROPERTY(PropertyInfo(Variant::INT,"BottomSurface",PROPERTY_HINT_ENUM,SURF), "set_bottom_surface","get_bottom_surface");
        ADD_PROPERTY(PropertyInfo(Variant::INT,"FrontSurface",PROPERTY_HINT_ENUM,SURF),  "set_front_surface","get_front_surface");
        ADD_PROPERTY(PropertyInfo(Variant::INT,"BackSurface",PROPERTY_HINT_ENUM,SURF),   "set_back_surface","get_back_surface");
        ADD_PROPERTY(PropertyInfo(Variant::INT,"LeftSurface",PROPERTY_HINT_ENUM,SURF),   "set_left_surface","get_left_surface");
        ADD_PROPERTY(PropertyInfo(Variant::INT,"RightSurface",PROPERTY_HINT_ENUM,SURF),  "set_right_surface","get_right_surface");

        // ── Signals ─────────────────────────────────────────────────
        ADD_SIGNAL(MethodInfo("Touched",    PropertyInfo(Variant::OBJECT,"hit_node")));
        ADD_SIGNAL(MethodInfo("TouchEnded", PropertyInfo(Variant::OBJECT,"hit_node")));
        ClassDB::bind_method(D_METHOD("_on_body_entered","body"), &RobloxPart::_on_body_entered);
        ClassDB::bind_method(D_METHOD("_on_body_exited","body"),  &RobloxPart::_on_body_exited);
        ClassDB::bind_method(D_METHOD("_on_luau_part_touched","hit_node","luau_ref","lua_state_ptr"),
                             &RobloxPart::_on_luau_part_touched);
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            if (!mesh_instance) {
                mesh_instance = memnew(MeshInstance3D);
                material.instantiate();
                material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
                mesh_instance->set_material_override(material);
                add_child(mesh_instance);

                collision_shape = memnew(CollisionShape3D);
                add_child(collision_shape);

                phys_mat.instantiate();
                set_physics_material_override(phys_mat);

                _apply_shape_internal(roblox_shape);
                _apply_material_visual();
                _update_phys_mat();
                _update_mass_from_density();

                set_freeze_enabled(anchored);
                set_contact_monitor(true);
                set_max_contacts_reported(10);

                if (!cast_shadow)
                    mesh_instance->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);

                connect("body_entered", Callable(this, "_on_body_entered"));
                connect("body_exited",  Callable(this, "_on_body_exited"));
            }
        }
    }

public:
    RobloxPart() {}

    void _on_body_entered(Node* body) {
        if (body && body != this && can_touch) emit_signal("Touched", body);
    }
    void _on_body_exited(Node* body) {
        if (body && body != this && can_touch) emit_signal("TouchEnded", body);
    }

    void _on_luau_part_touched(Node* p_hit, int p_ref, int64_t ptr_L) {
        lua_State* L = (lua_State*)ptr_L;
        if (!L || !p_hit) return;
        lua_getref(L, p_ref);
        if (lua_isfunction(L, -1)) {
            struct GodotObjectWrapper { Node* node_ptr; };
            GodotObjectWrapper* wrap = (GodotObjectWrapper*)lua_newuserdata(L, sizeof(GodotObjectWrapper));
            wrap->node_ptr = p_hit;
            luaL_getmetatable(L, "GodotObject");
            lua_setmetatable(L, -2);
            if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                UtilityFunctions::print("[Luau Touched] ", lua_tostring(L, -1));
                lua_pop(L, 1);
            }
        } else { lua_pop(L, 1); }
    }

    // ── Anchored ───────────────────────────────────────────────────
    void set_anchored(bool a) { anchored = a; set_freeze_enabled(a); }
    bool get_anchored() const { return anchored; }

    // ── Color ──────────────────────────────────────────────────────
    void set_color(Color c) {
        color = c;
        if (material.is_valid()) {
            if (roblox_material == 2) material->set_emission(color); // Neon
            _update_albedo();
        }
    }
    Color get_color() const { return color; }

    // ── Size ───────────────────────────────────────────────────────
    void set_size(Vector3 s) { size = s; _update_shape_dims(); }
    Vector3 get_size() const { return size; }

    // ── Transparency ───────────────────────────────────────────────
    void set_transparency(float t) {
        transparency = Math::clamp(t, 0.0f, 1.0f);
        if (material.is_valid()) _update_albedo();
    }
    float get_transparency() const { return transparency; }

    // ── Reflectance ────────────────────────────────────────────────
    void set_reflectance(float r) {
        reflectance = Math::clamp(r, 0.0f, 1.0f);
        if (material.is_valid()) material->set_specular(reflectance);
    }
    float get_reflectance() const { return reflectance; }

    // ── Shape ──────────────────────────────────────────────────────
    void set_roblox_shape(int s) {
        roblox_shape = s;
        _apply_shape_internal(s);
        _apply_material_visual();
    }
    int get_roblox_shape() const { return roblox_shape; }

    // ── Material ───────────────────────────────────────────────────
    void set_roblox_material(int m) { roblox_material = m; _apply_material_visual(); }
    int  get_roblox_material() const { return roblox_material; }

    // ── Collision ──────────────────────────────────────────────────
    void set_can_collide(bool c) {
        can_collide = c;
        if (collision_shape) collision_shape->set_disabled(!c);
    }
    bool get_can_collide() const { return can_collide; }

    void set_can_touch(bool c)  { can_touch = c; }
    bool get_can_touch() const  { return can_touch; }
    void set_can_query(bool c)  { can_query = c; }
    bool get_can_query() const  { return can_query; }

    // ── Rendering ──────────────────────────────────────────────────
    void set_cast_shadow(bool b) {
        cast_shadow = b;
        if (mesh_instance)
            mesh_instance->set_cast_shadows_setting(
                b ? GeometryInstance3D::SHADOW_CASTING_SETTING_ON
                  : GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
    }
    bool get_cast_shadow() const { return cast_shadow; }

    void set_locked(bool b)   { locked = b; }
    bool get_locked() const   { return locked; }
    void set_massless(bool b) { massless = b; _update_mass_from_density(); }
    bool get_massless() const { return massless; }

    // ── Physical material ──────────────────────────────────────────
    void  set_density(float d)          { density = Math::max(d, 0.001f); _update_mass_from_density(); }
    float get_density() const           { return density; }
    void  set_friction_val(float f)     { friction_val = Math::max(f, 0.0f); _update_phys_mat(); }
    float get_friction_val() const      { return friction_val; }
    void  set_elasticity(float e)       { elasticity = Math::clamp(e, 0.0f, 1.0f); _update_phys_mat(); }
    float get_elasticity() const        { return elasticity; }
    void  set_friction_weight(float w)  { friction_weight = Math::max(w, 0.0f); }
    float get_friction_weight() const   { return friction_weight; }
    void  set_elasticity_weight(float w){ elasticity_weight = Math::max(w, 0.0f); }
    float get_elasticity_weight() const { return elasticity_weight; }

    float get_mass_roblox() const {
        if (massless) return 0.0f;
        return density * size.x * size.y * size.z;
    }

    // ── Surfaces ───────────────────────────────────────────────────
    void set_top_surface(int s)     { top_surface = s; }
    int  get_top_surface() const    { return top_surface; }
    void set_bottom_surface(int s)  { bottom_surface = s; }
    int  get_bottom_surface() const { return bottom_surface; }
    void set_front_surface(int s)   { front_surface = s; }
    int  get_front_surface() const  { return front_surface; }
    void set_back_surface(int s)    { back_surface = s; }
    int  get_back_surface() const   { return back_surface; }
    void set_left_surface(int s)    { left_surface = s; }
    int  get_left_surface() const   { return left_surface; }
    void set_right_surface(int s)   { right_surface = s; }
    int  get_right_surface() const  { return right_surface; }

    // ── Roblox Position/Orientation/Velocity aliases ───────────────
    void    set_position_roblox(Vector3 p)    { set_global_position(p); }
    Vector3 get_position_roblox() const       { return get_global_position(); }
    void    set_orientation_roblox(Vector3 d) { set_rotation_degrees(d); }
    Vector3 get_orientation_roblox() const    { return get_rotation_degrees(); }
    void    set_velocity_roblox(Vector3 v)    { set_linear_velocity(v); }
    Vector3 get_velocity_roblox() const       { return get_linear_velocity(); }
    void    set_angular_velocity_roblox(Vector3 v) { set_angular_velocity(v); }
    Vector3 get_angular_velocity_roblox() const    { return get_angular_velocity(); }
};

#endif // ROBLOX_PART_H