#ifndef RBX_IMPORT_PLUGIN_H
#define RBX_IMPORT_PLUGIN_H

// Entrada "Importar lugar de Roblox (.rbxl)" en el menu Proyecto > Herramientas.
// Abre el archivo, reconstruye el arbol y lo cuelga de la escena abierta.

#include "rbx_importer.h"

#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_file_dialog.hpp>
#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

class RBXImportPlugin : public EditorPlugin {
    GDCLASS(RBXImportPlugin, EditorPlugin);

    EditorFileDialog *dialog = nullptr;
    AcceptDialog *report_dialog = nullptr;
    RichTextLabel *report_text = nullptr;

    void _ensure_dialogs() {
        Control *base = EditorInterface::get_singleton()->get_base_control();
        if (!base) return;

        if (!dialog) {
            dialog = memnew(EditorFileDialog);
            dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
            dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
            dialog->add_filter("*.rbxl", "Lugar de Roblox (binario)");
            dialog->add_filter("*.rbxm", "Modelo de Roblox (binario)");
            dialog->set_title("Importar lugar de Roblox");
            base->add_child(dialog);
            dialog->connect("file_selected", callable_mp(this, &RBXImportPlugin::_on_file_selected));
        }
        if (!report_dialog) {
            report_dialog = memnew(AcceptDialog);
            report_dialog->set_title("Importacion terminada");
            report_text = memnew(RichTextLabel);
            report_text->set_custom_minimum_size(Vector2(520, 380));
            report_text->set_selection_enabled(true);
            report_dialog->add_child(report_text);
            base->add_child(report_dialog);
        }
    }

    void _open_dialog() {
        _ensure_dialogs();
        if (dialog) dialog->popup_centered_ratio(0.6);
    }

    // Las 'n' clases mas frecuentes de un Dictionary clase -> conteo.
    static String _top_of(const Dictionary &d, int n) {
        Array keys = d.keys();
        // Insercion simple: son pocas decenas de entradas.
        Array order;
        for (int k = 0; k < keys.size(); k++) order.push_back(keys[k]);
        for (int a = 0; a < order.size(); a++)
            for (int b = a + 1; b < order.size(); b++)
                if ((int)d[order[b]] > (int)d[order[a]]) {
                    Variant t = order[a]; order[a] = order[b]; order[b] = t;
                }
        String out;
        for (int k = 0; k < order.size() && k < n; k++)
            out += String("    ") + String(order[k]) + String(" x") +
                   String::num_int64((int)d[order[k]]) + String("\n");
        return out;
    }

    void _on_file_selected(const String &path) {
        Ref<RBXImporter> imp;
        imp.instantiate();
        Node *root = imp->import_file(path);

        _ensure_dialogs();
        if (!root) {
            if (report_text) {
                report_text->clear();
                report_text->add_text(String("No se pudo importar:\n\n") + imp->get_last_error());
            }
            if (report_dialog) report_dialog->popup_centered();
            return;
        }

        // Colgar el resultado de la escena abierta; si no hay ninguna, el propio
        // arbol importado pasa a ser la escena.
        Node *scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
        if (scene_root) {
            scene_root->add_child(root);
            // El owner debe asignarse con el arbol ya colgado o no se guarda nada.
            root->set_owner(scene_root);
            _own_all(root, scene_root);
        } else {
            EditorInterface::get_singleton()->get_editor_main_screen();
            root->set_name(root->get_name());
            EditorInterface::get_singleton()->edit_node(root);
        }

        Dictionary rep = imp->get_report();
        Dictionary missing = rep.get("clases_sin_equivalente", Dictionary());
        Array assets = rep.get("assets_faltantes", Array());

        String msg;
        msg += String("Archivo: ") + path + String("\n\n");
        msg += String("Instancias importadas: ") +
               String::num_int64((int)rep.get("nodos_creados", 0)) + String(" de ") +
               String::num_int64((int)rep.get("instancias_totales", 0)) + String("\n");
        msg += String("Scripts con codigo: ") +
               String::num_int64((int)rep.get("scripts_importados", 0)) + String("\n");
        msg += String("Tags aplicados: ") +
               String::num_int64((int)rep.get("tags_aplicados", 0)) + String("\n");
        msg += String("Atributos aplicados: ") +
               String::num_int64((int)rep.get("atributos_aplicados", 0)) + String("\n");
        msg += String("Referencias resueltas: ") +
               String::num_int64((int)rep.get("referencias_resueltas", 0)) + String("\n");
        msg += String("Tiempo: ") +
               String::num_int64((int)rep.get("tiempo_ms", 0)) + String(" ms\n\n");

        if (missing.size() > 0) {
            msg += String("Clases sin equivalente nativo (") +
                   String::num_int64(missing.size()) +
                   String("). Se importaron como RBXInstance, conservando nombre,\n"
                          "jerarquia y propiedades, asi que no se ha perdido nada:\n");
            msg += _top_of(missing, 10);
            msg += String("\n");
        }

        if (assets.size() > 0) {
            msg += String("Assets de la nube de Roblox sin resolver: ") +
                   String::num_int64(assets.size()) +
                   String("\n    (meshes, texturas y sonidos viven en los servidores de\n"
                          "     Roblox y no se pueden descargar. El id queda guardado en\n"
                          "     cada nodo: exporta el modelo desde Studio y asignalo.)\n");
        }

        if (report_text) { report_text->clear(); report_text->add_text(msg); }
        if (report_dialog) report_dialog->popup_centered();
    }

    void _own_all(Node *n, Node *owner) {
        for (int k = 0; k < n->get_child_count(); k++) {
            Node *c = n->get_child(k);
            c->set_owner(owner);
            _own_all(c, owner);
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("_open_dialog"), &RBXImportPlugin::_open_dialog);
        ClassDB::bind_method(D_METHOD("_on_file_selected", "path"),
                             &RBXImportPlugin::_on_file_selected);
    }

public:
    void _enter_tree() override {
        add_tool_menu_item("Importar lugar de Roblox (.rbxl)",
                           callable_mp(this, &RBXImportPlugin::_open_dialog));
    }

    void _exit_tree() override {
        remove_tool_menu_item("Importar lugar de Roblox (.rbxl)");
        if (dialog)        { dialog->queue_free();        dialog = nullptr; }
        if (report_dialog) { report_dialog->queue_free(); report_dialog = nullptr; }
    }
};

#endif // RBX_IMPORT_PLUGIN_H
