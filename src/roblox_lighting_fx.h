#ifndef ROBLOX_LIGHTING_FX_H
#define ROBLOX_LIGHTING_FX_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/classes/procedural_sky_material.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/camera_attributes_practical.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/window.hpp>

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
    WorldEnvironment* we = _lx_find_we((Node*)self->get_tree()->get_root());
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
    return _lx_find_we((Node*)self->get_tree()->get_root());
}

// ════════════════════════════════════════════════════════════════════
//  AtmosphereNode — Roblox Atmosphere equivalent
// ════════════════════════════════════════════════════════════════════
class AtmosphereNode : public Node {
    GDCLASS(AtmosphereNode, Node);

    float density          = 0.395f;
    float offset           = 0.0f;
    Color color            = Color(0.71f, 0.81f, 0.93f);
    Color haze_color       = Color(0.80f, 0.86f, 0.95f);
    Color glare_color      = Color(1.00f, 0.90f, 0.70f);
    float decay            = 0.0f;
    float glare            = 0.0f;
    float haze             = 0.0f;
    float fog_sun_scatter  = 0.5f;
    float volumetric_dist  = 100.0f;

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
        env->set_fog_sun_scatter(fog_sun_scatter);
        env->set_fog_aerial_perspective(haze);
        Color blend = color.lerp(haze_color, haze * 0.5f);
        env->set_fog_light_color(blend);
        env->set_fog_light_energy(1.0f + glare);
        env->set_fog_sky_affect(haze * 0.6f);
        env->set_volumetric_fog_enabled(volumetric_dist < 99.0f);
        if (volumetric_dist < 99.0f) {
            env->set_volumetric_fog_density(density * 0.05f);
            env->set_volumetric_fog_albedo(blend);
            env->set_volumetric_fog_emission(glare_color * glare * 0.2f);
            env->set_volumetric_fog_length(volumetric_dist);
            env->set_volumetric_fog_detail_spread(1.0f + decay * 2.0f);
        }
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
        ClassDB::bind_method(D_METHOD("set_haze_color","c"), &AtmosphereNode::set_haze_color);
        ClassDB::bind_method(D_METHOD("get_haze_color"),     &AtmosphereNode::get_haze_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"HazeColor"),"set_haze_color","get_haze_color");
        ClassDB::bind_method(D_METHOD("set_glare_color","c"), &AtmosphereNode::set_glare_color);
        ClassDB::bind_method(D_METHOD("get_glare_color"),     &AtmosphereNode::get_glare_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"GlareColor"),"set_glare_color","get_glare_color");
        ClassDB::bind_method(D_METHOD("set_decay","v"), &AtmosphereNode::set_decay);
        ClassDB::bind_method(D_METHOD("get_decay"),     &AtmosphereNode::get_decay);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Decay",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_decay","get_decay");
        ClassDB::bind_method(D_METHOD("set_glare","v"), &AtmosphereNode::set_glare);
        ClassDB::bind_method(D_METHOD("get_glare"),     &AtmosphereNode::get_glare);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Glare",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_glare","get_glare");
        ClassDB::bind_method(D_METHOD("set_haze","v"), &AtmosphereNode::set_haze);
        ClassDB::bind_method(D_METHOD("get_haze"),     &AtmosphereNode::get_haze);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Haze",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_haze","get_haze");
        ClassDB::bind_method(D_METHOD("set_fog_sun_scatter","v"), &AtmosphereNode::set_fog_sun_scatter);
        ClassDB::bind_method(D_METHOD("get_fog_sun_scatter"),     &AtmosphereNode::get_fog_sun_scatter);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"FogSunScatter",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_fog_sun_scatter","get_fog_sun_scatter");
        ClassDB::bind_method(D_METHOD("set_volumetric_dist","v"), &AtmosphereNode::set_volumetric_dist);
        ClassDB::bind_method(D_METHOD("get_volumetric_dist"),     &AtmosphereNode::get_volumetric_dist);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"VolumetricDistance",PROPERTY_HINT_RANGE,"0,100,0.1"),"set_volumetric_dist","get_volumetric_dist");
    }
    void _notification(int p_what) { if (p_what == NOTIFICATION_ENTER_TREE) _apply(); }

public:
    void set_density(float v)         { density = Math::clamp(v,0.0f,1.0f);        if(is_inside_tree()) _apply(); }
    float get_density() const         { return density; }
    void set_offset(float v)          { offset  = Math::clamp(v,0.0f,1.0f);        if(is_inside_tree()) _apply(); }
    float get_offset() const          { return offset; }
    void set_color(Color c)           { color = c;                                  if(is_inside_tree()) _apply(); }
    Color get_color() const           { return color; }
    void set_haze_color(Color c)      { haze_color = c;                             if(is_inside_tree()) _apply(); }
    Color get_haze_color() const      { return haze_color; }
    void set_glare_color(Color c)     { glare_color = c;                            if(is_inside_tree()) _apply(); }
    Color get_glare_color() const     { return glare_color; }
    void set_decay(float v)           { decay = Math::clamp(v,0.0f,1.0f);           if(is_inside_tree()) _apply(); }
    float get_decay() const           { return decay; }
    void set_glare(float v)           { glare = Math::clamp(v,0.0f,1.0f);           if(is_inside_tree()) _apply(); }
    float get_glare() const           { return glare; }
    void set_haze(float v)            { haze  = Math::clamp(v,0.0f,1.0f);           if(is_inside_tree()) _apply(); }
    float get_haze() const            { return haze; }
    void set_fog_sun_scatter(float v) { fog_sun_scatter = Math::clamp(v,0.0f,1.0f); if(is_inside_tree()) _apply(); }
    float get_fog_sun_scatter() const { return fog_sun_scatter; }
    void set_volumetric_dist(float v) { volumetric_dist = Math::clamp(v,0.0f,100.0f); if(is_inside_tree()) _apply(); }
    float get_volumetric_dist() const { return volumetric_dist; }
};

// ════════════════════════════════════════════════════════════════════
//  LightingSkyNode — 6 sky presets with full day/night cycle shaders
//  All custom shaders react to LIGHT0_DIRECTION.y (sun elevation)
//  so moving the sun (via Lighting.ClockTime) auto-updates the sky.
// ════════════════════════════════════════════════════════════════════
class LightingSkyNode : public Node {
    GDCLASS(LightingSkyNode, Node);

public:
    enum SkyPreset { PRESET_PROCEDURAL=0, PRESET_ATMOSPHERIC, PRESET_CARTOON, PRESET_ANIME, PRESET_NIGHT, PRESET_SUNSET };

private:
    int   sky_preset        = PRESET_PROCEDURAL;
    int   _cur_preset       = -1;
    Ref<ShaderMaterial> _smat;

    // ── Core sky colors ───────────────────────────────────────────
    Color sky_color         = Color(0.06f, 0.14f, 0.55f);
    Color horizon_color     = Color(0.42f, 0.68f, 0.92f);
    Color ground_color      = Color(0.10f, 0.08f, 0.06f);
    Color dawn_color        = Color(0.90f, 0.35f, 0.12f);
    float sky_exposure      = 1.0f;
    float sky_curve         = 0.09f;
    float sun_angular_size  = 21.0f;

    // ── Atmospheric scattering ────────────────────────────────────
    float rayleigh_strength = 1.0f;
    Color rayleigh_color    = Color(0.26f, 0.47f, 1.00f);
    float mie_strength      = 0.5f;
    float mie_eccentricity  = 0.80f;
    Color mie_color         = Color(1.00f, 0.90f, 0.70f);

    // ── Horizon fog ───────────────────────────────────────────────
    Color horizon_fog_color  = Color(0.60f, 0.75f, 0.90f);
    float horizon_fog_amount = 0.30f;
    float horizon_sharpness  = 4.0f;

    // ── Sunset / dusk ─────────────────────────────────────────────
    Color sunset_color      = Color(1.00f, 0.35f, 0.08f);
    float sunset_strength   = 1.0f;
    Color sky_mid_color     = Color(0.50f, 0.20f, 0.60f);

    // ── Sun disk ─────────────────────────────────────────────────
    float sun_disk_scale    = 1.0f;
    float sun_energy        = 1.0f;
    float sun_corona_size   = 1.0f;
    Color sun_disk_color    = Color(1.00f, 0.95f, 0.80f);
    float ground_energy     = 0.5f;

    // ── Stars ─────────────────────────────────────────────────────
    float star_density      = 30.0f;
    float star_brightness   = 0.80f;
    float star_size         = 1.0f;
    float star_twinkle_speed= 1.0f;

    // ── Night-specific ────────────────────────────────────────────
    Color moon_color        = Color(0.95f, 0.95f, 0.85f);
    float moon_size         = 1.0f;
    float moon_energy       = 1.0f;
    Color nebula_color      = Color(0.20f, 0.10f, 0.30f);
    float nebula_strength   = 0.30f;
    float milky_way_strength= 0.50f;

    // ── Clouds ───────────────────────────────────────────────────
    float cloud_amount      = 0.0f;
    Color cloud_color       = Color(1.00f, 0.98f, 0.95f);

    // ── Global ────────────────────────────────────────────────────
    float sky_saturation    = 1.0f;

    // ─────────────────────────────────────────────────────────────
    //  SHADER: ATMOSPHERIC
    //  Full day/night cycle via LIGHT0_DIRECTION.y
    //  Dawn/dusk warm tones, stars at night, FBM clouds, ozone layer
    // ─────────────────────────────────────────────────────────────
    static const char* _shader_atmospheric() { return
R"SHADER(shader_type sky;
render_mode use_half_res_pass;
const float PI = 3.14159265359;
uniform vec3 sky_top_color : source_color = vec3(0.06,0.14,0.55);
uniform vec3 sky_horizon_color : source_color = vec3(0.42,0.68,0.92);
uniform vec3 dawn_color : source_color = vec3(0.90,0.35,0.12);
uniform vec3 ground_color : source_color = vec3(0.08,0.07,0.05);
uniform float rayleigh_strength : hint_range(0.0,5.0) = 1.0;
uniform vec3 rayleigh_color : source_color = vec3(0.26,0.47,1.00);
uniform float mie_strength : hint_range(0.0,5.0) = 0.5;
uniform float mie_eccentricity : hint_range(-0.99,0.99) = 0.80;
uniform vec3 mie_color : source_color = vec3(1.00,0.90,0.70);
uniform vec3 horizon_fog_color : source_color = vec3(0.60,0.75,0.90);
uniform float horizon_fog_amount : hint_range(0.0,1.0) = 0.30;
uniform float horizon_sharpness : hint_range(0.5,20.0) = 4.0;
uniform vec3 sunset_color : source_color = vec3(1.00,0.35,0.08);
uniform float sunset_strength : hint_range(0.0,3.0) = 1.0;
uniform float sun_disk_scale : hint_range(0.0,10.0) = 1.0;
uniform float sun_energy : hint_range(0.0,16.0) = 1.0;
uniform float sun_corona_size : hint_range(0.0,5.0) = 1.0;
uniform vec3 sun_disk_color : source_color = vec3(1.00,0.95,0.80);
uniform float ground_energy : hint_range(0.0,4.0) = 0.5;
uniform float star_density : hint_range(0.0,100.0) = 30.0;
uniform float star_brightness : hint_range(0.0,2.0) = 0.8;
uniform float star_size : hint_range(0.1,5.0) = 1.0;
uniform float star_twinkle_speed : hint_range(0.0,5.0) = 1.0;
uniform float cloud_amount : hint_range(0.0,1.0) = 0.0;
uniform vec3 cloud_color : source_color = vec3(1.0,0.98,0.95);
uniform float sky_exposure : hint_range(0.0,8.0) = 1.0;
uniform float sky_saturation : hint_range(0.0,3.0) = 1.0;

float hash21(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise2(vec2 p){
    vec2 i=floor(p);vec2 f=fract(p);f=f*f*(3.0-2.0*f);
    return mix(mix(hash21(i),hash21(i+vec2(1,0)),f.x),
               mix(hash21(i+vec2(0,1)),hash21(i+vec2(1,1)),f.x),f.y);
}
float fbm(vec2 p){
    return noise2(p)*0.500+noise2(p*2.1)*0.250
          +noise2(p*4.2)*0.125+noise2(p*8.4)*0.0625;
}
float mie_phase(float c,float g){
    float g2=g*g;
    return(1.0-g2)/(4.0*PI*pow(max(1.0+g2-2.0*g*c,0.0001),1.5));
}
float rayleigh_phase(float c){return(3.0/(16.0*PI))*(1.0+c*c);}
float star_field(vec3 d,float t){
    if(d.y<-0.05)return 0.0;
    float u=atan(d.z,d.x)/(2.0*PI)+0.5;
    float v=asin(clamp(d.y,-1.0,1.0))/PI+0.5;
    vec2 uv=vec2(u,v)*(50.0*star_size);
    vec2 id=floor(uv);vec2 fd=fract(uv)-0.5;
    float h=hash21(id);float h2=hash21(id+vec2(3.7,8.1));
    if(h>star_density*0.01)return 0.0;
    float twinkle=0.7+0.3*sin(t*star_twinkle_speed*(1.0+h2*4.0));
    float d2=length(fd);
    return max(0.0,0.003/(d2*d2+0.0002))*twinkle*star_brightness;
}
void sky(){
    vec3 dir=EYEDIR;
    if(dir.y<-0.001){COLOR=ground_color*ground_energy;return;}
    float alt=max(dir.y,0.0);
    vec3 sun_dir=LIGHT0_DIRECTION;
    float cos_t=clamp(dot(dir,sun_dir),-1.0,1.0);
    float sun_elev=sun_dir.y;

    // Time factors driven by sun elevation
    float t_day   =clamp(sun_elev*2.5+0.5,0.0,1.0);
    float t_sunset=pow(max(0.0,1.0-abs(sun_elev)*6.0),1.5)*step(-0.15,sun_elev);
    float t_night =clamp(-sun_elev*4.0,0.0,1.0);

    // Sky gradient colors transition with time of day
    vec3 top  =mix(mix(sky_top_color, vec3(0.06,0.06,0.22),t_sunset*0.6),vec3(0.006,0.012,0.05),t_night);
    vec3 horiz=mix(mix(sky_horizon_color,dawn_color,t_sunset),vec3(0.02,0.03,0.09),t_night);

    float sky_t=pow(1.0-alt,2.5);
    vec3 col=mix(top,horiz,clamp(sky_t,0.0,1.0));

    // Rayleigh + Mie scattering (day only)
    col+=rayleigh_color*rayleigh_phase(cos_t)*rayleigh_strength*t_day*0.4;
    col+=mie_color*mie_phase(cos_t,mie_eccentricity)*mie_strength*t_day*0.2;

    // Horizon haze (warm tint at dusk)
    float horiz_f=pow(max(0.0,1.0-alt*horizon_sharpness),5.0);
    vec3 fog_tint=mix(horizon_fog_color,sunset_color*0.9,t_sunset);
    col=mix(col,fog_tint,horiz_f*horizon_fog_amount*(1.0-t_night*0.7));

    // Sunset glow cone around sun
    float ss_cone=pow(max(0.0,cos_t+0.2),3.0);
    float ss_low =pow(max(0.0,1.0-alt*5.0),3.0);
    col=mix(col,sunset_color,ss_cone*ss_low*t_sunset*sunset_strength*0.6);

    // Ozone layer (subtle blue-green at zenith during day)
    col+=vec3(-0.015,0.025,0.04)*pow(alt,1.5)*t_day*0.2;

    // FBM clouds (lit by time of day)
    if(cloud_amount>0.0&&alt>0.005){
        vec2 cu=vec2(atan(dir.z,dir.x)/6.28318+0.5,alt)*vec2(2.8,2.0);
        float cn=fbm(cu*2.5);
        float cloud=smoothstep(1.0-cloud_amount*0.65,1.0,cn)*clamp(alt*6.0,0.0,1.0);
        vec3 cloud_lit=mix(cloud_color*0.55,cloud_color,t_day);
        cloud_lit=mix(cloud_lit,sunset_color*0.9+cloud_color*0.6,t_sunset*0.55);
        col=mix(col,cloud_lit,cloud*0.88);
    }

    // Stars appear as sun sets
    if(star_density>0.0&&star_brightness>0.0){
        float sv=clamp(0.6-sun_elev*5.0,0.0,1.0);sv*=sv;
        if(sv>0.001)col+=vec3(star_field(dir,TIME))*sv;
    }

    // Sun disk — color shifts orange-red near horizon
    if(LIGHT0_ENABLED){
        float sun_h=1.0-clamp(sun_elev*4.0+0.3,0.0,1.0);
        vec3 act_sun=mix(sun_disk_color,vec3(1.0,0.42,0.06),sun_h*0.85);
        float sa=acos(clamp(cos_t,-1.0,1.0));
        float sr=0.018*max(sun_disk_scale,0.001);
        float cr=sr*(1.0+sun_corona_size*3.0);
        float corona=exp(-sa*sa/max(cr*cr,0.0001));
        col+=act_sun*LIGHT0_COLOR*LIGHT0_ENERGY*corona*sun_energy*0.45;
        if(sa<sr){
            float tt=sa/max(sr,0.0001);
            float limb=mix(0.5,1.0,sqrt(max(0.0,1.0-tt*tt)));
            col+=act_sun*LIGHT0_COLOR*LIGHT0_ENERGY*limb*sun_energy*6.0;
        }
    }

    col*=sky_exposure;
    float luma=dot(col,vec3(0.299,0.587,0.114));
    col=mix(vec3(luma),col,sky_saturation);
    COLOR=max(col,vec3(0.0));
}
)SHADER"; }

    // ─────────────────────────────────────────────────────────────
    //  SHADER: CARTOON  — cel-shaded with day/night + FBM clouds
    // ─────────────────────────────────────────────────────────────
    static const char* _shader_cartoon() { return
R"SHADER(shader_type sky;
const float PI=3.14159265359;
uniform vec3 sky_top_color : source_color = vec3(0.20,0.50,1.00);
uniform vec3 sky_horizon_color : source_color = vec3(0.70,0.85,1.00);
uniform vec3 night_sky_color : source_color = vec3(0.03,0.04,0.16);
uniform vec3 ground_color : source_color = vec3(0.28,0.52,0.18);
uniform vec3 sun_disk_color : source_color = vec3(1.00,0.95,0.70);
uniform vec3 sunset_color : source_color = vec3(1.00,0.50,0.10);
uniform vec3 dawn_color : source_color = vec3(0.90,0.35,0.12);
uniform float sun_disk_scale : hint_range(0.0,10.0) = 2.0;
uniform float sun_energy : hint_range(0.0,8.0) = 1.0;
uniform float sky_exposure : hint_range(0.0,4.0) = 1.0;
uniform vec3 cloud_color : source_color = vec3(1.00,1.00,1.00);
uniform float cloud_amount : hint_range(0.0,1.0) = 0.3;
uniform float star_density : hint_range(0.0,100.0) = 40.0;
uniform float star_brightness : hint_range(0.0,2.0) = 0.8;

float hash21c(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise2c(vec2 p){
    vec2 i=floor(p);vec2 f=fract(p);f=f*f*(3.0-2.0*f);
    return mix(mix(hash21c(i),hash21c(i+vec2(1,0)),f.x),
               mix(hash21c(i+vec2(0,1)),hash21c(i+vec2(1,1)),f.x),f.y);
}
float fbmc(vec2 p){
    return noise2c(p)*0.5+noise2c(p*2.1)*0.25+noise2c(p*4.2)*0.125;
}
void sky(){
    vec3 dir=EYEDIR;
    if(dir.y<0.0){COLOR=ground_color*sky_exposure;return;}
    float sun_elev=LIGHT0_DIRECTION.y;
    float t_day   =clamp(sun_elev*2.5+0.5,0.0,1.0);
    float t_sunset=pow(max(0.0,1.0-abs(sun_elev)*5.0),2.0)*step(-0.2,sun_elev);
    float t_night =clamp(-sun_elev*4.0,0.0,1.0);
    float t=pow(clamp(dir.y,0.0,1.0),0.5);

    // Sky colors
    vec3 sky_top =mix(sky_top_color,night_sky_color,t_night);
    vec3 sky_horiz=mix(sky_horizon_color,mix(sky_horizon_color,dawn_color,t_sunset),1.0-t_day*0.5);
    sky_horiz=mix(sky_horiz,night_sky_color*1.5,t_night);
    vec3 col=mix(sky_horiz,sky_top,t);

    // Cel-shading bands
    float band=floor(t*5.0)/5.0;
    col*=0.93+0.07*smoothstep(band,band+0.04,t);

    // FBM clouds
    if(cloud_amount>0.0&&dir.y>0.01){
        vec2 cu=vec2(atan(dir.z,dir.x)/6.28318+0.5,dir.y)*vec2(3.0,2.0);
        float cn=fbmc(cu*3.0);
        float cloud=smoothstep(1.0-cloud_amount*0.7,1.0,cn)*clamp(dir.y*4.0,0.0,1.0);
        vec3 cloud_lit=mix(cloud_color*0.6,cloud_color,t_day);
        cloud_lit=mix(cloud_lit,sunset_color*0.8+cloud_color*0.5,t_sunset*0.5);
        col=mix(col,cloud_lit,cloud*0.9);
    }

    // Stars at night
    if(t_night>0.05&&star_density>0.0){
        vec2 uv=vec2(atan(dir.z,dir.x)/(2.0*PI)+0.5,
                     asin(clamp(dir.y,-1.0,1.0))/PI+0.5)*50.0;
        vec2 id=floor(uv);vec2 fd=fract(uv)-0.5;
        float h=hash21c(id);
        if(h<=star_density*0.01){
            float d=length(fd);
            col+=vec3(max(0.0,0.004/(d*d+0.0003)))*star_brightness*t_night;
        }
    }

    // Sun disk (color shifts at horizon)
    if(LIGHT0_ENABLED){
        float cos_t=dot(dir,LIGHT0_DIRECTION);
        float sa=acos(clamp(cos_t,-1.0,1.0));
        float sr=0.04*sun_disk_scale;
        if(sa<sr*1.3){
            float h2=smoothstep(sr*1.3,sr*0.7,sa);
            float sun_h=1.0-clamp(sun_elev*3.0+0.3,0.0,1.0);
            vec3 sun_col=mix(sun_disk_color,sunset_color,sun_h*0.8);
            col=mix(col,sun_col*sun_energy*LIGHT0_ENERGY,h2*max(t_day,0.1));
        }
    }
    COLOR=col*sky_exposure;
}
)SHADER"; }

    // ─────────────────────────────────────────────────────────────
    //  SHADER: ANIME  — pastel gradient bands, full day/night cycle
    // ─────────────────────────────────────────────────────────────
    static const char* _shader_anime() { return
R"SHADER(shader_type sky;
const float PI=3.14159265359;
uniform vec3 sky_top_color : source_color = vec3(0.25,0.50,0.90);
uniform vec3 sky_horizon_color : source_color = vec3(0.80,0.85,1.00);
uniform vec3 night_top_color : source_color = vec3(0.02,0.03,0.12);
uniform vec3 ground_color : source_color = vec3(0.40,0.35,0.30);
uniform vec3 dawn_color : source_color = vec3(0.90,0.35,0.12);
uniform vec3 sunset_color : source_color = vec3(1.00,0.55,0.25);
uniform float sunset_strength : hint_range(0.0,2.0) = 1.0;
uniform vec3 sun_disk_color : source_color = vec3(1.00,0.98,0.90);
uniform float sun_disk_scale : hint_range(0.0,10.0) = 1.5;
uniform float sun_energy : hint_range(0.0,8.0) = 1.0;
uniform float sky_exposure : hint_range(0.0,4.0) = 1.0;
uniform float star_density : hint_range(0.0,100.0) = 25.0;
uniform float star_brightness : hint_range(0.0,2.0) = 0.7;
uniform vec3 cloud_color : source_color = vec3(1.00,0.95,0.90);
uniform float cloud_amount : hint_range(0.0,1.0) = 0.2;

float hash21a(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise2a(vec2 p){
    vec2 i=floor(p);vec2 f=fract(p);f=f*f*(3.0-2.0*f);
    return mix(mix(hash21a(i),hash21a(i+vec2(1,0)),f.x),
               mix(hash21a(i+vec2(0,1)),hash21a(i+vec2(1,1)),f.x),f.y);
}
float fbma(vec2 p){
    return noise2a(p)*0.5+noise2a(p*2.1)*0.25+noise2a(p*4.2)*0.125;
}
void sky(){
    vec3 dir=EYEDIR;
    if(dir.y<-0.05){COLOR=ground_color*sky_exposure;return;}
    float sun_elev=LIGHT0_DIRECTION.y;
    float t_day   =clamp(sun_elev*2.5+0.5,0.0,1.0);
    float t_sunset=pow(max(0.0,1.0-abs(sun_elev)*5.0),1.5)*step(-0.15,sun_elev);
    float t_night =clamp(-sun_elev*3.5,0.0,1.0);
    float alt=clamp(dir.y,0.0,1.0);
    float t1=clamp(alt*2.5,0.0,1.0);

    vec3 top  =mix(sky_top_color,night_top_color,t_night);
    vec3 horiz=mix(sky_horizon_color,
                   mix(sky_horizon_color,dawn_color,t_sunset*1.2),1.0-t_day*0.5);
    horiz=mix(horiz,vec3(0.03,0.04,0.10),t_night);
    vec3 col=mix(horiz,top,t1);

    // Anime gradient bands
    col*=0.94+0.06*smoothstep(0.0,0.09,fract(alt*6.0));

    if(LIGHT0_ENABLED){
        float cos_t=dot(dir,LIGHT0_DIRECTION);

        // Sunset warm spread near sun at horizon
        float ns=pow(max(0.0,cos_t),3.0)*pow(max(0.0,1.0-alt*4.0),2.0);
        col=mix(col,sunset_color,ns*t_sunset*sunset_strength*0.7);

        // Stars at night
        float sv=t_night*clamp(alt+0.1,0.0,1.0);
        if(star_density>0.0&&sv>0.01){
            vec2 uv=vec2(atan(dir.z,dir.x)/(2.0*PI)+0.5,
                         asin(clamp(dir.y,-1.0,1.0))/PI+0.5)*40.0;
            vec2 id=floor(uv);vec2 fd=fract(uv)-0.5;
            float h2=hash21a(id);
            if(h2<=star_density*0.01){
                float d=length(fd);
                col+=vec3(max(0.0,0.004/(d*d+0.0003)))*star_brightness*sv;
            }
        }

        // Sun disk (fades at night)
        float sa=acos(clamp(cos_t,-1.0,1.0));
        float sr=0.025*sun_disk_scale;
        if(sa<sr*1.5&&t_day>0.05){
            float h2=smoothstep(sr*1.5,sr*0.6,sa);
            float sun_h=1.0-clamp(sun_elev*3.0+0.3,0.0,1.0);
            vec3 sun_col=mix(sun_disk_color,sunset_color,sun_h*0.75);
            col=mix(col,sun_col*sun_energy*LIGHT0_ENERGY,h2*0.9);
        }
    }

    // Soft clouds
    if(cloud_amount>0.0&&alt>0.02){
        vec2 cu=vec2(atan(dir.z,dir.x)/6.28318+0.5,alt)*vec2(2.5,3.0);
        float cn=fbma(cu*2.5);
        float cloud=smoothstep(1.0-cloud_amount*0.6,1.0,cn)*clamp(alt*5.0,0.0,1.0);
        vec3 cloud_lit=mix(cloud_color*0.6,cloud_color,t_day);
        cloud_lit=mix(cloud_lit,sunset_color*0.9+cloud_color*0.5,t_sunset*0.5);
        col=mix(col,cloud_lit,cloud*0.65);
    }

    COLOR=col*sky_exposure;
}
)SHADER"; }

    // ─────────────────────────────────────────────────────────────
    //  SHADER: NIGHT  — deep starfield, Milky Way, moon with craters
    //  Dawn horizon glow appears when sun rises below horizon
    // ─────────────────────────────────────────────────────────────
    static const char* _shader_night() { return
R"SHADER(shader_type sky;
render_mode use_half_res_pass;
const float PI=3.14159265359;
uniform vec3 sky_top_color : source_color = vec3(0.006,0.012,0.06);
uniform vec3 sky_horizon_color : source_color = vec3(0.025,0.038,0.10);
uniform vec3 ground_color : source_color = vec3(0.012,0.012,0.022);
uniform float star_density : hint_range(0.0,100.0) = 65.0;
uniform float star_brightness : hint_range(0.0,3.0) = 1.2;
uniform float star_size : hint_range(0.1,5.0) = 1.0;
uniform float star_twinkle_speed : hint_range(0.0,5.0) = 1.5;
uniform vec3 moon_color : source_color = vec3(0.95,0.95,0.85);
uniform float moon_size : hint_range(0.0,5.0) = 1.0;
uniform float moon_energy : hint_range(0.0,4.0) = 1.0;
uniform vec3 nebula_color : source_color = vec3(0.20,0.10,0.30);
uniform float nebula_strength : hint_range(0.0,1.0) = 0.30;
uniform float milky_way_strength : hint_range(0.0,2.0) = 0.50;
uniform vec3 dawn_color : source_color = vec3(0.90,0.35,0.12);
uniform float sky_exposure : hint_range(0.0,4.0) = 1.0;

float hash21n(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise2n(vec2 p){
    vec2 i=floor(p);vec2 f=fract(p);f=f*f*(3.0-2.0*f);
    return mix(mix(hash21n(i),hash21n(i+vec2(1,0)),f.x),
               mix(hash21n(i+vec2(0,1)),hash21n(i+vec2(1,1)),f.x),f.y);
}
float stars(vec3 dir,float time){
    if(dir.y<-0.1)return 0.0;
    float u=atan(dir.z,dir.x)/(2.0*PI)+0.5;
    float v=asin(clamp(dir.y,-1.0,1.0))/PI+0.5;
    vec2 uv=vec2(u,v)*(60.0*star_size);
    vec2 id=floor(uv);vec2 fd=fract(uv)-0.5;
    float h=hash21n(id);float h2=hash21n(id+vec2(5.3,9.1));
    if(h>star_density*0.01)return 0.0;
    float twinkle=0.6+0.4*sin(time*star_twinkle_speed*(1.0+h2*5.0));
    float d=length(fd);
    return max(0.0,0.004/(d*d+0.0002))*twinkle*star_brightness;
}
float milky_way_band(vec3 dir){
    float b=atan(dir.z,dir.x)/PI;
    float band=abs(dir.y*2.0-b*0.2);
    float mw=noise2n(vec2(b*4.0,dir.y*8.0))*0.5
             +noise2n(vec2(b*8.0,dir.y*16.0))*0.25
             +noise2n(vec2(b*16.0,dir.y*32.0))*0.125;
    return mw*max(0.0,1.0-band*5.0);
}
void sky(){
    vec3 dir=EYEDIR;
    if(dir.y<-0.05){COLOR=ground_color*sky_exposure;return;}
    float alt=clamp(dir.y,0.0,1.0);
    vec3 col=mix(sky_horizon_color,sky_top_color,pow(alt,0.5));

    col+=vec3(stars(dir,TIME));

    if(milky_way_strength>0.0&&alt>0.0){
        float mw=milky_way_band(dir);
        col+=nebula_color*mw*nebula_strength;
        col+=vec3(mw*0.08*milky_way_strength);
    }

    if(LIGHT0_ENABLED){
        // Moon appears at position opposite to sun direction
        vec3 moon_dir=-LIGHT0_DIRECTION;
        float cos_m=dot(dir,moon_dir);
        float ma=acos(clamp(cos_m,-1.0,1.0));
        float mr=0.015*moon_size;
        if(ma<mr*1.5){
            float mh=smoothstep(mr*1.5,mr*0.7,ma);
            float tt=ma/max(mr,0.0001);
            float limb=mix(0.55,1.0,sqrt(max(0.0,1.0-tt*tt)));
            // Subtle crater texture using noise
            float crater=noise2n(dir.xz*80.0+vec2(1.3,2.7))*0.12+0.88;
            col=mix(col,moon_color*limb*moon_energy*crater,mh);
        }
        // Moon glow halo
        float mg=exp(-ma*ma/0.04);
        col+=moon_color*mg*moon_energy*0.05;

        // Dawn/dusk horizon glow when sun is near horizon below us
        float sun_elev=LIGHT0_DIRECTION.y;
        float dawn_factor=clamp(sun_elev*12.0+0.9,0.0,1.0)*pow(max(0.0,1.0-alt*10.0),2.5);
        if(dawn_factor>0.001){
            float cos_dawn=dot(dir,vec3(LIGHT0_DIRECTION.x,0.0,LIGHT0_DIRECTION.z));
            col=mix(col,dawn_color,dawn_factor*pow(max(0.0,cos_dawn),2.0)*0.7);
        }
    }

    col*=sky_exposure;
    COLOR=max(col,vec3(0.0));
}
)SHADER"; }

    // ─────────────────────────────────────────────────────────────
    //  SHADER: SUNSET  — dramatic tricolor sky, full day/night cycle
    //  Starts warm at sunset, transitions to night with moon
    // ─────────────────────────────────────────────────────────────
    static const char* _shader_sunset() { return
R"SHADER(shader_type sky;
render_mode use_half_res_pass;
const float PI=3.14159265359;
uniform vec3 zenith_color : source_color = vec3(0.05,0.10,0.35);
uniform vec3 sky_mid_color : source_color = vec3(0.50,0.20,0.60);
uniform vec3 sky_horizon_color : source_color = vec3(1.00,0.45,0.10);
uniform vec3 sun_glow_color : source_color = vec3(1.00,0.80,0.20);
uniform vec3 night_top_color : source_color = vec3(0.008,0.012,0.05);
uniform vec3 dawn_color : source_color = vec3(0.90,0.35,0.12);
uniform vec3 ground_color : source_color = vec3(0.08,0.05,0.04);
uniform float sun_disk_scale : hint_range(0.0,10.0) = 1.5;
uniform float sun_energy : hint_range(0.0,16.0) = 1.5;
uniform vec3 sun_disk_color : source_color = vec3(1.00,0.85,0.50);
uniform float sun_corona_size : hint_range(0.0,5.0) = 2.0;
uniform vec3 horizon_fog_color : source_color = vec3(1.00,0.60,0.30);
uniform float horizon_fog_amount : hint_range(0.0,1.0) = 0.6;
uniform float sky_exposure : hint_range(0.0,8.0) = 1.0;
uniform float star_density : hint_range(0.0,100.0) = 25.0;
uniform float star_brightness : hint_range(0.0,2.0) = 0.6;
uniform float cloud_amount : hint_range(0.0,1.0) = 0.15;
uniform vec3 cloud_color : source_color = vec3(1.00,0.70,0.50);

float hash21s(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise2s(vec2 p){
    vec2 i=floor(p);vec2 f=fract(p);f=f*f*(3.0-2.0*f);
    return mix(mix(hash21s(i),hash21s(i+vec2(1,0)),f.x),
               mix(hash21s(i+vec2(0,1)),hash21s(i+vec2(1,1)),f.x),f.y);
}
float fbms(vec2 p){
    return noise2s(p)*0.5+noise2s(p*2.1)*0.25+noise2s(p*4.2)*0.125;
}
void sky(){
    vec3 dir=EYEDIR;
    if(dir.y<-0.02){COLOR=ground_color*sky_exposure;return;}
    float sun_elev=LIGHT0_DIRECTION.y;
    float t_day   =clamp(sun_elev*2.5+0.5,0.0,1.0);
    float t_sunset=pow(max(0.0,1.0-abs(sun_elev)*5.5),1.5)*step(-0.15,sun_elev);
    float t_night =clamp(-sun_elev*4.0,0.0,1.0);
    float alt=clamp(dir.y,0.0,1.0);

    // Tricolor sunset gradient
    float t1=clamp(alt*3.0,0.0,1.0);
    float t2=clamp((alt-0.33)*3.0,0.0,1.0);
    vec3 horizon=mix(mix(sky_horizon_color,dawn_color,t_sunset*0.5),vec3(0.03,0.04,0.10),t_night);
    vec3 mid    =mix(sky_mid_color,vec3(0.04,0.04,0.15),t_night);
    vec3 top    =mix(zenith_color,night_top_color,t_night);
    vec3 col=mix(horizon,mid,t1);
    col=mix(col,top,t2);

    // Horizon fog
    float horiz=pow(max(0.0,1.0-alt*8.0),4.0);
    vec3 fog_h=mix(horizon_fog_color,vec3(0.03,0.04,0.10),t_night);
    col=mix(col,fog_h,horiz*horizon_fog_amount);

    if(LIGHT0_ENABLED){
        float cos_t=dot(dir,LIGHT0_DIRECTION);
        // Sun glow spread
        float glow=pow(max(0.0,cos_t),5.0)*clamp(1.0-alt*3.0,0.0,1.0)*(1.0-t_night);
        col=mix(col,sun_glow_color,glow*0.6);

        // Sun disk with horizon color shift
        float sa=acos(clamp(cos_t,-1.0,1.0));
        float sr=0.02*sun_disk_scale;
        float cr=sr*(1.0+sun_corona_size*2.0);
        float corona=exp(-sa*sa/max(cr*cr,0.0001));
        float sun_h=1.0-clamp(sun_elev*4.0+0.3,0.0,1.0);
        vec3 act_sun=mix(sun_disk_color,vec3(1.0,0.42,0.08),sun_h*0.8);
        col+=act_sun*LIGHT0_ENERGY*corona*sun_energy*0.6*(1.0-t_night);
        if(sa<sr){
            float tt=sa/max(sr,0.0001);
            float limb=mix(0.4,1.0,sqrt(max(0.0,1.0-tt*tt)));
            col+=act_sun*LIGHT0_ENERGY*limb*sun_energy*4.0*(1.0-t_night);
        }

        // Moon at night
        if(t_night>0.1){
            vec3 moon_dir=-LIGHT0_DIRECTION;
            float cos_m=dot(dir,moon_dir);
            float ma=acos(clamp(cos_m,-1.0,1.0));
            float mr=0.018;
            if(ma<mr*1.4){
                float mh=smoothstep(mr*1.4,mr*0.8,ma);
                col=mix(col,vec3(0.95,0.95,0.85)*0.9,mh*t_night);
            }
            float mg=exp(-ma*ma/0.05);
            col+=vec3(0.95,0.95,0.85)*mg*0.04*t_night;
        }
    }

    // Stars
    float sv=clamp(0.6-sun_elev*5.0,0.0,1.0);sv*=sv;
    if(star_density>0.0&&sv>0.001){
        vec2 uv=vec2(atan(dir.z,dir.x)/(2.0*PI)+0.5,
                     asin(clamp(dir.y,-1.0,1.0))/PI+0.5)*50.0;
        vec2 id=floor(uv);vec2 fd=fract(uv)-0.5;
        float h=hash21s(id);
        if(h<=star_density*0.01){
            float d=length(fd);
            col+=vec3(max(0.0,0.003/(d*d+0.0002)))*star_brightness*sv;
        }
    }

    // Warm sunset clouds
    if(cloud_amount>0.0&&alt>0.01){
        vec2 cu=vec2(atan(dir.z,dir.x)/6.28318+0.5,alt)*vec2(2.8,2.0);
        float cn=fbms(cu*2.5);
        float cloud=smoothstep(1.0-cloud_amount*0.65,1.0,cn)*clamp(alt*6.0,0.0,1.0);
        col=mix(col,cloud_color,cloud*0.85*(1.0-t_night*0.8));
    }

    col*=sky_exposure;
    COLOR=max(col,vec3(0.0));
}
)SHADER"; }

    static const char* _shader_for_preset(int p) {
        switch (p) {
            case PRESET_ATMOSPHERIC: return _shader_atmospheric();
            case PRESET_CARTOON:     return _shader_cartoon();
            case PRESET_ANIME:       return _shader_anime();
            case PRESET_NIGHT:       return _shader_night();
            case PRESET_SUNSET:      return _shader_sunset();
            default: return _shader_atmospheric();
        }
    }

    void _apply_procedural() {
        Ref<Environment> env = _lx_get_env(this);
        if (!env.is_valid()) return;
        Ref<Sky> sky_obj = env->get_sky();
        if (!sky_obj.is_valid()) return;
        Ref<Material> mat = sky_obj->get_material();
        Ref<ProceduralSkyMaterial> pmat = mat;
        if (!pmat.is_valid()) {
            pmat.instantiate();
            sky_obj->set_material(pmat);
        }
        pmat->set_sky_top_color(sky_color);
        pmat->set_sky_horizon_color(horizon_color);
        pmat->set_sky_curve(sky_curve);
        pmat->set_ground_bottom_color(ground_color);
        pmat->set_ground_horizon_color(horizon_color.lightened(0.15f));
        pmat->set_sun_angle_max(sun_angular_size);
        pmat->set_sky_energy_multiplier(sky_exposure);
        pmat->set_energy_multiplier(sky_exposure);
    }

    void _update_shader_uniforms() {
        if (!_smat.is_valid()) return;
        _smat->set_shader_parameter("sky_top_color",      sky_color);
        _smat->set_shader_parameter("sky_horizon_color",  horizon_color);
        _smat->set_shader_parameter("sky_mid_color",      sky_mid_color);
        _smat->set_shader_parameter("zenith_color",       sky_color);
        _smat->set_shader_parameter("dawn_color",         dawn_color);
        _smat->set_shader_parameter("ground_color",       ground_color);
        _smat->set_shader_parameter("rayleigh_strength",  rayleigh_strength);
        _smat->set_shader_parameter("rayleigh_color",     rayleigh_color);
        _smat->set_shader_parameter("mie_strength",       mie_strength);
        _smat->set_shader_parameter("mie_eccentricity",   mie_eccentricity);
        _smat->set_shader_parameter("mie_color",          mie_color);
        _smat->set_shader_parameter("horizon_fog_color",  horizon_fog_color);
        _smat->set_shader_parameter("horizon_fog_amount", horizon_fog_amount);
        _smat->set_shader_parameter("horizon_sharpness",  horizon_sharpness);
        _smat->set_shader_parameter("sunset_color",       sunset_color);
        _smat->set_shader_parameter("sunset_strength",    sunset_strength);
        _smat->set_shader_parameter("sun_glow_color",     sun_disk_color);
        _smat->set_shader_parameter("sun_disk_scale",     sun_disk_scale);
        _smat->set_shader_parameter("sun_energy",         sun_energy);
        _smat->set_shader_parameter("sun_corona_size",    sun_corona_size);
        _smat->set_shader_parameter("sun_disk_color",     sun_disk_color);
        _smat->set_shader_parameter("ground_energy",      ground_energy);
        _smat->set_shader_parameter("star_density",       star_density);
        _smat->set_shader_parameter("star_brightness",    star_brightness);
        _smat->set_shader_parameter("star_size",          star_size);
        _smat->set_shader_parameter("star_twinkle_speed", star_twinkle_speed);
        _smat->set_shader_parameter("cloud_amount",       cloud_amount);
        _smat->set_shader_parameter("cloud_color",        cloud_color);
        _smat->set_shader_parameter("sky_exposure",       sky_exposure);
        _smat->set_shader_parameter("sky_saturation",     sky_saturation);
        _smat->set_shader_parameter("moon_color",         moon_color);
        _smat->set_shader_parameter("moon_size",          moon_size);
        _smat->set_shader_parameter("moon_energy",        moon_energy);
        _smat->set_shader_parameter("nebula_color",       nebula_color);
        _smat->set_shader_parameter("nebula_strength",    nebula_strength);
        _smat->set_shader_parameter("milky_way_strength", milky_way_strength);
        _smat->set_shader_parameter("night_sky_color",    Color(0.03f, 0.04f, 0.16f));
        _smat->set_shader_parameter("night_top_color",    Color(0.02f, 0.03f, 0.12f));
    }

    void _apply() {
        if (!is_inside_tree()) return;
        if (sky_preset == PRESET_PROCEDURAL) {
            _cur_preset = PRESET_PROCEDURAL;
            _smat = Ref<ShaderMaterial>();
            _apply_procedural();
            return;
        }
        bool needs_rebuild = (sky_preset != _cur_preset || !_smat.is_valid());
        if (needs_rebuild) {
            _cur_preset = sky_preset;
            Ref<Shader> sh;
            sh.instantiate();
            sh->set_code(String(_shader_for_preset(sky_preset)));
            _smat.instantiate();
            _smat->set_shader(sh);
        }
        Ref<Environment> env = _lx_get_env(this);
        if (env.is_valid()) {
            Ref<Sky> sky_obj = env->get_sky();
            if (sky_obj.is_valid()) sky_obj->set_material(_smat);
        }
        _update_shader_uniforms();
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_sky_preset","v"), &LightingSkyNode::set_sky_preset);
        ClassDB::bind_method(D_METHOD("get_sky_preset"),     &LightingSkyNode::get_sky_preset);
        ADD_PROPERTY(PropertyInfo(Variant::INT,"SkyPreset",PROPERTY_HINT_ENUM,
            "Procedural,Atmospheric,Cartoon,Anime,Night,Sunset"),"set_sky_preset","get_sky_preset");

        ClassDB::bind_method(D_METHOD("set_sky_color","c"),    &LightingSkyNode::set_sky_color);
        ClassDB::bind_method(D_METHOD("get_sky_color"),        &LightingSkyNode::get_sky_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"SkyColor"),"set_sky_color","get_sky_color");
        ClassDB::bind_method(D_METHOD("set_horizon_color","c"),&LightingSkyNode::set_horizon_color);
        ClassDB::bind_method(D_METHOD("get_horizon_color"),    &LightingSkyNode::get_horizon_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"HorizonColor"),"set_horizon_color","get_horizon_color");
        ClassDB::bind_method(D_METHOD("set_dawn_color","c"),   &LightingSkyNode::set_dawn_color);
        ClassDB::bind_method(D_METHOD("get_dawn_color"),       &LightingSkyNode::get_dawn_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"DawnColor"),"set_dawn_color","get_dawn_color");
        ClassDB::bind_method(D_METHOD("set_sky_mid_color","c"),&LightingSkyNode::set_sky_mid_color);
        ClassDB::bind_method(D_METHOD("get_sky_mid_color"),    &LightingSkyNode::get_sky_mid_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"SkyMidColor"),"set_sky_mid_color","get_sky_mid_color");
        ClassDB::bind_method(D_METHOD("set_ground_color","c"), &LightingSkyNode::set_ground_color);
        ClassDB::bind_method(D_METHOD("get_ground_color"),     &LightingSkyNode::get_ground_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"GroundColor"),"set_ground_color","get_ground_color");

        ClassDB::bind_method(D_METHOD("set_sky_exposure","v"),&LightingSkyNode::set_sky_exposure);
        ClassDB::bind_method(D_METHOD("get_sky_exposure"),    &LightingSkyNode::get_sky_exposure);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"SkyExposure",PROPERTY_HINT_RANGE,"0,8,0.01"),"set_sky_exposure","get_sky_exposure");
        ClassDB::bind_method(D_METHOD("set_sky_saturation","v"),&LightingSkyNode::set_sky_saturation);
        ClassDB::bind_method(D_METHOD("get_sky_saturation"),    &LightingSkyNode::get_sky_saturation);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"SkySaturation",PROPERTY_HINT_RANGE,"0,3,0.01"),"set_sky_saturation","get_sky_saturation");
        ClassDB::bind_method(D_METHOD("set_sky_curve","v"),&LightingSkyNode::set_sky_curve);
        ClassDB::bind_method(D_METHOD("get_sky_curve"),    &LightingSkyNode::get_sky_curve);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"SkyCurve",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_sky_curve","get_sky_curve");
        ClassDB::bind_method(D_METHOD("set_sun_angular_size","v"),&LightingSkyNode::set_sun_angular_size);
        ClassDB::bind_method(D_METHOD("get_sun_angular_size"),    &LightingSkyNode::get_sun_angular_size);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"SunAngularSize",PROPERTY_HINT_RANGE,"0,90,0.1"),"set_sun_angular_size","get_sun_angular_size");

        ClassDB::bind_method(D_METHOD("set_rayleigh_strength","v"),&LightingSkyNode::set_rayleigh_strength);
        ClassDB::bind_method(D_METHOD("get_rayleigh_strength"),    &LightingSkyNode::get_rayleigh_strength);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"RayleighStrength",PROPERTY_HINT_RANGE,"0,5,0.01"),"set_rayleigh_strength","get_rayleigh_strength");
        ClassDB::bind_method(D_METHOD("set_rayleigh_color","c"),&LightingSkyNode::set_rayleigh_color);
        ClassDB::bind_method(D_METHOD("get_rayleigh_color"),    &LightingSkyNode::get_rayleigh_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"RayleighColor"),"set_rayleigh_color","get_rayleigh_color");
        ClassDB::bind_method(D_METHOD("set_mie_strength","v"),&LightingSkyNode::set_mie_strength);
        ClassDB::bind_method(D_METHOD("get_mie_strength"),    &LightingSkyNode::get_mie_strength);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"MieStrength",PROPERTY_HINT_RANGE,"0,5,0.01"),"set_mie_strength","get_mie_strength");
        ClassDB::bind_method(D_METHOD("set_mie_eccentricity","v"),&LightingSkyNode::set_mie_eccentricity);
        ClassDB::bind_method(D_METHOD("get_mie_eccentricity"),    &LightingSkyNode::get_mie_eccentricity);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"MieEccentricity",PROPERTY_HINT_RANGE,"-0.99,0.99,0.01"),"set_mie_eccentricity","get_mie_eccentricity");
        ClassDB::bind_method(D_METHOD("set_mie_color","c"),&LightingSkyNode::set_mie_color);
        ClassDB::bind_method(D_METHOD("get_mie_color"),    &LightingSkyNode::get_mie_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"MieColor"),"set_mie_color","get_mie_color");

        ClassDB::bind_method(D_METHOD("set_horizon_fog_color","c"),&LightingSkyNode::set_horizon_fog_color);
        ClassDB::bind_method(D_METHOD("get_horizon_fog_color"),    &LightingSkyNode::get_horizon_fog_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"HorizonFogColor"),"set_horizon_fog_color","get_horizon_fog_color");
        ClassDB::bind_method(D_METHOD("set_horizon_fog_amount","v"),&LightingSkyNode::set_horizon_fog_amount);
        ClassDB::bind_method(D_METHOD("get_horizon_fog_amount"),    &LightingSkyNode::get_horizon_fog_amount);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"HorizonFogAmount",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_horizon_fog_amount","get_horizon_fog_amount");
        ClassDB::bind_method(D_METHOD("set_horizon_sharpness","v"),&LightingSkyNode::set_horizon_sharpness);
        ClassDB::bind_method(D_METHOD("get_horizon_sharpness"),    &LightingSkyNode::get_horizon_sharpness);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"HorizonSharpness",PROPERTY_HINT_RANGE,"0.5,20,0.1"),"set_horizon_sharpness","get_horizon_sharpness");

        ClassDB::bind_method(D_METHOD("set_sunset_color","c"),&LightingSkyNode::set_sunset_color);
        ClassDB::bind_method(D_METHOD("get_sunset_color"),    &LightingSkyNode::get_sunset_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"SunsetColor"),"set_sunset_color","get_sunset_color");
        ClassDB::bind_method(D_METHOD("set_sunset_strength","v"),&LightingSkyNode::set_sunset_strength);
        ClassDB::bind_method(D_METHOD("get_sunset_strength"),    &LightingSkyNode::get_sunset_strength);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"SunsetStrength",PROPERTY_HINT_RANGE,"0,3,0.01"),"set_sunset_strength","get_sunset_strength");

        ClassDB::bind_method(D_METHOD("set_sun_disk_scale","v"),&LightingSkyNode::set_sun_disk_scale);
        ClassDB::bind_method(D_METHOD("get_sun_disk_scale"),    &LightingSkyNode::get_sun_disk_scale);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"SunDiskScale",PROPERTY_HINT_RANGE,"0,10,0.01"),"set_sun_disk_scale","get_sun_disk_scale");
        ClassDB::bind_method(D_METHOD("set_sun_energy","v"),&LightingSkyNode::set_sun_energy);
        ClassDB::bind_method(D_METHOD("get_sun_energy"),    &LightingSkyNode::get_sun_energy);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"SunEnergy",PROPERTY_HINT_RANGE,"0,16,0.01"),"set_sun_energy","get_sun_energy");
        ClassDB::bind_method(D_METHOD("set_sun_corona_size","v"),&LightingSkyNode::set_sun_corona_size);
        ClassDB::bind_method(D_METHOD("get_sun_corona_size"),    &LightingSkyNode::get_sun_corona_size);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"SunCoronaSize",PROPERTY_HINT_RANGE,"0,5,0.01"),"set_sun_corona_size","get_sun_corona_size");
        ClassDB::bind_method(D_METHOD("set_sun_disk_color","c"),&LightingSkyNode::set_sun_disk_color);
        ClassDB::bind_method(D_METHOD("get_sun_disk_color"),    &LightingSkyNode::get_sun_disk_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"SunDiskColor"),"set_sun_disk_color","get_sun_disk_color");
        ClassDB::bind_method(D_METHOD("set_ground_energy","v"),&LightingSkyNode::set_ground_energy);
        ClassDB::bind_method(D_METHOD("get_ground_energy"),    &LightingSkyNode::get_ground_energy);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"GroundEnergy",PROPERTY_HINT_RANGE,"0,4,0.01"),"set_ground_energy","get_ground_energy");

        ClassDB::bind_method(D_METHOD("set_star_density","v"),&LightingSkyNode::set_star_density);
        ClassDB::bind_method(D_METHOD("get_star_density"),    &LightingSkyNode::get_star_density);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"StarDensity",PROPERTY_HINT_RANGE,"0,100,0.1"),"set_star_density","get_star_density");
        ClassDB::bind_method(D_METHOD("set_star_brightness","v"),&LightingSkyNode::set_star_brightness);
        ClassDB::bind_method(D_METHOD("get_star_brightness"),    &LightingSkyNode::get_star_brightness);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"StarBrightness",PROPERTY_HINT_RANGE,"0,3,0.01"),"set_star_brightness","get_star_brightness");
        ClassDB::bind_method(D_METHOD("set_star_size","v"),&LightingSkyNode::set_star_size);
        ClassDB::bind_method(D_METHOD("get_star_size"),    &LightingSkyNode::get_star_size);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"StarSize",PROPERTY_HINT_RANGE,"0.1,5,0.01"),"set_star_size","get_star_size");
        ClassDB::bind_method(D_METHOD("set_star_twinkle_speed","v"),&LightingSkyNode::set_star_twinkle_speed);
        ClassDB::bind_method(D_METHOD("get_star_twinkle_speed"),    &LightingSkyNode::get_star_twinkle_speed);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"StarTwinkleSpeed",PROPERTY_HINT_RANGE,"0,5,0.01"),"set_star_twinkle_speed","get_star_twinkle_speed");

        ClassDB::bind_method(D_METHOD("set_cloud_amount","v"),&LightingSkyNode::set_cloud_amount);
        ClassDB::bind_method(D_METHOD("get_cloud_amount"),    &LightingSkyNode::get_cloud_amount);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"CloudAmount",PROPERTY_HINT_RANGE,"0,1,0.01"),"set_cloud_amount","get_cloud_amount");
        ClassDB::bind_method(D_METHOD("set_cloud_color","c"),&LightingSkyNode::set_cloud_color);
        ClassDB::bind_method(D_METHOD("get_cloud_color"),    &LightingSkyNode::get_cloud_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"CloudColor"),"set_cloud_color","get_cloud_color");

        ClassDB::bind_method(D_METHOD("set_moon_color","c"),&LightingSkyNode::set_moon_color);
        ClassDB::bind_method(D_METHOD("get_moon_color"),    &LightingSkyNode::get_moon_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"MoonColor"),"set_moon_color","get_moon_color");
        ClassDB::bind_method(D_METHOD("set_moon_size","v"),&LightingSkyNode::set_moon_size);
        ClassDB::bind_method(D_METHOD("get_moon_size"),    &LightingSkyNode::get_moon_size);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"MoonSize",PROPERTY_HINT_RANGE,"0,5,0.01"),"set_moon_size","get_moon_size");
        ClassDB::bind_method(D_METHOD("set_moon_energy","v"),&LightingSkyNode::set_moon_energy);
        ClassDB::bind_method(D_METHOD("get_moon_energy"),    &LightingSkyNode::get_moon_energy);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"MoonEnergy",PROPERTY_HINT_RANGE,"0,4,0.01"),"set_moon_energy","get_moon_energy");
        ClassDB::bind_method(D_METHOD("set_nebula_color","c"),&LightingSkyNode::set_nebula_color);
        ClassDB::bind_method(D_METHOD("get_nebula_color"),    &LightingSkyNode::get_nebula_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"NebulaColor"),"set_nebula_color","get_nebula_color");
        ClassDB::bind_method(D_METHOD("set_nebula_strength","v"),&LightingSkyNode::set_nebula_strength);
        ClassDB::bind_method(D_METHOD("get_nebula_strength"),    &LightingSkyNode::get_nebula_strength);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"NebulaStrength",PROPERTY_HINT_RANGE,"0,1,0.01"),"set_nebula_strength","get_nebula_strength");
        ClassDB::bind_method(D_METHOD("set_milky_way_strength","v"),&LightingSkyNode::set_milky_way_strength);
        ClassDB::bind_method(D_METHOD("get_milky_way_strength"),    &LightingSkyNode::get_milky_way_strength);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"MilkyWayStrength",PROPERTY_HINT_RANGE,"0,2,0.01"),"set_milky_way_strength","get_milky_way_strength");
    }
    void _notification(int p_what) { if (p_what == NOTIFICATION_ENTER_TREE) _apply(); }

public:
    void set_sky_preset(int v)          { sky_preset = v;                                  if(is_inside_tree()) _apply(); }
    int  get_sky_preset() const         { return sky_preset; }
    void set_sky_color(Color c)         { sky_color = c;                                   if(is_inside_tree()) _apply(); }
    Color get_sky_color() const         { return sky_color; }
    void set_horizon_color(Color c)     { horizon_color = c;                               if(is_inside_tree()) _apply(); }
    Color get_horizon_color() const     { return horizon_color; }
    void set_dawn_color(Color c)        { dawn_color = c;                                  if(is_inside_tree()) _apply(); }
    Color get_dawn_color() const        { return dawn_color; }
    void set_sky_mid_color(Color c)     { sky_mid_color = c;                               if(is_inside_tree()) _apply(); }
    Color get_sky_mid_color() const     { return sky_mid_color; }
    void set_ground_color(Color c)      { ground_color = c;                                if(is_inside_tree()) _apply(); }
    Color get_ground_color() const      { return ground_color; }
    void set_sky_exposure(float v)      { sky_exposure = Math::clamp(v,0.0f,8.0f);         if(is_inside_tree()) _apply(); }
    float get_sky_exposure() const      { return sky_exposure; }
    void set_sky_saturation(float v)    { sky_saturation = Math::clamp(v,0.0f,3.0f);       if(is_inside_tree()) _apply(); }
    float get_sky_saturation() const    { return sky_saturation; }
    void set_sky_curve(float v)         { sky_curve = Math::clamp(v,0.0f,1.0f);            if(is_inside_tree()) _apply(); }
    float get_sky_curve() const         { return sky_curve; }
    void set_sun_angular_size(float v)  { sun_angular_size = Math::clamp(v,0.0f,90.0f);    if(is_inside_tree()) _apply(); }
    float get_sun_angular_size() const  { return sun_angular_size; }
    void set_rayleigh_strength(float v) { rayleigh_strength = Math::clamp(v,0.0f,5.0f);    if(is_inside_tree()) _apply(); }
    float get_rayleigh_strength() const { return rayleigh_strength; }
    void set_rayleigh_color(Color c)    { rayleigh_color = c;                               if(is_inside_tree()) _apply(); }
    Color get_rayleigh_color() const    { return rayleigh_color; }
    void set_mie_strength(float v)      { mie_strength = Math::clamp(v,0.0f,5.0f);         if(is_inside_tree()) _apply(); }
    float get_mie_strength() const      { return mie_strength; }
    void set_mie_eccentricity(float v)  { mie_eccentricity = Math::clamp(v,-0.99f,0.99f);  if(is_inside_tree()) _apply(); }
    float get_mie_eccentricity() const  { return mie_eccentricity; }
    void set_mie_color(Color c)         { mie_color = c;                                    if(is_inside_tree()) _apply(); }
    Color get_mie_color() const         { return mie_color; }
    void set_horizon_fog_color(Color c) { horizon_fog_color = c;                            if(is_inside_tree()) _apply(); }
    Color get_horizon_fog_color() const { return horizon_fog_color; }
    void set_horizon_fog_amount(float v){ horizon_fog_amount = Math::clamp(v,0.0f,1.0f);   if(is_inside_tree()) _apply(); }
    float get_horizon_fog_amount() const{ return horizon_fog_amount; }
    void set_horizon_sharpness(float v) { horizon_sharpness = Math::clamp(v,0.5f,20.0f);   if(is_inside_tree()) _apply(); }
    float get_horizon_sharpness() const { return horizon_sharpness; }
    void set_sunset_color(Color c)      { sunset_color = c;                                 if(is_inside_tree()) _apply(); }
    Color get_sunset_color() const      { return sunset_color; }
    void set_sunset_strength(float v)   { sunset_strength = Math::clamp(v,0.0f,3.0f);      if(is_inside_tree()) _apply(); }
    float get_sunset_strength() const   { return sunset_strength; }
    void set_sun_disk_scale(float v)    { sun_disk_scale = Math::clamp(v,0.0f,10.0f);      if(is_inside_tree()) _apply(); }
    float get_sun_disk_scale() const    { return sun_disk_scale; }
    void set_sun_energy(float v)        { sun_energy = Math::clamp(v,0.0f,16.0f);          if(is_inside_tree()) _apply(); }
    float get_sun_energy() const        { return sun_energy; }
    void set_sun_corona_size(float v)   { sun_corona_size = Math::clamp(v,0.0f,5.0f);      if(is_inside_tree()) _apply(); }
    float get_sun_corona_size() const   { return sun_corona_size; }
    void set_sun_disk_color(Color c)    { sun_disk_color = c;                               if(is_inside_tree()) _apply(); }
    Color get_sun_disk_color() const    { return sun_disk_color; }
    void set_ground_energy(float v)     { ground_energy = Math::clamp(v,0.0f,4.0f);        if(is_inside_tree()) _apply(); }
    float get_ground_energy() const     { return ground_energy; }
    void set_star_density(float v)      { star_density = Math::clamp(v,0.0f,100.0f);       if(is_inside_tree()) _apply(); }
    float get_star_density() const      { return star_density; }
    void set_star_brightness(float v)   { star_brightness = Math::clamp(v,0.0f,3.0f);      if(is_inside_tree()) _apply(); }
    float get_star_brightness() const   { return star_brightness; }
    void set_star_size(float v)         { star_size = Math::clamp(v,0.1f,5.0f);            if(is_inside_tree()) _apply(); }
    float get_star_size() const         { return star_size; }
    void set_star_twinkle_speed(float v){ star_twinkle_speed = Math::clamp(v,0.0f,5.0f);   if(is_inside_tree()) _apply(); }
    float get_star_twinkle_speed() const{ return star_twinkle_speed; }
    void set_cloud_amount(float v)      { cloud_amount = Math::clamp(v,0.0f,1.0f);         if(is_inside_tree()) _apply(); }
    float get_cloud_amount() const      { return cloud_amount; }
    void set_cloud_color(Color c)       { cloud_color = c;                                  if(is_inside_tree()) _apply(); }
    Color get_cloud_color() const       { return cloud_color; }
    void set_moon_color(Color c)        { moon_color = c;                                   if(is_inside_tree()) _apply(); }
    Color get_moon_color() const        { return moon_color; }
    void set_moon_size(float v)         { moon_size = Math::clamp(v,0.0f,5.0f);            if(is_inside_tree()) _apply(); }
    float get_moon_size() const         { return moon_size; }
    void set_moon_energy(float v)       { moon_energy = Math::clamp(v,0.0f,4.0f);          if(is_inside_tree()) _apply(); }
    float get_moon_energy() const       { return moon_energy; }
    void set_nebula_color(Color c)      { nebula_color = c;                                 if(is_inside_tree()) _apply(); }
    Color get_nebula_color() const      { return nebula_color; }
    void set_nebula_strength(float v)   { nebula_strength = Math::clamp(v,0.0f,1.0f);      if(is_inside_tree()) _apply(); }
    float get_nebula_strength() const   { return nebula_strength; }
    void set_milky_way_strength(float v){ milky_way_strength = Math::clamp(v,0.0f,2.0f);   if(is_inside_tree()) _apply(); }
    float get_milky_way_strength() const{ return milky_way_strength; }
};

// ════════════════════════════════════════════════════════════════════
//  SunRaysNode — God-rays / sun shafts via glow
// ════════════════════════════════════════════════════════════════════
class SunRaysNode : public Node {
    GDCLASS(SunRaysNode, Node);

    float intensity   = 0.25f;
    float spread      = 1.0f;
    Color ray_color   = Color(1.00f, 0.95f, 0.70f);
    float bloom_boost = 0.5f;

    void _apply() {
        Ref<Environment> env = _lx_get_env(this);
        if (!env.is_valid()) return;
        if (intensity < 0.01f) { env->set_glow_enabled(false); return; }
        env->set_glow_enabled(true);
        env->set_glow_normalized(false);
        env->set_glow_strength(1.5f + intensity);
        env->set_glow_intensity(intensity);
        env->set_glow_bloom(bloom_boost * 0.2f);
        env->set_glow_blend_mode(Environment::GLOW_BLEND_MODE_ADDITIVE);
        int max_level = (int)(spread * 6.0f + 0.5f);
        for (int i = 0; i < 7; i++)
            env->set_glow_level(i, (i <= max_level) ? intensity * 0.25f : 0.0f);
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_intensity","v"), &SunRaysNode::set_intensity);
        ClassDB::bind_method(D_METHOD("get_intensity"),     &SunRaysNode::get_intensity);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Intensity",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_intensity","get_intensity");
        ClassDB::bind_method(D_METHOD("set_spread","v"), &SunRaysNode::set_spread);
        ClassDB::bind_method(D_METHOD("get_spread"),     &SunRaysNode::get_spread);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Spread",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_spread","get_spread");
        ClassDB::bind_method(D_METHOD("set_ray_color","c"), &SunRaysNode::set_ray_color);
        ClassDB::bind_method(D_METHOD("get_ray_color"),     &SunRaysNode::get_ray_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"RayColor"),"set_ray_color","get_ray_color");
        ClassDB::bind_method(D_METHOD("set_bloom_boost","v"), &SunRaysNode::set_bloom_boost);
        ClassDB::bind_method(D_METHOD("get_bloom_boost"),     &SunRaysNode::get_bloom_boost);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"BloomBoost",PROPERTY_HINT_RANGE,"0,2,0.01"),"set_bloom_boost","get_bloom_boost");
    }
    void _notification(int p_what) { if (p_what == NOTIFICATION_ENTER_TREE) _apply(); }

public:
    void set_intensity(float v)   { intensity   = Math::clamp(v,0.0f,1.0f); if(is_inside_tree()) _apply(); }
    float get_intensity() const   { return intensity; }
    void set_spread(float v)      { spread      = Math::clamp(v,0.0f,1.0f); if(is_inside_tree()) _apply(); }
    float get_spread() const      { return spread; }
    void set_ray_color(Color c)   { ray_color   = c;                         if(is_inside_tree()) _apply(); }
    Color get_ray_color() const   { return ray_color; }
    void set_bloom_boost(float v) { bloom_boost = Math::clamp(v,0.0f,2.0f); if(is_inside_tree()) _apply(); }
    float get_bloom_boost() const { return bloom_boost; }
};

// ════════════════════════════════════════════════════════════════════
//  BloomEffect — HDR bloom / glow
// ════════════════════════════════════════════════════════════════════
class BloomEffect : public Node {
    GDCLASS(BloomEffect, Node);

    float intensity    = 0.5f;
    float size         = 0.5f;
    float threshold    = 0.8f;
    float hdr_scale    = 2.0f;
    float lum_cap      = 8.0f;
    int   blend_mode   = (int)Environment::GLOW_BLEND_MODE_SOFTLIGHT;

    void _apply() {
        Ref<Environment> env = _lx_get_env(this);
        if (!env.is_valid()) return;
        env->set_glow_enabled(intensity > 0.001f);
        if (intensity < 0.001f) return;
        env->set_glow_normalized(false);
        env->set_glow_intensity(intensity);
        env->set_glow_bloom(size);
        env->set_glow_hdr_bleed_threshold(threshold);
        env->set_glow_hdr_bleed_scale(hdr_scale);
        env->set_glow_hdr_luminance_cap(lum_cap);
        env->set_glow_blend_mode((Environment::GlowBlendMode)blend_mode);
        for (int i = 0; i < 7; i++)
            env->set_glow_level(i, intensity * 0.15f);
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
        ClassDB::bind_method(D_METHOD("set_hdr_scale","v"), &BloomEffect::set_hdr_scale);
        ClassDB::bind_method(D_METHOD("get_hdr_scale"),     &BloomEffect::get_hdr_scale);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"HdrScale",PROPERTY_HINT_RANGE,"0.5,4,0.01"),"set_hdr_scale","get_hdr_scale");
        ClassDB::bind_method(D_METHOD("set_lum_cap","v"), &BloomEffect::set_lum_cap);
        ClassDB::bind_method(D_METHOD("get_lum_cap"),     &BloomEffect::get_lum_cap);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"LuminanceCap",PROPERTY_HINT_RANGE,"0,32,0.1"),"set_lum_cap","get_lum_cap");
        ClassDB::bind_method(D_METHOD("set_blend_mode","v"), &BloomEffect::set_blend_mode);
        ClassDB::bind_method(D_METHOD("get_blend_mode"),     &BloomEffect::get_blend_mode);
        ADD_PROPERTY(PropertyInfo(Variant::INT,"BlendMode",PROPERTY_HINT_ENUM,
            "Additive,Screen,Softlight,Replace,Mix"),"set_blend_mode","get_blend_mode");
    }
    void _notification(int p_what) { if (p_what == NOTIFICATION_ENTER_TREE) _apply(); }

public:
    void set_intensity(float v)  { intensity  = Math::clamp(v,0.0f,3.0f);  if(is_inside_tree()) _apply(); }
    float get_intensity() const  { return intensity; }
    void set_size(float v)       { size       = Math::clamp(v,0.0f,1.0f);  if(is_inside_tree()) _apply(); }
    float get_size() const       { return size; }
    void set_threshold(float v)  { threshold  = Math::clamp(v,0.0f,8.0f);  if(is_inside_tree()) _apply(); }
    float get_threshold() const  { return threshold; }
    void set_hdr_scale(float v)  { hdr_scale  = Math::clamp(v,0.5f,4.0f);  if(is_inside_tree()) _apply(); }
    float get_hdr_scale() const  { return hdr_scale; }
    void set_lum_cap(float v)    { lum_cap    = Math::clamp(v,0.0f,32.0f); if(is_inside_tree()) _apply(); }
    float get_lum_cap() const    { return lum_cap; }
    void set_blend_mode(int v)   { blend_mode = v;                          if(is_inside_tree()) _apply(); }
    int  get_blend_mode() const  { return blend_mode; }
};

// ════════════════════════════════════════════════════════════════════
//  BlurEffect — Full-scene blur (Roblox BlurEffect)
// ════════════════════════════════════════════════════════════════════
class BlurEffect : public Node {
    GDCLASS(BlurEffect, Node);

    float size       = 0.0f;
    float near_size  = 0.0f;
    float far_size   = 0.0f;

    void _apply() {
        WorldEnvironment* we = _lx_find_we_node(this);
        if (!we) return;
        Ref<CameraAttributesPractical> cam;
        cam.instantiate();
        float effective = Math::max(size, Math::max(near_size, far_size));
        if (effective < 0.01f) {
            cam->set_dof_blur_far_enabled(false);
            cam->set_dof_blur_near_enabled(false);
        } else {
            float far_amt  = Math::max(size, far_size);
            float near_amt = Math::max(size, near_size);
            cam->set_dof_blur_far_enabled(far_amt > 0.01f);
            cam->set_dof_blur_far_distance(1.0f + (1.0f - far_amt * 0.018f) * 200.0f);
            cam->set_dof_blur_far_transition(far_amt * 3.0f);
            cam->set_dof_blur_near_enabled(near_amt > 0.01f);
            cam->set_dof_blur_near_distance((1.0f - near_amt * 0.018f) * 5.0f);
            cam->set_dof_blur_near_transition(near_amt * 1.5f);
            cam->set_dof_blur_amount(Math::clamp(effective / 56.0f, 0.0f, 1.0f));
        }
        we->set_camera_attributes(cam);
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_size","v"),      &BlurEffect::set_size);
        ClassDB::bind_method(D_METHOD("get_size"),          &BlurEffect::get_size);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Size",PROPERTY_HINT_RANGE,"0,56,0.1"),"set_size","get_size");
        ClassDB::bind_method(D_METHOD("set_near_size","v"), &BlurEffect::set_near_size);
        ClassDB::bind_method(D_METHOD("get_near_size"),     &BlurEffect::get_near_size);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"NearBlur",PROPERTY_HINT_RANGE,"0,56,0.1"),"set_near_size","get_near_size");
        ClassDB::bind_method(D_METHOD("set_far_size","v"),  &BlurEffect::set_far_size);
        ClassDB::bind_method(D_METHOD("get_far_size"),      &BlurEffect::get_far_size);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"FarBlur",PROPERTY_HINT_RANGE,"0,56,0.1"),"set_far_size","get_far_size");
    }
    void _notification(int p_what) { if (p_what == NOTIFICATION_ENTER_TREE) _apply(); }

public:
    void set_size(float v)      { size      = Math::clamp(v,0.0f,56.0f); if(is_inside_tree()) _apply(); }
    float get_size() const      { return size; }
    void set_near_size(float v) { near_size = Math::clamp(v,0.0f,56.0f); if(is_inside_tree()) _apply(); }
    float get_near_size() const { return near_size; }
    void set_far_size(float v)  { far_size  = Math::clamp(v,0.0f,56.0f); if(is_inside_tree()) _apply(); }
    float get_far_size() const  { return far_size; }
};

// ════════════════════════════════════════════════════════════════════
//  ColorCorrectionEffect — brightness, contrast, saturation, tint
// ════════════════════════════════════════════════════════════════════
class ColorCorrectionEffect : public Node {
    GDCLASS(ColorCorrectionEffect, Node);

    float brightness  = 1.0f;
    float contrast    = 1.0f;
    float saturation  = 1.0f;
    float gamma       = 1.0f;
    Color tint_color  = Color(1.0f, 1.0f, 1.0f);
    float tint_amount = 0.0f;

    void _apply() {
        Ref<Environment> env = _lx_get_env(this);
        if (!env.is_valid()) return;
        bool active = (fabsf(brightness - 1.0f) > 0.001f ||
                       fabsf(contrast   - 1.0f) > 0.001f ||
                       fabsf(saturation - 1.0f) > 0.001f ||
                       fabsf(gamma      - 1.0f) > 0.001f ||
                       tint_amount > 0.001f);
        env->set_adjustment_enabled(active);
        float eff_brightness = brightness;
        if (tint_amount > 0.001f) {
            Color tc = tint_color;
            float luma = 0.299f * tc.r + 0.587f * tc.g + 0.114f * tc.b;
            eff_brightness *= (1.0f - tint_amount + tint_amount * luma);
        }
        env->set_adjustment_brightness(eff_brightness);
        env->set_adjustment_contrast(contrast);
        float eff_sat = saturation;
        if (tint_amount > 0.001f) {
            float tc_sat = Math::max(fabsf(tint_color.r - tint_color.g),
                           Math::max(fabsf(tint_color.g - tint_color.b),
                                     fabsf(tint_color.r - tint_color.b)));
            eff_sat *= (1.0f + tint_amount * tc_sat * 0.5f);
        }
        env->set_adjustment_saturation(eff_sat);
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
        ClassDB::bind_method(D_METHOD("set_gamma","v"), &ColorCorrectionEffect::set_gamma);
        ClassDB::bind_method(D_METHOD("get_gamma"),     &ColorCorrectionEffect::get_gamma);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Gamma",PROPERTY_HINT_RANGE,"0.1,4,0.01"),"set_gamma","get_gamma");
        ClassDB::bind_method(D_METHOD("set_tint_color","c"), &ColorCorrectionEffect::set_tint_color);
        ClassDB::bind_method(D_METHOD("get_tint_color"),     &ColorCorrectionEffect::get_tint_color);
        ADD_PROPERTY(PropertyInfo(Variant::COLOR,"TintColor"),"set_tint_color","get_tint_color");
        ClassDB::bind_method(D_METHOD("set_tint_amount","v"), &ColorCorrectionEffect::set_tint_amount);
        ClassDB::bind_method(D_METHOD("get_tint_amount"),     &ColorCorrectionEffect::get_tint_amount);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"TintAmount",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_tint_amount","get_tint_amount");
    }
    void _notification(int p_what) { if (p_what == NOTIFICATION_ENTER_TREE) _apply(); }

public:
    void set_brightness(float v)  { brightness  = Math::clamp(v,0.0f,5.0f);  if(is_inside_tree()) _apply(); }
    float get_brightness() const  { return brightness; }
    void set_contrast(float v)    { contrast    = Math::clamp(v,0.0f,5.0f);  if(is_inside_tree()) _apply(); }
    float get_contrast() const    { return contrast; }
    void set_saturation(float v)  { saturation  = Math::clamp(v,0.0f,5.0f);  if(is_inside_tree()) _apply(); }
    float get_saturation() const  { return saturation; }
    void set_gamma(float v)       { gamma       = Math::clamp(v,0.1f,4.0f);  if(is_inside_tree()) _apply(); }
    float get_gamma() const       { return gamma; }
    void set_tint_color(Color c)  { tint_color  = c;                          if(is_inside_tree()) _apply(); }
    Color get_tint_color() const  { return tint_color; }
    void set_tint_amount(float v) { tint_amount = Math::clamp(v,0.0f,1.0f);  if(is_inside_tree()) _apply(); }
    float get_tint_amount() const { return tint_amount; }
};

// ════════════════════════════════════════════════════════════════════
//  DepthOfFieldEffect — camera depth-of-field blur
// ════════════════════════════════════════════════════════════════════
class DepthOfFieldEffect : public Node {
    GDCLASS(DepthOfFieldEffect, Node);

    float far_distance   = 100.0f;
    float far_transition = 10.0f;
    float near_distance  = 2.0f;
    float near_transition= 1.0f;
    float blur_amount    = 0.1f;
    bool  far_enabled    = false;
    bool  near_enabled   = false;

    void _apply() {
        WorldEnvironment* we = _lx_find_we_node(this);
        if (!we) return;
        Ref<CameraAttributesPractical> cam;
        cam.instantiate();
        cam->set_dof_blur_far_enabled(far_enabled);
        if (far_enabled) {
            cam->set_dof_blur_far_distance(far_distance);
            cam->set_dof_blur_far_transition(far_transition);
        }
        cam->set_dof_blur_near_enabled(near_enabled);
        if (near_enabled) {
            cam->set_dof_blur_near_distance(near_distance);
            cam->set_dof_blur_near_transition(near_transition);
        }
        cam->set_dof_blur_amount(blur_amount);
        if (far_enabled || near_enabled)
            we->set_camera_attributes(cam);
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_far_enabled","v"),    &DepthOfFieldEffect::set_far_enabled);
        ClassDB::bind_method(D_METHOD("get_far_enabled"),        &DepthOfFieldEffect::get_far_enabled);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"FarEnabled"),"set_far_enabled","get_far_enabled");
        ClassDB::bind_method(D_METHOD("set_near_enabled","v"),   &DepthOfFieldEffect::set_near_enabled);
        ClassDB::bind_method(D_METHOD("get_near_enabled"),       &DepthOfFieldEffect::get_near_enabled);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"NearEnabled"),"set_near_enabled","get_near_enabled");
        ClassDB::bind_method(D_METHOD("set_far_distance","v"),   &DepthOfFieldEffect::set_far_distance);
        ClassDB::bind_method(D_METHOD("get_far_distance"),       &DepthOfFieldEffect::get_far_distance);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"FarDistance",PROPERTY_HINT_RANGE,"0,5000,0.1"),"set_far_distance","get_far_distance");
        ClassDB::bind_method(D_METHOD("set_far_transition","v"), &DepthOfFieldEffect::set_far_transition);
        ClassDB::bind_method(D_METHOD("get_far_transition"),     &DepthOfFieldEffect::get_far_transition);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"FarTransition",PROPERTY_HINT_RANGE,"0,1000,0.1"),"set_far_transition","get_far_transition");
        ClassDB::bind_method(D_METHOD("set_near_distance","v"),  &DepthOfFieldEffect::set_near_distance);
        ClassDB::bind_method(D_METHOD("get_near_distance"),      &DepthOfFieldEffect::get_near_distance);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"NearDistance",PROPERTY_HINT_RANGE,"0,1000,0.1"),"set_near_distance","get_near_distance");
        ClassDB::bind_method(D_METHOD("set_near_transition","v"),&DepthOfFieldEffect::set_near_transition);
        ClassDB::bind_method(D_METHOD("get_near_transition"),    &DepthOfFieldEffect::get_near_transition);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"NearTransition",PROPERTY_HINT_RANGE,"0,1000,0.1"),"set_near_transition","get_near_transition");
        ClassDB::bind_method(D_METHOD("set_blur_amount","v"),    &DepthOfFieldEffect::set_blur_amount);
        ClassDB::bind_method(D_METHOD("get_blur_amount"),        &DepthOfFieldEffect::get_blur_amount);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"BlurAmount",PROPERTY_HINT_RANGE,"0,1,0.001"),"set_blur_amount","get_blur_amount");
    }
    void _notification(int p_what) { if (p_what == NOTIFICATION_ENTER_TREE) _apply(); }

public:
    void set_far_enabled(bool v)     { far_enabled    = v;                                  if(is_inside_tree()) _apply(); }
    bool get_far_enabled() const     { return far_enabled; }
    void set_near_enabled(bool v)    { near_enabled   = v;                                  if(is_inside_tree()) _apply(); }
    bool get_near_enabled() const    { return near_enabled; }
    void set_far_distance(float v)   { far_distance   = Math::clamp(v,0.0f,5000.0f);        if(is_inside_tree()) _apply(); }
    float get_far_distance() const   { return far_distance; }
    void set_far_transition(float v) { far_transition  = Math::clamp(v,0.0f,1000.0f);       if(is_inside_tree()) _apply(); }
    float get_far_transition() const { return far_transition; }
    void set_near_distance(float v)  { near_distance  = Math::clamp(v,0.0f,1000.0f);        if(is_inside_tree()) _apply(); }
    float get_near_distance() const  { return near_distance; }
    void set_near_transition(float v){ near_transition = Math::clamp(v,0.0f,1000.0f);       if(is_inside_tree()) _apply(); }
    float get_near_transition() const{ return near_transition; }
    void set_blur_amount(float v)    { blur_amount     = Math::clamp(v,0.0f,1.0f);           if(is_inside_tree()) _apply(); }
    float get_blur_amount() const    { return blur_amount; }
};

#endif // ROBLOX_LIGHTING_FX_H
