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

// ── Modo de calidad (como Roblox) ────────────────────────────────────
//   0 = Automatic — el motor sube/baja la calidad solo para sostener los FPS
//   1 = Manual    — un nivel fijo 1..10 (lo clasico)
//   2 = Custom    — cada ajuste por separado (resolucion/FSR, sombras, vista…)
enum GLQualityMode { GL_QUALITY_AUTOMATIC = 0, GL_QUALITY_MANUAL = 1, GL_QUALITY_CUSTOM = 2 };
inline int& gl_quality_mode() { static int m = GL_QUALITY_MANUAL; return m; }

// FPS objetivo del modo Automatic (Roblox apunta a una experiencia fluida).
inline int& gl_auto_target_fps() { static int f = 45; return f; }

// ── Ajustes del modo Custom ──────────────────────────────────────────
// Cada uno es independiente; asi el jugador puede, por ejemplo, bajar sombras
// pero mantener la resolucion nativa.
struct GLCustomSettings {
    float render_scale   = 1.0f;   // 0.25..1.0 (con FSR sube la nitidez al reescalar)
    int   upscaler       = 2;      // 0=Bilinear 1=FSR 2=FSR2
    float sharpness      = 0.5f;   // nitidez del FSR
    int   shadow_quality = 4;      // 0=off .. 5=ultra
    float view_distance  = 1.0f;   // 0.25..2.0 (multiplica el LOD/distancia de sombra)
    bool  ssao           = true;
    bool  glow           = true;
    bool  fog            = true;
};
inline GLCustomSettings& gl_custom() { static GLCustomSettings c; return c; }

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

    // En Custom cada ajuste lo decide el jugador, no la curva del nivel.
    if (gl_quality_mode() == GL_QUALITY_CUSTOM) {
        const GLCustomSettings& c = gl_custom();
        q.render_scale  = c.render_scale;
        q.shadow_filter = CLAMP(c.shadow_quality, 0, 5);
        q.shadows       = c.shadow_quality > 0;
        q.ssao          = c.ssao;
        q.glow          = c.glow;
        q.fog           = c.fog;
        q.atlas_size    = c.shadow_quality >= 4 ? 8192 : (c.shadow_quality >= 2 ? 4096 : 2048);
    }

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

    // ── Escala de render 3D + reescalado FSR ─────────────────────────────
    // Rebajar la resolucion interna es lo que MAS FPS da (medido: a la mitad,
    // +28%). Con FSR2 la imagen se reconstruye a resolucion completa, asi que
    // se gana rendimiento perdiendo mucha menos nitidez que con un simple
    // escalado bilineal. En Custom manda lo que haya elegido el jugador.
    if (Viewport* vp = any->get_viewport()) {
        const bool custom = (gl_quality_mode() == GL_QUALITY_CUSTOM);
        const float scale = custom ? gl_custom().render_scale : q.render_scale;
        vp->set_scaling_3d_scale(scale);
        int up = custom ? gl_custom().upscaler : (scale < 0.99f ? 2 : 0);
        vp->set_scaling_3d_mode((Viewport::Scaling3DMode)CLAMP(up, 0, 2));
        vp->set_fsr_sharpness(custom ? gl_custom().sharpness : 0.5f);
    }

    // Sol: sombra on/off, tamaño de penumbra y bias que evita el acne.
    // Si existe el servicio Lighting, EL manda sobre GlobalShadows y ShadowSoftness
    // (como en Roblox): la calidad solo ajusta resolucion/filtro/bias, no pisa esos
    // dos valores del juego.
    const bool has_lighting = gl_find_by_class(root, "Lighting") != nullptr;
    if (Node* sn = gl_find_by_class(root, "DirectionalLight3D")) {
        if (DirectionalLight3D* dl = Object::cast_to<DirectionalLight3D>(sn)) {
            if (!has_lighting) {
                dl->set_shadow(q.shadows);
                dl->set_param(Light3D::PARAM_SIZE, q.sun_size);
                dl->set_param(Light3D::PARAM_SHADOW_BLUR, 1.0f);
            }
            // Bias: suficiente para matar el acne (los "puntos") sin peter-panning.
            dl->set_param(Light3D::PARAM_SHADOW_NORMAL_BIAS, 1.0f);
            dl->set_param(Light3D::PARAM_SHADOW_BIAS, 0.06f);
            // Rango de sombra: cuanto mas corto, menos mundo hay que redibujar en
            // el mapa de sombras cada frame. En Custom lo escala ViewDistance.
            float sdist = 200.0f;
            if (gl_quality_mode() == GL_QUALITY_CUSTOM)
                sdist = 200.0f * CLAMP(gl_custom().view_distance, 0.25f, 2.0f);
            else if (level <= 5) sdist = 100.0f;
            dl->set_param(Light3D::PARAM_SHADOW_MAX_DISTANCE, sdist);
            dl->set_param(Light3D::PARAM_SHADOW_PANCAKE_SIZE, 20.0f);
        }
    }

    // Environment: SSAO / glow / niebla según el nivel. PERO glow/niebla NO se
    // tocan si hay un efecto de Lighting que los gobierna (Bloom/SunRays → glow,
    // Atmosphere → niebla): asi un efecto con Enabled=false no se reactiva al
    // aplicar la calidad. SSAO no lo controla ningun efecto de Roblox, va libre.
    if (Node* wen = gl_find_by_class(root, "WorldEnvironment")) {
        if (WorldEnvironment* we = Object::cast_to<WorldEnvironment>(wen)) {
            Ref<Environment> env = we->get_environment();
            if (env.is_valid()) {
                const bool has_glow_fx = gl_find_by_class(root, "BloomEffect") ||
                                         gl_find_by_class(root, "SunRaysNode");
                const bool has_fog_fx  = gl_find_by_class(root, "AtmosphereNode");
                env->set_ssao_enabled(q.ssao);
                if (!has_glow_fx) env->set_glow_enabled(q.glow);
                if (!has_fog_fx)  env->set_fog_enabled(q.fog);
            }
        }
    }
}

// ── Modo Automatic ───────────────────────────────────────────────────
// Como el "Automatic" de Roblox: vigila los FPS y sube o baja el nivel de
// calidad solo, buscando mantenerse por encima del objetivo (45 FPS por
// defecto). Cambia de UN nivel cada vez y espera entre ajustes para no ir
// oscilando arriba y abajo. Llamar cada frame; no hace nada en otros modos.
inline void gl_auto_quality_tick(double delta, Node* any) {
    if (gl_quality_mode() != GL_QUALITY_AUTOMATIC || !any) return;
    static double acc = 0.0, cooldown = 0.0;
    static int frames = 0;
    static double fps_sum = 0.0;

    if (delta > 0.0) { fps_sum += 1.0 / delta; frames++; }
    acc += delta;
    if (cooldown > 0.0) cooldown -= delta;
    if (acc < 1.0 || frames < 10) return;      // promediar ~1 segundo

    const double fps = fps_sum / frames;
    acc = 0.0; frames = 0; fps_sum = 0.0;
    if (cooldown > 0.0) return;

    const int target = gl_auto_target_fps();
    const int lvl = gl_graphics_level();
    if (fps < target - 3 && lvl > 1) {          // va justo: bajar calidad
        gl_apply_graphics_quality(lvl - 1, any);
        cooldown = 2.0;
    } else if (fps > target + 15 && lvl < 10) { // sobra margen: subir calidad
        gl_apply_graphics_quality(lvl + 1, any);
        cooldown = 4.0;                          // subir con mas prudencia
    }
}

#endif // GL_GRAPHICS_H
