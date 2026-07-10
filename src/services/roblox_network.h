#ifndef ROBLOX_NETWORK_H
#define ROBLOX_NETWORK_H

// ════════════════════════════════════════════════════════════════════
//  NetworkService — Fase 0 del multijugador: conectividad real (ENet/UDP)
//  usando la red nativa de Godot. El usuario elige el modo:
//    net:StartServer(port, maxPlayers)   → tu PC como servidor (host)
//    net:Connect(ip, port)               → unirse a un servidor
//    net:Disconnect()                    → volver a un jugador
//  Señales (estilo Roblox): PlayerConnected(peerId), PlayerDisconnected(peerId),
//           Connected(), ConnectionFailed().
//  (Los RemoteEvent por red y la réplica llegan en fases siguientes.)
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/multiplayer_api.hpp>
#include <godot_cpp/classes/multiplayer_peer.hpp>
#include <godot_cpp/classes/e_net_multiplayer_peer.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <vector>

#include "lua.h"
#include "lualib.h"
#include "gl_runtime.h"   // gl_state_alive

using namespace godot;

class NetworkService : public Node {
    GDCLASS(NetworkService, Node);

    struct NCB { lua_State* L; int ref; bool active = true; };
    Ref<ENetMultiplayerPeer> peer;
    int mode = 0;   // 0 = SinglePlayer, 1 = Host/Server, 2 = Client
    bool signals_bound = false;
    std::vector<NCB> pc_cbs, pd_cbs, conn_cbs, fail_cbs;

    Ref<MultiplayerAPI> _mp() {
        if (!is_inside_tree()) return Ref<MultiplayerAPI>();
        return get_tree()->get_multiplayer();
    }
    void _bind_mp_signals() {
        if (signals_bound) return;
        Ref<MultiplayerAPI> m = _mp();
        if (m.is_null()) return;
        m->connect("peer_connected",      Callable(this, "_on_peer_connected"));
        m->connect("peer_disconnected",   Callable(this, "_on_peer_disconnected"));
        m->connect("connected_to_server", Callable(this, "_on_connected_ok"));
        m->connect("connection_failed",   Callable(this, "_on_connect_failed"));
        signals_bound = true;
    }
    // Dispara los callbacks Luau (cada uno en su propio hilo, como el resto del motor)
    static void _fire(std::vector<NCB>& cbs, bool has_id, int id) {
        for (auto& cb : cbs) {
            if (!cb.active || !gl_state_alive(cb.L)) continue;
            lua_State* th = lua_newthread(cb.L);
            lua_rawgeti(cb.L, LUA_REGISTRYINDEX, cb.ref);
            if (lua_isfunction(cb.L, -1)) {
                lua_xmove(cb.L, th, 1);
                int nargs = 0;
                if (has_id) { lua_pushinteger(th, id); nargs = 1; }
                lua_resume(th, nullptr, nargs);
            } else lua_pop(cb.L, 1);
            lua_pop(cb.L, 1);
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("_on_peer_connected", "id"),    &NetworkService::_on_peer_connected);
        ClassDB::bind_method(D_METHOD("_on_peer_disconnected", "id"), &NetworkService::_on_peer_disconnected);
        ClassDB::bind_method(D_METHOD("_on_connected_ok"),           &NetworkService::_on_connected_ok);
        ClassDB::bind_method(D_METHOD("_on_connect_failed"),         &NetworkService::_on_connect_failed);
    }

public:
    void _on_peer_connected(int id)    { _fire(pc_cbs, true, id); }
    void _on_peer_disconnected(int id) { _fire(pd_cbs, true, id); }
    void _on_connected_ok()            { _fire(conn_cbs, false, 0); }
    void _on_connect_failed()          { mode = 0; _fire(fail_cbs, false, 0); }

    void add_pc_cb(lua_State* L, int ref)   { pc_cbs.push_back({L, ref}); }
    void add_pd_cb(lua_State* L, int ref)   { pd_cbs.push_back({L, ref}); }
    void add_conn_cb(lua_State* L, int ref) { conn_cbs.push_back({L, ref}); }
    void add_fail_cb(lua_State* L, int ref) { fail_cbs.push_back({L, ref}); }

    void _ready() override { _bind_mp_signals(); }

    // ── API para Luau ────────────────────────────────────────────────
    bool start_server(int port, int max_players) {
        _bind_mp_signals();
        peer.instantiate();
        Error e = peer->create_server(port, max_players > 0 ? max_players : 32);
        if (e != OK) { UtilityFunctions::push_warning(String("[NetworkService] No se pudo abrir el servidor en el puerto ") + String::num_int64(port)); return false; }
        Ref<MultiplayerAPI> m = _mp();
        if (m.is_valid()) m->set_multiplayer_peer(peer);
        mode = 1;
        return true;
    }
    bool connect_server(const String& ip, int port) {
        _bind_mp_signals();
        peer.instantiate();
        Error e = peer->create_client(ip, port);
        if (e != OK) { UtilityFunctions::push_warning(String("[NetworkService] No se pudo conectar a ") + ip); return false; }
        Ref<MultiplayerAPI> m = _mp();
        if (m.is_valid()) m->set_multiplayer_peer(peer);
        mode = 2;
        return true;
    }
    void disconnect_net() {
        Ref<MultiplayerAPI> m = _mp();
        if (m.is_valid()) m->set_multiplayer_peer(Ref<MultiplayerPeer>());
        peer = Ref<ENetMultiplayerPeer>();
        mode = 0;
    }
    bool is_server() {
        Ref<MultiplayerAPI> m = _mp();
        return m.is_valid() && m->has_multiplayer_peer() && m->is_server();
    }
    bool is_client() {
        Ref<MultiplayerAPI> m = _mp();
        return m.is_valid() && m->has_multiplayer_peer() && !m->is_server();
    }
    int get_mode() const { return mode; }
    int get_peer_id() {
        Ref<MultiplayerAPI> m = _mp();
        return (m.is_valid() && m->has_multiplayer_peer()) ? m->get_unique_id() : 0;
    }
    int get_player_count() {
        Ref<MultiplayerAPI> m = _mp();
        if (m.is_null() || !m->has_multiplayer_peer()) return 1;
        return (int)m->get_peers().size() + 1;   // + uno mismo
    }

    NetworkService() {}
};

#endif // ROBLOX_NETWORK_H
