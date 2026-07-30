#ifndef GL_GUI_CORE_H
#define GL_GUI_CORE_H

// ════════════════════════════════════════════════════════════════════
//  Semantica de layout de Roblox, en un solo sitio.
//
//  Lo usan TANTO las clases de GUI (roblox_gui.h) como los modificadores
//  de layout (UIListLayout / UIGridLayout / UIPadding en roblox_behavior.h),
//  que se compilan ANTES. Para no crear dependencias circulares, todo lo que
//  necesita hablar con otro nodo lo hace por NOMBRE de metodo/propiedad:
//
//    UIPadding          -> metodo  "_gl_pad_px"(Vector2 tam_padre) -> Vector4(l,t,r,b)
//    RobloxScrollingFrame -> metodo "_gl_scroll"() -> Vector2
//    cualquier GuiObject  -> propiedad "LayoutOrder" (int)
//
//  Reglas que se replican de Roblox y que antes no estaban:
//   · UDim2 de posicion/tamaño se mide contra el AREA DE CONTENIDO del padre,
//     que un UIPadding reduce por los 4 lados.
//   · SizeConstraint: RelativeXX/RelativeYY miden AMBOS ejes contra un solo
//     lado del padre (asi se hacen los cuadrados que escalan con el ancho).
//   · Rotation gira alrededor del CENTRO del elemento.
//   · SortOrder de los layouts: LayoutOrder (por defecto) o Name.
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector4.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/templates/hash_map.hpp>

using namespace godot;

struct GuiUDim2 { float xs = 0, xo = 0, ys = 0, yo = 0; };

// ── Puerta de dibujado (regla de Roblox) ─────────────────────────────
//  Un GuiObject se dibuja SOLO si cuelga de un ScreenGui (pantalla) o de un
//  SurfaceGui / BillboardGui (mundo 3D). Un Frame guardado en ReplicatedStorage
//  o en una carpeta suelta NO se ve en ninguna parte hasta que el juego lo
//  coloca donde toca.
//
//  Antes no habia esa regla y se dibujaba TODO: un place normal guarda docenas
//  de interfaces como plantilla (tiendas, popups, barras de arma, paneles de
//  mejora) y salian todas encima del HUD a la vez, sin que nada las hubiera
//  abierto. Se resuelve con visibility_layer + canvas_cull_mask del viewport,
//  asi que la propiedad Visible NO se toca y los scripts siguen leyendo la
//  verdad; solo deja de pintarse.
static const uint32_t GL_WORLD_GUI_CANVAS_LAYER = 1u << 19;   // "no en pantalla"

inline void gl_gui_cull_world_layer(Node* owner) {
    if (!owner) return;
    Viewport* vp = owner->get_viewport();
    if (!vp) return;
    const uint32_t mask = vp->get_canvas_cull_mask();
    if (mask & GL_WORLD_GUI_CANVAS_LAYER)
        vp->set_canvas_cull_mask(mask & ~GL_WORLD_GUI_CANVAS_LAYER);
}

inline void gl_gui_apply_render_gate(Control* c) {
    if (!c) return;
    if (Engine::get_singleton()->is_editor_hint()) return;   // en el editor se ve todo
    gl_gui_cull_world_layer(c);
    uint32_t layer = GL_WORLD_GUI_CANVAS_LAYER;              // por defecto: no se pinta
    for (Node* p = c->get_parent(); p; p = p->get_parent()) {
        if (p->is_class("ScreenGui")) { layer = 1u; break; }
        if (p->is_class("SurfaceGui") || p->is_class("BillboardGui")) break;  // UI de mundo
        if (Object::cast_to<Viewport>(p)) break;
    }
    c->set_visibility_layer(layer);
}

// Enum.SizeConstraint
enum GLSizeConstraint { GL_SC_RELATIVE_XY = 0, GL_SC_RELATIVE_XX = 1, GL_SC_RELATIVE_YY = 2 };
// Enum.AutomaticSize
enum GLAutomaticSize { GL_AUTO_NONE = 0, GL_AUTO_X = 1, GL_AUTO_Y = 2, GL_AUTO_XY = 3 };
// Enum.SortOrder
enum GLSortOrder { GL_SORT_NAME = 0, GL_SORT_CUSTOM = 1, GL_SORT_LAYOUT = 2 };

// ── Padding del padre (px) ───────────────────────────────────────────
// Roblox: un UIPadding hijo del padre reduce el area donde se colocan los
// hermanos. Se busca por clase para no depender del tipo en tiempo de
// compilacion (UIPadding se define despues).
inline Vector4 gl_gui_padding_px(Control* parent) {
    if (!parent) return Vector4();
    const Vector2 psz = parent->get_size();
    for (int i = 0; i < parent->get_child_count(); i++) {
        Node* c = parent->get_child(i);
        if (c && c->is_class("UIPadding") && c->has_method("_gl_pad_px"))
            return c->call("_gl_pad_px", psz);
    }
    return Vector4();
}

// ── Desplazamiento de scroll del padre ──────────────────────────────
inline Vector2 gl_gui_scroll_of(Control* parent) {
    if (parent && parent->has_method("_gl_scroll")) return parent->call("_gl_scroll");
    return Vector2();
}

// ── Tamaño del area de contenido del padre ──────────────────────────
inline Vector2 gl_gui_parent_area(Control* ctrl, Vector4* out_pad = nullptr) {
    Control* parent = Object::cast_to<Control>(ctrl->get_parent());
    Vector2 psz;
    if (parent) {
        psz = parent->get_size();
    } else if (Viewport* vp = ctrl->get_viewport()) {
        psz = vp->get_visible_rect().size;
    }
    Vector4 pad = gl_gui_padding_px(parent);
    if (out_pad) *out_pad = pad;
    return Vector2(MAX(0.0f, psz.x - (float)pad.x - (float)pad.z),
                   MAX(0.0f, psz.y - (float)pad.y - (float)pad.w));
}

// Esquina inferior-derecha maxima que ocupan los hijos: la usa AutomaticSize y
// el AutomaticCanvasSize del ScrollingFrame.
inline Vector2 gl_gui_children_extent(Control* parent) {
    Vector2 m;
    if (!parent) return m;
    for (int i = 0; i < parent->get_child_count(); i++) {
        Control* c = Object::cast_to<Control>(parent->get_child(i));
        if (!c || !c->is_visible()) continue;
        const Vector2 e = c->get_position() + c->get_size();
        m.x = MAX(m.x, e.x);
        m.y = MAX(m.y, e.y);
    }
    return m;
}

// Pide a todos los hijos Control que se vuelvan a colocar (lo usa UIPadding
// cuando cambia el inset: sus hermanos tienen que recalcular su rect).
inline void gl_gui_relayout_children(Control* parent) {
    if (!parent) return;
    for (int i = 0; i < parent->get_child_count(); i++) {
        Node* c = parent->get_child(i);
        if (c && c->has_method("_on_parent_resized")) c->call_deferred("_on_parent_resized");
    }
}

// ── Constraints que el propio GuiObject se aplica ────────────────────
//  UIAspectRatioConstraint y UISizeConstraint son HIJOS del elemento al que
//  limitan. Antes cada uno reescribia el tamaño de su padre en su _process, lo
//  que se peleaba con las anclas cada frame. Ahora se aplican aqui, justo
//  despues de colocar el elemento, ajustando los offsets y dejando fijo el
//  AnchorPoint (igual que Roblox).
inline void gl_gui_apply_constraints(Control* ctrl, float ax, float ay) {
    if (!ctrl) return;
    Node* arc = nullptr;
    Node* sc  = nullptr;
    for (int i = 0; i < ctrl->get_child_count(); i++) {
        Node* c = ctrl->get_child(i);
        if (!c) continue;
        if (!arc && c->is_class("UIAspectRatioConstraint")) arc = c;
        else if (!sc && c->is_class("UISizeConstraint"))    sc = c;
    }
    if (!arc && !sc) return;

    const Vector2 cur = ctrl->get_size();
    Vector2 want = cur;

    if (arc) {
        const double ratio = (double)arc->get(StringName("AspectRatio"));
        const int aspect_type = (int)(int64_t)arc->get(StringName("AspectType"));
        const int dominant    = (int)(int64_t)arc->get(StringName("DominantAxis"));
        if (ratio > 0.0001 && cur.x > 0.01f && cur.y > 0.01f) {
            if (aspect_type == 0) {
                // FitWithinMaxSize: encaja DENTRO del rect actual conservando la
                // proporcion. Es el modo por defecto y el que usa el HUD de los
                // juegos para que sus botones no se deformen. Antes solo se
                // aplicaba DominantAxis, asi que un boton de 382x35 con ratio 4
                // salia de 382x96 (gigante) en vez de 140x35.
                if ((double)cur.x / (double)cur.y > ratio) want = Vector2((float)(cur.y * ratio), cur.y);
                else                                       want = Vector2(cur.x, (float)(cur.x / ratio));
            } else {
                // ScaleWithParentSize: el eje dominante se mide contra el PADRE.
                Control* par = Object::cast_to<Control>(ctrl->get_parent());
                const Vector2 psz = par ? par->get_size() : cur;
                if (dominant == 0) want = Vector2(psz.x, (float)(psz.x / ratio));
                else               want = Vector2((float)(psz.y * ratio), psz.y);
            }
        }
    }
    if (sc) {
        const Vector2 mn = sc->get(StringName("MinSize"));
        const Vector2 mx = sc->get(StringName("MaxSize"));
        want.x = Math::clamp(want.x, mn.x, mx.x);
        want.y = Math::clamp(want.y, mn.y, mx.y);
    }
    // Acotar SIEMPRE el resultado: un AspectRatio pequeno (0.01) sobre un rect
    // ancho daba tamanos astronomicos (cientos de miles de px). Godot intenta
    // reservar el canvas de ese elemento y el render se cae con una textura
    // vacia. Roblox tampoco dibuja nada mayor que la pantalla.
    if (!Math::is_finite(want.x) || !Math::is_finite(want.y)) return;
    want.x = Math::clamp(want.x, 0.0f, 8192.0f);
    want.y = Math::clamp(want.y, 0.0f, 8192.0f);
    if (Math::abs(want.x - cur.x) < 0.25f && Math::abs(want.y - cur.y) < 0.25f) return;

    // Se mantiene fijo el punto de ancla: L' = xa - ax*w
    const float L = ctrl->get_offset(SIDE_LEFT),  R = ctrl->get_offset(SIDE_RIGHT);
    const float T = ctrl->get_offset(SIDE_TOP),   B = ctrl->get_offset(SIDE_BOTTOM);
    const float dx = (want.x - cur.x), dy = (want.y - cur.y);
    ctrl->set_offset(SIDE_LEFT,   L - ax * dx);
    ctrl->set_offset(SIDE_RIGHT,  R + (1.0f - ax) * dx);
    ctrl->set_offset(SIDE_TOP,    T - ay * dy);
    ctrl->set_offset(SIDE_BOTTOM, B + (1.0f - ay) * dy);
    ctrl->set_pivot_offset(ctrl->get_size() * 0.5f);
}

// Tope de reentrada del layout: set_offset() emite "resized" en el acto y eso
// puede volver a entrar aqui por los manejadores. Se corta a 3 niveles; lo que
// falte se recalcula en el siguiente frame (las anclas de Godot ya mantienen el
// rect correcto por su cuenta mientras tanto).
inline int& gl_gui_layout_depth() { static int d = 0; return d; }

// Firma de las ENTRADAS de la ultima colocacion de cada elemento.
//
// Sin esto el layout se reaplicaba en cascada eternamente: la constraint de
// aspecto cambia el tamaño del elemento -> eso emite "resized" -> sus hijos se
// recolocan -> vuelven a partir del UDim2 (deshaciendo la constraint) -> y otra
// vez. Medido en el place real: 31.444 aplicaciones de constraint en una
// partida de 17 s, sin converger nunca, y el juego iba a tirones.
// Con la firma, un elemento solo se recoloca cuando algo de lo que depende
// cambia de verdad (area del padre, su UDim2, el padding o el scroll).
inline HashMap<uint64_t, uint64_t>& gl_gui_layout_sig() {
    static HashMap<uint64_t, uint64_t> m;
    return m;
}

// ── Colocacion EXACTA de un UDim2 ────────────────────────────────────
//  Base: anclas + offsets NATIVOS de Godot, para que el motor resuelva la
//  cadena de padres el solo (no depende de que el padre ya tenga tamaño en el
//  instante de aplicarlo, que era la causa de que un Size=(1,1) saliera
//  100x50). Sobre esa base se suman correcciones en pixeles para lo que las
//  anclas NO pueden expresar: el padding del padre, SizeConstraint y el scroll.
//  Esas correcciones se recalculan cuando el padre cambia de tamaño
//  (_on_parent_resized ya esta conectado), asi que siguen siendo exactas.
inline void gl_gui_apply_layout(Control* ctrl, const GuiUDim2& pos, const GuiUDim2& sz,
                                float ax = 0, float ay = 0,
                                int size_constraint = GL_SC_RELATIVE_XY,
                                float rotation_deg = 0.0f) {
    if (!ctrl) return;
    if (gl_gui_layout_depth() > 3) return;
    gl_gui_layout_depth()++;
    struct Guard { ~Guard() { gl_gui_layout_depth()--; } } _guard;

    Vector4 pad;
    const Vector2 area = gl_gui_parent_area(ctrl, &pad);
    const Vector2 scroll = gl_gui_scroll_of(Object::cast_to<Control>(ctrl->get_parent()));

    {
        uint64_t h = 1469598103934665603ULL;
        auto mix = [&h](double v) {
            h ^= (uint64_t)(int64_t)(v * 64.0);
            h *= 1099511628211ULL;
        };
        mix(area.x); mix(area.y);
        mix(pos.xs); mix(pos.xo); mix(pos.ys); mix(pos.yo);
        mix(sz.xs);  mix(sz.xo);  mix(sz.ys);  mix(sz.yo);
        mix(ax); mix(ay); mix(size_constraint); mix(rotation_deg);
        mix(pad.x); mix(pad.y); mix(pad.z); mix(pad.w);
        mix(scroll.x); mix(scroll.y);
        HashMap<uint64_t, uint64_t>& sig = gl_gui_layout_sig();
        const uint64_t id = (uint64_t)ctrl->get_instance_id();
        HashMap<uint64_t, uint64_t>::Iterator it = sig.find(id);
        if (it && it->value == h) return;                 // nada ha cambiado
        if (sig.size() > 200000) sig.clear();             // tope: nodos ya liberados
        sig[id] = h;
    }

    //  rect_izq  = posScale + (lado==lejano ? (1-a)*sizeScale : -a*sizeScale)
    float aL = pos.xs - ax * sz.xs;
    float aR = pos.xs + (1.0f - ax) * sz.xs;
    float aT = pos.ys - ay * sz.ys;
    float aB = pos.ys + (1.0f - ay) * sz.ys;
    float oL = pos.xo - ax * sz.xo;
    float oR = pos.xo + (1.0f - ax) * sz.xo;
    float oT = pos.yo - ay * sz.yo;
    float oB = pos.yo + (1.0f - ay) * sz.yo;

    // SizeConstraint: el tamaño de un eje se mide contra el OTRO lado del padre.
    // Se saca de las anclas y se mete en offsets con el tamaño real del padre.
    if (size_constraint == GL_SC_RELATIVE_XX) {
        aT = pos.ys;  aB = pos.ys;
        const float h = sz.ys * area.x;
        oT = pos.yo - ay * (sz.yo + h);
        oB = pos.yo + (1.0f - ay) * (sz.yo + h);
    } else if (size_constraint == GL_SC_RELATIVE_YY) {
        aL = pos.xs;  aR = pos.xs;
        const float w = sz.xs * area.y;
        oL = pos.xo - ax * (sz.xo + w);
        oR = pos.xo + (1.0f - ax) * (sz.xo + w);
    }

    // Padding del padre: x = pad_l + A*a + o  ==>  o' = o + pad_l*(1-a) - pad_r*a
    const float pl = (float)pad.x, pt = (float)pad.y, pr = (float)pad.z, pb = (float)pad.w;
    if (pl != 0.0f || pr != 0.0f) {
        oL += pl * (1.0f - aL) - pr * aL;
        oR += pl * (1.0f - aR) - pr * aR;
    }
    if (pt != 0.0f || pb != 0.0f) {
        oT += pt * (1.0f - aT) - pb * aT;
        oB += pt * (1.0f - aB) - pb * aB;
    }
    // Scroll del padre (ScrollingFrame): traslada todo el contenido.
    if (scroll != Vector2()) {
        oL -= scroll.x; oR -= scroll.x;
        oT -= scroll.y; oB -= scroll.y;
    }

    ctrl->set_anchor(SIDE_LEFT,   aL);
    ctrl->set_anchor(SIDE_RIGHT,  aR);
    ctrl->set_anchor(SIDE_TOP,    aT);
    ctrl->set_anchor(SIDE_BOTTOM, aB);
    ctrl->set_offset(SIDE_LEFT,   oL);
    ctrl->set_offset(SIDE_RIGHT,  oR);
    ctrl->set_offset(SIDE_TOP,    oT);
    ctrl->set_offset(SIDE_BOTTOM, oB);
    ctrl->set_custom_minimum_size(Vector2(0, 0));

    // Constraints propias (aspecto / min-max) sobre el rect ya resuelto.
    gl_gui_apply_constraints(ctrl, ax, ay);

    // Roblox gira alrededor del centro; el pivote hay que refrescarlo cada vez
    // que cambia el tamaño o la rotacion se descoloca.
    ctrl->set_pivot_offset(ctrl->get_size() * 0.5f);
    ctrl->set_rotation((float)Math::deg_to_rad((double)rotation_deg));
}

// ── LayoutOrder / orden de los layouts ──────────────────────────────
inline int gl_gui_layout_order(Node* n) {
    if (!n) return 0;
    Variant v = n->get(StringName("LayoutOrder"));
    if (v.get_type() == Variant::INT) return (int)(int64_t)v;
    if (n->has_meta("LayoutOrder")) return (int)(int64_t)n->get_meta("LayoutOrder");
    return 0;
}

// Hijos Control visibles del padre, en el orden que usaria Roblox.
// Enum.SortOrder: Name=0, Custom=1, LayoutOrder=2 (el de por defecto).
// Ordenacion ESTABLE: a igual clave se conserva el orden del arbol, igual que
// hace Roblox.
inline void gl_gui_sorted_children(Control* parent, int sort_order,
                                   LocalVector<Control*>& out) {
    out.clear();
    if (!parent) return;
    const int cnt = parent->get_child_count();
    for (int i = 0; i < cnt; i++) {
        Control* c = Object::cast_to<Control>(parent->get_child(i));
        if (c && c->is_visible()) out.push_back(c);
    }
    if (sort_order == GL_SORT_CUSTOM) return;   // orden del arbol tal cual

    const int n = (int)out.size();
    for (int i = 1; i < n; i++) {              // insercion: estable y n pequeño
        Control* key = out[i];
        int j = i - 1;
        while (j >= 0) {
            bool greater;
            if (sort_order == GL_SORT_NAME)
                greater = String(out[j]->get_name()) > String(key->get_name());
            else
                greater = gl_gui_layout_order(out[j]) > gl_gui_layout_order(key);
            if (!greater) break;
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = key;
    }
}

// ── Alineacion en el eje transversal ────────────────────────────────
// Enum.HorizontalAlignment: Center=0, Left=1, Right=2
// Enum.VerticalAlignment:   Center=0, Top=1,  Bottom=2
inline float gl_gui_cross(int align, float total, float item) {
    if (align == 0) return (total - item) * 0.5f;
    if (align == 2) return total - item;
    return 0.0f;
}

// ── Huella de estado, para no recolocar cada frame ──────────────────
// Recolocar dispara una cascada de notificaciones en Godot; con 1400 Frames
// eso solo se puede hacer cuando algo cambia de verdad.
inline uint64_t gl_gui_children_stamp(Control* parent) {
    if (!parent) return 0;
    uint64_t h = 1469598103934665603ULL;
    auto mix = [&h](uint64_t v) { h ^= v; h *= 1099511628211ULL; };
    const Vector2 psz = parent->get_size();
    mix((uint64_t)(int)psz.x); mix((uint64_t)(int)psz.y);
    const int cnt = parent->get_child_count();
    mix((uint64_t)cnt);
    for (int i = 0; i < cnt; i++) {
        Control* c = Object::cast_to<Control>(parent->get_child(i));
        if (!c) continue;
        const Vector2 s = c->get_size();
        mix((uint64_t)(int)(s.x * 4.0f));
        mix((uint64_t)(int)(s.y * 4.0f));
        mix(c->is_visible() ? 3ULL : 7ULL);
        mix((uint64_t)(gl_gui_layout_order(c) + 4096));
    }
    return h;
}

#endif // GL_GUI_CORE_H
