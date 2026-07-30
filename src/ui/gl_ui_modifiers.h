#ifndef GL_UI_MODIFIERS_H
#define GL_UI_MODIFIERS_H

// ════════════════════════════════════════════════════════════════════
//  Modificadores de UI de Roblox (UICorner, UIListLayout, UIGridLayout,
//  UIPadding, UIStroke, UIGradient, UIScale, las constraints…) con TODAS
//  sus propiedades y con la semantica real de Roblox.
//
//  Por que se reescribieron: las versiones viejas llamaban a set_position /
//  set_size sobre TODOS los hijos del padre en cada frame:
//
//    · UIPadding forzaba a cada hijo a llenar el area entera  -> todas las UIs
//      de un panel salian apiladas una encima de otra en el mismo sitio.
//    · UIAspectRatioConstraint y UISizeConstraint reescribian el tamaño del
//      padre cada frame, peleandose con las anclas -> la UI "temblaba" y
//      acababa donde no debia.
//    · UIPageLayout forzaba Visible en los hijos -> aparecian paneles que el
//      juego tenia guardados.
//    · UIListLayout ignoraba LayoutOrder, asi que los botones salian en el
//      orden del arbol y no en el del juego.
//
//  Ahora las CONSTRAINTS son solo datos (las aplica el propio GuiObject al
//  colocarse, en gl_gui_core.h) y los LAYOUTS solo recolocan cuando algo ha
//  cambiado de verdad (huella gl_gui_children_stamp).
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/style_box_texture.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include "gl_gui_core.h"

using namespace godot;

// Re-pinta los modificadores de estilo del control (UICorner/UIStroke/UIGradient)
// tras reconstruir su StyleBox.
inline void gl_ui_restyle(Control* c) {
    if (!c) return;
    for (int i = 0; i < c->get_child_count(); i++) {
        Node* ch = c->get_child(i);
        if (ch && ch->has_method("_apply")) ch->call_deferred("_apply");
    }
}

// ── UICorner ─────────────────────────────────────────────────────────
//  CornerRadius es un UDim: la escala es relativa a la MITAD del lado corto
//  del elemento (asi un radio de 0.5 hace una pastilla perfecta).
class UICorner : public Node {
    GDCLASS(UICorner, Node);
    float r_scale = 0.0f;
    int   r_offset = 8;

    void _apply() {
        Control* p = Object::cast_to<Control>(get_parent());
        if (!p) return;
        const Vector2 s = p->get_size();
        const float half = MIN(s.x, s.y) * 0.5f;
        int radius = (int)(r_scale * half) + r_offset;
        if (radius < 0) radius = 0;
        static const char* keys[] = { "panel", "normal", "hover", "pressed", "focus" };
        for (int i = 0; i < 5; i++) {
            if (!p->has_theme_stylebox_override(keys[i])) continue;
            Ref<StyleBoxFlat> sb = p->get_theme_stylebox(keys[i]);
            if (sb.is_valid()) sb->set_corner_radius_all(radius);
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_CornerRadius", "r"), &UICorner::set_CornerRadius);
        ClassDB::bind_method(D_METHOD("get_CornerRadius"),      &UICorner::get_CornerRadius);
        ClassDB::bind_method(D_METHOD("set_gl_udim", "sc", "off"), &UICorner::set_gl_udim);
        ClassDB::bind_method(D_METHOD("_apply"),                &UICorner::_apply);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "CornerRadius"), "set_CornerRadius", "get_CornerRadius");
    }

public:
    void set_CornerRadius(int r) { r_scale = 0.0f; r_offset = r; call_deferred("_apply"); }
    int  get_CornerRadius() const { return r_offset; }
    // UDim completo (lo usa el importador y el puente Lua)
    void set_gl_udim(float sc, int off) { r_scale = sc; r_offset = off; call_deferred("_apply"); }
    void _ready() override { call_deferred("_apply"); }
    UICorner() { set_meta("_gl_bridge", true); }
};

// ── UIPadding ────────────────────────────────────────────────────────
//  SOLO datos: el inset lo aplica cada hijo al colocarse (gl_gui_apply_layout
//  llama a _gl_pad_px). Antes forzaba el tamaño de todos los hijos y por eso
//  cualquier panel con UIPadding salia con las UIs amontonadas.
class UIPadding : public Node {
    GDCLASS(UIPadding, Node);
    float sl = 0, st = 0, sr = 0, sb = 0;   // escala (relativa al padre)
    int   ol = 0, ot = 0, orr = 0, ob = 0;  // offset en px

    void _dirty() {
        Control* p = Object::cast_to<Control>(get_parent());
        if (p) gl_gui_relayout_children(p);
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_PaddingLeft","v"),   &UIPadding::set_PaddingLeft);
        ClassDB::bind_method(D_METHOD("get_PaddingLeft"),       &UIPadding::get_PaddingLeft);
        ClassDB::bind_method(D_METHOD("set_PaddingTop","v"),    &UIPadding::set_PaddingTop);
        ClassDB::bind_method(D_METHOD("get_PaddingTop"),        &UIPadding::get_PaddingTop);
        ClassDB::bind_method(D_METHOD("set_PaddingRight","v"),  &UIPadding::set_PaddingRight);
        ClassDB::bind_method(D_METHOD("get_PaddingRight"),      &UIPadding::get_PaddingRight);
        ClassDB::bind_method(D_METHOD("set_PaddingBottom","v"), &UIPadding::set_PaddingBottom);
        ClassDB::bind_method(D_METHOD("get_PaddingBottom"),     &UIPadding::get_PaddingBottom);
        ClassDB::bind_method(D_METHOD("set_gl_pad","side","sc","off"), &UIPadding::set_gl_pad);
        ClassDB::bind_method(D_METHOD("_gl_pad_px","parent_size"),     &UIPadding::_gl_pad_px);
        ADD_PROPERTY(PropertyInfo(Variant::INT,"PaddingLeft"),  "set_PaddingLeft",  "get_PaddingLeft");
        ADD_PROPERTY(PropertyInfo(Variant::INT,"PaddingTop"),   "set_PaddingTop",   "get_PaddingTop");
        ADD_PROPERTY(PropertyInfo(Variant::INT,"PaddingRight"), "set_PaddingRight", "get_PaddingRight");
        ADD_PROPERTY(PropertyInfo(Variant::INT,"PaddingBottom"),"set_PaddingBottom","get_PaddingBottom");
    }

public:
    void set_PaddingLeft(int v)   { ol = v;  sl = 0; _dirty(); }
    int  get_PaddingLeft() const  { return ol; }
    void set_PaddingTop(int v)    { ot = v;  st = 0; _dirty(); }
    int  get_PaddingTop() const   { return ot; }
    void set_PaddingRight(int v)  { orr = v; sr = 0; _dirty(); }
    int  get_PaddingRight() const { return orr; }
    void set_PaddingBottom(int v) { ob = v;  sb = 0; _dirty(); }
    int  get_PaddingBottom() const { return ob; }

    // side: 0=Left 1=Top 2=Right 3=Bottom. UDim completo (escala + offset).
    void set_gl_pad(int side, float sc, int off) {
        switch (side) {
            case 0: sl = sc; ol = off; break;
            case 1: st = sc; ot = off; break;
            case 2: sr = sc; orr = off; break;
            default: sb = sc; ob = off; break;
        }
        _dirty();
    }

    Vector4 _gl_pad_px(Vector2 psz) const {
        return Vector4(sl * psz.x + (float)ol,  st * psz.y + (float)ot,
                       sr * psz.x + (float)orr, sb * psz.y + (float)ob);
    }
    void _ready() override { _dirty(); }
    UIPadding() { set_meta("_gl_bridge", true); }
};

// ── UIAspectRatioConstraint ──────────────────────────────────────────
//  SOLO datos. Lo aplica el GuiObject al colocarse.
//  AspectType: FitWithinMaxSize=0, ScaleWithParentSize=1
//  DominantAxis: Width=0, Height=1
class UIAspectRatioConstraint : public Node {
    GDCLASS(UIAspectRatioConstraint, Node);
    double ratio = 1.0;
    int aspect_type = 0;
    int dominant = 1;   // Height por defecto, como Roblox

    void _dirty() {
        Control* p = Object::cast_to<Control>(get_parent());
        if (p && p->has_method("_on_parent_resized")) p->call_deferred("_on_parent_resized");
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_AspectRatio", "r"), &UIAspectRatioConstraint::set_AspectRatio);
        ClassDB::bind_method(D_METHOD("get_AspectRatio"),      &UIAspectRatioConstraint::get_AspectRatio);
        ClassDB::bind_method(D_METHOD("set_AspectType", "v"),  &UIAspectRatioConstraint::set_AspectType);
        ClassDB::bind_method(D_METHOD("get_AspectType"),       &UIAspectRatioConstraint::get_AspectType);
        ClassDB::bind_method(D_METHOD("set_DominantAxis", "v"),&UIAspectRatioConstraint::set_DominantAxis);
        ClassDB::bind_method(D_METHOD("get_DominantAxis"),     &UIAspectRatioConstraint::get_DominantAxis);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "AspectRatio"),  "set_AspectRatio",  "get_AspectRatio");
        ADD_PROPERTY(PropertyInfo(Variant::INT,   "AspectType"),   "set_AspectType",   "get_AspectType");
        ADD_PROPERTY(PropertyInfo(Variant::INT,   "DominantAxis"), "set_DominantAxis", "get_DominantAxis");
    }

public:
    void   set_AspectRatio(double r) { ratio = r > 0.0001 ? r : 1.0; _dirty(); }
    double get_AspectRatio() const { return ratio; }
    void   set_AspectType(int v) { aspect_type = v; _dirty(); }
    int    get_AspectType() const { return aspect_type; }
    void   set_DominantAxis(int v) { dominant = v; _dirty(); }
    int    get_DominantAxis() const { return dominant; }
    void _ready() override { _dirty(); }
    UIAspectRatioConstraint() { set_meta("_gl_bridge", true); }
};

// ── UISizeConstraint ─────────────────────────────────────────────────
class UISizeConstraint : public Node {
    GDCLASS(UISizeConstraint, Node);
    Vector2 min_size = Vector2(0, 0), max_size = Vector2(1e7f, 1e7f);
    void _dirty() {
        Control* p = Object::cast_to<Control>(get_parent());
        if (p && p->has_method("_on_parent_resized")) p->call_deferred("_on_parent_resized");
    }
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_MinSize","v"), &UISizeConstraint::set_MinSize);
        ClassDB::bind_method(D_METHOD("get_MinSize"),     &UISizeConstraint::get_MinSize);
        ClassDB::bind_method(D_METHOD("set_MaxSize","v"), &UISizeConstraint::set_MaxSize);
        ClassDB::bind_method(D_METHOD("get_MaxSize"),     &UISizeConstraint::get_MaxSize);
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR2,"MinSize"),"set_MinSize","get_MinSize");
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR2,"MaxSize"),"set_MaxSize","get_MaxSize");
    }
public:
    void set_MinSize(Vector2 v) { min_size = v; _dirty(); }
    Vector2 get_MinSize() const { return min_size; }
    void set_MaxSize(Vector2 v) { max_size = v; _dirty(); }
    Vector2 get_MaxSize() const { return max_size; }
    void _ready() override { _dirty(); }
    UISizeConstraint() { set_meta("_gl_bridge", true); }
};

// ── UITextSizeConstraint ─────────────────────────────────────────────
//  Limita el tamaño de letra que puede elegir TextScaled.
class UITextSizeConstraint : public Node {
    GDCLASS(UITextSizeConstraint, Node);
    int min_text = 1, max_text = 100;
    void _dirty() {
        Control* p = Object::cast_to<Control>(get_parent());
        if (p && p->has_method("_gl_refit_text")) p->call_deferred("_gl_refit_text");
    }
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_MinTextSize","v"), &UITextSizeConstraint::set_MinTextSize);
        ClassDB::bind_method(D_METHOD("get_MinTextSize"),     &UITextSizeConstraint::get_MinTextSize);
        ClassDB::bind_method(D_METHOD("set_MaxTextSize","v"), &UITextSizeConstraint::set_MaxTextSize);
        ClassDB::bind_method(D_METHOD("get_MaxTextSize"),     &UITextSizeConstraint::get_MaxTextSize);
        ADD_PROPERTY(PropertyInfo(Variant::INT,"MinTextSize"),"set_MinTextSize","get_MinTextSize");
        ADD_PROPERTY(PropertyInfo(Variant::INT,"MaxTextSize"),"set_MaxTextSize","get_MaxTextSize");
    }
public:
    void set_MinTextSize(int v) { min_text = v; _dirty(); }
    int  get_MinTextSize() const { return min_text; }
    void set_MaxTextSize(int v) { max_text = v; _dirty(); }
    int  get_MaxTextSize() const { return max_text; }
    void _ready() override { _dirty(); }
    UITextSizeConstraint() { set_meta("_gl_bridge", true); }
};

// ── UIScale ──────────────────────────────────────────────────────────
class UIScale : public Node {
    GDCLASS(UIScale, Node);
    double scale = 1.0;
    void _apply() {
        Control* p = Object::cast_to<Control>(get_parent());
        if (p) p->set_scale(Vector2((float)scale, (float)scale));
    }
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_Scale", "s"), &UIScale::set_Scale);
        ClassDB::bind_method(D_METHOD("get_Scale"),      &UIScale::get_Scale);
        ClassDB::bind_method(D_METHOD("_apply"),         &UIScale::_apply);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Scale"), "set_Scale", "get_Scale");
    }
public:
    void   set_Scale(double s) { scale = s; call_deferred("_apply"); }
    double get_Scale() const { return scale; }
    void _ready() override { call_deferred("_apply"); }
    UIScale() { set_meta("_gl_bridge", true); }
};

// ── UIFlexItem ───────────────────────────────────────────────────────
//  Datos para el reparto de espacio de UIListLayout (HorizontalFlex/VerticalFlex).
//  FlexMode: None=0, Grow=1, Shrink=2, Fill=3, Custom=4
class UIFlexItem : public Node {
    GDCLASS(UIFlexItem, Node);
    int flex_mode = 0, item_align = 0;
    double grow = 0.0, shrink = 0.0;
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_FlexMode","v"),  &UIFlexItem::set_FlexMode);
        ClassDB::bind_method(D_METHOD("get_FlexMode"),      &UIFlexItem::get_FlexMode);
        ClassDB::bind_method(D_METHOD("set_GrowRatio","v"), &UIFlexItem::set_GrowRatio);
        ClassDB::bind_method(D_METHOD("get_GrowRatio"),     &UIFlexItem::get_GrowRatio);
        ClassDB::bind_method(D_METHOD("set_ShrinkRatio","v"), &UIFlexItem::set_ShrinkRatio);
        ClassDB::bind_method(D_METHOD("get_ShrinkRatio"),     &UIFlexItem::get_ShrinkRatio);
        ClassDB::bind_method(D_METHOD("set_ItemLineAlignment","v"), &UIFlexItem::set_ItemLineAlignment);
        ClassDB::bind_method(D_METHOD("get_ItemLineAlignment"),     &UIFlexItem::get_ItemLineAlignment);
        ADD_PROPERTY(PropertyInfo(Variant::INT,  "FlexMode"),          "set_FlexMode","get_FlexMode");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"GrowRatio"),         "set_GrowRatio","get_GrowRatio");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"ShrinkRatio"),       "set_ShrinkRatio","get_ShrinkRatio");
        ADD_PROPERTY(PropertyInfo(Variant::INT,  "ItemLineAlignment"), "set_ItemLineAlignment","get_ItemLineAlignment");
    }
public:
    void set_FlexMode(int v) { flex_mode = v; }
    int  get_FlexMode() const { return flex_mode; }
    void set_GrowRatio(double v) { grow = v; }
    double get_GrowRatio() const { return grow; }
    void set_ShrinkRatio(double v) { shrink = v; }
    double get_ShrinkRatio() const { return shrink; }
    void set_ItemLineAlignment(int v) { item_align = v; }
    int  get_ItemLineAlignment() const { return item_align; }
    UIFlexItem() { set_meta("_gl_bridge", true); }
};

// ── UIStroke ─────────────────────────────────────────────────────────
//  En Roblox, un UIStroke sobre un TextLabel/TextButton contornea el TEXTO
//  (ApplyStrokeMode.Contextual); sobre un Frame contornea el borde. Antes
//  siempre hacia borde, asi que los textos del juego perdian su contorno y se
//  volvian ilegibles sobre fondos claros.
//  ApplyStrokeMode: Contextual=0, Border=1
class UIStroke : public Node {
    GDCLASS(UIStroke, Node);
    double thickness = 1.0;
    Color  color = Color(0, 0, 0, 1);
    double transparency = 0.0;
    bool   enabled = true;
    int    apply_mode = 0;
    int    line_join = 2;   // Round

    void _apply() {
        Control* p = Object::cast_to<Control>(get_parent());
        if (!p) return;
        const float a = 1.0f - (float)Math::clamp(transparency, 0.0, 1.0);
        const Color col(color.r, color.g, color.b, enabled ? a : 0.0f);
        const bool is_text = (Object::cast_to<Label>(p) != nullptr) ||
                             (Object::cast_to<Button>(p) != nullptr);
        if (is_text && apply_mode == 0) {
            const int px = enabled ? MAX(1, (int)Math::round(thickness * 2.0)) : 0;
            p->add_theme_constant_override("outline_size", px);
            p->add_theme_constant_override("shadow_outline_size", px);
            p->add_theme_color_override("font_outline_color", col);
            return;
        }
        static const char* keys[] = { "panel", "normal", "hover", "pressed", "focus" };
        for (int i = 0; i < 5; i++) {
            if (!p->has_theme_stylebox_override(keys[i])) continue;
            Ref<StyleBoxFlat> sb = p->get_theme_stylebox(keys[i]);
            if (sb.is_valid()) {
                sb->set_border_width_all(enabled ? MAX(1, (int)Math::round(thickness)) : 0);
                sb->set_border_color(col);
            }
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_Thickness", "t"),    &UIStroke::set_Thickness);
        ClassDB::bind_method(D_METHOD("get_Thickness"),         &UIStroke::get_Thickness);
        ClassDB::bind_method(D_METHOD("set_Color", "c"),        &UIStroke::set_Color);
        ClassDB::bind_method(D_METHOD("get_Color"),             &UIStroke::get_Color);
        ClassDB::bind_method(D_METHOD("set_Transparency", "v"), &UIStroke::set_Transparency);
        ClassDB::bind_method(D_METHOD("get_Transparency"),      &UIStroke::get_Transparency);
        ClassDB::bind_method(D_METHOD("set_Enabled", "v"),      &UIStroke::set_Enabled);
        ClassDB::bind_method(D_METHOD("get_Enabled"),           &UIStroke::get_Enabled);
        ClassDB::bind_method(D_METHOD("set_ApplyStrokeMode","v"), &UIStroke::set_ApplyStrokeMode);
        ClassDB::bind_method(D_METHOD("get_ApplyStrokeMode"),     &UIStroke::get_ApplyStrokeMode);
        ClassDB::bind_method(D_METHOD("set_LineJoinMode","v"),   &UIStroke::set_LineJoinMode);
        ClassDB::bind_method(D_METHOD("get_LineJoinMode"),       &UIStroke::get_LineJoinMode);
        ClassDB::bind_method(D_METHOD("_apply"),                 &UIStroke::_apply);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Thickness"),      "set_Thickness", "get_Thickness");
        ADD_PROPERTY(PropertyInfo(Variant::COLOR, "Color"),          "set_Color",     "get_Color");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Transparency"),   "set_Transparency","get_Transparency");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "Enabled"),        "set_Enabled",   "get_Enabled");
        ADD_PROPERTY(PropertyInfo(Variant::INT,   "ApplyStrokeMode"),"set_ApplyStrokeMode","get_ApplyStrokeMode");
        ADD_PROPERTY(PropertyInfo(Variant::INT,   "LineJoinMode"),   "set_LineJoinMode","get_LineJoinMode");
    }

public:
    void   set_Thickness(double t) { thickness = t; call_deferred("_apply"); }
    double get_Thickness() const { return thickness; }
    void   set_Color(Color c) { color = c; call_deferred("_apply"); }
    Color  get_Color() const { return color; }
    void   set_Transparency(double v) { transparency = v; call_deferred("_apply"); }
    double get_Transparency() const { return transparency; }
    void   set_Enabled(bool v) { enabled = v; call_deferred("_apply"); }
    bool   get_Enabled() const { return enabled; }
    void   set_ApplyStrokeMode(int v) { apply_mode = v; call_deferred("_apply"); }
    int    get_ApplyStrokeMode() const { return apply_mode; }
    void   set_LineJoinMode(int v) { line_join = v; }
    int    get_LineJoinMode() const { return line_join; }
    void _ready() override { call_deferred("_apply"); }
    UIStroke() { set_meta("_gl_bridge", true); }
};

// ── UIGradient ───────────────────────────────────────────────────────
//  Pinta el fondo del GuiObject padre con el ColorSequence completo (todos
//  los keypoints, no solo el primero y el ultimo), su Offset, su Rotation y
//  la Transparency (NumberSequence -> alfa). Limitacion conocida: usa
//  StyleBoxTexture, que no redondea esquinas, asi que si el mismo elemento
//  tiene UICorner el gradiente gana y las esquinas salen rectas.
class UIGradient : public Node {
    GDCLASS(UIGradient, Node);
    PackedFloat32Array stops;    // 0..1
    PackedColorArray   cols;
    Vector2 offset;
    double  rotation = 0.0;
    bool    enabled  = true;
    double  scale    = 1.0;

    void _defaults_if_empty() {
        if (cols.size() >= 2) return;
        stops.clear(); cols.clear();
        stops.push_back(0.0f); cols.push_back(Color(1, 1, 1));
        stops.push_back(1.0f); cols.push_back(Color(0.6f, 0.6f, 0.6f));
    }

    // El importador deja el ColorSequence como meta "Color": array de
    // {Color, Time, Envelope}. Aqui se pasa a paradas reales del gradiente.
    void _adopt_imported_color() {
        if (!has_meta("Color")) return;
        Variant v = get_meta("Color");
        if (v.get_type() != Variant::ARRAY) return;
        Array kps = v;
        PackedFloat32Array ns; PackedColorArray nc;
        for (int i = 0; i < kps.size(); i++) {
            if (kps[i].get_type() != Variant::DICTIONARY) continue;
            Dictionary d = kps[i];
            if (!d.has("Color")) continue;
            Variant c = d["Color"];
            if (c.get_type() != Variant::COLOR) continue;
            float t = d.has("Time") ? (float)d["Time"] : (kps.size() > 1 ? (float)i / (kps.size() - 1) : 0.0f);
            ns.push_back(Math::clamp(t, 0.0f, 1.0f));
            nc.push_back(c);
        }
        if (nc.size() >= 2) { stops = ns; cols = nc; }
        else if (nc.size() == 1) {
            stops.clear(); cols.clear();
            stops.push_back(0.0f); cols.push_back(nc[0]);
            stops.push_back(1.0f); cols.push_back(nc[0]);
        }
    }

    void _apply() {
        Control* p = Object::cast_to<Control>(get_parent());
        if (!p || !enabled) return;
        _defaults_if_empty();
        if (stops.size() != cols.size() || cols.size() < 2) return;
        Ref<Gradient> grad; grad.instantiate();
        // OJO: Gradient::set_offsets y set_colors FALLAN si el tamaño no coincide
        // con el numero de puntos que ya tiene (nace con 2). Si fallan, el
        // GradientTexture2D genera una imagen VACIA y el hilo de render de Godot
        // se cae al usarla. Hay que ajustar primero el numero de puntos.
        while (grad->get_point_count() > (int)stops.size())
            grad->remove_point(grad->get_point_count() - 1);
        while (grad->get_point_count() < (int)stops.size())
            grad->add_point(1.0f, Color(1, 1, 1));
        // Y los offsets tienen que ir ordenados y dentro de 0..1.
        PackedFloat32Array so = stops;
        for (int i = 0; i < so.size(); i++)
            so.set(i, Math::clamp(so[i], 0.0f, 1.0f));
        for (int i = 1; i < so.size(); i++)
            if (so[i] < so[i - 1]) so.set(i, so[i - 1]);
        grad->set_offsets(so);
        grad->set_colors(cols);
        Ref<GradientTexture2D> tex; tex.instantiate();
        tex->set_gradient(grad);
        tex->set_width(128); tex->set_height(128);
        const float rad = (float)Math::deg_to_rad(rotation);
        const float c = Math::cos(rad), s = Math::sin(rad);
        const float half = 0.5f * (float)CLAMP(scale, 0.05, 8.0);
        const Vector2 mid(0.5f + Math::clamp(offset.x, -4.0f, 4.0f),
                          0.5f + Math::clamp(offset.y, -4.0f, 4.0f));
        Vector2 from(mid.x - half * c, mid.y - half * s);
        Vector2 to(  mid.x + half * c, mid.y + half * s);
        if (from.distance_to(to) < 0.01f) { from = Vector2(0, 0.5f); to = Vector2(1, 0.5f); }
        tex->set_fill_from(from);
        tex->set_fill_to(to);
        Ref<StyleBoxTexture> sb; sb.instantiate();
        sb->set_texture(tex);
        const char* slot = p->has_theme_stylebox_override("panel") ? "panel" : "normal";
        p->add_theme_stylebox_override(slot, sb);
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_Rotation", "r"),     &UIGradient::set_Rotation);
        ClassDB::bind_method(D_METHOD("get_Rotation"),          &UIGradient::get_Rotation);
        ClassDB::bind_method(D_METHOD("set_Enabled", "e"),      &UIGradient::set_Enabled);
        ClassDB::bind_method(D_METHOD("get_Enabled"),           &UIGradient::get_Enabled);
        ClassDB::bind_method(D_METHOD("set_Offset", "v"),       &UIGradient::set_Offset);
        ClassDB::bind_method(D_METHOD("get_Offset"),            &UIGradient::get_Offset);
        ClassDB::bind_method(D_METHOD("set_Scale", "v"),        &UIGradient::set_Scale);
        ClassDB::bind_method(D_METHOD("get_Scale"),             &UIGradient::get_Scale);
        ClassDB::bind_method(D_METHOD("set_gl_colors", "a", "b"),      &UIGradient::set_gl_colors);
        ClassDB::bind_method(D_METHOD("set_gl_stops", "times", "colors"), &UIGradient::set_gl_stops);
        ClassDB::bind_method(D_METHOD("get_gl_color0"),         &UIGradient::get_gl_color0);
        ClassDB::bind_method(D_METHOD("get_gl_color1"),         &UIGradient::get_gl_color1);
        ClassDB::bind_method(D_METHOD("_apply"),                &UIGradient::_apply);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "Rotation"), "set_Rotation", "get_Rotation");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,   "Enabled"),  "set_Enabled",  "get_Enabled");
        ADD_PROPERTY(PropertyInfo(Variant::VECTOR2,"Offset"),   "set_Offset",   "get_Offset");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "Scale"),    "set_Scale",    "get_Scale");
    }

public:
    void   set_Rotation(double r) { rotation = r; call_deferred("_apply"); }
    double get_Rotation() const { return rotation; }
    void   set_Enabled(bool e) { enabled = e; call_deferred("_apply"); }
    bool   get_Enabled() const { return enabled; }
    void   set_Offset(Vector2 v) { offset = v; call_deferred("_apply"); }
    Vector2 get_Offset() const { return offset; }
    void   set_Scale(double v) { scale = v; call_deferred("_apply"); }
    double get_Scale() const { return scale; }

    void set_gl_colors(Color a, Color b) {
        stops.clear(); cols.clear();
        stops.push_back(0.0f); cols.push_back(a);
        stops.push_back(1.0f); cols.push_back(b);
        call_deferred("_apply");
    }
    void set_gl_stops(PackedFloat32Array times, PackedColorArray colors) {
        if (times.size() != colors.size() || colors.size() < 2) return;
        stops = times; cols = colors;
        call_deferred("_apply");
    }
    Color get_gl_color0() const { return cols.size() ? cols[0] : Color(1, 1, 1); }
    Color get_gl_color1() const { return cols.size() ? cols[cols.size() - 1] : Color(1, 1, 1); }
    void _ready() override { _adopt_imported_color(); call_deferred("_apply"); }
    UIGradient() { set_meta("_gl_bridge", true); }
};

// ── UIListLayout ─────────────────────────────────────────────────────
//  Completo: FillDirection, Padding (UDim), H/V Alignment, SortOrder
//  (LayoutOrder por defecto, que es lo que el juego usa para ordenar sus
//  botones), Wraps, ItemLineAlignment y HorizontalFlex/VerticalFlex.
//  Respeta el UIPadding del mismo contenedor y solo recoloca cuando algo
//  cambia (antes recolocaba 44 listas por frame).
class UIListLayout : public Node {
    GDCLASS(UIListLayout, Node);
    int   fill_dir = 1;         // Horizontal=0, Vertical=1
    float pad_scale = 0.0f;
    int   pad_offset = 0;
    int   h_align = 1;          // Center=0, Left=1, Right=2
    int   v_align = 1;          // Center=0, Top=1,  Bottom=2
    int   sort_order = GL_SORT_LAYOUT;
    bool  wraps = false;
    int   item_line_align = 0;
    int   h_flex = 0, v_flex = 0;   // Enum.UIFlexAlignment: None=0, Fill=1, SpaceAround=2, SpaceBetween=3, SpaceEvenly=4
    uint64_t stamp = 0;
    Vector2 content_size;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_FillDirection", "d"), &UIListLayout::set_FillDirection);
        ClassDB::bind_method(D_METHOD("get_FillDirection"),      &UIListLayout::get_FillDirection);
        ClassDB::bind_method(D_METHOD("set_Padding", "p"),       &UIListLayout::set_Padding);
        ClassDB::bind_method(D_METHOD("get_Padding"),            &UIListLayout::get_Padding);
        ClassDB::bind_method(D_METHOD("set_gl_padding_udim","sc","off"), &UIListLayout::set_gl_padding_udim);
        ClassDB::bind_method(D_METHOD("set_HorizontalAlignment", "a"), &UIListLayout::set_HorizontalAlignment);
        ClassDB::bind_method(D_METHOD("get_HorizontalAlignment"),      &UIListLayout::get_HorizontalAlignment);
        ClassDB::bind_method(D_METHOD("set_VerticalAlignment", "a"),   &UIListLayout::set_VerticalAlignment);
        ClassDB::bind_method(D_METHOD("get_VerticalAlignment"),        &UIListLayout::get_VerticalAlignment);
        ClassDB::bind_method(D_METHOD("set_SortOrder", "v"),     &UIListLayout::set_SortOrder);
        ClassDB::bind_method(D_METHOD("_gl_invalidate"),          &UIListLayout::_gl_invalidate);
        ClassDB::bind_method(D_METHOD("get_SortOrder"),          &UIListLayout::get_SortOrder);
        ClassDB::bind_method(D_METHOD("set_Wraps", "v"),         &UIListLayout::set_Wraps);
        ClassDB::bind_method(D_METHOD("get_Wraps"),              &UIListLayout::get_Wraps);
        ClassDB::bind_method(D_METHOD("set_ItemLineAlignment","v"), &UIListLayout::set_ItemLineAlignment);
        ClassDB::bind_method(D_METHOD("get_ItemLineAlignment"),     &UIListLayout::get_ItemLineAlignment);
        ClassDB::bind_method(D_METHOD("set_HorizontalFlex","v"), &UIListLayout::set_HorizontalFlex);
        ClassDB::bind_method(D_METHOD("get_HorizontalFlex"),     &UIListLayout::get_HorizontalFlex);
        ClassDB::bind_method(D_METHOD("set_VerticalFlex","v"),   &UIListLayout::set_VerticalFlex);
        ClassDB::bind_method(D_METHOD("get_VerticalFlex"),       &UIListLayout::get_VerticalFlex);
        ClassDB::bind_method(D_METHOD("get_AbsoluteContentSize"), &UIListLayout::get_AbsoluteContentSize);
        ADD_PROPERTY(PropertyInfo(Variant::INT,  "FillDirection"),       "set_FillDirection",       "get_FillDirection");
        ADD_PROPERTY(PropertyInfo(Variant::INT,  "Padding"),             "set_Padding",             "get_Padding");
        ADD_PROPERTY(PropertyInfo(Variant::INT,  "HorizontalAlignment"), "set_HorizontalAlignment", "get_HorizontalAlignment");
        ADD_PROPERTY(PropertyInfo(Variant::INT,  "VerticalAlignment"),   "set_VerticalAlignment",   "get_VerticalAlignment");
        ADD_PROPERTY(PropertyInfo(Variant::INT,  "SortOrder"),           "set_SortOrder",           "get_SortOrder");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "Wraps"),               "set_Wraps",               "get_Wraps");
        ADD_PROPERTY(PropertyInfo(Variant::INT,  "ItemLineAlignment"),   "set_ItemLineAlignment",   "get_ItemLineAlignment");
        ADD_PROPERTY(PropertyInfo(Variant::INT,  "HorizontalFlex"),      "set_HorizontalFlex",      "get_HorizontalFlex");
        ADD_PROPERTY(PropertyInfo(Variant::INT,  "VerticalFlex"),        "set_VerticalFlex",        "get_VerticalFlex");
    }

public:
    void set_FillDirection(int d) { fill_dir = d; stamp = 0; }
    int  get_FillDirection() const { return fill_dir; }
    void set_Padding(int p) { pad_offset = p; pad_scale = 0.0f; stamp = 0; }
    int  get_Padding() const { return pad_offset; }
    void set_gl_padding_udim(float sc, int off) { pad_scale = sc; pad_offset = off; stamp = 0; }
    void set_HorizontalAlignment(int a) { h_align = a; stamp = 0; }
    int  get_HorizontalAlignment() const { return h_align; }
    void set_VerticalAlignment(int a) { v_align = a; stamp = 0; }
    int  get_VerticalAlignment() const { return v_align; }
    void set_SortOrder(int v) { sort_order = v; stamp = 0; }
    int  get_SortOrder() const { return sort_order; }
    void _gl_invalidate() { stamp = 0; }
    void set_Wraps(bool v) { wraps = v; stamp = 0; }
    bool get_Wraps() const { return wraps; }
    void set_ItemLineAlignment(int v) { item_line_align = v; stamp = 0; }
    int  get_ItemLineAlignment() const { return item_line_align; }
    void set_HorizontalFlex(int v) { h_flex = v; stamp = 0; }
    int  get_HorizontalFlex() const { return h_flex; }
    void set_VerticalFlex(int v) { v_flex = v; stamp = 0; }
    int  get_VerticalFlex() const { return v_flex; }
    Vector2 get_AbsoluteContentSize() const { return content_size; }

    void _process(double) override {
        Control* p = Object::cast_to<Control>(get_parent());
        if (!p) return;
        const uint64_t now = gl_gui_children_stamp(p);
        if (now == stamp) return;
        stamp = now;

        const Vector4 pad = gl_gui_padding_px(p);
        const Vector2 psz = p->get_size();
        const Vector2 area(MAX(0.0f, psz.x - (float)pad.x - (float)pad.z),
                           MAX(0.0f, psz.y - (float)pad.y - (float)pad.w));
        const float ox = (float)pad.x, oy = (float)pad.y;
        const bool vert = (fill_dir != 0);
        const float gap = pad_scale * (vert ? psz.y : psz.x) + (float)pad_offset;

        LocalVector<Control*> kids;
        gl_gui_sorted_children(p, sort_order, kids);
        if (kids.is_empty()) { content_size = Vector2(); return; }

        // Reparto por lineas (Wraps). Sin Wraps es una sola linea.
        const float line_limit = vert ? area.y : area.x;
        LocalVector<int> line_start; line_start.push_back(0);
        if (wraps) {
            float run = 0.0f;
            for (uint32_t i = 0; i < kids.size(); i++) {
                const float len = vert ? kids[i]->get_size().y : kids[i]->get_size().x;
                if (run > 0.0f && run + gap + len > line_limit) {
                    line_start.push_back((int)i);
                    run = len;
                } else {
                    run += (run > 0.0f ? gap : 0.0f) + len;
                }
            }
        }
        line_start.push_back((int)kids.size());

        float cross_cursor = 0.0f;
        float total_main = 0.0f, total_cross = 0.0f;
        for (uint32_t L = 0; L + 1 < line_start.size(); L++) {
            const int a = line_start[L], b = line_start[L + 1];
            float main_len = 0.0f, cross_max = 0.0f;
            for (int i = a; i < b; i++) {
                const Vector2 s = kids[i]->get_size();
                main_len += (vert ? s.y : s.x);
                cross_max = MAX(cross_max, vert ? s.x : s.y);
            }
            const int n = b - a;
            if (n > 1) main_len += gap * (n - 1);

            // Flex: reparte el hueco sobrante entre los elementos.
            const int flex = vert ? v_flex : h_flex;
            float extra_lead = 0.0f, extra_gap = 0.0f;
            const float slack = line_limit - main_len;
            if (flex != 0 && slack > 0.0f && n > 0) {
                if (flex == 3 && n > 1)      extra_gap = slack / (n - 1);          // SpaceBetween
                else if (flex == 2)          { extra_gap = slack / n; extra_lead = extra_gap * 0.5f; } // SpaceAround
                else if (flex == 4)          { extra_gap = slack / (n + 1); extra_lead = extra_gap; }  // SpaceEvenly
            }

            // Alineacion del bloque en el eje principal.
            float start = extra_lead;
            if (extra_gap == 0.0f) {
                const int main_align = vert ? v_align : h_align;
                start = gl_gui_cross(main_align, line_limit, main_len);
            }

            float cursor = start;
            for (int i = a; i < b; i++) {
                Control* c = kids[i];
                const Vector2 s = c->get_size();
                const float item_cross = vert ? s.x : s.y;
                // ItemLineAlignment: 0=Automatic (usa la del layout), 1=Start,
                // 2=Center, 3=End, 4=Stretch
                int cross_align = vert ? h_align : v_align;
                int ila = item_line_align;
                if (Node* fi = c->get_node_or_null(NodePath("UIFlexItem")))
                    if (fi->is_class("UIFlexItem")) ila = (int)(int64_t)fi->get(StringName("ItemLineAlignment"));
                if (ila == 1) cross_align = vert ? 1 : 1;          // Start = Left/Top
                else if (ila == 2) cross_align = 0;                 // Center
                else if (ila == 3) cross_align = 2;                 // End
                const float cross_off = cross_cursor + gl_gui_cross(cross_align, wraps ? cross_max : (vert ? area.x : area.y), item_cross);

                if (vert) c->set_position(Vector2(ox + cross_off, oy + cursor));
                else      c->set_position(Vector2(ox + cursor,    oy + cross_off));
                cursor += (vert ? s.y : s.x) + gap + extra_gap;
            }
            cross_cursor += cross_max + (wraps ? gap : 0.0f);
            total_main = MAX(total_main, main_len);
            total_cross += cross_max + (wraps ? gap : 0.0f);
        }
        content_size = vert ? Vector2(total_cross, total_main) : Vector2(total_main, total_cross);
    }

    UIListLayout() { set_meta("_gl_bridge", true); set_process(true); }
};

// ── UIGridLayout ─────────────────────────────────────────────────────
//  CellSize y CellPadding son UDim2 (relativos al contenedor), con
//  FillDirection, FillDirectionMaxCells, StartCorner, SortOrder y alineacion.
class UIGridLayout : public Node {
    GDCLASS(UIGridLayout, Node);
    GuiUDim2 cell = {0, 100, 0, 100};
    GuiUDim2 cpad = {0, 5, 0, 5};
    int fill_dir = 0;            // Horizontal=0, Vertical=1
    int max_cells = 0;           // 0 = sin limite
    int start_corner = 0;        // TopLeft=0, TopRight=1, BottomLeft=2, BottomRight=3
    int sort_order = GL_SORT_LAYOUT;
    int h_align = 1, v_align = 1;
    uint64_t stamp = 0;
    Vector2 content_size;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_CellSizeX", "v"),   &UIGridLayout::set_CellSizeX);
        ClassDB::bind_method(D_METHOD("get_CellSizeX"),        &UIGridLayout::get_CellSizeX);
        ClassDB::bind_method(D_METHOD("set_CellSizeY", "v"),   &UIGridLayout::set_CellSizeY);
        ClassDB::bind_method(D_METHOD("get_CellSizeY"),        &UIGridLayout::get_CellSizeY);
        ClassDB::bind_method(D_METHOD("set_CellPadding", "v"), &UIGridLayout::set_CellPadding);
        ClassDB::bind_method(D_METHOD("get_CellPadding"),      &UIGridLayout::get_CellPadding);
        ClassDB::bind_method(D_METHOD("set_gl_cell_size","xs","xo","ys","yo"),    &UIGridLayout::set_gl_cell_size);
        ClassDB::bind_method(D_METHOD("set_gl_cell_padding","xs","xo","ys","yo"), &UIGridLayout::set_gl_cell_padding);
        ClassDB::bind_method(D_METHOD("set_FillDirection","v"), &UIGridLayout::set_FillDirection);
        ClassDB::bind_method(D_METHOD("get_FillDirection"),     &UIGridLayout::get_FillDirection);
        ClassDB::bind_method(D_METHOD("set_FillDirectionMaxCells","v"), &UIGridLayout::set_FillDirectionMaxCells);
        ClassDB::bind_method(D_METHOD("get_FillDirectionMaxCells"),     &UIGridLayout::get_FillDirectionMaxCells);
        ClassDB::bind_method(D_METHOD("set_StartCorner","v"),   &UIGridLayout::set_StartCorner);
        ClassDB::bind_method(D_METHOD("get_StartCorner"),       &UIGridLayout::get_StartCorner);
        ClassDB::bind_method(D_METHOD("set_SortOrder","v"),     &UIGridLayout::set_SortOrder);
        ClassDB::bind_method(D_METHOD("_gl_invalidate"),         &UIGridLayout::_gl_invalidate);
        ClassDB::bind_method(D_METHOD("get_SortOrder"),         &UIGridLayout::get_SortOrder);
        ClassDB::bind_method(D_METHOD("set_HorizontalAlignment","v"), &UIGridLayout::set_HorizontalAlignment);
        ClassDB::bind_method(D_METHOD("get_HorizontalAlignment"),     &UIGridLayout::get_HorizontalAlignment);
        ClassDB::bind_method(D_METHOD("set_VerticalAlignment","v"),   &UIGridLayout::set_VerticalAlignment);
        ClassDB::bind_method(D_METHOD("get_VerticalAlignment"),       &UIGridLayout::get_VerticalAlignment);
        ClassDB::bind_method(D_METHOD("get_AbsoluteCellCount"),  &UIGridLayout::get_AbsoluteCellCount);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "CellSizeX"),   "set_CellSizeX",   "get_CellSizeX");
        ADD_PROPERTY(PropertyInfo(Variant::INT, "CellSizeY"),   "set_CellSizeY",   "get_CellSizeY");
        ADD_PROPERTY(PropertyInfo(Variant::INT, "CellPadding"), "set_CellPadding", "get_CellPadding");
        ADD_PROPERTY(PropertyInfo(Variant::INT, "FillDirection"),         "set_FillDirection","get_FillDirection");
        ADD_PROPERTY(PropertyInfo(Variant::INT, "FillDirectionMaxCells"), "set_FillDirectionMaxCells","get_FillDirectionMaxCells");
        ADD_PROPERTY(PropertyInfo(Variant::INT, "StartCorner"),           "set_StartCorner","get_StartCorner");
        ADD_PROPERTY(PropertyInfo(Variant::INT, "SortOrder"),             "set_SortOrder","get_SortOrder");
        ADD_PROPERTY(PropertyInfo(Variant::INT, "HorizontalAlignment"),   "set_HorizontalAlignment","get_HorizontalAlignment");
        ADD_PROPERTY(PropertyInfo(Variant::INT, "VerticalAlignment"),     "set_VerticalAlignment","get_VerticalAlignment");
    }

public:
    void set_CellSizeX(int v) { cell.xs = 0; cell.xo = (float)v; stamp = 0; }
    int  get_CellSizeX() const { return (int)cell.xo; }
    void set_CellSizeY(int v) { cell.ys = 0; cell.yo = (float)v; stamp = 0; }
    int  get_CellSizeY() const { return (int)cell.yo; }
    void set_CellPadding(int v) { cpad = {0, (float)v, 0, (float)v}; stamp = 0; }
    int  get_CellPadding() const { return (int)cpad.xo; }
    void set_gl_cell_size(float xs, float xo, float ys, float yo)    { cell = {xs, xo, ys, yo}; stamp = 0; }
    void set_gl_cell_padding(float xs, float xo, float ys, float yo) { cpad = {xs, xo, ys, yo}; stamp = 0; }
    void set_FillDirection(int v) { fill_dir = v; stamp = 0; }
    int  get_FillDirection() const { return fill_dir; }
    void set_FillDirectionMaxCells(int v) { max_cells = v; stamp = 0; }
    int  get_FillDirectionMaxCells() const { return max_cells; }
    void set_StartCorner(int v) { start_corner = v; stamp = 0; }
    int  get_StartCorner() const { return start_corner; }
    void set_SortOrder(int v) { sort_order = v; stamp = 0; }
    int  get_SortOrder() const { return sort_order; }
    void _gl_invalidate() { stamp = 0; }
    void set_HorizontalAlignment(int v) { h_align = v; stamp = 0; }
    int  get_HorizontalAlignment() const { return h_align; }
    void set_VerticalAlignment(int v) { v_align = v; stamp = 0; }
    int  get_VerticalAlignment() const { return v_align; }
    Vector2 get_AbsoluteCellCount() const { return content_size; }

    void _process(double) override {
        Control* p = Object::cast_to<Control>(get_parent());
        if (!p) return;
        const uint64_t now = gl_gui_children_stamp(p);
        if (now == stamp) return;
        stamp = now;

        const Vector4 pad = gl_gui_padding_px(p);
        const Vector2 psz = p->get_size();
        const Vector2 area(MAX(0.0f, psz.x - (float)pad.x - (float)pad.z),
                           MAX(0.0f, psz.y - (float)pad.y - (float)pad.w));
        const Vector2 cs(cell.xs * area.x + cell.xo, cell.ys * area.y + cell.yo);
        const Vector2 cp(cpad.xs * area.x + cpad.xo, cpad.ys * area.y + cpad.yo);

        LocalVector<Control*> kids;
        gl_gui_sorted_children(p, sort_order, kids);
        if (kids.is_empty()) { content_size = Vector2(); return; }

        // Celdas por linea en la direccion de llenado.
        int per_line;
        if (max_cells > 0) per_line = max_cells;
        else {
            const float span = (fill_dir == 0) ? area.x : area.y;
            const float step = ((fill_dir == 0) ? cs.x : cs.y) + ((fill_dir == 0) ? cp.x : cp.y);
            per_line = step > 0.01f ? (int)Math::floor((span + ((fill_dir == 0) ? cp.x : cp.y)) / step) : 1;
        }
        if (per_line < 1) per_line = 1;

        const int lines = ((int)kids.size() + per_line - 1) / per_line;
        const float grid_w = (fill_dir == 0)
            ? per_line * cs.x + (per_line - 1) * cp.x
            : lines * cs.x + (lines - 1) * cp.x;
        const float grid_h = (fill_dir == 0)
            ? lines * cs.y + (lines - 1) * cp.y
            : per_line * cs.y + (per_line - 1) * cp.y;
        const float base_x = (float)pad.x + gl_gui_cross(h_align, area.x, grid_w);
        const float base_y = (float)pad.y + gl_gui_cross(v_align, area.y, grid_h);

        for (uint32_t i = 0; i < kids.size(); i++) {
            int col, row;
            if (fill_dir == 0) { col = (int)i % per_line; row = (int)i / per_line; }
            else               { row = (int)i % per_line; col = (int)i / per_line; }
            // StartCorner: espeja la rejilla.
            const int ncols = (fill_dir == 0) ? per_line : lines;
            const int nrows = (fill_dir == 0) ? lines : per_line;
            if (start_corner == 1 || start_corner == 3) col = ncols - 1 - col;
            if (start_corner == 2 || start_corner == 3) row = nrows - 1 - row;
            kids[i]->set_position(Vector2(base_x + col * (cs.x + cp.x),
                                          base_y + row * (cs.y + cp.y)));
            kids[i]->set_size(cs);
        }
        content_size = Vector2((float)per_line, (float)lines);
    }

    UIGridLayout() { set_meta("_gl_bridge", true); set_process(true); }
};

// ── UIPageLayout ─────────────────────────────────────────────────────
//  Solo toca la visibilidad cuando el juego cambia de pagina de verdad; antes
//  la forzaba cada frame y hacia aparecer paneles que estaban guardados.
class UIPageLayout : public Node {
    GDCLASS(UIPageLayout, Node);
    int page = -1;
    int applied = -2;
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_CurrentPage","v"), &UIPageLayout::set_CurrentPage);
        ClassDB::bind_method(D_METHOD("get_CurrentPage"),     &UIPageLayout::get_CurrentPage);
        ADD_PROPERTY(PropertyInfo(Variant::INT,"CurrentPage"),"set_CurrentPage","get_CurrentPage");
    }
public:
    void set_CurrentPage(int v) { page = v; }
    int  get_CurrentPage() const { return page; }
    void _process(double) override {
        if (page < 0 || page == applied) return;
        Control* p = Object::cast_to<Control>(get_parent());
        if (!p) return;
        applied = page;
        int idx = 0;
        for (int i = 0; i < p->get_child_count(); i++) {
            Control* c = Object::cast_to<Control>(p->get_child(i));
            if (!c) continue;
            c->set_visible(idx == page);
            idx++;
        }
    }
    UIPageLayout() { set_meta("_gl_bridge", true); set_process(true); }
};

// ── UITableLayout ────────────────────────────────────────────────────
//  Filas = hijos del contenedor; columnas = hijos de cada fila.
class UITableLayout : public Node {
    GDCLASS(UITableLayout, Node);
    bool fill_empty = false;
    int  major_axis = 0;        // FillDirection: Rows=0, Columns=1
    int  sort_order = GL_SORT_LAYOUT;
    GuiUDim2 cpad = {0, 0, 0, 0};
    uint64_t stamp = 0;
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_FillEmptySpaceColumns","v"), &UITableLayout::set_FillEmptySpaceColumns);
        ClassDB::bind_method(D_METHOD("get_FillEmptySpaceColumns"),     &UITableLayout::get_FillEmptySpaceColumns);
        ClassDB::bind_method(D_METHOD("set_MajorAxis","v"), &UITableLayout::set_MajorAxis);
        ClassDB::bind_method(D_METHOD("get_MajorAxis"),     &UITableLayout::get_MajorAxis);
        ClassDB::bind_method(D_METHOD("set_SortOrder","v"), &UITableLayout::set_SortOrder);
        ClassDB::bind_method(D_METHOD("_gl_invalidate"),     &UITableLayout::_gl_invalidate);
        ClassDB::bind_method(D_METHOD("get_SortOrder"),     &UITableLayout::get_SortOrder);
        ClassDB::bind_method(D_METHOD("set_gl_cell_padding","xs","xo","ys","yo"), &UITableLayout::set_gl_cell_padding);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"FillEmptySpaceColumns"),"set_FillEmptySpaceColumns","get_FillEmptySpaceColumns");
        ADD_PROPERTY(PropertyInfo(Variant::INT, "MajorAxis"),"set_MajorAxis","get_MajorAxis");
        ADD_PROPERTY(PropertyInfo(Variant::INT, "SortOrder"),"set_SortOrder","get_SortOrder");
    }
public:
    void set_FillEmptySpaceColumns(bool v) { fill_empty = v; stamp = 0; }
    bool get_FillEmptySpaceColumns() const { return fill_empty; }
    void set_MajorAxis(int v) { major_axis = v; stamp = 0; }
    int  get_MajorAxis() const { return major_axis; }
    void set_SortOrder(int v) { sort_order = v; stamp = 0; }
    int  get_SortOrder() const { return sort_order; }
    void _gl_invalidate() { stamp = 0; }
    void set_gl_cell_padding(float xs, float xo, float ys, float yo) { cpad = {xs, xo, ys, yo}; stamp = 0; }

    void _process(double) override {
        Control* p = Object::cast_to<Control>(get_parent());
        if (!p) return;
        const uint64_t now = gl_gui_children_stamp(p);
        if (now == stamp) return;
        stamp = now;
        const Vector4 pad = gl_gui_padding_px(p);
        const Vector2 psz = p->get_size();
        const Vector2 cp(cpad.xs * psz.x + cpad.xo, cpad.ys * psz.y + cpad.yo);
        LocalVector<Control*> rows;
        gl_gui_sorted_children(p, sort_order, rows);
        float y = (float)pad.y;
        for (uint32_t r = 0; r < rows.size(); r++) {
            Control* row = rows[r];
            row->set_position(Vector2((float)pad.x, y));
            LocalVector<Control*> cells;
            gl_gui_sorted_children(row, sort_order, cells);
            float x = 0.0f, hmax = row->get_size().y;
            for (uint32_t c = 0; c < cells.size(); c++) {
                cells[c]->set_position(Vector2(x, 0));
                x += cells[c]->get_size().x + cp.x;
                hmax = MAX(hmax, cells[c]->get_size().y);
            }
            y += hmax + cp.y;
        }
    }
    UITableLayout() { set_meta("_gl_bridge", true); set_process(true); }
};

inline void gl_register_ui_modifier_classes() {
    ClassDB::register_class<UICorner>();
    ClassDB::register_class<UIPadding>();
    ClassDB::register_class<UIAspectRatioConstraint>();
    ClassDB::register_class<UISizeConstraint>();
    ClassDB::register_class<UITextSizeConstraint>();
    ClassDB::register_class<UIScale>();
    ClassDB::register_class<UIFlexItem>();
    ClassDB::register_class<UIStroke>();
    ClassDB::register_class<UIGradient>();
    ClassDB::register_class<UIListLayout>();
    ClassDB::register_class<UIGridLayout>();
    ClassDB::register_class<UIPageLayout>();
    ClassDB::register_class<UITableLayout>();
}

#endif // GL_UI_MODIFIERS_H
