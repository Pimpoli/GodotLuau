#ifndef ROBLOX_GAME2D_H
#define ROBLOX_GAME2D_H

// ════════════════════════════════════════════════════════════════════
//  RobloxGame2D — Root node for 2D Roblox-style games
////
//  RobloxGame2D — Nodo raíz para juegos 2D tipo Roblox
//
//  Adding it as the first scene node AUTOMATICALLY creates
//  the full Roblox service hierarchy adapted for 2D:
////
//  Al añadirlo como primer nodo de la escena, crea AUTOMÁTICAMENTE
//  toda la jerarquía de servicios Roblox adaptada para 2D:
//
//    Game (RobloxGame2D)
//    ├── Workspace        — 2D world with floor and camera / Mundo 2D con suelo y cámara
//    │   └── CurrentCamera (Camera2D)
//    ├── Players          — Player management / Gestión de jugadores
//    ├── Lighting         — World lighting / Iluminación del mundo
//    ├── ReplicatedStorage / ReplicatedFirst
//    ├── ServerScriptService
//    │   └── GameManager (ServerScript)
//    ├── StarterPlayer
//    │   ├── StarterCharacterScripts
//    │   │   ├── Health   (LocalScript)  ← Health regen / Regeneración de vida
//    │   │   └── Animate  (LocalScript)  ← Animations / Animaciones
//    │   └── StarterPlayerScripts
//    │       ├── PlayerModule (LocalScript) ← Main loader / Loader principal
//    │       └── Modules (Folder)
//    │           ├── ControlModule (ModuleScript) ← 2D movement / Movimiento 2D
//    │           └── CameraModule  (ModuleScript) ← 2D camera / Cámara 2D
//    ├── Teams / SoundService / TextChatService
//    └── ...
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

class RobloxGame2D : public Node2D {
    GDCLASS(RobloxGame2D, Node2D);

protected:
    static void _bind_methods() {}

    // ── Helper: creates a node by class name ──────────────────────────
    //// ── Helper: crea un nodo por nombre de clase ──────────────────────
    Node* make_node(const String& p_class, const String& p_name,
                    Node* p_parent, Node* root) {
        if (!p_parent) return nullptr;
        Node* existing = p_parent->get_node_or_null(NodePath(p_name));
        if (existing) return existing;

        Node* n = Object::cast_to<Node>(ClassDB::instantiate(StringName(p_class)));
        if (!n) {
            n = memnew(Node);
        }
        n->set_name(p_name);
        p_parent->add_child(n);
        n->set_owner(root);
        return n;
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE && Engine::get_singleton()->is_editor_hint()) {
            // Rename to "Game" the first time it is added
            //// Renombrar a "Game" la primera vez que se añade
            if (get_name() == StringName(get_class())) {
                set_name("Game");
            }
            // Only build the structure if empty (first time)
            //// Solo crear estructura si está vacío (primera vez)
            if (get_child_count() == 0) {
                _crear_entorno_2d();
            }
        }
    }

    void _crear_entorno_2d() {
        Node* root = get_tree()->get_edited_scene_root();
        if (!root) root = this;

        // ─── 1. WORKSPACE 2D ────────────────────────────────────────
        // RobloxWorkspace2D auto-creates: floor with collision,
        // green visual, and Camera2D "CurrentCamera" in the editor
        ////
        // RobloxWorkspace2D crea automáticamente: suelo con colisión,
        // visual verde y Camera2D "CurrentCamera" en el editor
        make_node("RobloxWorkspace2D", "Workspace", this, root);

        // ─── 2. MAIN SERVICES / SERVICIOS PRINCIPALES ───────────────
        make_node("Players",          "Players",          this, root);
        make_node("Lighting",         "Lighting",         this, root);
        make_node("MaterialService",  "MaterialService",  this, root);
        make_node("ReplicatedFirst",  "ReplicatedFirst",  this, root);
        make_node("ReplicatedStorage","ReplicatedStorage",this, root);

        // ─── 3. SERVER SERVICES / SERVICIOS DEL SERVIDOR ────────────
        Node* sss = make_node("ServerScriptService","ServerScriptService", this, root);
        if (sss) {
            make_node("ServerScript", "GameManager", sss, root);
        }
        make_node("ServerStorage", "ServerStorage", this, root);

        // ─── 4. STARTER GUI & PACK ──────────────────────────────────
        make_node("StarterGui",  "StarterGui",  this, root);
        make_node("StarterPack", "StarterPack", this, root);

        // ─── 5. STARTER PLAYER — Full Roblox structure / Estructura completa Roblox ─
        Node* sp = make_node("StarterPlayer", "StarterPlayer", this, root);
        if (sp) {
            Node* scs = make_node("StarterCharacterScripts", "StarterCharacterScripts", sp, root);
            if (scs) {
                make_node("LocalScript", "Health",  scs, root);
                make_node("LocalScript", "Animate", scs, root);
            }

            Node* sps = make_node("StarterPlayerScripts", "StarterPlayerScripts", sp, root);
            if (sps) {
                make_node("LocalScript", "PlayerModule", sps, root);

                Node* mods = make_node("Folder", "Modules", sps, root);
                if (mods) {
                    // ControlModule 2D: speeds in pixels/second
                    //// ControlModule 2D: velocidades en píxeles/segundo
                    make_node("ModuleScript", "ControlModule", mods, root);
                    make_node("ModuleScript", "CameraModule",  mods, root);
                }
            }

        }

        // ─── 6. OTHER SERVICES / OTROS SERVICIOS ────────────────────
        make_node("Teams",           "Teams",           this, root);
        make_node("SoundService",    "SoundService",    this, root);
        make_node("TextChatService", "TextChatService", this, root);

        UtilityFunctions::print(
            "[GodotLuau] ═══════════════════════════════════════\n"
            "[GodotLuau]   RobloxGame2D creado exitosamente!\n"
            "[GodotLuau]   Estructura 2D completa con módulos Luau\n"
            "[GodotLuau]   Edita los scripts en StarterPlayer/\n"
            "[GodotLuau] ═══════════════════════════════════════");
    }
};

#endif // ROBLOX_GAME2D_H
