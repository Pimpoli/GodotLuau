#ifndef GL_NET_BRIDGE_H
#define GL_NET_BRIDGE_H

// ════════════════════════════════════════════════════════════════════
//  Puente de red Lua <-> Variant (Fase 2 — multijugador real)
//  --------------------------------------------------------------
//  Los RemoteEvent/RemoteFunction viajan por ENet como un Array de Variants.
//  Aquí está el serializador que convierte los argumentos Luau a un Array
//  transmisible y de vuelta, más el registro global de respuestas de
//  RemoteFunction (petición/respuesta con id de correlación).
//
//  Header NEUTRAL: solo depende de godot-cpp + Luau + gl_runtime.h, así
//  que lo pueden incluir roblox_remote.h, roblox_network.h y luau_script.h
//  sin crear ciclos de include.
//
//  Tipos soportados en el payload (paridad RemoteEvent con Roblox):
//    nil, bool, number, string; tablas array, diccionario Y MIXTAS (array+hash),
//    anidadas; Vector3, Color3 (userdata); TODOS los datatypes tipo-tabla con
//    __type (Vector2, CFrame, UDim, UDim2, Rect, Region3, Ray, NumberRange,
//    BrickColor, NumberSequence, ColorSequence, PhysicalProperties, DateTime…);
//    e Instances (por ruta absoluta; se re-resuelven en destino, nil si no existe).
//  Nota: los EnumItem viajan como su valor numérico (no como objeto EnumItem).
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/node_path.hpp>

#include "lua.h"
#include "lualib.h"
#include "gl_runtime.h"   // GodotObjectWrapper, gow_node, gow_set

#include <unordered_map>
#include <cstdint>
#include <cstring>

using namespace godot;

// Empuja un nodo (o nil) como GodotObject en el estado L.
static inline void gl_net_push_node(lua_State* L, Node* node) {
    if (!node) { lua_pushnil(L); return; }
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_newuserdata(L, sizeof(GodotObjectWrapper));
    gow_set(w, node);
    luaL_getmetatable(L, "GodotObject");
    lua_setmetatable(L, -2);
}

// ── Codificar un valor Luau (en idx) a Variant ────────────────────────────
static Variant gl_net_encode(lua_State* L, int idx, int depth = 0) {
    if (idx < 0) idx = lua_gettop(L) + idx + 1;
    if (!lua_checkstack(L, 6)) return Variant();   // reserva pila para metatables/claves de este nivel
    int t = lua_type(L, idx);
    switch (t) {
        case LUA_TNIL:     return Variant();
        case LUA_TBOOLEAN: return Variant((bool)lua_toboolean(L, idx));
        case LUA_TNUMBER: {
            double n = lua_tonumber(L, idx);
            return (n == (double)(int64_t)n) ? Variant((int64_t)n) : Variant(n);
        }
        case LUA_TSTRING:  return Variant(String::utf8(lua_tostring(L, idx)));
        case LUA_TUSERDATA: {
            if (!lua_getmetatable(L, idx)) return Variant();
            // Instance (GodotObject) → ruta absoluta
            luaL_getmetatable(L, "GodotObject");
            bool is_obj = lua_rawequal(L, -1, -2);
            lua_pop(L, 1);
            if (is_obj) {
                lua_pop(L, 1);  // metatable original
                GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, idx);
                Node* nd = w ? gow_node(w) : nullptr;
                Dictionary d;
                d["__glnet"] = "inst";
                d["p"] = (nd && nd->is_inside_tree()) ? String(nd->get_path()) : String();
                return d;
            }
            // Vector3 / Color3 (userdata de 3 floats)
            luaL_getmetatable(L, "Vector3");
            bool is_v3 = lua_rawequal(L, -1, -2);
            lua_pop(L, 1);
            if (is_v3) {
                lua_pop(L, 1);
                float* f = (float*)lua_touserdata(L, idx);
                return Variant(Vector3(f[0], f[1], f[2]));
            }
            luaL_getmetatable(L, "Color3");
            bool is_c3 = lua_rawequal(L, -1, -2);
            lua_pop(L, 1);
            if (is_c3) {
                lua_pop(L, 1);
                float* f = (float*)lua_touserdata(L, idx);
                return Variant(Color(f[0], f[1], f[2]));
            }
            lua_pop(L, 1);  // metatable original
            return Variant();  // userdata desconocido → nil
        }
        case LUA_TTABLE: {
            if (depth >= 8) return Variant();
            // ¿tabla tipada (CFrame/UDim2/…) con metatable.__type?
            String type_name;
            if (lua_getmetatable(L, idx)) {
                lua_getfield(L, -1, "__type");
                if (lua_isstring(L, -1)) type_name = String(lua_tostring(L, -1));
                lua_pop(L, 2);  // __type + metatable
            }
            int seq = (int)lua_objlen(L, idx);
            if (type_name.is_empty() && seq > 0) {
                // Solo usar el Array rápido si es un array PURO (sin parte hash).
                // Si la tabla es MIXTA (array + claves extra, p.ej. {1,2,foo=3}),
                // cae al dict de abajo para NO perder la parte hash — antes se
                // perdía, igual que el bug del encoder JSON. Roblox sí la envía.
                int counted = 0;
                lua_pushnil(L);
                while (lua_next(L, idx) != 0) { counted++; lua_pop(L, 1); }
                if (counted == seq) {
                    Array arr;
                    for (int i = 1; i <= seq; i++) {
                        lua_rawgeti(L, idx, i);
                        arr.push_back(gl_net_encode(L, -1, depth + 1));
                        lua_pop(L, 1);
                    }
                    return arr;
                }
                // tabla mixta → continúa al dict (preserva claves int 1..seq + hash)
            }
            Dictionary d;
            lua_pushnil(L);
            while (lua_next(L, idx) != 0) {
                Variant key;
                int kt = lua_type(L, -2);
                if (kt == LUA_TSTRING)      key = String(lua_tostring(L, -2));
                else if (kt == LUA_TNUMBER) {
                    double kn = lua_tonumber(L, -2);   // preservar claves no enteras como FLOAT (no truncar)
                    key = (kn == (double)(int64_t)kn) ? Variant((int64_t)kn) : Variant(kn);
                }
                else { lua_pop(L, 1); continue; }
                d[key] = gl_net_encode(L, -1, depth + 1);
                lua_pop(L, 1);
            }
            if (!type_name.is_empty()) {
                Dictionary wrap;
                wrap["__glnet"] = "typed";
                wrap["t"] = type_name;
                wrap["f"] = d;
                return wrap;
            }
            if (d.has("__glnet")) {   // tabla de usuario con la clave reservada: envolver para no confundirla al decodificar
                Dictionary wrap;
                wrap["__glnet"] = "rawdict";
                wrap["d"] = d;
                return wrap;
            }
            return d;
        }
        default: return Variant();
    }
}

// Empuja una clave (String o int) en L.
static inline void gl_net_push_key(lua_State* L, const Variant& k) {
    if (k.get_type() == Variant::INT)       lua_pushnumber(L, (double)(int64_t)k);
    else if (k.get_type() == Variant::FLOAT) lua_pushnumber(L, (double)k);
    else { String s = k; lua_pushstring(L, s.utf8().get_data()); }
}

// ── Decodificar un Variant y empujarlo en L ───────────────────────────────
// ctx: nodo desde el que resolver las rutas de Instances (get_node_or_null).
static void gl_net_decode(lua_State* L, const Variant& v, Node* ctx, int depth = 0) {
    if (depth >= 16) { lua_pushnil(L); return; }               // tope de anidamiento (payload no confiable)
    if (!lua_checkstack(L, 6)) { lua_pushnil(L); return; }     // reserva pila para tabla+clave+valor de este nivel
    switch (v.get_type()) {
        case Variant::NIL:    lua_pushnil(L); return;
        case Variant::BOOL:   lua_pushboolean(L, (bool)v ? 1 : 0); return;
        case Variant::INT:    lua_pushnumber(L, (double)(int64_t)v); return;
        case Variant::FLOAT:  lua_pushnumber(L, (double)v); return;
        case Variant::STRING: { String s = v; lua_pushstring(L, s.utf8().get_data()); return; }
        case Variant::VECTOR3: {
            Vector3 vec = v;
            float* f = (float*)lua_newuserdata(L, 3 * sizeof(float));
            f[0] = vec.x; f[1] = vec.y; f[2] = vec.z;
            luaL_getmetatable(L, "Vector3");
            lua_setmetatable(L, -2);
            return;
        }
        case Variant::COLOR: {
            Color c = v;
            float* f = (float*)lua_newuserdata(L, 3 * sizeof(float));
            f[0] = c.r; f[1] = c.g; f[2] = c.b;
            luaL_getmetatable(L, "Color3");
            lua_setmetatable(L, -2);
            return;
        }
        case Variant::ARRAY: {
            Array a = v;
            lua_newtable(L);
            for (int i = 0; i < a.size(); i++) {
                gl_net_decode(L, a[i], ctx, depth + 1);
                lua_rawseti(L, -2, i + 1);
            }
            return;
        }
        case Variant::DICTIONARY: {
            Dictionary d = v;
            if (d.has("__glnet")) {
                String tag = d["__glnet"];
                if (tag == String("inst")) {
                    String p = d.get("p", String());
                    Node* nd = nullptr;
                    if (!p.is_empty() && ctx && ctx->is_inside_tree())
                        nd = ctx->get_node_or_null(NodePath(p));
                    gl_net_push_node(L, nd);
                    return;
                }
                if (tag == String("typed")) {
                    String tn = d.get("t", String());
                    Dictionary f = d.get("f", Dictionary());
                    lua_newtable(L);
                    Array keys = f.keys();
                    for (int i = 0; i < keys.size(); i++) {
                        Variant k = keys[i];
                        gl_net_push_key(L, k);
                        gl_net_decode(L, f[k], ctx, depth + 1);
                        lua_settable(L, -3);
                    }
                    if (!tn.is_empty()) {
                        lua_getglobal(L, tn.utf8().get_data());
                        if (lua_istable(L, -1)) lua_setmetatable(L, -2);
                        else lua_pop(L, 1);
                    }
                    return;
                }
                if (tag == String("rawdict")) { d = d.get("d", Dictionary()); }  // dict de usuario con la clave reservada: decodificar el interior tal cual
            }
            lua_newtable(L);
            Array keys = d.keys();
            for (int i = 0; i < keys.size(); i++) {
                Variant k = keys[i];
                gl_net_push_key(L, k);
                gl_net_decode(L, d[k], ctx, depth + 1);
                lua_settable(L, -3);
            }
            return;
        }
        default: lua_pushnil(L); return;
    }
}

// Codifica un rango de argumentos [base, base+n) de la pila a un Array.
static inline Array gl_net_encode_args(lua_State* L, int base, int n) {
    Array a;
    for (int i = base; i < base + n; i++) a.push_back(gl_net_encode(L, i));
    return a;
}

// Empuja todos los elementos de un Array como argumentos en L; devuelve cuántos.
static inline int gl_net_decode_args(lua_State* L, const Array& a, Node* ctx) {
    for (int i = 0; i < a.size(); i++) gl_net_decode(L, a[i], ctx);
    return (int)a.size();
}

// ── Registro global de respuestas de RemoteFunction ───────────────────────
// El invocador (cliente) genera un call_id, cede su hilo y espera; cuando la
// respuesta llega por RPC, NetworkService la deposita aquí; el ScriptNodeBase
// que esperaba la recoge en su _process, la decodifica y reanuda el hilo.
struct GLNetResponse { Array rets; bool ready = false; };
static inline std::unordered_map<int64_t, GLNetResponse>& gl_net_responses() {
    static std::unordered_map<int64_t, GLNetResponse> m;
    return m;
}
static inline int64_t gl_net_new_call_id() {
    static int64_t next = 1;
    return next++;
}

#endif // GL_NET_BRIDGE_H
