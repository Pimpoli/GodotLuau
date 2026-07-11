#ifndef ROBLOX_NETWORK_H
#define ROBLOX_NETWORK_H

// ════════════════════════════════════════════════════════════════════
//  NetworkService — multijugador local estilo Roblox Studio.
//
//  Fase 0: conectividad real (ENet/UDP) con la red nativa de Godot.
//    net:StartServer(port, maxPlayers)   → tu PC como servidor (host)
//    net:Connect(ip, port)               → unirse a un servidor
//    net:Disconnect()                    → volver a un jugador
//
//  Fase 1 (ESTE archivo): RÉPLICA de jugadores. Cada instancia difunde la
//  posición de su propio personaje y ve a los demás como "muñecos" (puppets)
//  que se mueven en tiempo real y tienen colisión (te chocas con ellos).
//  Topología estrella con relay por el servidor: todos ven a todos.
//
//  Arranque automático: cuando el usuario pulsa el botón Play nativo con
//  "Players" >= 2 en la barra del editor, se lanzan N instancias:
//    · la instancia nativa (sin --glindex) lee user://gl_mp_session.gl y
//      se vuelve el HOST (index 1),
//    · las demás se lanzan con --glindex 2..N y se conectan como clientes.
//
//  Señales (estilo Roblox): PlayerConnected(peerId), PlayerDisconnected(peerId),
//           Connected(), ConnectionFailed().
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/multiplayer_api.hpp>
#include <godot_cpp/classes/multiplayer_peer.hpp>
#include <godot_cpp/classes/e_net_multiplayer_peer.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/animatable_body3d.hpp>
#include <godot_cpp/classes/animatable_body2d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/capsule_shape2d.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <vector>
#include <map>

#include "lua.h"
#include "lualib.h"
#include "gl_runtime.h"   // gl_state_alive

using namespace godot;

// ── Sesión de multijugador escrita por la barra del editor ───────────────
// El botón Play nativo no puede pasarle argumentos a la instancia que él lanza,
// así que la barra escribe user://gl_mp_session.gl con "count|device|stamp".
// La instancia nativa (sin --glindex) lo lee y se convierte en el HOST.
static inline bool gl_mp_session_read(int& out_count, String& out_device) {
    Ref<FileAccess> f = FileAccess::open("user://gl_mp_session.gl", FileAccess::READ);
    if (f.is_null()) return false;
    String txt = f->get_as_text();
    f->close();
    PackedStringArray parts = txt.strip_edges().split("|");
    if (parts.size() < 3) return false;
    int    cnt   = String(parts[0]).to_int();
    String dev   = parts[1];
    double stamp = String(parts[2]).to_float();
    double now   = Time::get_singleton()->get_unix_time_from_system();
    if (now - stamp > 45.0) return false;   // sesión vieja (corrida anterior)
    if (cnt < 2) return false;
    out_count = cnt;
    out_device = dev;
    return true;
}
static inline bool gl_mp_session_active() {
    int c; String d;
    return gl_mp_session_read(c, d);
}
static inline void gl_mp_session_consume() {
    Ref<DirAccess> d = DirAccess::open("user://");
    if (d.is_valid() && d->file_exists("gl_mp_session.gl")) d->remove("gl_mp_session.gl");
}

// ¿Debe auto-crearse el NetworkService al arrancar el juego?
//   · clientes: se lanzan con --glindex
//   · host nativo: hay una sesión fresca escrita por la barra del editor
static inline bool gl_mp_autostart_requested() {
    PackedStringArray a = OS::get_singleton()->get_cmdline_user_args();
    for (int i = 0; i < a.size(); i++) if (String(a[i]) == "--glindex") return true;
    return gl_mp_session_active();
}

class NetworkService : public Node {
    GDCLASS(NetworkService, Node);

    struct NCB { lua_State* L; int ref; bool active = true; };
    struct Puppet {
        Node*   node = nullptr;
        Vector3 tpos;                 // objetivo 3D
        Vector2 tpos2d;               // objetivo 2D
        float   tyaw = 0.0f;
        bool    has_target = false;
    };

    Ref<ENetMultiplayerPeer> peer;
    int  mode = 0;   // 0 = SinglePlayer, 1 = Host/Server, 2 = Client
    bool signals_bound = false;
    std::vector<NCB> pc_cbs, pd_cbs, conn_cbs, fail_cbs;

    // Auto-arranque local
    String device = "PC";
    int    player_index = 0;
    bool   net_connected = false;
    int    retry_left = 0;
    double retry_timer = 0.0;

    // Réplica de jugadores
    bool   is_2d = false;
    double bcast_timer = 0.0;
    std::map<int, Puppet> puppets;   // peerId -> muñeco visible

    static Node* _find_by_class(Node* n, const char* cls) {
        if (!n) return nullptr;
        if (n->is_class(cls)) return n;
        for (int i = 0; i < n->get_child_count(); i++) {
            Node* r = _find_by_class(n->get_child(i), cls);
            if (r) return r;
        }
        return nullptr;
    }
    bool _valid(Node* n) const { return n && UtilityFunctions::is_instance_valid(n); }

    // El personaje local. También fija is_2d según lo que exista en el mundo.
    Node* _local_player() {
        if (!is_inside_tree()) return nullptr;
        Node* root = (Node*)get_tree()->get_root();
        Node* p = _find_by_class(root, "RobloxPlayer");
        if (p) { is_2d = false; return p; }
        p = _find_by_class(root, "RobloxPlayer2D");
        if (p) { is_2d = true; return p; }
        return nullptr;
    }
    Node* _workspace() {
        if (!is_inside_tree()) return nullptr;
        Node* root = (Node*)get_tree()->get_root();
        Node* w = _find_by_class(root, is_2d ? "RobloxWorkspace2D" : "RobloxWorkspace");
        return w ? w : root;
    }

    void _name_local_player(int idx) {
        Node* p = _local_player();
        if (p) {
            // Solo DisplayName (NO renombrar el nodo: el nombre "LocalPlayer" lo
            // usa el fallback de workspace.CurrentCamera; renombrarlo lo dejaría nil).
            p->set("DisplayName", String("Player") + String::num_int64(idx));
            p->set_meta("PlayerIndex", idx);
        }
    }
    void _apply_device() {
        if (!is_inside_tree()) return;
        Window* w = get_tree()->get_root();
        if (device == "Mobile" && w) w->set_size(Vector2i(400, 820));   // aspecto de teléfono
        // Console/VR: sin efecto visual aún; el juego lee el modo con net:GetDevice().
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
        m->connect("server_disconnected", Callable(this, "_on_server_disconnected"));
        signals_bound = true;
    }
    void _setup_rpcs() {
        Dictionary u;
        u["rpc_mode"]      = MultiplayerAPI::RPC_MODE_ANY_PEER;
        u["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_UNRELIABLE;
        u["call_local"]    = false;
        u["channel"]       = 0;
        Dictionary r = u.duplicate();
        r["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
        rpc_config("_recv_state",    u);
        rpc_config("_relay_state",   u);
        rpc_config("_recv_state2d",  u);
        rpc_config("_relay_state2d", u);
        rpc_config("_recv_remove",   r);
    }

    // Dispara los callbacks Luau (cada uno en su propio hilo)
    static void _fire(std::vector<NCB>& cbs, bool has_id, int id) {
        // Copia defensiva: un callback Luau puede hacer :Connect (add_*_cb ->
        // push_back) DURANTE el dispatch y reasignar el vector, invalidando
        // iteradores/referencias. Iterar sobre una copia elimina esa reentrancia
        // (las conexiones nuevas disparan en el SIGUIENTE evento, como en Roblox).
        std::vector<NCB> snapshot = cbs;
        for (auto& cb : snapshot) {
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

    // ── Muñecos (representación visual de los otros jugadores) ────────────
    Node3D* _make_puppet3d(int idx) {
        AnimatableBody3D* body = memnew(AnimatableBody3D);

        CollisionShape3D* col = memnew(CollisionShape3D);
        Ref<CapsuleShape3D> cs; cs.instantiate();
        cs->set_radius(0.5f);
        cs->set_height(2.0f);
        col->set_shape(cs);
        body->add_child(col);

        // Visual: el mismo modelo del avatar; si falta, cápsula azulada.
        ResourceLoader* rl = ResourceLoader::get_singleton();
        const String glb = "res://assets/avatars/AvatarR15_face.glb";
        Node3D* vis = nullptr;
        if (rl && rl->exists(glb)) {
            Ref<Resource> res = rl->load(glb);
            Ref<PackedScene> sc = Ref<PackedScene>(Object::cast_to<PackedScene>(res.ptr()));
            if (sc.is_valid()) {
                Node* inst = sc->instantiate();
                vis = Object::cast_to<Node3D>(inst);
                if (!vis && inst) inst->queue_free();   // raíz no-Node3D: no fugar el subárbol
            }
        }
        if (vis) {
            float s = 2.0f / 5.187f;
            vis->set_scale(Vector3(s, s, s));
            vis->set_position(Vector3(0, -1.0f + 3.0f * s, 0));
            body->add_child(vis);
        } else {
            MeshInstance3D* m = memnew(MeshInstance3D);
            Ref<CapsuleMesh> cm; cm.instantiate();
            m->set_mesh(cm);
            Ref<StandardMaterial3D> mat; mat.instantiate();
            mat->set_albedo(Color(0.40f, 0.60f, 0.95f));   // otros jugadores en azul
            mat->set_roughness(0.95f);
            m->set_material_override(mat);
            body->add_child(m);
        }

        Label3D* tag = memnew(Label3D);
        tag->set_text(String("Player") + String::num_int64(idx));
        tag->set_position(Vector3(0.0f, 2.4f, 0.0f));
        tag->set_pixel_size(0.012f);
        tag->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
        tag->set_draw_flag(Label3D::FLAG_DISABLE_DEPTH_TEST, true);   // se ve a través de paredes
        body->add_child(tag);
        return body;
    }
    Node2D* _make_puppet2d(int idx) {
        Node2D* body = memnew(Node2D);
        ColorRect* r = memnew(ColorRect);
        r->set_size(Vector2(40.0f, 80.0f));
        r->set_position(Vector2(-20.0f, -80.0f));
        r->set_color(Color(0.40f, 0.60f, 0.95f));
        body->add_child(r);
        Label* tag = memnew(Label);
        tag->set_text(String("Player") + String::num_int64(idx));
        tag->set_position(Vector2(-20.0f, -104.0f));
        body->add_child(tag);
        return body;
    }

    void _update_puppet3d(int key, int idx, Vector3 pos, float yaw) {
        // El estado 3D que llega DEFINE la dimensión del mundo: no dependemos del
        // is_2d cacheado (que puede no estar puesto si el jugador local aún no nació).
        is_2d = false;
        auto it = puppets.find(key);
        if (it == puppets.end() || !_valid(it->second.node)) {
            Node* wsp = _workspace();
            if (!wsp) return;
            Node3D* pup = _make_puppet3d(idx);
            pup->set_name(String("RemotePlayer_") + String::num_int64(key));
            wsp->add_child(pup);
            pup->set_global_position(pos);
            // Si el muñeco sale del árbol por fuera (Luau Destroy/ClearAllChildren),
            // quitar la entrada ANTES de que el puntero quede colgando.
            pup->connect("tree_exiting", Callable(this, "_on_puppet_gone").bind(key));
            Puppet P; P.node = pup; P.tpos = pos; P.tyaw = yaw; P.has_target = true;
            puppets[key] = P;
        } else {
            it->second.tpos = pos; it->second.tyaw = yaw; it->second.has_target = true;
        }
    }
    void _update_puppet2d(int key, int idx, Vector2 pos, float rot) {
        is_2d = true;
        auto it = puppets.find(key);
        if (it == puppets.end() || !_valid(it->second.node)) {
            Node* wsp = _workspace();
            if (!wsp) return;
            Node2D* pup = _make_puppet2d(idx);
            pup->set_name(String("RemotePlayer_") + String::num_int64(key));
            wsp->add_child(pup);
            pup->set_global_position(pos);
            pup->connect("tree_exiting", Callable(this, "_on_puppet_gone").bind(key));
            Puppet P; P.node = pup; P.tpos2d = pos; P.tyaw = rot; P.has_target = true;
            puppets[key] = P;
        } else {
            it->second.tpos2d = pos; it->second.tyaw = rot; it->second.has_target = true;
        }
    }
    // El muñeco salió del árbol: quitar su entrada del mapa mientras el puntero
    // todavía es válido (evita is_instance_valid sobre un Node* colgante -> UAF).
    void _on_puppet_gone(int key) {
        auto it = puppets.find(key);
        if (it != puppets.end()) puppets.erase(it);
    }
    void _remove_puppet(int key) {
        auto it = puppets.find(key);
        if (it != puppets.end()) {
            if (_valid(it->second.node)) it->second.node->queue_free();
            puppets.erase(it);
        }
    }
    void _clear_puppets() {
        for (auto& kv : puppets) if (_valid(kv.second.node)) kv.second.node->queue_free();
        puppets.clear();
    }
    void _lerp_puppets(double dt) {
        float a = 1.0f - (float)Math::pow(0.01, dt);
        for (auto& kv : puppets) {
            Puppet& pu = kv.second;
            if (!_valid(pu.node) || !pu.has_target) continue;
            if (is_2d) {
                Node2D* n = Object::cast_to<Node2D>(pu.node);
                if (!n) continue;
                n->set_global_position(n->get_global_position().lerp(pu.tpos2d, a));
                n->set_rotation(pu.tyaw);
            } else {
                Node3D* n = Object::cast_to<Node3D>(pu.node);
                if (!n) continue;
                n->set_global_position(n->get_global_position().lerp(pu.tpos, a));
                Vector3 r = n->get_rotation(); r.y = pu.tyaw; n->set_rotation(r);
            }
        }
    }
    void _broadcast_local() {
        Node* lp = _local_player();
        if (!lp) return;
        bool server = is_server();
        if (is_2d) {
            Node2D* n = Object::cast_to<Node2D>(lp);
            if (!n) return;
            Vector2 p = n->get_global_position();
            float   r = n->get_global_rotation();
            if (server) rpc("_recv_state2d", 1, p, r, player_index);
            else        rpc_id(1, "_relay_state2d", p, r, player_index);
        } else {
            Node3D* n = Object::cast_to<Node3D>(lp);
            if (!n) return;
            Vector3 p = n->get_global_position();
            float   y = n->get_rotation().y;
            if (server) rpc("_recv_state", 1, p, y, player_index);
            else        rpc_id(1, "_relay_state", p, y, player_index);
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("_on_peer_connected", "id"),    &NetworkService::_on_peer_connected);
        ClassDB::bind_method(D_METHOD("_on_peer_disconnected", "id"), &NetworkService::_on_peer_disconnected);
        ClassDB::bind_method(D_METHOD("_on_connected_ok"),            &NetworkService::_on_connected_ok);
        ClassDB::bind_method(D_METHOD("_on_connect_failed"),          &NetworkService::_on_connect_failed);
        ClassDB::bind_method(D_METHOD("_on_server_disconnected"),     &NetworkService::_on_server_disconnected);
        ClassDB::bind_method(D_METHOD("_auto_init"),                  &NetworkService::_auto_init);
        // RPCs de réplica
        ClassDB::bind_method(D_METHOD("_recv_state", "who", "pos", "yaw", "idx"),   &NetworkService::_recv_state);
        ClassDB::bind_method(D_METHOD("_relay_state", "pos", "yaw", "idx"),         &NetworkService::_relay_state);
        ClassDB::bind_method(D_METHOD("_recv_state2d", "who", "pos", "rot", "idx"), &NetworkService::_recv_state2d);
        ClassDB::bind_method(D_METHOD("_relay_state2d", "pos", "rot", "idx"),       &NetworkService::_relay_state2d);
        ClassDB::bind_method(D_METHOD("_recv_remove", "who"),                       &NetworkService::_recv_remove);
        ClassDB::bind_method(D_METHOD("_on_puppet_gone", "key"),                    &NetworkService::_on_puppet_gone);
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_EXIT_TREE) _clear_puppets();
    }

public:
    // ── Callbacks de señales de red ──────────────────────────────────────
    void _on_peer_connected(int id)    { _fire(pc_cbs, true, id); }
    void _on_peer_disconnected(int id) {
        _remove_puppet(id);
        if (is_server()) rpc("_recv_remove", id);   // que los clientes también lo quiten
        _fire(pd_cbs, true, id);
    }
    void _on_connected_ok()            { net_connected = true; _fire(conn_cbs, false, 0); }
    void _on_connect_failed()          { mode = 0; if (retry_left > 0) return; _fire(fail_cbs, false, 0); }
    void _on_server_disconnected() {
        net_connected = false;
        _clear_puppets();
        // Cliente local: si el host (editor) se detiene, cerrar esta ventana para
        // que no queden ventanas huérfanas conectándose a la nada.
        if (player_index >= 2 && is_inside_tree()) get_tree()->quit();
    }

    // ── RPCs de réplica ──────────────────────────────────────────────────
    // Cliente -> servidor: "aquí estoy". El servidor lo muestra y lo reenvía.
    void _relay_state(Vector3 pos, float yaw, int idx) {
        int sid = get_multiplayer()->get_remote_sender_id();
        _update_puppet3d(sid, idx, pos, yaw);
        rpc("_recv_state", sid, pos, yaw, idx);
    }
    void _relay_state2d(Vector2 pos, float rot, int idx) {
        int sid = get_multiplayer()->get_remote_sender_id();
        _update_puppet2d(sid, idx, pos, rot);
        rpc("_recv_state2d", sid, pos, rot, idx);
    }
    // Servidor -> clientes: estado autoritativo de "who".
    void _recv_state(int who, Vector3 pos, float yaw, int idx) {
        Ref<MultiplayerAPI> m = _mp();
        int myid = (m.is_valid() && m->has_multiplayer_peer()) ? m->get_unique_id() : 0;
        if (who == myid) return;              // ese soy yo
        _update_puppet3d(who, idx, pos, yaw);
    }
    void _recv_state2d(int who, Vector2 pos, float rot, int idx) {
        Ref<MultiplayerAPI> m = _mp();
        int myid = (m.is_valid() && m->has_multiplayer_peer()) ? m->get_unique_id() : 0;
        if (who == myid) return;
        _update_puppet2d(who, idx, pos, rot);
    }
    void _recv_remove(int who) { _remove_puppet(who); }

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
        _setup_rpcs();
        call_deferred("_auto_init");
    }

    void _process(double dt) override {
        // 1. Reintentos de conexión del cliente (hasta que el host abra el puerto)
        if (retry_left > 0) {
            if (net_connected) {
                retry_left = 0;
            } else {
                retry_timer += dt;
                if (retry_timer >= 1.5) {
                    retry_timer = 0.0;
                    retry_left--;
                    if (retry_left > 0) connect_server("127.0.0.1", 25575);
                    else if (!net_connected && player_index >= 2 && is_inside_tree())
                        get_tree()->quit();   // cliente huérfano: el host nunca abrió el puerto
                }
            }
        }
        // 2. Réplica: difundir mi estado y suavizar a los muñecos
        Ref<MultiplayerAPI> m = _mp();
        if (m.is_valid() && m->has_multiplayer_peer()) {
            if (m->get_peers().size() > 0) {
                bcast_timer += dt;
                if (bcast_timer >= 0.05) { bcast_timer = 0.0; _broadcast_local(); }
            }
            _lerp_puppets(dt);
        }
    }

    // Lee --glindex/--glcount/--gldevice (clientes) o la sesión del editor (host)
    // y auto-conecta el multijugador local.
    void _auto_init() {
        PackedStringArray args = OS::get_singleton()->get_cmdline_user_args();
        int idx = 0, cnt = 1;
        bool have_idx = false;
        for (int i = 0; i < args.size(); i++) {
            String a = args[i];
            if      (a == "--glindex"  && i + 1 < args.size()) { idx = String(args[i + 1]).to_int(); have_idx = true; }
            else if (a == "--glcount"  && i + 1 < args.size()) cnt = String(args[i + 1]).to_int();
            else if (a == "--gldevice" && i + 1 < args.size()) device = args[i + 1];
        }
        if (!have_idx) {
            // ¿Instancia anfitriona nativa? (lanzada por el botón Play de Godot)
            int scnt; String sdev;
            if (gl_mp_session_read(scnt, sdev)) {
                idx = 1; cnt = scnt; device = sdev;
                gl_mp_session_consume();   // que no afecte a futuras corridas de un jugador
            }
        }
        player_index = idx;
        if (idx >= 1 && cnt >= 2) {
            if (idx == 1) start_server(25575, cnt);
            else { connect_server("127.0.0.1", 25575); retry_left = 12; }
            set_process(true);   // difundir estado / suavizar muñecos
        }
        if (idx >= 1) _name_local_player(idx);
        _apply_device();
    }
    int    get_player_index() const { return player_index; }
    String get_device() const { return device; }

    // ── API para Luau ────────────────────────────────────────────────────
    bool start_server(int port, int max_players) {
        _bind_mp_signals();
        peer.instantiate();
        Error e = peer->create_server(port, max_players > 0 ? max_players : 32);
        if (e != OK) { UtilityFunctions::push_warning(String("[NetworkService] No se pudo abrir el servidor en el puerto ") + String::num_int64(port)); return false; }
        Ref<MultiplayerAPI> m = _mp();
        if (m.is_valid()) m->set_multiplayer_peer(peer);
        mode = 1;
        set_process(true);
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
        set_process(true);
        return true;
    }
    void disconnect_net() {
        _clear_puppets();
        Ref<MultiplayerAPI> m = _mp();
        if (m.is_valid()) m->set_multiplayer_peer(Ref<MultiplayerPeer>());
        peer = Ref<ENetMultiplayerPeer>();
        mode = 0;
        net_connected = false;
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
