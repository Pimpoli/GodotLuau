#ifndef ROBLOX_ANIMATION_H
#define ROBLOX_ANIMATION_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/animation_player.hpp>
#include <godot_cpp/classes/animation.hpp>
#include <godot_cpp/classes/animation_library.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <vector>
#include <cstring>

#include "lua.h"
#include "lualib.h"

using namespace godot;

// ────────────────────────────────────────────────────────────────────
//  AnimationTrack — equivalent to Roblox's AnimationTrack
//  Returned by Humanoid:LoadAnimation(animation)
//  Usage from Luau:
//    local anim = humanoid:LoadAnimation(animObject)
//    anim:Play()
//    anim:Stop()
//    anim.Looped = true
//    anim.Speed = 1.5
//    anim.Ended:Connect(function() end)
////
//  AnimationTrack — equivalente a Roblox AnimationTrack
//  Devuelto por Humanoid:LoadAnimation(animation)
//  Uso desde Luau:
//    local anim = humanoid:LoadAnimation(animObject)
//    anim:Play()
//    anim:Stop()
//    anim.Looped = true
//    anim.Speed = 1.5
//    anim.Ended:Connect(function() end)
// ────────────────────────────────────────────────────────────────────
class AnimationTrack : public Node {
    GDCLASS(AnimationTrack, Node);

public:
    struct LuaCallback { lua_State* main_L; int ref; bool active = true; };

    std::vector<LuaCallback> ended_cbs;
    std::vector<LuaCallback> stopped_cbs;

private:
    String            anim_name    = "";
    bool              looped       = false;
    float             speed        = 1.0f;
    float             weight       = 1.0f;
    bool              playing      = false;
    AnimationPlayer*  anim_player  = nullptr;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("play"),  &AnimationTrack::play);
        ClassDB::bind_method(D_METHOD("stop"),  &AnimationTrack::stop);
        ClassDB::bind_method(D_METHOD("is_playing"), &AnimationTrack::is_playing);
        ClassDB::bind_method(D_METHOD("set_looped", "v"),  &AnimationTrack::set_looped);
        ClassDB::bind_method(D_METHOD("get_looped"),       &AnimationTrack::get_looped);
        ClassDB::bind_method(D_METHOD("set_speed", "v"),   &AnimationTrack::set_speed);
        ClassDB::bind_method(D_METHOD("get_speed"),        &AnimationTrack::get_speed);
        ClassDB::bind_method(D_METHOD("set_weight", "v"),  &AnimationTrack::set_weight);
        ClassDB::bind_method(D_METHOD("get_weight"),       &AnimationTrack::get_weight);
        ClassDB::bind_method(D_METHOD("get_length"),       &AnimationTrack::get_length);
        ClassDB::bind_method(D_METHOD("get_time_position"), &AnimationTrack::get_time_position);
        ClassDB::bind_method(D_METHOD("adjust_speed", "v"), &AnimationTrack::adjust_speed);

        ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "Looped"), "set_looped", "get_looped");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Speed",  PROPERTY_HINT_RANGE, "0,10,0.01"), "set_speed", "get_speed");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "WeightCurrent", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_weight", "get_weight");
        ADD_SIGNAL(MethodInfo("Ended"));
        ADD_SIGNAL(MethodInfo("Stopped", PropertyInfo(Variant::BOOL, "was_completed")));
    }

public:
    void setup(const String& name, AnimationPlayer* player) {
        anim_name   = name;
        anim_player = player;
        if (anim_player) anim_player->connect("animation_finished",
            callable_mp(this, &AnimationTrack::_on_finished));
    }

    void play() {
        if (!anim_player || anim_name.is_empty()) return;
        if (!anim_player->has_animation(StringName(anim_name))) return;
        anim_player->set_speed_scale(speed);
        anim_player->play(StringName(anim_name));
        playing = true;
        _update_loop();
    }

    void stop() {
        if (!anim_player) return;
        anim_player->stop();
        playing = false;
        emit_signal("Stopped", false);
        _fire_noarg(stopped_cbs);
    }

    bool is_playing() const {
        if (!anim_player) return false;
        return anim_player->is_playing() && anim_player->get_current_animation() == StringName(anim_name);
    }

    void set_looped(bool v) {
        looped = v;
        if (anim_player && !anim_name.is_empty()) _update_loop();
    }
    bool get_looped() const { return looped; }

    void set_speed(float v) {
        speed = Math::clamp(v, 0.0f, 20.0f);
        if (anim_player) anim_player->set_speed_scale(speed);
    }
    float get_speed() const { return speed; }

    void set_weight(float v) { weight = Math::clamp(v, 0.0f, 1.0f); }
    float get_weight() const { return weight; }

    void adjust_speed(float v) { set_speed(v); }

    float get_length() const {
        if (!anim_player || anim_name.is_empty()) return 0.0f;
        Ref<Animation> a = anim_player->get_animation(StringName(anim_name));
        if (!a.is_valid()) return 0.0f;
        return (float)a->get_length();
    }

    float get_time_position() const {
        if (!anim_player || !is_playing()) return 0.0f;
        return (float)anim_player->get_current_animation_position();
    }

    void add_ended_cb(lua_State* L, int ref)   { ended_cbs.push_back({L,ref,true}); }
    void add_stopped_cb(lua_State* L, int ref) { stopped_cbs.push_back({L,ref,true}); }

    void remove_callbacks_for_state(lua_State* L) {
        for (auto& cb : ended_cbs)   if (cb.main_L==L) cb.active=false;
        for (auto& cb : stopped_cbs) if (cb.main_L==L) cb.active=false;
    }

private:
    void _update_loop() {
        if (!anim_player || anim_name.is_empty()) return;
        Ref<Animation> a = anim_player->get_animation(StringName(anim_name));
        if (!a.is_valid()) return;
        a->set_loop_mode(looped ? Animation::LOOP_LINEAR : Animation::LOOP_NONE);
    }

    void _on_finished(StringName name) {
        if (name != StringName(anim_name)) return;
        playing = false;
        emit_signal("Ended");
        emit_signal("Stopped", true);
        _fire_noarg(ended_cbs);
        _fire_noarg(stopped_cbs);
    }

    void _fire_noarg(std::vector<LuaCallback>& list) {
        for (int i=(int)list.size()-1; i>=0; --i) {
            auto& cb=list[i];
            if (!cb.active){list.erase(list.begin()+i);continue;}
            lua_State* th=lua_newthread(cb.main_L);
            lua_rawgeti(cb.main_L,LUA_REGISTRYINDEX,cb.ref);
            if(lua_isfunction(cb.main_L,-1)){lua_xmove(cb.main_L,th,1);lua_resume(th,nullptr,0);}
            else lua_pop(cb.main_L,1);
            lua_pop(cb.main_L,1);
        }
    }
};

// ────────────────────────────────────────────────────────────────────
//  AnimationObject — animation object referenceable from Luau
//  Usage: local anim = Instance.new("Animation")
//         anim.AnimationId = "res://anims/idle.anim"
//         humanoid:LoadAnimation(anim)
////
//  AnimationObject — objeto de animación referenciable desde Luau
//  Uso: local anim = Instance.new("Animation")
//       anim.AnimationId = "res://anims/idle.anim"
//       humanoid:LoadAnimation(anim)
// ────────────────────────────────────────────────────────────────────
class AnimationObject : public Node {
    GDCLASS(AnimationObject, Node);

    String anim_id   = "";
    String anim_name = "default";

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_animation_id", "id"), &AnimationObject::set_animation_id);
        ClassDB::bind_method(D_METHOD("get_animation_id"),       &AnimationObject::get_animation_id);
        ClassDB::bind_method(D_METHOD("set_anim_name", "n"),    &AnimationObject::set_anim_name);
        ClassDB::bind_method(D_METHOD("get_anim_name"),          &AnimationObject::get_anim_name);
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "AnimationId"), "set_animation_id", "get_animation_id");
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "Name"),        "set_anim_name",    "get_anim_name");
    }

public:
    void set_animation_id(const String& id)  { anim_id = id; }
    String get_animation_id() const          { return anim_id; }
    void set_anim_name(const String& n)      { anim_name = n; }
    String get_anim_name() const             { return anim_name; }
};

#endif // ROBLOX_ANIMATION_H
