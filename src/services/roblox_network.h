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
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <vector>

#include "lua.h"
#include "lualib.h"
#include "gl_runtime.h"   // gl_state_alive

using namespace godot;

// ¿Se lanzó el juego desde la barra "Jugadores" del editor (con --glindex)?
// Solo entonces auto-creamos el NetworkService al arrancar; los juegos normales
// no reciben un servicio de red permanente que no pidieron.
static inline bool gl_mp_autostart_requested() {
    PackedStringArray a = OS::get_singleton()->get_cmdline_user_args();
    for (int i = 0; i < a.size(); i++) if (String(a[i]) == "--glindex") return true;
    return false;
}

class NetworkService : public Node {
    GDCLASS(NetworkService, Node);

    struct NCB { lua_State* L; int ref; bool active = true; };
    Ref<ENetMultiplayerPeer> peer;
    int mode = 0;   // 0 = SinglePlayer, 1 = Host/Server, 2 = Client
    bool signals_bound = false;
    std::vector<NCB> pc_cbs, pd_cbs, conn_cbs, fail_cbs;
    // Auto-arranque desde la barra de "Jugadores" del editor (multijugador local)
    String device = "PC";
    int  player_index = 0;
    bool net_connected = false;
    int  retry_left = 0;
    double retry_timer = 0.0;

    static Node* _find_by_class(Node* n, const char* cls) {
        if (!n) return nullptr;
        if (n->is_class(cls)) return n;
        for (int i = 0; i < n->get_child_count(); i++) {
            Node* r = _find_by_class(n->get_child(i), cls);
            if (r) return r;
        }
        return nullptr;
    }
    void _name_local_player(int idx) {
        if (!is_inside_tree()) return;
        Node* p = _find_by_class((Node*)get_tree()->get_root(), "RobloxPlayer");
        if (!p) p = _find_by_class((Node*)get_tree()->get_root(), "RobloxPlayer2D");
        if (p) {
            // Solo DisplayName (NO renombrar el nodo: el nombre "LocalPlayer" lo usa
            // el fallback de workspace.CurrentCamera; renombrarlo dejaría la cámara en nil).
            p->set("DisplayName", String("Player") + String::num_int64(idx));
            p->set_meta("PlayerIndex", idx);
        }
    }
    void _apply_device() {
        if (!is_inside_tree()) return;
        Window* w = get_tree()->get_root();
        if (device == "Mobile" && w) w->set_size(Vector2i(400, 820));   // aspecto de teléfono
        // Console/VR: sin efecto visual aún; el juego puede leer el modo con net:GetDevice().
    }

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
        ClassDB::bind_method(D_METHOD("_auto_init"),                 &NetworkService::_auto_init);
    }

public:
    void _on_peer_connected(int id)    { _fire(pc_cbs, true, id); }
    void _on_peer_disconnected(int id) { _fire(pd_cbs, true, id); }
    void _on_connected_ok()            { net_connected = true; _fire(conn_cbs, false, 0); }
    void _on_connect_failed()          { mode = 0; if (retry_left > 0) return; _fire(fail_cbs, false, 0); }

    void add_pc_cb(lua_State* L, int ref)   { pc_cbs.push_back({L, ref}); }
    void add_pd_cb(lua_State* L, int ref)   { pd_cbs.push_back({L, ref}); }
    void add_conn_cb(lua_State* L, int ref) { conn_cbs.push_back({L, ref}); }
    void add_fail_cb(lua_State* L, int ref) { fail_cbs.push_back({L, ref}); }

    void _ready() override {
        // Dedup: si ya existe otro NetworkService bajo el mismo padre, este sobra
        // (se puede crear tanto en Workspace._ready como perezosamente en GetService).
        Node* par = get_parent();
        if (par) {
            for (int i = 0; i < par->get_child_count(); i++) {
                Node* c = par->get_child(i);
                if (c != this && c->is_class("NetworkService")) { queue_free(); return; }
            }
        }
        _bind_mp_signals();
        call_deferred("_auto_init");
    }

    // Reintenta la conexión del cliente hasta que el host abra el puerto (~18s)
    void _process(double dt) override {
        if (retry_left <= 0) return;
        if (net_connected) { retry_left = 0; set_process(false); return; }
        retry_timer += dt;
        if (retry_timer >= 1.5) {
            retry_timer = 0.0;
            retry_left--;
            if (retry_left <= 0) { set_process(false); return; }
            connect_server("127.0.0.1", 25575);
        }
    }

    // Lee los args de arranque (--glindex/--glcount/--gldevice) que pasa la barra
    // "Jugadores" del editor y auto-conecta el multijugador local (host/clientes).
    void _auto_init() {
        PackedStringArray args = OS::get_singleton()->get_cmdline_user_args();
        int idx = 0, cnt = 1;
        for (int i = 0; i < args.size(); i++) {
            String a = args[i];
            if (a == "--glindex"  && i + 1 < args.size()) idx = String(args[i + 1]).to_int();
            else if (a == "--glcount"  && i + 1 < args.size()) cnt = String(args[i + 1]).to_int();
            else if (a == "--gldevice" && i + 1 < args.size()) device = args[i + 1];
        }
        player_index = idx;
        if (idx >= 1 && cnt >= 2) {
            if (idx == 1) start_server(25575, cnt);
            else { connect_server("127.0.0.1", 25575); retry_left = 12; set_process(true); }
        }
        if (idx >= 1) _name_local_player(idx);
        _apply_device();
    }
    int    get_player_index() const { return player_index; }
    String get_device() const { return device; }

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
