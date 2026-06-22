#ifndef ROBLOX_GAME3D_H
#define ROBLOX_GAME3D_H

// ════════════════════════════════════════════════════════════════════
//  RobloxGame3D — Root node for 3D Roblox-style games
////
//  RobloxGame3D — Nodo raíz para juegos 3D tipo Roblox
//
//  Adding it as the first scene node AUTOMATICALLY creates
//  the full Roblox service hierarchy in the editor:
////
//  Al añadirlo como primer nodo de la escena, crea AUTOMÁTICAMENTE
//  toda la jerarquía de servicios de Roblox en el editor:
//
//    Game (RobloxGame3D)
//    ├── Workspace        — 3D world with floor and sun / Mundo 3D con suelo y sol
//    │   └── CurrentCamera
//    ├── Players / RunService / Lighting / ...
//    ├── ServerScriptService
//    │   └── GameManager (ServerScript)
//    ├── StarterPlayer
//    │   ├── StarterCharacterScripts
//    │   │   ├── Health  (ServerScript)  ← Health regen / Regeneracion de vida
//    │   │   └── Animate (LocalScript)   ← Animations / Animaciones
//    │   └── StarterPlayerScripts
//    │       ├── PlayerModule (LocalScript) ← Main loader / Loader principal
//    │       └── Modules (Folder)
//    │           ├── ControlModule (ModuleScript)
//    │           │   ├── PCModule      (ModuleScript) ← Keyboard/mouse / Teclado/raton
//    │           │   ├── MobileModule  (ModuleScript) ← Touch / Tactil
//    │           │   └── ConsoleModule (ModuleScript) ← Gamepad / Mando/gamepad
//    │           ├── CameraModule  (ModuleScript) ← Camera / Camara
//    │           └── ChatModule    (ModuleScript) ← Chat in Luau / Chat en Luau
//    ├── Teams / SoundService / TextChatService
//    └── ...
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

class RobloxGame3D : public Node3D {
    GDCLASS(RobloxGame3D, Node3D);

protected:
    static void _bind_methods() {}

    // ── Helper: creates a node by class name ──────────────────────────
    // Uses ClassDB::instantiate to avoid include dependencies.
    // If the node already exists as a child of p_parent, returns it.
    ////
    // ── Helper: crea un nodo por nombre de clase ──────────────────────
    // Usa ClassDB::instantiate para no crear dependencias de include.
    // Si el nodo ya existe como hijo de p_parent, lo devuelve.
    Node* make_node(const String& p_class, const String& p_name,
                    Node* p_parent, Node* root) {
        if (!p_parent) return nullptr;
        // Already exists → return existing / Ya existe → devolver existente
        Node* existing = p_parent->get_node_or_null(NodePath(p_name));
        if (existing) return existing;

        Node* n = Object::cast_to<Node>(ClassDB::instantiate(StringName(p_class)));
        if (!n) {
            // Fallback to generic Node if class not found
            //// Fallback a Node genérico si la clase no se encuentra
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
                _crear_entorno_3d();
            }
        }
    }

    void _crear_entorno_3d() {
        Node* root = get_tree()->get_edited_scene_root();
        if (!root) root = this;

        // ─── 1. WORKSPACE 3D ────────────────────────────────────────
        // RobloxWorkspace auto-creates: sky, sun, baseplate, and
        // Camera3D "CurrentCamera" when entering the tree in the editor
        ////
        // RobloxWorkspace crea automáticamente: cielo, sol, baseplate
        // y Camera3D "CurrentCamera" al entrar al árbol en el editor
        make_node("RobloxWorkspace", "Workspace", this, root);

        // ─── 2. MAIN SERVICES / SERVICIOS PRINCIPALES ───────────────
        make_node("Players",          "Players",          this, root);
        make_node("RunService",       "RunService",       this, root);
        make_node("Lighting",         "Lighting",         this, root);
        make_node("MaterialService",  "MaterialService",  this, root);
        make_node("ReplicatedFirst",  "ReplicatedFirst",  this, root);
        make_node("ReplicatedStorage","ReplicatedStorage",this, root);

        // ─── 3. SERVER SERVICES / SERVICIOS DEL SERVIDOR ────────────
        Node* sss = make_node("ServerScriptService","ServerScriptService", this, root);
        if (sss) {
            // GameManager: server-side script template
            //// GameManager: script plantilla del servidor
            make_node("ServerScript", "GameManager", sss, root);
        }
        make_node("ServerStorage", "ServerStorage", this, root);

        // ─── 4. STARTER GUI & PACK ──────────────────────────────────
        make_node("StarterGui",  "StarterGui",  this, root);
        make_node("StarterPack", "StarterPack", this, root);

        // ─── 5. STARTER PLAYER — Full Roblox structure / Estructura completa Roblox ─
        Node* sp = make_node("StarterPlayer", "StarterPlayer", this, root);
        if (sp) {
            // StarterCharacterScripts — cloned to character on spawn
            //// StarterCharacterScripts — se clonan al personaje al spawnear
            Node* scs = make_node("StarterCharacterScripts", "StarterCharacterScripts", sp, root);
            if (scs) {
                make_node("ServerScript", "Health",  scs, root);
                make_node("LocalScript",  "Animate", scs, root);
            }

            // StarterPlayerScripts — cloned to the local player
            //// StarterPlayerScripts — se clonan al jugador local
            Node* sps = make_node("StarterPlayerScripts", "StarterPlayerScripts", sp, root);
            if (sps) {
                make_node("LocalScript", "PlayerModule", sps, root);

                Node* mods = make_node("Folder", "Modules", sps, root);
                if (mods) {
                    // ControlModule contains platform sub-modules
                    //// ControlModule contiene los sub-modulos de plataforma
                    Node* ctrl = make_node("ModuleScript", "ControlModule", mods, root);
                    if (ctrl) {
                        make_node("ModuleScript", "PCModule",      ctrl, root);
                        make_node("ModuleScript", "MobileModule",  ctrl, root);
                        make_node("ModuleScript", "ConsoleModule", ctrl, root);
                    }
                    make_node("ModuleScript", "CameraModule", mods, root);
                    make_node("ModuleScript", "ChatModule",   mods, root);
                }
            }
        }

        // ─── 6. LIGHTING FX — children under Lighting / hijos bajo Lighting ─
        Node* lit = make_node("Lighting", "Lighting", this, root);
        // (RobloxWorkspace already creates the base Lighting node; if it exists, lit != nullptr)
        //// (RobloxWorkspace ya crea el nodo Lighting base; si ya existe, lit != nullptr)
        if (lit) {
            make_node("AtmosphereNode",       "Atmosphere",      lit, root);
            make_node("LightingSkyNode",      "Sky",             lit, root);
            make_node("SunRaysNode",          "SunRays",         lit, root);
            make_node("BloomEffect",          "Bloom",           lit, root);
            make_node("BlurEffect",           "Blur",            lit, root);
            make_node("ColorCorrectionEffect","ColorCorrection",  lit, root);
            make_node("DepthOfFieldEffect",   "DepthOfField",    lit, root);
        }

        // ─── 7. OTHER SERVICES / OTROS SERVICIOS ────────────────────
        make_node("Teams",           "Teams",           this, root);
        make_node("SoundService",    "SoundService",    this, root);
        make_node("TextChatService", "TextChatService", this, root);

        UtilityFunctions::print(
            "[GodotLuau] ═══════════════════════════════════════\n"
            "[GodotLuau]   RobloxGame3D creado exitosamente!\n"
            "[GodotLuau]   Estructura 3D completa con módulos Luau\n"
            "[GodotLuau]   Edita los scripts en StarterPlayer/\n"
            "[GodotLuau] ═══════════════════════════════════════");
    }
};

#endif // ROBLOX_GAME3D_H
