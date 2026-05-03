#ifndef ROBLOX_WORKSPACE2D_H
#define ROBLOX_WORKSPACE2D_H

#include "roblox_player2d.h"
#include "humanoid2d.h"

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/static_body2d.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>
#include <godot_cpp/classes/rectangle_shape2d.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/camera2d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ════════════════════════════════════════════════════════════════════
//  RobloxWorkspace2D — Workspace equivalent for 2D games
//
//  In the editor it creates a base floor and environment.
//  At runtime it creates the player's 2D character.
////
//  RobloxWorkspace2D — Equivalente al Workspace para juegos 2D
//
//  En el editor crea un suelo base y el ambiente.
//  En runtime crea el personaje 2D del jugador.
// ════════════════════════════════════════════════════════════════════
class RobloxWorkspace2D : public Node2D {
    GDCLASS(RobloxWorkspace2D, Node2D);

protected:
    static void _bind_methods() {}

    void _notification(int p_what) {
        // ── Editor: create base floor ────────────────────────────
        //// ── Editor: crear suelo base ──────────────────────────────
        if (p_what == NOTIFICATION_ENTER_TREE && Engine::get_singleton()->is_editor_hint()) {
            if (get_child_count() == 0) {
                Node* root = get_tree()->get_edited_scene_root();
                if (!root) root = this;

                // Base floor (BasePlate 2D)
                //// Suelo base (BasePlate 2D)
                StaticBody2D* ground = memnew(StaticBody2D);
                ground->set_name("BasePlate");
                add_child(ground);
                ground->set_owner(root);
                ground->set_position(Vector2(0.0f, 300.0f));

                CollisionShape2D* col = memnew(CollisionShape2D);
                Ref<RectangleShape2D> shape; shape.instantiate();
                shape->set_size(Vector2(2000.0f, 40.0f));
                col->set_shape(shape);
                ground->add_child(col);
                col->set_owner(root);

                ColorRect* vis = memnew(ColorRect);
                vis->set_name("GroundVisual");
                vis->set_size(Vector2(2000.0f, 40.0f));
                vis->set_position(Vector2(-1000.0f, -20.0f));
                vis->set_color(Color(0.20f, 0.56f, 0.20f)); // Grass green / Verde cesped
                ground->add_child(vis);
                vis->set_owner(root);

                // 2D Camera — visible in the scene tree like Roblox
                // Disabled by default: the character creates its own active one in _ready()
                //// Cámara 2D — visible en el árbol de escena como en Roblox
                //// Desactivada por defecto: el personaje crea la suya activa en _ready()
                Camera2D* current_cam = memnew(Camera2D);
                current_cam->set_name("CurrentCamera");
                current_cam->set_enabled(false); // Player activates their own / El jugador activa la suya
                add_child(current_cam);
                current_cam->set_owner(root);

                UtilityFunctions::print("[GodotLuau] RobloxWorkspace2D initialized in editor.");
            }
        }
    }

public:
    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;

        // ── Find StarterPlayer (sibling in the DataModel) ────────
        //// ── Buscar StarterPlayer (hermano en el DataModel) ────────
        Node* starter_player = nullptr;
        if (get_parent()) {
            starter_player = get_parent()->get_node_or_null("StarterPlayer");
        }

        // ── Create the 2D player character ───────────────────────
        //// ── Crear el personaje 2D del jugador ─────────────────────
        RobloxPlayer2D* p = memnew(RobloxPlayer2D);
        p->set_name("LocalPlayer");
        p->set_position(Vector2(200.0f, 200.0f));
        add_child(p);

        // ── Always ensure a Humanoid2D exists ────────────────────
        //// ── Siempre garantizar Humanoid2D ─────────────────────────
        if (!p->get_node_or_null("Humanoid")) {
            Humanoid2D* hum = memnew(Humanoid2D);
            hum->set_name("Humanoid");
            p->add_child(hum);
        }

        // ── Clone StarterCharacterScripts ────────────────────────
        //// ── Clonar StarterCharacterScripts ────────────────────────
        if (starter_player) {
            Node* char_scripts = starter_player->get_node_or_null("StarterCharacterScripts");
            if (char_scripts) {
                TypedArray<Node> hijos = char_scripts->get_children();
                for (int i = 0; i < hijos.size(); i++) {
                    Node* hijo = Object::cast_to<Node>(hijos[i]);
                    if (hijo) p->add_child(hijo->duplicate());
                }
            }

            // ── Clone StarterPlayerScripts ───────────────────────
            //// ── Clonar StarterPlayerScripts ──────────────────────
            Node* player_scripts = starter_player->get_node_or_null("StarterPlayerScripts");
            if (player_scripts) {
                TypedArray<Node> hijos = player_scripts->get_children();
                for (int i = 0; i < hijos.size(); i++) {
                    Node* hijo = Object::cast_to<Node>(hijos[i]);
                    if (hijo) p->add_child(hijo->duplicate());
                }
            }
            UtilityFunctions::print("[GodotLuau] 2D player scripts loaded.");
        }

        UtilityFunctions::print("[GodotLuau] RobloxWorkspace2D ready. Player created at (200, 200).");
    }
};

#endif // ROBLOX_WORKSPACE2D_H
