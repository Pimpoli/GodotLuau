#ifndef GL_SETTINGS_MENU_H
#define GL_SETTINGS_MENU_H

// ════════════════════════════════════════════════════════════════════
//  GLSettingsMenu — menu del juego estilo Roblox (v2).
//
//  · Se abre con ESC (PC), boton ☰ arriba-derecha (siempre visible,
//    como en Roblox movil) o START del gamepad. No pausa el mundo.
//  · Fondo oscurecido + panel con animacion de apertura (escala+fade).
//  · PESTANAS: Players (lista en vivo) / Settings / Help (controles).
//  · Settings: calidad grafica, FPS maximos (60 por defecto), volumen,
//    sensibilidad del mouse, mostrar FPS / ping en pantalla.
//  · Leave Game pide confirmacion (segundo click), como Roblox.
//  · Cada jugador guarda SUS ajustes en user://gl_settings_player<N>.cfg.
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/h_separator.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/check_button.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/style_box_empty.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_joypad_button.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "gl_debug.h"
#include "gl_graphics.h"

using namespace godot;

class GLSettingsMenu : public CanvasLayer {
    GDCLASS(GLSettingsMenu, CanvasLayer);

    // ── Paleta (oscura estilo Roblox) ─────────────────────────────────
    const Color COL_BG      = Color(0.055f, 0.063f, 0.075f, 0.96f);
    const Color COL_ROW     = Color(1, 1, 1, 0.045f);
    const Color COL_ACCENT  = Color(0.20f, 0.55f, 1.0f);
    const Color COL_TEXT    = Color(0.94f, 0.95f, 0.97f);
    const Color COL_DIM     = Color(0.62f, 0.65f, 0.70f);
    const Color COL_GREEN   = Color(0.22f, 0.78f, 0.40f);
    const Color COL_RED     = Color(0.92f, 0.30f, 0.30f);

    // ── Estado persistente ────────────────────────────────────────────
    int    quality   = 8;      // 1..10 (como Roblox), 1.15
    bool   cam_lock  = false;  // bloquear cámara: 3ª persona se ve como 1ª (1.15)
    int    fps_idx   = 1;      // default 60
    bool   show_fps  = false;
    bool   show_ping = false;
    double volume    = 100.0;  // 0..100
    double sensitivity = 1.0;  // 0.2..3.0 (multiplicador)

    static const int FPS_COUNT = 6;
    const int FPS_OPTIONS[FPS_COUNT] = { 30, 60, 120, 144, 240, 0 };

    int display_num = 0;

    // ── UI ────────────────────────────────────────────────────────────
    ColorRect*      dim = nullptr;
    Control*        panel_root = nullptr;
    PanelContainer* panel = nullptr;
    Button*         tab_btns[3] = { nullptr, nullptr, nullptr };
    Control*        tab_pages[3] = { nullptr, nullptr, nullptr };
    int             tab_active = 1;   // abre en Settings
    VBoxContainer*  players_list = nullptr;
    Label*          players_count = nullptr;
    Label*          ping_val    = nullptr;
    Label*          quality_val = nullptr;
    Label*          fps_val     = nullptr;
    Label*          volume_val  = nullptr;
    Label*          sens_val    = nullptr;
    Button*         leave_btn   = nullptr;
    Label*          hud         = nullptr;
    double          hud_timer   = 0.0;
    bool            leave_armed = false;
    double          leave_timer = 0.0;
    double          sens_retry  = 2.0;   // reaplicar sensibilidad cuando exista el jugador
    bool            is_open     = false;

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
    Ref<StyleBoxFlat> _box(Color c, int radius, int margin) {
        Ref<StyleBoxFlat> sb; sb.instantiate();
        sb->set_bg_color(c);
        sb->set_corner_radius_all(radius);
        sb->set_content_margin_all((float)margin);
        return sb;
    }
    Label* _mklabel(const String& text, int size, Color col) {
        Label* l = memnew(Label);
        l->set_text(text);
        l->add_theme_font_size_override("font_size", size);
        l->add_theme_color_override("font_color", col);
        return l;
    }
    // Fila con fondo suave: [titulo expandido][...controles que agregue el caller]
    HBoxContainer* _row(VBoxContainer* vb, const String& title) {
        PanelContainer* pc = memnew(PanelContainer);
        pc->add_theme_stylebox_override("panel", _box(COL_ROW, 8, 10));
        vb->add_child(pc);
        HBoxContainer* hb = memnew(HBoxContainer);
        hb->add_theme_constant_override("separation", 10);
        pc->add_child(hb);
        Label* l = _mklabel(title, 16, COL_TEXT);
        l->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        l->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
        hb->add_child(l);
        return hb;
    }
    void _selector(HBoxContainer* hb, Label** value_out, const StringName& method) {
        Button* prev = memnew(Button);
        prev->set_text(" < ");
        prev->set_flat(true);
        prev->add_theme_color_override("font_color", COL_ACCENT);
        prev->connect("pressed", Callable(this, method).bind(-1));
        hb->add_child(prev);
        Label* v = _mklabel("--", 16, COL_TEXT);
        v->set_custom_minimum_size(Vector2(88, 0));
        v->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        v->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
        hb->add_child(v);
        Button* next = memnew(Button);
        next->set_text(" > ");
        next->set_flat(true);
        next->add_theme_color_override("font_color", COL_ACCENT);
        next->connect("pressed", Callable(this, method).bind(1));
        hb->add_child(next);
        if (value_out) *value_out = v;
    }
    HSlider* _slider(HBoxContainer* hb, double minv, double maxv, double step, double val,
                     Label** value_out, const StringName& method) {
        HSlider* s = memnew(HSlider);
        s->set_min(minv); s->set_max(maxv); s->set_step(step);
        s->set_value(val);
        s->set_custom_minimum_size(Vector2(150, 0));
        s->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
        s->connect("value_changed", Callable(this, method));
        hb->add_child(s);
        Label* v = _mklabel("--", 15, COL_DIM);
        v->set_custom_minimum_size(Vector2(52, 0));
        v->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        v->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
        hb->add_child(v);
        if (value_out) *value_out = v;
        return s;
    }
    CheckButton* _toggle(HBoxContainer* hb, bool val, const StringName& method) {
        CheckButton* t = memnew(CheckButton);
        t->set_pressed(val);
        t->connect("toggled", Callable(this, method));
        hb->add_child(t);
        return t;
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("toggle_menu"),         &GLSettingsMenu::toggle_menu);
        ClassDB::bind_method(D_METHOD("_on_tab", "i"),        &GLSettingsMenu::_on_tab);
        ClassDB::bind_method(D_METHOD("_on_quality", "dir"),  &GLSettingsMenu::_on_quality);
        ClassDB::bind_method(D_METHOD("_on_cam_lock", "on"),  &GLSettingsMenu::_on_cam_lock);
        ClassDB::bind_method(D_METHOD("_on_fps", "dir"),      &GLSettingsMenu::_on_fps);
        ClassDB::bind_method(D_METHOD("_on_volume", "v"),     &GLSettingsMenu::_on_volume);
        ClassDB::bind_method(D_METHOD("_on_sens", "v"),       &GLSettingsMenu::_on_sens);
        ClassDB::bind_method(D_METHOD("_on_show_fps", "on"),  &GLSettingsMenu::_on_show_fps);
        ClassDB::bind_method(D_METHOD("_on_show_ping", "on"), &GLSettingsMenu::_on_show_ping);
        ClassDB::bind_method(D_METHOD("_on_resume"),          &GLSettingsMenu::_on_resume);
        ClassDB::bind_method(D_METHOD("_on_leave"),           &GLSettingsMenu::_on_leave);
        ClassDB::bind_method(D_METHOD("_hide_done"),          &GLSettingsMenu::_hide_done);
    }

public:
    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        set_layer(96);

        PackedStringArray a = OS::get_singleton()->get_cmdline_user_args();
        for (int i = 0; i < a.size(); i++)
            if (String(a[i]) == "--glindex" && i + 1 < a.size()) {
                int gi = String(a[i + 1]).to_int();
                display_num = (gi >= 2) ? gi - 1 : 0;
            }

        _load_settings();

        // ── ☰ flotante arriba-derecha ─────────────────────────────────
        Button* burger = memnew(Button);
        burger->set_text(String::utf8("  ☰  "));
        burger->add_theme_font_size_override("font_size", 20);
        burger->add_theme_stylebox_override("normal",  _box(Color(0, 0, 0, 0.35f), 10, 6));
        burger->add_theme_stylebox_override("hover",   _box(Color(0, 0, 0, 0.55f), 10, 6));
        burger->add_theme_stylebox_override("pressed", _box(Color(0, 0, 0, 0.7f), 10, 6));
        burger->set_anchor(SIDE_LEFT, 1.0f); burger->set_anchor(SIDE_RIGHT, 1.0f);
        burger->set_offset(SIDE_LEFT, -56.0f); burger->set_offset(SIDE_RIGHT, -10.0f);
        burger->set_offset(SIDE_TOP, 8.0f);    burger->set_offset(SIDE_BOTTOM, 46.0f);
        burger->set_tooltip_text("Game menu (Esc / gamepad Start)");
        burger->connect("pressed", Callable(this, "toggle_menu"));
        add_child(burger);

        // ── HUD FPS/ping ──────────────────────────────────────────────
        hud = _mklabel("", 15, Color(0.45f, 1.0f, 0.45f));
        hud->set_position(Vector2(10, 8));
        hud->set_visible(false);
        add_child(hud);

        // ── Raiz del menu: dim + panel ────────────────────────────────
        panel_root = memnew(Control);
        panel_root->set_anchors_preset(Control::PRESET_FULL_RECT);
        panel_root->set_visible(false);
        add_child(panel_root);

        dim = memnew(ColorRect);
        dim->set_color(Color(0, 0, 0, 0.45f));
        dim->set_anchors_preset(Control::PRESET_FULL_RECT);
        panel_root->add_child(dim);

        panel = memnew(PanelContainer);
        panel->add_theme_stylebox_override("panel", _box(COL_BG, 14, 18));
        panel->set_anchor(SIDE_LEFT, 0.5f);  panel->set_anchor(SIDE_RIGHT, 0.5f);
        panel->set_anchor(SIDE_TOP, 0.5f);   panel->set_anchor(SIDE_BOTTOM, 0.5f);
        panel->set_offset(SIDE_LEFT, -250.0f); panel->set_offset(SIDE_RIGHT, 250.0f);
        panel->set_offset(SIDE_TOP, -235.0f);  panel->set_offset(SIDE_BOTTOM, 235.0f);
        panel_root->add_child(panel);

        VBoxContainer* main_vb = memnew(VBoxContainer);
        main_vb->add_theme_constant_override("separation", 10);
        panel->add_child(main_vb);

        // ── Header: titulo + X ────────────────────────────────────────
        {
            HBoxContainer* hb = memnew(HBoxContainer);
            main_vb->add_child(hb);
            Label* t = _mklabel("Menu", 24, COL_TEXT);
            t->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            hb->add_child(t);
            Button* x = memnew(Button);
            x->set_text(String::utf8("  ✕  "));
            x->set_flat(true);
            x->add_theme_color_override("font_color", COL_DIM);
            x->connect("pressed", Callable(this, "_on_resume"));
            hb->add_child(x);
        }

        // ── Pestañas ──────────────────────────────────────────────────
        {
            HBoxContainer* tabs = memnew(HBoxContainer);
            tabs->add_theme_constant_override("separation", 6);
            main_vb->add_child(tabs);
            const char* names[3] = { "Players", "Settings", "Help" };
            for (int i = 0; i < 3; i++) {
                Button* b = memnew(Button);
                b->set_text(names[i]);
                b->set_h_size_flags(Control::SIZE_EXPAND_FILL);
                b->add_theme_font_size_override("font_size", 16);
                b->connect("pressed", Callable(this, "_on_tab").bind(i));
                tabs->add_child(b);
                tab_btns[i] = b;
            }
        }
        main_vb->add_child(memnew(HSeparator));

        // ── Contenido de pestañas (con scroll) ────────────────────────
        ScrollContainer* sc = memnew(ScrollContainer);
        sc->set_v_size_flags(Control::SIZE_EXPAND_FILL);
        sc->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
        main_vb->add_child(sc);
        Control* pages = memnew(Control);
        pages->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        pages->set_v_size_flags(Control::SIZE_EXPAND_FILL);
        sc->add_child(pages);

        // — Pagina 0: PLAYERS —
        {
            VBoxContainer* vb = memnew(VBoxContainer);
            vb->set_anchors_preset(Control::PRESET_FULL_RECT);
            vb->add_theme_constant_override("separation", 8);
            pages->add_child(vb);
            players_count = _mklabel("", 15, COL_DIM);
            vb->add_child(players_count);
            players_list = memnew(VBoxContainer);
            players_list->add_theme_constant_override("separation", 6);
            vb->add_child(players_list);
            tab_pages[0] = vb;
        }
        // — Pagina 1: SETTINGS —
        {
            VBoxContainer* vb = memnew(VBoxContainer);
            vb->set_anchors_preset(Control::PRESET_FULL_RECT);
            vb->add_theme_constant_override("separation", 8);
            pages->add_child(vb);

            { HBoxContainer* hb = _row(vb, "Ping");
              ping_val = _mklabel("--", 16, COL_DIM);
              ping_val->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
              hb->add_child(ping_val); }
            { HBoxContainer* hb = _row(vb, "Graphics Quality"); _selector(hb, &quality_val, "_on_quality"); }
            { HBoxContainer* hb = _row(vb, "Lock Camera (1st person)"); _toggle(hb, cam_lock, "_on_cam_lock"); }
            { HBoxContainer* hb = _row(vb, "Max FPS");          _selector(hb, &fps_val,     "_on_fps"); }
            { HBoxContainer* hb = _row(vb, "Volume");           _slider(hb, 0, 100, 1, volume, &volume_val, "_on_volume"); }
            { HBoxContainer* hb = _row(vb, "Camera Sensitivity"); _slider(hb, 0.2, 3.0, 0.1, sensitivity, &sens_val, "_on_sens"); }
            { HBoxContainer* hb = _row(vb, "Show FPS");  _toggle(hb, show_fps,  "_on_show_fps"); }
            { HBoxContainer* hb = _row(vb, "Show Ping"); _toggle(hb, show_ping, "_on_show_ping"); }
            tab_pages[1] = vb;
        }
        // — Pagina 2: HELP —
        {
            VBoxContainer* vb = memnew(VBoxContainer);
            vb->set_anchors_preset(Control::PRESET_FULL_RECT);
            vb->add_theme_constant_override("separation", 6);
            pages->add_child(vb);
            const char* lines[] = {
                "W / A / S / D  -  Move",
                "Space  -  Jump",
                "Right mouse  -  Rotate camera",
                "Mouse wheel  -  Zoom",
                "/  or  Enter  -  Chat",
                "Esc  -  This menu",
                "Gamepad: left stick move, A jump, Start menu",
                "Mobile: joystick to move, drag right side to look",
            };
            for (int i = 0; i < 8; i++) {
                Label* l = _mklabel(String::utf8(lines[i]), 15, i % 2 ? COL_DIM : COL_TEXT);
                vb->add_child(l);
            }
            tab_pages[2] = vb;
        }

        // ── Footer: Resume + Leave ────────────────────────────────────
        main_vb->add_child(memnew(HSeparator));
        {
            HBoxContainer* hb = memnew(HBoxContainer);
            hb->add_theme_constant_override("separation", 12);
            main_vb->add_child(hb);
            Button* resume = memnew(Button);
            resume->set_text("Resume");
            resume->add_theme_font_size_override("font_size", 17);
            resume->add_theme_stylebox_override("normal",  _box(Color(COL_GREEN, 0.22f), 10, 10));
            resume->add_theme_stylebox_override("hover",   _box(Color(COL_GREEN, 0.38f), 10, 10));
            resume->add_theme_stylebox_override("pressed", _box(Color(COL_GREEN, 0.50f), 10, 10));
            resume->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            resume->connect("pressed", Callable(this, "_on_resume"));
            hb->add_child(resume);
            leave_btn = memnew(Button);
            leave_btn->set_text("Leave Game");
            leave_btn->add_theme_font_size_override("font_size", 17);
            leave_btn->add_theme_stylebox_override("normal",  _box(Color(COL_RED, 0.20f), 10, 10));
            leave_btn->add_theme_stylebox_override("hover",   _box(Color(COL_RED, 0.38f), 10, 10));
            leave_btn->add_theme_stylebox_override("pressed", _box(Color(COL_RED, 0.55f), 10, 10));
            leave_btn->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            leave_btn->connect("pressed", Callable(this, "_on_leave"));
            hb->add_child(leave_btn);
        }

        _apply_all();
        _select_tab(1);
        _refresh_labels();
        set_process(true);
        GL_DEBUG_PRINT(String("[GodotLuau] Settings loaded for Player") + String::num_int64(display_num)
                       + " (quality=" + String::num_int64(quality) + ", maxFPS=" + String::num_int64(FPS_OPTIONS[fps_idx]) + ")");
    }

    // ── Abrir / cerrar con animacion ──────────────────────────────────
    void toggle_menu() {
        if (!panel_root) return;
        if (is_open) { _close_anim(); return; }
        is_open = true;
        panel_root->set_visible(true);
        Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
        _refresh_labels();
        _refresh_players();
        // Animacion: dim fade + panel escala 0.9→1 con rebote suave
        panel->set_pivot_offset(panel->get_size() * 0.5f);
        Ref<Tween> tw = create_tween();
        tw->set_parallel(true);
        tw->tween_property(dim, NodePath("modulate:a"), 1.0, 0.16)->from(Variant(0.0));
        tw->tween_property(panel, NodePath("scale"), Vector2(1, 1), 0.22)
          ->from(Variant(Vector2(0.88f, 0.88f)))->set_trans(Tween::TRANS_BACK)->set_ease(Tween::EASE_OUT);
        tw->tween_property(panel, NodePath("modulate:a"), 1.0, 0.14)->from(Variant(0.0));
    }
    void _close_anim() {
        if (!is_open) return;
        is_open = false;
        leave_armed = false;
        if (leave_btn) leave_btn->set_text("Leave Game");
        Ref<Tween> tw = create_tween();
        tw->set_parallel(true);
        tw->tween_property(dim, NodePath("modulate:a"), 0.0, 0.12);
        tw->tween_property(panel, NodePath("scale"), Vector2(0.9f, 0.9f), 0.12)->set_ease(Tween::EASE_IN);
        tw->tween_property(panel, NodePath("modulate:a"), 0.0, 0.12);
        tw->chain()->tween_callback(Callable(this, "_hide_done"));
    }
    void _hide_done() { if (panel_root && !is_open) panel_root->set_visible(false); }
    void _on_resume() { _close_anim(); }
    void _on_leave() {
        if (!leave_armed) {
            leave_armed = true;
            leave_timer = 2.5;
            if (leave_btn) leave_btn->set_text("Are you sure?");
            return;
        }
        if (is_inside_tree()) get_tree()->quit();
    }

    void _input(const Ref<InputEvent>& e) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        Ref<InputEventKey> k = e;
        if (k.is_valid() && k->is_pressed() && !k->is_echo() && k->get_keycode() == KEY_ESCAPE) toggle_menu();
        Ref<InputEventJoypadButton> j = e;
        if (j.is_valid() && j->is_pressed() && j->get_button_index() == JOY_BUTTON_START) toggle_menu();
    }

    // ── Pestañas ──────────────────────────────────────────────────────
    void _on_tab(int i) { _select_tab(i); }
    void _select_tab(int i) {
        tab_active = Math::clamp(i, 0, 2);
        for (int t = 0; t < 3; t++) {
            bool on = (t == tab_active);
            if (tab_pages[t]) tab_pages[t]->set_visible(on);
            if (tab_btns[t]) {
                tab_btns[t]->add_theme_color_override("font_color", on ? COL_TEXT : COL_DIM);
                tab_btns[t]->add_theme_stylebox_override("normal",
                    on ? _box(Color(COL_ACCENT, 0.28f), 9, 8) : _box(COL_ROW, 9, 8));
                tab_btns[t]->add_theme_stylebox_override("hover",  _box(Color(COL_ACCENT, 0.18f), 9, 8));
                tab_btns[t]->add_theme_stylebox_override("pressed", _box(Color(COL_ACCENT, 0.35f), 9, 8));
            }
        }
        if (tab_active == 0) _refresh_players();
        // Fade del contenido al cambiar
        if (tab_pages[tab_active]) {
            Ref<Tween> tw = create_tween();
            tw->tween_property(tab_pages[tab_active], NodePath("modulate:a"), 1.0, 0.15)->from(Variant(0.35));
        }
    }

    // ── Lista de jugadores ────────────────────────────────────────────
    void _refresh_players() {
        if (!players_list) return;
        while (players_list->get_child_count() > 0) {
            Node* c = players_list->get_child(0);
            players_list->remove_child(c);
            c->queue_free();
        }
        Node* ns = _netservice();
        int count = ns ? (int)ns->call("get_player_count") : 1;
        int my_idx = ns ? (int)ns->call("get_player_index") : 0;
        if (players_count) players_count->set_text(String::num_int64(count) + (count == 1 ? " player in this server" : " players in this server"));
        // Yo
        String me = (my_idx >= 2) ? (String("Player") + String::num_int64(my_idx - 1))
                   : (my_idx == 1 ? String("Server") : String("Player"));
        _add_player_row(me + "  (You)", true);
        // Los demas (muñecos RemotePlayer_* del Workspace, su label es el nombre)
        if (is_inside_tree()) {
            Node* ws = _find_by_class((Node*)get_tree()->get_root(), "RobloxWorkspace");
            if (!ws) ws = _find_by_class((Node*)get_tree()->get_root(), "RobloxWorkspace2D");
            if (ws) {
                for (int i = 0; i < ws->get_child_count(); i++) {
                    Node* c = ws->get_child(i);
                    if (String(c->get_name()).begins_with("RemotePlayer_")) {
                        String nm = "Player";
                        for (int k = 0; k < c->get_child_count(); k++) {
                            Node* h = c->get_child(k);
                            if (h->is_class("Label3D") || h->is_class("Label")) { nm = h->call("get_text"); break; }
                        }
                        _add_player_row(nm, false);
                    }
                }
            }
        }
    }
    void _add_player_row(const String& name, bool me) {
        PanelContainer* pc = memnew(PanelContainer);
        pc->add_theme_stylebox_override("panel", _box(me ? Color(COL_ACCENT, 0.14f) : COL_ROW, 8, 9));
        players_list->add_child(pc);
        HBoxContainer* hb = memnew(HBoxContainer);
        hb->add_theme_constant_override("separation", 10);
        pc->add_child(hb);
        Panel* dot = memnew(Panel);
        dot->add_theme_stylebox_override("panel", _box(me ? COL_ACCENT : COL_GREEN, 99, 0));
        dot->set_custom_minimum_size(Vector2(12, 12));
        dot->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
        hb->add_child(dot);
        Label* l = _mklabel(name, 16, COL_TEXT);
        hb->add_child(l);
    }

    // ── Cambios de ajustes ────────────────────────────────────────────
    void _on_quality(int dir) { quality = Math::clamp(quality + dir, 1, 10); _apply_all(); _refresh_labels(); _save_settings(); }
    void _on_cam_lock(bool on) { cam_lock = on; _apply_camera_lock(); _save_settings(); }
    void _on_fps(int dir)     { fps_idx = Math::clamp(fps_idx + dir, 0, FPS_COUNT - 1); _apply_all(); _refresh_labels(); _save_settings(); }
    void _on_volume(double v) { volume = v; _apply_all(); _refresh_labels(); _save_settings(); }
    void _on_sens(double v)   { sensitivity = v; _apply_all(); _refresh_labels(); _save_settings(); }
    void _on_show_fps(bool on)  { show_fps = on;  _save_settings(); }
    void _on_show_ping(bool on) { show_ping = on; _save_settings(); }

    // ── Aplicar ───────────────────────────────────────────────────────
    void _apply_all() {
        Engine::get_singleton()->set_max_fps(FPS_OPTIONS[fps_idx]);
        // Calidad 1..10: escala, sombras suaves reales, SSAO, glow… en un sitio (1.15)
        gl_apply_graphics_quality(quality, this);
        _apply_camera_lock();
        // Volumen maestro
        AudioServer* as = AudioServer::get_singleton();
        if (as) {
            as->set_bus_mute(0, volume <= 0.5);
            as->set_bus_volume_db(0, (float)UtilityFunctions::linear_to_db(Math::max(volume, 0.5) / 100.0));
        }
        _apply_sensitivity();
    }
    void _apply_camera_lock() {
        if (!is_inside_tree()) return;
        Node* p = _find_by_class((Node*)get_tree()->get_root(), "RobloxPlayer");
        if (p && p->has_method("set_lock_first_person")) p->call("set_lock_first_person", cam_lock);
    }
    void _apply_sensitivity() {
        if (!is_inside_tree()) return;
        Node* p = _find_by_class((Node*)get_tree()->get_root(), "RobloxPlayer");
        if (p) p->call("set_mouse_sensitivity", 0.0022 * sensitivity);
    }

    void _refresh_labels() {
        // Calidad 1..10 estilo Roblox: se muestra el número (10 = máxima).
        if (quality_val) quality_val->set_text(String::num_int64(quality) + "/10");
        if (fps_val) fps_val->set_text(FPS_OPTIONS[fps_idx] == 0 ? String("Unlimited") : String::num_int64(FPS_OPTIONS[fps_idx]));
        if (volume_val) volume_val->set_text(String::num_int64((int64_t)volume) + "%");
        if (sens_val) sens_val->set_text(String::num((double)sensitivity, 1) + "x");
        Node* ns = _netservice();
        if (ping_val) {
            double ping = ns ? (double)ns->call("get_ping_ms") : -1.0;
            ping_val->set_text(ping < 0.0 ? String("--") : (String::num_int64((int64_t)ping) + " ms"));
        }
    }

    // ── Loop: HUD + confirmacion de salida + reintento sensibilidad ──
    void _process(double dt) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        if (leave_armed) {
            leave_timer -= dt;
            if (leave_timer <= 0.0) {
                leave_armed = false;
                if (leave_btn) leave_btn->set_text("Leave Game");
            }
        }
        if (sens_retry > 0.0) {
            sens_retry -= dt;
            if (sens_retry <= 0.0) _apply_sensitivity();   // el jugador ya existe
        }
        hud_timer += dt;
        if (hud_timer < 0.25) return;
        hud_timer = 0.0;
        bool any = show_fps || show_ping;
        if (hud) hud->set_visible(any);
        if (any && hud) {
            String txt;
            if (show_fps) txt += String("FPS: ") + String::num_int64((int64_t)Engine::get_singleton()->get_frames_per_second());
            if (show_ping) {
                Node* ns = _netservice();
                double ping = ns ? (double)ns->call("get_ping_ms") : -1.0;
                if (!txt.is_empty()) txt += "   ";
                txt += String("Ping: ") + (ping < 0.0 ? String("--") : (String::num_int64((int64_t)ping) + " ms"));
            }
            hud->set_text(txt);
        }
        if (is_open) {
            _refresh_labels();
            if (tab_active == 0) _refresh_players();
        }
    }

    // ── Persistencia por jugador ──────────────────────────────────────
    void _load_settings() {
        Ref<ConfigFile> cf; cf.instantiate();
        if (cf->load(_cfg_path()) == OK) {
            quality     = Math::clamp((int)(int64_t)cf->get_value("settings", "quality", 8), 1, 10);
            cam_lock    = (bool)cf->get_value("settings", "cam_lock", false);
            fps_idx     = Math::clamp((int)(int64_t)cf->get_value("settings", "fps_idx", 1), 0, FPS_COUNT - 1);
            show_fps    = (bool)cf->get_value("settings", "show_fps", false);
            show_ping   = (bool)cf->get_value("settings", "show_ping", false);
            volume      = Math::clamp((double)cf->get_value("settings", "volume", 100.0), 0.0, 100.0);
            sensitivity = Math::clamp((double)cf->get_value("settings", "sensitivity", 1.0), 0.2, 3.0);
        }
    }
    void _save_settings() {
        Ref<ConfigFile> cf; cf.instantiate();
        cf->set_value("settings", "quality",     quality);
        cf->set_value("settings", "cam_lock",    cam_lock);
        cf->set_value("settings", "fps_idx",     fps_idx);
        cf->set_value("settings", "show_fps",    show_fps);
        cf->set_value("settings", "show_ping",   show_ping);
        cf->set_value("settings", "volume",      volume);
        cf->set_value("settings", "sensitivity", sensitivity);
        cf->save(_cfg_path());
    }

    GLSettingsMenu() {}
};

#endif // GL_SETTINGS_MENU_H
