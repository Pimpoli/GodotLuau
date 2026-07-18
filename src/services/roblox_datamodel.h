#ifndef ROBLOX_DATAMODEL_H
#define ROBLOX_DATAMODEL_H

// RobloxTemplate — root node that generates the full Roblox structure.
// Provides a property-driven generator in the editor.
// NOTE: node/class names ALWAYS stay in English (systems reference them);
// only UI texts are localized.
////
// RobloxTemplate — nodo raíz que genera la estructura Roblox completa.
// Expone un generador mediante una propiedad en el editor.
// NOTA: los nombres de nodos/clases SIEMPRE quedan en inglés (los sistemas
// los referencian); solo se traducen los textos de la interfaz.

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "gl_debug.h"
// gl_builtin_template / gl_is_managed_template / gl_tpl_base_* + LuauScript.
// luau_script.h ya se incluye antes que este header en register_types.cpp (guard
// evita doble inclusión); lo pedimos explícitamente para el actualizador seguro.
#include "../core/luau_script.h"

using namespace godot;

class RobloxTemplate : public Node {
    GDCLASS(RobloxTemplate, Node);

private:
    int entorno_setup = 0;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_entorno_setup", "val"), &RobloxTemplate::set_entorno_setup);
        ClassDB::bind_method(D_METHOD("get_entorno_setup"), &RobloxTemplate::get_entorno_setup);
        ClassDB::bind_method(D_METHOD("generar_entorno", "tipo"), &RobloxTemplate::generar_entorno);

        ClassDB::bind_method(D_METHOD("_gl_update_structure"), &RobloxTemplate::_gl_update_structure);

        // El "Seleccionar..." se traduce según el idioma del sistema;
        // los nombres de las plantillas quedan en inglés a propósito. La 3ª opción
        // (1.15) es el ACTUALIZADOR: rellena lo que falte y migra nodos sin borrar
        // ni pisar la configuración del usuario.
        String hint = gl_tr3("Select...", "Seleccionar...", "Selecionar...") +
                      String(",Game 3D,Game 2D,") +
                      gl_tr3("Update structure (keep my setup)",
                             "Actualizar estructura (mantener mi config)",
                             "Atualizar estrutura (manter config)");
        ADD_PROPERTY(PropertyInfo(Variant::INT, "Modo_Roblox_Studio", PROPERTY_HINT_ENUM, hint),
                     "set_entorno_setup", "get_entorno_setup");
    }

public:
    void set_entorno_setup(int p_val) {
        // 1 = Game 3D, 2 = Game 2D, 3 = Actualizar estructura (1.15)
        if (p_val == 3) {
            call_deferred(StringName("_gl_update_structure"));
        } else if (p_val > 0) {
            call_deferred(StringName("generar_entorno"), p_val);
        }
        entorno_setup = 0;
    }

    int get_entorno_setup() const { return entorno_setup; }

    // Actualizador incremental (1.15): rellena los nodos que falten y aplica las
    // migraciones SIN borrar el Game ni pisar lo que el usuario tenga dentro.
    // No está gateado a is_editor_hint para poder verificarlo también en headless;
    // en el editor lo dispara la opción "Actualizar estructura" del inspector.
    void _gl_update_structure() {
        // Detectar 3D vs 2D por la clase del Workspace ya presente.
        int tipo = 1;
        if (Node* ws = get_node_or_null(NodePath("Workspace"))) {
            if (ws->is_class("RobloxWorkspace2D")) tipo = 2;
        }
        _ensure_structure(tipo);
        int migrated = _gl_migrate_tree(this);
        // Merge seguro de contenido de scripts de fábrica (1.15): actualiza los
        // que el jugador no tocó y ofrece en Game/ScriptUpdate los que sí tocó.
        int synced = _gl_sync_managed_scripts();
        GL_DEBUG_PRINT("[GodotLuau] Estructura actualizada (tipo=", tipo, "), nodos migrados=",
                       migrated, ", scripts revisados=", synced, ".");
    }

    void generar_entorno(int tipo) {
        if (!Engine::get_singleton()->is_editor_hint()) return;
        _ensure_structure(tipo);
        GL_DEBUG_PRINT("[GodotLuau] 'Game' environment generated successfully.");
    }

private:
    // Devuelve el nodo raíz para set_owner: el de la escena editada (editor) o la
    // escena en ejecución (runtime), con fallback a this.
    Node* _owner_root() {
        SceneTree* st = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
        if (st) {
            if (Node* er = st->get_edited_scene_root()) return er;
            if (Node* cs = st->get_current_scene())     return cs;
        }
        return this;
    }

    // Construye/rellena la estructura de forma IDEMPOTENTE (reutiliza lo que ya
    // exista). Compartido por la generación inicial y el actualizador.
    void _ensure_structure(int tipo) {
        set_name("Game");
        Node* root = _owner_root();

        // Helper lambda: creates a service node; reuses it if already present
        //// Lambda helper: crea un nodo de servicio; lo reutiliza si ya existe
        auto create_service = [&](const String& p_class, const String& p_name, Node* p_parent = nullptr, const String& p_source = "") -> Node* {
            if (!p_parent) p_parent = this;
            if (p_parent->has_node(NodePath(p_name))) return p_parent->get_node_or_null(NodePath(p_name));

            Node* new_node = Object::cast_to<Node>(ClassDB::instantiate(StringName(p_class)));
            if (!new_node) return nullptr;

            new_node->set_name(p_name);
            p_parent->add_child(new_node);
            if (root) new_node->set_owner(root);

            // Inject source code if the node is a script type
            //// Inyectar código fuente si el nodo es un tipo de script
            if (p_source != "" && (new_node->get_class() == "ModuleScript" ||
                new_node->get_class() == "LocalScript" ||
                new_node->get_class() == "ServerScript")) {
                new_node->set("source", p_source);
            }
            return new_node;
        };

        // 1. WORKSPACE (icon registered in .gdextension)
        //// 1. WORKSPACE (Icono añadido en .gdextension)
        Node* workspace = nullptr;
        if (tipo == 1) {
            workspace = create_service("RobloxWorkspace", "Workspace");
            // Como en Roblox: el mundo arranca con un SpawnLocation
            // (la placa visual se auto-construye al entrar al arbol)
            create_service("SpawnLocation", "SpawnLocation", workspace);
        } else {
            workspace = create_service("RobloxWorkspace2D", "Workspace");
            create_service("RobloxPart2D", "Baseplate", workspace);
        }

        // 2. SERVICES / SERVICIOS
        // NOTA: RunService NO se crea aqui — como en Roblox no aparece en el
        // explorer; el Workspace lo crea oculto en runtime.
        create_service("Players",    "Players");
        create_service("Lighting",   "Lighting");
        create_service("MaterialService", "MaterialService");
        create_service("ReplicatedFirst", "ReplicatedFirst");
        create_service("ReplicatedStorage", "ReplicatedStorage");
        create_service("ServerScriptService", "ServerScriptService");
        create_service("ServerStorage", "ServerStorage");
        create_service("StarterGui", "StarterGui"); // icon added below / Icono añadido abajo
        create_service("StarterPack", "StarterPack");

        // 3. STARTERPLAYER
        Node* st_player = create_service("StarterPlayer", "StarterPlayer");
        if (st_player) {
            Node* st_char_scripts = create_service("StarterCharacterScripts", "StarterCharacterScripts", st_player);

            // Health: ServerScript (cloned to character on spawn)
            //// Health: ServerScript (se clona al personaje en spawn)
            create_service("ServerScript", "Health", st_char_scripts);

            Node* st_player_scripts = create_service("StarterPlayerScripts", "StarterPlayerScripts", st_player);
            // PlayerModule: LocalScript that loads ControlModule and CameraModule
            //// PlayerModule: LocalScript que carga ControlModule y CameraModule
            create_service("LocalScript", "PlayerModule", st_player_scripts);
            // Modules folder with movement, camera, and chat sub-modules
            //// Carpeta Modules con sub-módulos de movimiento, cámara y chat
            Node* modules_folder = create_service("Folder", "Modules", st_player_scripts);
            // ControlModule contains platform sub-modules as children
            //// ControlModule contiene los sub-módulos de plataforma como hijos
            Node* ctrl_module = create_service("ModuleScript", "ControlModule", modules_folder);
            create_service("ModuleScript", "PCModule",      ctrl_module);
            create_service("ModuleScript", "MobileModule",  ctrl_module);
            create_service("ModuleScript", "ConsoleModule", ctrl_module);
            create_service("ModuleScript", "CameraModule",  modules_folder);
            create_service("ModuleScript", "ChatModule",    modules_folder);
            // Menu (1.15): menú de Escape modular y EDITABLE. Menu principal con
            // submódulos MenuUi (todas las UIs), Settings (Config) y Players
            // (Personas). Reemplaza al antiguo SettingsModule único.
            Node* menu = create_service("ModuleScript", "Menu", modules_folder);
            create_service("ModuleScript", "MenuUi",   menu);
            create_service("ModuleScript", "Settings", menu);
            create_service("ModuleScript", "Players",  menu);
        }

        create_service("Teams", "Teams");
        create_service("SoundService", "SoundService");
        create_service("TextChatService", "TextChatService");
    }

    // ── Migración de árbol (1.15) ────────────────────────────────────────
    // Tabla de renombres de CLASE. La máquina que necesita F5.2: cuando una
    // clase cambia de nombre entre versiones, los nodos viejos se convierten a la
    // nueva clase conservando nombre, propiedades e hijos, en vez de romper la
    // escena. Hoy va vacía; se rellena cuando se renombre algo (ver F5.2).
    struct ClassRename { const char* from; const char* to; };
    static const ClassRename* _class_renames(int& count) {
        static const ClassRename table[] = {
            { "RobloxPart", "Part" },   // 1.15: migra nodos viejos al nombre limpio
        };
        count = (int)(sizeof(table) / sizeof(table[0]));
        return table;
    }

    // Cambia la clase de un nodo conservando nombre, hijos, dueño y las
    // propiedades que ambas clases comparten. Devuelve el nodo nuevo (o el
    // mismo si no se pudo). Es la primitiva de migración de F5.2.
    Node* _gl_swap_node_class(Node* old, const String& new_class) {
        if (!old) return old;
        Node* parent = old->get_parent();
        if (!parent) return old;
        Node* fresh = Object::cast_to<Node>(ClassDB::instantiate(StringName(new_class)));
        if (!fresh) return old;

        fresh->set_name(old->get_name());
        // Copiar las propiedades que la clase NUEVA reconoce y no son de solo
        // lectura (evita perder Size/Color/Anchored/etc. del usuario).
        TypedArray<Dictionary> props = fresh->get_property_list();
        for (int i = 0; i < props.size(); i++) {
            Dictionary pd = props[i];
            String pname = pd.get("name", "");
            int usage = (int)pd.get("usage", 0);
            if (pname.is_empty() || !(usage & PROPERTY_USAGE_STORAGE)) continue;
            fresh->set(pname, old->get(pname));
        }
        int idx = old->get_index();
        Node* owner = old->get_owner();
        // Mover SOLO los hijos del usuario (con dueño). Los hijos internos que la
        // clase auto-crea (p.ej. la malla y la colisión de una Part) no tienen
        // dueño y la clase nueva los recrea sola — moverlos los duplicaría.
        TypedArray<Node> kids = old->get_children();
        for (int i = 0; i < kids.size(); i++) {
            Node* ch = Object::cast_to<Node>(kids[i]);
            if (!ch || ch->get_owner() == nullptr) continue;
            old->remove_child(ch);
            fresh->add_child(ch);
        }
        parent->remove_child(old);
        parent->add_child(fresh);
        parent->move_child(fresh, idx);
        if (owner) {
            fresh->set_owner(owner);
            // Re-asignar dueño a los hijos migrados para que se guarden en la escena.
            _reassign_owner_recursive(fresh, owner);
        }
        old->queue_free();
        return fresh;
    }

    void _reassign_owner_recursive(Node* n, Node* owner) {
        for (int i = 0; i < n->get_child_count(); i++) {
            Node* c = n->get_child(i);
            if (!c) continue;
            c->set_owner(owner);
            _reassign_owner_recursive(c, owner);
        }
    }

    // Recorre el árbol aplicando la tabla de renombres. Devuelve cuántos migró.
    int _gl_migrate_tree(Node* n) {
        int count = 0, nrenames = 0;
        const ClassRename* table = _class_renames(nrenames);
        // Copiar hijos: _gl_swap_node_class reemplaza nodos mientras iteramos.
        TypedArray<Node> kids = n->get_children();
        for (int i = 0; i < kids.size(); i++) {
            Node* c = Object::cast_to<Node>(kids[i]);
            if (!c) continue;
            Node* cur = c;
            for (int r = 0; r < nrenames; r++) {
                if (cur->is_class(table[r].from) && !cur->is_class(table[r].to)) {
                    cur = _gl_swap_node_class(cur, table[r].to);
                    count++;
                    break;
                }
            }
            count += _gl_migrate_tree(cur);
        }
        return count;
    }

    // ══ Merge seguro de scripts de fábrica (1.15) ════════════════════════
    // Objetivo del usuario: al actualizar, NUNCA pisar lo que el jugador tocó.
    //   · script inexistente          → ya lo crea _ensure_structure (+ su plantilla)
    //   · archivo == plantilla actual → nada
    //   · plantilla sin cambios       → nada
    //   · versión nueva y NO tocado    → se actualiza en sitio
    //   · versión nueva y SÍ tocado    → se ofrece en Game/ScriptUpdate (apagado)
    int _gl_sync_managed_scripts() {
        TypedArray<Node> scripts;
        _gl_collect_managed(this, scripts);   // recolectar ANTES de mutar el árbol
        for (int i = 0; i < scripts.size(); i++)
            _gl_sync_one(Object::cast_to<Node>(scripts[i]));
        return (int)scripts.size();
    }

    void _gl_collect_managed(Node* n, TypedArray<Node>& out) {
        for (int i = 0; i < n->get_child_count(); i++) {
            Node* c = n->get_child(i);
            if (!c) continue;
            if (c->get_name() == StringName("ScriptUpdate")) continue;   // no re-procesar ofertas
            if (Object::cast_to<ScriptNodeBase>(c) && gl_is_managed_template(c->get_name()))
                out.push_back(c);
            _gl_collect_managed(c, out);
        }
    }

    void _gl_sync_one(Node* node) {
        if (!node) return;
        String name = node->get_name();
        String cls  = node->get_class();
        String sid  = node->get("script_id");
        if (sid.is_empty()) return;
        String file_path = "res://GodotLuau/" + cls + "s/" + sid + ".lua";
        if (!FileAccess::file_exists(file_path)) return;   // solo hacemos merge de archivos EXISTENTES

        // Plantilla de fábrica actual (con el id de ESTE nodo sustituido).
        String tmpl     = gl_builtin_template(name, cls).replace("%SCRIPT_ID%", sid);
        String tmpl_md5 = tmpl.md5_text();
        String base     = gl_tpl_base_get(sid);

        String file_txt;
        Ref<FileAccess> rf = FileAccess::open(file_path, FileAccess::READ);
        if (rf.is_valid()) file_txt = rf->get_as_text();
        String file_md5 = file_txt.md5_text();

        // Ya está en la versión de fábrica actual → nada (adoptamos la base).
        if (file_md5 == tmpl_md5) {
            if (base != tmpl_md5) gl_tpl_base_set(sid, tmpl_md5);
            return;
        }
        // La fábrica no cambió respecto a la base → no hay actualización nueva.
        if (!base.is_empty() && base == tmpl_md5) return;

        // Hay una versión de fábrica NUEVA.
        bool untouched = (!base.is_empty() && file_md5 == base);
        if (untouched) {
            _gl_write_script(node, file_path, tmpl);
            gl_tpl_base_set(sid, tmpl_md5);
            GL_DEBUG_PRINT("[GodotLuau] Script actualizado en sitio (intacto): ", name);
        } else {
            // Tocado por el jugador (o base desconocida): no pisar; ofrecer aparte.
            _gl_offer_update(name, cls);
            GL_DEBUG_PRINT("[GodotLuau] '", name, "' tocado: nueva version en Game/ScriptUpdate (tu archivo intacto).");
        }
    }

    void _gl_write_script(Node* node, const String& path, const String& text) {
        Ref<LuauScript> res = node->get("codigo_luau");
        if (res.is_valid()) {
            res->_set_source_code(text);
            res->take_over_path(path);
            ResourceSaver::get_singleton()->save(res, path);
        } else {
            Ref<FileAccess> wf = FileAccess::open(path, FileAccess::WRITE);
            if (wf.is_valid()) wf->store_string(text);
        }
    }

    // Coloca la versión nueva de <name> en Game/ScriptUpdate. Reutiliza la
    // carpeta y REEMPLAZA el nodo si ya existía (así dos actualizaciones del
    // mismo script no generan carpetas/nodos duplicados). El nodo se crea
    // apagado si es ejecutable, para no interferir con el juego.
    void _gl_offer_update(const String& name, const String& cls) {
        Node* root = _owner_root();
        Node* folder = get_node_or_null(NodePath("ScriptUpdate"));
        if (!folder) {
            folder = Object::cast_to<Node>(ClassDB::instantiate(StringName("Folder")));
            if (!folder) return;
            folder->set_name("ScriptUpdate");
            add_child(folder);
            if (root) folder->set_owner(root);
        }
        if (Node* prev = folder->get_node_or_null(NodePath(name))) {
            folder->remove_child(prev);      // liberar el nombre antes de recrear
            prev->queue_free();
        }
        Node* fresh = Object::cast_to<Node>(ClassDB::instantiate(StringName(cls)));
        if (!fresh) return;
        fresh->set_name(name);
        folder->add_child(fresh);            // ENTER_TREE (editor) le escribe la plantilla ACTUAL
        if (root) fresh->set_owner(root);
        if (cls == "LocalScript" || cls == "ServerScript")
            fresh->set("Enabled", false);    // no debe ejecutarse
    }
};

#endif
