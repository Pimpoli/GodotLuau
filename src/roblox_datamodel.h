#ifndef ROBLOX_DATAMODEL_H
#define ROBLOX_DATAMODEL_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

class RobloxDataModel : public Node {
    GDCLASS(RobloxDataModel, Node);

private:
    int entorno_setup = 0;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_entorno_setup", "val"), &RobloxDataModel::set_entorno_setup);
        ClassDB::bind_method(D_METHOD("get_entorno_setup"), &RobloxDataModel::get_entorno_setup);
        ClassDB::bind_method(D_METHOD("generar_entorno", "tipo"), &RobloxDataModel::generar_entorno);

        ADD_PROPERTY(PropertyInfo(Variant::INT, "Modo_Roblox_Studio", PROPERTY_HINT_ENUM, "Seleccionar...,Generar Estructura 3D,Generar Estructura 2D"), "set_entorno_setup", "get_entorno_setup");
    }

public:
    void set_entorno_setup(int p_val) {
        if (p_val > 0) {
            call_deferred(StringName("generar_entorno"), p_val);
        }
        entorno_setup = 0; 
    }

    int get_entorno_setup() const { return entorno_setup; }

    void generar_entorno(int tipo) {
        if (!Engine::get_singleton()->is_editor_hint()) return;

        set_name("Game");
        Node* root = get_tree()->get_edited_scene_root();
        if (!root) root = this;

        auto create_service = [&](const String& p_class, const String& p_name, Node* p_parent = nullptr, const String& p_source = "") -> Node* {
            if (!p_parent) p_parent = this;
            if (p_parent->has_node(NodePath(p_name))) return p_parent->get_node_or_null(NodePath(p_name));

            Node* new_node = Object::cast_to<Node>(ClassDB::instantiate(StringName(p_class)));
            if (!new_node) return nullptr;
            
            new_node->set_name(p_name);
            p_parent->add_child(new_node);
            new_node->set_owner(root);

            if (p_source != "" && (new_node->get_class() == "ModuleScript" ||
                new_node->get_class() == "LocalScript" ||
                new_node->get_class() == "ServerScript")) {
                new_node->set("source", p_source);
            }
            return new_node;
        };

        // 1. WORKSPACE (Icono añadido en .gdextension)
        Node* workspace = nullptr;
        if (tipo == 1) {
            workspace = create_service("RobloxWorkspace", "Workspace");
            create_service("RobloxPart", "Baseplate", workspace);
        } else {
            workspace = create_service("RobloxWorkspace2D", "Workspace");
            create_service("RobloxPart2D", "Baseplate", workspace);
        }

        // 2. SERVICIOS
        create_service("Players",    "Players");
        create_service("RunService", "RunService");
        create_service("Lighting",   "Lighting");
        create_service("MaterialService", "MaterialService");
        create_service("ReplicatedFirst", "ReplicatedFirst");
        create_service("ReplicatedStorage", "ReplicatedStorage");
        create_service("ServerScriptService", "ServerScriptService");
        create_service("ServerStorage", "ServerStorage");
        create_service("StarterGui", "StarterGui"); // Icono añadido abajo
        create_service("StarterPack", "StarterPack");

        // 3. STARTERPLAYER
        Node* st_player = create_service("StarterPlayer", "StarterPlayer");
        if (st_player) {
            Node* st_char_scripts = create_service("StarterCharacterScripts", "StarterCharacterScripts", st_player);
            
            // Health: ServerScript (se clona al personaje en spawn)
            create_service("ServerScript", "Health", st_char_scripts);

            Node* st_player_scripts = create_service("StarterPlayerScripts", "StarterPlayerScripts", st_player);
            // PlayerModule: LocalScript que carga ControlModule y CameraModule
            create_service("LocalScript", "PlayerModule", st_player_scripts);
            // Carpeta Modules con sub-módulos de movimiento, cámara y chat
            Node* modules_folder = create_service("Folder", "Modules", st_player_scripts);
            // ControlModule contiene los sub-módulos de plataforma como hijos
            Node* ctrl_module = create_service("ModuleScript", "ControlModule", modules_folder);
            create_service("ModuleScript", "PCModule",      ctrl_module);
            create_service("ModuleScript", "MobileModule",  ctrl_module);
            create_service("ModuleScript", "ConsoleModule", ctrl_module);
            create_service("ModuleScript", "CameraModule",  modules_folder);
            create_service("ModuleScript", "ChatModule",    modules_folder);
        }

        create_service("Teams", "Teams");
        create_service("SoundService", "SoundService");
        create_service("TextChatService", "TextChatService");
        create_service("Folder", "Folder");

        UtilityFunctions::print("[GodotLuau] Entorno 'Game' generado correctamente.");
    }
};

#endif