#ifndef ROBLOX_GUI_H
#define ROBLOX_GUI_H

// ════════════════════════════════════════════════════════════════════
//  Roblox-style GUI system for GodotLuau
//
//  ScreenGui    — Interface layer (CanvasLayer)
//  RobloxFrame  — Container/panel (Panel)
//  RobloxTextLabel   — Text (Label)
//  RobloxTextButton  — Text button (Button)
//  RobloxTextBox     — Input field (LineEdit)
//  RobloxImageLabel  — Image (TextureRect)
//  RobloxImageButton — Image button (TextureButton)
//  RobloxScrollingFrame — Scrollable container (ScrollContainer)
//
//  NOTE: Position/size uses UDim2 (Scale + Offset):
//    Size     = UDim2.new(xScale, xOffset, yScale, yOffset)
//    Position = UDim2.new(xScale, xOffset, yScale, yOffset)
////
//  Sistema de GUI tipo Roblox para GodotLuau
//
//  ScreenGui    — Capa de interfaz (CanvasLayer)
//  RobloxFrame  — Contenedor/panel (Panel)
//  RobloxTextLabel   — Texto (Label)
//  RobloxTextButton  — Botón con texto (Button)
//  RobloxTextBox     — Campo de entrada (LineEdit)
//  RobloxImageLabel  — Imagen (TextureRect)
//  RobloxImageButton — Botón imagen (TextureButton)
//  RobloxScrollingFrame — Contenedor scrollable (ScrollContainer)
//
//  NOTA: La posición/tamaño usa UDim2 (Scale + Offset):
//    Size     = UDim2.new(xScale, xOffset, yScale, yOffset)
//    Position = UDim2.new(xScale, xOffset, yScale, yOffset)
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/text_edit.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/texture_button.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/style_box_empty.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <vector>

#include "lua.h"
#include "lualib.h"

using namespace godot;

// ────────────────────────────────────────────────────────────────────
//  UDim2 layout helpers
////  Helpers de layout UDim2
// ────────────────────────────────────────────────────────────────────
struct GuiUDim2 { float xs = 0, xo = 0, ys = 0, yo = 0; };

static void _gui_apply_layout(Control* ctrl, const GuiUDim2& pos, const GuiUDim2& sz,
                               float ax = 0, float ay = 0) {
    if (!ctrl || !ctrl->is_inside_tree()) return;
    Vector2 ps;
    Control* parent_ctrl = Object::cast_to<Control>(ctrl->get_parent());
    if (parent_ctrl) {
        ps = parent_ctrl->get_size();
    } else {
        Viewport* vp = ctrl->get_viewport();
        ps = vp ? vp->get_visible_rect().size : Vector2(800, 600);
    }
    float w = ps.x * sz.xs + sz.xo;
    float h = ps.y * sz.ys + sz.yo;
    float x = ps.x * pos.xs + pos.xo - w * ax;
    float y = ps.y * pos.ys + pos.yo - h * ay;
    ctrl->set_position(Vector2(x, y));
    ctrl->set_size(Vector2(w, h));
}

// ────────────────────────────────────────────────────────────────────
//  Macro: common GUI fields (position, size, background color)
////  Macro: campos comunes de GUI (posición, tamaño, color de fondo)
// ────────────────────────────────────────────────────────────────────
#define GUI_COMMON_FIELDS \
    GuiUDim2 _pos;                                        \
    GuiUDim2 _sz = {0, 100, 0, 50};                      \
    float    _anchor_x = 0, _anchor_y = 0;               \
    float    _bg_r = 1, _bg_g = 1, _bg_b = 1;            \
    float    _bg_alpha = 0; /* 0=opaque, 1=transparent / 0=opaco, 1=transparente */ \
    float    _border_r = 0, _border_g = 0, _border_b = 0;\
    int      _border_px = 0;                              \
    bool     _clips_descendants = false;

#define GUI_COMMON_APPLY_LAYOUT _gui_apply_layout(this, _pos, _sz, _anchor_x, _anchor_y);

#define GUI_COMMON_METHODS(ClassName)                             \
    void set_udim2_pos(float xs, float xo, float ys, float yo) { \
        _pos = {xs, xo, ys, yo};                                  \
        if (is_inside_tree()) GUI_COMMON_APPLY_LAYOUT            \
    }                                                             \
    void set_udim2_size(float xs, float xo, float ys, float yo) {\
        _sz = {xs, xo, ys, yo};                                   \
        if (is_inside_tree()) GUI_COMMON_APPLY_LAYOUT            \
    }                                                             \
    void set_anchor_point(float ax, float ay) {                   \
        _anchor_x = ax; _anchor_y = ay;                           \
        if (is_inside_tree()) GUI_COMMON_APPLY_LAYOUT            \
    }                                                             \
    GuiUDim2 get_udim2_pos()  const { return _pos; }              \
    GuiUDim2 get_udim2_size() const { return _sz; }               \
    void set_bg_color(float r, float g, float b) {                \
        _bg_r = r; _bg_g = g; _bg_b = b;                         \
        _apply_style();                                           \
    }                                                             \
    void set_bg_alpha(float a) {                                  \
        _bg_alpha = Math::clamp(a, 0.0f, 1.0f);                  \
        _apply_style();                                           \
    }                                                             \
    void set_border_color(float r, float g, float b) {            \
        _border_r = r; _border_g = g; _border_b = b;             \
        _apply_style();                                           \
    }                                                             \
    void set_border_px(int px) {                                   \
        _border_px = px;                                          \
        _apply_style();                                           \
    }                                                             \
    float get_bg_r() const   { return _bg_r; }                    \
    float get_bg_g() const   { return _bg_g; }                    \
    float get_bg_b() const   { return _bg_b; }                    \
    float get_bg_alpha() const { return _bg_alpha; }


// ────────────────────────────────────────────────────────────────────
//  ScreenGui — Interface layer (equivalent to Godot CanvasLayer)
////  ScreenGui — Capa de interfaz (equivalente al CanvasLayer de Godot)
// ────────────────────────────────────────────────────────────────────
class ScreenGui : public CanvasLayer {
    GDCLASS(ScreenGui, CanvasLayer);

    bool enabled          = true;
    bool reset_on_spawn   = true;
    bool ignore_gui_inset = false;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_sg_enabled","b"),          &ScreenGui::set_sg_enabled);
        ClassDB::bind_method(D_METHOD("get_sg_enabled"),               &ScreenGui::get_sg_enabled);
        ClassDB::bind_method(D_METHOD("set_reset_on_spawn","b"),      &ScreenGui::set_reset_on_spawn);
        ClassDB::bind_method(D_METHOD("get_reset_on_spawn"),           &ScreenGui::get_reset_on_spawn);
        ClassDB::bind_method(D_METHOD("set_ignore_gui_inset","b"),    &ScreenGui::set_ignore_gui_inset);
        ClassDB::bind_method(D_METHOD("get_ignore_gui_inset"),         &ScreenGui::get_ignore_gui_inset);
        ClassDB::bind_method(D_METHOD("set_display_order","o"),       &ScreenGui::set_display_order);
        ClassDB::bind_method(D_METHOD("get_display_order"),            &ScreenGui::get_display_order);

        ADD_GROUP("ScreenGui","");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"Enabled"),         "set_sg_enabled","get_sg_enabled");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"ResetOnSpawn"),    "set_reset_on_spawn","get_reset_on_spawn");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"IgnoreGuiInset"),  "set_ignore_gui_inset","get_ignore_gui_inset");
        ADD_PROPERTY(PropertyInfo(Variant::INT, "DisplayOrder",     PROPERTY_HINT_RANGE,"-100,100,1"), "set_display_order","get_display_order");
    }

public:
    void set_sg_enabled(bool b)          { enabled = b; set_visible(b); }
    bool get_sg_enabled() const          { return enabled; }
    void set_reset_on_spawn(bool b)      { reset_on_spawn = b; }
    bool get_reset_on_spawn() const      { return reset_on_spawn; }
    void set_ignore_gui_inset(bool b)    { ignore_gui_inset = b; }
    bool get_ignore_gui_inset() const    { return ignore_gui_inset; }
    void set_display_order(int o)        { set_layer(o); }
    int  get_display_order() const       { return get_layer(); }
};

// ────────────────────────────────────────────────────────────────────
//  RobloxFrame — Container/panel
////  RobloxFrame — Contenedor/panel
// ────────────────────────────────────────────────────────────────────
class RobloxFrame : public Panel {
    GDCLASS(RobloxFrame, Panel);
    GUI_COMMON_FIELDS

    void _apply_style() {
        Ref<StyleBoxFlat> style;
        style.instantiate();
        style->set_bg_color(Color(_bg_r, _bg_g, _bg_b, 1.0f - _bg_alpha));
        style->set_border_color(Color(_border_r, _border_g, _border_b, 1.0f));
        style->set_border_width_all(_border_px);
        style->set_corner_radius_all(0);
        add_theme_stylebox_override("panel", style);
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            // Connect to parent to re-layout when parent resizes
            //// Conectar al padre para re-layout cuando el padre cambie de tamaño
            Control* parent_ctrl = Object::cast_to<Control>(get_parent());
            if (parent_ctrl && !parent_ctrl->is_connected("resized", Callable(this, "_on_parent_resized")))
                parent_ctrl->connect("resized", Callable(this, "_on_parent_resized"));
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("_on_parent_resized"), &RobloxFrame::_on_parent_resized);
        ClassDB::bind_method(D_METHOD("set_udim2_pos","xs","xo","ys","yo"),  &RobloxFrame::set_udim2_pos);
        ClassDB::bind_method(D_METHOD("set_udim2_size","xs","xo","ys","yo"), &RobloxFrame::set_udim2_size);
        ClassDB::bind_method(D_METHOD("set_anchor_point","ax","ay"),         &RobloxFrame::set_anchor_point);
        ClassDB::bind_method(D_METHOD("set_bg_color","r","g","b"),           &RobloxFrame::set_bg_color);
        ClassDB::bind_method(D_METHOD("set_bg_alpha","a"),                   &RobloxFrame::set_bg_alpha);
        ClassDB::bind_method(D_METHOD("set_border_color","r","g","b"),       &RobloxFrame::set_border_color);
        ClassDB::bind_method(D_METHOD("set_border_px","px"),                 &RobloxFrame::set_border_px);
        ADD_SIGNAL(MethodInfo("MouseEnter"));
        ADD_SIGNAL(MethodInfo("MouseLeave"));
    }

public:
    GUI_COMMON_METHODS(RobloxFrame)

    void _on_parent_resized() {
        GUI_COMMON_APPLY_LAYOUT
    }

    void _input_event(const Ref<InputEvent>& event, const Vector2& pos, const Vector2& shape_motion,
                      int shape_idx, bool shape_inside) {}
};

// ────────────────────────────────────────────────────────────────────
//  Callbacks for GUI signals from Luau
////  Callbacks para señales de GUI desde Luau
// ────────────────────────────────────────────────────────────────────
struct GuiLuaCallback {
    lua_State* main_L;
    int ref;
    bool active = true;
};

static void _fire_gui_cbs(std::vector<GuiLuaCallback>& cbs) {
    for (int i = (int)cbs.size() - 1; i >= 0; i--) {
        auto& cb = cbs[i];
        if (!cb.active) { cbs.erase(cbs.begin() + i); continue; }
        lua_State* th = lua_newthread(cb.main_L);
        lua_rawgeti(cb.main_L, LUA_REGISTRYINDEX, cb.ref);
        if (lua_isfunction(cb.main_L, -1)) {
            lua_xmove(cb.main_L, th, 1);
            int st = lua_resume(th, nullptr, 0);
            if (st != LUA_OK && st != LUA_YIELD)
                UtilityFunctions::print("[GUI] Error: ", lua_tostring(th, -1));
        } else { lua_pop(cb.main_L, 1); }
        lua_pop(cb.main_L, 1);
    }
}

// ────────────────────────────────────────────────────────────────────
//  RobloxTextLabel — Static text
////  RobloxTextLabel — Texto estático
// ────────────────────────────────────────────────────────────────────
class RobloxTextLabel : public Label {
    GDCLASS(RobloxTextLabel, Label);
    GUI_COMMON_FIELDS
    float _txt_r = 0, _txt_g = 0, _txt_b = 0;
    bool  _text_scaled = false;

    void _apply_style() {
        Ref<StyleBoxEmpty> empty;
        empty.instantiate();
        add_theme_stylebox_override("normal", empty);
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            add_theme_color_override("font_color", Color(_txt_r, _txt_g, _txt_b));
            if (_text_scaled) set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
            Control* parent_ctrl = Object::cast_to<Control>(get_parent());
            if (parent_ctrl && !parent_ctrl->is_connected("resized", Callable(this, "_on_parent_resized")))
                parent_ctrl->connect("resized", Callable(this, "_on_parent_resized"));
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("_on_parent_resized"), &RobloxTextLabel::_on_parent_resized);
        ClassDB::bind_method(D_METHOD("set_udim2_pos","xs","xo","ys","yo"),  &RobloxTextLabel::set_udim2_pos);
        ClassDB::bind_method(D_METHOD("set_udim2_size","xs","xo","ys","yo"), &RobloxTextLabel::set_udim2_size);
        ClassDB::bind_method(D_METHOD("set_anchor_point","ax","ay"),         &RobloxTextLabel::set_anchor_point);
        ClassDB::bind_method(D_METHOD("set_bg_color","r","g","b"),           &RobloxTextLabel::set_bg_color);
        ClassDB::bind_method(D_METHOD("set_bg_alpha","a"),                   &RobloxTextLabel::set_bg_alpha);
        ClassDB::bind_method(D_METHOD("set_border_color","r","g","b"),       &RobloxTextLabel::set_border_color);
        ClassDB::bind_method(D_METHOD("set_border_px","px"),                 &RobloxTextLabel::set_border_px);
        ClassDB::bind_method(D_METHOD("set_text_color","r","g","b"),         &RobloxTextLabel::set_text_color);
        ClassDB::bind_method(D_METHOD("set_text_size","s"),                  &RobloxTextLabel::set_text_size);
        ClassDB::bind_method(D_METHOD("set_text_scaled","b"),                &RobloxTextLabel::set_text_scaled);
    }

public:
    GUI_COMMON_METHODS(RobloxTextLabel)

    void _on_parent_resized() { GUI_COMMON_APPLY_LAYOUT }

    void set_text_color(float r, float g, float b) {
        _txt_r = r; _txt_g = g; _txt_b = b;
        add_theme_color_override("font_color", Color(r, g, b));
    }
    void set_text_size(int s) {
        add_theme_font_size_override("font_size", s);
    }
    void set_text_scaled(bool b) {
        _text_scaled = b;
        set_autowrap_mode(b ? TextServer::AUTOWRAP_WORD_SMART : TextServer::AUTOWRAP_OFF);
    }
};

// ────────────────────────────────────────────────────────────────────
//  RobloxTextButton — Text button with Roblox-style signals
////  RobloxTextButton — Botón con texto, señales tipo Roblox
// ────────────────────────────────────────────────────────────────────
class RobloxTextButton : public Button {
    GDCLASS(RobloxTextButton, Button);
    GUI_COMMON_FIELDS
    float _txt_r = 0, _txt_g = 0, _txt_b = 0;

    std::vector<GuiLuaCallback> _click_cbs;
    std::vector<GuiLuaCallback> _enter_cbs;
    std::vector<GuiLuaCallback> _leave_cbs;

    void _apply_style() {
        Ref<StyleBoxFlat> normal_style, hover_style, pressed_style;
        normal_style.instantiate();
        normal_style->set_bg_color(Color(_bg_r, _bg_g, _bg_b, 1.0f - _bg_alpha));
        normal_style->set_border_color(Color(_border_r, _border_g, _border_b));
        normal_style->set_border_width_all(_border_px);
        hover_style.instantiate();
        hover_style->set_bg_color(Color(
            Math::clamp(_bg_r + 0.1f, 0.f, 1.f),
            Math::clamp(_bg_g + 0.1f, 0.f, 1.f),
            Math::clamp(_bg_b + 0.1f, 0.f, 1.f), 1.0f - _bg_alpha));
        hover_style->set_border_color(Color(_border_r, _border_g, _border_b));
        hover_style->set_border_width_all(_border_px);
        pressed_style.instantiate();
        pressed_style->set_bg_color(Color(
            Math::clamp(_bg_r - 0.1f, 0.f, 1.f),
            Math::clamp(_bg_g - 0.1f, 0.f, 1.f),
            Math::clamp(_bg_b - 0.1f, 0.f, 1.f), 1.0f - _bg_alpha));
        pressed_style->set_border_width_all(_border_px);
        add_theme_stylebox_override("normal",  normal_style);
        add_theme_stylebox_override("hover",   hover_style);
        add_theme_stylebox_override("pressed", pressed_style);
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            add_theme_color_override("font_color", Color(_txt_r, _txt_g, _txt_b));
            connect("pressed",      Callable(this, "_on_pressed"));
            connect("mouse_entered",Callable(this, "_on_mouse_enter"));
            connect("mouse_exited", Callable(this, "_on_mouse_leave"));
            Control* parent_ctrl = Object::cast_to<Control>(get_parent());
            if (parent_ctrl && !parent_ctrl->is_connected("resized", Callable(this, "_on_parent_resized")))
                parent_ctrl->connect("resized", Callable(this, "_on_parent_resized"));
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("_on_parent_resized"), &RobloxTextButton::_on_parent_resized);
        ClassDB::bind_method(D_METHOD("_on_pressed"),        &RobloxTextButton::_on_pressed);
        ClassDB::bind_method(D_METHOD("_on_mouse_enter"),    &RobloxTextButton::_on_mouse_enter);
        ClassDB::bind_method(D_METHOD("_on_mouse_leave"),    &RobloxTextButton::_on_mouse_leave);
        ClassDB::bind_method(D_METHOD("set_udim2_pos","xs","xo","ys","yo"),  &RobloxTextButton::set_udim2_pos);
        ClassDB::bind_method(D_METHOD("set_udim2_size","xs","xo","ys","yo"), &RobloxTextButton::set_udim2_size);
        ClassDB::bind_method(D_METHOD("set_anchor_point","ax","ay"),         &RobloxTextButton::set_anchor_point);
        ClassDB::bind_method(D_METHOD("set_bg_color","r","g","b"),           &RobloxTextButton::set_bg_color);
        ClassDB::bind_method(D_METHOD("set_bg_alpha","a"),                   &RobloxTextButton::set_bg_alpha);
        ClassDB::bind_method(D_METHOD("set_border_color","r","g","b"),       &RobloxTextButton::set_border_color);
        ClassDB::bind_method(D_METHOD("set_border_px","px"),                 &RobloxTextButton::set_border_px);
        ClassDB::bind_method(D_METHOD("set_text_color","r","g","b"),         &RobloxTextButton::set_text_color);
        ClassDB::bind_method(D_METHOD("set_text_size","s"),                  &RobloxTextButton::set_text_size);
        ADD_SIGNAL(MethodInfo("MouseButton1Click"));
        ADD_SIGNAL(MethodInfo("MouseEnter"));
        ADD_SIGNAL(MethodInfo("MouseLeave"));
    }

public:
    GUI_COMMON_METHODS(RobloxTextButton)

    void _on_parent_resized() { GUI_COMMON_APPLY_LAYOUT }
    void _on_pressed()        { emit_signal("MouseButton1Click"); _fire_gui_cbs(_click_cbs); }
    void _on_mouse_enter()    { emit_signal("MouseEnter");        _fire_gui_cbs(_enter_cbs); }
    void _on_mouse_leave()    { emit_signal("MouseLeave");        _fire_gui_cbs(_leave_cbs); }

    void set_text_color(float r, float g, float b) {
        _txt_r = r; _txt_g = g; _txt_b = b;
        add_theme_color_override("font_color", Color(r, g, b));
    }
    void set_text_size(int s)  { add_theme_font_size_override("font_size", s); }

    void add_click_cb(lua_State* L, int ref)  { _click_cbs.push_back({L, ref, true}); }
    void add_enter_cb(lua_State* L, int ref)  { _enter_cbs.push_back({L, ref, true}); }
    void add_leave_cb(lua_State* L, int ref)  { _leave_cbs.push_back({L, ref, true}); }
};

// ────────────────────────────────────────────────────────────────────
//  RobloxTextBox — Text input field
////  RobloxTextBox — Campo de entrada de texto
// ────────────────────────────────────────────────────────────────────
class RobloxTextBox : public LineEdit {
    GDCLASS(RobloxTextBox, LineEdit);
    GUI_COMMON_FIELDS
    float _txt_r = 0, _txt_g = 0, _txt_b = 0;

    std::vector<GuiLuaCallback> _focus_lost_cbs;
    std::vector<GuiLuaCallback> _focus_gained_cbs;
    std::vector<GuiLuaCallback> _changed_cbs;

    void _apply_style() {
        Ref<StyleBoxFlat> style;
        style.instantiate();
        style->set_bg_color(Color(_bg_r, _bg_g, _bg_b, 1.0f - _bg_alpha));
        style->set_border_color(Color(_border_r, _border_g, _border_b));
        style->set_border_width_all(_border_px);
        add_theme_stylebox_override("normal", style);
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            add_theme_color_override("font_color", Color(_txt_r, _txt_g, _txt_b));
            connect("focus_exited",  Callable(this, "_on_focus_lost"));
            connect("focus_entered", Callable(this, "_on_focus_gained"));
            connect("text_changed",  Callable(this, "_on_changed"));
            Control* parent_ctrl = Object::cast_to<Control>(get_parent());
            if (parent_ctrl && !parent_ctrl->is_connected("resized", Callable(this, "_on_parent_resized")))
                parent_ctrl->connect("resized", Callable(this, "_on_parent_resized"));
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("_on_parent_resized"), &RobloxTextBox::_on_parent_resized);
        ClassDB::bind_method(D_METHOD("_on_focus_lost"),     &RobloxTextBox::_on_focus_lost);
        ClassDB::bind_method(D_METHOD("_on_focus_gained"),   &RobloxTextBox::_on_focus_gained);
        ClassDB::bind_method(D_METHOD("_on_changed","t"),    &RobloxTextBox::_on_changed);
        ClassDB::bind_method(D_METHOD("set_udim2_pos","xs","xo","ys","yo"),  &RobloxTextBox::set_udim2_pos);
        ClassDB::bind_method(D_METHOD("set_udim2_size","xs","xo","ys","yo"), &RobloxTextBox::set_udim2_size);
        ClassDB::bind_method(D_METHOD("set_anchor_point","ax","ay"),         &RobloxTextBox::set_anchor_point);
        ClassDB::bind_method(D_METHOD("set_bg_color","r","g","b"),           &RobloxTextBox::set_bg_color);
        ClassDB::bind_method(D_METHOD("set_bg_alpha","a"),                   &RobloxTextBox::set_bg_alpha);
        ClassDB::bind_method(D_METHOD("set_border_color","r","g","b"),       &RobloxTextBox::set_border_color);
        ClassDB::bind_method(D_METHOD("set_border_px","px"),                 &RobloxTextBox::set_border_px);
        ClassDB::bind_method(D_METHOD("set_text_color","r","g","b"),         &RobloxTextBox::set_text_color);
        ADD_SIGNAL(MethodInfo("FocusLost"));
        ADD_SIGNAL(MethodInfo("FocusGained"));
        ADD_SIGNAL(MethodInfo("Changed", PropertyInfo(Variant::STRING,"text")));
    }

public:
    GUI_COMMON_METHODS(RobloxTextBox)

    void _on_parent_resized() { GUI_COMMON_APPLY_LAYOUT }
    void _on_focus_lost()     { emit_signal("FocusLost");   _fire_gui_cbs(_focus_lost_cbs); }
    void _on_focus_gained()   { emit_signal("FocusGained"); _fire_gui_cbs(_focus_gained_cbs); }
    void _on_changed(String t){ emit_signal("Changed", t); }

    void set_text_color(float r, float g, float b) {
        _txt_r = r; _txt_g = g; _txt_b = b;
        add_theme_color_override("font_color", Color(r, g, b));
    }

    void add_focus_lost_cb(lua_State* L, int ref)   { _focus_lost_cbs.push_back({L, ref, true}); }
    void add_focus_gained_cb(lua_State* L, int ref) { _focus_gained_cbs.push_back({L, ref, true}); }
};

// ────────────────────────────────────────────────────────────────────
//  RobloxImageLabel — Static image
////  RobloxImageLabel — Imagen estática
// ────────────────────────────────────────────────────────────────────
class RobloxImageLabel : public TextureRect {
    GDCLASS(RobloxImageLabel, TextureRect);
    GUI_COMMON_FIELDS
    String _image_path;
    float  _img_r = 1, _img_g = 1, _img_b = 1, _img_alpha = 1;

    void _apply_style() {
        set_modulate(Color(_img_r, _img_g, _img_b, _img_alpha));
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            Control* parent_ctrl = Object::cast_to<Control>(get_parent());
            if (parent_ctrl && !parent_ctrl->is_connected("resized", Callable(this, "_on_parent_resized")))
                parent_ctrl->connect("resized", Callable(this, "_on_parent_resized"));
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("_on_parent_resized"), &RobloxImageLabel::_on_parent_resized);
        ClassDB::bind_method(D_METHOD("set_udim2_pos","xs","xo","ys","yo"),  &RobloxImageLabel::set_udim2_pos);
        ClassDB::bind_method(D_METHOD("set_udim2_size","xs","xo","ys","yo"), &RobloxImageLabel::set_udim2_size);
        ClassDB::bind_method(D_METHOD("set_anchor_point","ax","ay"),         &RobloxImageLabel::set_anchor_point);
        ClassDB::bind_method(D_METHOD("set_bg_color","r","g","b"),           &RobloxImageLabel::set_bg_color);
        ClassDB::bind_method(D_METHOD("set_bg_alpha","a"),                   &RobloxImageLabel::set_bg_alpha);
        ClassDB::bind_method(D_METHOD("set_border_color","r","g","b"),       &RobloxImageLabel::set_border_color);
        ClassDB::bind_method(D_METHOD("set_border_px","px"),                 &RobloxImageLabel::set_border_px);
        ClassDB::bind_method(D_METHOD("set_image","path"),                   &RobloxImageLabel::set_image);
        ClassDB::bind_method(D_METHOD("set_image_color","r","g","b"),        &RobloxImageLabel::set_image_color);
        ClassDB::bind_method(D_METHOD("set_image_transparency","a"),         &RobloxImageLabel::set_image_transparency);
    }

public:
    GUI_COMMON_METHODS(RobloxImageLabel)

    void _on_parent_resized() { GUI_COMMON_APPLY_LAYOUT }

    void set_image(String path) {
        _image_path = path;
        if (path.is_empty()) return;
        Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(path);
        if (tex.is_valid()) set_texture(tex);
    }

    void set_image_color(float r, float g, float b) {
        _img_r = r; _img_g = g; _img_b = b;
        set_modulate(Color(_img_r, _img_g, _img_b, _img_alpha));
    }

    void set_image_transparency(float a) {
        _img_alpha = 1.0f - Math::clamp(a, 0.0f, 1.0f);
        set_modulate(Color(_img_r, _img_g, _img_b, _img_alpha));
    }
};

// ────────────────────────────────────────────────────────────────────
//  RobloxScrollingFrame — Scrollable container
////  RobloxScrollingFrame — Contenedor con scroll
// ────────────────────────────────────────────────────────────────────
class RobloxScrollingFrame : public ScrollContainer {
    GDCLASS(RobloxScrollingFrame, ScrollContainer);
    GUI_COMMON_FIELDS
    GuiUDim2 _canvas_size = {0, 400, 0, 400};
    bool     _scrolling_enabled = true;

    void _apply_style() {
        // ScrollContainer background
        //// Fondo del ScrollContainer
        Ref<StyleBoxFlat> style;
        style.instantiate();
        style->set_bg_color(Color(_bg_r, _bg_g, _bg_b, 1.0f - _bg_alpha));
        style->set_border_color(Color(_border_r, _border_g, _border_b));
        style->set_border_width_all(_border_px);
        add_theme_stylebox_override("panel", style);
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            Control* parent_ctrl = Object::cast_to<Control>(get_parent());
            if (parent_ctrl && !parent_ctrl->is_connected("resized", Callable(this, "_on_parent_resized")))
                parent_ctrl->connect("resized", Callable(this, "_on_parent_resized"));
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("_on_parent_resized"), &RobloxScrollingFrame::_on_parent_resized);
        ClassDB::bind_method(D_METHOD("set_udim2_pos","xs","xo","ys","yo"),       &RobloxScrollingFrame::set_udim2_pos);
        ClassDB::bind_method(D_METHOD("set_udim2_size","xs","xo","ys","yo"),      &RobloxScrollingFrame::set_udim2_size);
        ClassDB::bind_method(D_METHOD("set_anchor_point","ax","ay"),              &RobloxScrollingFrame::set_anchor_point);
        ClassDB::bind_method(D_METHOD("set_bg_color","r","g","b"),                &RobloxScrollingFrame::set_bg_color);
        ClassDB::bind_method(D_METHOD("set_bg_alpha","a"),                        &RobloxScrollingFrame::set_bg_alpha);
        ClassDB::bind_method(D_METHOD("set_border_color","r","g","b"),            &RobloxScrollingFrame::set_border_color);
        ClassDB::bind_method(D_METHOD("set_border_px","px"),                      &RobloxScrollingFrame::set_border_px);
        ClassDB::bind_method(D_METHOD("set_scrolling_enabled","b"),               &RobloxScrollingFrame::set_scrolling_enabled_val);
        ClassDB::bind_method(D_METHOD("get_scrolling_enabled"),                   &RobloxScrollingFrame::get_scrolling_enabled_val);

        ADD_GROUP("ScrollingFrame","");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"ScrollingEnabled"), "set_scrolling_enabled","get_scrolling_enabled");
    }

public:
    GUI_COMMON_METHODS(RobloxScrollingFrame)

    void _on_parent_resized() { GUI_COMMON_APPLY_LAYOUT }

    void set_scrolling_enabled_val(bool b) {
        _scrolling_enabled = b;
        // Disable scroll bars if scrolling is disabled
        //// Desactivar barras de scroll si se deshabilita
        if (!b) {
            set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
            set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
        } else {
            set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_AUTO);
            set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_AUTO);
        }
    }
    bool get_scrolling_enabled_val() const { return _scrolling_enabled; }
};

#endif // ROBLOX_GUI_H
