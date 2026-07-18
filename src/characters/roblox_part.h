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
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "lua.h"
#include "lualib.h"
#include "gl_errors.h"

#include "gl_runtime.h"   // gl_state_alive, GodotObjectWrapper, gow_set

using namespace godot;

// Cache GLOBAL de materiales compartidos: todas las Parts con el mismo
// aspecto (Material, Color, Transparency, Reflectance) usan UN SOLO
// StandardMaterial3D. Sin esto, 14.000 bloques = 14.000 materiales unicos:
// el heap de descriptores de D3D12 se desborda ("Cannot create uniform set")
// y cada draw arrastra un estado distinto (lag). Asi lo agrupa Roblox.
// (static local en funcion, NO global: ver gotcha del error 1114 en gl_errors.h)
static HashMap<String, Ref<StandardMaterial3D>>& gl_shared_materials() {
    static HashMap<String, Ref<StandardMaterial3D>> cache;
    return cache;
}

class RobloxPart : public RigidBody3D {
    GDCLASS(RobloxPart, RigidBody3D);

private:
    MeshInstance3D* mesh_instance   = nullptr;
    CollisionShape3D* collision_shape = nullptr;
    Ref<StandardMaterial3D> material;
    bool material_shared = false;   // true = viene del cache global (NO mutarlo)
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
    bool    touch_monitoring = false;  // contact monitor activado bajo demanda
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
    //           23=Ground 24=Mud 25=Sandstone 26=Basalt 27=CrackedLava
    //           28=Glacier 29=LeafyGrass 30=Salt 31=Limestone 32=Pavement
    //           33=Asphalt 34=ForceField 35=Water 36=Air
    // (0-22 no se reordenan: escenas y Enum.Material existentes dependen de ellos)
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

    // Clave visual de la part: dos parts con la misma clave comparten material.
    String _material_key() const {
        return String::num_int64(roblox_material) + "|" +
               String::num_int64((int64_t)Math::round(color.r * 255.0f)) + "|" +
               String::num_int64((int64_t)Math::round(color.g * 255.0f)) + "|" +
               String::num_int64((int64_t)Math::round(color.b * 255.0f)) + "|" +
               String::num_int64((int64_t)Math::round(transparency * 255.0f)) + "|" +
               String::num_int64((int64_t)Math::round(reflectance * 255.0f));
    }

    // Textura de la Part (PERSISTIDA): la usa el Baseplate para la cuadricula
    // estilo Studio y cualquier Part puede llevar la suya. Se TIÑE con Color.
    Ref<Texture2D> part_texture;
    float part_texture_scale = 1.0f;

    void _apply_part_texture() {
        if (!material.is_valid()) return;
        if (part_texture.is_valid()) {
            material->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, part_texture);
            material->set_uv1_scale(Vector3(part_texture_scale, part_texture_scale, 1.0f));
            material->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_LINEAR_WITH_MIPMAPS);
        } else {
            material->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, Ref<Texture2D>());
        }
    }

public:
    void set_part_texture(const Ref<Texture2D>& t) { part_texture = t; _apply_material_visual(); }
    Ref<Texture2D> get_part_texture() const { return part_texture; }
    void set_part_texture_scale(float s) { part_texture_scale = Math::max(s, 0.001f); _apply_material_visual(); }
    float get_part_texture_scale() const { return part_texture_scale; }

    // Conveniencia del Baseplate (aplica textura + escala + suelo mate)
    void gl_apply_grid_texture(const Ref<Texture2D>& tex, float uv_scale) {
        part_texture = tex;
        part_texture_scale = uv_scale;
        _apply_material_visual();   // con textura propia el material es dedicado
        if (material.is_valid() && !material_shared) material->set_roughness(0.9f);
    }

private:

    void _apply_material_visual() {
        if (!mesh_instance) return;
        if (part_texture.is_valid()) {
            // Textura propia (ej. Baseplate): material DEDICADO de esta part
            if (!material.is_valid() || material_shared) {
                material.instantiate();
                material_shared = false;
            }
            _configure_material();
            mesh_instance->set_material_override(material);
            return;
        }
        // Sin textura: material COMPARTIDO por clave visual
        String key = _material_key();
        HashMap<String, Ref<StandardMaterial3D>>& cache = gl_shared_materials();
        Ref<StandardMaterial3D>* found = cache.getptr(key);
        if (found) {
            material = *found;
        } else {
            material.instantiate();
            material_shared = false;   // recien creado: configurarlo
            _configure_material();
            cache[key] = material;
        }
        material_shared = true;
        mesh_instance->set_material_override(material);
    }

    // Aplica TODO el aspecto al material actual (solo materiales no compartidos
    // o recien creados para el cache).
    void _configure_material() {
        if (!material.is_valid()) return;
        // Reset to neutral
        material->set_metallic(0.0f);
        material->set_roughness(0.55f);
        // FIX 1: Use set_feature to disable emission in Godot 4
        //// CORRECCIÓN 1: Usar set_feature para deshabilitar la emisión en Godot 4
        material->set_feature(BaseMaterial3D::FEATURE_EMISSION, false);
        // Opaco por defecto: solo las piezas realmente translucidas usan alpha.
        // Antes TODA pieza usaba TRANSPARENCY_ALPHA, lo que rompe el orden de
        // dibujado y desactiva efectos (SSAO/sombras) en piezas opacas.
        material->set_transparency(BaseMaterial3D::TRANSPARENCY_DISABLED);

        switch (roblox_material) {
            case 0:  material->set_roughness(0.55f); break; // Plastic (Roblox: ligero brillo, no super mate)
            case 1:  material->set_roughness(0.30f); break; // SmoothPlastic
            case 2:  // Neon — self-illuminated
                // FIX 2: Use set_feature to enable emission
                //// CORRECCIÓN 2: Usar set_feature para habilitar la emisión
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
                // FIX 3: Replace TRANSPARENCY_GLASS with TRANSPARENCY_ALPHA
                //// CORRECCIÓN 3: Reemplazar TRANSPARENCY_GLASS por TRANSPARENCY_ALPHA
                material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
                break;
            case 16: // Glass
                material->set_roughness(0.0f);
                material->set_metallic(0.0f);
                // FIX 4: Replace TRANSPARENCY_GLASS with TRANSPARENCY_ALPHA
                //// CORRECCIÓN 4: Reemplazar TRANSPARENCY_GLASS por TRANSPARENCY_ALPHA
                material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
                break;
            case 17: material->set_roughness(1.00f); break; // Sand
            case 18: material->set_roughness(0.95f); break; // Fabric
            case 19: material->set_roughness(0.95f); break; // Rock
            case 20: material->set_roughness(0.90f); break; // Snow
            case 21: material->set_roughness(0.95f); break; // Cobblestone
            case 22: material->set_roughness(0.85f); break; // Pebble
            case 23: material->set_roughness(1.00f); break; // Ground
            case 24: material->set_roughness(0.85f); break; // Mud (algo húmedo)
            case 25: material->set_roughness(0.95f); break; // Sandstone
            case 26: material->set_roughness(0.90f); break; // Basalt
            case 27: // CrackedLava — roca con brasas incandescentes
                material->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
                material->set_emission(Color(1.0f, 0.35f, 0.05f));
                material->set_emission_energy_multiplier(1.6f);
                material->set_roughness(0.80f);
                break;
            case 28: // Glacier — hielo compacto ligeramente translúcido
                material->set_roughness(0.10f);
                material->set_metallic(0.05f);
                break;
            case 29: material->set_roughness(0.98f); break; // LeafyGrass
            case 30: material->set_roughness(0.85f); break; // Salt
            case 31: material->set_roughness(0.90f); break; // Limestone
            case 32: material->set_roughness(0.92f); break; // Pavement
            case 33: material->set_roughness(0.95f); break; // Asphalt
            case 34: // ForceField — campo de energía translúcido y brillante
                material->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
                material->set_emission(Color(0.3f, 0.7f, 1.0f));
                material->set_emission_energy_multiplier(1.4f);
                material->set_roughness(0.20f);
                material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
                break;
            case 35: // Water — agua translúcida
                material->set_roughness(0.05f);
                material->set_metallic(0.10f);
                material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
                break;
            case 36: // Air — invisible (bloque vacío, como en Terrain)
                material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
                break;
            default: material->set_roughness(0.55f); break;
        }
        // Specular base ~0.5 (brillo tipo plastico de Roblox); reflectance lo
        // sube. Antes era set_specular(reflectance) => 0 por defecto => TODO se
        // veia mate y plano, sin reflejos especulares. Este es el cambio que
        // mas acerca las piezas al look de Roblox.
        material->set_specular(Math::clamp(0.5f + reflectance * 0.5f, 0.0f, 1.0f));
        // Activar alpha solo si la pieza es translucida (las de vidrio/hielo ya
        // lo ponen en su case).
        if (transparency > 0.001f)
            material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
        _update_albedo();
        // Materiales con transparencia inherente: conservan el Color de la pieza
        // pero fuerzan un alfa característico si el usuario no definió Transparency.
        if (roblox_material == 36) {                                  // Air: invisible
            material->set_albedo(Color(color.r, color.g, color.b, 0.0f));
        } else if (roblox_material == 35 && transparency < 0.01f) {   // Water
            material->set_albedo(Color(color.r, color.g, color.b, 0.55f));
        } else if (roblox_material == 34 && transparency < 0.01f) {   // ForceField
            material->set_albedo(Color(color.r, color.g, color.b, 0.35f));
        }
        _apply_part_texture();   // la textura persistida sobrevive re-aplicados
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

        // Reset de rotación (el Cylinder la usa; el resto debe quedar recto)
        mesh_instance->set_rotation(Vector3(0, 0, 0));
        collision_shape->set_rotation(Vector3(0, 0, 0));

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
            case 2: { // Cylinder — en Roblox apunta al eje X (Godot lo hace en Y)
                float cr = Math::min(size.y, size.z) * 0.5f;
                cyl_mesh.instantiate();
                cyl_mesh->set_top_radius(cr);
                cyl_mesh->set_bottom_radius(cr);
                cyl_mesh->set_height(size.x);
                mesh_instance->set_mesh(cyl_mesh);
                cyl_shape.instantiate();
                cyl_shape->set_radius(cr);
                cyl_shape->set_height(size.x);
                collision_shape->set_shape(cyl_shape);
                // Rotar 90° en Z para que el eje del cilindro quede en X
                mesh_instance->set_rotation(Vector3(0, 0, 1.5707964f));
                collision_shape->set_rotation(Vector3(0, 0, 1.5707964f));
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
        // Cylinder: misma convencion que _apply_shape_internal (eje X de Roblox:
        // radio por Y/Z, altura por X) — antes usaba Y y el resize lo deformaba.
        float cr = Math::min(size.y, size.z) * 0.5f;
        if (cyl_mesh.is_valid()) {
            cyl_mesh->set_top_radius(cr); cyl_mesh->set_bottom_radius(cr); cyl_mesh->set_height(size.x);
        }
        if (cyl_shape.is_valid()) { cyl_shape->set_radius(cr); cyl_shape->set_height(size.x); }
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

        // ── Textura de la Part (persistida; se tiñe con Color) ──────
        ClassDB::bind_method(D_METHOD("set_part_texture","t"), &RobloxPart::set_part_texture);
        ClassDB::bind_method(D_METHOD("get_part_texture"),     &RobloxPart::get_part_texture);
        ClassDB::bind_method(D_METHOD("set_part_texture_scale","s"), &RobloxPart::set_part_texture_scale);
        ClassDB::bind_method(D_METHOD("get_part_texture_scale"),     &RobloxPart::get_part_texture_scale);
        ADD_PROPERTY(PropertyInfo(Variant::OBJECT,"Texture",PROPERTY_HINT_RESOURCE_TYPE,"Texture2D"),
            "set_part_texture","get_part_texture");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"TextureScale",PROPERTY_HINT_RANGE,"0.01,500,0.01"),
            "set_part_texture_scale","get_part_texture_scale");

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
            "Rock:19,Snow:20,Cobblestone:21,Pebble:22,"
            "Ground:23,Mud:24,Sandstone:25,Basalt:26,CrackedLava:27,"
            "Glacier:28,LeafyGrass:29,Salt:30,Limestone:31,Pavement:32,"
            "Asphalt:33,ForceField:34,Water:35,Air:36"),
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
                add_child(mesh_instance);
                // El material lo resuelve _apply_material_visual() abajo
                // (compartido por clave visual, o dedicado si hay textura).

                collision_shape = memnew(CollisionShape3D);
                add_child(collision_shape);

                phys_mat.instantiate();
                set_physics_material_override(phys_mat);

                _apply_shape_internal(roblox_shape);
                _apply_material_visual();
                _update_phys_mat();
                _update_mass_from_density();

                set_freeze_enabled(anchored);
                // CanCollide seteado ANTES del Parent (patron Roblox: configurar
                // todo y parentear al final) debe aplicarse al crear la shape.
                collision_shape->set_disabled(!can_collide);

                if (!cast_shadow)
                    mesh_instance->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);

                // El contact monitor NO se activa aqui: con miles de parts el
                // monitoreo de contactos de Jolt es carisimo. Se activa recien
                // cuando alguien conecta Touched/TouchEnded (_ensure_touch_monitoring).
            }
            // Visibilidad por ancestro (1.15): una Part en ReplicatedStorage/
            // Lighting/Players… no se renderiza. Se re-evalua en CADA ENTER_TREE,
            // asi un reparent (p.ej. de ReplicatedStorage a Workspace) la muestra.
            _apply_ancestor_visibility();
        }
    }

    void _apply_ancestor_visibility() {
        bool hidden = gl_visual_hidden_by_ancestor(this);
        set_visible(!hidden);
        // Fuera del mundo tampoco debe colisionar (en Roblox no está en el espacio
        // físico). Al volver a Workspace se restaura el CanCollide real.
        if (collision_shape) collision_shape->set_disabled(hidden ? true : !can_collide);
    }

public:
    RobloxPart() {}

    // Activa el monitoreo de contactos SOLO cuando el juego lo necesita
    // (alguien conecto Touched/TouchEnded). Idempotente.
    void _ensure_touch_monitoring() {
        if (touch_monitoring) return;
        touch_monitoring = true;
        set_contact_monitor(true);
        set_max_contacts_reported(10);
        connect("body_entered", Callable(this, "_on_body_entered"));
        connect("body_exited",  Callable(this, "_on_body_exited"));
    }

    // Si el que entra es un personaje se reporta su HumanoidRootPart, no el
    // personaje entero: en Roblox Touched entrega una PARTE y los scripts hacen
    // `hit.Parent:FindFirstChild("Humanoid")` (1.14.11).
    void _on_body_entered(Node* body) {
        if (body && body != this && can_touch) emit_signal("Touched", gl_touch_reported_node(body));
    }
    void _on_body_exited(Node* body) {
        if (body && body != this && can_touch) emit_signal("TouchEnded", gl_touch_reported_node(body));
    }

    void _on_luau_part_touched(Node* p_hit, int p_ref, int64_t ptr_L) {
        lua_State* L = (lua_State*)ptr_L;
        // El estado Luau puede haberse cerrado (script destruido/recargado): no usarlo.
        if (!gl_state_alive(L) || !p_hit) return;
        lua_getref(L, p_ref);
        if (lua_isfunction(L, -1)) {
            int fidx = lua_gettop(L);
            lua_pushcfunction(L, gl_trace_handler, "errh");
            lua_insert(L, fidx);   // handler debajo de la funcion (captura el stack)
            GodotObjectWrapper* wrap = (GodotObjectWrapper*)lua_newuserdata(L, sizeof(GodotObjectWrapper));
            gow_set(wrap, p_hit);
            luaL_getmetatable(L, "GodotObject");
            lua_setmetatable(L, -2);
            if (lua_pcall(L, 1, 0, fidx) != LUA_OK) {
                gl_report_script_error_with_trace(L, gl_pcall_trace());
                lua_pop(L, 1);
            }
            lua_remove(L, fidx);
        } else { lua_pop(L, 1); }
    }

    // ── Anchored ───────────────────────────────────────────────────
    void set_anchored(bool a) { anchored = a; set_freeze_enabled(a); }
    bool get_anchored() const { return anchored; }

    // ── Color ──────────────────────────────────────────────────────
    void set_color(Color c) {
        color = c;
        _apply_material_visual();   // nueva clave visual -> material del cache
    }
    Color get_color() const { return color; }

    // ── Size ───────────────────────────────────────────────────────
    void set_size(Vector3 s) { size = s; _update_shape_dims(); }
    Vector3 get_size() const { return size; }

    // ── Transparency ───────────────────────────────────────────────
    void set_transparency(float t) {
        transparency = Math::clamp(t, 0.0f, 1.0f);
        _apply_material_visual();
    }
    float get_transparency() const { return transparency; }

    // ── Reflectance ────────────────────────────────────────────────
    void set_reflectance(float r) {
        reflectance = Math::clamp(r, 0.0f, 1.0f);
        _apply_material_visual();
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

// ── Part (1.15): el nombre SIN "Roblox" ──────────────────────────────────────
// El usuario ve y crea "Part", no "RobloxPart". Es una subclase fina: hereda TODO
// el comportamiento y, al ser Part IS-A RobloxPart, funciona en cada sitio que
// castea a RobloxPart. La clase base RobloxPart se registra como INTERNA (oculta
// del "Add Node" del editor) pero sigue siendo cargable, así las escenas viejas
// con type="RobloxPart" no se rompen; el actualizador las migra a Part.
class Part : public RobloxPart {
    GDCLASS(Part, RobloxPart);
protected:
    static void _bind_methods() {}
public:
    Part() {}
};

#endif // ROBLOX_PART_H