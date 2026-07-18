#ifndef GL_GRAPHICS_H
#define GL_GRAPHICS_H

// ════════════════════════════════════════════════════════════════════
//  Calidad gráfica estilo Roblox (1.15) — 10 niveles (1..10)
//  Un solo sitio que aplica TODO: escala de render, sombras (filtro soft
//  real, resolución del atlas, bias del sol), SSAO, glow y niebla. Lo usan
//  la iluminación por defecto del Workspace y el sistema de ajustes.
//
//  El problema de las sombras "con puntos" y "difuminado raro" era esto:
//  Godot por defecto usa el filtro de sombra soft en calidad BAJA (un
//  dithered que parece borrar píxeles) y el sol tenía un tamaño de penumbra
//  enorme (1.5) sin bias → acne + ruido. Aquí se sube el filtro a HIGH/ULTRA,
//  se ajusta el bias y se baja el tamaño para una sombra limpia y suave.
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/light3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// Nivel de calidad efectivo actual (1..10). Lo lee quien necesite saberlo.
inline int& gl_graphics_level() { static int lvl = 8; return lvl; }

// Busca el primer nodo de una clase bajo la raíz (sun / WorldEnvironment).
inline Node* gl_find_by_class(Node* n, const char* cls) {
    if (!n) return nullptr;
    if (n->is_class(cls)) return n;
    for (int i = 0; i < n->get_child_count(); i++)
        if (Node* r = gl_find_by_class(n->get_child(i), cls)) return r;
    return nullptr;
}

// Parámetros de calidad por nivel. Curva pensada para que del 1 al 10 haya un
// salto notable pero coherente (como los 1..10 de Roblox).
struct GLQualityDef {
    float render_scale;      // escala 3D del viewport
    int   shadow_filter;     // RenderingServer::ShadowQuality (0 hard .. 5 ultra)
    int   atlas_size;        // resolución del shadow map direccional
    bool  ssao;
    bool  glow;
    bool  fog;
    float sun_size;          // tamaño de penumbra del sol (grados)
    bool  shadows;
};

inline GLQualityDef gl_quality_def(int level) {
    level = level < 1 ? 1 : (level > 10 ? 10 : level);
    // {escala, filtro, atlas, ssao, glow, fog, sunSize, shadows}
    static const GLQualityDef T[10] = {
        {0.50f, 0, 1024,  false, false, false, 0.0f, false}, // 1
        {0.60f, 0, 1024,  false, false, false, 0.0f, false}, // 2
        {0.70f, 0, 2048,  false, false, true,  0.3f, true },  // 3 (sombra dura)
        {0.75f, 1, 2048,  false, true,  true,  0.4f, true },  // 4 (soft very low)
        {0.85f, 2, 2048,  false, true,  true,  0.5f, true },  // 5 (soft low)
        {0.90f, 3, 4096,  true,  true,  true,  0.5f, true },  // 6 (soft medium)
        {1.00f, 3, 4096,  true,  true,  true,  0.6f, true },  // 7
        {1.00f, 4, 4096,  true,  true,  true,  0.6f, true },  // 8 (soft high)
        {1.00f, 4, 8192,  true,  true,  true,  0.7f, true },  // 9
        {1.00f, 5, 8192,  true,  true,  true,  0.7f, true },  // 10 (soft ultra)
    };
    return T[level - 1];
}

// Aplica un nivel de calidad (1..10) a TODA la escena. `any` es cualquier nodo
// dentro del árbol (para llegar a la raíz, el viewport y el sol).
inline void gl_apply_graphics_quality(int level, Node* any) {
    level = level < 1 ? 1 : (level > 10 ? 10 : level);
    gl_graphics_level() = level;
    GLQualityDef q = gl_quality_def(level);

    RenderingServer* rs = RenderingServer::get_singleton();
    if (rs) {
        // El filtro soft REAL: esto es lo que quita el "dithered" y da una
        // penumbra suave de verdad en vez de píxeles sueltos.
        rs->directional_soft_shadow_filter_set_quality((RenderingServer::ShadowQuality)q.shadow_filter);
        rs->positional_soft_shadow_filter_set_quality((RenderingServer::ShadowQuality)q.shadow_filter);
        rs->directional_shadow_atlas_set_size(q.atlas_size, false);
    }

    if (!any || !any->is_inside_tree()) return;
    Node* root = (Node*)any->get_tree()->get_root();

    // Escala de render 3D (afecta al viewport principal).
    if (Viewport* vp = any->get_viewport()) vp->set_scaling_3d_scale(q.render_scale);

    // Sol: sombra on/off, tamaño de penumbra y bias que evita el acne.
    if (Node* sn = gl_find_by_class(root, "DirectionalLight3D")) {
        if (DirectionalLight3D* dl = Object::cast_to<DirectionalLight3D>(sn)) {
            dl->set_shadow(q.shadows);
            dl->set_param(Light3D::PARAM_SIZE, q.sun_size);
            dl->set_param(Light3D::PARAM_SHADOW_BLUR, 1.0f);
            // Bias: suficiente para matar el acne (los "puntos") sin peter-panning.
            dl->set_param(Light3D::PARAM_SHADOW_NORMAL_BIAS, 1.0f);
            dl->set_param(Light3D::PARAM_SHADOW_BIAS, 0.06f);
            // Rango de sombra amplio pero con más resolución cerca (menos ruido).
            dl->set_param(Light3D::PARAM_SHADOW_MAX_DISTANCE, 200.0f);
            dl->set_param(Light3D::PARAM_SHADOW_PANCAKE_SIZE, 20.0f);
        }
    }

    // Environment: SSAO / glow / niebla según el nivel.
    if (Node* wen = gl_find_by_class(root, "WorldEnvironment")) {
        if (WorldEnvironment* we = Object::cast_to<WorldEnvironment>(wen)) {
            Ref<Environment> env = we->get_environment();
            if (env.is_valid()) {
                env->set_ssao_enabled(q.ssao);
                env->set_glow_enabled(q.glow);
                env->set_fog_enabled(q.fog);
            }
        }
    }
}

#endif // GL_GRAPHICS_H
