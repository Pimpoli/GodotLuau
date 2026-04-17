#ifndef ROBLOX_WORKSPACE_H
#define ROBLOX_WORKSPACE_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>      // <-- INDISPENSABLE
#include <godot_cpp/classes/capsule_shape3d.hpp>   // <-- INDISPENSABLE
#include <godot_cpp/classes/directional_light3d.hpp>
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

using namespace godot;

class RobloxWorkspace : public Node3D {
    GDCLASS(RobloxWorkspace, Node3D);

private:
    // Genera la textura de cuadros tipo Roblox
    Ref<ImageTexture> generar_textura_grid() {
        Ref<Image> img = Image::create(64, 64, false, Image::FORMAT_RGBA8);
        Color color_fondo(0.2, 0.2, 0.2); 
        Color color_linea(0.25, 0.25, 0.25); 
        
        img->fill(color_fondo);
        for (int i = 0; i < 64; i++) {
            img->set_pixel(i, 0, color_linea);
            img->set_pixel(0, i, color_linea);
        }
        return ImageTexture::create_from_image(img);
    }

protected:
    static void _bind_methods() {}

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE && Engine::get_singleton()->is_editor_hint()) {
            if (get_child_count() == 0) {
                Node* root = get_tree()->get_edited_scene_root();
                if (!root) root = this;

                // 1. CIELO AZUL ROBLOX
                WorldEnvironment* env_node = memnew(WorldEnvironment);
                env_node->set_name("WorldEnvironment");
                Ref<Environment> env; env.instantiate();
                Ref<Sky> sky; sky.instantiate();
                Ref<ProceduralSkyMaterial> sky_mat; sky_mat.instantiate();
                
                sky_mat->set_sky_top_color(Color(0.15, 0.55, 0.95)); 
                sky_mat->set_sky_horizon_color(Color(0.7, 0.85, 0.95)); 
                sky_mat->set_ground_bottom_color(Color(0.1, 0.1, 0.1));
                sky_mat->set_ground_horizon_color(Color(0.7, 0.85, 0.95));

                sky->set_material(sky_mat);
                env->set_sky(sky);
                env->set_background(Environment::BG_SKY);
                env->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
                
                env_node->set_environment(env);
                add_child(env_node);
                env_node->set_owner(root);

                // 2. SOL
                DirectionalLight3D* sol = memnew(DirectionalLight3D);
                sol->set_name("SunLight");
                sol->set_rotation_degrees(Vector3(-45, 45, 0));
                sol->set_shadow(true);
                add_child(sol);
                sol->set_owner(root);

                // 3. BASEPLATE CON CUADRÍCULA
                StaticBody3D* bp = memnew(StaticBody3D);
                bp->set_name("BasePlate");
                add_child(bp);
                bp->set_owner(root);

                MeshInstance3D* mesh = memnew(MeshInstance3D);
                Ref<BoxMesh> m; m.instantiate();
                m->set_size(Vector3(1000, 1, 1000));
                
                Ref<StandardMaterial3D> mat; mat.instantiate();
                // Usamos el método oficial set_texture
                mat->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, generar_textura_grid());
                mat->set_uv1_scale(Vector3(100, 100, 1)); 
                mat->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_NEAREST);
                
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

                // 4. CÁMARA ACTUAL — visible en el árbol de escena como en Roblox
                // workspace.CurrentCamera es la cámara que renderiza el juego.
                // En runtime el jugador la controla; en el editor puedes ver su posición.
                Camera3D* current_cam = memnew(Camera3D);
                current_cam->set_name("CurrentCamera");
                current_cam->set_position(Vector3(0.0f, 5.0f, 10.0f));
                current_cam->set_rotation_degrees(Vector3(-15.0f, 0.0f, 0.0f));
                add_child(current_cam);
                current_cam->set_owner(root);

                UtilityFunctions::print("[GodotLuau] RobloxWorkspace inicializado en editor.");
            }
        }
    }

public:
    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;

        // ── 1. Buscar StarterPlayer (hermano del Workspace en la escena) ────────
        Node* starter_player = nullptr;
        if (get_parent()) {
            starter_player = get_parent()->get_node_or_null("StarterPlayer");
        }

        // ── 2. Crear el personaje del jugador ────────────────────────────────────
        RobloxPlayer* p = memnew(RobloxPlayer);
        p->set_name("LocalPlayer");
        p->set_position(Vector3(0, 5, 0));
        add_child(p);

        // ── 3. Personaje personalizado desde StarterCharacter ────────────────────
        bool tiene_personaje_custom = false;
        if (starter_player) {
            Node* starter_char = starter_player->get_node_or_null("StarterCharacter");
            if (starter_char && starter_char->get_child_count() > 0) {
                tiene_personaje_custom = true;
                TypedArray<Node> hijos = starter_char->get_children();
                for (int i = 0; i < hijos.size(); i++) {
                    Node* hijo = Object::cast_to<Node>(hijos[i]);
                    if (hijo) {
                        Node* clon = hijo->duplicate();
                        p->add_child(clon);
                    }
                }
                UtilityFunctions::print("[GodotLuau] Personaje personalizado cargado desde StarterCharacter.");
            }
        }

        // ── 4. Personaje predeterminado si StarterCharacter está vacío ────────────
        if (!tiene_personaje_custom) {
            MeshInstance3D* m = memnew(MeshInstance3D);
            m->set_name("Mesh");
            Ref<CapsuleMesh> cm; cm.instantiate();
            m->set_mesh(cm);
            p->add_child(m);

            CollisionShape3D* c = memnew(CollisionShape3D);
            Ref<CapsuleShape3D> cs; cs.instantiate();
            c->set_shape(cs);
            p->add_child(c);
        }

        // ── 5. Siempre garantizar Humanoid (movimiento) ──────────────────────────
        if (!p->get_node_or_null("Humanoid")) {
            Humanoid* hum = memnew(Humanoid);
            hum->set_name("Humanoid");
            p->add_child(hum);
        }

        // ── 6. Clonar StarterCharacterScripts al personaje ───────────────────────
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

            // ── 7. Clonar StarterPlayerScripts al jugador ────────────────────────
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

            UtilityFunctions::print("[GodotLuau] Scripts del jugador cargados desde StarterPlayer.");
        }
    }
};

#endif