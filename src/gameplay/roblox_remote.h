#ifndef ROBLOX_REMOTE_H
#define ROBLOX_REMOTE_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include "lua.h"
#include "lualib.h"
#include "gl_errors.h"
#include "gl_runtime.h"   // gl_state_alive, GodotObjectWrapper, gow_set
#include <vector>
#include <algorithm>
#include <functional>
#include <cstring>

using namespace godot;

// Busca el "jugador local" (RobloxPlayer / RobloxPlayer2D) en el árbol.
// En el modelo de una sola máquina, FireServer entrega este jugador al
// handler OnServerEvent (en Roblox el server recibe SIEMPRE el player que disparó).
static inline Node* _gl_find_local_player(Node* from) {
    if (!from || !from->is_inside_tree()) return nullptr;
    std::function<Node*(Node*)> rec = [&](Node* n) -> Node* {
        if (!n) return nullptr;
        if (n->is_class("RobloxPlayer") || n->is_class("RobloxPlayer2D")) return n;
        for (int i = 0; i < n->get_child_count(); i++) {
            Node* r = rec(n->get_child(i));
            if (r) return r;
        }
        return nullptr;
    };
    return rec((Node*)from->get_tree()->get_root());
}

// Empuja un nodo como GodotObject (o nil) en el estado dst.
static inline void _gl_push_node(lua_State* dst, Node* node) {
    if (!node) { lua_pushnil(dst); return; }
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_newuserdata(dst, sizeof(GodotObjectWrapper));
    gow_set(w, node);
    luaL_getmetatable(dst, "GodotObject");
    lua_setmetatable(dst, -2);
}

// ── Shared helper to copy a Lua value between states ────────────────────────
// Copia primitivos Y tablas (recursivo, máx. 8 niveles) — como Roblox, que
// serializa tablas a través de los RemoteEvents.
static void _lua_cross_push(lua_State* src, int idx, lua_State* dst, int depth = 0) {
    int t = lua_type(src, idx);
    if (idx < 0 && (t == LUA_TTABLE || t == LUA_TUSERDATA)) idx = lua_gettop(src) + idx + 1;
    if      (t == LUA_TNIL)     lua_pushnil(dst);
    else if (t == LUA_TBOOLEAN) lua_pushboolean(dst, lua_toboolean(src, idx));
    else if (t == LUA_TNUMBER)  lua_pushnumber(dst, lua_tonumber(src, idx));
    else if (t == LUA_TSTRING)  lua_pushstring(dst, lua_tostring(src, idx));
    else if (t == LUA_TTABLE && depth < 8) {
        lua_newtable(dst);
        lua_pushnil(src);
        while (lua_next(src, idx) != 0) {
            _lua_cross_push(src, -2, dst, depth + 1);  // clave
            _lua_cross_push(src, -1, dst, depth + 1);  // valor
            lua_settable(dst, -3);
            lua_pop(src, 1);
        }
        // Preservar la metatable de tablas tipadas (CFrame, UDim2, UDim, Region3, Rect…)
        if (lua_getmetatable(src, idx)) {
            lua_getfield(src, -1, "__type");
            if (lua_isstring(src, -1)) {
                lua_getglobal(dst, lua_tostring(src, -1));
                if (lua_istable(dst, -1)) lua_setmetatable(dst, -2);
                else lua_pop(dst, 1);
            }
            lua_pop(src, 2);  // __type + metatable
        }
    }
    else if (t == LUA_TUSERDATA) {
        bool done = false;
        if (lua_getmetatable(src, idx)) {
            // Instance (GodotObject) → recrear en dst por ObjectID
            luaL_getmetatable(src, "GodotObject");
            bool is_obj = lua_rawequal(src, -1, -2);
            lua_pop(src, 1);
            if (is_obj) {
                GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(src, idx);
                _gl_push_node(dst, w ? gow_node(w) : nullptr);
                done = true;
            } else {
                // Vector3 / Color3 (userdata de 3 floats)
                static const char* uds[] = { "Vector3", "Color3", nullptr };
                for (int m = 0; uds[m] && !done; m++) {
                    luaL_getmetatable(src, uds[m]);
                    bool eq = lua_rawequal(src, -1, -2);
                    lua_pop(src, 1);
                    if (eq) {
                        void* s = lua_touserdata(src, idx);
                        void* d = lua_newuserdata(dst, 3 * sizeof(float));
                        memcpy(d, s, 3 * sizeof(float));
                        luaL_getmetatable(dst, uds[m]);
                        lua_setmetatable(dst, -2);
                        done = true;
                    }
                }
            }
            lua_pop(src, 1);  // metatable original
        }
        if (!done) lua_pushnil(dst);
    }
    else lua_pushnil(dst);
}

struct LuaCB { lua_State* L; int ref; bool active = true; };

// ════════════════════════════════════════════════════════════════════
//  RemoteEventNode — client→server / server→client events
//  Roblox API:
//    :FireServer(...)           client → server
//    :FireClient(player, ...)   server → one client
//    :FireAllClients(...)       server → all clients
//    .OnServerEvent:Connect(fn) server subscribes
//    .OnClientEvent:Connect(fn) client subscribes
// ════════════════════════════════════════════════════════════════════
class RemoteEventNode : public Node {
    GDCLASS(RemoteEventNode, Node);
protected:
    static void _bind_methods() { ClassDB::bind_method(D_METHOD("_gl_disconnect","ref"), &RemoteEventNode::_gl_disconnect); }
public:
    std::vector<LuaCB> server_cbs;
    std::vector<LuaCB> client_cbs;

    void add_server_cb(lua_State* L, int ref) { server_cbs.push_back({L, ref}); }
    void add_client_cb(lua_State* L, int ref) { client_cbs.push_back({L, ref}); }

    // FireServer: call all server listeners with (player, ...args).
    // El primer argumento es el jugador que disparó (igual que Roblox); en el
    // modelo de una sola máquina es el LocalPlayer. Antes se pasaba nil y la
    // plantilla GameManager petaba al hacer player.Name.
    void fire_server(lua_State* L, int stack_base, int nargs) {
        Node* player = _gl_find_local_player(this);
        for (auto& cb : server_cbs) {
            if (!cb.active || !gl_state_alive(cb.L)) continue;
            // Cada handler corre en su propio hilo (como el resto del motor):
            // así puede usar wait/task.wait sin "yield across C-call boundary".
            lua_State* th = lua_newthread(cb.L);
            lua_rawgeti(cb.L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.L, -1)) {
                lua_xmove(cb.L, th, 1);
                _gl_push_node(th, player);              // arg1: player (LocalPlayer) o nil
                for (int i = stack_base; i < stack_base + nargs; i++)
                    _lua_cross_push(L, i, th);
                gl_check_resume(th, lua_resume(th, nullptr, 1 + nargs));
            } else lua_pop(cb.L, 1);
            lua_pop(cb.L, 1);  // quitar el hilo
        }
    }

    // FireClient: call all client listeners with (...args)  (player ignored in single-machine)
    void fire_client(lua_State* L, int stack_base, int nargs) {
        for (auto& cb : client_cbs) {
            if (!cb.active || !gl_state_alive(cb.L)) continue;
            lua_State* th = lua_newthread(cb.L);
            lua_rawgeti(cb.L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.L, -1)) {
                lua_xmove(cb.L, th, 1);
                for (int i = stack_base; i < stack_base + nargs; i++)
                    _lua_cross_push(L, i, th);
                gl_check_resume(th, lua_resume(th, nullptr, nargs));
            } else lua_pop(cb.L, 1);
            lua_pop(cb.L, 1);
        }
    }

    void cleanup_state(lua_State* L) {
        auto pred = [L](const LuaCB& c){ return c.L == L; };
        server_cbs.erase(std::remove_if(server_cbs.begin(), server_cbs.end(), pred), server_cbs.end());
        client_cbs.erase(std::remove_if(client_cbs.begin(), client_cbs.end(), pred), client_cbs.end());
    }
    void _gl_disconnect(int ref) {
        for (auto& cb : server_cbs) if (cb.ref == ref) cb.active = false;
        for (auto& cb : client_cbs) if (cb.ref == ref) cb.active = false;
    }
};

// ════════════════════════════════════════════════════════════════════
//  RemoteFunctionNode — request/response across client↔server
//  Roblox API:
//    :InvokeServer(...)          client calls → server handler → return value
//    :InvokeClient(player, ...)  server calls → client handler → return value
//    .OnServerInvoke = function  server handler (set, not Connect)
//    .OnClientInvoke = function  client handler (set, not Connect)
// ════════════════════════════════════════════════════════════════════
class RemoteFunctionNode : public Node {
    GDCLASS(RemoteFunctionNode, Node);
protected:
    static void _bind_methods() {}
public:
    lua_State* server_invoke_L = nullptr;
    int        server_invoke_ref = LUA_NOREF;

    lua_State* client_invoke_L = nullptr;
    int        client_invoke_ref = LUA_NOREF;

    void set_server_invoke(lua_State* L, int ref) {
        if (server_invoke_L && server_invoke_ref != LUA_NOREF)
            lua_unref(server_invoke_L, server_invoke_ref);
        server_invoke_L = L; server_invoke_ref = ref;
    }

    void set_client_invoke(lua_State* L, int ref) {
        if (client_invoke_L && client_invoke_ref != LUA_NOREF)
            lua_unref(client_invoke_L, client_invoke_ref);
        client_invoke_L = L; client_invoke_ref = ref;
    }

    // InvokeServer: call server handler synchronously, return values to caller
    int invoke_server(lua_State* caller, int stack_base, int nargs) {
        if (!gl_state_alive(server_invoke_L) || server_invoke_ref == LUA_NOREF) {
            lua_pushnil(caller); return 1;
        }
        int pre_top = lua_gettop(server_invoke_L);
        lua_pushcfunction(server_invoke_L, gl_trace_handler, "errh");   // captura el stack del handler
        lua_rawgeti(server_invoke_L, LUA_REGISTRYINDEX, server_invoke_ref);
        lua_pushnil(server_invoke_L);
        for (int i = stack_base; i < stack_base + nargs; i++)
            _lua_cross_push(caller, i, server_invoke_L);
        int ret_count = 0;
        if (lua_pcall(server_invoke_L, 1 + nargs, LUA_MULTRET, pre_top + 1) == 0) {
            ret_count = lua_gettop(server_invoke_L) - (pre_top + 1);
            if (ret_count < 0) ret_count = 0;
            for (int i = 1; i <= ret_count; i++)
                _lua_cross_push(server_invoke_L, pre_top + 1 + i, caller);
            lua_settop(server_invoke_L, pre_top);
        } else {
            gl_report_script_error_with_trace(server_invoke_L, gl_pcall_trace());
            lua_settop(server_invoke_L, pre_top);
            lua_pushnil(caller); ret_count = 1;
        }
        return ret_count;
    }

    // InvokeClient: call client handler synchronously, return values to caller
    int invoke_client(lua_State* caller, int stack_base, int nargs) {
        if (!gl_state_alive(client_invoke_L) || client_invoke_ref == LUA_NOREF) {
            lua_pushnil(caller); return 1;
        }
        int pre_top = lua_gettop(client_invoke_L);
        lua_pushcfunction(client_invoke_L, gl_trace_handler, "errh");   // captura el stack del handler
        lua_rawgeti(client_invoke_L, LUA_REGISTRYINDEX, client_invoke_ref);
        for (int i = stack_base; i < stack_base + nargs; i++)
            _lua_cross_push(caller, i, client_invoke_L);
        int ret_count = 0;
        if (lua_pcall(client_invoke_L, nargs, LUA_MULTRET, pre_top + 1) == 0) {
            ret_count = lua_gettop(client_invoke_L) - (pre_top + 1);
            if (ret_count < 0) ret_count = 0;
            for (int i = 1; i <= ret_count; i++)
                _lua_cross_push(client_invoke_L, pre_top + 1 + i, caller);
            lua_settop(client_invoke_L, pre_top);
        } else {
            gl_report_script_error_with_trace(client_invoke_L, gl_pcall_trace());
            lua_settop(client_invoke_L, pre_top);
            lua_pushnil(caller); ret_count = 1;
        }
        return ret_count;
    }

    void cleanup_state(lua_State* L) {
        if (server_invoke_L == L) { server_invoke_L = nullptr; server_invoke_ref = LUA_NOREF; }
        if (client_invoke_L == L) { client_invoke_L = nullptr; client_invoke_ref = LUA_NOREF; }
    }
};

// ════════════════════════════════════════════════════════════════════
//  BindableEventNode — same-side (server↔server or client↔client)
//  Simpler than RemoteEvent: no player arg, same Lua state.
//  Roblox API: :Fire(...) / .Event:Connect(fn)
// ════════════════════════════════════════════════════════════════════
class BindableEventNode : public Node {
    GDCLASS(BindableEventNode, Node);
protected:
    static void _bind_methods() { ClassDB::bind_method(D_METHOD("_gl_disconnect","ref"), &BindableEventNode::_gl_disconnect); }
public:
    std::vector<LuaCB> cbs;

    void add_cb(lua_State* L, int ref) { cbs.push_back({L, ref}); }
    void _gl_disconnect(int ref) { for (auto& cb : cbs) if (cb.ref == ref) cb.active = false; }

    void fire(lua_State* L, int stack_base, int nargs) {
        for (auto& cb : cbs) {
            if (!cb.active || !gl_state_alive(cb.L)) continue;
            lua_State* th = lua_newthread(cb.L);
            lua_rawgeti(cb.L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.L, -1)) {
                lua_xmove(cb.L, th, 1);
                for (int i = stack_base; i < stack_base + nargs; i++)
                    _lua_cross_push(L, i, th);
                gl_check_resume(th, lua_resume(th, nullptr, nargs));
            } else lua_pop(cb.L, 1);
            lua_pop(cb.L, 1);
        }
    }

    void cleanup_state(lua_State* L) {
        auto pred = [L](const LuaCB& c){ return c.L == L; };
        cbs.erase(std::remove_if(cbs.begin(), cbs.end(), pred), cbs.end());
    }
};

#endif
