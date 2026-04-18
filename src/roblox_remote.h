#ifndef ROBLOX_REMOTE_H
#define ROBLOX_REMOTE_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include "lua.h"
#include "lualib.h"
#include <vector>
#include <algorithm>

using namespace godot;

// ── Shared helper to copy a Lua value between states (primitives only) ──────
static void _lua_cross_push(lua_State* src, int idx, lua_State* dst) {
    int t = lua_type(src, idx);
    if      (t == LUA_TNIL)     lua_pushnil(dst);
    else if (t == LUA_TBOOLEAN) lua_pushboolean(dst, lua_toboolean(src, idx));
    else if (t == LUA_TNUMBER)  lua_pushnumber(dst, lua_tonumber(src, idx));
    else if (t == LUA_TSTRING)  lua_pushstring(dst, lua_tostring(src, idx));
    else lua_pushnil(dst);
}

struct LuaCB { lua_State* L; int ref; };

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
    static void _bind_methods() {}
public:
    std::vector<LuaCB> server_cbs;
    std::vector<LuaCB> client_cbs;

    void add_server_cb(lua_State* L, int ref) { server_cbs.push_back({L, ref}); }
    void add_client_cb(lua_State* L, int ref) { client_cbs.push_back({L, ref}); }

    // FireServer: call all server listeners with (player=nil, ...args)
    void fire_server(lua_State* L, int stack_base, int nargs) {
        for (auto& cb : server_cbs) {
            if (!cb.L) continue;
            lua_rawgeti(cb.L, LUA_REGISTRYINDEX, cb.ref);
            lua_pushnil(cb.L); // player = nil (same-machine simulation)
            for (int i = stack_base; i < stack_base + nargs; i++) {
                _lua_cross_push(L, i, cb.L);
            }
            lua_pcall(cb.L, 1 + nargs, 0, 0);
        }
    }

    // FireClient: call all client listeners with (...args)  (player ignored in single-machine)
    void fire_client(lua_State* L, int stack_base, int nargs) {
        for (auto& cb : client_cbs) {
            if (!cb.L) continue;
            lua_rawgeti(cb.L, LUA_REGISTRYINDEX, cb.ref);
            for (int i = stack_base; i < stack_base + nargs; i++) {
                _lua_cross_push(L, i, cb.L);
            }
            lua_pcall(cb.L, nargs, 0, 0);
        }
    }

    void cleanup_state(lua_State* L) {
        auto pred = [L](const LuaCB& c){ return c.L == L; };
        server_cbs.erase(std::remove_if(server_cbs.begin(), server_cbs.end(), pred), server_cbs.end());
        client_cbs.erase(std::remove_if(client_cbs.begin(), client_cbs.end(), pred), client_cbs.end());
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
        if (!server_invoke_L || server_invoke_ref == LUA_NOREF) {
            lua_pushnil(caller); return 1;
        }
        int pre_top = lua_gettop(server_invoke_L);
        lua_rawgeti(server_invoke_L, LUA_REGISTRYINDEX, server_invoke_ref);
        lua_pushnil(server_invoke_L);
        for (int i = stack_base; i < stack_base + nargs; i++)
            _lua_cross_push(caller, i, server_invoke_L);
        int ret_count = 0;
        if (lua_pcall(server_invoke_L, 1 + nargs, LUA_MULTRET, 0) == 0) {
            ret_count = lua_gettop(server_invoke_L) - pre_top;
            if (ret_count < 0) ret_count = 0;
            for (int i = 1; i <= ret_count; i++)
                _lua_cross_push(server_invoke_L, pre_top + i, caller);
            lua_settop(server_invoke_L, pre_top);
        } else {
            lua_settop(server_invoke_L, pre_top);
            lua_pushnil(caller); ret_count = 1;
        }
        return ret_count;
    }

    // InvokeClient: call client handler synchronously, return values to caller
    int invoke_client(lua_State* caller, int stack_base, int nargs) {
        if (!client_invoke_L || client_invoke_ref == LUA_NOREF) {
            lua_pushnil(caller); return 1;
        }
        int pre_top = lua_gettop(client_invoke_L);
        lua_rawgeti(client_invoke_L, LUA_REGISTRYINDEX, client_invoke_ref);
        for (int i = stack_base; i < stack_base + nargs; i++)
            _lua_cross_push(caller, i, client_invoke_L);
        int ret_count = 0;
        if (lua_pcall(client_invoke_L, nargs, LUA_MULTRET, 0) == 0) {
            ret_count = lua_gettop(client_invoke_L) - pre_top;
            if (ret_count < 0) ret_count = 0;
            for (int i = 1; i <= ret_count; i++)
                _lua_cross_push(client_invoke_L, pre_top + i, caller);
            lua_settop(client_invoke_L, pre_top);
        } else {
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
    static void _bind_methods() {}
public:
    std::vector<LuaCB> cbs;

    void add_cb(lua_State* L, int ref) { cbs.push_back({L, ref}); }

    void fire(lua_State* L, int stack_base, int nargs) {
        for (auto& cb : cbs) {
            if (!cb.L) continue;
            lua_rawgeti(cb.L, LUA_REGISTRYINDEX, cb.ref);
            for (int i = stack_base; i < stack_base + nargs; i++)
                _lua_cross_push(L, i, cb.L);
            lua_pcall(cb.L, nargs, 0, 0);
        }
    }

    void cleanup_state(lua_State* L) {
        auto pred = [L](const LuaCB& c){ return c.L == L; };
        cbs.erase(std::remove_if(cbs.begin(), cbs.end(), pred), cbs.end());
    }
};

#endif
