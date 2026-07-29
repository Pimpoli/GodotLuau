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
#include "gl_asset.h"
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

// ────────────────────────────────────────────────────────────────────
//  UDim2 layout helpers
////  Helpers de layout UDim2
// ────────────────────────────────────────────────────────────────────
struct GuiUDim2 { float xs = 0, xo = 0, ys = 0, yo = 0; };

static void _gui_apply_layout(Control* ctrl, const GuiUDim2& pos, const GuiUDim2& sz,
                               float ax = 0, float ay = 0) {
    if (!ctrl) return;
    // Roblox UDim2 -> anclas + offsets NATIVAS de Godot. Mapeo EXACTO que no
    // depende del tamaño del padre en el instante de aplicarlo: Godot recalcula el
    // rect solo cuando el padre cambia de tamaño, así que ya no importa que al
    // ENTER_TREE el padre valga 0 (era la causa de que un Size=(1,1) saliera 100x50).
    // El AnchorPoint (ax,ay) se hornea en las anclas/offsets. Y como anclas/offsets
    // SÍ son propiedades de Control, sobreviven a duplicate() → los ScreenGui que se
    // clonan de StarterGui a PlayerGui conservan su posición y tamaño.
    //   rect_scale  = posScale + (side==far ? (1-a)*sizeScale : -a*sizeScale)
    //   rect_offset = posOffset + (side==far ? (1-a)*sizeOffset : -a*sizeOffset)
    ctrl->set_anchor(SIDE_LEFT,   pos.xs - ax * sz.xs);
    ctrl->set_anchor(SIDE_RIGHT,  pos.xs + (1.0f - ax) * sz.xs);
    ctrl->set_anchor(SIDE_TOP,    pos.ys - ay * sz.ys);
    ctrl->set_anchor(SIDE_BOTTOM, pos.ys + (1.0f - ay) * sz.ys);
    ctrl->set_offset(SIDE_LEFT,   pos.xo - ax * sz.xo);
    ctrl->set_offset(SIDE_RIGHT,  pos.xo + (1.0f - ax) * sz.xo);
    ctrl->set_offset(SIDE_TOP,    pos.yo - ay * sz.yo);
    ctrl->set_offset(SIDE_BOTTOM, pos.yo + (1.0f - ay) * sz.yo);
    ctrl->set_custom_minimum_size(Vector2(0, 0));
}

// Re-aplica los modificadores hijos (UICorner/UIStroke/UIGradient…) tras
// reconstruir el StyleBox del control: como _apply_style crea una caja nueva,
// el redondeo/borde se perdía en cuanto el fondo cambiaba (1.15). Los
// modificadores exponen `_apply` (roblox_behavior.h / roblox_extra.h).
static void _gl_gui_reapply_modifiers(Control* c) {
    if (!c) return;
    for (int i = 0; i < c->get_child_count(); i++) {
        Node* ch = c->get_child(i);
        if (ch && ch->has_method("_apply")) ch->call_deferred("_apply");
    }
}

// Reconecta el re-layout: al padre si es Control, o al VIEWPORT si es top-level
// (padre CanvasLayer/ScreenGui). Antes solo se conectaba al padre Control, así
// que una GUI top-level nunca se re-ajustaba cuando cambiaba el tamaño de la
// pantalla — se quedaba con el tamaño del viewport que hubiera en ENTER_TREE
// (a veces aún sin resolver: el clásico "todo amontonado arriba-izquierda").
// El call_deferred re-aplica cuando el viewport ya tiene su tamaño real.
static void _gl_gui_wire_relayout(Control* self, const char* method) {
    if (!self) return;
    Control* parent_ctrl = Object::cast_to<Control>(self->get_parent());
    if (parent_ctrl) {
        if (!parent_ctrl->is_connected("resized", Callable(self, method)))
            parent_ctrl->connect("resized", Callable(self, method));
    } else if (Viewport* vp = self->get_viewport()) {
        if (!vp->is_connected("size_changed", Callable(self, method)))
            vp->connect("size_changed", Callable(self, method));
    }
    self->call_deferred(method);
}

// ────────────────────────────────────────────────────────────────────
//  Propiedades de GuiObject de Roblox que NO son simples setters de la clase.
//  Se aplican aqui, en un solo sitio, para que las use tanto el importador como
//  el repintado desde metadatos. Sin esto, un place importado perdia la
//  alineacion del texto, el contorno, la rotacion y el recorte, y por eso las
//  UIs se veian desordenadas aunque el tamaño y el color fueran correctos.
////  Roblox GuiObject properties that need translation (not plain setters).
inline bool gl_apply_rbx_gui_extra(Node* n, const String& prop, const Variant& v) {
    Control* c = Object::cast_to<Control>(n);
    if (!c) return false;

    // ── Recorte y rotacion (cualquier GuiObject) ─────────────────────
    if (prop == "ClipsDescendants") { c->set_clip_contents((bool)v); return true; }
    if (prop == "Rotation") {
        // Roblox rota alrededor del CENTRO del elemento.
        c->set_pivot_offset(c->get_size() * 0.5f);
        c->set_rotation((float)Math::deg_to_rad((double)v));
        return true;
    }
    if (prop == "Visible")  { c->set_visible((bool)v); return true; }

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
//  Aplica las props de GUI de Roblox que el importador dejo como METADATA
//  (Position/Size/BackgroundColor3/Text/...). Los .rbxl importados con
//  versiones viejas guardaban esas props como meta porque no sabian
//  traducirlas a los metodos (set_udim2_pos, etc.), dejando cada Frame con su
//  default (blanco, ~100x50, arriba-izquierda). Esto las aplica al CARGAR la
//  escena, asi se arreglan sin re-importar. Cada meta se borra tras aplicarla
//  (idempotente y no ensucia el inspector). Para UI creada por Lua no hay estas
//  metas, asi que es un no-op.
////  Apply Roblox GUI props the importer left as METADATA.
static void gl_apply_rbx_gui_meta(Node* n) {
    if (!n) return;
    // NO se borran las metas: así sobreviven a duplicate() y el ScreenGui clonado
    // a PlayerGui (o un reparent) las vuelve a aplicar. Reaplicar es idempotente.
    auto take = [&](const char* k) -> Variant {
        return n->get_meta(k, Variant());
    };
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

    // Props que necesitan traduccion (alineacion, contorno, rotacion, recorte).
    static const char* kExtra[] = { "ClipsDescendants", "Rotation", "TextXAlignment",
        "TextYAlignment", "TextWrapped", "TextTransparency",
        "TextStrokeTransparency", "TextStrokeColor3" };
    for (int i = 0; i < 8; i++)
        if (n->has_meta(kExtra[i])) gl_apply_rbx_gui_extra(n, kExtra[i], n->get_meta(kExtra[i]));
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

    // StarterGui es una PLANTILLA: al entrar un jugador su contenido se clona a
    // PlayerGui (roblox_services.h) y solo esa copia se ve. Sin esto la interfaz
    // sale DOS veces, y en un place importado con 9 ScreenGui en StarterGui la
    // pantalla se llena de recuadros. Igual que en Roblox, el original no se
    // dibuja; en el editor si, para poder disenarlo.
    bool _gl_is_template() const {
        if (Engine::get_singleton()->is_editor_hint()) return false;
        for (const Node* p = get_parent(); p; p = p->get_parent())
            if (p->get_name() == StringName("StarterGui")) return true;
        return false;
    }

    void _notification(int p_what) {
        if (p_what != NOTIFICATION_ENTER_TREE) return;
        // El ORIGINAL bajo StarterGui es una plantilla: se oculta (su copia en
        // PlayerGui es la que se ve). Pero la COPIA hereda visible=false al
        // duplicarse (el original ya estaba oculto), y sin esto se quedaba
        // invisible: el HUD del jugador NUNCA aparecia. Al entrar en PlayerGui
        // ya no es plantilla, asi que se fuerza visible segun Enabled. Igual en
        // el editor, donde se muestra para poder disenarla.
        set_visible(_gl_is_template() ? false : enabled);
    }

public:
    void set_sg_enabled(bool b) {
        enabled = b;
        set_visible(b && !_gl_is_template());
    }
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
        _gl_gui_reapply_modifiers(this);
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            gl_apply_rbx_gui_meta(this);   // props importadas guardadas como meta
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            // Connect to parent to re-layout when parent resizes
            //// Conectar al padre para re-layout cuando el padre cambie de tamaño
            _gl_gui_wire_relayout(this, "_on_parent_resized");
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
            gl_apply_rbx_gui_meta(this);   // props importadas guardadas como meta
            // clip_text: sin esto el mínimo del Label es el ancho del texto, que
            // clampaba el tamaño UDim2 y descuadraba el anchor. Con clip, el box
            // vale exactamente su Size (el texto que no cabe se recorta).
            set_clip_text(true);
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            add_theme_color_override("font_color", Color(_txt_r, _txt_g, _txt_b));
            if (_text_scaled) set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
            _gl_gui_wire_relayout(this, "_on_parent_resized");
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
        _gl_gui_reapply_modifiers(this);
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            gl_apply_rbx_gui_meta(this);   // props importadas guardadas como meta
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            set_clip_text(true);   // el box vale su Size, no el ancho del texto
            add_theme_color_override("font_color", Color(_txt_r, _txt_g, _txt_b));
            connect("pressed",      Callable(this, "_on_pressed"));
            connect("mouse_entered",Callable(this, "_on_mouse_enter"));
            connect("mouse_exited", Callable(this, "_on_mouse_leave"));
            _gl_gui_wire_relayout(this, "_on_parent_resized");
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("_gl_disconnect","ref"), &RobloxTextButton::_gl_disconnect);
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
        ADD_SIGNAL(MethodInfo("Activated"));   // Roblox GuiButton.Activated
        ADD_SIGNAL(MethodInfo("MouseEnter"));
        ADD_SIGNAL(MethodInfo("MouseLeave"));
    }

public:
    GUI_COMMON_METHODS(RobloxTextButton)

    void _on_parent_resized() { GUI_COMMON_APPLY_LAYOUT }
    void _on_pressed()        { emit_signal("MouseButton1Click"); emit_signal("Activated"); _fire_gui_cbs(_click_cbs); }
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
    void _gl_disconnect(int ref) {
        for (auto& cb : _click_cbs) if (cb.ref==ref) cb.active=false;
        for (auto& cb : _enter_cbs) if (cb.ref==ref) cb.active=false;
        for (auto& cb : _leave_cbs) if (cb.ref==ref) cb.active=false;
    }
};

// ────────────────────────────────────────────────────────────────────
//  RobloxImageButton — Image button (clickable). Antes ImageButton mapeaba a
//  RobloxImageLabel y no tenia Activated/MouseButton1Click, asi que TODOS los
//  botones-imagen de los juegos importados fallaban ("Activated is not a valid
//  member of ImageLabel"). Esta clase es un boton de verdad: fondo + imagen
//  (como icono que llena) + eventos tipo Roblox.
////  RobloxImageButton — Botón con imagen, señales tipo Roblox.
// ────────────────────────────────────────────────────────────────────
class RobloxImageButton : public Button {
    GDCLASS(RobloxImageButton, Button);
    GUI_COMMON_FIELDS
    String _image_path;
    float  _img_r = 1, _img_g = 1, _img_b = 1, _img_alpha = 1;

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
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            set_expand_icon(true);   // el icono (imagen) llena el boton, como Roblox
            set_modulate(Color(_img_r, _img_g, _img_b, _img_alpha));
            connect("pressed",       Callable(this, "_on_pressed"));
            connect("mouse_entered", Callable(this, "_on_mouse_enter"));
            connect("mouse_exited",  Callable(this, "_on_mouse_leave"));
            _gl_gui_wire_relayout(this, "_on_parent_resized");
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("_gl_disconnect","ref"), &RobloxImageButton::_gl_disconnect);
        ClassDB::bind_method(D_METHOD("_on_parent_resized"), &RobloxImageButton::_on_parent_resized);
        ClassDB::bind_method(D_METHOD("_on_pressed"),        &RobloxImageButton::_on_pressed);
        ClassDB::bind_method(D_METHOD("_on_mouse_enter"),    &RobloxImageButton::_on_mouse_enter);
        ClassDB::bind_method(D_METHOD("_on_mouse_leave"),    &RobloxImageButton::_on_mouse_leave);
        ClassDB::bind_method(D_METHOD("set_udim2_pos","xs","xo","ys","yo"),  &RobloxImageButton::set_udim2_pos);
        ClassDB::bind_method(D_METHOD("set_udim2_size","xs","xo","ys","yo"), &RobloxImageButton::set_udim2_size);
        ClassDB::bind_method(D_METHOD("set_anchor_point","ax","ay"),         &RobloxImageButton::set_anchor_point);
        ClassDB::bind_method(D_METHOD("set_bg_color","r","g","b"),           &RobloxImageButton::set_bg_color);
        ClassDB::bind_method(D_METHOD("set_bg_alpha","a"),                   &RobloxImageButton::set_bg_alpha);
        ClassDB::bind_method(D_METHOD("set_border_color","r","g","b"),       &RobloxImageButton::set_border_color);
        ClassDB::bind_method(D_METHOD("set_border_px","px"),                 &RobloxImageButton::set_border_px);
        ClassDB::bind_method(D_METHOD("set_image","path"),                   &RobloxImageButton::set_image);
        ClassDB::bind_method(D_METHOD("set_image_color","r","g","b"),        &RobloxImageButton::set_image_color);
        ClassDB::bind_method(D_METHOD("set_image_transparency","a"),         &RobloxImageButton::set_image_transparency);
        ADD_SIGNAL(MethodInfo("MouseButton1Click"));
        ADD_SIGNAL(MethodInfo("Activated"));
        ADD_SIGNAL(MethodInfo("MouseButton1Down"));
        ADD_SIGNAL(MethodInfo("MouseButton1Up"));
        ADD_SIGNAL(MethodInfo("MouseEnter"));
        ADD_SIGNAL(MethodInfo("MouseLeave"));
    }

public:
    GUI_COMMON_METHODS(RobloxImageButton)

    void _on_parent_resized() { GUI_COMMON_APPLY_LAYOUT }
    void _on_pressed()     { emit_signal("MouseButton1Click"); emit_signal("Activated"); _fire_gui_cbs(_click_cbs); }
    void _on_mouse_enter() { emit_signal("MouseEnter"); _fire_gui_cbs(_enter_cbs); }
    void _on_mouse_leave() { emit_signal("MouseLeave"); _fire_gui_cbs(_leave_cbs); }

    void set_image(String path) {
        _image_path = path;
        if (path.is_empty()) return;
        String real = path;
        if (gl_is_roblox_asset(path)) {
            real = gl_local_asset_path(path);
            if (real.is_empty()) { set_meta("__rbx_missing_asset", path); return; }
            remove_meta("__rbx_missing_asset");
        }
        ResourceLoader* rl = ResourceLoader::get_singleton();
        if (!rl || !rl->exists(real)) return;
        Ref<Texture2D> tex = rl->load(real);
        if (tex.is_valid()) set_button_icon(tex);
    }
    void set_image_color(float r, float g, float b) {
        _img_r = r; _img_g = g; _img_b = b;
        set_modulate(Color(_img_r, _img_g, _img_b, _img_alpha));
    }
    void set_image_transparency(float a) {
        _img_alpha = 1.0f - Math::clamp(a, 0.0f, 1.0f);
        set_modulate(Color(_img_r, _img_g, _img_b, _img_alpha));
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
            gl_apply_rbx_gui_meta(this);   // props importadas guardadas como meta
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            add_theme_color_override("font_color", Color(_txt_r, _txt_g, _txt_b));
            connect("focus_exited",  Callable(this, "_on_focus_lost"));
            connect("focus_entered", Callable(this, "_on_focus_gained"));
            connect("text_changed",  Callable(this, "_on_changed"));
            _gl_gui_wire_relayout(this, "_on_parent_resized");
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("_gl_disconnect","ref"), &RobloxTextBox::_gl_disconnect);
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
    String _image_path;
    float  _img_r = 1, _img_g = 1, _img_b = 1, _img_alpha = 1;

    void _apply_style() {
        set_modulate(Color(_img_r, _img_g, _img_b, _img_alpha));
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) {
            gl_apply_rbx_gui_meta(this);   // props importadas guardadas como meta
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            _gl_gui_wire_relayout(this, "_on_parent_resized");
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
        // Imagen de la nube de Roblox: no se puede descargar. Se guarda el id y
        // se sale sin tocar ResourceLoader; si no, cada ImageLabel importado
        // suelta dos errores rojos y un place trae cientos.
        // Asset de la nube: se busca primero una copia LOCAL puesta por el usuario
        // en GodotLuau/assets/rbx/<id>.png (ver gl_local_asset_path).
        String real = path;
        if (gl_is_roblox_asset(path)) {
            real = gl_local_asset_path(path);
            if (real.is_empty()) { set_meta("__rbx_missing_asset", path); return; }
            remove_meta("__rbx_missing_asset");
        }
        ResourceLoader* rl = ResourceLoader::get_singleton();
        if (!rl || !rl->exists(real)) return;
        Ref<Texture2D> tex = rl->load(real);
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
            gl_apply_rbx_gui_meta(this);   // props importadas guardadas como meta
            GUI_COMMON_APPLY_LAYOUT
            _apply_style();
            _gl_gui_wire_relayout(this, "_on_parent_resized");
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
