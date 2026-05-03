#ifndef ROBLOX_TWEEN_H
#define ROBLOX_TWEEN_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/object.hpp>
#include <vector>
#include <functional>
#include <cmath>
#include <cstring>

#include "lua.h"
#include "lualib.h"

using namespace godot;

// ────────────────────────────────────────────────────────────────────
//  EasingStyle / EasingDirection — mirror of Roblox Enum
////
//  EasingStyle / EasingDirection — espejo de Roblox Enum
// ────────────────────────────────────────────────────────────────────
enum TweenEasingStyle {
    EASE_LINEAR = 0,
    EASE_SINE,
    EASE_QUAD,
    EASE_CUBIC,
    EASE_QUART,
    EASE_QUINT,
    EASE_BOUNCE,
    EASE_ELASTIC,
    EASE_BACK,
    EASE_EXPO
};

enum TweenEasingDirection {
    EASE_IN = 0,
    EASE_OUT,
    EASE_IN_OUT
};

// ────────────────────────────────────────────────────────────────────
//  Easing functions (t ∈ [0,1] → [0,1])
////
//  Funciones de easing (t ∈ [0,1] → [0,1])
// ────────────────────────────────────────────────────────────────────
static inline float _tween_apply_ease(float t, int style, int dir) {
    const float PI = 3.14159265f;
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;

    auto in_fn = [&](float x) -> float {
        switch (style) {
            case EASE_SINE:    return 1.0f - cosf((x * PI) / 2.0f);
            case EASE_QUAD:    return x * x;
            case EASE_CUBIC:   return x * x * x;
            case EASE_QUART:   return x * x * x * x;
            case EASE_QUINT:   return x * x * x * x * x;
            case EASE_EXPO:    return x == 0.0f ? 0.0f : powf(2.0f, 10.0f * x - 10.0f);
            case EASE_BACK:    return c3 * x * x * x - c1 * x * x;
            case EASE_ELASTIC: {
                const float c4 = (2.0f * PI) / 3.0f;
                return x == 0.0f ? 0.0f : x == 1.0f ? 1.0f :
                       -powf(2.0f, 10.0f * x - 10.0f) * sinf((x * 10.0f - 10.75f) * c4);
            }
            case EASE_BOUNCE: {
                float v = 1.0f - x;
                const float n1 = 7.5625f, d1 = 2.75f;
                if (v < 1.0f/d1)      return 1.0f - (n1*v*v);
                else if (v < 2.0f/d1) return 1.0f - (n1*(v-1.5f/d1)*v + 0.75f);
                else if (v < 2.5f/d1) return 1.0f - (n1*(v-2.25f/d1)*v + 0.9375f);
                else                  return 1.0f - (n1*(v-2.625f/d1)*v + 0.984375f);
            }
            default: return x; // LINEAR
        }
    };

    auto out_fn = [&](float x) -> float { return 1.0f - in_fn(1.0f - x); };

    switch (dir) {
        case EASE_IN:     return in_fn(t);
        case EASE_OUT:    return out_fn(t);
        case EASE_IN_OUT: {
            if (t < 0.5f) return in_fn(t * 2.0f) / 2.0f;
            else          return (2.0f - in_fn((1.0f - t) * 2.0f)) / 2.0f;
        }
        default: return t;
    }
}

// ────────────────────────────────────────────────────────────────────
//  TweenInfo — immutable struct that describes a tween
////
//  TweenInfo — estructura inmutable que describe un tween
// ────────────────────────────────────────────────────────────────────
struct TweenInfo {
    float    time           = 1.0f;
    int      easing_style   = EASE_LINEAR;
    int      easing_dir     = EASE_OUT;
    int      repeat_count   = 0;
    bool     reverses       = false;
    float    delay_time     = 0.0f;
};

// ────────────────────────────────────────────────────────────────────
//  Generic variant for interpolatable values
////
//  Variante genérica para valores interpolables
// ────────────────────────────────────────────────────────────────────
struct TweenValue {
    enum Type { FLOAT, COLOR, VECTOR3, VECTOR2 } type = FLOAT;
    float   f = 0;
    Color   c;
    Vector3 v3;
    Vector2 v2;

    TweenValue() {}
    explicit TweenValue(float x)   : type(FLOAT),   f(x) {}
    explicit TweenValue(Color x)   : type(COLOR),   c(x) {}
    explicit TweenValue(Vector3 x) : type(VECTOR3), v3(x) {}
    explicit TweenValue(Vector2 x) : type(VECTOR2), v2(x) {}

    TweenValue lerp(const TweenValue& to, float t) const {
        TweenValue out;
        out.type = type;
        switch (type) {
            case FLOAT:   out.f  = f  + (to.f  - f)  * t; break;
            case COLOR:   out.c  = c.lerp(to.c,  t);       break;
            case VECTOR3: out.v3 = v3.lerp(to.v3, t);      break;
            case VECTOR2: out.v2 = v2.lerp(to.v2, t);      break;
        }
        return out;
    }
};

// ────────────────────────────────────────────────────────────────────
//  A single animation track inside a Tween
////
//  Una pista de animación individual dentro de un Tween
// ────────────────────────────────────────────────────────────────────
struct TweenTrack {
    Object*     target      = nullptr;
    String      property;
    TweenValue  from_val;
    TweenValue  to_val;
    bool        from_current = true; // capture "from" when starting / capturar "from" al iniciar

    void apply(float alpha) const {
        if (!target) return;
        TweenValue v = from_val.lerp(to_val, alpha);
        switch (v.type) {
            case TweenValue::FLOAT:   target->set(property, v.f);  break;
            case TweenValue::COLOR:   target->set(property, v.c);  break;
            case TweenValue::VECTOR3: target->set(property, v.v3); break;
            case TweenValue::VECTOR2: target->set(property, v.v2); break;
        }
    }

    void capture_from() {
        if (!target || !from_current) return;
        Variant cur = target->get(property);
        switch (cur.get_type()) {
            case Variant::FLOAT: case Variant::INT:
                from_val = TweenValue((float)(double)cur); break;
            case Variant::COLOR:
                from_val = TweenValue((Color)cur); break;
            case Variant::VECTOR3:
                from_val = TweenValue((Vector3)cur); break;
            case Variant::VECTOR2:
                from_val = TweenValue((Vector2)cur); break;
            default: break;
        }
    }
};

// ────────────────────────────────────────────────────────────────────
//  RobloxTween — the active tween object returned by TweenService
////
//  RobloxTween — el objeto tween activo devuelto por TweenService
// ────────────────────────────────────────────────────────────────────
class RobloxTween : public Node {
    GDCLASS(RobloxTween, Node);

public:
    struct LuaCallback { lua_State* main_L; int ref; bool active = true; };

    TweenInfo               info;
    std::vector<TweenTrack> tracks;
    std::vector<LuaCallback> completed_cbs;

    float  elapsed     = 0.0f;
    int    rep_done    = 0;
    bool   playing     = false;
    bool   reversing   = false;
    bool   paused      = false;
    bool   finished    = false;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("play"),   &RobloxTween::play);
        ClassDB::bind_method(D_METHOD("pause"),  &RobloxTween::pause);
        ClassDB::bind_method(D_METHOD("cancel"), &RobloxTween::cancel);
    }

public:
    void play() {
        if (finished) {
            elapsed   = 0.0f;
            rep_done  = 0;
            reversing = false;
            finished  = false;
        }
        for (auto& t : tracks) t.capture_from();
        playing = true;
        paused  = false;
        set_process(true);
    }

    void pause()  { paused = true; }
    void cancel() { playing = false; paused = false; set_process(false); }

    void add_completed_cb(lua_State* L, int ref) {
        completed_cbs.push_back({L, ref, true});
    }

    void _process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        if (!playing || paused || finished) return;

        float wait = info.delay_time;
        if (elapsed < wait) { elapsed += (float)delta; return; }

        float t = (elapsed - wait) / (info.time > 0.0f ? info.time : 0.001f);
        t = Math::clamp(t, 0.0f, 1.0f);

        float alpha_t = reversing ? (1.0f - t) : t;
        float eased   = _tween_apply_ease(alpha_t, info.easing_style, info.easing_dir);

        for (auto& track : tracks) track.apply(eased);

        elapsed += (float)delta;

        if (t >= 1.0f) {
            if (info.reverses && !reversing) {
                reversing = true;
                elapsed   = wait;
                return;
            }
            reversing = false;
            rep_done++;

            bool more = (info.repeat_count < 0) || (rep_done <= info.repeat_count);
            if (more) {
                elapsed = wait;
            } else {
                playing  = false;
                finished = true;
                set_process(false);
                _fire_completed();
            }
        }
    }

    void _fire_completed() {
        for (auto& cb : completed_cbs) {
            if (!cb.active) continue;
            lua_State* th = lua_newthread(cb.main_L);
            lua_rawgeti(cb.main_L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.main_L, -1)) {
                lua_xmove(cb.main_L, th, 1);
                lua_resume(th, nullptr, 0);
            } else lua_pop(cb.main_L, 1);
            lua_pop(cb.main_L, 1);
        }
    }
};

// ────────────────────────────────────────────────────────────────────
//  TweenService — accessed via game:GetService("TweenService")
////
//  TweenService — acceso via game:GetService("TweenService")
// ────────────────────────────────────────────────────────────────────
class TweenService : public Node {
    GDCLASS(TweenService, Node);

protected:
    static void _bind_methods() {}

public:
    // Creates a tween — actual logic is handled from Lua via luau_api
    //// Crea un tween — la lógica real se maneja desde Lua vía luau_api
    RobloxTween* create_tween(Node* parent_node) {
        RobloxTween* tw = memnew(RobloxTween);
        tw->set_name("__tween__");
        tw->set_process(false);
        if (parent_node) parent_node->add_child(tw);
        else add_child(tw);
        return tw;
    }
};

#endif // ROBLOX_TWEEN_H
