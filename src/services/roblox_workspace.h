#ifndef ROBLOX_WORKSPACE_H
#define ROBLOX_WORKSPACE_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include "roblox_network.h"
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>      // <-- INDISPENSABLE
#include <godot_cpp/classes/capsule_shape3d.hpp>   // <-- INDISPENSABLE
#include <godot_cpp/classes/resource_loader.hpp>   // malla de avatar por defecto
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>      // personaje del usuario (.glb cara arreglada)
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/light3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/classes/procedural_sky_material.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include "roblox_player.h"
#include "humanoid.h"
#include "roblox_services.h"
#include "roblox_interactive.h"

using namespace godot;

class RobloxWorkspace : public Node3D {
    GDCLASS(RobloxWorkspace, Node3D);

private:
    // Roblox Workspace properties
    float gravity                      = 196.2f;
    float fallen_parts_destroy_height  = -500.0f;
    bool  streaming_enabled            = false;
    float air_density                  = 0.0f;
    bool  touches_use_collision_groups = false;

    void _apply_gravity() {
        if (!is_inside_tree()) return;
        Viewport* vp = get_viewport();
        if (!vp) return;
        Ref<World3D> world = vp->find_world_3d();
        if (!world.is_valid()) return;
        RID space = world->get_space();
        if (space.is_valid())
            PhysicsServer3D::get_singleton()->area_set_param(
                space, PhysicsServer3D::AREA_PARAM_GRAVITY, Variant(gravity));
    }

    // Generates the Roblox-style grid texture
    //// Genera la textura de cuadros tipo Roblox
    Ref<ImageTexture> generar_textura_grid() {
        Ref<Image> img = Image::create(64, 64, false, Image::FORMAT_RGBA8);
        Color color_fondo(0.64, 0.635, 0.647);  // gris piedra medio de Roblox (RGB 163,162,165)
        Color color_linea(0.5, 0.5, 0.52);       // lineas de stud, mas oscuras para que se vean
        
        img->fill(color_fondo);
        for (int i = 0; i < 64; i++) {
            img->set_pixel(i, 0, color_linea);
            img->set_pixel(0, i, color_linea);
        }
        return ImageTexture::create_from_image(img);
    }

    // Aplica el look de iluminacion estilo Roblox "Future" a un Environment + sol:
    // tonemapping filmico (antes era Linear plano), glow sutil (bloom real para
    // Neon y brillos), SSAO (profundidad barata), luz ambiental del cielo y
    // sombras suaves del sol (PCSS, Forward+). Idempotente: se llama al crear la
    // escena en el editor Y en runtime sobre un Environment ya existente, asi el
    // look mejora tambien en proyectos viejos sin recrear la escena.
    void _apply_roblox_render(const Ref<Environment>& env, DirectionalLight3D* sun) {
        if (env.is_valid()) {
            env->set_tonemapper(Environment::TONE_MAPPER_ACES);
            env->set_tonemap_white(6.0f);
            env->set_tonemap_exposure(1.0f);

            env->set_glow_enabled(true);
            env->set_glow_blend_mode(Environment::GLOW_BLEND_MODE_SCREEN);
            env->set_glow_intensity(0.5f);
            env->set_glow_strength(1.0f);
            env->set_glow_bloom(0.1f);
            env->set_glow_hdr_bleed_threshold(0.95f);
            env->set_glow_hdr_bleed_scale(2.0f);
            env->set_glow_level(2, 0.6f);
            env->set_glow_level(3, 0.4f);
            env->set_glow_level(4, 0.2f);

            env->set_ssao_enabled(true);
            env->set_ssao_radius(1.0f);
            env->set_ssao_intensity(1.5f);
            env->set_ssao_power(1.5f);

            env->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
            env->set_ambient_light_sky_contribution(1.0f);
            env->set_ambient_light_energy(1.0f);
        }
        if (sun) {
            sun->set_shadow(true);
            sun->set_param(Light3D::PARAM_ENERGY, 1.0f);
            sun->set_param(Light3D::PARAM_SIZE, 1.5f);              // sombras suaves PCSS (Forward+)
            sun->set_param(Light3D::PARAM_SHADOW_BLUR, 1.0f);
            sun->set_param(Light3D::PARAM_SHADOW_NORMAL_BIAS, 2.0f);
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_gravity","g"),                      &RobloxWorkspace::set_gravity);
        ClassDB::bind_method(D_METHOD("get_gravity"),                          &RobloxWorkspace::get_gravity);
        ClassDB::bind_method(D_METHOD("set_fallen_parts_destroy_height","h"),  &RobloxWorkspace::set_fallen_parts_destroy_height);
        ClassDB::bind_method(D_METHOD("get_fallen_parts_destroy_height"),      &RobloxWorkspace::get_fallen_parts_destroy_height);
        ClassDB::bind_method(D_METHOD("set_streaming_enabled","b"),            &RobloxWorkspace::set_streaming_enabled);
        ClassDB::bind_method(D_METHOD("get_streaming_enabled"),                &RobloxWorkspace::get_streaming_enabled);
        ClassDB::bind_method(D_METHOD("set_air_density","d"),                  &RobloxWorkspace::set_air_density);
        ClassDB::bind_method(D_METHOD("get_air_density"),                      &RobloxWorkspace::get_air_density);
        ClassDB::bind_method(D_METHOD("set_touches_use_collision_groups","b"), &RobloxWorkspace::set_touches_use_collision_groups);
        ClassDB::bind_method(D_METHOD("get_touches_use_collision_groups"),     &RobloxWorkspace::get_touches_use_collision_groups);

        ADD_GROUP("Mundo","");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Gravity",PROPERTY_HINT_RANGE,"0,1000,0.1"),
            "set_gravity","get_gravity");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"FallenPartsDestroyHeight",PROPERTY_HINT_RANGE,"-10000,0,1"),
            "set_fallen_parts_destroy_height","get_fallen_parts_destroy_height");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"StreamingEnabled"),
            "set_streaming_enabled","get_streaming_enabled");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"AirDensity",PROPERTY_HINT_RANGE,"0,10,0.01"),
            "set_air_density","get_air_density");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"TouchesUseCollisionGroups"),
            "set_touches_use_collision_groups","get_touches_use_collision_groups");
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE && Engine::get_singleton()->is_editor_hint()) {
            if (get_child_count() == 0) {
                Node* root = get_tree()->get_edited_scene_root();
                if (!root) root = this;

                // 1. ROBLOX BLUE SKY
                //// 1. CIELO AZUL ROBLOX
                WorldEnvironment* env_node = memnew(WorldEnvironment);
                env_node->set_name("WorldEnvironment");
                Ref<Environment> env; env.instantiate();
                Ref<Sky> sky; sky.instantiate();
                Ref<ProceduralSkyMaterial> sky_mat; sky_mat.instantiate();
                
                // Colores del cielo clasico de Roblox (azul suave, no tan saturado):
                // top RGB(78,150,205), horizonte RGB(170,205,230), suelo RGB(135,140,150)
                sky_mat->set_sky_top_color(Color(0.306, 0.588, 0.804));
                sky_mat->set_sky_horizon_color(Color(0.667, 0.804, 0.902));
                sky_mat->set_ground_bottom_color(Color(0.529, 0.549, 0.588));
                sky_mat->set_ground_horizon_color(Color(0.667, 0.804, 0.902));

                sky->set_material(sky_mat);
                env->set_sky(sky);
                env->set_background(Environment::BG_SKY);
                env->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
                // Mas relleno ambiental para que los lados en sombra (p.ej. el
                // personaje) no salgan casi negros.
                env->set_ambient_light_energy(1.15);
                
                env_node->set_environment(env);
                add_child(env_node);
                env_node->set_owner(root);

                // 2. SUN
                //// 2. SOL
                DirectionalLight3D* sol = memnew(DirectionalLight3D);
                sol->set_name("SunLight");
                sol->set_rotation_degrees(Vector3(-45, 45, 0));
                sol->set_shadow(true);
                add_child(sol);
                sol->set_owner(root);

                // Look de iluminacion estilo Roblox "Future" sobre el entorno + sol
                _apply_roblox_render(env, sol);

                // 3. BASEPLATE WITH GRID
                //// 3. BASEPLATE CON CUADRÍCULA
                StaticBody3D* bp = memnew(StaticBody3D);
                bp->set_name("BasePlate");
                add_child(bp);
                bp->set_owner(root);

                MeshInstance3D* mesh = memnew(MeshInstance3D);
                Ref<BoxMesh> m; m.instantiate();
                m->set_size(Vector3(1000, 1, 1000));
                
                Ref<StandardMaterial3D> mat; mat.instantiate();
                // Use the official set_texture method
                //// Usamos el método oficial set_texture
                mat->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, generar_textura_grid());
                mat->set_uv1_scale(Vector3(100, 100, 1));
                mat->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_NEAREST);
                // Suelo MATE como Roblox (antes roughness 0.0 = suelo pulido/espejo)
                mat->set_roughness(0.9f);
                mat->set_metallic(0.0f);
                
                mesh->set_mesh(m);
                mesh->set_material_override(mat);
                bp->add_child(mesh);
                mesh->set_owner(root);

                CollisionShape3D* col = memnew(CollisionShape3D);
                Ref<BoxShape3D> s; s.instantiate();
                s->set_size(Vector3(1000, 1, 1000));
                col->set_shape(s);
                bp->add_child(col);
                col->set_owner(root);

                // 4. CURRENT CAMERA — visible in the scene tree like Roblox
                // workspace.CurrentCamera is the camera that renders the game.
                // At runtime the player controls it; in the editor you can see its position.
                //// 4. CÁMARA ACTUAL — visible en el árbol de escena como en Roblox
                //// workspace.CurrentCamera es la cámara que renderiza el juego.
                //// En runtime el jugador la controla; en el editor puedes ver su posición.
                Camera3D* current_cam = memnew(Camera3D);
                current_cam->set_name("CurrentCamera");
                current_cam->set_position(Vector3(0.0f, 5.0f, 10.0f));
                current_cam->set_rotation_degrees(Vector3(-15.0f, 0.0f, 0.0f));
                add_child(current_cam);
                current_cam->set_owner(root);

                GL_DEBUG_PRINT("[GodotLuau] RobloxWorkspace initialized in editor.");
            }
        }
    }

public:
    // ── Getters / Setters (properties match Roblox Workspace API) ─
    void  set_gravity(float g)                     { gravity = Math::max(g, 0.0f); _apply_gravity(); }
    float get_gravity() const                      { return gravity; }
    void  set_fallen_parts_destroy_height(float h) { fallen_parts_destroy_height = h; }
    float get_fallen_parts_destroy_height() const  { return fallen_parts_destroy_height; }
    void  set_streaming_enabled(bool b)            { streaming_enabled = b; }
    bool  get_streaming_enabled() const            { return streaming_enabled; }
    void  set_air_density(float d)                 { air_density = Math::max(d, 0.0f); }
    float get_air_density() const                  { return air_density; }
    void  set_touches_use_collision_groups(bool b) { touches_use_collision_groups = b; }
    bool  get_touches_use_collision_groups() const { return touches_use_collision_groups; }

    SpawnLocation* _find_spawn_r(Node* n) {
        if (!n) return nullptr;
        if (SpawnLocation* s = Object::cast_to<SpawnLocation>(n)) return s;
        for (int i = 0; i < n->get_child_count(); i++)
            if (SpawnLocation* f = _find_spawn_r(n->get_child(i))) return f;
        return nullptr;
    }

    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;

        _apply_gravity();

        // Aplicar el look de iluminacion Roblox al entorno YA existente: las
        // escenas viejas no se recrean (su WorldEnvironment se guardo con los
        // ajustes antiguos), pero al jugar deben verse con tonemap/glow/SSAO/
        // sombras suaves igual. Idempotente.
        {
            WorldEnvironment* we = nullptr;
            DirectionalLight3D* sun = nullptr;
            for (int i = 0; i < get_child_count(); i++) {
                Node* c = get_child(i);
                if (!we)  we  = Object::cast_to<WorldEnvironment>(c);
                if (!sun) sun = Object::cast_to<DirectionalLight3D>(c);
            }
            Ref<Environment> env = we ? we->get_environment() : Ref<Environment>();
            if (env.is_valid() || sun) _apply_roblox_render(env, sun);
        }

        // ── RunService oculto: como en Roblox, existe pero no se ve en el editor ──
        if (get_parent() && !get_parent()->get_node_or_null("RunService")) {
            Node* rs = Object::cast_to<Node>(ClassDB::instantiate(StringName("RunService")));
            // add_child diferido: en _ready el padre (game) aun esta montando sus
            // hijos y Godot rechaza un add_child directo ("Parent node is busy").
            // El deferred corre antes que los scripts (iniciar_corrutina tambien
            // es diferido y se encola despues), asi RunService existe a tiempo.
            if (rs) { rs->set_name("RunService"); get_parent()->call_deferred("add_child", rs); }
        }

        // ── NetworkService: SOLO se auto-crea si el juego se lanzó desde la barra
        //    "Jugadores" del editor (multijugador local). Los juegos normales no
        //    reciben un servicio de red que no pidieron. ──
        if (gl_mp_autostart_requested() && get_parent() && !get_parent()->get_node_or_null("NetworkService")) {
            Node* ns = Object::cast_to<Node>(ClassDB::instantiate(StringName("NetworkService")));
            if (ns) { ns->set_name("NetworkService"); get_parent()->call_deferred("add_child", ns); }
        }

        // ── Menú de ajustes (Esc), como el de Roblox: existe en TODO juego ──
        if (get_parent() && !get_parent()->get_node_or_null("SettingsMenu")) {
            Node* sm = Object::cast_to<Node>(ClassDB::instantiate(StringName("GLSettingsMenu")));
            if (sm) { sm->set_name("SettingsMenu"); get_parent()->call_deferred("add_child", sm); }
        }

        // ── 1. Find StarterPlayer (sibling of Workspace in the scene) ──────────
        //// ── 1. Buscar StarterPlayer (hermano del Workspace en la escena) ────────
        Node* starter_player = nullptr;
        if (get_parent()) {
            starter_player = get_parent()->get_node_or_null("StarterPlayer");
        }

        // ── 2. Create the player character ──────────────────────────────────────
        //// ── 2. Crear el personaje del jugador ────────────────────────────────────
        RobloxPlayer* p = memnew(RobloxPlayer);
        p->set_name("LocalPlayer");
        // Spawnear sobre el primer SpawnLocation del Workspace (como Roblox)
        Vector3 spawn_pos(0, 5, 0);
        if (SpawnLocation* sl = _find_spawn_r(this)) spawn_pos = sl->get_spawn_position();
        p->set_position(spawn_pos);
        add_child(p);

        // ── 3. Default capsule character (same as Roblox) ──────────────────────
        //// ── 3. Personaje cápsula predeterminado (igual que Roblox) ──────────────────
        {
            // Visual: el MODELO DEL USUARIO con la cara arreglada
            // (AvatarR15_face.glb: misma forma, cuerpo gris limpio + cara en la
            // cabeza). Si falta, capsula gris. La FISICA sigue siendo la capsula.
            ResourceLoader* rl = ResourceLoader::get_singleton();
            const String glb_path = "res://assets/avatars/AvatarR15_face.glb";
            Node3D* character = nullptr;
            if (rl && rl->exists(glb_path)) {
                Ref<Resource> res = rl->load(glb_path);
                Ref<PackedScene> scene = Ref<PackedScene>(Object::cast_to<PackedScene>(res.ptr()));
                if (scene.is_valid())
                    character = Object::cast_to<Node3D>(scene->instantiate());
            }
            if (character) {
                character->set_name("Character");
                // El modelo mide ~5.19 de alto con los pies en y=-3: lo escalamos
                // a ~2 (altura de la capsula) y dejamos los pies en y=-1.
                float s = 2.0f / 5.187f;
                character->set_scale(Vector3(s, s, s));
                character->set_position(Vector3(0, -1.0f + 3.0f * s, 0));
                p->add_child(character);
            } else {
                MeshInstance3D* m = memnew(MeshInstance3D);
                m->set_name("Mesh");
                Ref<CapsuleMesh> cm; cm.instantiate();
                m->set_mesh(cm);
                Ref<StandardMaterial3D> mat; mat.instantiate();
                mat->set_albedo(Color(0.55, 0.55, 0.55));
                mat->set_roughness(0.95);
                m->set_material_override(mat);
                p->add_child(m);
            }

            CollisionShape3D* c = memnew(CollisionShape3D);
            Ref<CapsuleShape3D> cs; cs.instantiate();
            c->set_shape(cs);
            p->add_child(c);
        }

        // ── 4. Always ensure a Humanoid exists (movement) ───────────────────────
        //// ── 4. Siempre garantizar Humanoid (movimiento) ──────────────────────────
        if (!p->get_node_or_null("Humanoid")) {
            Humanoid* hum = memnew(Humanoid);
            hum->set_name("Humanoid");
            p->add_child(hum);
        }

        // ── 5. Clone StarterCharacterScripts onto the character ─────────────────
        //// ── 5. Clonar StarterCharacterScripts al personaje ───────────────────────
        if (starter_player) {
            Node* char_scripts = starter_player->get_node_or_null("StarterCharacterScripts");
            if (char_scripts) {
                TypedArray<Node> hijos = char_scripts->get_children();
                for (int i = 0; i < hijos.size(); i++) {
                    Node* hijo = Object::cast_to<Node>(hijos[i]);
                    if (hijo) {
                        p->add_child(hijo->duplicate());
                    }
                }
            }

            // ── 7. Clone StarterPlayerScripts onto the player ───────────────────
            //// ── 7. Clonar StarterPlayerScripts al jugador ────────────────────────
            Node* player_scripts = starter_player->get_node_or_null("StarterPlayerScripts");
            if (player_scripts) {
                TypedArray<Node> hijos = player_scripts->get_children();
                for (int i = 0; i < hijos.size(); i++) {
                    Node* hijo = Object::cast_to<Node>(hijos[i]);
                    if (hijo) {
                        p->add_child(hijo->duplicate());
                    }
                }
            }

            GL_DEBUG_PRINT("[GodotLuau] Player scripts loaded from StarterPlayer.");
        }
    }
};

#endif