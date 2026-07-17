#ifndef ROBLOX_NETWORK_H
#define ROBLOX_NETWORK_H

// ════════════════════════════════════════════════════════════════════
//  NetworkService — multijugador local estilo Roblox Studio.
//
//  Fase 0: conectividad real (ENet/UDP) con la red nativa de Godot.
//    net:StartServer(port, maxPlayers)   → tu PC como servidor (host)
//    net:Connect(ip, port)               → unirse a un servidor (acepta IP o hostname)
//    net:Disconnect()                    → volver a un jugador
//    net:IsConnected() / net:GetConnectionState() / net:GetServerAddress()
//
//  Direct Connect por IP (juegos exportados, sin escribir Lua) vía args:
//    --glconnect <ip>:<port>   → arrancar uniéndose a ese servidor
//    --glserver  <port>        → arrancar hospedando en ese puerto
//    --glhost    <ip>          → host al que apuntan las ventanas cliente del
//                                test local (por defecto 127.0.0.1; o res://.gl_host)
//
//  Fase 1 (ESTE archivo): RÉPLICA de jugadores. Cada instancia difunde la
//  posición de su propio personaje y ve a los demás como "muñecos" (puppets)
//  que se mueven en tiempo real y tienen colisión (te chocas con ellos).
//  Topología estrella con relay por el servidor: todos ven a todos.
//
//  Arranque automático: cuando el usuario pulsa el botón Play nativo con
//  "Players" >= 2 en la barra del editor, se lanzan N instancias:
//    · la instancia nativa recibe "--glindex 1" vía ProjectSettings
//      "editor/run/main_run_args" (el editor los añade DESPUÉS de la escena,
//      así llegan como user args) y se vuelve el HOST,
//    · las demás se lanzan con --glindex 2..N y se conectan como clientes.
//  Las ventanas se acomodan en mosaico con título Player1..N para que se vean
//  todas (antes se apilaban en la misma posición y parecían menos ventanas).
//
//  Vista de Servidor: en sesiones multijugador cada ventana tiene un botón
//  "Server View" (como Roblox Studio) que suelta una cámara libre: WASD +
//  click derecho vuelan la cámara SIN mover al personaje.
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
#include <godot_cpp/classes/ip.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/sub_viewport_container.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/engine.hpp>
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

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/project_settings.hpp>

#include "lua.h"
#include "lualib.h"
#include "gl_errors.h"
#include "gl_runtime.h"   // gl_state_alive, gl_freecam
#include "roblox_services.h"   // RobloxValue (replicar subtipo IntValue/StringValue)
#include "gl_debug.h"     // gl_debug_on (prints internos solo en Modo Debug)
#include "gl_avatar.h"    // personaje R6 + animador (muñecos remotos)

using namespace godot;

// Prints internos del multijugador: SOLO con el Modo Debug activo (como el
// resto de la extension). Sin tildes: la consola de Windows los rompe.
static inline void gl_mp_log(const String& s) {
    if (gl_debug_on()) UtilityFunctions::print(String("[GodotLuau MP] ") + s);
}

// ── Sesion de prueba por res:// ────────────────────────────────────────────
// El plugin del editor escribe res://.gl_mp_session = "count|device|stamp".
// res:// es EXACTAMENTE la misma carpeta para el editor y para el juego
// lanzado desde el editor (corre sin pack), asi que este canal no puede
// fallar por rutas. count>=2 => la ventana NATIVA es el SERVIDOR; count==1
// el archivo solo transporta el dispositivo emulado.
static inline bool gl_mp_session_read_ex(int& out_count, String& out_device, double& out_age) {
    Ref<FileAccess> f = FileAccess::open("res://.gl_mp_session", FileAccess::READ);
    if (f.is_null()) return false;
    PackedStringArray parts = f->get_as_text().strip_edges().split("|");
    f->close();
    if (parts.size() < 3) return false;
    int    cnt   = String(parts[0]).to_int();
    double stamp = String(parts[2]).to_float();
    double now   = Time::get_singleton()->get_unix_time_from_system();
    if (cnt < 1) return false;
    out_age = now - stamp;
    if (out_age > 21600.0) return false;   // 6 h: sesion de otro dia no cuenta
    out_count  = cnt;
    out_device = parts[1];
    return true;
}
static inline bool gl_mp_session_read(int& out_count, String& out_device) {
    double age;
    return gl_mp_session_read_ex(out_count, out_device, age);
}

// Host al que se conectan las ventanas cliente del test local. Por defecto
// 127.0.0.1 (misma máquina); se puede apuntar a OTRA PC con el arg
// "--glhost <ip>" o el archivo res://.gl_host, para probar entre máquinas.
static inline String gl_mp_host() {
    PackedStringArray a = OS::get_singleton()->get_cmdline_user_args();
    for (int i = 0; i < a.size(); i++)
        if (String(a[i]) == "--glhost" && i + 1 < a.size()) return a[i + 1];
    Ref<FileAccess> f = FileAccess::open("res://.gl_host", FileAccess::READ);
    if (f.is_valid()) {
        String h = f->get_as_text().strip_edges();
        f->close();
        if (!h.is_empty()) return h;
    }
    return "127.0.0.1";
}

// ¿Debe auto-crearse el NetworkService al arrancar el juego?
// Siempre que el juego corra desde el editor (toggle de vista, dispositivo,
// sesiones) o que lleve --glindex (ventanas cliente).
static inline bool gl_mp_autostart_requested() {
    PackedStringArray a = OS::get_singleton()->get_cmdline_user_args();
    for (int i = 0; i < a.size(); i++) {
        const String arg = a[i];
        // Juegos EXPORTADOS que se unen/hospedan por IP o por coordinador.
        if (arg == "--glindex" || arg == "--glconnect" || arg == "--glserver" ||
            arg == "--glhost"  || arg == "--glmatch"   || arg == "--glinstance") return true;
    }
    int c; String d;
    if (gl_mp_session_read(c, d)) return true;
    return OS::get_singleton()->has_feature("editor");
}

// ── Cámara libre de la Vista de Servidor (como la cámara de Roblox Studio) ──
//  WASD mueve, click derecho + mouse rota, rueda cambia la velocidad,
//  E/Espacio sube, Q/Ctrl baja. NO toca al personaje.
class GLFreeCamera : public Camera3D {
    GDCLASS(GLFreeCamera, Camera3D);
    float speed = 16.0f;
    float sens  = 0.0025f;
protected:
    static void _bind_methods() {}
public:
    void _input(const Ref<InputEvent>& e) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        Input* in = Input::get_singleton();
        Ref<InputEventMouseButton> mb = e;
        if (mb.is_valid()) {
            if (mb->get_button_index() == MOUSE_BUTTON_RIGHT)
                in->set_mouse_mode(mb->is_pressed() ? Input::MOUSE_MODE_CAPTURED
                                                    : Input::MOUSE_MODE_VISIBLE);
            if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_UP)   speed = Math::min(speed * 1.15f, 200.0f);
            if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN) speed = Math::max(speed / 1.15f, 1.0f);
        }
        Ref<InputEventMouseMotion> mm = e;
        if (mm.is_valid() && in->get_mouse_mode() == Input::MOUSE_MODE_CAPTURED) {
            Vector3 r = get_rotation();
            r.y -= mm->get_relative().x * sens;
            r.x  = Math::clamp(r.x - mm->get_relative().y * sens, -1.5f, 1.5f);
            set_rotation(r);
        }
    }
    void _process(double dt) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        Input* in = Input::get_singleton();
        Basis b = get_global_transform().basis;
        Vector3 dir;
        if (in->is_key_pressed(KEY_W)) dir -= b.get_column(2);
        if (in->is_key_pressed(KEY_S)) dir += b.get_column(2);
        if (in->is_key_pressed(KEY_D)) dir += b.get_column(0);
        if (in->is_key_pressed(KEY_A)) dir -= b.get_column(0);
        if (in->is_key_pressed(KEY_E) || in->is_key_pressed(KEY_SPACE)) dir += Vector3(0, 1, 0);
        if (in->is_key_pressed(KEY_Q) || in->is_key_pressed(KEY_CTRL))  dir -= Vector3(0, 1, 0);
        if (dir.length_squared() > 0.0001f)
            set_global_position(get_global_position() + dir.normalized() * speed * (float)dt);
    }
    GLFreeCamera() {}
};

// ── Vista previa de VR: la pantalla se divide en dos "ojos" ─────────────────
//  Dos SubViewports que comparten el mundo 3D del juego, cada uno con una
//  camara desplazada ~3.2 cm (distancia interpupilar) que sigue a la camara
//  activa del juego. Es una PREVIEW visual (sin lentes reales).
class GLVRPreview : public Control {
    GDCLASS(GLVRPreview, Control);
    SubViewport* eye_l = nullptr;
    SubViewport* eye_r = nullptr;
    Camera3D*    cam_l = nullptr;
    Camera3D*    cam_r = nullptr;
protected:
    static void _bind_methods() {}
public:
    HBoxContainer* hb = nullptr;
    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        hb = memnew(HBoxContainer);
        hb->add_theme_constant_override("separation", 4);
        add_child(hb);
        for (int i = 0; i < 2; i++) {
            SubViewportContainer* svc = memnew(SubViewportContainer);
            svc->set_stretch(true);
            svc->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            svc->set_v_size_flags(Control::SIZE_EXPAND_FILL);
            hb->add_child(svc);
            SubViewport* sv = memnew(SubViewport);
            // Tamano inicial REAL: sin esto el primer render sale con un buffer
            // diminuto y el glow del look Roblox spamea errores de mipmaps.
            sv->set_size(Vector2i(600, 560));
            sv->set_update_mode(SubViewport::UPDATE_ALWAYS);
            svc->add_child(sv);
            Camera3D* c = memnew(Camera3D);
            sv->add_child(c);
            c->set_current(true);
            if (i == 0) { eye_l = sv; cam_l = c; } else { eye_r = sv; cam_r = c; }
        }
        // Compartir el mundo 3D del juego (mismas luces, mismo escenario)
        Ref<World3D> w = get_viewport()->find_world_3d();
        if (w.is_valid()) { eye_l->set_world_3d(w); eye_r->set_world_3d(w); }
    }
    void _process(double) override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        Viewport* root = get_viewport();
        if (!root) return;
        // Tamano forzado por codigo (los anchors dentro de un CanvasLayer pueden
        // quedarse en 0 el primer frame y el render de los ojos explota)
        Vector2 vs = root->get_visible_rect().size;
        if (vs.x >= 2.0f && !get_size().is_equal_approx(vs)) {
            set_position(Vector2());
            set_size(vs);
            if (hb) { hb->set_position(Vector2()); hb->set_size(vs); }
        }
        Camera3D* main_cam = root->get_camera_3d();
        if (!main_cam || !cam_l || !cam_r) return;
        Transform3D t = main_cam->get_global_transform();
        Vector3 right = t.basis.get_column(0).normalized() * 0.032f;   // ~IPD/2
        cam_l->set_global_transform(Transform3D(t.basis, t.origin - right));
        cam_r->set_global_transform(Transform3D(t.basis, t.origin + right));
        cam_l->set_fov(main_cam->get_fov());
        cam_r->set_fov(main_cam->get_fov());
    }
    GLVRPreview() {}
};

#include "roblox_remote.h"   // RemoteEventNode (entrega de eventos que llegan por red)
#include "roblox_part.h"     // RobloxPart (física en red / Network Ownership 1.14.7)

class NetworkService : public Node {
    GDCLASS(NetworkService, Node);

    struct NCB { lua_State* L; int ref; bool active = true; };
    struct Puppet {
        Node*   node = nullptr;
        Vector3 tpos;                 // objetivo 3D
        Vector2 tpos2d;               // objetivo 2D
        float   tyaw = 0.0f;
        bool    has_target = false;
        // Animacion de caminar (bob del visual, como el test-anim del local)
        float   walk_phase = 0.0f;
        float   vis_base_y = 0.0f;
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
    String target_address = "";   // "ip:puerto" del servidor al que se conectó (para GetServerAddress)
    bool   is_dedicated = false;   // servidor dedicado (sin jugador local, estilo minecraft_server.jar)
    double retry_timer = 0.0;

    // ── Coordinador / matchmaker (mundos/instancias estilo Roblox) ──────
    bool   is_coordinator = false;   // esta instancia REPARTE jugadores a mundos
    bool   is_instance    = false;   // esta instancia ES un mundo lanzado por el coordinador
    int    coord_port     = 0;       // puerto del coordinador
    int    coord_max      = 8;       // máximo de jugadores por mundo (como Roblox)
    String host_secret    = "";      // llave del dueño (solo él puede hostear)
    double coord_tick     = 0.0;     // temporizador de mantenimiento del coordinador
    double inst_hb_timer  = 0.0;     // temporizador de latido de la instancia
    String match_ip       = "";      // cliente: IP del coordinador para auto-join
    int    match_port     = 0;
    String reconnect_ip   = "";      // cliente: reconectar a la instancia asignada
    int    reconnect_port = 0;
    double reconnect_timer = 0.0;
    int    reconnect_try   = 0;

    // Réplica de jugadores
    bool   is_2d = false;
    double bcast_timer = 0.0;
    std::map<int, Puppet> puppets;   // peerId -> muñeco visible
    std::map<int, uint64_t> peer_player_ids;   // peerId remoto -> ObjectID del objeto Player

    // ── UserId secuencial e inmutable (asignado por el servidor) ─────────
    // Cada juego numera a sus jugadores por orden de entrada: host=1, luego
    // 2,3,... El servidor es autoridad; replica cada asignación a todos con
    // _gl_peer_uid para que todas las ventanas vean el mismo UserId por peer.
    std::map<int, int64_t> peer_seq_uid;   // peerId -> UserId asignado
    int64_t next_seq_uid = 2;              // servidor: siguiente UserId (host toma el 1)

    // ── Replicación automática (1.14.5) ─────────────────────────────────
    // netId (asignado por el servidor) -> ObjectID del nodo replicado. En el
    // servidor se llena al replicar; en el cliente al recibir _rep_new.
    std::map<uint64_t, uint64_t> net_instances;
    uint64_t next_net_id_ctr = 1;   // servidor: contador de netId

    // ── Física en red / Network Ownership (1.14.7) ───────────────────────
    // Cada parte no anclada tiene un dueño (meta "_gl_owner": 0=servidor,
    // N=peer). La máquina AUTORIDAD simula y transmite su transform+velocidad;
    // las demás la congelan (kinematic) e interpolan al estado recibido.
    double phys_timer = 0.0;
    double phys_auto_timer = 0.0;   // reasignación de dueño Auto (jugador más cercano)
    struct PhysTarget { Transform3D xform; Vector3 lvel; Vector3 avel; bool has = false; };
    std::map<uint64_t, PhysTarget> phys_targets;   // netId → estado remoto a interpolar

    // Ping (medido cliente→servidor→cliente, en milisegundos)
    double ping_ms = -1.0;
    double ping_timer = 0.0;
    // Reloj sincronizado (1.14.8): el cliente sincroniza su offset con el servidor.
    double time_sync_timer = 0.0;
    bool   time_synced = false;

    // Vista de Servidor (cámara libre) — comandada desde el panel Game del
    // editor vía res://.gl_view_cmd ("server|N" / "client|N")
    uint64_t     freecam_id = 0;    // ObjectID de la cámara libre
    uint64_t     prevcam_id = 0;    // ObjectID de la cámara del jugador
    int          view_last_n = -1;  // último comando aplicado
    double       view_poll = 0.0;
    bool         view_watch = false;
    bool         boot_server_view = false;   // la ventana del servidor arranca en cámara libre
    double       xform_poll = 0.0;           // vigilancia de sesión para transformarse en servidor

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

    // ── Objetos Player (Player ≠ Character) ──────────────────────────────
    Players* _players_service() {
        if (!is_inside_tree()) return nullptr;
        return Object::cast_to<Players>(_find_by_class((Node*)get_tree()->get_root(), "Players"));
    }
    PlayerObject* _local_player_obj() {
        Players* ps = _players_service();
        return ps ? ps->ensure_local_player_obj() : nullptr;
    }
    PlayerObject* _peer_player(int peer) {
        auto it = peer_player_ids.find(peer);
        if (it == peer_player_ids.end()) return nullptr;
        return Object::cast_to<PlayerObject>(ObjectDB::get_instance(it->second));
    }
    // Crea (si falta) el objeto Player de un peer remoto, hijo del servicio Players.
    PlayerObject* _ensure_peer_player(int peer) {
        PlayerObject* po = _peer_player(peer);
        if (po) return po;
        Players* ps = _players_service();
        if (!ps) return nullptr;
        po = memnew(PlayerObject);
        po->peer_id = peer;
        // UserId = el secuencial asignado por el servidor (host=1, 2,3,...). Si aún
        // no llegó la asignación (carrera), cae al peerId como provisional.
        auto uit = peer_seq_uid.find(peer);
        po->user_id = (uit != peer_seq_uid.end()) ? uit->second : (int64_t)peer;
        String pname = String("Player_") + String::num_int64(peer);
        po->set_name(pname);
        po->display_name = pname;
        ps->add_child(po);
        peer_player_ids[peer] = (uint64_t)po->get_instance_id();
        return po;
    }

    // (cliente) el servidor (peer 1) comunica el UserId secuencial de un peer.
    // Mantiene todas las ventanas con el mismo UserId por jugador y fija el del
    // jugador LOCAL cuando el peer soy yo.
    void _gl_peer_uid(int peer, int uid) {
        if (get_multiplayer()->get_remote_sender_id() != 1) return;   // solo el servidor asigna
        peer_seq_uid[peer] = (int64_t)uid;
        Ref<MultiplayerAPI> m = _mp();
        int myid = (m.is_valid() && m->has_multiplayer_peer()) ? m->get_unique_id() : 0;
        if (peer == myid) {
            gl_local_user_id() = (int64_t)uid;
            if (PlayerObject* lp = _local_player_obj()) lp->user_id = (int64_t)uid;
        }
        if (PlayerObject* po = _peer_player(peer)) po->user_id = (int64_t)uid;   // corregir si ya existía
    }

    void _name_local_player(int display_num) {
        String pname = String("Player") + String::num_int64(display_num);
        Node* p = _local_player();
        if (p) {
            // Solo DisplayName (NO renombrar el nodo: el nombre "LocalPlayer" lo
            // usa el fallback de workspace.CurrentCamera; renombrarlo lo dejaría nil).
            p->set("DisplayName", pname);
            p->set_meta("PlayerIndex", display_num);
        }
        // El chat firma con el nombre del jugador (como en Roblox)
        Node* tcs = is_inside_tree() ? _find_by_class((Node*)get_tree()->get_root(), "TextChatService") : nullptr;
        if (tcs) tcs->call("set_player_name", pname);
    }
    // Spawns separados: que los jugadores no nazcan uno dentro de otro
    void _offset_spawn(int idx) {
        Node* lp = _local_player();
        if (!lp) return;
        if (Node3D* n3 = Object::cast_to<Node3D>(lp))
            n3->set_global_position(n3->get_global_position() + Vector3((float)(idx - 2) * 2.5f, 0.0f, 0.0f));
        else if (Node2D* n2 = Object::cast_to<Node2D>(lp))
            n2->set_global_position(n2->get_global_position() + Vector2((float)(idx - 2) * 90.0f, 0.0f));
    }
    // La ventana del SERVIDOR no tiene personaje (como en Roblox Studio):
    // el server observa con la camara libre; los jugadores son los clientes.
    void _remove_local_player() {
        Node* p = _local_player();
        if (p) { gl_mp_log("ventana de servidor: sin personaje (solo observa)"); p->queue_free(); }
    }
    void _apply_device() {
        if (!is_inside_tree()) return;
        Window* w = get_tree()->get_root();
        if (!w) return;
        if (device == "Mobile") {
            // Telefono: ventana vertical (el joystick tactil lo pone RobloxPlayer)
            w->set_size(Vector2i(400, 820));
        } else if (device == "Console") {
            // Consola: pantalla 16:9 tipo TV; se juega con gamepad (ya soportado)
            w->set_size(Vector2i(1280, 720));
        } else if (device == "VR") {
            // VR: vista previa con dos "ojos" lado a lado
            w->set_size(Vector2i(1220, 640));
            CanvasLayer* cl = memnew(CanvasLayer);
            cl->set_name("GL_VRPreview");
            cl->set_layer(90);
            add_child(cl);
            GLVRPreview* vr = memnew(GLVRPreview);
            cl->add_child(vr);
        }
    }
    // Título + mosaico. Server (idx 1) = "Server View" (suele estar embebida en
    // el editor); clientes (idx 2..N+1) = "Player1..N" repartidos en pantalla.
    // Antes todas las ventanas abrían apiladas y parecían menos.
    void _apply_window_layout(int idx, int cnt) {
        if (!is_inside_tree()) return;
        Window* w = get_tree()->get_root();
        if (!w) return;
        if (idx == 1) { w->set_title("Server View - GodotLuau"); return; }
        w->set_title(String("Player") + String::num_int64(idx - 1));
        DisplayServer* ds = DisplayServer::get_singleton();
        if (!ds) return;
        Rect2i ur = ds->screen_get_usable_rect();
        if (ur.size.x < 320 || ur.size.y < 240) return;   // headless / pantalla rara
        int cols = (cnt <= 2) ? cnt : ((cnt <= 4) ? 2 : 3);
        int rows = (cnt + cols - 1) / cols;
        Vector2i cell(ur.size.x / cols, ur.size.y / rows);
        int i0 = Math::clamp(idx - 2, 0, cnt - 1);   // celda del cliente
        Vector2i pos = ur.position + Vector2i((i0 % cols) * cell.x, (i0 / cols) * cell.y);
        if (device == "Mobile" || device == "Console" || device == "VR") {
            // Estos modos fijan su propio tamano de ventana; solo se posicionan
            w->set_position(pos + Vector2i(24, 40));
        } else {
            w->set_size(Vector2i(Math::max(cell.x - 24, 480), Math::max(cell.y - 56, 320)));
            w->set_position(pos + Vector2i(12, 40));
        }
    }
    // Vigila res://.gl_view_cmd: el botón "Server View" del panel Game del
    // editor escribe ahí "server|N" o "client|N"; la instancia host (o la única
    // en un jugador) aplica el cambio de perspectiva.
    void _poll_view_cmd(double dt) {
        view_poll += dt;
        if (view_poll < 0.4) return;
        view_poll = 0.0;
        Ref<FileAccess> f = FileAccess::open("res://.gl_view_cmd", FileAccess::READ);
        if (f.is_null()) return;
        PackedStringArray parts = f->get_as_text().strip_edges().split("|");
        f->close();
        if (parts.size() < 2) return;
        int n = String(parts[1]).to_int();
        if (n == view_last_n) return;
        if (player_index > 1) { view_last_n = n; return; }   // clientes: ignoran
        bool want_server = (String(parts[0]) == "server");
        // La ventana del SERVIDOR de una prueba multijugador no puede volverse
        // cliente (no tiene personaje) — como en Roblox Studio.
        if (!want_server && player_index == 1 && mode == 1) {
            gl_mp_log("la ventana del servidor siempre queda en Server View");
            view_last_n = n;
            return;
        }
        if (_set_view(want_server)) view_last_n = n;   // si aún no hay cámara, reintenta
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
        rpc_config("_relay_chat",    r);   // chat: confiable (no se pierde ningun mensaje)
        rpc_config("_recv_chat",     r);
        rpc_config("_ping_req",      u);   // medicion de ping (para el menu de ajustes)
        rpc_config("_ping_res",      u);
        // RemoteEvent por red (Fase 2): confiables y no-confiables, ambos sentidos
        rpc_config("_re_deliver_server",  r);
        rpc_config("_re_deliver_client",  r);
        rpc_config("_ure_deliver_server", u);
        rpc_config("_ure_deliver_client", u);
        rpc_config("_gl_assign",          r);   // coordinador → cliente: ve a este mundo
        rpc_config("_rep_new",            r);   // replicación (1.14.5): crear instancia
        rpc_config("_rep_prop",           r);   // replicación: cambio de propiedad
        rpc_config("_rep_reparent",       r);   // replicación: mover de padre
        rpc_config("_rep_destroy",        r);   // replicación: destruir instancia
        rpc_config("_gl_peer_uid",        r);   // UserId secuencial: servidor → clientes
        rpc_config("_rf_invoke_server",   r);   // RemoteFunction (1.14.6): invocar server
        rpc_config("_rf_invoke_client",   r);   // RemoteFunction: invocar cliente
        rpc_config("_rf_return",          r);   // RemoteFunction: respuesta al invocador
        rpc_config("_phys_state",         u);   // física en red (1.14.7): estado (no confiable, último gana)
        rpc_config("_phys_owned",         u);   // física: estado del cliente-dueño al servidor
        rpc_config("_phys_owner",         r);   // física: cambio de dueño (confiable)
        rpc_config("_time_req",           u);   // reloj sincronizado (1.14.8)
        rpc_config("_time_res",           u);
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
                gl_check_resume(th, lua_resume(th, nullptr, nargs));
            } else lua_pop(cb.L, 1);
            lua_pop(cb.L, 1);
        }
    }

    // ── Muñecos (representación visual de los otros jugadores) ────────────
    Node3D* _make_puppet3d(int idx) {
        AnimatableBody3D* body = memnew(AnimatableBody3D);
        // CRITICO: con sync_to_physics (default) la fisica REVIERTE cualquier
        // set_global_position y el muñeco queda clavado donde nacio.
        body->set_sync_to_physics(false);

        CollisionShape3D* col = memnew(CollisionShape3D);
        Ref<CapsuleShape3D> cs; cs.instantiate();
        cs->set_radius(0.5f);
        cs->set_height(2.0f);
        col->set_shape(cs);
        body->add_child(col);

        // Visual: el MISMO personaje que el local (StarterCharacter o avatar R6
        // con animaciones); si falta todo, capsula azulada.
        Node3D* vis = gl_build_character(this);
        if (vis) {
            vis->set_name("Visual");
            body->add_child(vis);
        } else {
            MeshInstance3D* m = memnew(MeshInstance3D);
            m->set_name("Visual");
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
            // .Character del jugador remoto = este cuerpo; subir su HumanoidRootPart
            // a hijo directo para que el world gen del servidor lea su .Position.
            gl_lift_hrp(pup, pup->get_node_or_null("Visual"));
            // Si el muñeco sale del árbol por fuera (Luau Destroy/ClearAllChildren),
            // quitar la entrada ANTES de que el puntero quede colgando.
            pup->connect("tree_exiting", Callable(this, "_on_puppet_gone").bind(key));
            Puppet P; P.node = pup; P.tpos = pos; P.tyaw = yaw; P.has_target = true;
            if (Node3D* v = Object::cast_to<Node3D>(pup->get_node_or_null("Visual")))
                P.vis_base_y = v->get_position().y;
            puppets[key] = P;
            // El muñeco es el .Character del objeto Player de ese peer.
            // Primer personaje → PlayerAdded y LUEGO CharacterAdded (orden Roblox).
            if (PlayerObject* po = _ensure_peer_player(key)) {
                String pn = String("Player") + String::num_int64(idx);
                po->set_name(pn); po->display_name = pn;
                po->set_character_silent(pup);
                if (!po->added_fired) {
                    po->added_fired = true;
                    if (Players* ps = _players_service()) ps->fire_player_added(po);
                }
                po->fire_character_added();
            }
            gl_mp_log(String("Player") + String::num_int64(idx) + " aparecio en tu mundo (pos " + String(pos) + ")");
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
            if (PlayerObject* po = _ensure_peer_player(key)) {
                String pn = String("Player") + String::num_int64(idx);
                po->set_name(pn); po->display_name = pn;
                po->set_character_silent(pup);
                if (!po->added_fired) {
                    po->added_fired = true;
                    if (Players* ps = _players_service()) ps->fire_player_added(po);
                }
                po->fire_character_added();
            }
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
                Vector3 before = n->get_global_position();
                n->set_global_position(before.lerp(pu.tpos, a));
                // Rotacion SUAVIZADA por el camino corto (lerp_angle): antes el
                // muñeco saltaba de golpe al nuevo angulo y se veia raro.
                Vector3 r = n->get_rotation();
                r.y = (float)Math::lerp_angle((double)r.y, (double)pu.tyaw, (double)a);
                n->set_rotation(r);
                // Animacion: el R6Animator del muñeco recibe el estado derivado
                // del movimiento (velocidad horizontal + si esta en el aire).
                float dist = before.distance_to(pu.tpos);
                Node* anim = n->get_node_or_null(NodePath("Visual/R6Animator"));
                if (anim) {
                    Vector3 dp = pu.tpos - before;
                    double hspeed = Vector2(dp.x, dp.z).length() / Math::max(dt, 0.001);
                    bool airborne = Math::abs(dp.y) / Math::max(dt, 0.001) > 2.5;
                    anim->call("set_move_state", Math::clamp(hspeed / 8.0, 0.0, 1.0), airborne);
                } else {
                    // Fallback capsula: bob simple al moverse
                    Node3D* vis = Object::cast_to<Node3D>(n->get_node_or_null("Visual"));
                    if (vis) {
                        if (dist > 0.05f) pu.walk_phase += (float)dt * 12.0f;
                        else              pu.walk_phase *= 0.85f;
                        Vector3 vp = vis->get_position();
                        vp.y = pu.vis_base_y + Math::abs(Math::sin(pu.walk_phase)) * 0.14f;
                        vis->set_position(vp);
                    }
                }
            }
        }
    }
    double diag_timer = 0.0;   // diagnostico TX/RX (1 linea/seg, solo Modo Debug)
    bool _diag_tick() {
        // true ~1 vez por segundo (los broadcasts son 20/s; loguear todos seria spam)
        return diag_timer <= 0.0;
    }
    void _broadcast_local() {
        if (mode == 2 && !net_connected) return;   // cliente aún negociando: nada de RPC
        Node* lp = _local_player();
        if (!lp) return;   // la ventana del servidor no tiene personaje: no difunde
        bool server = is_server();
        int display_num = (player_index >= 2) ? player_index - 1 : player_index;   // glindex 2..N+1 → Player1..N
        if (_diag_tick()) {
            Node3D* n3 = Object::cast_to<Node3D>(lp);
            gl_mp_log(String("TX ") + (server ? "(server broadcast)" : "(a servidor)")
                      + " pos=" + (n3 ? String(n3->get_global_position()) : String("?"))
                      + " num=" + String::num_int64(display_num));
            diag_timer = 1.0;
        }
        if (is_2d) {
            Node2D* n = Object::cast_to<Node2D>(lp);
            if (!n) return;
            Vector2 p = n->get_global_position();
            float   r = n->get_global_rotation();
            if (server) rpc("_recv_state2d", 1, p, r, display_num);
            else        rpc_id(1, "_relay_state2d", p, r, display_num);
        } else {
            Node3D* n = Object::cast_to<Node3D>(lp);
            if (!n) return;
            Vector3 p = n->get_global_position();
            float   y = n->get_global_rotation().y;   // yaw GLOBAL (robusto ante padres rotados)
            if (server) rpc("_recv_state", 1, p, y, display_num);
            else        rpc_id(1, "_relay_state", p, y, display_num);
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
        ClassDB::bind_method(D_METHOD("gl_send_chat", "name", "text"),              &NetworkService::gl_send_chat);
        ClassDB::bind_method(D_METHOD("_relay_chat", "name", "text"),               &NetworkService::_relay_chat);
        ClassDB::bind_method(D_METHOD("_recv_chat", "from", "name", "text"),        &NetworkService::_recv_chat);
        ClassDB::bind_method(D_METHOD("_ping_req", "t"),                            &NetworkService::_ping_req);
        ClassDB::bind_method(D_METHOD("_ping_res", "t"),                            &NetworkService::_ping_res);
        ClassDB::bind_method(D_METHOD("_time_req", "c_unix", "c_dgt"),              &NetworkService::_time_req);
        ClassDB::bind_method(D_METHOD("_time_res", "c_unix", "c_dgt", "s_unix", "s_dgt"), &NetworkService::_time_res);
        ClassDB::bind_method(D_METHOD("get_ping_ms"),                               &NetworkService::get_ping_ms);
        ClassDB::bind_method(D_METHOD("get_player_count"),                          &NetworkService::get_player_count);
        ClassDB::bind_method(D_METHOD("get_player_index"),                          &NetworkService::get_player_index);
        ClassDB::bind_method(D_METHOD("is_connected_net"),                          &NetworkService::is_connected_net);
        ClassDB::bind_method(D_METHOD("get_server_address"),                        &NetworkService::get_server_address);
        ClassDB::bind_method(D_METHOD("get_connection_state"),                      &NetworkService::get_connection_state);
        ClassDB::bind_method(D_METHOD("is_dedicated_server"),                       &NetworkService::is_dedicated_server);
        ClassDB::bind_method(D_METHOD("get_local_ip"),                              &NetworkService::get_local_ip);
        ClassDB::bind_method(D_METHOD("get_server_list"),                           &NetworkService::get_server_list);
        ClassDB::bind_method(D_METHOD("add_server", "name", "ip", "port"),          &NetworkService::add_server);
        ClassDB::bind_method(D_METHOD("remove_server", "name"),                     &NetworkService::remove_server);
        ClassDB::bind_method(D_METHOD("join_server", "name"),                       &NetworkService::join_server);
        ClassDB::bind_method(D_METHOD("_gl_assign", "iport"),                       &NetworkService::_gl_assign);
        ClassDB::bind_method(D_METHOD("start_coordinator", "port", "max"),          &NetworkService::start_coordinator);
        ClassDB::bind_method(D_METHOD("join_matchmaking", "ip", "port"),            &NetworkService::join_matchmaking);
        ClassDB::bind_method(D_METHOD("get_host_key"),                              &NetworkService::get_host_key);
        ClassDB::bind_method(D_METHOD("read_or_make_host_key"),                     &NetworkService::read_or_make_host_key);
        // Fase 2: estado de red + RemoteEvent por red + roster de jugadores
        ClassDB::bind_method(D_METHOD("is_server"),                                 &NetworkService::is_server);
        ClassDB::bind_method(D_METHOD("is_client"),                                 &NetworkService::is_client);
        ClassDB::bind_method(D_METHOD("get_mode"),                                  &NetworkService::get_mode);
        ClassDB::bind_method(D_METHOD("get_peer_id"),                               &NetworkService::get_peer_id);
        ClassDB::bind_method(D_METHOD("net_send_event", "path", "args", "target", "reliable"), &NetworkService::net_send_event);
        ClassDB::bind_method(D_METHOD("player_node_for_peer", "peer"),              &NetworkService::player_node_for_peer);
        ClassDB::bind_method(D_METHOD("peer_for_player_node", "player"),            &NetworkService::peer_for_player_node);
        ClassDB::bind_method(D_METHOD("get_players_roster"),                        &NetworkService::get_players_roster);
        ClassDB::bind_method(D_METHOD("_re_deliver_server", "path", "args"),        &NetworkService::_re_deliver_server);
        ClassDB::bind_method(D_METHOD("_re_deliver_client", "path", "args"),        &NetworkService::_re_deliver_client);
        ClassDB::bind_method(D_METHOD("_ure_deliver_server", "path", "args"),       &NetworkService::_ure_deliver_server);
        ClassDB::bind_method(D_METHOD("_ure_deliver_client", "path", "args"),       &NetworkService::_ure_deliver_client);
        // Replicación automática (1.14.5)
        ClassDB::bind_method(D_METHOD("_rep_new", "id", "gclass", "name", "parent_ref", "props"), &NetworkService::_rep_new);
        ClassDB::bind_method(D_METHOD("_rep_prop", "id", "key", "enc"),             &NetworkService::_rep_prop);
        ClassDB::bind_method(D_METHOD("_rep_reparent", "id", "parent_ref"),         &NetworkService::_rep_reparent);
        ClassDB::bind_method(D_METHOD("_rep_destroy", "id"),                        &NetworkService::_rep_destroy);
        ClassDB::bind_method(D_METHOD("_gl_peer_uid", "peer", "uid"),               &NetworkService::_gl_peer_uid);
        // RemoteFunction por red (1.14.6)
        ClassDB::bind_method(D_METHOD("net_invoke_server", "path", "call_id", "args"),          &NetworkService::net_invoke_server);
        ClassDB::bind_method(D_METHOD("net_invoke_client", "peer", "path", "call_id", "args"),  &NetworkService::net_invoke_client);
        ClassDB::bind_method(D_METHOD("net_rf_respond", "reply_peer", "call_id", "ok", "rets"), &NetworkService::net_rf_respond);
        ClassDB::bind_method(D_METHOD("_rf_invoke_server", "path", "call_id", "args"),          &NetworkService::_rf_invoke_server);
        ClassDB::bind_method(D_METHOD("_rf_invoke_client", "path", "call_id", "args"),          &NetworkService::_rf_invoke_client);
        ClassDB::bind_method(D_METHOD("_rf_return", "call_id", "ok", "rets"),                   &NetworkService::_rf_return);
        // Física en red / Network Ownership (1.14.7)
        ClassDB::bind_method(D_METHOD("net_set_owner", "net_id", "owner_peer", "is_auto"),      &NetworkService::net_set_owner);
        ClassDB::bind_method(D_METHOD("net_is_owner", "net_id"),                                &NetworkService::net_is_owner);
        ClassDB::bind_method(D_METHOD("net_node_by_id", "net_id"),                              &NetworkService::net_node_by_id);
        ClassDB::bind_method(D_METHOD("_phys_state", "net_id", "t", "lv", "av"),                &NetworkService::_phys_state);
        ClassDB::bind_method(D_METHOD("_phys_owned", "net_id", "t", "lv", "av"),                &NetworkService::_phys_owned);
        ClassDB::bind_method(D_METHOD("_phys_owner", "net_id", "owner_peer", "is_auto"),        &NetworkService::_phys_owner);
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_EXIT_TREE) _clear_puppets();
    }

public:
    // ── Callbacks de señales de red ──────────────────────────────────────
    void _on_peer_connected(int id) {
        // COORDINADOR: no es un mundo; reparte al jugador a una instancia con
        // espacio (o arranca una nueva) y lo redirige. No dispara PlayerAdded.
        if (is_coordinator) {
            int iport = _coord_pick_or_spawn();
            rpc_id(id, "_gl_assign", iport);
            gl_mp_log(String("coordinador: peer ") + String::num_int64(id) + " -> mundo :" + String::num_int64(iport));
            return;
        }
        gl_mp_log(String("otro jugador entro (peer ") + String::num_int64(id) + ")");
        if (mode == 1) {
            // UserId secuencial: primero poner al día al peer nuevo con las
            // asignaciones YA hechas (host + otros peers), luego asignarle el suyo
            // y avisar a TODOS para que todas las ventanas coincidan.
            for (auto& kv : peer_seq_uid) rpc_id(id, "_gl_peer_uid", kv.first, (int)kv.second);
            int64_t uid = next_seq_uid++;
            peer_seq_uid[id] = uid;
            rpc("_gl_peer_uid", id, (int)uid);
            // Replicación (1.14.5): mandarle al peer nuevo el mundo ya replicado. Los
            // clientes ya no corren ServerScripts, así que sin esto no verían lo que el
            // servidor creó antes de que ellos conectaran (late-join).
            _rep_sync_peer(id);
        }
        // NO se dispara Players.PlayerAdded aquí: un peer SIN personaje (p.ej. la
        // ventana SERVIDOR sin jugador) NO es un Player. PlayerAdded se dispara
        // cuando aparece su personaje (muñeco), en _update_puppet*. Aquí solo la
        // señal propia net:PlayerConnected(peerId).
        _fire(pc_cbs, true, id);
    }
    // Dispara CharacterRemoving + Players.PlayerRemoving y libera el objeto Player
    // de un peer. Debe correr en TODAS las instancias que lo vieron entrar (server
    // por _on_peer_disconnected, clientes por _recv_remove relayado), si no
    // PlayerRemoving quedaría asimétrico con PlayerAdded y el Player se fugaría.
    void _remove_peer_player(int id) {
        PlayerObject* po = _peer_player(id);
        if (po) {
            po->fire_character_removing();
            if (Players* ps = _players_service()) ps->fire_player_removing(po);
            po->queue_free();
        }
        peer_player_ids.erase(id);
    }
    void _on_peer_disconnected(int id) {
        _remove_puppet(id);
        if (is_server()) rpc("_recv_remove", id);   // que los clientes también lo quiten
        // Física (1.14.7): partes que poseía ese peer vuelven al servidor.
        if (is_server()) {
            for (auto& kv : net_instances) {
                Node* n = _rep_node(kv.first);
                if (n && n->has_meta("_gl_owner") && (int)n->get_meta("_gl_owner") == id) {
                    n->set_meta("_gl_owner", 0);
                    rpc("_phys_owner", (int64_t)kv.first, 0, false);
                }
            }
        }
        _remove_peer_player(id);
        _fire(pd_cbs, true, id);
    }
    void _on_connected_ok()            { net_connected = true; gl_mp_log("conectado al host! sesion compartida activa"); _fire(conn_cbs, false, 0); }
    void _on_connect_failed()          { mode = 0; if (retry_left > 0) return; _fire(fail_cbs, false, 0); }
    void _on_server_disconnected() {
        net_connected = false;
        _clear_puppets();
        // Cliente local: si el host (editor) se detiene, cerrar esta ventana para
        // que no queden ventanas huérfanas conectándose a la nada.
        if (player_index >= 2 && is_inside_tree()) get_tree()->quit();
    }

    // ── RemoteEvent por red (Fase 2) ─────────────────────────────────────
    // Lo llama RemoteEventNode vía call(). target: 0 = al servidor,
    // -1 = a todos los clientes, >0 = a un peer concreto.
    void net_send_event(const String& path, const Array& args, int target, bool reliable) {
        const char* m_srv = reliable ? "_re_deliver_server" : "_ure_deliver_server";
        const char* m_cli = reliable ? "_re_deliver_client" : "_ure_deliver_client";
        // La red puede no estar lista cuando un script dispara un RemoteEvent al
        // arrancar: en Roblox el cliente ya está replicado antes de correr scripts,
        // pero aquí los scripts corren mientras ENet aún negocia la conexión. Sin
        // esta guarda, rpc_id lanzaría "peer not connected". Ignoramos el envío.
        Ref<MultiplayerAPI> m = _mp();
        if (m.is_null() || !m->has_multiplayer_peer()) return;
        if (target == 0) {
            if (mode == 2 && !net_connected) return;   // cliente aún conectándose al host
            rpc_id(1, m_srv, path, args);
        } else if (target == -1) {
            rpc(m_cli, path, args);                     // servidor → todos (0 peers = no-op)
        } else {
            if (!m->get_peers().has(target)) return;    // ese peer ya no está conectado
            rpc_id(target, m_cli, path, args);
        }
    }
    void _deliver_event(const String& path, const Array& args, bool to_server) {
        if (!is_inside_tree()) return;
        // Autoridad (como Roblox): OnServerEvent solo se dispara en el servidor;
        // OnClientEvent solo si el emisor es el servidor (peer 1). Evita que un
        // cliente modificado falsifique eventos por RPC crudo saltándose el sandbox.
        if (to_server) { if (!is_server()) return; }
        else           { if (get_multiplayer()->get_remote_sender_id() != 1) return; }
        RemoteEventNode* rev = Object::cast_to<RemoteEventNode>(get_node_or_null(NodePath(path)));
        if (!rev) return;
        if (to_server) {
            int sid = get_multiplayer()->get_remote_sender_id();
            // Como Roblox: OnServerEvent SIEMPRE recibe un Player válido. El objeto
            // Player de un peer se crea perezosamente al llegar su personaje, pero un
            // cliente puede FireServer antes de eso (p.ej. "ClienteListo" al arrancar).
            // _ensure_peer_player lo crea en el acto para que el emisor nunca sea nil.
            rev->deliver_to_server_cbs(args, _ensure_peer_player(sid), this);
        } else {
            rev->deliver_to_client_cbs(args, this);
        }
    }
    void _re_deliver_server(String path, Array args)  { _deliver_event(path, args, true); }
    void _re_deliver_client(String path, Array args)  { _deliver_event(path, args, false); }
    void _ure_deliver_server(String path, Array args) { _deliver_event(path, args, true); }
    void _ure_deliver_client(String path, Array args) { _deliver_event(path, args, false); }

    // ── RemoteFunction por red (1.14.6) ──────────────────────────────────
    // El invocador (InvokeServer/InvokeClient en luau_api.h) llama estos net_*
    // para mandar la petición; corre el handler remoto con soporte de yield vía
    // el wrapper Lua __gl_rf_serve y devuelve el resultado con _rf_return, que el
    // ScriptNodeBase que cedió recoge de gl_net_responses() en su _process.
    void net_invoke_server(String path, int call_id, Array args) {
        Ref<MultiplayerAPI> m = _mp();
        if (m.is_null() || !m->has_multiplayer_peer()) return;
        if (mode == 2 && !net_connected) return;   // cliente aún conectándose
        rpc_id(1, "_rf_invoke_server", path, call_id, args);
    }
    void net_invoke_client(int peer, String path, int call_id, Array args) {
        Ref<MultiplayerAPI> m = _mp();
        if (m.is_null() || !m->has_multiplayer_peer()) return;
        if (!m->get_peers().has(peer)) return;
        rpc_id(peer, "_rf_invoke_client", path, call_id, args);
    }
    // Enviar la respuesta al invocador (peer 1 = servidor, o un cliente concreto).
    void _rf_send_return(int reply_peer, int call_id, bool ok, const Array& rets) {
        Ref<MultiplayerAPI> m = _mp();
        if (m.is_null() || !m->has_multiplayer_peer()) return;
        if (reply_peer == 1 || m->get_peers().has(reply_peer))
            rpc_id(reply_peer, "_rf_return", call_id, ok, rets);
    }
    // Lo llama __gl_rf_respond (C) cuando el handler termina.
    void net_rf_respond(int reply_peer, int call_id, bool ok, Array rets) {
        _rf_send_return(reply_peer, call_id, ok, rets);
    }
    // Corre el handler (OnServerInvoke/OnClientInvoke) vía __gl_rf_serve, que lo
    // envuelve en task.spawn (soporta yield) y responde al terminar.
    void _rf_run_handler(lua_State* L, int handler_ref, Node* player, int call_id, int reply_peer, const Array& args, bool is_server) {
        if (!gl_state_alive(L) || handler_ref == LUA_NOREF) { _rf_send_return(reply_peer, call_id, true, Array()); return; }
        lua_State* th = lua_newthread(L);
        lua_getglobal(th, "__gl_rf_serve");
        if (!lua_isfunction(th, -1)) { lua_pop(L, 1); _rf_send_return(reply_peer, call_id, true, Array()); return; }
        lua_rawgeti(th, LUA_REGISTRYINDEX, handler_ref);   // handler
        if (player) _gl_push_node(th, player); else lua_pushnil(th);   // player
        lua_pushnumber(th, (double)call_id);               // callId
        lua_pushnumber(th, (double)reply_peer);            // replyPeer
        lua_pushboolean(th, is_server ? 1 : 0);            // isServer
        int n = gl_net_decode_args(th, args, this);        // ...args
        gl_check_resume(th, lua_resume(th, nullptr, 5 + n));
        lua_pop(L, 1);   // quitar el hilo wrapper (el handler corre en su propio hilo de task.spawn)
    }
    void _rf_invoke_server(String path, int call_id, Array args) {
        if (!is_server()) return;
        int sender = get_multiplayer()->get_remote_sender_id();
        RemoteFunctionNode* rfn = Object::cast_to<RemoteFunctionNode>(get_node_or_null(NodePath(path)));
        if (!rfn) { _rf_send_return(sender, call_id, true, Array()); return; }
        Node* player = _ensure_peer_player(sender);   // como OnServerEvent: Player válido
        _rf_run_handler(rfn->server_invoke_L, rfn->server_invoke_ref, player, call_id, sender, args, true);
    }
    void _rf_invoke_client(String path, int call_id, Array args) {
        if (get_multiplayer()->get_remote_sender_id() != 1) return;   // solo el servidor invoca al cliente
        RemoteFunctionNode* rfn = Object::cast_to<RemoteFunctionNode>(get_node_or_null(NodePath(path)));
        if (!rfn) { _rf_send_return(1, call_id, true, Array()); return; }
        _rf_run_handler(rfn->client_invoke_L, rfn->client_invoke_ref, nullptr, call_id, 1, args, false);
    }
    void _rf_return(int call_id, bool ok, Array rets) {
        GLNetResponse& r = gl_net_responses()[(int64_t)call_id];
        r.rets = rets; r.ok = ok; r.ready = true; r.age = 0.0;
    }

    // ── Replicación automática de instancias/propiedades (1.14.5) ─────────
    // El SERVIDOR es autoridad: cuando un script del servidor crea una instancia
    // bajo un contenedor replicable (Workspace/ReplicatedStorage) o cambia una
    // propiedad de una ya replicada, difunde el DELTA a los clientes. Cada
    // instancia lleva un netId estable (meta "_gl_netid"). El mundo INICIAL
    // (puesto en el editor) NO se replica: cada ventana ya cargó el mismo place;
    // esto es una capa de deltas de runtime. Los envíos los dispara luau_api.h
    // (godot_object_newindex/method_destroy) llamando estos métodos por tipo.
    int64_t rep_next_id() { return (int64_t)(next_net_id_ctr++); }
    void rep_map(int64_t id, Object* node) {
        Node* n = Object::cast_to<Node>(node);
        if (n) net_instances[(uint64_t)id] = (uint64_t)n->get_instance_id();
    }
    Node* _rep_node(uint64_t id) {
        auto it = net_instances.find(id);
        if (it == net_instances.end()) return nullptr;
        return Object::cast_to<Node>(ObjectDB::get_instance(it->second));
    }
    Node* _rep_resolve_parent(const String& ref) {
        if (ref.begins_with("#")) return _rep_node((uint64_t)ref.substr(1).to_int());
        if (!is_inside_tree()) return nullptr;
        return get_node_or_null(NodePath(ref));   // ruta absoluta (mismo place en cada ventana)
    }

    // ── Física en red / Network Ownership (1.14.7) ───────────────────────
    // ¿Esta máquina simula esta parte? owner 0 = servidor; owner N = ese peer.
    bool _phys_authority(Node* part) {
        int owner = part->has_meta("_gl_owner") ? (int)part->get_meta("_gl_owner") : 0;
        if (owner == 0) return is_server();
        return owner == get_peer_id();
    }
    bool net_is_owner(int64_t netId) {
        Node* n = _rep_node((uint64_t)netId);
        return n ? _phys_authority(n) : true;
    }
    // Resolver una instancia replicada por su netId (1.14.9: Instances por id en
    // los payloads de RemoteEvent/Function en vez de por ruta).
    Object* net_node_by_id(int64_t netId) { return _rep_node((uint64_t)netId); }
    // Fijar dueño (SetNetworkOwner/Auto, solo servidor): aplicar local + difundir.
    void net_set_owner(int64_t netId, int owner_peer, bool is_auto) {
        _apply_owner((uint64_t)netId, owner_peer, is_auto);
        rpc("_phys_owner", netId, owner_peer, is_auto);
    }
    void _apply_owner(uint64_t netId, int owner_peer, bool is_auto) {
        RobloxPart* part = Object::cast_to<RobloxPart>(_rep_node(netId));
        if (!part) return;
        part->set_meta("_gl_owner", owner_peer);
        part->set_meta("_gl_owner_auto", is_auto);
        if (_phys_authority(part)) {
            part->set_freeze_enabled(part->get_anchored());   // yo simulo (salvo anclada)
            phys_targets.erase(netId);
        } else if (!part->get_anchored()) {
            part->set_freeze_mode(RigidBody3D::FREEZE_MODE_KINEMATIC);
            part->set_freeze_enabled(true);                    // congelar para no pelear con el stream
        }
    }
    void _phys_owner(int64_t netId, int owner_peer, bool is_auto) {
        if (get_multiplayer()->get_remote_sender_id() != 1) return;   // solo el servidor
        _apply_owner((uint64_t)netId, owner_peer, is_auto);
    }
    void _phys_apply_remote(RobloxPart* part, uint64_t netId, const Transform3D& t, const Vector3& lv, const Vector3& av) {
        if (!part->is_freeze_enabled()) { part->set_freeze_mode(RigidBody3D::FREEZE_MODE_KINEMATIC); part->set_freeze_enabled(true); }
        PhysTarget& pt = phys_targets[netId];
        pt.xform = t; pt.lvel = lv; pt.avel = av; pt.has = true;
    }
    void _phys_state(int64_t netId, Transform3D t, Vector3 lv, Vector3 av) {
        if (get_multiplayer()->get_remote_sender_id() != 1) return;   // autoridad-relay = servidor
        RobloxPart* part = Object::cast_to<RobloxPart>(_rep_node((uint64_t)netId));
        if (!part || _phys_authority(part)) return;
        _phys_apply_remote(part, (uint64_t)netId, t, lv, av);
    }
    void _phys_owned(int64_t netId, Transform3D t, Vector3 lv, Vector3 av) {
        if (!is_server()) return;
        int sender = get_multiplayer()->get_remote_sender_id();
        RobloxPart* part = Object::cast_to<RobloxPart>(_rep_node((uint64_t)netId));
        if (!part) return;
        int owner = part->has_meta("_gl_owner") ? (int)part->get_meta("_gl_owner") : 0;
        if (owner != sender) return;   // solo el dueño real puede mandar su estado
        _phys_apply_remote(part, (uint64_t)netId, t, lv, av);   // el server tambien lo muestra
        Ref<MultiplayerAPI> m = _mp();
        if (m.is_valid()) {
            PackedInt32Array peers = m->get_peers();
            for (int i = 0; i < peers.size(); i++) if (peers[i] != sender) rpc_id(peers[i], "_phys_state", netId, t, lv, av);
        }
    }
    // Tick ~20Hz: la autoridad transmite sus partes no ancladas despiertas; los
    // demás las congelan (kinematic) para no simular contra el stream.
    void _phys_tick() {
        for (auto& kv : net_instances) {
            RobloxPart* part = Object::cast_to<RobloxPart>(_rep_node(kv.first));
            if (!part || part->get_anchored()) continue;
            if (_phys_authority(part)) {
                if (part->is_freeze_enabled()) part->set_freeze_enabled(false);
                phys_targets.erase(kv.first);
                if (part->is_sleeping()) continue;
                Transform3D t = part->get_global_transform();
                Vector3 lv = part->get_linear_velocity(), av = part->get_angular_velocity();
                if (is_server()) rpc("_phys_state", (int64_t)kv.first, t, lv, av);
                else             rpc_id(1, "_phys_owned", (int64_t)kv.first, t, lv, av);
            } else if (!part->is_freeze_enabled()) {
                part->set_freeze_mode(RigidBody3D::FREEZE_MODE_KINEMATIC);
                part->set_freeze_enabled(true);
            }
        }
    }
    void _phys_lerp(double dt) {
        if (phys_targets.empty()) return;
        float a = 1.0f - (float)Math::pow(0.0001, dt);
        for (auto it = phys_targets.begin(); it != phys_targets.end(); ++it) {
            if (!it->second.has) continue;
            RobloxPart* part = Object::cast_to<RobloxPart>(_rep_node(it->first));
            if (!part) continue;
            Transform3D cur = part->get_global_transform();
            const Transform3D& tgt = it->second.xform;
            cur.origin = cur.origin.lerp(tgt.origin, a);
            Quaternion q = cur.basis.get_rotation_quaternion().slerp(tgt.basis.get_rotation_quaternion(), a);
            cur.basis = Basis(q);
            part->set_global_transform(cur);
        }
    }
    // Auto ownership (como Roblox): el servidor da cada parte no anclada en modo
    // Auto al jugador MÁS CERCANO dentro de rango (o al servidor si nadie cerca).
    // Con histéresis + throttle (~2Hz) para no oscilar en las fronteras.
    void _phys_auto_tick() {
        if (!is_server()) return;
        struct PP { int peer; Vector3 pos; };
        std::vector<PP> plrs;
        TypedArray<Node> roster = get_players_roster();
        for (int i = 0; i < roster.size(); i++) {
            PlayerObject* po = Object::cast_to<PlayerObject>(roster[i]);
            if (!po) continue;
            Node3D* ch = Object::cast_to<Node3D>(po->get_character());
            if (ch) plrs.push_back({ peer_for_player_node(po), ch->get_global_position() });
        }
        const float RANGE2 = 150.0f * 150.0f;   // fuera de rango → servidor
        for (auto& kv : net_instances) {
            RobloxPart* part = Object::cast_to<RobloxPart>(_rep_node(kv.first));
            if (!part || part->get_anchored()) continue;
            bool is_auto = !part->has_meta("_gl_owner_auto") || (bool)part->get_meta("_gl_owner_auto");
            if (!is_auto) continue;   // dueño fijado a mano → no tocar
            Vector3 pp = part->get_global_position();
            int best = 0; float best_d = RANGE2;
            for (auto& pl : plrs) { float d = pl.pos.distance_squared_to(pp); if (d < best_d) { best_d = d; best = pl.peer; } }
            int cur = part->has_meta("_gl_owner") ? (int)part->get_meta("_gl_owner") : 0;
            if (best == cur) continue;
            // Histéresis: si ya hay un dueño-jugador, solo cambiar si el nuevo está
            // >25% más cerca (evita flip-flop y spam de RPC en la frontera).
            if (cur != 0 && best != 0) {
                for (auto& pl : plrs) if (pl.peer == cur) {
                    if (best_d > pl.pos.distance_squared_to(pp) * 0.75f) best = cur;
                    break;
                }
            }
            if (best == cur) continue;
            _apply_owner(kv.first, best, true);
            rpc("_phys_owner", (int64_t)kv.first, best, true);
        }
    }

    // Snapshot de propiedades a replicar (shadow "_glrep_*"). Para Node3D con
    // transform tocada, manda la posición/rotación GLOBAL resuelta en vez del
    // valor crudo: el cliente la aplica siempre en-árbol (add_child antes de
    // props) → set_global_*, así coincide sin importar el orden de escritura ni
    // el offset del ancestro (fix del bug local/global).
    Dictionary rep_build_props(Node* n) {
        Dictionary props;
        TypedArray<StringName> metas = n->get_meta_list();
        for (int i = 0; i < metas.size(); i++) {
            String mk = String(metas[i]);
            if (mk.begins_with("_glrep_")) props[mk.substr(7)] = n->get_meta(StringName(mk));
        }
        if (Node3D* n3 = Object::cast_to<Node3D>(n)) {
            if (props.has("Position") || props.has("Rotation") || props.has("CFrame")) {
                props.erase("Position"); props.erase("Rotation"); props.erase("CFrame");
                props["Position"] = n3->get_global_position();
                props["Rotation"] = n3->get_global_rotation_degrees();
            }
        }
        // Subtipo de Value: gclass es "RobloxValue" para todos; el subtipo real
        // (IntValue/StringValue/…) vive en roblox_class → replicarlo (#10).
        if (RobloxValue* rv = Object::cast_to<RobloxValue>(n)) props["__glvclass"] = rv->roblox_class;
        return props;
    }

    // (servidor) difundir creación / propiedad / re-parent / destrucción
    void rep_send_new(int64_t id, const String& gclass, const String& name, const String& parent_ref, const Dictionary& props) {
        if (mode != 1) return;
        rpc("_rep_new", id, gclass, name, parent_ref, props);
    }
    void rep_send_prop(int64_t id, const String& key, const Variant& enc) {
        if (mode != 1) return;
        rpc("_rep_prop", id, key, enc);
    }
    void rep_send_reparent(int64_t id, const String& parent_ref) {
        if (mode != 1) return;
        rpc("_rep_reparent", id, parent_ref);
    }
    // Olvida el netId de TODO el subárbol (raíz + hijos) y quita sus metas.
    void _rep_collect_forget(Node* n) {
        if (!n) return;
        if (n->has_meta("_gl_netid")) {
            net_instances.erase((uint64_t)(int64_t)n->get_meta("_gl_netid"));
            n->remove_meta("_gl_netid");
        }
        for (int i = 0; i < n->get_child_count(); i++) _rep_collect_forget(n->get_child(i));
    }
    // (servidor) el subárbol deja de replicarse: los clientes destruyen su copia
    // y el servidor olvida los netId. NO libera el nodo del servidor (lo usa tanto
    // Destroy —que libera aparte— como mover a ServerStorage —que lo conserva—).
    void rep_unreplicate(Node* n) {
        if (mode != 1 || !n || !n->has_meta("_gl_netid")) return;
        int64_t root = (int64_t)n->get_meta("_gl_netid");
        _rep_collect_forget(n);
        rpc("_rep_destroy", root);
    }

    // (servidor) mandar el estado ya replicado a un peer recién conectado. Los
    // clientes ya no corren ServerScripts → dependen de esto para ver lo que el
    // servidor creó ANTES de que ellos conectaran (late-join).
    void _rep_sync_peer(int id) {
        // Orden TOPOLÓGICO (padre replicado antes que hijo), no por netId: un nodo
        // re-parentado bajo un padre de netId mayor rompería el orden ascendente y
        // el cliente lo descartaría por padre inexistente. Punto fijo multipasada.
        std::vector<std::pair<uint64_t, Node*>> live;
        for (auto& kv : net_instances) {
            Node* n = Object::cast_to<Node>(ObjectDB::get_instance(kv.second));
            if (n) live.push_back(std::pair<uint64_t, Node*>(kv.first, n));
        }
        std::unordered_set<uint64_t> sent;
        bool progress = true;
        while (progress) {
            progress = false;
            for (auto& pr : live) {
                if (sent.count(pr.first)) continue;
                Node* n = pr.second;
                Node* parent = n->get_parent();
                if (parent && parent->has_meta("_gl_netid")) {
                    uint64_t pid = (uint64_t)(int64_t)parent->get_meta("_gl_netid");
                    if (net_instances.count(pid) && !sent.count(pid)) continue;   // padre aún no enviado
                }
                String parent_ref;
                if (parent && parent->has_meta("_gl_netid"))
                    parent_ref = String("#") + String::num_int64((int64_t)parent->get_meta("_gl_netid"));
                else if (parent)
                    parent_ref = String(parent->get_path());
                rpc_id(id, "_rep_new", (int64_t)pr.first, n->get_class(), String(n->get_name()), parent_ref, rep_build_props(n));
                sent.insert(pr.first);
                progress = true;
            }
        }
    }

    // (cliente) receptores — solo aceptan del servidor (peer 1)
    void _rep_new(int64_t id, String gclass, String name, String parent_ref, Dictionary props) {
        if (get_multiplayer()->get_remote_sender_id() != 1) return;
        if (net_instances.count((uint64_t)id)) return;   // ya existe (idempotente ante re-sync)
        Object* o = ClassDB::instantiate(StringName(gclass));
        Node* n = Object::cast_to<Node>(o);
        if (!n) { if (o) memdelete(o); return; }
        n->set_name(name);
        n->set_meta("_gl_netid", id);
        net_instances[(uint64_t)id] = (uint64_t)n->get_instance_id();
        Node* parent = _rep_resolve_parent(parent_ref);
        if (!parent) { net_instances.erase((uint64_t)id); memdelete(n); return; }
        parent->add_child(n);
        // Restaurar el subtipo de Value (#10) antes de aplicar props (no es una prop del hook).
        if (RobloxValue* rv = Object::cast_to<RobloxValue>(n)) {
            if (props.has("__glvclass")) { rv->roblox_class = props["__glvclass"]; props.erase("__glvclass"); }
        }
        if (gl_apply_prop_hook()) {
            Array keys = props.keys();
            for (int i = 0; i < keys.size(); i++) {
                String k = keys[i];
                gl_apply_prop_hook()(n, k, props[k]);
            }
        }
    }
    void _rep_prop(int64_t id, String key, Variant enc) {
        if (get_multiplayer()->get_remote_sender_id() != 1) return;
        Node* n = _rep_node((uint64_t)id);
        if (n && gl_apply_prop_hook()) gl_apply_prop_hook()(n, key, enc);
    }
    void _rep_reparent(int64_t id, String parent_ref) {
        if (get_multiplayer()->get_remote_sender_id() != 1) return;
        Node* n = _rep_node((uint64_t)id);
        if (!n) return;
        Node* parent = _rep_resolve_parent(parent_ref);
        if (!parent) return;
        if (n->get_parent()) n->get_parent()->remove_child(n);
        parent->add_child(n);
    }
    void _rep_destroy(int64_t id) {
        if (get_multiplayer()->get_remote_sender_id() != 1) return;
        Node* n = _rep_node((uint64_t)id);
        if (n) { _rep_collect_forget(n); if (n->get_parent()) n->get_parent()->remove_child(n); n->queue_free(); }
        else net_instances.erase((uint64_t)id);
    }

    // ── Roster de jugadores (Players:GetPlayers / FireClient targeting) ───
    // v1 pragmático: el "jugador" es el nodo-personaje local o el muñeco
    // (Puppet) del peer remoto. Un objeto Player propio + PlayerAdded por red
    // llegan en la siguiente tanda.
    // Devuelven OBJETOS Player (Player ≠ Character): OnServerEvent y
    // Players:GetPlayers reciben Players reales con UserId/Name/.Character.
    Node* player_node_for_peer(int peer_id) {
        if (peer_id == get_peer_id()) return _local_player_obj();
        return _peer_player(peer_id);
    }
    int peer_for_player_node(Node* player) {
        if (!player) return 0;
        if (player == (Node*)_local_player_obj()) return get_peer_id();
        for (auto& kv : peer_player_ids)
            if ((Node*)Object::cast_to<PlayerObject>(ObjectDB::get_instance(kv.second)) == player) return kv.first;
        return 0;
    }
    TypedArray<Node> get_players_roster() {
        TypedArray<Node> r;
        // El jugador local solo cuenta si TIENE personaje: la ventana "servidor"
        // (host sin personaje, cámara libre) no debe aparecer como jugador.
        if (PlayerObject* lp = _local_player_obj())
            if (lp->get_character()) r.push_back(lp);
        for (auto& kv : peer_player_ids)
            if (PlayerObject* po = Object::cast_to<PlayerObject>(ObjectDB::get_instance(kv.second)))
                if (po->get_character()) r.push_back(po);   // solo peers con personaje (no el servidor)
        return r;
    }

    // ── RPCs de réplica ──────────────────────────────────────────────────
    // Cliente -> servidor: "aquí estoy". El servidor lo muestra y lo reenvía.
    void _relay_state(Vector3 pos, float yaw, int idx) {
        int sid = get_multiplayer()->get_remote_sender_id();
        if (_diag_tick()) { gl_mp_log(String("RX relay de peer ") + String::num_int64(sid) + " pos=" + String(pos)); diag_timer = 1.0; }
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
        if (_diag_tick()) { gl_mp_log(String("RX estado de ") + String::num_int64(who) + " pos=" + String(pos)); diag_timer = 1.0; }
        _update_puppet3d(who, idx, pos, yaw);
    }
    void _recv_state2d(int who, Vector2 pos, float rot, int idx) {
        Ref<MultiplayerAPI> m = _mp();
        int myid = (m.is_valid() && m->has_multiplayer_peer()) ? m->get_unique_id() : 0;
        if (who == myid) return;
        _update_puppet2d(who, idx, pos, rot);
    }
    void _recv_remove(int who) { _remove_puppet(who); _remove_peer_player(who); }

    // ── CHAT EN RED (como Roblox: todos ven los mensajes de todos) ────────
    // El RobloxChat local llama gl_send_chat al enviar; el servidor lo muestra
    // y lo reenvia a todos; cada ventana lo agrega a su chat.
    void _show_chat(const String& name, const String& text) {
        if (!is_inside_tree()) return;
        Node* tcs = _find_by_class((Node*)get_tree()->get_root(), "TextChatService");
        if (tcs) tcs->call("send_message", name, text);
        gl_mp_log(String("chat << ") + name + ": " + text);
    }
    void gl_send_chat(const String& name, const String& text) {
        Ref<MultiplayerAPI> m = _mp();
        if (m.is_null() || !m->has_multiplayer_peer()) return;   // sin sesion: solo local
        if (is_server()) rpc("_recv_chat", 1, name, text);
        else             rpc_id(1, "_relay_chat", name, text);
        gl_mp_log(String("chat >> ") + name + ": " + text);
    }
    void _relay_chat(String name, String text) {
        int sid = get_multiplayer()->get_remote_sender_id();
        _show_chat(name, text);                    // el servidor tambien lo ve
        rpc("_recv_chat", sid, name, text);        // reenviar a todos los clientes
    }
    void _recv_chat(int from, String name, String text) {
        Ref<MultiplayerAPI> m = _mp();
        int myid = (m.is_valid() && m->has_multiplayer_peer()) ? m->get_unique_id() : 0;
        if (from == myid) return;                  // yo ya lo mostre al enviarlo
        _show_chat(name, text);
    }

    // ── PING (para el menu de ajustes, como el de Roblox) ─────────────────
    // El cliente manda su reloj al servidor cada segundo; el servidor lo
    // devuelve tal cual; el cliente resta contra SU reloj → ida y vuelta en ms.
    void _ping_req(double t) {
        int sid = get_multiplayer()->get_remote_sender_id();
        rpc_id(sid, "_ping_res", t);
    }
    void _ping_res(double t) {
        ping_ms = (double)Time::get_singleton()->get_ticks_msec() - t;
    }
    // Reloj sincronizado (1.14.8). El cliente manda su reloj; el servidor responde
    // con el suyo (unix + DGT). El cliente estima el desfase con media vuelta.
    void _time_req(double c_unix, double c_dgt) {
        int sid = get_multiplayer()->get_remote_sender_id();
        rpc_id(sid, "_time_res", c_unix, c_dgt,
               Time::get_singleton()->get_unix_time_from_system(),
               (double)Time::get_singleton()->get_ticks_usec() / 1000000.0);
    }
    void _time_res(double c_unix, double c_dgt, double s_unix, double s_dgt) {
        if (get_multiplayer()->get_remote_sender_id() != 1) return;   // solo del servidor
        double now_unix = Time::get_singleton()->get_unix_time_from_system();
        double now_dgt  = (double)Time::get_singleton()->get_ticks_usec() / 1000000.0;
        double half = (now_unix - c_unix) * 0.5;   // rtt/2 en segundos
        if (half < 0.0) half = 0.0;
        double new_unix_off = (s_unix + half) - now_unix;
        double new_dgt_off  = (s_dgt + half) - now_dgt;
        if (!time_synced) {                          // 1er sync: snap
            gl_server_unix_offset() = new_unix_off;
            gl_dgt_offset() = new_dgt_off;
            time_synced = true;
        } else {                                     // suavizar (evita saltos del reloj)
            gl_server_unix_offset() += (new_unix_off - gl_server_unix_offset()) * 0.25;
            gl_dgt_offset()         += (new_dgt_off  - gl_dgt_offset())         * 0.25;
        }
    }
    double get_ping_ms() {
        if (mode == 1) return 0.0;                      // el servidor/host: 0
        if (mode == 2 && net_connected) return ping_ms; // cliente conectado
        return -1.0;                                    // sin sesion
    }

    // ── Vista de Servidor: poner (true) o quitar (false) la cámara libre ──
    // Devuelve false si aún no se puede aplicar (sin cámara todavía) para que
    // el watcher reintente en el próximo tick.
    bool _set_view(bool server) {
        if (server == gl_freecam().active) return true;   // ya está así
        if (server) {
            Viewport* vp = get_viewport();
            Camera3D* cur = vp ? vp->get_camera_3d() : nullptr;
            prevcam_id = cur ? (uint64_t)cur->get_instance_id() : 0;
            GLFreeCamera* fc = memnew(GLFreeCamera);
            fc->set_name("GL_ServerFreeCam");
            get_tree()->get_root()->add_child(fc);
            if (cur) {
                fc->set_global_transform(cur->get_global_transform());
            } else {
                // Sin camara previa (ventana del servidor sin personaje):
                // vista aerea del spawn, mirando al centro del mapa
                fc->set_global_position(Vector3(0.0f, 14.0f, 26.0f));
                fc->set_rotation(Vector3(-0.45f, 0.0f, 0.0f));
            }
            fc->make_current();
            freecam_id = (uint64_t)fc->get_instance_id();
            gl_freecam().active = true;
            gl_mp_log("Vista de SERVIDOR: camara libre (WASD + click derecho; el personaje no se mueve)");
        } else {
            gl_freecam().active = false;
            if (Object* o = ObjectDB::get_instance(freecam_id))
                if (Node* n = Object::cast_to<Node>(o)) n->queue_free();
            freecam_id = 0;
            Camera3D* pc = Object::cast_to<Camera3D>(ObjectDB::get_instance(prevcam_id));
            if (pc) pc->make_current();
            Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
            gl_mp_log("Vista de CLIENTE: camara del jugador");
        }
        return true;
    }

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
        // 0-. Instancia (mundo lanzado por el coordinador): publica su latido
        //     (puerto + nº de jugadores) para que el coordinador sepa su estado.
        if (is_instance) {
            inst_hb_timer += dt;
            if (inst_hb_timer >= 1.0) { inst_hb_timer = 0.0; _inst_write_heartbeat(); }
        }
        // 0-. Cliente: reconectar al MUNDO que asignó el coordinador (reintenta,
        //     el mundo recién arrancado puede tardar en abrir su puerto).
        if (reconnect_try > 0) {
            if (net_connected) { reconnect_try = 0; }
            else {
                reconnect_timer += dt;
                if (reconnect_timer >= 0.6) {
                    reconnect_timer = 0.0;
                    reconnect_try--;
                    connect_server(reconnect_ip, reconnect_port);
                    if (reconnect_try == 0 && !net_connected)
                        UtilityFunctions::push_warning("[GodotLuau] No se pudo entrar al mundo asignado por el coordinador.");
                }
            }
        }
        // 0a. La ventana del servidor arranca en camara libre (reintenta hasta
        //     que exista una camara que reemplazar)
        if (boot_server_view && _set_view(true)) boot_server_view = false;
        // 0b. Toggle Server/Client View pedido desde el panel Game del editor
        if (view_watch) _poll_view_cmd(dt);
        // 0c. TRANSFORMACION EN VIVO: una ventana del editor sin rol (la nativa,
        //     incluso si quedo corriendo de una prueba anterior) se convierte en
        //     el SERVIDOR cuando detecta una sesion recien creada por el Play.
        //     Eleccion por puerto: solo UNA gana; si el puerto ya es de otro, nada.
        if (view_watch && mode == 0 && player_index == 0) {
            xform_poll += dt;
            if (xform_poll >= 0.5) {
                xform_poll = 0.0;
                int scnt; String sdev; double sage;
                if (gl_mp_session_read_ex(scnt, sdev, sage) && scnt >= 2 && sage < 30.0) {
                    if (start_server(25575, scnt + 2)) {
                        device = sdev;
                        player_index = 1;
                        _remove_local_player();
                        boot_server_view = true;
                        _apply_window_layout(1, scnt);
                        gl_mp_log("sesion multijugador detectada -> esta ventana ahora es el SERVIDOR");
                    }
                }
            }
        }
        if (diag_timer > 0.0) diag_timer -= dt;
        // 1. Reintentos de conexión del cliente (hasta que el host abra el puerto)
        if (retry_left > 0) {
            if (net_connected) {
                retry_left = 0;
            } else {
                retry_timer += dt;
                if (retry_timer >= 1.5) {
                    retry_timer = 0.0;
                    retry_left--;
                    // Red de seguridad final: si tras ~6s nadie hostea, el PRIMER
                    // jugador se promueve a host (sigue jugando como Player1) para
                    // que la sesion SIEMPRE se forme.
                    if (retry_left == 16 && player_index == 2 && !net_connected) {
                        if (start_server(25575, 10)) {
                            retry_left = 0;
                            gl_mp_log("nadie hostea; Player1 se promueve a HOST de respaldo (sigue jugando)");
                            return;
                        }
                    }
                    if (retry_left > 0) connect_server(gl_mp_host(), 25575);
                    else if (!net_connected && player_index >= 2 && is_inside_tree()) {
                        gl_mp_log("no se encontro el host; cerrando esta ventana");
                        get_tree()->quit();   // cliente huérfano: el host nunca abrió el puerto
                    }
                }
            }
        }
        // 2. Réplica: difundir mi estado y suavizar a los muñecos
        Ref<MultiplayerAPI> m = _mp();
        if (m.is_valid() && m->has_multiplayer_peer()) {
            if (m->get_peers().size() > 0) {
                bcast_timer += dt;
                if (bcast_timer >= 0.05) { bcast_timer = 0.0; _broadcast_local(); }
                // Ping: el cliente pregunta cada segundo
                if (mode == 2 && net_connected) {
                    ping_timer += dt;
                    if (ping_timer >= 1.0) {
                        ping_timer = 0.0;
                        rpc_id(1, "_ping_req", (double)Time::get_singleton()->get_ticks_msec());
                    }
                    // Reloj sincronizado (1.14.8): rápido hasta el 1er sync, luego cada ~2.5s
                    time_sync_timer += dt;
                    if (time_sync_timer >= (time_synced ? 2.5 : 0.4)) {
                        time_sync_timer = 0.0;
                        rpc_id(1, "_time_req",
                               Time::get_singleton()->get_unix_time_from_system(),
                               (double)Time::get_singleton()->get_ticks_usec() / 1000000.0);
                    }
                }
            }
            _lerp_puppets(dt);
            // Física en red (1.14.7): la autoridad transmite ~20Hz; todos interpolan.
            phys_timer += dt;
            if (phys_timer >= 0.05) { phys_timer = 0.0; _phys_tick(); }
            _phys_lerp(dt);
            // Auto ownership (1.14.7.1): el servidor reasigna al más cercano ~2Hz.
            if (is_server()) {
                phys_auto_timer += dt;
                if (phys_auto_timer >= 0.5) { phys_auto_timer = 0.0; _phys_auto_tick(); }
            }
        }
    }

    // Resuelve el rol de esta instancia y auto-conecta el multijugador local.
    //   · ventana NATIVA (sin --glindex) + sesion res:// con count>=2  → SERVIDOR
    //     (sin personaje, camara libre — como la vista de servidor de Roblox)
    //   · --glindex 2..N+1 → cliente PlayerN (glindex-1)
    //   · sin nada → un jugador normal
    void _auto_init() {
        PackedStringArray args = OS::get_singleton()->get_cmdline_user_args();
        int idx = 0, cnt = 1;
        String direct_connect = "";   // --glconnect <ip>:<port>  (unirse por IP)
        int    dedicated_port = 0;    // --glserver  <port>       (hospedar por IP)
        int    coordinator_port = 0;  // --glhost   <port>        (COORDINADOR de mundos)
        String matchmaking = "";      // --glmatch  <ip>:<port>   (cliente por matchmaking)
        bool   instance_flag = false; // --glinstance             (este server es un MUNDO del coordinador)
        String secret = "";           // --glsecret <key>         (llave del dueño)
        int    maxper = 8;            // --glmax    <n>           (máx jugadores por mundo)
        for (int i = 0; i < args.size(); i++) {
            String a = args[i];
            if      (a == "--glindex"    && i + 1 < args.size()) idx = String(args[i + 1]).to_int();
            else if (a == "--glcount"    && i + 1 < args.size()) cnt = String(args[i + 1]).to_int();
            else if (a == "--gldevice"   && i + 1 < args.size()) device = args[i + 1];
            else if (a == "--glconnect"  && i + 1 < args.size()) direct_connect = args[i + 1];
            else if (a == "--glserver"   && i + 1 < args.size()) dedicated_port = String(args[i + 1]).to_int();
            else if (a == "--glhost"     && i + 1 < args.size()) coordinator_port = String(args[i + 1]).to_int();
            else if (a == "--glmatch"    && i + 1 < args.size()) matchmaking = args[i + 1];
            else if (a == "--glinstance") instance_flag = true;
            else if (a == "--glsecret"   && i + 1 < args.size()) secret = args[i + 1];
            else if (a == "--glmax"      && i + 1 < args.size()) maxper = String(args[i + 1]).to_int();
        }
        if (!secret.is_empty()) host_secret = secret;
        if (maxper > 0) coord_max = maxper;
        String role_src = "user args";
        if (idx == 0) {
            // Ventana nativa: la sesion res:// dice si hay prueba multijugador
            // (count>=2 → esta ventana es el SERVIDOR) y el dispositivo emulado.
            int scnt; String sdev; double sage;
            if (gl_mp_session_read_ex(scnt, sdev, sage)) {
                device = sdev;
                if (scnt >= 2) { idx = 1; cnt = scnt; role_src = "res://.gl_mp_session"; }
                gl_mp_log(String("sesion res:// leida: count=") + String::num_int64(scnt)
                          + " device=" + sdev + " edad=" + String::num_int64((int64_t)sage) + "s");
            } else {
                gl_mp_log("sin sesion res:// al arrancar (la vigilo por si aparece)");
            }
        }
        // Diagnostico (solo Modo Debug): con esto se ve EXACTAMENTE que llego
        gl_mp_log(String("user args = [") + String(", ").join(args) + "]");
        gl_mp_log(String("rol: idx=") + String::num_int64(idx) + " count=" + String::num_int64(cnt)
                  + " device=" + device + " (fuente: " + role_src + ")");
        player_index = idx;

        // ── COORDINADOR de mundos (lo corre el DUEÑO) ──────────────────────
        //   --glhost <port> [--glmax N]  → reparte jugadores a mundos, respeta N
        if (coordinator_port > 0) {
            start_coordinator(coordinator_port, coord_max);
            view_watch = false; set_process(true); _apply_device();
            return;
        }
        // ── Cliente por MATCHMAKING (auto-join a un mundo con gente) ────────
        //   --glmatch <ip>:<port>  → se conecta al coordinador y este lo redirige
        if (!matchmaking.is_empty()) {
            String host = matchmaking; int cport = 25565;
            int colon = matchmaking.rfind(":");
            if (colon > 0) { host = matchmaking.substr(0, colon); cport = matchmaking.substr(colon + 1).to_int(); }
            join_matchmaking(host, cport);
            gl_mp_log(String("Matchmaking: pidiendo mundo al coordinador ") + host + String(":") + String::num_int64(cport));
            view_watch = false; set_process(true); _apply_device();
            return;
        }
        // ── Auto-join BAKED (juego exportado) ──────────────────────────────
        // El dueño pone res://gl_match.cfg = "ip:puerto" (su coordinador) ANTES de
        // exportar; el jugador al abrir el juego se une solo a un mundo con gente.
        // Solo en build exportado (en el editor se hace el test local con glindex).
        if (!OS::get_singleton()->has_feature("editor")) {
            Ref<FileAccess> mf = FileAccess::open("res://gl_match.cfg", FileAccess::READ);
            if (mf.is_valid()) {
                String line = mf->get_line().strip_edges(); mf->close();
                if (!line.is_empty()) {
                    String host = line; int cport = 25565;
                    int colon = line.rfind(":");
                    if (colon > 0) { host = line.substr(0, colon); cport = line.substr(colon + 1).to_int(); }
                    join_matchmaking(host, cport);
                    UtilityFunctions::print(String("[GodotLuau] Auto-join al coordinador ") + host + String(":") + String::num_int64(cport));
                    view_watch = false; set_process(true); _apply_device();
                    return;
                }
            }
        }

        // ── Direct Connect / servidor por argumento (juegos exportados) ─────
        // Permite unir/hospedar por IP entre máquinas distintas sin escribir
        // Lua (Minecraft-style). Tiene prioridad sobre el test local por glindex.
        //   --glconnect <ip>:<port>   → cliente que se une a ese servidor
        //   --glserver  <port>        → host escuchando en ese puerto
        if (!direct_connect.is_empty()) {
            String host = direct_connect; int cport = 25565;
            int colon = direct_connect.rfind(":");
            if (colon > 0) { host = direct_connect.substr(0, colon); cport = direct_connect.substr(colon + 1).to_int(); }
            connect_server(host, cport);
            retry_left = 20;
            gl_mp_log(String("Direct Connect a ") + host + String(":") + String::num_int64(cport));
            view_watch = false; set_process(true); _apply_device();
            return;
        }
        if (dedicated_port > 0) {
            // Servidor DEDICADO (estilo minecraft_server.jar): corre los
            // ServerScripts del creador y acepta clientes, SIN jugador local.
            // Con --headless corre sin ventana; si no, deja cámara libre para verlo.
            if (start_server(dedicated_port, 64)) {
                is_dedicated = true;
                player_index = 1;
                _remove_local_player();     // servidor puro: sin personaje
                boot_server_view = true;    // cámara libre (ignorada en headless)
                if (instance_flag) {        // es un MUNDO del coordinador → publica latido
                    is_instance = true;
                    coord_port  = dedicated_port;   // su propio puerto (para el latido)
                    _inst_write_heartbeat();
                }
                UtilityFunctions::print(String("[GodotLuau] ") + (instance_flag ? String("MUNDO") : String("SERVIDOR DEDICADO"))
                    + " escuchando en el puerto " + String::num_int64(dedicated_port) + " (sin jugador local).");
            } else {
                UtilityFunctions::push_warning(String("[GodotLuau] No se pudo abrir el servidor dedicado en el puerto ")
                    + String::num_int64(dedicated_port));
            }
            view_watch = false; set_process(true); _apply_device();
            return;
        }

        if (idx >= 1 && cnt >= 2) {
            bool as_client = (idx != 1);
            if (idx == 1) {
                // Eleccion por puerto: si otra instancia ya abrio el 25575,
                // esta se une como cliente (robusto ante un doble servidor).
                if (!start_server(25575, cnt + 2)) { as_client = true; gl_mp_log("puerto 25575 ocupado -> me uno como cliente"); }
            }
            if (as_client) {
                String host = gl_mp_host();
                connect_server(host, 25575);
                retry_left = 20;
                _name_local_player(idx - 1);   // glindex 2..N+1 → Player1..PlayerN
                _offset_spawn(idx);            // que no nazcan uno dentro de otro
                gl_mp_log(String("cliente Player") + String::num_int64(idx - 1) + " conectando a " + host + ":25575...");
            } else {
                // VENTANA DEL SERVIDOR: observa con camara libre, sin personaje
                _remove_local_player();
                boot_server_view = true;   // la camara del jugador puede tardar un frame
                gl_mp_log(String("SERVIDOR escuchando en 25575 (") + String::num_int64(cnt) + " jugadores, dispositivo " + device + ")");
            }
            _apply_window_layout(idx, cnt);
        } else {
            gl_mp_log("modo un jugador");
        }
        // El watcher del toggle Server/Client corre siempre que el juego se
        // lanzo desde el editor (tambien con 1 jugador, como Roblox Studio).
        view_watch = OS::get_singleton()->has_feature("editor");
        set_process(true);
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
        gl_net_role() = 1; gl_net_service_id() = (uint64_t)get_instance_id();   // replicación: somos servidor
        // UserId secuencial: el host es SIEMPRE el jugador 1; los peers toman 2,3,...
        peer_seq_uid[1] = 1; next_seq_uid = 2; gl_local_user_id() = 1;
        if (PlayerObject* lp = _local_player_obj()) lp->user_id = 1;
        gl_run_deferred_server_scripts();   // si esta ventana era cliente y se promovió a host, corre ahora sus ServerScripts
        set_process(true);
        return true;
    }
    bool connect_server(const String& ip, int port) {
        _bind_mp_signals();
        peer.instantiate();
        // ENet exige una IP numérica: si llega un hostname, se resuelve por DNS.
        String addr = ip;
        if (!ip.is_valid_ip_address()) {
            String resolved = IP::get_singleton()->resolve_hostname(ip, IP::TYPE_ANY);
            if (!resolved.is_empty()) addr = resolved;
            else { UtilityFunctions::push_warning(String("[NetworkService] No se pudo resolver el host ") + ip); return false; }
        }
        target_address = ip + String(":") + String::num_int64(port);
        Error e = peer->create_client(addr, port);
        if (e != OK) { UtilityFunctions::push_warning(String("[NetworkService] No se pudo conectar a ") + ip); target_address = ""; return false; }
        Ref<MultiplayerAPI> m = _mp();
        if (m.is_valid()) m->set_multiplayer_peer(peer);
        mode = 2;
        gl_net_role() = 2; gl_net_service_id() = (uint64_t)get_instance_id();   // replicación: somos cliente
        set_process(true);
        return true;
    }
    void disconnect_net() {
        _clear_puppets();
        Ref<MultiplayerAPI> m = _mp();
        if (m.is_valid()) m->set_multiplayer_peer(Ref<MultiplayerPeer>());
        peer = Ref<ENetMultiplayerPeer>();
        mode = 0;
        gl_net_role() = 0;   // replicación off
        net_instances.clear();
        peer_seq_uid.clear(); next_seq_uid = 2; gl_local_user_id() = 0;   // UserId: reset al salir
        gl_server_unix_offset() = 0.0; gl_dgt_offset() = 0.0; time_synced = false;   // reloj: volver al local
        net_connected = false;
        target_address = "";
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
    bool is_dedicated_server() const { return is_dedicated; }

    // IP local (LAN) para que el host se la pase a sus amigos ("hospedar en mi PC").
    // Devuelve la primera IPv4 no-loopback; "127.0.0.1" si no hay red.
    String get_local_ip() {
        PackedStringArray addrs = IP::get_singleton()->get_local_addresses();
        for (int i = 0; i < addrs.size(); i++) {
            String a = addrs[i];
            if (a.is_valid_ip_address() && a.get_slice_count(".") == 4 && a != "127.0.0.1")
                return a;
        }
        return "127.0.0.1";
    }

    // ── Lista de servidores guardados (estilo lista de Minecraft) ────────
    // Persistida en user://gl_servers.json: [{"name","ip","port"}].
    static String _servers_path() { return "user://gl_servers.json"; }
    Array get_server_list() {
        Ref<FileAccess> f = FileAccess::open(_servers_path(), FileAccess::READ);
        if (f.is_null()) return Array();
        String txt = f->get_as_text();
        f->close();
        Variant v = JSON::parse_string(txt);
        return (v.get_type() == Variant::ARRAY) ? (Array)v : Array();
    }
    void _save_server_list(const Array& list) {
        Ref<FileAccess> f = FileAccess::open(_servers_path(), FileAccess::WRITE);
        if (f.is_valid()) { f->store_string(JSON::stringify(list, "  ")); f->close(); }
    }
    void add_server(const String& name, const String& ip, int port) {
        Array list = get_server_list();
        for (int i = 0; i < list.size(); i++) {   // reemplazar si el nombre ya existe
            Dictionary d = list[i];
            if (String(d.get("name", "")) == name) { list.remove_at(i); break; }
        }
        Dictionary d; d["name"] = name; d["ip"] = ip; d["port"] = port;
        list.push_back(d);
        _save_server_list(list);
    }
    void remove_server(const String& name) {
        Array list = get_server_list();
        for (int i = 0; i < list.size(); i++) {
            Dictionary d = list[i];
            if (String(d.get("name", "")) == name) { list.remove_at(i); _save_server_list(list); return; }
        }
    }
    bool join_server(const String& name) {
        Array list = get_server_list();
        for (int i = 0; i < list.size(); i++) {
            Dictionary d = list[i];
            if (String(d.get("name", "")) == name)
                return connect_server(String(d.get("ip", "127.0.0.1")), (int)d.get("port", 25565));
        }
        return false;
    }

    // ════════════════════════════════════════════════════════════════
    //  Coordinador / matchmaker (mundos independientes estilo Roblox)
    //  El dueño corre el COORDINADOR; al entrar un jugador, lo manda a un
    //  MUNDO (instancia = proceso aparte en otro puerto) con espacio, o
    //  arranca uno nuevo si todos están llenos (respetando el máximo).
    // ════════════════════════════════════════════════════════════════
    static String _host_key_path() { return "user://host.key"; }
    String _gen_key() {
        static const char* al = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
        uint64_t s = (uint64_t)Time::get_singleton()->get_ticks_usec() ^ 0x9E3779B97F4A7C15ULL;
        String k;
        for (int i = 0; i < 24; i++) { s = s * 6364136223846793005ULL + 1442695040888963407ULL; k += String::chr(al[(s >> 33) % 31]); }
        return k;
    }
    // Devuelve la llave del dueño; la crea la primera vez que él hostea.
    String read_or_make_host_key() {
        String k = get_host_key();
        if (!k.is_empty()) return k;
        k = _gen_key();
        Ref<FileAccess> w = FileAccess::open(_host_key_path(), FileAccess::WRITE);
        if (w.is_valid()) { w->store_string(k); w->close(); }
        return k;
    }
    String get_host_key() {
        Ref<FileAccess> f = FileAccess::open(_host_key_path(), FileAccess::READ);
        if (f.is_null()) return "";
        String k = f->get_as_text().strip_edges(); f->close(); return k;
    }

    struct InstInfo { int port; int players; };
    std::vector<InstInfo> _coord_read_instances() {
        std::vector<InstInfo> out;
        Ref<DirAccess> d = DirAccess::open("user://");
        if (d.is_null()) return out;
        double now = Time::get_singleton()->get_unix_time_from_system();
        PackedStringArray files = d->get_files();
        for (int i = 0; i < files.size(); i++) {
            String fn = files[i];
            if (!fn.begins_with("gl_inst_") || !fn.ends_with(".json")) continue;
            Ref<FileAccess> f = FileAccess::open(String("user://") + fn, FileAccess::READ);
            if (f.is_null()) continue;
            Variant v = JSON::parse_string(f->get_as_text()); f->close();
            if (v.get_type() != Variant::DICTIONARY) continue;
            Dictionary di = v;
            if (now - (double)di.get("t", 0) > 6.0) { d->remove(fn); continue; }  // latido viejo → mundo muerto
            InstInfo ii; ii.port = (int)di.get("port", 0); ii.players = (int)di.get("players", 0);
            if (ii.port > 0) out.push_back(ii);
        }
        return out;
    }
    int _spawn_instance(int iport) {
        String exe = OS::get_singleton()->get_executable_path();
        PackedStringArray a;
        if (OS::get_singleton()->has_feature("editor")) {
            a.push_back("--path");
            a.push_back(ProjectSettings::get_singleton()->globalize_path("res://"));
        }
        a.push_back("--headless");
        a.push_back("--");
        a.push_back("--glserver");   a.push_back(String::num_int64(iport));
        a.push_back("--glinstance");
        a.push_back("--glmax");      a.push_back(String::num_int64(coord_max));
        if (!host_secret.is_empty()) { a.push_back("--glsecret"); a.push_back(host_secret); }
        int pid = OS::get_singleton()->create_process(exe, a);
        UtilityFunctions::print(String("[GodotLuau] Coordinador: nuevo MUNDO en el puerto ")
            + String::num_int64(iport) + " (pid " + String::num_int64(pid) + ")");
        return iport;
    }
    // Elige un mundo con espacio; si no hay, lanza uno nuevo. Devuelve su puerto.
    int _coord_pick_or_spawn() {
        std::vector<InstInfo> insts = _coord_read_instances();
        for (auto& ii : insts) if (ii.players < coord_max) return ii.port;   // mundo con hueco
        int iport = coord_port + 1;                                          // puerto libre para uno nuevo
        bool used = true;
        while (used) { used = false; for (auto& ii : insts) if (ii.port == iport) { used = true; iport++; break; } }
        return _spawn_instance(iport);
    }

    // Arranca el COORDINADOR (lo corre el dueño). Requiere la llave del dueño.
    bool start_coordinator(int port, int max_per_world) {
        host_secret = read_or_make_host_key();
        if (host_secret.is_empty()) { UtilityFunctions::push_warning("[GodotLuau] No se pudo crear/leer la host.key; el coordinador no arranca."); return false; }
        coord_port = port;
        coord_max = max_per_world > 0 ? max_per_world : 8;
        if (!start_server(port, 512)) return false;   // servidor liviano solo para repartir
        is_coordinator = true;
        _remove_local_player();       // el coordinador no juega
        UtilityFunctions::print(String("[GodotLuau] COORDINADOR activo en el puerto ") + String::num_int64(port)
            + " (máx " + String::num_int64(coord_max) + " por mundo). host.key = " + host_secret);
        return true;
    }
    // Cliente: unirse por matchmaking (el coordinador te manda a un mundo).
    bool join_matchmaking(const String& ip, int port) {
        match_ip = ip; match_port = port;
        return connect_server(ip, port);
    }
    // RPC: el coordinador (peer 1) le dice al cliente a qué mundo ir.
    void _gl_assign(int iport) {
        if (get_multiplayer()->get_remote_sender_id() != 1) return;   // solo el coordinador
        reconnect_ip = match_ip.is_empty() ? gl_mp_host() : match_ip;
        reconnect_port = iport;
        reconnect_try = 40; reconnect_timer = 0.0;   // el mundo nuevo puede tardar en abrir
        gl_mp_log(String("el coordinador me asignó el mundo :") + String::num_int64(iport));
        disconnect_net();   // soltar el coordinador; el _process reconecta al mundo
    }
    // Instancia (mundo): publica su latido para que el coordinador lo vea.
    void _inst_write_heartbeat() {
        Ref<MultiplayerAPI> m = _mp();
        int players = (m.is_valid() && m->has_multiplayer_peer()) ? (int)m->get_peers().size() : 0;
        Dictionary d; d["port"] = coord_port; d["players"] = players;
        d["t"] = Time::get_singleton()->get_unix_time_from_system();
        Ref<FileAccess> f = FileAccess::open(String("user://gl_inst_") + String::num_int64(coord_port) + ".json", FileAccess::WRITE);
        if (f.is_valid()) { f->store_string(JSON::stringify(d)); f->close(); }
    }

    // ¿Hay una sesión de red activa? El host siempre; el cliente al conectar.
    bool is_connected_net() { return mode == 1 || (mode == 2 && net_connected); }
    // "ip:puerto" del servidor al que se conectó el cliente ("" si es host/single).
    String get_server_address() const { return target_address; }
    // Estado legible: "Server" / "Connecting" / "Connected" / "Disconnected".
    String get_connection_state() {
        if (mode == 1) return "Server";
        if (mode == 2) return net_connected ? "Connected" : "Connecting";
        return "Disconnected";
    }

    NetworkService() {}
};

#endif // ROBLOX_NETWORK_H
