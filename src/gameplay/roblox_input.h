#ifndef ROBLOX_INPUT_H
#define ROBLOX_INPUT_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/input_event_screen_drag.hpp>
#include <godot_cpp/classes/input_event_joypad_button.hpp>
#include <godot_cpp/classes/input_event_joypad_motion.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <vector>
#include <cstring>
#include <algorithm>
#include <map>

#include "lua.h"
#include "lualib.h"
#include "gl_errors.h"
#include "gl_runtime.h"

using namespace godot;

// ────────────────────────────────────────────────────────────────────
//  Helpers: Roblox Enum.KeyCode ↔ Godot Key conversion
//  Roblox uses ASCII values for letters/digits (A=65, Space=32) and
//  its own values for special keys (LeftShift=304, etc.)
//  Godot uses the same ASCII for letters but 0x01000000+ for specials.
////
//  Helpers: conversión Roblox Enum.KeyCode ↔ Godot Key
//  Roblox usa valores ASCII para letras/dígitos (A=65, Space=32) y
//  valores propios para teclas especiales (LeftShift=304, etc.)
//  Godot usa ASCII igual para letras, pero 0x01000000+ para especiales.
// ────────────────────────────────────────────────────────────────────
static inline Key roblox_keycode_to_godot(int rc) {
    if (rc >= 32 && rc <= 127) return (Key)rc;  // ASCII: letters, digits, punctuation / letras, dígitos, puntuación
    switch (rc) {
        case 8:   return KEY_BACKSPACE;
        case 9:   return KEY_TAB;
        case 13:  return KEY_ENTER;
        case 27:  return KEY_ESCAPE;
        case 127: return KEY_DELETE;
        case 273: return KEY_UP;
        case 274: return KEY_DOWN;
        case 275: return KEY_RIGHT;
        case 276: return KEY_LEFT;
        case 277: return KEY_INSERT;
        case 278: return KEY_HOME;
        case 279: return KEY_END;
        case 280: return KEY_PAGEUP;
        case 281: return KEY_PAGEDOWN;
        case 282: return KEY_F1;   case 283: return KEY_F2;
        case 284: return KEY_F3;   case 285: return KEY_F4;
        case 286: return KEY_F5;   case 287: return KEY_F6;
        case 288: return KEY_F7;   case 289: return KEY_F8;
        case 290: return KEY_F9;   case 291: return KEY_F10;
        case 292: return KEY_F11;  case 293: return KEY_F12;
        case 303: case 304: return KEY_SHIFT;
        case 305: case 306: return KEY_CTRL;
        case 307: case 308: return KEY_ALT;
        case 301: return KEY_CAPSLOCK;
        default:  return KEY_UNKNOWN;
    }
}

static inline int godot_key_to_roblox(Key gk) {
    int k = (int)gk;
    if (k >= 32 && k <= 127) return k;  // Direct ASCII / ASCII directo
    switch (gk) {
        case KEY_BACKSPACE:  return 8;
        case KEY_TAB:        return 9;
        case KEY_ENTER:      return 13;
        case KEY_ESCAPE:     return 27;
        case KEY_DELETE:     return 127;
        case KEY_UP:         return 273;
        case KEY_DOWN:       return 274;
        case KEY_RIGHT:      return 275;
        case KEY_LEFT:       return 276;
        case KEY_INSERT:     return 277;
        case KEY_HOME:       return 278;
        case KEY_END:        return 279;
        case KEY_PAGEUP:     return 280;
        case KEY_PAGEDOWN:   return 281;
        case KEY_F1:  return 282; case KEY_F2:  return 283;
        case KEY_F3:  return 284; case KEY_F4:  return 285;
        case KEY_F5:  return 286; case KEY_F6:  return 287;
        case KEY_F7:  return 288; case KEY_F8:  return 289;
        case KEY_F9:  return 290; case KEY_F10: return 291;
        case KEY_F11: return 292; case KEY_F12: return 293;
        case KEY_SHIFT:    return 304;
        case KEY_CTRL:     return 306;
        case KEY_ALT:      return 308;
        case KEY_CAPSLOCK: return 301;
        default:           return 0;
    }
}

// ────────────────────────────────────────────────────────────────────
//  UserInputService — exact equivalent to Roblox UserInputService
//  Usage from Luau:
//    local UIS = game:GetService("UserInputService")
//    UIS.InputBegan:Connect(function(input, gameProcessed) ... end)
//    UIS.InputEnded:Connect(function(input, gameProcessed) ... end)
//    UIS.InputChanged:Connect(function(input, gameProcessed) ... end)
//    UIS:IsKeyDown(Enum.KeyCode.W)
//    UIS:GetMouseLocation() → Vector2
////
//  UserInputService — equivalente exacto a Roblox UserInputService
//  Uso desde Luau:
//    local UIS = game:GetService("UserInputService")
//    UIS.InputBegan:Connect(function(input, gameProcessed) ... end)
//    UIS.InputEnded:Connect(function(input, gameProcessed) ... end)
//    UIS.InputChanged:Connect(function(input, gameProcessed) ... end)
//    UIS:IsKeyDown(Enum.KeyCode.W)
//    UIS:GetMouseLocation() → Vector2
// ────────────────────────────────────────────────────────────────────
// Crea un Vector3 de Luau (userdata con metatable "Vector3") sin depender de
// push_vector3 de luau_api.h (que se define DESPUES de incluir este header). El
// metatable solo lee los 3 primeros floats, asi que escribirlos basta.
static inline void uis_push_vec3(lua_State* L, float x, float y, float z) {
    float* v = (float*)lua_newuserdata(L, sizeof(float) * 3);
    v[0] = x; v[1] = y; v[2] = z;
    luaL_getmetatable(L, "Vector3");
    lua_setmetatable(L, -2);
}

class UserInputService : public Node {
    GDCLASS(UserInputService, Node);

public:
    struct LuaCallback { lua_State* main_L; int ref; bool active = true; };

    std::vector<LuaCallback> input_began_cbs;
    std::vector<LuaCallback> input_ended_cbs;
    std::vector<LuaCallback> input_changed_cbs;
    std::vector<LuaCallback> touch_tap_cbs;

private:
    bool    mouse_behavior_lock = false;
    Vector2 mouse_delta_accum;   // acumulado durante el frame en curso
    Vector2 mouse_delta_frame;   // doble-buffer: delta del frame anterior (lo que devuelve GetMouseDelta)
    int     last_input_type = 10; // Enum.UserInputType del ultimo input (Keyboard por defecto)

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("_gl_disconnect", "ref"),    &UserInputService::_gl_disconnect);
        ClassDB::bind_method(D_METHOD("is_key_down", "keycode"),   &UserInputService::is_key_down);
        ClassDB::bind_method(D_METHOD("get_mouse_location"),       &UserInputService::get_mouse_location_v);
        ClassDB::bind_method(D_METHOD("get_mouse_delta"),          &UserInputService::get_mouse_delta_v);
        ClassDB::bind_method(D_METHOD("set_mouse_behavior", "b"),  &UserInputService::set_mouse_behavior);
        ClassDB::bind_method(D_METHOD("get_mouse_behavior"),       &UserInputService::get_mouse_behavior);
        ClassDB::bind_method(D_METHOD("is_gamepad_button_down","device","button"), &UserInputService::is_gamepad_button_down);
        ClassDB::bind_method(D_METHOD("get_last_input_type"),      &UserInputService::get_last_input_type);
        ClassDB::bind_method(D_METHOD("get_touch_enabled"),        &UserInputService::get_touch_enabled);
        ClassDB::bind_method(D_METHOD("get_gamepad_connected"),    &UserInputService::get_gamepad_connected);

        ADD_SIGNAL(MethodInfo("InputBegan",   PropertyInfo(Variant::OBJECT, "input"), PropertyInfo(Variant::BOOL, "gameProcessed")));
        ADD_SIGNAL(MethodInfo("InputEnded",   PropertyInfo(Variant::OBJECT, "input"), PropertyInfo(Variant::BOOL, "gameProcessed")));
        ADD_SIGNAL(MethodInfo("InputChanged", PropertyInfo(Variant::OBJECT, "input"), PropertyInfo(Variant::BOOL, "gameProcessed")));
    }

public:
    void add_input_began_cb(lua_State* L, int ref)   { input_began_cbs.push_back({L,ref,true}); }
    void add_input_ended_cb(lua_State* L, int ref)   { input_ended_cbs.push_back({L,ref,true}); }
    void add_input_changed_cb(lua_State* L, int ref) { input_changed_cbs.push_back({L,ref,true}); }

    void remove_callbacks_for_state(lua_State* L) {
        for (auto& cb : input_began_cbs)   if (cb.main_L==L) cb.active=false;
        for (auto& cb : input_ended_cbs)   if (cb.main_L==L) cb.active=false;
        for (auto& cb : input_changed_cbs) if (cb.main_L==L) cb.active=false;
    }
    void _gl_disconnect(int ref) {
        for (auto& cb : input_began_cbs)   if (cb.ref==ref) cb.active=false;
        for (auto& cb : input_ended_cbs)   if (cb.ref==ref) cb.active=false;
        for (auto& cb : input_changed_cbs) if (cb.ref==ref) cb.active=false;
    }

    // Antes casteaba (Key)keycode directo → las teclas especiales (Shift, flechas,
    // etc.) nunca se detectaban. Ahora convierte el Enum.KeyCode de Roblox al Key
    // de Godot. Acepta tambien un Key de Godot crudo como respaldo.
    bool is_key_down(int keycode) const {
        if (!is_inside_tree()) return false;
        Key gk = roblox_keycode_to_godot(keycode);
        if (gk == KEY_UNKNOWN) gk = (Key)keycode;
        return Input::get_singleton()->is_key_pressed(gk);
    }
    bool is_gamepad_button_down(int device, int button) const {
        return Input::get_singleton()->is_joy_button_pressed(device, (JoyButton)button);
    }
    int  get_last_input_type() const  { return last_input_type; }
    bool get_touch_enabled() const    { return DisplayServer::get_singleton()->is_touchscreen_available(); }
    bool get_gamepad_connected() const{ return Input::get_singleton()->get_connected_joypads().size() > 0; }

    Vector2 get_mouse_location_v() const {
        return DisplayServer::get_singleton()->mouse_get_position();
    }
    // Delta real del mouse en el frame (antes siempre devolvia (0,0)).
    Vector2 get_mouse_delta_v() const { return mouse_delta_frame; }

    void set_mouse_behavior(int b) {
        mouse_behavior_lock = (b != 0);
        if (mouse_behavior_lock) {
            Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
        } else {
            Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
        }
    }
    int get_mouse_behavior() const { return mouse_behavior_lock ? 2 : 0; }

    bool is_mouse_button_down(int btn) const {
        return Input::get_singleton()->is_mouse_button_pressed((MouseButton)btn);
    }

    void _ready() override {
        if (!Engine::get_singleton()->is_editor_hint()) {
            set_process_input(true);
            set_process(true);   // para el doble-buffer del delta de mouse
        }
    }

    // Doble-buffer del delta de mouse: lo acumulado en _input este frame pasa a
    // ser el valor que GetMouseDelta devuelve durante el frame siguiente, asi
    // todos los lectores ven el mismo valor consistente.
    void _process(double) override {
        mouse_delta_frame = mouse_delta_accum;
        mouse_delta_accum = Vector2();
    }

    // Recibe TODOS los eventos (teclado, mouse, rueda, tactil, gamepad) y dispara
    // InputBegan / InputChanged / InputEnded a Luau en tiempo real, con un
    // InputObject que trae UserInputType, KeyCode, UserInputState, Position y Delta.
    void _input(const Ref<InputEvent>& event) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        Vector2 mloc = get_mouse_location_v();

        // ── Teclado ────────────────────────────────────────────────
        const InputEventKey* key_ev = Object::cast_to<const InputEventKey>(*event);
        if (key_ev) {
            if (key_ev->is_echo()) return;
            int rc = godot_key_to_roblox(key_ev->get_keycode());
            if (rc == 0) return;
            last_input_type = 10;
            if (key_ev->is_pressed()) _fire(input_began_cbs, 10, rc, 0, Vector3(), Vector3());
            else                      _fire(input_ended_cbs, 10, rc, 2, Vector3(), Vector3());
            return;
        }

        // ── Boton de mouse + rueda ─────────────────────────────────
        const InputEventMouseButton* mb = Object::cast_to<const InputEventMouseButton>(*event);
        if (mb && !mb->is_double_click()) {
            int btn = (int)mb->get_button_index();   // 1=Izq 2=Der 3=Medio 4=ruedaArriba 5=ruedaAbajo
            if (btn == MOUSE_BUTTON_WHEEL_UP || btn == MOUSE_BUTTON_WHEEL_DOWN) {
                if (mb->is_pressed()) {
                    float dir = (btn == MOUSE_BUTTON_WHEEL_UP) ? 1.0f : -1.0f;
                    last_input_type = 4;  // MouseWheel
                    _fire(input_changed_cbs, 4, 0, 1, Vector3(mloc.x, mloc.y, dir), Vector3(0, 0, dir));
                }
                return;
            }
            last_input_type = btn;  // MouseButton1..3
            if (mb->is_pressed()) _fire(input_began_cbs, btn, 0, 0, Vector3(mloc.x, mloc.y, 0), Vector3());
            else                  _fire(input_ended_cbs, btn, 0, 2, Vector3(mloc.x, mloc.y, 0), Vector3());
            return;
        }

        // ── Movimiento de mouse → InputChanged (MouseMovement=5) ───
        const InputEventMouseMotion* mm = Object::cast_to<const InputEventMouseMotion>(*event);
        if (mm) {
            Vector2 rel = mm->get_relative();
            mouse_delta_accum += rel;
            last_input_type = 5;
            _fire(input_changed_cbs, 5, 0, 1, Vector3(mloc.x, mloc.y, 0), Vector3(rel.x, rel.y, 0));
            return;
        }

        // ── Tactil ─────────────────────────────────────────────────
        const InputEventScreenTouch* st = Object::cast_to<const InputEventScreenTouch>(*event);
        if (st) {
            Vector2 p = st->get_position();
            last_input_type = 7;  // Touch
            if (st->is_pressed()) _fire(input_began_cbs, 7, st->get_index(), 0, Vector3(p.x, p.y, 0), Vector3());
            else                  _fire(input_ended_cbs, 7, st->get_index(), 2, Vector3(p.x, p.y, 0), Vector3());
            return;
        }
        const InputEventScreenDrag* sd = Object::cast_to<const InputEventScreenDrag>(*event);
        if (sd) {
            Vector2 p = sd->get_position(), rel = sd->get_relative();
            last_input_type = 7;
            _fire(input_changed_cbs, 7, sd->get_index(), 1, Vector3(p.x, p.y, 0), Vector3(rel.x, rel.y, 0));
            return;
        }

        // ── Gamepad: botones y sticks/gatillos ─────────────────────
        const InputEventJoypadButton* jb = Object::cast_to<const InputEventJoypadButton>(*event);
        if (jb) {
            last_input_type = 8;  // Gamepad1
            if (jb->is_pressed()) _fire(input_began_cbs, 8, (int)jb->get_button_index(), 0, Vector3(), Vector3());
            else                  _fire(input_ended_cbs, 8, (int)jb->get_button_index(), 2, Vector3(), Vector3());
            return;
        }
        const InputEventJoypadMotion* jm = Object::cast_to<const InputEventJoypadMotion>(*event);
        if (jm) {
            float v = jm->get_axis_value();
            last_input_type = 8;
            _fire(input_changed_cbs, 8, (int)jm->get_axis(), 1, Vector3(v, 0, 0), Vector3(v, 0, 0));
            return;
        }
    }

    // Compatibilidad: helpers antiguos que disparaban began/ended por tipo+codigo.
    void fire_input_began(int t, int kc, bool gp) { _fire(input_began_cbs, t, kc, 0, Vector3(), Vector3(), gp); }
    void fire_input_ended(int t, int kc, bool gp) { _fire(input_ended_cbs, t, kc, 2, Vector3(), Vector3(), gp); }

private:
    // state: 0=Begin 1=Change 2=End (Enum.UserInputState)
    void _fire(std::vector<LuaCallback>& list, int type, int kc, int state,
               Vector3 pos, Vector3 delta, bool game_proc = false) {
        for (int i=(int)list.size()-1; i>=0; --i) {
            auto& cb = list[i];
            if (!cb.active || !gl_state_alive(cb.main_L)) { list.erase(list.begin()+i); continue; }
            lua_State* th = lua_newthread(cb.main_L);
            lua_rawgeti(cb.main_L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.main_L, -1)) {
                lua_xmove(cb.main_L, th, 1);
                // InputObject (tabla con campos estilo Roblox)
                lua_newtable(th);
                lua_pushinteger(th, type);  lua_setfield(th, -2, "UserInputType");
                lua_pushinteger(th, kc);    lua_setfield(th, -2, "KeyCode");
                lua_pushinteger(th, state); lua_setfield(th, -2, "UserInputState");
                uis_push_vec3(th, pos.x,   pos.y,   pos.z);   lua_setfield(th, -2, "Position");
                uis_push_vec3(th, delta.x, delta.y, delta.z); lua_setfield(th, -2, "Delta");
                lua_pushboolean(th, game_proc);  // 2do argumento: gameProcessed
                gl_check_resume(th, lua_resume(th, nullptr, 2));
            } else lua_pop(cb.main_L, 1);
            lua_pop(cb.main_L, 1);
        }
    }
};

// ────────────────────────────────────────────────────────────────────
//  CollectionService — Roblox-style tag/label system
//  Usage from Luau:
//    CollectionService:AddTag(inst, "Enemy")
//    CollectionService:GetTagged("Enemy") → array
//    CollectionService:HasTag(inst, "Enemy") → bool
////
//  CollectionService — sistema de tags/etiquetas tipo Roblox
//  Uso desde Luau:
//    CollectionService:AddTag(inst, "Enemy")
//    CollectionService:GetTagged("Enemy") → array
//    CollectionService:HasTag(inst, "Enemy") → bool
// ────────────────────────────────────────────────────────────────────
class CollectionService : public Node {
    GDCLASS(CollectionService, Node);

    std::map<String, std::vector<Node*>> tagged;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("add_tag", "inst", "tag"),    &CollectionService::add_tag);
        ClassDB::bind_method(D_METHOD("remove_tag", "inst", "tag"), &CollectionService::remove_tag);
        ClassDB::bind_method(D_METHOD("has_tag", "inst", "tag"),    &CollectionService::has_tag);
        ClassDB::bind_method(D_METHOD("get_tagged", "tag"),         &CollectionService::get_tagged);
        ClassDB::bind_method(D_METHOD("get_tags", "inst"),          &CollectionService::get_tags);
    }

public:
    void add_tag(Node* inst, const String& tag) {
        if (!inst) return;
        auto& list = tagged[tag];
        for (auto* n : list) if (n == inst) return;
        list.push_back(inst);
    }

    void remove_tag(Node* inst, const String& tag) {
        auto it = tagged.find(tag);
        if (it == tagged.end()) return;
        auto& list = it->second;
        list.erase(std::remove(list.begin(), list.end(), inst), list.end());
    }

    bool has_tag(Node* inst, const String& tag) const {
        auto it = tagged.find(tag);
        if (it == tagged.end()) return false;
        for (auto* n : it->second) if (n == inst) return true;
        return false;
    }

    TypedArray<Node> get_tagged(const String& tag) const {
        TypedArray<Node> result;
        auto it = tagged.find(tag);
        if (it == tagged.end()) return result;
        for (auto* n : it->second) {
            if (n && n->is_inside_tree()) result.push_back(n);
        }
        return result;
    }

    PackedStringArray get_tags(Node* inst) const {
        PackedStringArray result;
        for (auto& [tag, list] : tagged) {
            for (auto* n : list) {
                if (n == inst) { result.push_back(tag); break; }
            }
        }
        return result;
    }
};

#endif // ROBLOX_INPUT_H
