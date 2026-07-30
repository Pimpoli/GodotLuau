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
//  RobloxImageButton — Image button (Button + icon)
//  RobloxScrollingFrame — Scrollable container (Panel + clip + scroll)
//
//  NOTE: Position/size uses UDim2 (Scale + Offset):
//    Size     = UDim2.new(xScale, xOffset, yScale, yOffset)
//    Position = UDim2.new(xScale, xOffset, yScale, yOffset)
////
//  Sistema de GUI tipo Roblox para GodotLuau
//
//  Cada clase expone el juego COMPLETO de propiedades de su equivalente en
//  Roblox (LayoutOrder, ZIndex, SizeConstraint, AutomaticSize, ScaleType,
//  RichText, CanvasSize…). Se exponen con el nombre PascalCase de Roblox, asi
//  que el puente Lua (_gl_bridge) las lee y escribe igual que en Studio, el
//  importador de .rbxl las aplica directamente y salen en el inspector de Godot.
//
//  La colocacion (UDim2 -> rect) y las constraints viven en gl_gui_core.h,
//  porque los modificadores (UIListLayout, UIPadding…) usan la misma
//  semantica y se compilan antes.
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/text_edit.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/atlas_texture.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_pan_gesture.hpp>
#include "gl_asset.h"
#include "gl_gui_core.h"
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
#include "gl_errors.h"
#include "gl_runtime.h"

using namespace godot;

// Re-aplica los modificadores hijos (UICorner/UIStroke/UIGradient…) tras
// reconstruir el StyleBox del control: como _apply_style crea una caja nueva,
// el redondeo/borde se perdía en cuanto el fondo cambiaba (1.15).
static void _gl_gui_reapply_modifiers(Control* c) {
    if (!c) return;
    for (int i = 0; i < c->get_child_count(); i++) {
        Node* ch = c->get_child(i);
        if (ch && ch->has_method("_apply")) ch->call_deferred("_apply");
    }
}

// Avisa al layout del padre (UIListLayout/UIGridLayout) de que hay que
// recolocar: es lo que hace que cambiar LayoutOrder desde un script reordene
// la lista, igual que en Roblox.
static void _gl_gui_bump_parent_layout(Node* self) {
    if (!self) return;
    Node* p = self->get_parent();
    if (!p) return;
    for (int i = 0; i < p->get_child_count(); i++) {
        Node* c = p->get_child(i);
        if (!c) continue;
        if (c->has_method("_gl_invalidate")) c->call("_gl_invalidate");
    }
}

// Reconecta el re-layout: al padre si es Control, o al VIEWPORT si es top-level
// (padre CanvasLayer/ScreenGui). Sin esto una GUI top-level nunca se reajustaba
// al cambiar el tamaño de pantalla — el clásico "todo amontonado arriba-izquierda".
static void _gl_gui_wire_relayout(Control* self, const char* method) {
    if (!self) return;
    // CONNECT_DEFERRED es imprescindible: set_offset() emite "resized" DENTRO
    // del propio layout, y con conexion directa cada ajuste reentraba en el
    // layout de los hijos. Con 253 constraints anidadas el juego se quedaba
    // clavado. Diferido, cada nivel se recoloca en el siguiente idle y
    // converge en un par de frames.
    const uint32_t DEFER = Object::CONNECT_DEFERRED;
    Control* parent_ctrl = Object::cast_to<Control>(self->get_parent());
    if (parent_ctrl) {
        if (!parent_ctrl->is_connected("resized", Callable(self, method)))
            parent_ctrl->connect("resized", Callable(self, method), DEFER);
    } else if (Viewport* vp = self->get_viewport()) {
        if (!vp->is_connected("size_changed", Callable(self, method)))
            vp->connect("size_changed", Callable(self, method), DEFER);
    }
    // El propio elemento tambien se re-mide cuando cambia de tamaño: es lo que
    // mantiene el pivote de Rotation y el ajuste de TextScaled al dia.
    if (!self->is_connected("resized", Callable(self, "_gl_on_self_resized")))
        self->connect("resized", Callable(self, "_gl_on_self_resized"), DEFER);
    self->call_deferred(method);
}

// Quita las etiquetas de RichText de Roblox (<b>, <font color="#fff">…). Godot
// las pintaria literalmente en un Label; en Roblox con RichText=true forman
// parte del formato, no del texto.
static String _gl_strip_rich_tags(const String& s) {
    if (s.find("<") < 0) return s;
    String out;
    bool in_tag = false;
    for (int i = 0; i < s.length(); i++) {
        const char32_t c = s[i];
        if (c == '<') { in_tag = true; continue; }
        if (c == '>') { in_tag = false; continue; }
        if (!in_tag) out += String::chr(c);
    }
    out = out.replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", "\"")
             .replace("&apos;", "'").replace("&amp;", "&");
    return out;
}

// ────────────────────────────────────────────────────────────────────
//  Propiedades de GuiObject de Roblox que NO son simples setters de la clase.
//  Un solo sitio para el importador y para el repintado desde metadatos.
////  Roblox GuiObject properties that need translation (not plain setters).
inline bool gl_apply_rbx_gui_extra(Node* n, const String& prop, const Variant& v) {
    Control* c = Object::cast_to<Control>(n);
    if (!c) return false;

    // ── Cualquier GuiObject ──────────────────────────────────────────
    if (prop == "ClipsDescendants") { c->set_clip_contents((bool)v); return true; }
    if (prop == "Visible")          { c->set_visible((bool)v); return true; }
    if (prop == "Rotation") {
        if (c->has_method("set_rbx_rotation")) { c->call("set_rbx_rotation", (float)v); return true; }
        c->set_pivot_offset(c->get_size() * 0.5f);
        c->set_rotation((float)Math::deg_to_rad((double)v));
        return true;
    }

    // ── Texto (Label y Button) ───────────────────────────────────────
    // Enum.TextXAlignment de Roblox: Left=0, Right=1, Center=2 (¡Right antes que
    // Center!). Confundir ese orden descoloca todos los textos del juego.
    if (prop == "TextXAlignment") {
        const int rb = (int)(int64_t)v;
        HorizontalAlignment h = (rb == 1) ? HORIZONTAL_ALIGNMENT_RIGHT
                             : (rb == 2) ? HORIZONTAL_ALIGNMENT_CENTER
                                         : HORIZONTAL_ALIGNMENT_LEFT;
        if (Label* l = Object::cast_to<Label>(c))   { l->set_horizontal_alignment(h); return true; }
        if (Button* b = Object::cast_to<Button>(c)) { b->set_text_alignment(h); return true; }
        if (LineEdit* e = Object::cast_to<LineEdit>(c)) { e->set_horizontal_alignment(h); return true; }
        return false;
    }
    // Enum.TextYAlignment: Top=0, Center=1, Bottom=2
    if (prop == "TextYAlignment") {
        const int rb = (int)(int64_t)v;
        VerticalAlignment va = (rb == 1) ? VERTICAL_ALIGNMENT_CENTER
                            : (rb == 2) ? VERTICAL_ALIGNMENT_BOTTOM
                                        : VERTICAL_ALIGNMENT_TOP;
        if (Label* l = Object::cast_to<Label>(c)) { l->set_vertical_alignment(va); return true; }
        return false;
    }
    if (prop == "TextWrapped") {
        if (Label* l = Object::cast_to<Label>(c)) {
            l->set_autowrap_mode((bool)v ? TextServer::AUTOWRAP_WORD_SMART : TextServer::AUTOWRAP_OFF);
            return true;
        }
        return false;
    }
    // Enum.TextTruncate: None=0, AtEnd=1, SplitWord=2
    if (prop == "TextTruncate") {
        if (Label* l = Object::cast_to<Label>(c)) {
            const int t = (int)(int64_t)v;
            l->set_text_overrun_behavior(t == 0 ? TextServer::OVERRUN_NO_TRIMMING
                                                : TextServer::OVERRUN_TRIM_ELLIPSIS);
            return true;
        }
        return false;
    }
    if (prop == "RichText") {
        if (c->has_method("set_rich_text")) { c->call("set_rich_text", (bool)v); return true; }
        return false;
    }
    if (prop == "LineHeight") {
        Ref<Font> f = c->get_theme_font("font");
        const int base = c->get_theme_font_size("font_size");
        c->add_theme_constant_override("line_spacing", (int)(((float)v - 1.0f) * (float)MAX(1, base)));
        return true;
    }
    if (prop == "MaxVisibleGraphemes") {
        if (Label* l = Object::cast_to<Label>(c)) {
            const int g = (int)(int64_t)v;
            l->set_visible_characters(g < 0 ? -1 : g);
            return true;
        }
        return false;
    }
    // TextTransparency: 0 = opaco, 1 = invisible (al reves que el alfa de Godot)
    if (prop == "TextTransparency") {
        const float a = 1.0f - Math::clamp((float)v, 0.0f, 1.0f);
        Color col = c->get_theme_color("font_color");
        col.a = a;
        c->add_theme_color_override("font_color", col);
        return true;
    }
    // Contorno del texto: en Roblox se usa muchisimo para que el texto se lea
    // sobre cualquier fondo. TextStrokeTransparency 0 = contorno bien visible.
    if (prop == "TextStrokeTransparency") {
        const float vis = 1.0f - Math::clamp((float)v, 0.0f, 1.0f);
        c->add_theme_constant_override("outline_size", vis > 0.02f ? (int)(vis * 6.0f) + 2 : 0);
        return true;
    }
    if (prop == "TextStrokeColor3") {
        if (v.get_type() != Variant::COLOR) return false;
        Color col = v;
        c->add_theme_color_override("font_outline_color", col);
        return true;
    }
    return false;
}

// ────────────────────────────────────────────────────────────────────
//  Aplica las props de GUI de Roblox que el importador dejo como METADATA.
//  NO se borran las metas: así sobreviven a duplicate() y el ScreenGui clonado
//  a PlayerGui (o un reparent) las vuelve a aplicar. Reaplicar es idempotente.
////  Apply Roblox GUI props the importer left as METADATA.
static void gl_apply_rbx_gui_meta(Node* n) {
    if (!n) return;
    auto take = [&](const char* k) -> Variant { return n->has_meta(k) ? n->get_meta(k) : Variant(); };
    if (n->has_meta("Position")) { Variant v = take("Position");
        if (v.get_type() == Variant::VECTOR4 && n->has_method("set_udim2_pos")) {
            Vector4 u = v; n->call("set_udim2_pos", u.x, u.y, u.z, u.w); } }
    if (n->has_meta("Size")) { Variant v = take("Size");
        if (v.get_type() == Variant::VECTOR4 && n->has_method("set_udim2_size")) {
            Vector4 u = v; n->call("set_udim2_size", u.x, u.y, u.z, u.w); } }
    if (n->has_meta("AnchorPoint")) { Variant v = take("AnchorPoint");
        if (v.get_type() == Variant::VECTOR2 && n->has_method("set_anchor_point")) {
            Vector2 a = v; n->call("set_anchor_point", a.x, a.y); } }
    if (n->has_meta("BackgroundColor3")) { Variant v = take("BackgroundColor3");
        if (v.get_type() == Variant::COLOR && n->has_method("set_bg_color")) {
            Color c = v; n->call("set_bg_color", c.r, c.g, c.b); } }
    if (n->has_meta("BackgroundTransparency")) { Variant v = take("BackgroundTransparency");
        if (n->has_method("set_bg_alpha")) n->call("set_bg_alpha", (float)v); }
    if (n->has_meta("BorderColor3")) { Variant v = take("BorderColor3");
        if (v.get_type() == Variant::COLOR && n->has_method("set_border_color")) {
            Color c = v; n->call("set_border_color", c.r, c.g, c.b); } }
    if (n->has_meta("BorderSizePixel")) { Variant v = take("BorderSizePixel");
        if (n->has_method("set_border_px")) n->call("set_border_px", (int)(int64_t)v); }
    if (n->has_meta("Text")) { Variant v = take("Text");
        if (n->has_method("set_text")) n->call("set_text", String(v)); }
    if (n->has_meta("TextColor3")) { Variant v = take("TextColor3");
        if (v.get_type() == Variant::COLOR && n->has_method("set_text_color")) {
            Color c = v; n->call("set_text_color", c.r, c.g, c.b); } }
    if (n->has_meta("TextSize")) { Variant v = take("TextSize");
        if (n->has_method("set_text_size")) n->call("set_text_size", (int)(float)v); }
    if (n->has_meta("TextScaled")) { Variant v = take("TextScaled");
        if (n->has_method("set_text_scaled")) n->call("set_text_scaled", (bool)v); }
    if (n->has_meta("Image")) { Variant v = take("Image");
        if (n->has_method("set_image")) n->call("set_image", String(v)); }
    if (n->has_meta("ImageColor3")) { Variant v = take("ImageColor3");
        if (v.get_type() == Variant::COLOR && n->has_method("set_image_color")) {
            Color c = v; n->call("set_image_color", c.r, c.g, c.b); } }
    if (n->has_meta("ImageTransparency")) { Variant v = take("ImageTransparency");
        if (n->has_method("set_image_transparency")) n->call("set_image_transparency", (float)v); }

    // Props con setter PascalCase propio (LayoutOrder, ZIndex, ScaleType…):
    // se aplican por nombre para no repetir 30 ifs.
    static const char* kDirect[] = {
        "LayoutOrder", "ZIndex", "SizeConstraint", "AutomaticSize", "BorderMode",
        "Active", "Interactable", "Selectable", "Draggable", "Style", "FontFace",
        "ScaleType", "SliceScale", "SliceCenter", "ResampleMode",
        "ImageRectOffset", "ImageRectSize", "HoverImage", "PressedImage",
        "AutoButtonColor", "ScrollBarThickness", "ScrollingDirection",
        "ScrollingEnabled", "AutomaticCanvasSize", "ElasticBehavior",
        "CanvasPosition", "ScrollBarImageColor3", "ScrollBarImageTransparency",
        "VerticalScrollBarInset", "HorizontalScrollBarInset",
        "VerticalScrollBarPosition", "TopImage", "MidImage", "BottomImage",
        "PlaceholderText", "PlaceholderColor3", "ClearTextOnFocus", "MultiLine",
        "TextEditable"
    };
    for (int i = 0; i < 39; i++) {
        if (!n->has_meta(kDirect[i])) continue;
        const String setter = String("set_") + kDirect[i];
        if (n->has_method(setter)) n->call(setter, n->get_meta(kDirect[i]));
    }

    // Props que necesitan traduccion (alineacion, contorno, rotacion, recorte).
    static const char* kExtra[] = { "ClipsDescendants", "Rotation", "TextXAlignment",
        "TextYAlignment", "TextWrapped", "TextTransparency",
        "TextStrokeTransparency", "TextStrokeColor3", "TextTruncate", "RichText",
        "LineHeight", "MaxVisibleGraphemes" };
    for (int i = 0; i < 12; i++)
        if (n->has_meta(kExtra[i])) gl_apply_rbx_gui_extra(n, kExtra[i], n->get_meta(kExtra[i]));
}

// ────────────────────────────────────────────────────────────────────
//  Macros: campos, metodos y binds comunes de GuiObject.
//  El juego COMPLETO de propiedades de Roblox esta aqui, para que las 7 clases
//  lo tengan identico y no se quede ninguna fuera.
// ────────────────────────────────────────────────────────────────────
#define GUI_COMMON_FIELDS                                 \
    GuiUDim2 _pos;                                        \
    GuiUDim2 _sz = {0, 100, 0, 50};                       \
    float    _anchor_x = 0, _anchor_y = 0;                \
    float    _bg_r = 1, _bg_g = 1, _bg_b = 1;             \
    float    _bg_alpha = 0; /* 0=opaque, 1=transparent */ \
    float    _border_r = 0, _border_g = 0, _border_b = 0; \
    int      _border_px = 0;                              \
    int      _border_mode = 0;   /* Outline=0, Middle=1, Inset=2 */ \
    bool     _clips_descendants = false;                  \
    int      _layout_order = 0;                           \
    int      _rbx_z = 0;                                  \
    int      _size_constraint = 0;                        \
    int      _auto_size = 0;                              \
    float    _rot_deg = 0;                                \
    bool     _active = true;                              \
    bool     _interactable = true;                        \
    bool     _selectable = false;                         \
    bool     _draggable = false;                          \
    int      _style = 0;                                  \
    String   _font_face;                                  \
    Vector2  _last_mod_size = Vector2(-1, -1);            \
    int      _font_weight = 400;

#define GUI_COMMON_APPLY_LAYOUT \
    gl_gui_apply_layout(this, _pos, _sz, _anchor_x, _anchor_y, _size_constraint, _rot_deg);

#define GUI_COMMON_METHODS(ClassName)                              \
    void set_udim2_pos(float xs, float xo, float ys, float yo) {   \
        _pos = {xs, xo, ys, yo};                                   \
        if (is_inside_tree()) GUI_COMMON_APPLY_LAYOUT              \
    }                                                              \
    void set_udim2_size(float xs, float xo, float ys, float yo) {  \
        _sz = {xs, xo, ys, yo};                                    \
        if (is_inside_tree()) GUI_COMMON_APPLY_LAYOUT              \
    }                                                              \
    void set_anchor_point(float ax, float ay) {                    \
        _anchor_x = ax; _anchor_y = ay;                            \
        if (is_inside_tree()) GUI_COMMON_APPLY_LAYOUT              \
    }                                                              \
    GuiUDim2 get_udim2_pos()  const { return _pos; }               \
    GuiUDim2 get_udim2_size() const { return _sz; }                \
    void set_bg_color(float r, float g, float b) {                 \
        _bg_r = r; _bg_g = g; _bg_b = b;                           \
        _apply_style();                                            \
    }                                                              \
    void set_bg_alpha(float a) {                                   \
        _bg_alpha = Math::clamp(a, 0.0f, 1.0f);                    \
        _apply_style();                                            \
    }                                                              \
    void set_border_color(float r, float g, float b) {             \
        _border_r = r; _border_g = g; _border_b = b;               \
        _apply_style();                                            \
    }                                                              \
    void set_border_px(int px) { _border_px = px; _apply_style(); } \
    float get_bg_r() const   { return _bg_r; }                     \
    float get_bg_g() const   { return _bg_g; }                     \
    float get_bg_b() const   { return _bg_b; }                     \
    float get_bg_alpha() const { return _bg_alpha; }               \
    /* --- Propiedades PascalCase de Roblox --- */                 \
    /* Position/Size = UDim2 como Vector4(xScale,xOffset,yScale,yOffset). \
       Son propiedades REALES para que duplicate() y el guardado de escena las \
       conserven: sin esto el clon de StarterGui a PlayerGui perdia posicion, \
       tamano y colores y el HUD entero salia en 100x50 arriba-izquierda. */ \
    void set_Position(Vector4 v) { set_udim2_pos(v.x, v.y, v.z, v.w); } \
    Vector4 get_Position() const { return Vector4(_pos.xs, _pos.xo, _pos.ys, _pos.yo); } \
    void set_Size(Vector4 v) { set_udim2_size(v.x, v.y, v.z, v.w); } \
    Vector4 get_Size() const { return Vector4(_sz.xs, _sz.xo, _sz.ys, _sz.yo); } \
    void set_AnchorPoint(Vector2 v) { set_anchor_point(v.x, v.y); } \
    Vector2 get_AnchorPoint() const { return Vector2(_anchor_x, _anchor_y); } \
    void set_BackgroundColor3(Color c) { set_bg_color(c.r, c.g, c.b); } \
    Color get_BackgroundColor3() const { return Color(_bg_r, _bg_g, _bg_b); } \
    void set_BackgroundTransparency(float a) { set_bg_alpha(a); } \
    float get_BackgroundTransparency() const { return _bg_alpha; } \
    void set_BorderColor3(Color c) { set_border_color(c.r, c.g, c.b); } \
    Color get_BorderColor3() const { return Color(_border_r, _border_g, _border_b); } \
    void set_BorderSizePixel(int px) { set_border_px(px); } \
    int  get_BorderSizePixel() const { return _border_px; } \
    void set_LayoutOrder(int v) { _layout_order = v; _gl_gui_bump_parent_layout(this); } \
    int  get_LayoutOrder() const { return _layout_order; }         \
    void set_ZIndex(int v) { _rbx_z = v; set_z_index(CLAMP(v, -4096, 4096)); } \
    int  get_ZIndex() const { return _rbx_z; }                     \
    void set_SizeConstraint(int v) {                               \
        _size_constraint = v;                                      \
        if (is_inside_tree()) GUI_COMMON_APPLY_LAYOUT              \
    }                                                              \
    int  get_SizeConstraint() const { return _size_constraint; }   \
    void set_AutomaticSize(int v) {                                \
        _auto_size = v;                                            \
        if (is_inside_tree()) call_deferred("_on_parent_resized");  \
    }                                                              \
    int  get_AutomaticSize() const { return _auto_size; }          \
    void set_rbx_rotation(float d) {                               \
        _rot_deg = d;                                              \
        set_pivot_offset(get_size() * 0.5f);                       \
        set_rotation((float)Math::deg_to_rad((double)d));          \
    }                                                              \
    float get_rbx_rotation() const { return _rot_deg; }            \
    void set_ClipsDescendants(bool b) { _clips_descendants = b; set_clip_contents(b); } \
    bool get_ClipsDescendants() const { return _clips_descendants; } \
    void set_Visible(bool b) { set_visible(b); }                   \
    bool get_Visible() const { return is_visible(); }              \
    void set_BorderMode(int v) { _border_mode = v; _apply_style(); } \
    int  get_BorderMode() const { return _border_mode; }           \
    void set_Active(bool b) {                                      \
        _active = b;                                               \
        set_mouse_filter(b ? Control::MOUSE_FILTER_STOP : Control::MOUSE_FILTER_PASS); \
    }                                                              \
    bool get_Active() const { return _active; }                    \
    void set_Interactable(bool b) {                                \
        _interactable = b;                                         \
        set_mouse_filter(b ? Control::MOUSE_FILTER_STOP : Control::MOUSE_FILTER_IGNORE); \
    }                                                              \
    bool get_Interactable() const { return _interactable; }        \
    void set_Selectable(bool b) {                                  \
        _selectable = b;                                           \
        set_focus_mode(b ? Control::FOCUS_ALL : Control::FOCUS_NONE); \
    }                                                              \
    bool get_Selectable() const { return _selectable; }            \
    void set_Draggable(bool b) { _draggable = b; }                 \
    bool get_Draggable() const { return _draggable; }              \
    void set_Style(int v) { _style = v; }                          \
    int  get_Style() const { return _style; }                      \
    void set_FontFace(String v) { _font_face = v; }                \
    String get_FontFace() const { return _font_face; }             \
    void _gl_on_self_resized() {                                   \
        set_pivot_offset(get_size() * 0.5f);                       \
        /* Los modificadores que dependen del TAMANO hay que rehacerlos: */ \
        /* el radio del UICorner de Roblox es por ESCALA (0.3 x lado corto, */ \
        /* 581 de los 662 del place lo usan asi) y el shader del UIGradient */ \
        /* necesita el rect. Antes solo se aplicaban una vez, cuando el     */ \
        /* elemento aun medía 0, y el redondeo salia siempre en 0.          */ \
        /* FRENO: cambiar el radio de la caja cambia el tamano MINIMO del   */ \
        /* control, y eso puede volver a disparar "resized" -> bucle. Solo  */ \
        /* se rehacen si el tamano cambio de verdad (>1 px).                */ \
        const Vector2 _sz_now = get_size();                        \
        if (Math::abs(_sz_now.x - _last_mod_size.x) > 1.0f ||       \
            Math::abs(_sz_now.y - _last_mod_size.y) > 1.0f) {       \
            _last_mod_size = _sz_now;                              \
            _gl_gui_reapply_modifiers(this);                       \
        }                                                          \
        if (_auto_size != GL_AUTO_NONE) return; /* lo hace el layout */ \
        if (has_method("_gl_refit_text")) call("_gl_refit_text");   \
    }

// Binds compartidos: layout + todas las props PascalCase.
#define GUI_BIND_COMMON(C)                                                                    \
    ClassDB::bind_method(D_METHOD("_on_parent_resized"), &C::_on_parent_resized);              \
    ClassDB::bind_method(D_METHOD("_gl_on_self_resized"), &C::_gl_on_self_resized);            \
    ClassDB::bind_method(D_METHOD("set_udim2_pos","xs","xo","ys","yo"),  &C::set_udim2_pos);   \
    ClassDB::bind_method(D_METHOD("set_udim2_size","xs","xo","ys","yo"), &C::set_udim2_size);  \
    ClassDB::bind_method(D_METHOD("set_anchor_point","ax","ay"),         &C::set_anchor_point);\
    ClassDB::bind_method(D_METHOD("set_bg_color","r","g","b"),           &C::set_bg_color);    \
    ClassDB::bind_method(D_METHOD("set_bg_alpha","a"),                   &C::set_bg_alpha);    \
    ClassDB::bind_method(D_METHOD("set_border_color","r","g","b"),       &C::set_border_color);\
    ClassDB::bind_method(D_METHOD("set_border_px","px"),                 &C::set_border_px);   \
    ClassDB::bind_method(D_METHOD("set_LayoutOrder","v"),   &C::set_LayoutOrder);              \
    ClassDB::bind_method(D_METHOD("get_LayoutOrder"),       &C::get_LayoutOrder);              \
    ClassDB::bind_method(D_METHOD("set_ZIndex","v"),        &C::set_ZIndex);                   \
    ClassDB::bind_method(D_METHOD("get_ZIndex"),            &C::get_ZIndex);                   \
    ClassDB::bind_method(D_METHOD("set_SizeConstraint","v"),&C::set_SizeConstraint);           \
    ClassDB::bind_method(D_METHOD("get_SizeConstraint"),    &C::get_SizeConstraint);           \
    ClassDB::bind_method(D_METHOD("set_AutomaticSize","v"), &C::set_AutomaticSize);            \
    ClassDB::bind_method(D_METHOD("get_AutomaticSize"),     &C::get_AutomaticSize);            \
    ClassDB::bind_method(D_METHOD("set_rbx_rotation","d"),  &C::set_rbx_rotation);             \
    ClassDB::bind_method(D_METHOD("get_rbx_rotation"),      &C::get_rbx_rotation);             \
    ClassDB::bind_method(D_METHOD("set_ClipsDescendants","b"), &C::set_ClipsDescendants);      \
    ClassDB::bind_method(D_METHOD("get_ClipsDescendants"),  &C::get_ClipsDescendants);         \
    ClassDB::bind_method(D_METHOD("set_Visible","b"),       &C::set_Visible);                  \
    ClassDB::bind_method(D_METHOD("get_Visible"),           &C::get_Visible);                  \
    ClassDB::bind_method(D_METHOD("set_BorderMode","v"),    &C::set_BorderMode);               \
    ClassDB::bind_method(D_METHOD("get_BorderMode"),        &C::get_BorderMode);               \
    ClassDB::bind_method(D_METHOD("set_Active","b"),        &C::set_Active);                   \
    ClassDB::bind_method(D_METHOD("get_Active"),            &C::get_Active);                   \
    ClassDB::bind_method(D_METHOD("set_Interactable","b"),  &C::set_Interactable);             \
    ClassDB::bind_method(D_METHOD("get_Interactable"),      &C::get_Interactable);             \
    ClassDB::bind_method(D_METHOD("set_Selectable","b"),    &C::set_Selectable);               \
    ClassDB::bind_method(D_METHOD("get_Selectable"),        &C::get_Selectable);               \
    ClassDB::bind_method(D_METHOD("set_Draggable","b"),     &C::set_Draggable);                \
    ClassDB::bind_method(D_METHOD("get_Draggable"),         &C::get_Draggable);                \
    ClassDB::bind_method(D_METHOD("set_Style","v"),         &C::set_Style);                    \
    ClassDB::bind_method(D_METHOD("get_Style"),             &C::get_Style);                    \
    ClassDB::bind_method(D_METHOD("set_FontFace","v"),      &C::set_FontFace);                 \
    ClassDB::bind_method(D_METHOD("get_FontFace"),          &C::get_FontFace);                 \
    ClassDB::bind_method(D_METHOD("set_Position","v"),               &C::set_Position); \
    ClassDB::bind_method(D_METHOD("get_Position"),                   &C::get_Position); \
    ClassDB::bind_method(D_METHOD("set_Size","v"),                   &C::set_Size); \
    ClassDB::bind_method(D_METHOD("get_Size"),                       &C::get_Size); \
    ClassDB::bind_method(D_METHOD("set_AnchorPoint","v"),            &C::set_AnchorPoint); \
    ClassDB::bind_method(D_METHOD("get_AnchorPoint"),                &C::get_AnchorPoint); \
    ClassDB::bind_method(D_METHOD("set_BackgroundColor3","v"),       &C::set_BackgroundColor3); \
    ClassDB::bind_method(D_METHOD("get_BackgroundColor3"),           &C::get_BackgroundColor3); \
    ClassDB::bind_method(D_METHOD("set_BackgroundTransparency","v"), &C::set_BackgroundTransparency); \
    ClassDB::bind_method(D_METHOD("get_BackgroundTransparency"),     &C::get_BackgroundTransparency); \
    ClassDB::bind_method(D_METHOD("set_BorderColor3","v"),           &C::set_BorderColor3); \
    ClassDB::bind_method(D_METHOD("get_BorderColor3"),               &C::get_BorderColor3); \
    ClassDB::bind_method(D_METHOD("set_BorderSizePixel","v"),        &C::set_BorderSizePixel); \
    ClassDB::bind_method(D_METHOD("get_BorderSizePixel"),            &C::get_BorderSizePixel); \
    ADD_GROUP("Roblox GUI",""); \
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR4,"Position"),        "set_Position","get_Position"); \
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR4,"Size"),            "set_Size","get_Size"); \
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2,"AnchorPoint"),     "set_AnchorPoint","get_AnchorPoint"); \
    ADD_PROPERTY(PropertyInfo(Variant::COLOR, "BackgroundColor3"), "set_BackgroundColor3","get_BackgroundColor3"); \
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "BackgroundTransparency"), "set_BackgroundTransparency","get_BackgroundTransparency"); \
    ADD_PROPERTY(PropertyInfo(Variant::COLOR, "BorderColor3"),     "set_BorderColor3","get_BorderColor3"); \
    ADD_PROPERTY(PropertyInfo(Variant::INT,   "BorderSizePixel"),  "set_BorderSizePixel","get_BorderSizePixel");                                                               \
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "Visible"),          "set_Visible","get_Visible"); \
    ADD_PROPERTY(PropertyInfo(Variant::INT,   "LayoutOrder"),      "set_LayoutOrder","get_LayoutOrder"); \
    ADD_PROPERTY(PropertyInfo(Variant::INT,   "ZIndex"),           "set_ZIndex","get_ZIndex"); \
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Rotation"),         "set_rbx_rotation","get_rbx_rotation"); \
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "ClipsDescendants"), "set_ClipsDescendants","get_ClipsDescendants"); \
    ADD_PROPERTY(PropertyInfo(Variant::INT,   "SizeConstraint",  PROPERTY_HINT_ENUM, "RelativeXY,RelativeXX,RelativeYY"), "set_SizeConstraint","get_SizeConstraint"); \
    ADD_PROPERTY(PropertyInfo(Variant::INT,   "AutomaticSize",   PROPERTY_HINT_ENUM, "None,X,Y,XY"), "set_AutomaticSize","get_AutomaticSize"); \
    ADD_PROPERTY(PropertyInfo(Variant::INT,   "BorderMode",      PROPERTY_HINT_ENUM, "Outline,Middle,Inset"), "set_BorderMode","get_BorderMode"); \
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "Active"),         "set_Active","get_Active");   \
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "Interactable"),   "set_Interactable","get_Interactable"); \
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "Selectable"),     "set_Selectable","get_Selectable"); \
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "Draggable"),      "set_Draggable","get_Draggable"); \
    ADD_PROPERTY(PropertyInfo(Variant::INT,   "Style"),          "set_Style","get_Style");     \
    ADD_PROPERTY(PropertyInfo(Variant::STRING,"FontFace"),       "set_FontFace","get_FontFace");

// AutomaticSize: crece el elemento para que quepa su contenido.
#define GUI_APPLY_AUTO_SIZE(content)                                                   \
    if (_auto_size != GL_AUTO_NONE) {                                                  \
        const Vector2 _cur = get_size();                                               \
        Vector2 _want = _cur;                                                          \
        if (_auto_size == GL_AUTO_X || _auto_size == GL_AUTO_XY) _want.x = (content).x;  \
        if (_auto_size == GL_AUTO_Y || _auto_size == GL_AUTO_XY) _want.y = (content).y;  \
        if (_want != _cur) {                                                           \
            const float _dx = _want.x - _cur.x, _dy = _want.y - _cur.y;                \
            set_offset(SIDE_LEFT,   get_offset(SIDE_LEFT)   - _anchor_x * _dx);        \
            set_offset(SIDE_RIGHT,  get_offset(SIDE_RIGHT)  + (1.0f - _anchor_x) * _dx); \
            set_offset(SIDE_TOP,    get_offset(SIDE_TOP)    - _anchor_y * _dy);        \
            set_offset(SIDE_BOTTOM, get_offset(SIDE_BOTTOM) + (1.0f - _anchor_y) * _dy); \
        }                                                                              \
    }

// ────────────────────────────────────────────────────────────────────
//  ScreenGui — Interface layer (equivalent to Godot CanvasLayer)
////  ScreenGui — Capa de interfaz (equivalente al CanvasLayer de Godot)
// ────────────────────────────────────────────────────────────────────
class ScreenGui : public CanvasLayer {
    GDCLASS(ScreenGui, CanvasLayer);

    bool enabled          = true;
    bool reset_on_spawn   = true;
    bool ignore_gui_inset = false;
    int  display_order    = 0;
    int  zindex_behavior  = 1;   // Global=0, Sibling=1 (el de Roblox por defecto)
    bool clip_safe_area   = false;
    int  screen_insets    = 1;   // DeviceSafeInsets

    // ZIndexBehavior.Global: el ZIndex es ABSOLUTO en toda la capa, asi que un
    // hijo con ZIndex bajo puede quedar por debajo de su padre. En Godot eso es
    // z_as_relative = false. Con Sibling (por defecto) el ZIndex solo ordena
    // hermanos y los hijos siempre van encima -> z_as_relative = true.
    void _apply_zindex_behavior(Node* n) {
        if (!n) return;
        if (CanvasItem* ci = Object::cast_to<CanvasItem>(n))
            ci->set_z_as_relative(zindex_behavior != 0);
        for (int i = 0; i < n->get_child_count(); i++) _apply_zindex_behavior(n->get_child(i));
    }

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
        ClassDB::bind_method(D_METHOD("set_ZIndexBehavior","v"),      &ScreenGui::set_ZIndexBehavior);
        ClassDB::bind_method(D_METHOD("get_ZIndexBehavior"),           &ScreenGui::get_ZIndexBehavior);
        ClassDB::bind_method(D_METHOD("set_ClipToDeviceSafeArea","v"),&ScreenGui::set_ClipToDeviceSafeArea);
        ClassDB::bind_method(D_METHOD("get_ClipToDeviceSafeArea"),     &ScreenGui::get_ClipToDeviceSafeArea);
        ClassDB::bind_method(D_METHOD("set_ScreenInsets","v"),        &ScreenGui::set_ScreenInsets);
        ClassDB::bind_method(D_METHOD("get_ScreenInsets"),             &ScreenGui::get_ScreenInsets);
        ClassDB::bind_method(D_METHOD("_gl_refresh_zindex"),           &ScreenGui::_gl_refresh_zindex);

        ADD_GROUP("ScreenGui","");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"Enabled"),         "set_sg_enabled","get_sg_enabled");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"ResetOnSpawn"),    "set_reset_on_spawn","get_reset_on_spawn");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"IgnoreGuiInset"),  "set_ignore_gui_inset","get_ignore_gui_inset");
        ADD_PROPERTY(PropertyInfo(Variant::INT, "DisplayOrder"),    "set_display_order","get_display_order");
        ADD_PROPERTY(PropertyInfo(Variant::INT, "ZIndexBehavior",   PROPERTY_HINT_ENUM,"Global,Sibling"), "set_ZIndexBehavior","get_ZIndexBehavior");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"ClipToDeviceSafeArea"), "set_ClipToDeviceSafeArea","get_ClipToDeviceSafeArea");
        ADD_PROPERTY(PropertyInfo(Variant::INT, "ScreenInsets"),    "set_ScreenInsets","get_ScreenInsets");
    }

    // REGLA DE ROBLOX: un ScreenGui SOLO se dibuja si esta dentro de PlayerGui
    // (o CoreGui). En cualquier otro sitio es una PLANTILLA guardada y no se ve:
    //   · StarterGui        -> se clona a PlayerGui al entrar; el original no se ve.
    //   · ReplicatedStorage / ReplicatedFirst / ServerStorage / Workspace ->
    //     interfaces que el juego guarda y clona cuando toca (tiendas, popups,
    //     la pantalla de carga...). Antes se dibujaban TODAS a la vez y la
    //     pantalla salia con paneles encima de paneles sin que nada los hubiera
    //     abierto: es justo lo que se veia en el place de prueba.
    bool _gl_should_render() const {
        if (Engine::get_singleton()->is_editor_hint()) return true;
        for (const Node* p = get_parent(); p; p = p->get_parent()) {
            const StringName nm = p->get_name();
            if (nm == StringName("PlayerGui") || nm == StringName("CoreGui")) return true;
            if (nm == StringName("StarterGui")) return false;
        }
        return false;
    }

    void _notification(int p_what) {
        if (p_what != NOTIFICATION_ENTER_TREE) return;
        // La COPIA hereda visible=false al duplicarse del original oculto, y sin
        // esto el HUD del jugador NUNCA aparecia. Al entrar en PlayerGui ya deja
        // de ser plantilla -> visible segun Enabled.
        set_visible(enabled && _gl_should_render());
        call_deferred("_gl_refresh_zindex");
    }

public:
    void set_sg_enabled(bool b) {
        enabled = b;
        set_visible(b && _gl_should_render());
    }
    bool get_sg_enabled() const          { return enabled; }
    void set_reset_on_spawn(bool b)      { reset_on_spawn = b; }
    bool get_reset_on_spawn() const      { return reset_on_spawn; }
    void set_ignore_gui_inset(bool b)    { ignore_gui_inset = b; }
    bool get_ignore_gui_inset() const    { return ignore_gui_inset; }
    // DisplayOrder de Roblox no tiene tope (el place usa 99999 para "siempre
    // encima"); el layer de un CanvasLayer si, asi que se recorta conservando
    // el orden relativo y se guarda el valor original para que los scripts lo lean.
    void set_display_order(int o) {
        display_order = o;
        set_layer(CLAMP(o, -128, 128));
    }
    int  get_display_order() const       { return display_order; }
    void set_ZIndexBehavior(int v) {
        zindex_behavior = v;
        if (is_inside_tree()) call_deferred("_gl_refresh_zindex");
    }
    int  get_ZIndexBehavior() const      { return zindex_behavior; }
    void set_ClipToDeviceSafeArea(bool b){ clip_safe_area = b; }
    bool get_ClipToDeviceSafeArea() const{ return clip_safe_area; }
    void set_ScreenInsets(int v)         { screen_insets = v; }
    int  get_ScreenInsets() const        { return screen_insets; }
    void _gl_refresh_zindex()            { _apply_zindex_behavior(this); }
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
        _gl_gui_reapply_modifiers(this);
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            gl_apply_rbx_gui_meta(this);   // props importadas guardadas como meta
            gl_gui_apply_render_gate(this);  // regla de Roblox: solo dentro de un *Gui
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            _gl_gui_wire_relayout(this, "_on_parent_resized");
        }
    }

protected:
    static void _bind_methods() {
        GUI_BIND_COMMON(RobloxFrame)
        ADD_SIGNAL(MethodInfo("MouseEnter"));
        ADD_SIGNAL(MethodInfo("MouseLeave"));
    }

public:
    GUI_COMMON_METHODS(RobloxFrame)

    void _on_parent_resized() {
        GUI_COMMON_APPLY_LAYOUT
        GUI_APPLY_AUTO_SIZE(gl_gui_children_extent(this))
    }
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
        if (!cb.active || !gl_state_alive(cb.main_L)) { cbs.erase(cbs.begin() + i); continue; }
        lua_State* th = lua_newthread(cb.main_L);
        lua_rawgeti(cb.main_L, LUA_REGISTRYINDEX, cb.ref);
        if (lua_isfunction(cb.main_L, -1)) {
            lua_xmove(cb.main_L, th, 1);
            gl_check_resume(th, lua_resume(th, nullptr, 0));
        } else { lua_pop(cb.main_L, 1); }
        lua_pop(cb.main_L, 1);
    }
}

// ────────────────────────────────────────────────────────────────────
//  Macros de TEXTO: Text/TextColor3/TextSize/TextScaled/RichText/…
// ────────────────────────────────────────────────────────────────────
#define GUI_TEXT_FIELDS                       \
    float _txt_r = 0, _txt_g = 0, _txt_b = 0; \
    float _txt_alpha = 1;                     \
    bool  _text_scaled = false;               \
    bool  _rich_text = false;                 \
    int   _text_size = 14;                    \
    String _raw_text;                         \
    Color _stroke_col = Color(0, 0, 0);       \
    float _stroke_t = 1;                      \
    int   _tx_align = 2;   /* Center, como Roblox */ \
    int   _ty_align = 1;   /* Center */       \
    bool  _wrapped = false;                   \
    int   _truncate = 0;                      \
    float _line_height = 1;                   \
    int   _max_graphemes = -1;

// TextScaled de Roblox: la letra crece/encoge para llenar el rect. Se estima el
// tamaño de una medida y se refina con otra (dos medidas por elemento, no un
// barrido de 1..100: hay 1988 TextLabel en un place real).
#define GUI_TEXT_METHODS(ClassName)                                            \
    /* Todas las props de texto de Roblox, como propiedades reales: el clon \
       de StarterGui a PlayerGui las copia y el HUD sale igual. */ \
    void set_TextColor3(Color c) { set_text_color(c.r, c.g, c.b); } \
    Color get_TextColor3() const { return Color(_txt_r, _txt_g, _txt_b); } \
    void set_TextTransparency(float v) { \
        _txt_alpha = 1.0f - Math::clamp(v, 0.0f, 1.0f); \
        add_theme_color_override("font_color", Color(_txt_r, _txt_g, _txt_b, _txt_alpha)); \
    } \
    float get_TextTransparency() const { return 1.0f - _txt_alpha; } \
    void set_TextStrokeColor3(Color c) { \
        _stroke_col = c; \
        add_theme_color_override("font_outline_color", c); \
    } \
    Color get_TextStrokeColor3() const { return _stroke_col; } \
    void set_TextStrokeTransparency(float v) { \
        _stroke_t = v; \
        gl_apply_rbx_gui_extra(this, "TextStrokeTransparency", v); \
    } \
    float get_TextStrokeTransparency() const { return _stroke_t; } \
    void set_TextXAlignment(int v) { \
        _tx_align = v; \
        gl_apply_rbx_gui_extra(this, "TextXAlignment", (int64_t)v); \
    } \
    int  get_TextXAlignment() const { return _tx_align; } \
    void set_TextYAlignment(int v) { \
        _ty_align = v; \
        gl_apply_rbx_gui_extra(this, "TextYAlignment", (int64_t)v); \
    } \
    int  get_TextYAlignment() const { return _ty_align; } \
    void set_TextWrapped(bool b) { \
        _wrapped = b; \
        gl_apply_rbx_gui_extra(this, "TextWrapped", b); \
    } \
    bool get_TextWrapped() const { return _wrapped; } \
    void set_TextTruncate(int v) { \
        _truncate = v; \
        gl_apply_rbx_gui_extra(this, "TextTruncate", (int64_t)v); \
    } \
    int  get_TextTruncate() const { return _truncate; } \
    void set_LineHeight(float v) { \
        _line_height = v; \
        gl_apply_rbx_gui_extra(this, "LineHeight", v); \
    } \
    float get_LineHeight() const { return _line_height; } \
    void set_MaxVisibleGraphemes(int v) { \
        _max_graphemes = v; \
        gl_apply_rbx_gui_extra(this, "MaxVisibleGraphemes", (int64_t)v); \
    } \
    int  get_MaxVisibleGraphemes() const { return _max_graphemes; } \
    void set_text_color(float r, float g, float b) {                           \
        _txt_r = r; _txt_g = g; _txt_b = b;                                    \
        add_theme_color_override("font_color", Color(r, g, b, _txt_alpha));     \
    }                                                                          \
    void set_text_size(int s) {                                                \
        _text_size = s;                                                        \
        if (!_text_scaled) add_theme_font_size_override("font_size", CLAMP(s, 6, 200)); \
        else _gl_refit_text();                                                 \
    }                                                                          \
    int  get_text_size() const { return _text_size; }                          \
    void set_text_scaled(bool b) {                                             \
        _text_scaled = b;                                                      \
        if (b) _gl_refit_text();                                               \
        else   add_theme_font_size_override("font_size", _text_size);           \
    }                                                                          \
    bool get_text_scaled() const { return _text_scaled; }                      \
    void set_rich_text(bool b) {                                               \
        _rich_text = b;                                                        \
        _gl_push_text();                                                       \
    }                                                                          \
    bool get_rich_text() const { return _rich_text; }                          \
    void set_rbx_text(String t) { _raw_text = t; _gl_push_text(); }             \
    String get_rbx_text() const { return _raw_text; }                          \
    void _gl_refit_text() {                                                    \
        if (!_text_scaled) return;                                             \
        const Vector2 box = get_size();                                        \
        if (box.x < 2.0f || box.y < 2.0f) return;                              \
        Ref<Font> f = get_theme_font("font");                                   \
        if (f.is_null()) return;                                               \
        int mn = 6, mx = 100;   /* por debajo de 6 px el glifo sale vacio */                                                  \
        for (int i = 0; i < get_child_count(); i++) {                          \
            Node* ch = get_child(i);                                           \
            if (ch && ch->is_class("UITextSizeConstraint")) {                  \
                mn = MAX(6, (int)(int64_t)ch->get(StringName("MinTextSize"))); \
                mx = CLAMP((int)(int64_t)ch->get(StringName("MaxTextSize")), 6, 200); \
                break;                                                        \
            }                                                                 \
        }                                                                     \
        String t = _gl_display_text();                                        \
        if (t.is_empty()) return;                                             \
        int probe = CLAMP((int)box.y, MAX(1, mn), MAX(1, mx));                \
        Vector2 m = f->get_string_size(t, HORIZONTAL_ALIGNMENT_LEFT, -1, probe); \
        if (m.x > 0.5f && m.y > 0.5f) {                                        \
            const float k = MIN(box.x / m.x, box.y / m.y);                     \
            probe = CLAMP((int)((float)probe * k), MAX(1, mn), MAX(1, mx));    \
            m = f->get_string_size(t, HORIZONTAL_ALIGNMENT_LEFT, -1, probe);   \
            while (probe > mn && (m.x > box.x || m.y > box.y)) {               \
                probe--;                                                      \
                m = f->get_string_size(t, HORIZONTAL_ALIGNMENT_LEFT, -1, probe); \
            }                                                                 \
        }                                                                     \
        add_theme_font_size_override("font_size", probe);                      \
    }

#define GUI_BIND_TEXT(C)                                                            \
    ClassDB::bind_method(D_METHOD("set_text_color","r","g","b"), &C::set_text_color); \
    ClassDB::bind_method(D_METHOD("set_text_size","s"),          &C::set_text_size);  \
    ClassDB::bind_method(D_METHOD("get_text_size"),              &C::get_text_size);  \
    ClassDB::bind_method(D_METHOD("set_text_scaled","b"),        &C::set_text_scaled);\
    ClassDB::bind_method(D_METHOD("get_text_scaled"),            &C::get_text_scaled);\
    ClassDB::bind_method(D_METHOD("set_rich_text","b"),          &C::set_rich_text);  \
    ClassDB::bind_method(D_METHOD("get_rich_text"),              &C::get_rich_text);  \
    ClassDB::bind_method(D_METHOD("set_rbx_text","t"),           &C::set_rbx_text);   \
    ClassDB::bind_method(D_METHOD("get_rbx_text"),               &C::get_rbx_text);   \
    ClassDB::bind_method(D_METHOD("_gl_refit_text"),             &C::_gl_refit_text); \
    ClassDB::bind_method(D_METHOD("set_TextColor3","v"), &C::set_TextColor3); \
    ClassDB::bind_method(D_METHOD("get_TextColor3"),     &C::get_TextColor3); \
    ClassDB::bind_method(D_METHOD("set_TextTransparency","v"), &C::set_TextTransparency); \
    ClassDB::bind_method(D_METHOD("get_TextTransparency"),     &C::get_TextTransparency); \
    ClassDB::bind_method(D_METHOD("set_TextStrokeColor3","v"), &C::set_TextStrokeColor3); \
    ClassDB::bind_method(D_METHOD("get_TextStrokeColor3"),     &C::get_TextStrokeColor3); \
    ClassDB::bind_method(D_METHOD("set_TextStrokeTransparency","v"), &C::set_TextStrokeTransparency); \
    ClassDB::bind_method(D_METHOD("get_TextStrokeTransparency"),     &C::get_TextStrokeTransparency); \
    ClassDB::bind_method(D_METHOD("set_TextXAlignment","v"), &C::set_TextXAlignment); \
    ClassDB::bind_method(D_METHOD("get_TextXAlignment"),     &C::get_TextXAlignment); \
    ClassDB::bind_method(D_METHOD("set_TextYAlignment","v"), &C::set_TextYAlignment); \
    ClassDB::bind_method(D_METHOD("get_TextYAlignment"),     &C::get_TextYAlignment); \
    ClassDB::bind_method(D_METHOD("set_TextWrapped","v"), &C::set_TextWrapped); \
    ClassDB::bind_method(D_METHOD("get_TextWrapped"),     &C::get_TextWrapped); \
    ClassDB::bind_method(D_METHOD("set_TextTruncate","v"), &C::set_TextTruncate); \
    ClassDB::bind_method(D_METHOD("get_TextTruncate"),     &C::get_TextTruncate); \
    ClassDB::bind_method(D_METHOD("set_LineHeight","v"), &C::set_LineHeight); \
    ClassDB::bind_method(D_METHOD("get_LineHeight"),     &C::get_LineHeight); \
    ClassDB::bind_method(D_METHOD("set_MaxVisibleGraphemes","v"), &C::set_MaxVisibleGraphemes); \
    ClassDB::bind_method(D_METHOD("get_MaxVisibleGraphemes"),     &C::get_MaxVisibleGraphemes); \
    ADD_PROPERTY(PropertyInfo(Variant::INT,  "TextSize"),   "set_text_size","get_text_size"); \
    ADD_PROPERTY(PropertyInfo(Variant::COLOR,"TextColor3"),       "set_TextColor3","get_TextColor3"); \
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"TextTransparency"), "set_TextTransparency","get_TextTransparency"); \
    ADD_PROPERTY(PropertyInfo(Variant::COLOR,"TextStrokeColor3"), "set_TextStrokeColor3","get_TextStrokeColor3"); \
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"TextStrokeTransparency"), "set_TextStrokeTransparency","get_TextStrokeTransparency"); \
    ADD_PROPERTY(PropertyInfo(Variant::INT,  "TextXAlignment", PROPERTY_HINT_ENUM,"Left,Right,Center"), "set_TextXAlignment","get_TextXAlignment"); \
    ADD_PROPERTY(PropertyInfo(Variant::INT,  "TextYAlignment", PROPERTY_HINT_ENUM,"Top,Center,Bottom"), "set_TextYAlignment","get_TextYAlignment"); \
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "TextWrapped"),      "set_TextWrapped","get_TextWrapped"); \
    ADD_PROPERTY(PropertyInfo(Variant::INT,  "TextTruncate", PROPERTY_HINT_ENUM,"None,AtEnd,SplitWord"), "set_TextTruncate","get_TextTruncate"); \
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"LineHeight"),       "set_LineHeight","get_LineHeight"); \
    ADD_PROPERTY(PropertyInfo(Variant::INT,  "MaxVisibleGraphemes"), "set_MaxVisibleGraphemes","get_MaxVisibleGraphemes");   \
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "TextScaled"), "set_text_scaled","get_text_scaled"); \
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "RichText"),   "set_rich_text","get_rich_text");

// ────────────────────────────────────────────────────────────────────
//  RobloxTextLabel — Static text
////  RobloxTextLabel — Texto estático
// ────────────────────────────────────────────────────────────────────
class RobloxTextLabel : public Label {
    GDCLASS(RobloxTextLabel, Label);
    GUI_COMMON_FIELDS
    GUI_TEXT_FIELDS

    void _apply_style() {
        Ref<StyleBoxFlat> style;
        style.instantiate();
        style->set_bg_color(Color(_bg_r, _bg_g, _bg_b, 1.0f - _bg_alpha));
        style->set_border_color(Color(_border_r, _border_g, _border_b, 1.0f));
        style->set_border_width_all(_border_px);
        add_theme_stylebox_override("normal", style);
        _gl_gui_reapply_modifiers(this);
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            gl_apply_rbx_gui_meta(this);   // props importadas guardadas como meta
            gl_gui_apply_render_gate(this);  // regla de Roblox: solo dentro de un *Gui
            // clip_text: sin esto el mínimo del Label es el ancho del texto, que
            // clampaba el tamaño UDim2 y descuadraba el anchor.
            set_clip_text(true);
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            add_theme_color_override("font_color", Color(_txt_r, _txt_g, _txt_b, _txt_alpha));
            _gl_gui_wire_relayout(this, "_on_parent_resized");
        }
    }

protected:
    static void _bind_methods() {
        GUI_BIND_COMMON(RobloxTextLabel)
        GUI_BIND_TEXT(RobloxTextLabel)
    }

public:
    GUI_COMMON_METHODS(RobloxTextLabel)
    GUI_TEXT_METHODS(RobloxTextLabel)

    String _gl_display_text() const { return get_text(); }
    void _gl_push_text() {
        set_text(_rich_text ? _gl_strip_rich_tags(_raw_text) : _raw_text);
        _gl_refit_text();
    }

    void _on_parent_resized() {
        GUI_COMMON_APPLY_LAYOUT
        if (_auto_size != GL_AUTO_NONE) {
            Vector2 want = get_minimum_size();
            Ref<Font> f = get_theme_font("font");
            if (f.is_valid())
                want = f->get_string_size(get_text(), HORIZONTAL_ALIGNMENT_LEFT, -1,
                                          get_theme_font_size("font_size"));
            GUI_APPLY_AUTO_SIZE(want)
        }
        _gl_refit_text();
    }
};

// ────────────────────────────────────────────────────────────────────
//  RobloxTextButton — Text button with Roblox-style signals
////  RobloxTextButton — Botón con texto, señales tipo Roblox
// ────────────────────────────────────────────────────────────────────
class RobloxTextButton : public Button {
    GDCLASS(RobloxTextButton, Button);
    GUI_COMMON_FIELDS
    GUI_TEXT_FIELDS
    bool _auto_button_color = true;

    std::vector<GuiLuaCallback> _click_cbs;
    std::vector<GuiLuaCallback> _enter_cbs;
    std::vector<GuiLuaCallback> _leave_cbs;

    void _apply_style() {
        const float d = _auto_button_color ? 0.1f : 0.0f;
        Ref<StyleBoxFlat> normal_style, hover_style, pressed_style;
        normal_style.instantiate();
        normal_style->set_bg_color(Color(_bg_r, _bg_g, _bg_b, 1.0f - _bg_alpha));
        normal_style->set_border_color(Color(_border_r, _border_g, _border_b));
        normal_style->set_border_width_all(_border_px);
        hover_style.instantiate();
        hover_style->set_bg_color(Color(
            Math::clamp(_bg_r + d, 0.f, 1.f),
            Math::clamp(_bg_g + d, 0.f, 1.f),
            Math::clamp(_bg_b + d, 0.f, 1.f), 1.0f - _bg_alpha));
        hover_style->set_border_color(Color(_border_r, _border_g, _border_b));
        hover_style->set_border_width_all(_border_px);
        pressed_style.instantiate();
        pressed_style->set_bg_color(Color(
            Math::clamp(_bg_r - d, 0.f, 1.f),
            Math::clamp(_bg_g - d, 0.f, 1.f),
            Math::clamp(_bg_b - d, 0.f, 1.f), 1.0f - _bg_alpha));
        pressed_style->set_border_width_all(_border_px);
        add_theme_stylebox_override("normal",  normal_style);
        add_theme_stylebox_override("hover",   hover_style);
        add_theme_stylebox_override("pressed", pressed_style);
        _gl_gui_reapply_modifiers(this);
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            gl_apply_rbx_gui_meta(this);   // props importadas guardadas como meta
            gl_gui_apply_render_gate(this);  // regla de Roblox: solo dentro de un *Gui
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            set_clip_text(true);   // el box vale su Size, no el ancho del texto
            add_theme_color_override("font_color", Color(_txt_r, _txt_g, _txt_b, _txt_alpha));
            if (!is_connected("pressed", Callable(this, "_on_pressed"))) {
                connect("pressed",      Callable(this, "_on_pressed"));
                connect("mouse_entered",Callable(this, "_on_mouse_enter"));
                connect("mouse_exited", Callable(this, "_on_mouse_leave"));
            }
            _gl_gui_wire_relayout(this, "_on_parent_resized");
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("_gl_disconnect","ref"), &RobloxTextButton::_gl_disconnect);
        ClassDB::bind_method(D_METHOD("_on_pressed"),        &RobloxTextButton::_on_pressed);
        ClassDB::bind_method(D_METHOD("_on_mouse_enter"),    &RobloxTextButton::_on_mouse_enter);
        ClassDB::bind_method(D_METHOD("_on_mouse_leave"),    &RobloxTextButton::_on_mouse_leave);
        ClassDB::bind_method(D_METHOD("set_AutoButtonColor","b"), &RobloxTextButton::set_AutoButtonColor);
        ClassDB::bind_method(D_METHOD("get_AutoButtonColor"),     &RobloxTextButton::get_AutoButtonColor);
        GUI_BIND_COMMON(RobloxTextButton)
        GUI_BIND_TEXT(RobloxTextButton)
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"AutoButtonColor"), "set_AutoButtonColor","get_AutoButtonColor");
        ADD_SIGNAL(MethodInfo("MouseButton1Click"));
        ADD_SIGNAL(MethodInfo("Activated"));   // Roblox GuiButton.Activated
        ADD_SIGNAL(MethodInfo("MouseButton1Down"));
        ADD_SIGNAL(MethodInfo("MouseButton1Up"));
        ADD_SIGNAL(MethodInfo("MouseEnter"));
        ADD_SIGNAL(MethodInfo("MouseLeave"));
    }

public:
    GUI_COMMON_METHODS(RobloxTextButton)
    GUI_TEXT_METHODS(RobloxTextButton)

    String _gl_display_text() const { return get_text(); }
    void _gl_push_text() {
        set_text(_rich_text ? _gl_strip_rich_tags(_raw_text) : _raw_text);
        _gl_refit_text();
    }
    void set_AutoButtonColor(bool b) { _auto_button_color = b; _apply_style(); }
    bool get_AutoButtonColor() const { return _auto_button_color; }

    void _on_parent_resized() {
        GUI_COMMON_APPLY_LAYOUT
        GUI_APPLY_AUTO_SIZE(get_minimum_size())
        _gl_refit_text();
    }
    void _on_pressed()        { emit_signal("MouseButton1Down"); emit_signal("MouseButton1Click"); emit_signal("Activated"); emit_signal("MouseButton1Up"); _fire_gui_cbs(_click_cbs); }
    void _on_mouse_enter()    { emit_signal("MouseEnter");        _fire_gui_cbs(_enter_cbs); }
    void _on_mouse_leave()    { emit_signal("MouseLeave");        _fire_gui_cbs(_leave_cbs); }

    void add_click_cb(lua_State* L, int ref)  { _click_cbs.push_back({L, ref, true}); }
    void add_enter_cb(lua_State* L, int ref)  { _enter_cbs.push_back({L, ref, true}); }
    void add_leave_cb(lua_State* L, int ref)  { _leave_cbs.push_back({L, ref, true}); }
    void _gl_disconnect(int ref) {
        for (auto& cb : _click_cbs) if (cb.ref==ref) cb.active=false;
        for (auto& cb : _enter_cbs) if (cb.ref==ref) cb.active=false;
        for (auto& cb : _leave_cbs) if (cb.ref==ref) cb.active=false;
    }
};

// ────────────────────────────────────────────────────────────────────
//  Macros de IMAGEN: ScaleType/SliceCenter/TileSize/ImageRect…
// ────────────────────────────────────────────────────────────────────
#define GUI_IMAGE_FIELDS                                       \
    String _image_path;                                        \
    float  _img_r = 1, _img_g = 1, _img_b = 1, _img_alpha = 1;  \
    int    _scale_type = 0;   /* Stretch=0,Slice=1,Tile=2,Fit=3,Crop=4 */ \
    float  _slice_scale = 1;                                   \
    Rect2  _slice_center;                                      \
    GuiUDim2 _tile_size = {1, 0, 1, 0};                        \
    Vector2 _img_rect_off;                                     \
    Vector2 _img_rect_size;                                    \
    int    _resample = 0;     /* Default=0, Pixelated=1 */

#define GUI_IMAGE_METHODS(ClassName)                                    \
    void set_image_color(float r, float g, float b) {                   \
        _img_r = r; _img_g = g; _img_b = b;                             \
        set_modulate(Color(_img_r, _img_g, _img_b, _img_alpha));         \
    }                                                                   \
    void set_image_transparency(float a) {                              \
        _img_alpha = 1.0f - Math::clamp(a, 0.0f, 1.0f);                 \
        set_modulate(Color(_img_r, _img_g, _img_b, _img_alpha));         \
    }                                                                   \
    float get_image_transparency() const { return 1.0f - _img_alpha; }   \
    void set_Image(String p) { set_image(p); } \
    String get_Image() const { return _image_path; } \
    void set_ImageColor3(Color c) { set_image_color(c.r, c.g, c.b); } \
    Color get_ImageColor3() const { return Color(_img_r, _img_g, _img_b); } \
    void set_TileSize(Vector4 v) { _tile_size = {v.x, v.y, v.z, v.w}; } \
    Vector4 get_TileSize() const { \
        return Vector4(_tile_size.xs, _tile_size.xo, _tile_size.ys, _tile_size.yo); \
    } \
    void set_ScaleType(int v) { _scale_type = v; _gl_apply_image_mode(); } \
    int  get_ScaleType() const { return _scale_type; }                  \
    void set_SliceScale(float v) { _slice_scale = v; }                  \
    float get_SliceScale() const { return _slice_scale; }               \
    void set_SliceCenter(Rect2 v) { _slice_center = v; }                \
    Rect2 get_SliceCenter() const { return _slice_center; }             \
    void set_ImageRectOffset(Vector2 v) { _img_rect_off = v; _gl_apply_image_rect(); } \
    Vector2 get_ImageRectOffset() const { return _img_rect_off; }       \
    void set_ImageRectSize(Vector2 v) { _img_rect_size = v; _gl_apply_image_rect(); } \
    Vector2 get_ImageRectSize() const { return _img_rect_size; }        \
    void set_ResampleMode(int v) {                                      \
        _resample = v;                                                  \
        set_texture_filter(v == 1 ? CanvasItem::TEXTURE_FILTER_NEAREST   \
                                  : CanvasItem::TEXTURE_FILTER_PARENT_NODE); \
    }                                                                   \
    int  get_ResampleMode() const { return _resample; }                 \
    void set_gl_tile_size(float xs, float xo, float ys, float yo) { _tile_size = {xs, xo, ys, yo}; }

#define GUI_BIND_IMAGE(C)                                                                  \
    ClassDB::bind_method(D_METHOD("set_image","path"),            &C::set_image);            \
    ClassDB::bind_method(D_METHOD("set_image_color","r","g","b"), &C::set_image_color);      \
    ClassDB::bind_method(D_METHOD("set_image_transparency","a"),  &C::set_image_transparency);\
    ClassDB::bind_method(D_METHOD("get_image_transparency"),      &C::get_image_transparency);\
    ClassDB::bind_method(D_METHOD("set_ScaleType","v"),           &C::set_ScaleType);        \
    ClassDB::bind_method(D_METHOD("get_ScaleType"),               &C::get_ScaleType);        \
    ClassDB::bind_method(D_METHOD("set_SliceScale","v"),          &C::set_SliceScale);       \
    ClassDB::bind_method(D_METHOD("get_SliceScale"),              &C::get_SliceScale);       \
    ClassDB::bind_method(D_METHOD("set_SliceCenter","v"),         &C::set_SliceCenter);      \
    ClassDB::bind_method(D_METHOD("get_SliceCenter"),             &C::get_SliceCenter);      \
    ClassDB::bind_method(D_METHOD("set_ImageRectOffset","v"),     &C::set_ImageRectOffset);  \
    ClassDB::bind_method(D_METHOD("get_ImageRectOffset"),         &C::get_ImageRectOffset);  \
    ClassDB::bind_method(D_METHOD("set_ImageRectSize","v"),       &C::set_ImageRectSize);    \
    ClassDB::bind_method(D_METHOD("get_ImageRectSize"),           &C::get_ImageRectSize);    \
    ClassDB::bind_method(D_METHOD("set_ResampleMode","v"),        &C::set_ResampleMode);     \
    ClassDB::bind_method(D_METHOD("get_ResampleMode"),            &C::get_ResampleMode);     \
    ClassDB::bind_method(D_METHOD("set_gl_tile_size","xs","xo","ys","yo"), &C::set_gl_tile_size); \
    ClassDB::bind_method(D_METHOD("_gl_apply_image_mode"),        &C::_gl_apply_image_mode); \
    ClassDB::bind_method(D_METHOD("_gl_apply_image_rect"),        &C::_gl_apply_image_rect); \
    ADD_PROPERTY(PropertyInfo(Variant::INT,    "ScaleType", PROPERTY_HINT_ENUM,"Stretch,Slice,Tile,Fit,Crop"), "set_ScaleType","get_ScaleType"); \
    ClassDB::bind_method(D_METHOD("set_Image","v"), &C::set_Image); \
    ClassDB::bind_method(D_METHOD("get_Image"),     &C::get_Image); \
    ClassDB::bind_method(D_METHOD("set_ImageColor3","v"), &C::set_ImageColor3); \
    ClassDB::bind_method(D_METHOD("get_ImageColor3"),     &C::get_ImageColor3); \
    ClassDB::bind_method(D_METHOD("set_TileSize","v"), &C::set_TileSize); \
    ClassDB::bind_method(D_METHOD("get_TileSize"),     &C::get_TileSize); \
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "SliceScale"),       "set_SliceScale","get_SliceScale"); \
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "Image"),        "set_Image","get_Image"); \
    ADD_PROPERTY(PropertyInfo(Variant::COLOR,  "ImageColor3"),  "set_ImageColor3","get_ImageColor3"); \
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR4,"TileSize"),     "set_TileSize","get_TileSize");        \
    ADD_PROPERTY(PropertyInfo(Variant::RECT2,  "SliceCenter"),      "set_SliceCenter","get_SliceCenter");      \
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2,"ImageRectOffset"),  "set_ImageRectOffset","get_ImageRectOffset"); \
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2,"ImageRectSize"),    "set_ImageRectSize","get_ImageRectSize");  \
    ADD_PROPERTY(PropertyInfo(Variant::INT,    "ResampleMode", PROPERTY_HINT_ENUM,"Default,Pixelated"), "set_ResampleMode","get_ResampleMode"); \
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "ImageTransparency"), "set_image_transparency","get_image_transparency");

// Carga la textura de un id de Roblox, buscando primero una copia LOCAL puesta
// por el usuario en GodotLuau/assets/rbx/<id>.png (ver gl_local_asset_path).
inline Ref<Texture2D> gl_gui_load_image(Node* owner, const String& path) {
    if (path.is_empty()) return Ref<Texture2D>();
    String real = path;
    if (gl_is_roblox_asset(path)) {
        real = gl_local_asset_path(path);
        if (real.is_empty()) { if (owner) owner->set_meta("__rbx_missing_asset", path); return Ref<Texture2D>(); }
        if (owner) owner->remove_meta("__rbx_missing_asset");
    }
    ResourceLoader* rl = ResourceLoader::get_singleton();
    if (!rl || !rl->exists(real)) return Ref<Texture2D>();
    return rl->load(real);
}

// ────────────────────────────────────────────────────────────────────
//  RobloxImageButton — Image button (clickable)
////  RobloxImageButton — Botón con imagen, señales tipo Roblox.
// ────────────────────────────────────────────────────────────────────
class RobloxImageButton : public Button {
    GDCLASS(RobloxImageButton, Button);
    GUI_COMMON_FIELDS
    GUI_IMAGE_FIELDS
    String _hover_image, _pressed_image;
    bool   _auto_button_color = true;
    Ref<Texture2D> _base_tex;

    std::vector<GuiLuaCallback> _click_cbs;
    std::vector<GuiLuaCallback> _enter_cbs;
    std::vector<GuiLuaCallback> _leave_cbs;

    void _apply_style() {
        Ref<StyleBoxFlat> st; st.instantiate();
        st->set_bg_color(Color(_bg_r, _bg_g, _bg_b, 1.0f - _bg_alpha));
        st->set_border_color(Color(_border_r, _border_g, _border_b));
        st->set_border_width_all(_border_px);
        add_theme_stylebox_override("normal",  st);
        add_theme_stylebox_override("hover",   st);
        add_theme_stylebox_override("pressed", st);
        _gl_gui_reapply_modifiers(this);
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            gl_apply_rbx_gui_meta(this);   // props importadas guardadas como meta
            gl_gui_apply_render_gate(this);  // regla de Roblox: solo dentro de un *Gui
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            set_expand_icon(true);   // el icono (imagen) llena el boton, como Roblox
            set_modulate(Color(_img_r, _img_g, _img_b, _img_alpha));
            if (!is_connected("pressed", Callable(this, "_on_pressed"))) {
                connect("pressed",       Callable(this, "_on_pressed"));
                connect("mouse_entered", Callable(this, "_on_mouse_enter"));
                connect("mouse_exited",  Callable(this, "_on_mouse_leave"));
            }
            _gl_gui_wire_relayout(this, "_on_parent_resized");
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("_gl_disconnect","ref"), &RobloxImageButton::_gl_disconnect);
        ClassDB::bind_method(D_METHOD("_on_pressed"),        &RobloxImageButton::_on_pressed);
        ClassDB::bind_method(D_METHOD("_on_mouse_enter"),    &RobloxImageButton::_on_mouse_enter);
        ClassDB::bind_method(D_METHOD("_on_mouse_leave"),    &RobloxImageButton::_on_mouse_leave);
        ClassDB::bind_method(D_METHOD("set_HoverImage","v"),   &RobloxImageButton::set_HoverImage);
        ClassDB::bind_method(D_METHOD("get_HoverImage"),       &RobloxImageButton::get_HoverImage);
        ClassDB::bind_method(D_METHOD("set_PressedImage","v"), &RobloxImageButton::set_PressedImage);
        ClassDB::bind_method(D_METHOD("get_PressedImage"),     &RobloxImageButton::get_PressedImage);
        ClassDB::bind_method(D_METHOD("set_AutoButtonColor","b"), &RobloxImageButton::set_AutoButtonColor);
        ClassDB::bind_method(D_METHOD("get_AutoButtonColor"),     &RobloxImageButton::get_AutoButtonColor);
        GUI_BIND_COMMON(RobloxImageButton)
        GUI_BIND_IMAGE(RobloxImageButton)
        ADD_PROPERTY(PropertyInfo(Variant::STRING,"HoverImage"),   "set_HoverImage","get_HoverImage");
        ADD_PROPERTY(PropertyInfo(Variant::STRING,"PressedImage"), "set_PressedImage","get_PressedImage");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "AutoButtonColor"), "set_AutoButtonColor","get_AutoButtonColor");
        ADD_SIGNAL(MethodInfo("MouseButton1Click"));
        ADD_SIGNAL(MethodInfo("Activated"));
        ADD_SIGNAL(MethodInfo("MouseButton1Down"));
        ADD_SIGNAL(MethodInfo("MouseButton1Up"));
        ADD_SIGNAL(MethodInfo("MouseEnter"));
        ADD_SIGNAL(MethodInfo("MouseLeave"));
    }

public:
    GUI_COMMON_METHODS(RobloxImageButton)
    GUI_IMAGE_METHODS(RobloxImageButton)

    void _on_parent_resized() {
        GUI_COMMON_APPLY_LAYOUT
        GUI_APPLY_AUTO_SIZE(get_minimum_size())
    }
    void _on_pressed()     { emit_signal("MouseButton1Down"); emit_signal("MouseButton1Click"); emit_signal("Activated"); emit_signal("MouseButton1Up"); _fire_gui_cbs(_click_cbs); }
    void _on_mouse_enter() { emit_signal("MouseEnter"); _fire_gui_cbs(_enter_cbs); }
    void _on_mouse_leave() { emit_signal("MouseLeave"); _fire_gui_cbs(_leave_cbs); }

    void set_image(String path) {
        _image_path = path;
        _base_tex = gl_gui_load_image(this, path);
        _gl_apply_image_rect();
    }
    String get_image() const { return _image_path; }
    void set_HoverImage(String v)   { _hover_image = v; }
    String get_HoverImage() const   { return _hover_image; }
    void set_PressedImage(String v) { _pressed_image = v; }
    String get_PressedImage() const { return _pressed_image; }
    void set_AutoButtonColor(bool b) { _auto_button_color = b; }
    bool get_AutoButtonColor() const { return _auto_button_color; }

    // ImageRectOffset/Size recortan un trozo del atlas (los sprite-sheets de
    // iconos de Roblox). Sin esto salia la hoja entera dentro del boton.
    void _gl_apply_image_rect() {
        if (_base_tex.is_null()) return;
        const Vector2 _asz = _base_tex->get_size();
        const bool _fits = _img_rect_size.x > 0.5f && _img_rect_size.y > 0.5f &&
                           _img_rect_off.x >= 0.0f && _img_rect_off.y >= 0.0f &&
                           _img_rect_off.x + _img_rect_size.x <= _asz.x + 0.5f &&
                           _img_rect_off.y + _img_rect_size.y <= _asz.y + 0.5f;
        // Si la region NO cabe en el atlas, la textura sale vacia y el hilo de
        // render de Godot se cae. En ese caso se usa la imagen entera.
        if (_fits) {
            Ref<AtlasTexture> at; at.instantiate();
            at->set_atlas(_base_tex);
            at->set_region(Rect2(_img_rect_off, _img_rect_size));
            set_button_icon(at);
        } else {
            set_button_icon(_base_tex);
        }
    }
    // ScaleType en un boton: el icono se estira (Stretch) o conserva su
    // proporcion (Fit/Crop).
    void _gl_apply_image_mode() {
        set_expand_icon(_scale_type != 3);
        set_icon_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    }

    void add_click_cb(lua_State* L, int ref)  { _click_cbs.push_back({L, ref, true}); }
    void add_enter_cb(lua_State* L, int ref)  { _enter_cbs.push_back({L, ref, true}); }
    void add_leave_cb(lua_State* L, int ref)  { _leave_cbs.push_back({L, ref, true}); }
    void _gl_disconnect(int ref) {
        for (auto& cb : _click_cbs) if (cb.ref==ref) cb.active=false;
        for (auto& cb : _enter_cbs) if (cb.ref==ref) cb.active=false;
        for (auto& cb : _leave_cbs) if (cb.ref==ref) cb.active=false;
    }
};

// ────────────────────────────────────────────────────────────────────
//  RobloxTextBox — Text input field
////  RobloxTextBox — Campo de entrada de texto
// ────────────────────────────────────────────────────────────────────
class RobloxTextBox : public LineEdit {
    GDCLASS(RobloxTextBox, LineEdit);
    GUI_COMMON_FIELDS
    GUI_TEXT_FIELDS
    Color _placeholder_color = Color(0.7f, 0.7f, 0.7f);
    bool  _clear_on_focus = true;
    bool  _multi_line = false;
    bool  _text_editable = true;

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
        _gl_gui_reapply_modifiers(this);
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            gl_apply_rbx_gui_meta(this);   // props importadas guardadas como meta
            gl_gui_apply_render_gate(this);  // regla de Roblox: solo dentro de un *Gui
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            add_theme_color_override("font_color", Color(_txt_r, _txt_g, _txt_b, _txt_alpha));
            if (!is_connected("focus_exited", Callable(this, "_on_focus_lost"))) {
                connect("focus_exited",  Callable(this, "_on_focus_lost"));
                connect("focus_entered", Callable(this, "_on_focus_gained"));
                connect("text_changed",  Callable(this, "_on_changed"));
            }
            _gl_gui_wire_relayout(this, "_on_parent_resized");
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("_gl_disconnect","ref"), &RobloxTextBox::_gl_disconnect);
        ClassDB::bind_method(D_METHOD("_on_focus_lost"),     &RobloxTextBox::_on_focus_lost);
        ClassDB::bind_method(D_METHOD("_on_focus_gained"),   &RobloxTextBox::_on_focus_gained);
        ClassDB::bind_method(D_METHOD("_on_changed","t"),    &RobloxTextBox::_on_changed);
        ClassDB::bind_method(D_METHOD("set_PlaceholderText","v"),   &RobloxTextBox::set_PlaceholderText);
        ClassDB::bind_method(D_METHOD("get_PlaceholderText"),       &RobloxTextBox::get_PlaceholderText);
        ClassDB::bind_method(D_METHOD("set_PlaceholderColor3","v"), &RobloxTextBox::set_PlaceholderColor3);
        ClassDB::bind_method(D_METHOD("get_PlaceholderColor3"),     &RobloxTextBox::get_PlaceholderColor3);
        ClassDB::bind_method(D_METHOD("set_ClearTextOnFocus","v"),  &RobloxTextBox::set_ClearTextOnFocus);
        ClassDB::bind_method(D_METHOD("get_ClearTextOnFocus"),      &RobloxTextBox::get_ClearTextOnFocus);
        ClassDB::bind_method(D_METHOD("set_MultiLine","v"),         &RobloxTextBox::set_MultiLine);
        ClassDB::bind_method(D_METHOD("get_MultiLine"),             &RobloxTextBox::get_MultiLine);
        ClassDB::bind_method(D_METHOD("set_TextEditable","v"),      &RobloxTextBox::set_TextEditable);
        ClassDB::bind_method(D_METHOD("get_TextEditable"),          &RobloxTextBox::get_TextEditable);
        ClassDB::bind_method(D_METHOD("CaptureFocus"),              &RobloxTextBox::CaptureFocus);
        ClassDB::bind_method(D_METHOD("ReleaseFocus"),              &RobloxTextBox::ReleaseFocusRbx);
        GUI_BIND_COMMON(RobloxTextBox)
        GUI_BIND_TEXT(RobloxTextBox)
        ADD_PROPERTY(PropertyInfo(Variant::STRING,"PlaceholderText"),  "set_PlaceholderText","get_PlaceholderText");
        ADD_PROPERTY(PropertyInfo(Variant::COLOR, "PlaceholderColor3"),"set_PlaceholderColor3","get_PlaceholderColor3");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "ClearTextOnFocus"), "set_ClearTextOnFocus","get_ClearTextOnFocus");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "MultiLine"),        "set_MultiLine","get_MultiLine");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "TextEditable"),     "set_TextEditable","get_TextEditable");
        ADD_SIGNAL(MethodInfo("FocusLost"));
        ADD_SIGNAL(MethodInfo("FocusGained"));
        ADD_SIGNAL(MethodInfo("Changed", PropertyInfo(Variant::STRING,"text")));
    }

public:
    GUI_COMMON_METHODS(RobloxTextBox)
    GUI_TEXT_METHODS(RobloxTextBox)

    String _gl_display_text() const { return get_text(); }
    void _gl_push_text() { set_text(_raw_text); }

    void _on_parent_resized() { GUI_COMMON_APPLY_LAYOUT _gl_refit_text(); }
    void _on_focus_lost() {
        emit_signal("FocusLost");   _fire_gui_cbs(_focus_lost_cbs);
    }
    void _on_focus_gained() {
        if (_clear_on_focus) set_text("");
        emit_signal("FocusGained"); _fire_gui_cbs(_focus_gained_cbs);
    }
    void _on_changed(String t) { emit_signal("Changed", t); }

    void set_PlaceholderText(String v) { set_placeholder(v); }
    String get_PlaceholderText() const { return get_placeholder(); }
    void set_PlaceholderColor3(Color c) {
        _placeholder_color = c;
        add_theme_color_override("font_placeholder_color", c);
    }
    Color get_PlaceholderColor3() const { return _placeholder_color; }
    void set_ClearTextOnFocus(bool b) { _clear_on_focus = b; }
    bool get_ClearTextOnFocus() const { return _clear_on_focus; }
    void set_MultiLine(bool b) { _multi_line = b; }
    bool get_MultiLine() const { return _multi_line; }
    void set_TextEditable(bool b) { _text_editable = b; set_editable(b); }
    bool get_TextEditable() const { return _text_editable; }
    void CaptureFocus() { grab_focus(); }
    void ReleaseFocusRbx() { release_focus(); }

    void add_focus_lost_cb(lua_State* L, int ref)   { _focus_lost_cbs.push_back({L, ref, true}); }
    void add_focus_gained_cb(lua_State* L, int ref) { _focus_gained_cbs.push_back({L, ref, true}); }
    void _gl_disconnect(int ref) {
        for (auto& cb : _focus_lost_cbs)   if (cb.ref==ref) cb.active=false;
        for (auto& cb : _focus_gained_cbs) if (cb.ref==ref) cb.active=false;
        for (auto& cb : _changed_cbs)      if (cb.ref==ref) cb.active=false;
    }
};

// ────────────────────────────────────────────────────────────────────
//  RobloxImageLabel — Static image
////  RobloxImageLabel — Imagen estática
// ────────────────────────────────────────────────────────────────────
class RobloxImageLabel : public TextureRect {
    GDCLASS(RobloxImageLabel, TextureRect);
    GUI_COMMON_FIELDS
    GUI_IMAGE_FIELDS
    Ref<Texture2D> _base_tex;

    void _apply_style() {
        set_modulate(Color(_img_r, _img_g, _img_b, _img_alpha));
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            gl_apply_rbx_gui_meta(this);   // props importadas guardadas como meta
            gl_gui_apply_render_gate(this);  // regla de Roblox: solo dentro de un *Gui
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            _gl_apply_image_mode();
            _gl_gui_wire_relayout(this, "_on_parent_resized");
        }
    }

protected:
    static void _bind_methods() {
        GUI_BIND_COMMON(RobloxImageLabel)
        GUI_BIND_IMAGE(RobloxImageLabel)
    }

public:
    GUI_COMMON_METHODS(RobloxImageLabel)
    GUI_IMAGE_METHODS(RobloxImageLabel)

    void _on_parent_resized() {
        GUI_COMMON_APPLY_LAYOUT
        GUI_APPLY_AUTO_SIZE(_base_tex.is_valid() ? _base_tex->get_size() : get_size())
    }

    void set_image(String path) {
        _image_path = path;
        _base_tex = gl_gui_load_image(this, path);
        _gl_apply_image_rect();
    }
    String get_image() const { return _image_path; }

    void _gl_apply_image_rect() {
        if (_base_tex.is_null()) return;
        const Vector2 _asz = _base_tex->get_size();
        const bool _fits = _img_rect_size.x > 0.5f && _img_rect_size.y > 0.5f &&
                           _img_rect_off.x >= 0.0f && _img_rect_off.y >= 0.0f &&
                           _img_rect_off.x + _img_rect_size.x <= _asz.x + 0.5f &&
                           _img_rect_off.y + _img_rect_size.y <= _asz.y + 0.5f;
        // Si la region NO cabe en el atlas, la textura sale vacia y el hilo de
        // render de Godot se cae. En ese caso se usa la imagen entera.
        if (_fits) {
            Ref<AtlasTexture> at; at.instantiate();
            at->set_atlas(_base_tex);
            at->set_region(Rect2(_img_rect_off, _img_rect_size));
            set_texture(at);
        } else {
            set_texture(_base_tex);
        }
    }

    // Enum.ScaleType: Stretch=0, Slice=1, Tile=2, Fit=3, Crop=4.
    // Sin esto TODAS las imagenes se estiraban: los iconos redondos salian
    // ovalados y los fondos con patron perdian el mosaico.
    void _gl_apply_image_mode() {
        switch (_scale_type) {
            case 2:  set_stretch_mode(TextureRect::STRETCH_TILE); break;
            case 3:  set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED); break;
            case 4:  set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_COVERED); break;
            default: set_stretch_mode(TextureRect::STRETCH_SCALE); break;
        }
    }
};

// ────────────────────────────────────────────────────────────────────
//  RobloxScrollingFrame — Scrollable container
//  Antes heredaba de ScrollContainer, que es un CONTENEDOR de Godot: forzaba
//  el rect de sus hijos y por eso todo lo que hubiera dentro de un
//  ScrollingFrame importado salia estirado y encima de lo demas. Ahora es un
//  Panel con recorte + desplazamiento propio: los hijos conservan su UDim2 y
//  el scroll los traslada (gl_gui_apply_layout consulta _gl_scroll).
////  RobloxScrollingFrame — Contenedor con scroll
// ────────────────────────────────────────────────────────────────────
class RobloxScrollingFrame : public Panel {
    GDCLASS(RobloxScrollingFrame, Panel);
    GUI_COMMON_FIELDS
    GuiUDim2 _canvas_size = {0, 0, 0, 0};
    Vector2  _canvas_pos;
    bool     _scrolling_enabled = true;
    int      _scroll_dir = 4;      // Enum.ScrollingDirection: X=1, Y=2, XY=4
    int      _bar_thickness = 12;
    Color    _bar_color = Color(0.7f, 0.7f, 0.7f);
    float    _bar_alpha = 1.0f;
    int      _auto_canvas = 0;     // None=0, X=1, Y=2, XY=3
    int      _elastic = 2;         // WhenScrollable
    int      _v_inset = 0, _h_inset = 0, _v_bar_pos = 0;
    String   _top_image, _mid_image, _bottom_image;

    void _apply_style() {
        Ref<StyleBoxFlat> style;
        style.instantiate();
        style->set_bg_color(Color(_bg_r, _bg_g, _bg_b, 1.0f - _bg_alpha));
        style->set_border_color(Color(_border_r, _border_g, _border_b));
        style->set_border_width_all(_border_px);
        add_theme_stylebox_override("panel", style);
        _gl_gui_reapply_modifiers(this);
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            gl_apply_rbx_gui_meta(this);   // props importadas guardadas como meta
            gl_gui_apply_render_gate(this);  // regla de Roblox: solo dentro de un *Gui
            set_clip_contents(true);       // un ScrollingFrame de Roblox recorta siempre
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            _gl_gui_wire_relayout(this, "_on_parent_resized");
        }
    }

    // Extremo maximo al que se puede desplazar.
    Vector2 _max_scroll() const {
        const Vector2 own = get_size();
        Vector2 canvas(_canvas_size.xs * own.x + _canvas_size.xo,
                       _canvas_size.ys * own.y + _canvas_size.yo);
        if (_auto_canvas != 0) {
            const Vector2 ext = gl_gui_children_extent(const_cast<RobloxScrollingFrame*>(this));
            if (_auto_canvas == 1 || _auto_canvas == 3) canvas.x = MAX(canvas.x, ext.x);
            if (_auto_canvas == 2 || _auto_canvas == 3) canvas.y = MAX(canvas.y, ext.y);
        }
        return Vector2(MAX(0.0f, canvas.x - own.x), MAX(0.0f, canvas.y - own.y));
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_ScrollingEnabled","b"),  &RobloxScrollingFrame::set_ScrollingEnabled);
        ClassDB::bind_method(D_METHOD("get_ScrollingEnabled"),      &RobloxScrollingFrame::get_ScrollingEnabled);
        ClassDB::bind_method(D_METHOD("set_gl_canvas_size","xs","xo","ys","yo"), &RobloxScrollingFrame::set_gl_canvas_size);
        ClassDB::bind_method(D_METHOD("set_CanvasSize","v"),        &RobloxScrollingFrame::set_CanvasSize);
        ClassDB::bind_method(D_METHOD("get_CanvasSize"),            &RobloxScrollingFrame::get_CanvasSize);
        ClassDB::bind_method(D_METHOD("set_CanvasPosition","v"),    &RobloxScrollingFrame::set_CanvasPosition);
        ClassDB::bind_method(D_METHOD("get_CanvasPosition"),        &RobloxScrollingFrame::get_CanvasPosition);
        ClassDB::bind_method(D_METHOD("set_ScrollBarThickness","v"),&RobloxScrollingFrame::set_ScrollBarThickness);
        ClassDB::bind_method(D_METHOD("get_ScrollBarThickness"),    &RobloxScrollingFrame::get_ScrollBarThickness);
        ClassDB::bind_method(D_METHOD("set_ScrollBarImageColor3","v"), &RobloxScrollingFrame::set_ScrollBarImageColor3);
        ClassDB::bind_method(D_METHOD("get_ScrollBarImageColor3"),     &RobloxScrollingFrame::get_ScrollBarImageColor3);
        ClassDB::bind_method(D_METHOD("set_ScrollBarImageTransparency","v"), &RobloxScrollingFrame::set_ScrollBarImageTransparency);
        ClassDB::bind_method(D_METHOD("get_ScrollBarImageTransparency"),     &RobloxScrollingFrame::get_ScrollBarImageTransparency);
        ClassDB::bind_method(D_METHOD("set_ScrollingDirection","v"),&RobloxScrollingFrame::set_ScrollingDirection);
        ClassDB::bind_method(D_METHOD("get_ScrollingDirection"),    &RobloxScrollingFrame::get_ScrollingDirection);
        ClassDB::bind_method(D_METHOD("set_AutomaticCanvasSize","v"), &RobloxScrollingFrame::set_AutomaticCanvasSize);
        ClassDB::bind_method(D_METHOD("get_AutomaticCanvasSize"),     &RobloxScrollingFrame::get_AutomaticCanvasSize);
        ClassDB::bind_method(D_METHOD("set_ElasticBehavior","v"),  &RobloxScrollingFrame::set_ElasticBehavior);
        ClassDB::bind_method(D_METHOD("get_ElasticBehavior"),      &RobloxScrollingFrame::get_ElasticBehavior);
        ClassDB::bind_method(D_METHOD("set_VerticalScrollBarInset","v"),   &RobloxScrollingFrame::set_VerticalScrollBarInset);
        ClassDB::bind_method(D_METHOD("get_VerticalScrollBarInset"),       &RobloxScrollingFrame::get_VerticalScrollBarInset);
        ClassDB::bind_method(D_METHOD("set_HorizontalScrollBarInset","v"), &RobloxScrollingFrame::set_HorizontalScrollBarInset);
        ClassDB::bind_method(D_METHOD("get_HorizontalScrollBarInset"),     &RobloxScrollingFrame::get_HorizontalScrollBarInset);
        ClassDB::bind_method(D_METHOD("set_VerticalScrollBarPosition","v"),&RobloxScrollingFrame::set_VerticalScrollBarPosition);
        ClassDB::bind_method(D_METHOD("get_VerticalScrollBarPosition"),    &RobloxScrollingFrame::get_VerticalScrollBarPosition);
        ClassDB::bind_method(D_METHOD("set_TopImage","v"),    &RobloxScrollingFrame::set_TopImage);
        ClassDB::bind_method(D_METHOD("get_TopImage"),        &RobloxScrollingFrame::get_TopImage);
        ClassDB::bind_method(D_METHOD("set_MidImage","v"),    &RobloxScrollingFrame::set_MidImage);
        ClassDB::bind_method(D_METHOD("get_MidImage"),        &RobloxScrollingFrame::get_MidImage);
        ClassDB::bind_method(D_METHOD("set_BottomImage","v"), &RobloxScrollingFrame::set_BottomImage);
        ClassDB::bind_method(D_METHOD("get_BottomImage"),     &RobloxScrollingFrame::get_BottomImage);
        ClassDB::bind_method(D_METHOD("_gl_scroll"),          &RobloxScrollingFrame::_gl_scroll);
        GUI_BIND_COMMON(RobloxScrollingFrame)

        ADD_GROUP("ScrollingFrame","");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,   "ScrollingEnabled"),  "set_ScrollingEnabled","get_ScrollingEnabled");
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR4,"CanvasSize"),        "set_CanvasSize","get_CanvasSize");
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR2,"CanvasPosition"),    "set_CanvasPosition","get_CanvasPosition");
        ADD_PROPERTY(PropertyInfo(Variant::INT,    "ScrollBarThickness"),"set_ScrollBarThickness","get_ScrollBarThickness");
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,  "ScrollBarImageColor3"),      "set_ScrollBarImageColor3","get_ScrollBarImageColor3");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "ScrollBarImageTransparency"),"set_ScrollBarImageTransparency","get_ScrollBarImageTransparency");
        ADD_PROPERTY(PropertyInfo(Variant::INT,    "ScrollingDirection", PROPERTY_HINT_ENUM,"None,X,Y,,XY"), "set_ScrollingDirection","get_ScrollingDirection");
        ADD_PROPERTY(PropertyInfo(Variant::INT,    "AutomaticCanvasSize",PROPERTY_HINT_ENUM,"None,X,Y,XY"), "set_AutomaticCanvasSize","get_AutomaticCanvasSize");
        ADD_PROPERTY(PropertyInfo(Variant::INT,    "ElasticBehavior"),   "set_ElasticBehavior","get_ElasticBehavior");
        ADD_PROPERTY(PropertyInfo(Variant::INT,    "VerticalScrollBarInset"),   "set_VerticalScrollBarInset","get_VerticalScrollBarInset");
        ADD_PROPERTY(PropertyInfo(Variant::INT,    "HorizontalScrollBarInset"), "set_HorizontalScrollBarInset","get_HorizontalScrollBarInset");
        ADD_PROPERTY(PropertyInfo(Variant::INT,    "VerticalScrollBarPosition"),"set_VerticalScrollBarPosition","get_VerticalScrollBarPosition");
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "TopImage"),    "set_TopImage","get_TopImage");
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "MidImage"),    "set_MidImage","get_MidImage");
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "BottomImage"), "set_BottomImage","get_BottomImage");
    }

public:
    GUI_COMMON_METHODS(RobloxScrollingFrame)

    void _on_parent_resized() { GUI_COMMON_APPLY_LAYOUT }

    Vector2 _gl_scroll() const { return _canvas_pos; }

    void set_ScrollingEnabled(bool b) { _scrolling_enabled = b; }
    bool get_ScrollingEnabled() const { return _scrolling_enabled; }
    void set_gl_canvas_size(float xs, float xo, float ys, float yo) { _canvas_size = {xs, xo, ys, yo}; }
    void set_CanvasPosition(Vector2 v) {
        const Vector2 mx = _max_scroll();
        _canvas_pos = Vector2(Math::clamp(v.x, 0.0f, mx.x), Math::clamp(v.y, 0.0f, mx.y));
        gl_gui_relayout_children(this);
    }
    Vector2 get_CanvasPosition() const { return _canvas_pos; }
    void set_CanvasSize(Vector4 v) { _canvas_size = {v.x, v.y, v.z, v.w}; }
    Vector4 get_CanvasSize() const {
        return Vector4(_canvas_size.xs, _canvas_size.xo, _canvas_size.ys, _canvas_size.yo);
    }
    void set_ScrollBarThickness(int v) { _bar_thickness = v; }
    int  get_ScrollBarThickness() const { return _bar_thickness; }
    void set_ScrollBarImageColor3(Color c) { _bar_color = c; }
    Color get_ScrollBarImageColor3() const { return _bar_color; }
    void set_ScrollBarImageTransparency(float v) { _bar_alpha = 1.0f - Math::clamp(v, 0.0f, 1.0f); }
    float get_ScrollBarImageTransparency() const { return 1.0f - _bar_alpha; }
    void set_ScrollingDirection(int v) { _scroll_dir = v; }
    int  get_ScrollingDirection() const { return _scroll_dir; }
    void set_AutomaticCanvasSize(int v) { _auto_canvas = v; }
    int  get_AutomaticCanvasSize() const { return _auto_canvas; }
    void set_ElasticBehavior(int v) { _elastic = v; }
    int  get_ElasticBehavior() const { return _elastic; }
    void set_VerticalScrollBarInset(int v) { _v_inset = v; }
    int  get_VerticalScrollBarInset() const { return _v_inset; }
    void set_HorizontalScrollBarInset(int v) { _h_inset = v; }
    int  get_HorizontalScrollBarInset() const { return _h_inset; }
    void set_VerticalScrollBarPosition(int v) { _v_bar_pos = v; }
    int  get_VerticalScrollBarPosition() const { return _v_bar_pos; }
    void set_TopImage(String v) { _top_image = v; }
    String get_TopImage() const { return _top_image; }
    void set_MidImage(String v) { _mid_image = v; }
    String get_MidImage() const { return _mid_image; }
    void set_BottomImage(String v) { _bottom_image = v; }
    String get_BottomImage() const { return _bottom_image; }

    void _gui_input(const Ref<InputEvent>& event) override {
        if (!_scrolling_enabled) return;
        Ref<InputEventMouseButton> mb = event;
        if (mb.is_null() || !mb->is_pressed()) return;
        const float step = 40.0f;
        Vector2 next = _canvas_pos;
        const bool can_x = (_scroll_dir == 1 || _scroll_dir == 4);
        const bool can_y = (_scroll_dir == 2 || _scroll_dir == 4);
        switch (mb->get_button_index()) {
            case MOUSE_BUTTON_WHEEL_UP:    if (can_y) next.y -= step; break;
            case MOUSE_BUTTON_WHEEL_DOWN:  if (can_y) next.y += step; break;
            case MOUSE_BUTTON_WHEEL_LEFT:  if (can_x) next.x -= step; break;
            case MOUSE_BUTTON_WHEEL_RIGHT: if (can_x) next.x += step; break;
            default: return;
        }
        if (next == _canvas_pos) return;
        set_CanvasPosition(next);
        accept_event();
    }
};

#endif // ROBLOX_GUI_H
