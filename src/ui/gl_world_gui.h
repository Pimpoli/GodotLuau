#ifndef GL_WORLD_GUI_H
#define GL_WORLD_GUI_H

// ════════════════════════════════════════════════════════════════════
//  UI que vive EN EL MUNDO 3D: BillboardGui y SurfaceGui.
//
//  El bug que arregla: las dos eran Node3D vacios y sus hijos son Control
//  (CanvasItem). En Godot un CanvasItem se dibuja en el canvas del viewport
//  MAS CERCANO, no dentro de su padre 3D, asi que el contenido de los 1185
//  SurfaceGui y 31 BillboardGui de un place normal se pintaba TODO encima de la
//  pantalla: paneles gigantes tapando el HUD ("V.I.P!", "KILL EVERYONE",
//  "NUCLEAR BOMB", barras de vida...) aunque en Roblox estuvieran pegados a un
//  cartel al otro lado del mapa.
//
//  Lo que se hace ahora:
//   1. El subarbol Control se saca del canvas de PANTALLA con visibility_layer
//      + canvas_cull_mask del viewport. Asi NO se toca su propiedad Visible
//      (los scripts que la leen o escriben siguen viendo la verdad) y deja de
//      ensuciar la pantalla.
//   2. Su contenido se refleja en el mundo con nodos 3D nativos: Label3D para
//      los textos y Sprite3D para las imagenes, en el sitio y la escala que les
//      toca. Con LOD (visibility_range_end desde MaxDistance) para que 1185
//      carteles no cuesten FPS.
//
//  Limitacion conocida y a la vista: es un REFLEJO del contenido (textos e
//  imagenes), no un render pixel a pixel del arbol de UI en la superficie. Los
//  degradados, bordes y capas de un cartel muy elaborado se simplifican.
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/sprite3d.hpp>
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/base_material3d.hpp>
#include "gl_gui_core.h"

using namespace godot;

// La capa reservada (GL_WORLD_GUI_CANVAS_LAYER) y el ajuste del cull_mask
// viven en gl_gui_core.h, que es donde tambien la usa la puerta de dibujado.

// Marca todo el subarbol CanvasItem para que el viewport lo descarte.
inline void gl_world_gui_mark(Node* n) {
    if (!n) return;
    if (CanvasItem* ci = Object::cast_to<CanvasItem>(n))
        ci->set_visibility_layer(GL_WORLD_GUI_CANVAS_LAYER);
    for (int i = 0; i < n->get_child_count(); i++) gl_world_gui_mark(n->get_child(i));
}

inline void gl_world_gui_cull(Node* owner) { gl_gui_cull_world_layer(owner); }

// Una UI de mundo solo se pinta si esta DENTRO del Workspace. En un place hay
// carteles guardados en ReplicatedStorage/ServerStorage como plantillas y no
// deben aparecer en ninguna parte hasta que el juego los coloque.
inline bool gl_world_gui_in_world(Node* n) {
    for (Node* p = n ? n->get_parent() : nullptr; p; p = p->get_parent()) {
        const StringName nm = p->get_name();
        if (nm == StringName("Workspace")) return true;
        if (nm == StringName("ReplicatedStorage") || nm == StringName("ServerStorage") ||
            nm == StringName("ReplicatedFirst")   || nm == StringName("StarterGui") ||
            nm == StringName("StarterPack")       || nm == StringName("ServerScriptService"))
            return false;
    }
    return false;
}

// Datos de un elemento reflejado en el mundo.
struct GLWorldGuiItem {
    String  text;
    Color   color = Color(1, 1, 1);
    Color   stroke = Color(0, 0, 0);
    bool    has_stroke = false;
    Ref<Texture2D> tex;
    Vector2 center_frac;   // centro del elemento, 0..1 sobre el area del GUI
    Vector2 size_frac;     // tamaño relativo
    int     zindex = 0;
};

// Recoge (hasta un tope) los textos e imagenes visibles del subarbol.
inline void gl_world_gui_collect(Node* n, Vector2 area, LocalVector<GLWorldGuiItem>& out,
                                 int max_items) {
    if (!n || (int)out.size() >= max_items) return;
    if (Control* c = Object::cast_to<Control>(n)) {
        if (!c->is_visible()) return;   // oculto en Roblox -> no se refleja
        const Rect2 r = c->get_global_rect();
        if (area.x > 1.0f && area.y > 1.0f && r.size.x > 0.5f && r.size.y > 0.5f) {
            GLWorldGuiItem it;
            it.center_frac = Vector2((r.position.x + r.size.x * 0.5f) / area.x,
                                     (r.position.y + r.size.y * 0.5f) / area.y);
            it.size_frac = Vector2(r.size.x / area.x, r.size.y / area.y);
            it.zindex = c->get_z_index();
            bool useful = false;
            String txt;
            if (Label* l = Object::cast_to<Label>(c))        txt = l->get_text();
            else if (Button* b = Object::cast_to<Button>(c)) txt = b->get_text();
            if (!txt.strip_edges().is_empty()) {
                it.text = txt;
                it.color = c->get_theme_color("font_color");
                const int os = c->get_theme_constant("outline_size");
                it.has_stroke = os > 0;
                it.stroke = c->get_theme_color("font_outline_color");
                useful = true;
            } else if (TextureRect* tr = Object::cast_to<TextureRect>(c)) {
                Ref<Texture2D> t = tr->get_texture();
                if (t.is_valid()) { it.tex = t; useful = true; }
            }
            if (useful) out.push_back(it);
        }
    }
    for (int i = 0; i < n->get_child_count(); i++)
        gl_world_gui_collect(n->get_child(i), area, out, max_items);
}

// Construye los nodos 3D del reflejo. `w`/`h` en studs, `normal` es el eje al
// que mira la cara (para un SurfaceGui) o Vector3() para un billboard.
inline void gl_world_gui_build(Node3D* host, const LocalVector<GLWorldGuiItem>& items,
                               float w, float h, bool billboard, float max_dist) {
    // Se borra el reflejo anterior (los nodos llevan meta para reconocerlos y
    // no se guardan en la escena).
    for (int i = host->get_child_count() - 1; i >= 0; i--) {
        Node* c = host->get_child(i);
        if (c && c->has_meta("__gl_world_gui")) { host->remove_child(c); c->queue_free(); }
    }
    for (uint32_t i = 0; i < items.size(); i++) {
        const GLWorldGuiItem& it = items[i];
        // Roblox: X crece a la derecha, Y crece hacia ABAJO.
        const Vector3 pos((it.center_frac.x - 0.5f) * w,
                          (0.5f - it.center_frac.y) * h,
                          0.002f * (float)(i + 1));   // separacion para el orden
        if (!it.text.is_empty()) {
            Label3D* l = memnew(Label3D);
            l->set_meta("__gl_world_gui", true);
            l->set_text(it.text);
            l->set_modulate(it.color);
            l->set_font_size(64);
            // Altura de la caja del elemento en studs / tamaño de fuente.
            const float target = CLAMP(it.size_frac.y * h, 0.05f, 200.0f);
            l->set_pixel_size(CLAMP(target / 64.0f, 0.0005f, 4.0f));
            if (it.has_stroke) {
                l->set_outline_size(12);
                l->set_outline_modulate(it.stroke);
            } else {
                l->set_outline_size(0);
            }
            l->set_billboard_mode(billboard ? BaseMaterial3D::BILLBOARD_ENABLED
                                            : BaseMaterial3D::BILLBOARD_DISABLED);
            l->set_draw_flag(Label3D::FLAG_DOUBLE_SIDED, true);
            l->set_position(pos);
            if (max_dist > 0.5f) l->set_visibility_range_end(max_dist);
            host->add_child(l);
        } else if (it.tex.is_valid()) {
            Sprite3D* s = memnew(Sprite3D);
            s->set_meta("__gl_world_gui", true);
            s->set_texture(it.tex);
            s->set_modulate(it.color);
            const Vector2 tsz = it.tex->get_size();
            if (tsz.x > 0.5f) s->set_pixel_size(CLAMP((it.size_frac.x * w) / tsz.x, 0.0005f, 4.0f));
            s->set_billboard_mode(billboard ? BaseMaterial3D::BILLBOARD_ENABLED
                                            : BaseMaterial3D::BILLBOARD_DISABLED);
            s->set_draw_flag(Sprite3D::FLAG_DOUBLE_SIDED, true);
            s->set_position(pos);
            if (max_dist > 0.5f) s->set_visibility_range_end(max_dist);
            host->add_child(s);
        }
    }
}

#endif // GL_WORLD_GUI_H
