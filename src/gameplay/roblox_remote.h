#ifndef ROBLOX_REMOTE_H
#define ROBLOX_REMOTE_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include "lua.h"
#include "lualib.h"
#include "gl_errors.h"
#include "gl_runtime.h"   // gl_state_alive, GodotObjectWrapper, gow_set
#include "gl_net_bridge.h" // serializador Lua<->red (Fase 2)
#include "roblox_services.h" // LuauPendingResume + get_pending_resumes (para :Wait)
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
    Node* root = (Node*)from->get_tree()->get_root();
    // Preferir el OBJETO Player local (Player ≠ Character): OnServerEvent debe
    // recibir un Player real con UserId/Name/.Character, no el nodo-personaje.
    std::function<Node*(Node*)> rec_players = [&](Node* n) -> Node* {
        if (!n) return nullptr;
        if (n->is_class("Players")) return n;
        for (int i = 0; i < n->get_child_count(); i++) { Node* r = rec_players(n->get_child(i)); if (r) return r; }
        return nullptr;
    };
    if (Node* ps = rec_players(root)) {
        Variant lp = ps->call("get_local_player");
        Object* o = lp;
        if (Node* p = Object::cast_to<Node>(o)) return p;
    }
    // Fallback: el personaje (compat si aún no hay servicio Players)
    std::function<Node*(Node*)> rec_char = [&](Node* n) -> Node* {
        if (!n) return nullptr;
        if (n->is_class("RobloxPlayer") || n->is_class("RobloxPlayer2D")) return n;
        for (int i = 0; i < n->get_child_count(); i++) { Node* r = rec_char(n->get_child(i)); if (r) return r; }
        return nullptr;
    };
    return rec_char(root);
}

// Localiza el NetworkService (singleton bajo el DataModel) en el árbol.
// RemoteEvent lo usa para preguntar el modo de red y enviar por ENet, SIN
// incluir roblox_network.h (evita ciclo de includes): se habla por call().
static inline Node* _gl_find_network_service(Node* from) {
    if (!from || !from->is_inside_tree()) return nullptr;
    std::function<Node*(Node*)> rec = [&](Node* n) -> Node* {
        if (!n) return nullptr;
        if (n->is_class("NetworkService")) return n;
        for (int i = 0; i < n->get_child_count(); i++) {
            Node* r = rec(n->get_child(i));
            if (r) return r;
        }
        return nullptr;
    };
    return rec((Node*)from->get_tree()->get_root());
}

// Extrae el Node* envuelto en un argumento Luau (GodotObject) o nullptr.
static inline Node* _gl_node_from_lua(lua_State* L, int idx) {
    if (lua_type(L, idx) != LUA_TUSERDATA) return nullptr;
    if (!lua_getmetatable(L, idx)) return nullptr;
    luaL_getmetatable(L, "GodotObject");
    bool ok = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);
    if (!ok) return nullptr;
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, idx);
    return w ? gow_node(w) : nullptr;
}

// Devuelve el modo de red del NetworkService (0 single / 1 host / 2 client).
static inline int _gl_net_mode(Node* ns) {
    if (!ns) return 0;
    Variant m = ns->call("get_mode");
    return (int)m;
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

struct LuaCB { lua_State* L; int ref; bool active = true; bool once = false; };
// Corrutina suspendida en OnServerEvent:Wait()/OnClientEvent:Wait().
struct LuaWait { lua_State* th; lua_State* main_L; };

// Empaqueta un nodo como referencia de red (para pasar el Player en :Wait server).
static inline Dictionary _gl_inst_dict(Node* nd) {
    Dictionary d;
    d["__glnet"] = "inst";
    d["p"] = (nd && nd->is_inside_tree()) ? String(nd->get_path()) : String();
    return d;
}

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
    std::vector<LuaWait> server_wait_cbs;   // OnServerEvent:Wait()
    std::vector<LuaWait> client_wait_cbs;   // OnClientEvent:Wait()
    bool reliable = true;   // false en UnreliableRemoteEvent (canal no confiable)

    void add_server_cb(lua_State* L, int ref, bool once = false) { server_cbs.push_back({L, ref, true, once}); }
    void add_client_cb(lua_State* L, int ref, bool once = false) { client_cbs.push_back({L, ref, true, once}); }
    void add_server_wait(lua_State* th, lua_State* mL) { server_wait_cbs.push_back({th, mL}); }
    void add_client_wait(lua_State* th, lua_State* mL) { client_wait_cbs.push_back({th, mL}); }

    // Reanuda (diferido, vía cola compartida) las corrutinas en :Wait() con los
    // args del evento. ctx = este remote, para resolver Instances en el destino.
    void _queue_waits(std::vector<LuaWait>& waits, const Array& args_arr, Node* ctx) {
        uint64_t ctx_id = ctx ? (uint64_t)ctx->get_instance_id() : 0;
        for (auto& w : waits) {
            if (!gl_state_alive(w.main_L)) continue;
            LuauPendingResume pr;
            pr.thread = w.th; pr.main_L = w.main_L; pr.delta = 0; pr.node_arg = nullptr;
            pr.args = args_arr; pr.ctx_id = ctx_id; pr.use_args = true;
            get_pending_resumes().push_back(pr);
        }
        waits.clear();
    }

    // Elimina (y libera el ref) las conexiones inactivas tras un dispatch, para
    // que :Once y Connection:Disconnect no acumulen entradas muertas ni fuguen refs.
    static void _compact_cbs(std::vector<LuaCB>& v) {
        for (int i = (int)v.size() - 1; i >= 0; --i) {
            if (!v[i].active) {
                if (gl_state_alive(v[i].L)) lua_unref(v[i].L, v[i].ref);
                v.erase(v.begin() + i);
            }
        }
    }

    // ── Entrega LOCAL (mismo proceso) ───────────────────────────────────────
    // Cada listener corre en su propio hilo (como el resto del motor) para que
    // pueda usar wait/task.wait sin "yield across C-call boundary".
    void _run_server_cbs(lua_State* L, int stack_base, int nargs, Node* player) {
        std::vector<LuaCB> snapshot = server_cbs;  // copia defensiva: un :Connect dentro del handler no invalida el bucle
        for (auto& cb : snapshot) {
            if (!cb.active || !gl_state_alive(cb.L)) continue;
            lua_State* th = lua_newthread(cb.L);
            lua_rawgeti(cb.L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.L, -1)) {
                lua_xmove(cb.L, th, 1);
                _gl_push_node(th, player);              // arg1: player que disparó
                for (int i = stack_base; i < stack_base + nargs; i++)
                    _lua_cross_push(L, i, th);
                if (cb.once) _gl_disconnect(cb.ref);    // :Once → desconectar ANTES de invocar (como Roblox)
                gl_check_resume(th, lua_resume(th, nullptr, 1 + nargs));
            } else lua_pop(cb.L, 1);
            lua_pop(cb.L, 1);
        }
        _compact_cbs(server_cbs);
        if (!server_wait_cbs.empty()) {                 // OnServerEvent:Wait() → (player, ...)
            Array wa; wa.push_back(_gl_inst_dict(player));
            Array ea = gl_net_encode_args(L, stack_base, nargs);
            for (int i = 0; i < ea.size(); i++) wa.push_back(ea[i]);
            _queue_waits(server_wait_cbs, wa, this);
        }
    }
    void _run_client_cbs(lua_State* L, int stack_base, int nargs) {
        std::vector<LuaCB> snapshot = client_cbs;  // copia defensiva (ver _run_server_cbs)
        for (auto& cb : snapshot) {
            if (!cb.active || !gl_state_alive(cb.L)) continue;
            lua_State* th = lua_newthread(cb.L);
            lua_rawgeti(cb.L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.L, -1)) {
                lua_xmove(cb.L, th, 1);
                for (int i = stack_base; i < stack_base + nargs; i++)
                    _lua_cross_push(L, i, th);
                if (cb.once) _gl_disconnect(cb.ref);    // :Once → desconectar ANTES de invocar (como Roblox)
                gl_check_resume(th, lua_resume(th, nullptr, nargs));
            } else lua_pop(cb.L, 1);
            lua_pop(cb.L, 1);
        }
        _compact_cbs(client_cbs);
        if (!client_wait_cbs.empty()) {                 // OnClientEvent:Wait() → (...)
            Array ea = gl_net_encode_args(L, stack_base, nargs);
            _queue_waits(client_wait_cbs, ea, this);
        }
    }

    // ── Entrega desde la RED (args ya llegaron como Array) ──────────────────
    // Las invoca el NetworkService al recibir el RPC en la máquina destino.
    void deliver_to_server_cbs(const Array& args, Node* player, Node* ctx) {
        std::vector<LuaCB> snapshot = server_cbs;  // copia defensiva (ver _run_server_cbs)
        for (auto& cb : snapshot) {
            if (!cb.active || !gl_state_alive(cb.L)) continue;
            lua_State* th = lua_newthread(cb.L);
            lua_rawgeti(cb.L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.L, -1)) {
                lua_xmove(cb.L, th, 1);
                _gl_push_node(th, player);
                int n = gl_net_decode_args(th, args, ctx);
                if (cb.once) _gl_disconnect(cb.ref);
                gl_check_resume(th, lua_resume(th, nullptr, 1 + n));
            } else lua_pop(cb.L, 1);
            lua_pop(cb.L, 1);
        }
        _compact_cbs(server_cbs);
        if (!server_wait_cbs.empty()) {
            Array wa; wa.push_back(_gl_inst_dict(player));
            for (int i = 0; i < args.size(); i++) wa.push_back(args[i]);
            _queue_waits(server_wait_cbs, wa, ctx);
        }
    }
    void deliver_to_client_cbs(const Array& args, Node* ctx) {
        std::vector<LuaCB> snapshot = client_cbs;  // copia defensiva (ver _run_server_cbs)
        for (auto& cb : snapshot) {
            if (!cb.active || !gl_state_alive(cb.L)) continue;
            lua_State* th = lua_newthread(cb.L);
            lua_rawgeti(cb.L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.L, -1)) {
                lua_xmove(cb.L, th, 1);
                int n = gl_net_decode_args(th, args, ctx);
                if (cb.once) _gl_disconnect(cb.ref);
                gl_check_resume(th, lua_resume(th, nullptr, n));
            } else lua_pop(cb.L, 1);
            lua_pop(cb.L, 1);
        }
        _compact_cbs(client_cbs);
        if (!client_wait_cbs.empty()) _queue_waits(client_wait_cbs, args, ctx);
    }

    // ── API pública (desde Luau) — decide red vs local según el modo ────────
    // FireServer: cliente → servidor. En host/single entrega localmente.
    void fire_server(lua_State* L, int stack_base, int nargs) {
        Node* ns = _gl_find_network_service(this);
        if (_gl_net_mode(ns) == 2 && is_inside_tree()) {
            Array args = gl_net_encode_args(L, stack_base, nargs);
            ns->call("net_send_event", String(get_path()), args, 0, reliable);
            return;
        }
        _run_server_cbs(L, stack_base, nargs, _gl_find_local_player(this));
    }
    // FireClient(player, ...): servidor → un cliente concreto.
    void fire_client(lua_State* L, int player_idx, int stack_base, int nargs) {
        Node* ns = _gl_find_network_service(this);
        int mode = _gl_net_mode(ns);
        if (mode == 0 || !ns || !is_inside_tree()) { _run_client_cbs(L, stack_base, nargs); return; }
        if (mode == 2) return;   // un cliente no puede FireClient
        Node* player = _gl_node_from_lua(L, player_idx);
        if (!player) return;     // FireClient(nil/no-nodo): descartar (Roblox lo trataría como error)
        int peer = (int)(ns->call("peer_for_player_node", player));
        int myid = (int)(ns->call("get_peer_id"));
        if (peer == myid && myid != 0) { _run_client_cbs(L, stack_base, nargs); return; }  // el jugador del host: local
        if (peer == 0) return;   // destino no resuelto o referencia obsoleta: NO entregar local ni enviar
        Array args = gl_net_encode_args(L, stack_base, nargs);
        ns->call("net_send_event", String(get_path()), args, peer, reliable);
    }
    // FireAllClients(...): servidor → todos los clientes (incl. el host).
    void fire_all_clients(lua_State* L, int stack_base, int nargs) {
        Node* ns = _gl_find_network_service(this);
        int mode = _gl_net_mode(ns);
        if (mode == 2) return;   // un cliente no puede FireAllClients
        _run_client_cbs(L, stack_base, nargs);   // host/single: entrega local
        if (mode == 1 && is_inside_tree()) {
            Array args = gl_net_encode_args(L, stack_base, nargs);
            ns->call("net_send_event", String(get_path()), args, -1, reliable);
        }
    }

    void cleanup_state(lua_State* L) {
        auto pred = [L](const LuaCB& c){ return c.L == L; };
        server_cbs.erase(std::remove_if(server_cbs.begin(), server_cbs.end(), pred), server_cbs.end());
        client_cbs.erase(std::remove_if(client_cbs.begin(), client_cbs.end(), pred), client_cbs.end());
        auto wpred = [L](const LuaWait& w){ return w.main_L == L; };
        server_wait_cbs.erase(std::remove_if(server_wait_cbs.begin(), server_wait_cbs.end(), wpred), server_wait_cbs.end());
        client_wait_cbs.erase(std::remove_if(client_wait_cbs.begin(), client_wait_cbs.end(), wpred), client_wait_cbs.end());
    }
    void _gl_disconnect(int ref) {
        for (auto& cb : server_cbs) if (cb.ref == ref) cb.active = false;
        for (auto& cb : client_cbs) if (cb.ref == ref) cb.active = false;
    }
};

// ════════════════════════════════════════════════════════════════════
//  UnreliableRemoteEventNode — igual que RemoteEvent pero por canal NO
//  confiable (para estado de alta frecuencia que puede perder paquetes).
// ════════════════════════════════════════════════════════════════════
class UnreliableRemoteEventNode : public RemoteEventNode {
    GDCLASS(UnreliableRemoteEventNode, RemoteEventNode);
protected:
    static void _bind_methods() {}
public:
    UnreliableRemoteEventNode() { reliable = false; }
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
        std::vector<LuaCB> snapshot = cbs;  // copia defensiva: un :Connect dentro del handler no invalida el bucle
        for (auto& cb : snapshot) {
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
