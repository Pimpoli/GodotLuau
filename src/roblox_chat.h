#ifndef ROBLOX_CHAT_H
#define ROBLOX_CHAT_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/v_scroll_bar.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ════════════════════════════════════════════════════════════════════
//  RobloxChat — Roblox-style chat system
////
//  RobloxChat — Sistema de chat tipo Roblox
//
//  Modern minimalist layout:
////
//  Diseño minimalista moderno:
//  ┌───────────────────────────────┐
//  │  PlayerName: message          │  ← RichTextLabel with BBCode
//  │  OtherPlayer: another message │
//  │  ...                         │
//  ├───────────────────────────────┤
//  │  Type something...  [ Send ]  │  ← LineEdit + Button
//  └───────────────────────────────┘
//
//  Open/close chat: / key
//  Send: Enter key or Send button
////
//  Para abrir/cerrar el chat: tecla /
//  Para enviar: tecla Enter o botón Enviar
// ════════════════════════════════════════════════════════════════════
class RobloxChat : public Node {
    GDCLASS(RobloxChat, Node);

private:
    CanvasLayer* canvas     = nullptr;
    Panel* main_panel = nullptr;
    ScrollContainer* scroll   = nullptr;
    RichTextLabel* msg_label  = nullptr;
    LineEdit* input_field = nullptr;
    Button* send_btn   = nullptr;

    String player_name = "Player";
    bool   chat_open   = false;
    int    max_messages = 50;
    int    message_count = 0;

    // Dark minimalist theme alpha constants
    //// Colores del tema minimalista oscuro
    static constexpr float PANEL_ALPHA  = 0.82f;
    static constexpr float INPUT_ALPHA  = 0.90f;

    // Create a minimalist dark StyleBox
    //// Crear StyleBox oscuro minimalista
    Ref<StyleBoxFlat> make_style(Color bg, float radius = 4.0f, Color border = Color(0, 0, 0, 0)) {
        Ref<StyleBoxFlat> s; s.instantiate();
        s->set_bg_color(bg);
        s->set_corner_radius_all((int)radius);
        s->set_border_color(border);
        if (border.a > 0.0f) {
            s->set_border_width_all(1);
        }
        s->set_content_margin_all(8.0f);
        return s;
    }

    void build_ui() {
        // ── CanvasLayer: always rendered on top / siempre encima de todo ──────────────────
        canvas = memnew(CanvasLayer);
        canvas->set_name("ChatLayer");
        canvas->set_layer(120);
        add_child(canvas);

        // ── Main panel (bottom-left corner) / Panel principal (esquina inferior izquierda) ──
        main_panel = memnew(Panel);
        main_panel->set_name("ChatPanel");
        main_panel->set_visible(false);

        // Anchor to bottom-left corner / Anclaje a esquina inferior izquierda
        main_panel->set_anchor(SIDE_LEFT,   0.0f);
        main_panel->set_anchor(SIDE_RIGHT,  0.0f);
        main_panel->set_anchor(SIDE_TOP,    1.0f);
        main_panel->set_anchor(SIDE_BOTTOM, 1.0f);
        main_panel->set_offset(SIDE_LEFT,   10.0f);
        main_panel->set_offset(SIDE_RIGHT,  380.0f);
        main_panel->set_offset(SIDE_TOP,   -220.0f);
        main_panel->set_offset(SIDE_BOTTOM, -10.0f);

        Color panel_bg = Color(0.08f, 0.08f, 0.10f, PANEL_ALPHA);
        main_panel->add_theme_stylebox_override("panel", make_style(panel_bg, 6.0f));
        canvas->add_child(main_panel);

        // ── Vertical layout inside the panel / Layout vertical dentro del panel ──────────
        VBoxContainer* vbox = memnew(VBoxContainer);
        vbox->set_name("VBox");
        vbox->set_anchors_preset(Control::PRESET_FULL_RECT);
        vbox->set_offset(SIDE_LEFT,   8.0f);
        vbox->set_offset(SIDE_RIGHT, -8.0f);
        vbox->set_offset(SIDE_TOP,    8.0f);
        vbox->set_offset(SIDE_BOTTOM,-8.0f);
        main_panel->add_child(vbox);

        // ── ScrollContainer for messages / ScrollContainer para los mensajes ──────────────
        scroll = memnew(ScrollContainer);
        scroll->set_name("Scroll");
        scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
        scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
        vbox->add_child(scroll);

        // ── RichTextLabel: messages with BBCode / mensajes con BBCode ──────────────────────
        msg_label = memnew(RichTextLabel);
        msg_label->set_name("Messages");
        msg_label->set_use_bbcode(true);
        msg_label->set_scroll_active(false); // scroll managed by ScrollContainer / el scroll lo maneja ScrollContainer
        msg_label->set_fit_content(true);
        msg_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);

        // Message area style / Estilo del área de mensajes
        msg_label->add_theme_color_override("default_color", Color(0.90f, 0.90f, 0.90f));
        msg_label->add_theme_font_size_override("normal_font_size", 14);
        scroll->add_child(msg_label);

        // ── Visual separator / Separador visual ──────────────────────────────────────────
        Panel* sep = memnew(Panel);
        sep->set_custom_minimum_size(Vector2(0.0f, 1.0f));
        Ref<StyleBoxFlat> sep_style = make_style(Color(0.3f, 0.3f, 0.35f, 0.8f), 0.0f);
        sep->add_theme_stylebox_override("panel", sep_style);
        vbox->add_child(sep);

        // ── Input row / Fila de input ──────────────────────────────────────────────────────
        HBoxContainer* hbox = memnew(HBoxContainer);
        hbox->set_name("InputRow");
        hbox->add_theme_constant_override("separation", 6);
        vbox->add_child(hbox);

        // LineEdit for typing / LineEdit para escribir
        input_field = memnew(LineEdit);
        input_field->set_name("ChatInput");
        input_field->set_placeholder("Escribe un mensaje...");
        input_field->set_h_size_flags(Control::SIZE_EXPAND_FILL);

        Color input_bg = Color(0.13f, 0.13f, 0.16f, INPUT_ALPHA);
        input_field->add_theme_stylebox_override("normal",  make_style(input_bg, 4.0f, Color(0.3f, 0.3f, 0.4f, 0.6f)));
        input_field->add_theme_stylebox_override("focused", make_style(input_bg, 4.0f, Color(0.45f, 0.55f, 0.90f, 0.8f)));
        input_field->add_theme_color_override("font_color",            Color(0.92f, 0.92f, 0.92f));
        input_field->add_theme_color_override("font_placeholder_color", Color(0.45f, 0.45f, 0.50f));
        input_field->add_theme_font_size_override("font_size", 14);
        hbox->add_child(input_field);

        // Send button / Botón Enviar
        send_btn = memnew(Button);
        send_btn->set_name("SendButton");
        send_btn->set_text("Enviar");
        send_btn->set_custom_minimum_size(Vector2(70.0f, 0.0f));

        Color btn_normal = Color(0.20f, 0.35f, 0.75f, 0.9f);
        Color btn_hover  = Color(0.30f, 0.45f, 0.90f, 1.0f);
        send_btn->add_theme_stylebox_override("normal",  make_style(btn_normal, 4.0f));
        send_btn->add_theme_stylebox_override("hover",   make_style(btn_hover,  4.0f));
        send_btn->add_theme_stylebox_override("pressed", make_style(Color(0.15f, 0.28f, 0.60f), 4.0f));
        send_btn->add_theme_color_override("font_color",         Color(1.0f, 1.0f, 1.0f));
        send_btn->add_theme_color_override("font_hover_color",   Color(1.0f, 1.0f, 1.0f));
        send_btn->add_theme_color_override("font_pressed_color", Color(0.85f, 0.85f, 1.0f));
        send_btn->add_theme_font_size_override("font_size", 13);
        hbox->add_child(send_btn);

        // ── Connect signals / Conectar señales ────────────────────────────────────────────
        send_btn->connect("pressed", Callable(this, "_on_send_pressed"));
        input_field->connect("text_submitted", Callable(this, "_on_text_submitted"));
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("send_message", "player_name", "message"),
                             &RobloxChat::send_message);
        ClassDB::bind_method(D_METHOD("set_player_name", "name"),
                             &RobloxChat::set_player_name);
        ClassDB::bind_method(D_METHOD("toggle_chat"),
                             &RobloxChat::toggle_chat);
        ClassDB::bind_method(D_METHOD("_on_send_pressed"),
                             &RobloxChat::_on_send_pressed);
        ClassDB::bind_method(D_METHOD("_on_text_submitted", "text"),
                             &RobloxChat::_on_text_submitted);

        // Signal emitted when someone sends a message
        //// Señal para cuando alguien envía un mensaje
        ADD_SIGNAL(MethodInfo("MessageSent",
            PropertyInfo(Variant::STRING, "player_name"),
            PropertyInfo(Variant::STRING, "message")));
    }

public:
    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        build_ui();
        UtilityFunctions::print("[GodotLuau] Chat system initialized. Press / to open.");
    }

    void _input(const Ref<InputEvent>& p_event) override {
        if (Engine::get_singleton()->is_editor_hint() || !main_panel) return;

        Ref<InputEventKey> key = p_event;
        if (key.is_valid() && key->is_pressed() && !key->is_echo()) {
            // / to open/close chat / para abrir/cerrar el chat
            if (key->get_keycode() == KEY_SLASH && !chat_open) {
                toggle_chat();
                // Open and pre-fill with / / Abrir y escribir /
                if (input_field) {
                    input_field->set_text("/");
                    input_field->set_caret_column(1);
                }
                get_viewport()->set_input_as_handled();
            }
            // Enter to open empty chat / Enter para abrir el chat vacío
            else if (key->get_keycode() == KEY_ENTER && !chat_open) {
                toggle_chat();
                get_viewport()->set_input_as_handled();
            }
            // Escape to close / Escape para cerrar
            else if (key->get_keycode() == KEY_ESCAPE && chat_open) {
                close_chat();
                get_viewport()->set_input_as_handled();
            }
        }
    }

    // ── Public API / API pública ──────────────────────────────────────

    // Send a message to the chat (callable from Luau)
    //// Enviar un mensaje al chat (puede llamarse desde Luau)
    void send_message(String p_player, String p_msg) {
        if (!msg_label || p_msg.is_empty()) return;

        // Name color by hash of player name (variety of colors)
        // Vibrant but not aggressive palette for dark mode
        ////
        // Colores de nombre por hash del nombre (variedad de colores)
        // Paleta de colores vivos pero no agresivos para modo oscuro
        Color name_colors[] = {
            Color(0.40f, 0.76f, 0.98f), // Light blue / Azul claro
            Color(0.98f, 0.73f, 0.40f), // Soft orange / Naranja suave
            Color(0.67f, 0.95f, 0.60f), // Light green / Verde claro
            Color(0.95f, 0.60f, 0.80f), // Pink / Rosa
            Color(0.80f, 0.65f, 0.98f), // Lilac / Lila
            Color(0.98f, 0.90f, 0.45f), // Soft yellow / Amarillo suave
        };
        int color_idx = 0;
        for (int i = 0; i < p_player.length(); i++) {
            color_idx += (int)p_player[i];
        }
        Color nc = name_colors[color_idx % 6];
        String hex = "#" + nc.to_html(false).substr(0, 6);

        // Format: [color=hex][b]Name[/b][/color]: message
        //// Formato: [color=hex][b]Nombre[/b][/color]: mensaje
        String formatted = "[color=" + hex + "][b]" + p_player + "[/b][/color][color=#c8c8cc]: " + p_msg + "[/color]\n";
        msg_label->append_text(formatted);

        message_count++;
        // Clear old messages if there are too many
        //// Limpiar mensajes viejos si hay demasiados
        if (message_count > max_messages) {
            msg_label->clear();
            message_count = 0;
        }

        // Scroll to bottom / Scroll al final
        if (scroll) {
            scroll->set_v_scroll((int)scroll->get_v_scroll_bar()->get_max());
        }

        emit_signal("MessageSent", p_player, p_msg);
    }

    void set_player_name(String name) {
        player_name = name.is_empty() ? "Player" : name;
    }

    void toggle_chat() {
        if (chat_open) {
            close_chat();
        } else {
            open_chat();
        }
    }

    void open_chat() {
        if (!main_panel) return;
        chat_open = true;
        main_panel->set_visible(true);
        if (input_field) {
            input_field->grab_focus();
            input_field->clear();
        }
        // Capture input so it doesn't move the character
        //// Capturar el input para que no mueva al personaje
        Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
    }

    void close_chat() {
        if (!main_panel) return;
        chat_open = false;
        main_panel->set_visible(false);
        if (input_field) input_field->release_focus();
    }

    bool is_chat_open() const { return chat_open; }

    // ── UI callbacks / Callbacks de UI ────────────────────────────────
    void _on_send_pressed() {
        if (!input_field) return;
        String text = input_field->get_text().strip_edges();
        if (!text.is_empty()) {
            send_message(player_name, text);
            input_field->clear();
        }
    }

    void _on_text_submitted(String text) {
        _on_send_pressed();
    }
};

#endif // ROBLOX_CHAT_H
