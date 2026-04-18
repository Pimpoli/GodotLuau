#ifndef ROBLOX_LIGHTING_FX_H
#define ROBLOX_LIGHTING_FX_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/classes/procedural_sky_material.hpp>
#include <godot_cpp/classes/camera_attributes_practical.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

// ── Tree-wide helpers ─────────────────────────────────────────────────
static WorldEnvironment* _lx_find_we(Node* n) {
    if (!n) return nullptr;
    WorldEnvironment* we = Object::cast_to<WorldEnvironment>(n);
    if (we) return we;
    for (int i = 0; i < n->get_child_count(); i++) {
        WorldEnvironment* f = _lx_find_we(n->get_child(i));
        if (f) return f;
    }
    return nullptr;
}
static Ref<Environment> _lx_get_env(Node* self) {
    if (!self || !self->is_inside_tree()) return {};
    WorldEnvironment* we = _lx_find_we(self->get_tree()->get_root());
    if (!we) return {};
    return we->get_environment();
}
static ProceduralSkyMaterial* _lx_get_sky_mat(Node* self) {
    Ref<Environment> env = _lx_get_env(self);
    if (!env.is_valid()) return nullptr;
    Ref<Sky> sky = env->get_sky();
    if (!sky.is_valid()) return nullptr;
    Ref<Material> mat = sky->get_material();
    return Object::cast_to<ProceduralSkyMaterial>(mat.ptr());
}
static WorldEnvironment* _lx_find_we_node(Node* self) {
    if (!self || !self->is_inside_tree()) return nullptr;
    return _lx_find_we(self->get_tree()->get_root());
}

// ════════════════════════════════════════════════════════════════════
//  AtmosphereNode — Roblox Atmosphere equivalent
//  Controls volumetric atmosphere and fog effects.
// ════════════════════════════════════════════════════════════════════
class AtmosphereNode : public Node {
    GDCLASS(AtmosphereNode, Node);

    float density = 0.395f;
    float offset  = 0.0f;
    Color color   = Color(0.71f, 0.81f, 0.93f);
    float decay   = 0.0f;
    float glare   = 0.0f;
    float haze    = 0.0f;

    void _apply() {
        Ref<Environment> env = _lx_get_env(this);
        if (!env.is_valid()) return;
        bool on = density > 0.001f;
        env->set_fog_enabled(on);
        if (!on) return;
        env->set_fog_mode(Environment::FOG_MODE_EXPONENTIAL);
        env->set_fog_density(density * 0.08f);
        env->set_fog_height(offset * 20.0f);
        env->set_fog_height_density(decay * 5.0f);
        env->set_fog_sun_scatter(glare * 0.5f);
        env->set_fog_aerial_perspective(haze);
        env->set_fog_light_color(color);
        env->set_fog_light_energy(1.0f);
        env->set_fog_sky_affect(haze * 0.5f);
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_density","v"), &AtmosphereNode::set_density);
        ClassDB::bind_method(D_METHOD("get_density"),     &AtmosphereNode::get_density);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Density",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_density","get_density");

        ClassDB::bind_method(D_METHOD("set_offset","v"), &AtmosphereNode::set_offset);
        ClassDB::bind_method(D_METHOD("get_offset"),     &AtmosphereNode::get_offset);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Offset",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_offset","get_offset");

        ClassDB::bind_method(D_METHOD("set_color","c"), &AtmosphereNode::set_color);
        ClassDB::bind_method(D_METHOD("get_color"),     &AtmosphereNode::get_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"Color"),"set_color","get_color");

        ClassDB::bind_method(D_METHOD("set_decay","v"), &AtmosphereNode::set_decay);
        ClassDB::bind_method(D_METHOD("get_decay"),     &AtmosphereNode::get_decay);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Decay",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_decay","get_decay");

        ClassDB::bind_method(D_METHOD("set_glare","v"), &AtmosphereNode::set_glare);
        ClassDB::bind_method(D_METHOD("get_glare"),     &AtmosphereNode::get_glare);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Glare",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_glare","get_glare");

        ClassDB::bind_method(D_METHOD("set_haze","v"), &AtmosphereNode::set_haze);
        ClassDB::bind_method(D_METHOD("get_haze"),     &AtmosphereNode::get_haze);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Haze",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_haze","get_haze");
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _apply();
    }

public:
    void set_density(float v) { density = Math::clamp(v,0.0f,1.0f); if(is_inside_tree()) _apply(); }
    float get_density() const { return density; }
    void set_offset(float v)  { offset = Math::clamp(v,0.0f,1.0f);  if(is_inside_tree()) _apply(); }
    float get_offset() const  { return offset; }
    void set_color(Color c)   { color = c;                           if(is_inside_tree()) _apply(); }
    Color get_color() const   { return color; }
    void set_decay(float v)   { decay = Math::clamp(v,0.0f,1.0f);   if(is_inside_tree()) _apply(); }
    float get_decay() const   { return decay; }
    void set_glare(float v)   { glare = Math::clamp(v,0.0f,1.0f);   if(is_inside_tree()) _apply(); }
    float get_glare() const   { return glare; }
    void set_haze(float v)    { haze = Math::clamp(v,0.0f,1.0f);    if(is_inside_tree()) _apply(); }
    float get_haze() const    { return haze; }
};

// ════════════════════════════════════════════════════════════════════
//  LightingSkyNode — Roblox Sky equivalent
//  Controls procedural sky colors, sun size, and sky brightness.
// ════════════════════════════════════════════════════════════════════
class LightingSkyNode : public Node {
    GDCLASS(LightingSkyNode, Node);

    Color sky_color     = Color(0.18f, 0.36f, 0.78f);
    Color horizon_color = Color(0.62f, 0.79f, 0.95f);
    Color ground_color  = Color(0.13f, 0.11f, 0.09f);
    float sun_angular_size = 21.0f;
    float sky_exposure  = 1.0f;
    float sky_curve     = 0.09f;

    void _apply() {
        ProceduralSkyMaterial* mat = _lx_get_sky_mat(this);
        if (!mat) return;
        mat->set_sky_top_color(sky_color);
        mat->set_sky_horizon_color(horizon_color);
        mat->set_sky_curve(sky_curve);
        mat->set_ground_bottom_color(ground_color);
        mat->set_ground_horizon_color(horizon_color.lightened(0.15f));
        mat->set_sun_angle_max(sun_angular_size);
        mat->set_sky_energy_multiplier(sky_exposure);
        mat->set_energy_multiplier(sky_exposure);
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_sky_color","c"),    &LightingSkyNode::set_sky_color);
        ClassDB::bind_method(D_METHOD("get_sky_color"),        &LightingSkyNode::get_sky_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"SkyColor"),"set_sky_color","get_sky_color");

        ClassDB::bind_method(D_METHOD("set_horizon_color","c"),&LightingSkyNode::set_horizon_color);
        ClassDB::bind_method(D_METHOD("get_horizon_color"),    &LightingSkyNode::get_horizon_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"HorizonColor"),"set_horizon_color","get_horizon_color");

        ClassDB::bind_method(D_METHOD("set_ground_color","c"), &LightingSkyNode::set_ground_color);
        ClassDB::bind_method(D_METHOD("get_ground_color"),     &LightingSkyNode::get_ground_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"GroundColor"),"set_ground_color","get_ground_color");

        ClassDB::bind_method(D_METHOD("set_sun_angular_size","v"),&LightingSkyNode::set_sun_angular_size);
        ClassDB::bind_method(D_METHOD("get_sun_angular_size"),    &LightingSkyNode::get_sun_angular_size);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"SunAngularSize",PROPERTY_HINT_RANGE,"0,90,0.1"),"set_sun_angular_size","get_sun_angular_size");

        ClassDB::bind_method(D_METHOD("set_sky_exposure","v"),&LightingSkyNode::set_sky_exposure);
        ClassDB::bind_method(D_METHOD("get_sky_exposure"),    &LightingSkyNode::get_sky_exposure);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"SkyExposure",PROPERTY_HINT_RANGE,"0,8,0.01"),"set_sky_exposure","get_sky_exposure");

        ClassDB::bind_method(D_METHOD("set_sky_curve","v"),&LightingSkyNode::set_sky_curve);
        ClassDB::bind_method(D_METHOD("get_sky_curve"),    &LightingSkyNode::get_sky_curve);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"SkyCurve",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_sky_curve","get_sky_curve");
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _apply();
    }

public:
    void set_sky_color(Color c)         { sky_color = c;                               if(is_inside_tree()) _apply(); }
    Color get_sky_color() const         { return sky_color; }
    void set_horizon_color(Color c)     { horizon_color = c;                           if(is_inside_tree()) _apply(); }
    Color get_horizon_color() const     { return horizon_color; }
    void set_ground_color(Color c)      { ground_color = c;                            if(is_inside_tree()) _apply(); }
    Color get_ground_color() const      { return ground_color; }
    void set_sun_angular_size(float v)  { sun_angular_size = Math::clamp(v,0.0f,90.0f); if(is_inside_tree()) _apply(); }
    float get_sun_angular_size() const  { return sun_angular_size; }
    void set_sky_exposure(float v)      { sky_exposure = Math::clamp(v,0.0f,8.0f);    if(is_inside_tree()) _apply(); }
    float get_sky_exposure() const      { return sky_exposure; }
    void set_sky_curve(float v)         { sky_curve = Math::clamp(v,0.0f,1.0f);       if(is_inside_tree()) _apply(); }
    float get_sky_curve() const         { return sky_curve; }
};

// ════════════════════════════════════════════════════════════════════
//  SunRaysNode — Roblox SunRaysEffect equivalent
//  Simulates sun ray scattering via glow with high-octave levels.
// ════════════════════════════════════════════════════════════════════
class SunRaysNode : public Node {
    GDCLASS(SunRaysNode, Node);

    float intensity = 0.25f;
    float spread    = 1.0f;

    void _apply() {
        Ref<Environment> env = _lx_get_env(this);
        if (!env.is_valid()) return;
        if (intensity < 0.01f) { env->set_glow_enabled(false); return; }
        env->set_glow_enabled(true);
        env->set_glow_normalized(false);
        env->set_glow_strength(2.0f);
        env->set_glow_intensity(intensity);
        env->set_glow_bloom(0.1f);
        env->set_glow_blend_mode(Environment::GLOW_BLEND_MODE_ADDITIVE);
        // Enable high-octave glow levels for god-ray effect based on spread
        int max_level = (int)(spread * 6.0f + 0.5f);
        for (int i = 0; i < 7; i++) {
            env->set_glow_level(i, (i <= max_level) ? intensity * 0.3f : 0.0f);
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_intensity","v"), &SunRaysNode::set_intensity);
        ClassDB::bind_method(D_METHOD("get_intensity"),     &SunRaysNode::get_intensity);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Intensity",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_intensity","get_intensity");

        ClassDB::bind_method(D_METHOD("set_spread","v"), &SunRaysNode::set_spread);
        ClassDB::bind_method(D_METHOD("get_spread"),     &SunRaysNode::get_spread);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Spread",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_spread","get_spread");
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _apply();
    }

public:
    void set_intensity(float v) { intensity = Math::clamp(v,0.0f,1.0f); if(is_inside_tree()) _apply(); }
    float get_intensity() const { return intensity; }
    void set_spread(float v)    { spread = Math::clamp(v,0.0f,1.0f);    if(is_inside_tree()) _apply(); }
    float get_spread() const    { return spread; }
};

// ════════════════════════════════════════════════════════════════════
//  BloomEffect — Roblox BloomEffect equivalent
//  Controls HDR glow/bloom post-processing.
// ════════════════════════════════════════════════════════════════════
class BloomEffect : public Node {
    GDCLASS(BloomEffect, Node);

    float intensity  = 0.5f;
    float size       = 0.5f;
    float threshold  = 0.8f;

    void _apply() {
        Ref<Environment> env = _lx_get_env(this);
        if (!env.is_valid()) return;
        env->set_glow_enabled(true);
        env->set_glow_normalized(false);
        env->set_glow_intensity(intensity);
        env->set_glow_bloom(size);
        env->set_glow_hdr_bleed_threshold(threshold);
        env->set_glow_hdr_bleed_scale(2.0f);
        env->set_glow_hdr_luminance_cap(8.0f);
        env->set_glow_blend_mode(Environment::GLOW_BLEND_MODE_SOFTLIGHT);
        // Activate all glow levels proportionally
        for (int i = 0; i < 7; i++) {
            env->set_glow_level(i, intensity * 0.15f);
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_intensity","v"), &BloomEffect::set_intensity);
        ClassDB::bind_method(D_METHOD("get_intensity"),     &BloomEffect::get_intensity);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Intensity",PROPERTY_HINT_RANGE,"0,3,0.001"),"set_intensity","get_intensity");

        ClassDB::bind_method(D_METHOD("set_size","v"), &BloomEffect::set_size);
        ClassDB::bind_method(D_METHOD("get_size"),     &BloomEffect::get_size);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Size",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_size","get_size");

        ClassDB::bind_method(D_METHOD("set_threshold","v"), &BloomEffect::set_threshold);
        ClassDB::bind_method(D_METHOD("get_threshold"),     &BloomEffect::get_threshold);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Threshold",PROPERTY_HINT_RANGE,"0,8,0.01"),"set_threshold","get_threshold");
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _apply();
    }

public:
    void set_intensity(float v)  { intensity = Math::clamp(v,0.0f,3.0f);  if(is_inside_tree()) _apply(); }
    float get_intensity() const  { return intensity; }
    void set_size(float v)       { size = Math::clamp(v,0.0f,1.0f);       if(is_inside_tree()) _apply(); }
    float get_size() const       { return size; }
    void set_threshold(float v)  { threshold = Math::clamp(v,0.0f,8.0f);  if(is_inside_tree()) _apply(); }
    float get_threshold() const  { return threshold; }
};

// ════════════════════════════════════════════════════════════════════
//  BlurEffect — Roblox BlurEffect equivalent
//  Applies a full-scene soft blur via depth-of-field.
// ════════════════════════════════════════════════════════════════════
class BlurEffect : public Node {
    GDCLASS(BlurEffect, Node);

    float size = 0.0f;

    void _apply() {
        WorldEnvironment* we = _lx_find_we_node(this);
        if (!we) return;
        Ref<CameraAttributesPractical> cam_attr;
        cam_attr.instantiate();
        if (size < 0.01f) {
            cam_attr->set_dof_blur_far_enabled(false);
            cam_attr->set_dof_blur_near_enabled(false);
        } else {
            float amt = size * 0.5f;
            cam_attr->set_dof_blur_far_enabled(true);
            cam_attr->set_dof_blur_far_distance(1.0f + (1.0f - size) * 200.0f);
            cam_attr->set_dof_blur_far_transition(size * 50.0f);
            cam_attr->set_dof_blur_near_enabled(true);
            cam_attr->set_dof_blur_near_distance((1.0f - size) * 5.0f);
            cam_attr->set_dof_blur_near_transition(size * 20.0f);
            cam_attr->set_dof_blur_amount(amt);
        }
        we->set_camera_attributes(cam_attr);
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_size","v"), &BlurEffect::set_size);
        ClassDB::bind_method(D_METHOD("get_size"),     &BlurEffect::get_size);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Size",PROPERTY_HINT_RANGE,"0,56,0.1"),"set_size","get_size");
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _apply();
    }

public:
    void set_size(float v) { size = Math::clamp(v,0.0f,56.0f); if(is_inside_tree()) _apply(); }
    float get_size() const { return size; }
};

// ════════════════════════════════════════════════════════════════════
//  ColorCorrectionEffect — Roblox ColorCorrectionEffect equivalent
//  Controls brightness, contrast, saturation, and tint.
// ════════════════════════════════════════════════════════════════════
class ColorCorrectionEffect : public Node {
    GDCLASS(ColorCorrectionEffect, Node);

    float brightness = 1.0f;
    float contrast   = 1.0f;
    float saturation = 1.0f;
    Color tint_color = Color(1.0f, 1.0f, 1.0f);

    void _apply() {
        Ref<Environment> env = _lx_get_env(this);
        if (!env.is_valid()) return;
        bool active = (fabsf(brightness - 1.0f) > 0.001f ||
                       fabsf(contrast   - 1.0f) > 0.001f ||
                       fabsf(saturation - 1.0f) > 0.001f);
        env->set_adjustment_enabled(active);
        env->set_adjustment_brightness(brightness);
        env->set_adjustment_contrast(contrast);
        env->set_adjustment_saturation(saturation);
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_brightness","v"), &ColorCorrectionEffect::set_brightness);
        ClassDB::bind_method(D_METHOD("get_brightness"),     &ColorCorrectionEffect::get_brightness);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Brightness",PROPERTY_HINT_RANGE,"0,5,0.001"),"set_brightness","get_brightness");

        ClassDB::bind_method(D_METHOD("set_contrast","v"), &ColorCorrectionEffect::set_contrast);
        ClassDB::bind_method(D_METHOD("get_contrast"),     &ColorCorrectionEffect::get_contrast);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Contrast",PROPERTY_HINT_RANGE,"0,5,0.001"),"set_contrast","get_contrast");

        ClassDB::bind_method(D_METHOD("set_saturation","v"), &ColorCorrectionEffect::set_saturation);
        ClassDB::bind_method(D_METHOD("get_saturation"),     &ColorCorrectionEffect::get_saturation);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Saturation",PROPERTY_HINT_RANGE,"0,5,0.001"),"set_saturation","get_saturation");

        ClassDB::bind_method(D_METHOD("set_tint_color","c"), &ColorCorrectionEffect::set_tint_color);
        ClassDB::bind_method(D_METHOD("get_tint_color"),     &ColorCorrectionEffect::get_tint_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"TintColor"),"set_tint_color","get_tint_color");
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _apply();
    }

public:
    void set_brightness(float v) { brightness = Math::clamp(v,0.0f,5.0f); if(is_inside_tree()) _apply(); }
    float get_brightness() const { return brightness; }
    void set_contrast(float v)   { contrast = Math::clamp(v,0.0f,5.0f);   if(is_inside_tree()) _apply(); }
    float get_contrast() const   { return contrast; }
    void set_saturation(float v) { saturation = Math::clamp(v,0.0f,5.0f); if(is_inside_tree()) _apply(); }
    float get_saturation() const { return saturation; }
    void set_tint_color(Color c) { tint_color = c;                         if(is_inside_tree()) _apply(); }
    Color get_tint_color() const { return tint_color; }
};

// ════════════════════════════════════════════════════════════════════
//  DepthOfFieldEffect — Roblox DepthOfFieldEffect equivalent
//  Controls depth-of-field blur for near/far objects.
// ════════════════════════════════════════════════════════════════════
class DepthOfFieldEffect : public Node {
    GDCLASS(DepthOfFieldEffect, Node);

    float far_distance    = 100.0f;
    float far_transition  = 10.0f;
    float near_distance   = 2.0f;
    float near_transition = 5.0f;
    bool  enabled         = false;

    void _apply() {
        WorldEnvironment* we = _lx_find_we_node(this);
        if (!we) return;
        Ref<CameraAttributesPractical> cam_attr;
        cam_attr.instantiate();
        cam_attr->set_dof_blur_far_enabled(enabled);
        cam_attr->set_dof_blur_far_distance(far_distance);
        cam_attr->set_dof_blur_far_transition(far_transition);
        cam_attr->set_dof_blur_near_enabled(enabled);
        cam_attr->set_dof_blur_near_distance(near_distance);
        cam_attr->set_dof_blur_near_transition(near_transition);
        cam_attr->set_dof_blur_amount(0.1f);
        we->set_camera_attributes(cam_attr);
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_enabled","v"), &DepthOfFieldEffect::set_enabled);
        ClassDB::bind_method(D_METHOD("get_enabled"),     &DepthOfFieldEffect::get_enabled);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"Enabled"),"set_enabled","get_enabled");

        ClassDB::bind_method(D_METHOD("set_far_distance","v"), &DepthOfFieldEffect::set_far_distance);
        ClassDB::bind_method(D_METHOD("get_far_distance"),     &DepthOfFieldEffect::get_far_distance);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"FarDistance",PROPERTY_HINT_RANGE,"0,5000,0.1"),"set_far_distance","get_far_distance");

        ClassDB::bind_method(D_METHOD("set_far_transition","v"), &DepthOfFieldEffect::set_far_transition);
        ClassDB::bind_method(D_METHOD("get_far_transition"),     &DepthOfFieldEffect::get_far_transition);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"FarTransition",PROPERTY_HINT_RANGE,"0,1000,0.1"),"set_far_transition","get_far_transition");

        ClassDB::bind_method(D_METHOD("set_near_distance","v"), &DepthOfFieldEffect::set_near_distance);
        ClassDB::bind_method(D_METHOD("get_near_distance"),     &DepthOfFieldEffect::get_near_distance);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"NearDistance",PROPERTY_HINT_RANGE,"0,1000,0.1"),"set_near_distance","get_near_distance");

        ClassDB::bind_method(D_METHOD("set_near_transition","v"), &DepthOfFieldEffect::set_near_transition);
        ClassDB::bind_method(D_METHOD("get_near_transition"),     &DepthOfFieldEffect::get_near_transition);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"NearTransition",PROPERTY_HINT_RANGE,"0,1000,0.1"),"set_near_transition","get_near_transition");
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE) _apply();
    }

public:
    void set_enabled(bool v)         { enabled = v;                             if(is_inside_tree()) _apply(); }
    bool get_enabled() const         { return enabled; }
    void set_far_distance(float v)   { far_distance = v;                        if(is_inside_tree()) _apply(); }
    float get_far_distance() const   { return far_distance; }
    void set_far_transition(float v) { far_transition = Math::clamp(v,0.0f,1000.0f); if(is_inside_tree()) _apply(); }
    float get_far_transition() const { return far_transition; }
    void set_near_distance(float v)  { near_distance = v;                       if(is_inside_tree()) _apply(); }
    float get_near_distance() const  { return near_distance; }
    void set_near_transition(float v){ near_transition = Math::clamp(v,0.0f,1000.0f); if(is_inside_tree()) _apply(); }
    float get_near_transition() const{ return near_transition; }
};

#endif // ROBLOX_LIGHTING_FX_H
