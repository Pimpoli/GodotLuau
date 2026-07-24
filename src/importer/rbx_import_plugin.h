#ifndef RBX_IMPORT_PLUGIN_H
#define RBX_IMPORT_PLUGIN_H

// Entrada "Importar lugar de Roblox (.rbxl)" en Proyecto > Herramientas.
//
// La importacion NO se hace de golpe: se reparte en lotes a lo largo de varios
// frames (unos 30 ms por frame), asi el editor sigue respondiendo y la ventana
// va mostrando fase, porcentaje y que se esta procesando ahora mismo.
//
// Antes de empezar busca un Game ya montado en la escena para reutilizarlo; si
// no lo hay, crea la estructura Roblox completa con RobloxTemplate.

#include "rbx_importer.h"
#include "roblox_datamodel.h"

#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_file_dialog.hpp>
#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

class RBXImportPlugin : public EditorPlugin {
    GDCLASS(RBXImportPlugin, EditorPlugin);

    EditorFileDialog *dialog = nullptr;

    AcceptDialog *progress_dialog = nullptr;
    Label *phase_label = nullptr;
    Label *item_label = nullptr;
    ProgressBar *bar = nullptr;

    AcceptDialog *report_dialog = nullptr;
    RichTextLabel *report_text = nullptr;

    Ref<RBXImporter> importer;
    bool importing = false;
    String current_path;

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
        if (!progress_dialog) {
            progress_dialog = memnew(AcceptDialog);
            progress_dialog->set_title("Importando lugar de Roblox");
            progress_dialog->set_min_size(Vector2(520, 0));
            VBoxContainer *box = memnew(VBoxContainer);
            phase_label = memnew(Label);
            phase_label->set_text("Preparando...");
            bar = memnew(ProgressBar);
            bar->set_min(0.0); bar->set_max(100.0); bar->set_value(0.0);
            bar->set_custom_minimum_size(Vector2(480, 22));
            item_label = memnew(Label);
            item_label->set_text("");
            box->add_child(phase_label);
            box->add_child(bar);
            box->add_child(item_label);
            progress_dialog->add_child(box);
            base->add_child(progress_dialog);
        }
        if (!report_dialog) {
            report_dialog = memnew(AcceptDialog);
            report_dialog->set_title("Importacion terminada");
            report_text = memnew(RichTextLabel);
            report_text->set_custom_minimum_size(Vector2(540, 400));
            report_text->set_selection_enabled(true);
            report_dialog->add_child(report_text);
            base->add_child(report_dialog);
        }
    }

    void _open_dialog() {
        if (importing) return;
        _ensure_dialogs();
        if (dialog) dialog->popup_centered_ratio(0.6);
    }

    static String _top_of(const Dictionary &d, int n) {
        Array order = d.keys();
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

    // Devuelve el Game de la escena: el que ya exista o uno recien creado con la
    // estructura Roblox completa. null si no hay escena abierta.
    Node *_find_or_create_game(Node *scene_root) {
        if (!scene_root) return nullptr;
        if (Node *g = RBXImporter::find_existing_game(scene_root)) return g;

        // No hay estructura: la creamos igual que el boton "Game 3D".
        RobloxTemplate *tpl = memnew(RobloxTemplate);
        tpl->set_name("Game");
        scene_root->add_child(tpl);
        tpl->set_owner(scene_root);
        tpl->_gl_update_structure();
        _own_all(tpl, scene_root);
        return tpl;
    }

    void _on_file_selected(const String &path) {
        if (importing) return;
        _ensure_dialogs();
        current_path = path;

        Node *scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
        Node *game = _find_or_create_game(scene_root);

        // El importador es solo 3D: con un Workspace 2D no tiene sentido.
        if (game) {
            Node *ws = game->get_node_or_null(NodePath("Workspace"));
            if (ws && ws->is_class("RobloxWorkspace2D")) {
                _show_message("La escena usa un Workspace 2D.\n\n"
                              "El importador de lugares de Roblox solo funciona en "
                              "proyectos 3D. Crea una escena con 'Game 3D' e importa ahi.");
                return;
            }
        }

        importer.instantiate();
        if (!importer->begin_import(path)) {
            _show_message(String("No se pudo importar:\n\n") + importer->get_last_error());
            return;
        }
        importer->set_target(game);

        importing = true;
        phase_label->set_text("Preparando...");
        item_label->set_text("");
        bar->set_value(0.0);
        progress_dialog->popup_centered();
        set_process(true);
    }

    void _show_message(const String &msg) {
        _ensure_dialogs();
        if (report_text) { report_text->clear(); report_text->add_text(msg); }
        if (report_dialog) report_dialog->popup_centered();
    }

    // Sin owner un nodo no se guarda al grabar la escena. Solo se asigna a los
    // que NO tienen: pisar el owner de un nodo instanciado de otra escena la
    // romperia.
    void _own_all(Node *n, Node *owner) {
        for (int k = 0; k < n->get_child_count(); k++) {
            Node *c = n->get_child(k);
            if (c->get_owner() == nullptr) c->set_owner(owner);
            _own_all(c, owner);
        }
    }

    void _on_finished() {
        importing = false;
        set_process(false);
        if (progress_dialog) progress_dialog->hide();

        if (importer->has_failed()) {
            _show_message(String("No se pudo importar:\n\n") + importer->get_last_error());
            return;
        }

        Node *result = importer->get_result();
        Node *scene_root = EditorInterface::get_singleton()->get_edited_scene_root();

        if (result && !result->get_parent()) {
            // No habia escena abierta: el arbol importado pasa a ser la escena.
            EditorInterface::get_singleton()->edit_node(result);
        } else if (scene_root) {
            _own_all(scene_root, scene_root);
        }

        Dictionary rep = importer->get_report();
        Dictionary missing = rep.get("clases_sin_equivalente", Dictionary());
        Array assets = rep.get("assets_faltantes", Array());
        Array verr = rep.get("errores_comprobacion", Array());

        String msg;
        msg += String("Archivo: ") + current_path + String("\n\n");
        msg += String("IMPORTADO\n");
        msg += String("  Instancias: ") + String::num_int64((int)rep.get("nodos_creados", 0)) +
               String(" de ") + String::num_int64((int)rep.get("instancias_totales", 0)) + String("\n");
        msg += String("  Scripts con codigo: ") + String::num_int64((int)rep.get("scripts_importados", 0)) + String("\n");
        msg += String("  Tags: ") + String::num_int64((int)rep.get("tags_aplicados", 0)) +
               String("   Atributos: ") + String::num_int64((int)rep.get("atributos_aplicados", 0)) +
               String("   Referencias: ") + String::num_int64((int)rep.get("referencias_resueltas", 0)) + String("\n\n");

        msg += String("COMPROBADO\n");
        msg += String("  Verificadas en el arbol: ") + String::num_int64((int)rep.get("comprobadas_ok", 0)) + String("\n");
        if (verr.size() > 0) {
            msg += String("  Incidencias corregidas: ") + String::num_int64(verr.size()) + String("\n");
            for (int k = 0; k < verr.size() && k < 5; k++)
                msg += String("    ") + String(verr[k]) + String("\n");
        } else {
            msg += String("  Sin incidencias: no falta nada del archivo.\n");
        }
        msg += String("\nRECOLOCADO\n");
        msg += String("  Servicios reutilizados de tu Game: ") +
               String::num_int64((int)rep.get("servicios_reutilizados", 0)) + String("\n");
        msg += String("  Servicios creados nuevos: ") +
               String::num_int64((int)rep.get("servicios_creados", 0)) + String("\n");
        msg += String("  Tiempo total: ") + String::num_int64((int)rep.get("tiempo_ms", 0)) + String(" ms\n\n");

        if (missing.size() > 0) {
            msg += String("Clases sin equivalente nativo (") + String::num_int64(missing.size()) +
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
        set_process(false);
    }

    void _exit_tree() override {
        remove_tool_menu_item("Importar lugar de Roblox (.rbxl)");
        if (dialog)          { dialog->queue_free();          dialog = nullptr; }
        if (progress_dialog) { progress_dialog->queue_free(); progress_dialog = nullptr; }
        if (report_dialog)   { report_dialog->queue_free();   report_dialog = nullptr; }
    }

    void _process(double delta) override {
        if (!importing || importer.is_null()) return;

        // Presupuesto de ~30 ms por frame: avanza rapido pero el editor sigue
        // respondiendo y la barra se ve moverse.
        const uint64_t t0 = Time::get_singleton()->get_ticks_msec();
        while (Time::get_singleton()->get_ticks_msec() - t0 < 30) {
            if (!importer->step()) break;
            if (importer->is_done() || importer->has_failed()) break;
        }

        if (bar) bar->set_value(importer->get_progress() * 100.0);
        if (phase_label) phase_label->set_text(importer->get_phase_name());
        if (item_label) item_label->set_text(importer->get_current_item());

        if (importer->is_done() || importer->has_failed()) _on_finished();
    }
};

#endif // RBX_IMPORT_PLUGIN_H
