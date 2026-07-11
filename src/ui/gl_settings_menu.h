#ifndef GL_SETTINGS_MENU_H
#define GL_SETTINGS_MENU_H

// ════════════════════════════════════════════════════════════════════
//  GLSettingsMenu — menu de ajustes del juego, como el de Roblox.
//
//  Se abre con ESC (PC), boton ☰ arriba-derecha (movil/siempre visible)
//  o START del gamepad (consola). No pausa el mundo (igual que Roblox).
//
//  Contenido: jugadores conectados, ping, calidad grafica (Low/Medium/High),
//  FPS maximos (30/60/120/144/240/Sin limite; 60 por defecto), mostrar FPS,
//  mostrar ping, Resume y Leave Game.
//
//  Cada jugador guarda SUS ajustes en user://gl_settings_player<N>.cfg y se
//  cargan y aplican solos al arrancar.
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/check_button.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_joypad_button.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "gl_debug.h"

using namespace godot;

class GLSettingsMenu : public CanvasLayer {
    GDCLASS(GLSettingsMenu, CanvasLayer);

    // ── Estado (lo que se guarda) ─────────────────────────────────────
    int  quality   = 2;      // 0 Low, 1 Medium, 2 High
    int  fps_idx   = 1;      // indice en FPS_OPTIONS (default 60)
    bool show_fps  = false;
    bool show_ping = false;

    static const int FPS_COUNT = 6;
    const int FPS_OPTIONS[FPS_COUNT] = { 30, 60, 120, 144, 240, 0 };   // 0 = sin limite

    int display_num = 0;     // Player N (0 = un jugador / servidor)

    // ── UI ────────────────────────────────────────────────────────────
    Control*        panel_root = nullptr;   // panel central (visible al abrir)
    Label*          players_val = nullptr;
    Label*          ping_val    = nullptr;
    Label*          quality_val = nullptr;
    Label*          fps_val     = nullptr;
    CheckButton*    fps_tgl     = nullptr;
    CheckButton*    ping_tgl    = nullptr;
    Label*          hud         = nullptr;  // overlay FPS/ping arriba-izquierda
    Button*         burger      = nullptr;  // ☰ arriba-derecha
    double          hud_timer   = 0.0;

    // ── Helpers ───────────────────────────────────────────────────────
    Node* _find_by_class(Node* n, const char* cls) {
        if (!n) return nullptr;
        if (n->is_class(cls)) return n;
        for (int i = 0; i < n->get_child_count(); i++)
            if (Node* r = _find_by_class(n->get_child(i), cls)) return r;
        return nullptr;
    }
    Node* _netservice() {
        return is_inside_tree() ? _find_by_class((Node*)get_tree()->get_root(), "NetworkService") : nullptr;
    }
    String _cfg_path() const {
        return String("user://gl_settings_player") + String::num_int64(display_num) + ".cfg";
    }
    Label* _row_label(VBoxContainer* vb, const String& title, Label** value_out) {
        HBoxContainer* hb = memnew(HBoxContainer);
        hb->add_theme_constant_override("separation", 10);
        vb->add_child(hb);
        Label* l = memnew(Label);
        l->set_text(title);
        l->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        hb->add_child(l);
        Label* v = memnew(Label);
        v->set_text("--");
        hb->add_child(v);
        if (value_out) *value_out = v;
        return l;
    }
    // Fila selector < valor > ; los botones llaman al metodo indicado con -1/+1
    void _row_selector(VBoxContainer* vb, const String& title, Label** value_out, const StringName& method) {
        HBoxContainer* hb = memnew(HBoxContainer);
        hb->add_theme_constant_override("separation", 8);
        vb->add_child(hb);
        Label* l = memnew(Label);
        l->set_text(title);
        l->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        hb->add_child(l);
        Button* prev = memnew(Button);
        prev->set_text("<");
        prev->connect("pressed", Callable(this, method).bind(-1));
        hb->add_child(prev);
        Label* v = memnew(Label);
        v->set_custom_minimum_size(Vector2(86, 0));
        v->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        hb->add_child(v);
        Button* next = memnew(Button);
        next->set_text(">");
        next->connect("pressed", Callable(this, method).bind(1));
        hb->add_child(next);
        if (value_out) *value_out = v;
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("toggle_menu"),            &GLSettingsMenu::toggle_menu);
        ClassDB::bind_method(D_METHOD("_on_quality", "dir"),     &GLSettingsMenu::_on_quality);
        ClassDB::bind_method(D_METHOD("_on_fps", "dir"),         &GLSettingsMenu::_on_fps);
        ClassDB::bind_method(D_METHOD("_on_show_fps", "on"),     &GLSettingsMenu::_on_show_fps);
        ClassDB::bind_method(D_METHOD("_on_show_ping", "on"),    &GLSettingsMenu::_on_show_ping);
        ClassDB::bind_method(D_METHOD("_on_resume"),             &GLSettingsMenu::_on_resume);
        ClassDB::bind_method(D_METHOD("_on_leave"),              &GLSettingsMenu::_on_leave);
    }

public:
    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        set_layer(96);

        // Numero de jugador (para el archivo de ajustes propio)
        PackedStringArray a = OS::get_singleton()->get_cmdline_user_args();
        for (int i = 0; i < a.size(); i++)
            if (String(a[i]) == "--glindex" && i + 1 < a.size()) {
                int gi = String(a[i + 1]).to_int();
                display_num = (gi >= 2) ? gi - 1 : 0;
            }

        _load_settings();

        // ── ☰ arriba-derecha (siempre visible, como en Roblox movil) ──
        burger = memnew(Button);
        burger->set_text("  MENU  ");
        burger->set_flat(true);
        burger->set_anchor(SIDE_LEFT, 1.0f); burger->set_anchor(SIDE_RIGHT, 1.0f);
        burger->set_offset(SIDE_LEFT, -96.0f); burger->set_offset(SIDE_RIGHT, -10.0f);
        burger->set_offset(SIDE_TOP, 8.0f);    burger->set_offset(SIDE_BOTTOM, 40.0f);
        burger->set_tooltip_text("Game menu (Esc / gamepad Start)");
        burger->connect("pressed", Callable(this, "toggle_menu"));
        add_child(burger);

        // ── HUD FPS/ping (arriba-izquierda) ───────────────────────────
        hud = memnew(Label);
        hud->set_position(Vector2(10, 8));
        hud->add_theme_color_override("font_color", Color(0.45f, 1.0f, 0.45f));
        hud->add_theme_font_size_override("font_size", 15);
        hud->set_visible(false);
        add_child(hud);

        // ── Panel central ──────────────────────────────────────────────
        panel_root = memnew(Control);
        panel_root->set_anchors_preset(Control::PRESET_FULL_RECT);
        panel_root->set_visible(false);
        add_child(panel_root);

        PanelContainer* pc = memnew(PanelContainer);
        Ref<StyleBoxFlat> sb; sb.instantiate();
        sb->set_bg_color(Color(0.07f, 0.08f, 0.10f, 0.94f));
        sb->set_corner_radius_all(12);
        sb->set_content_margin_all(22);
        pc->add_theme_stylebox_override("panel", sb);
        pc->set_anchor(SIDE_LEFT, 0.5f);  pc->set_anchor(SIDE_RIGHT, 0.5f);
        pc->set_anchor(SIDE_TOP, 0.5f);   pc->set_anchor(SIDE_BOTTOM, 0.5f);
        pc->set_offset(SIDE_LEFT, -190.0f); pc->set_offset(SIDE_RIGHT, 190.0f);
        pc->set_offset(SIDE_TOP, -195.0f);  pc->set_offset(SIDE_BOTTOM, 195.0f);
        panel_root->add_child(pc);

        VBoxContainer* vb = memnew(VBoxContainer);
        vb->add_theme_constant_override("separation", 10);
        pc->add_child(vb);

        Label* title = memnew(Label);
        title->set_text("Settings");
        title->add_theme_font_size_override("font_size", 24);
        title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        vb->add_child(title);

        _row_label(vb, "Players", &players_val);
        _row_label(vb, "Ping",    &ping_val);
        _row_selector(vb, "Graphics Quality", &quality_val, "_on_quality");
        _row_selector(vb, "Max FPS",          &fps_val,     "_on_fps");

        {   // Show FPS
            HBoxContainer* hb = memnew(HBoxContainer);
            vb->add_child(hb);
            Label* l = memnew(Label); l->set_text("Show FPS"); l->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            hb->add_child(l);
            fps_tgl = memnew(CheckButton);
            fps_tgl->set_pressed(show_fps);
            fps_tgl->connect("toggled", Callable(this, "_on_show_fps"));
            hb->add_child(fps_tgl);
        }
        {   // Show Ping
            HBoxContainer* hb = memnew(HBoxContainer);
            vb->add_child(hb);
            Label* l = memnew(Label); l->set_text("Show Ping"); l->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            hb->add_child(l);
            ping_tgl = memnew(CheckButton);
            ping_tgl->set_pressed(show_ping);
            ping_tgl->connect("toggled", Callable(this, "_on_show_ping"));
            hb->add_child(ping_tgl);
        }

        Button* resume = memnew(Button);
        resume->set_text("Resume");
        resume->connect("pressed", Callable(this, "_on_resume"));
        vb->add_child(resume);

        Button* leave = memnew(Button);
        leave->set_text("Leave Game");
        leave->add_theme_color_override("font_color", Color(1.0f, 0.45f, 0.45f));
        leave->connect("pressed", Callable(this, "_on_leave"));
        vb->add_child(leave);

        _apply_all();
        _refresh_labels();
        set_process(true);
        GL_DEBUG_PRINT(String("[GodotLuau] Settings loaded for Player") + String::num_int64(display_num)
                       + " (quality=" + String::num_int64(quality) + ", maxFPS=" + String::num_int64(FPS_OPTIONS[fps_idx]) + ")");
    }

    // ── Abrir/cerrar ──────────────────────────────────────────────────
    void toggle_menu() {
        if (!panel_root) return;
        bool open = !panel_root->is_visible();
        panel_root->set_visible(open);
        if (open) {
            Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
            _refresh_labels();
        }
    }
    void _on_resume() { if (panel_root) panel_root->set_visible(false); }
    void _on_leave()  { if (is_inside_tree()) get_tree()->quit(); }

    void _input(const Ref<InputEvent>& e) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        Ref<InputEventKey> k = e;
        if (k.is_valid() && k->is_pressed() && !k->is_echo() && k->get_keycode() == KEY_ESCAPE) toggle_menu();
        Ref<InputEventJoypadButton> j = e;
        if (j.is_valid() && j->is_pressed() && j->get_button_index() == JOY_BUTTON_START) toggle_menu();
    }

    // ── Cambios de ajustes ────────────────────────────────────────────
    void _on_quality(int dir) {
        quality = Math::clamp(quality + dir, 0, 2);
        _apply_all(); _refresh_labels(); _save_settings();
    }
    void _on_fps(int dir) {
        fps_idx = Math::clamp(fps_idx + dir, 0, FPS_COUNT - 1);
        _apply_all(); _refresh_labels(); _save_settings();
    }
    void _on_show_fps(bool on)  { show_fps = on;  _save_settings(); }
    void _on_show_ping(bool on) { show_ping = on; _save_settings(); }

    // ── Aplicar (calidad + FPS) ───────────────────────────────────────
    void _apply_all() {
        Engine::get_singleton()->set_max_fps(FPS_OPTIONS[fps_idx]);
        Viewport* vp = get_viewport();
        if (vp) {
            float s = (quality == 0) ? 0.6f : (quality == 1) ? 0.8f : 1.0f;
            vp->set_scaling_3d_scale(s);
        }
        // Sombras del sol segun calidad (Low las apaga)
        if (is_inside_tree()) {
            Node* sun = _find_by_class((Node*)get_tree()->get_root(), "DirectionalLight3D");
            if (DirectionalLight3D* dl = Object::cast_to<DirectionalLight3D>(sun))
                dl->set_shadow(quality > 0);
        }
    }

    void _refresh_labels() {
        static const char* QNAMES[3] = { "Low", "Medium", "High" };
        if (quality_val) quality_val->set_text(QNAMES[quality]);
        if (fps_val) fps_val->set_text(FPS_OPTIONS[fps_idx] == 0 ? String("Unlimited") : String::num_int64(FPS_OPTIONS[fps_idx]));
        Node* ns = _netservice();
        if (players_val) {
            int count = ns ? (int)ns->call("get_player_count") : 1;
            players_val->set_text(String::num_int64(count));
        }
        if (ping_val) {
            double ping = ns ? (double)ns->call("get_ping_ms") : -1.0;
            ping_val->set_text(ping < 0.0 ? String("--") : (String::num_int64((int64_t)ping) + " ms"));
        }
    }

    // ── HUD FPS/ping ──────────────────────────────────────────────────
    void _process(double dt) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        hud_timer += dt;
        if (hud_timer < 0.25) return;
        hud_timer = 0.0;
        bool any = show_fps || show_ping;
        if (hud) hud->set_visible(any);
        if (!any || !hud) {
            if (panel_root && panel_root->is_visible()) _refresh_labels();
            return;
        }
        String txt;
        if (show_fps) txt += String("FPS: ") + String::num_int64((int64_t)Engine::get_singleton()->get_frames_per_second());
        if (show_ping) {
            Node* ns = _netservice();
            double ping = ns ? (double)ns->call("get_ping_ms") : -1.0;
            if (!txt.is_empty()) txt += "   ";
            txt += String("Ping: ") + (ping < 0.0 ? String("--") : (String::num_int64((int64_t)ping) + " ms"));
        }
        hud->set_text(txt);
        if (panel_root && panel_root->is_visible()) _refresh_labels();
    }

    // ── Persistencia por jugador ──────────────────────────────────────
    void _load_settings() {
        Ref<ConfigFile> cf; cf.instantiate();
        if (cf->load(_cfg_path()) == OK) {
            quality   = Math::clamp((int)(int64_t)cf->get_value("settings", "quality", 2), 0, 2);
            fps_idx   = Math::clamp((int)(int64_t)cf->get_value("settings", "fps_idx", 1), 0, FPS_COUNT - 1);
            show_fps  = (bool)cf->get_value("settings", "show_fps", false);
            show_ping = (bool)cf->get_value("settings", "show_ping", false);
        }
    }
    void _save_settings() {
        Ref<ConfigFile> cf; cf.instantiate();
        cf->set_value("settings", "quality",   quality);
        cf->set_value("settings", "fps_idx",   fps_idx);
        cf->set_value("settings", "show_fps",  show_fps);
        cf->set_value("settings", "show_ping", show_ping);
        cf->save(_cfg_path());
        GL_DEBUG_PRINT(String("[GodotLuau] Settings saved: ") + _cfg_path());
    }

    GLSettingsMenu() {}
};

#endif // GL_SETTINGS_MENU_H
