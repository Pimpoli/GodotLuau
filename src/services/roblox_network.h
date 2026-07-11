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
#include <godot_cpp/classes/time.hpp>

#include "lua.h"
#include "lualib.h"
#include "gl_runtime.h"   // gl_state_alive, gl_freecam
#include "gl_debug.h"     // gl_debug_on (prints internos solo en Modo Debug)

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
static inline bool gl_mp_session_read(int& out_count, String& out_device) {
    Ref<FileAccess> f = FileAccess::open("res://.gl_mp_session", FileAccess::READ);
    if (f.is_null()) return false;
    PackedStringArray parts = f->get_as_text().strip_edges().split("|");
    f->close();
    if (parts.size() < 3) return false;
    int    cnt   = String(parts[0]).to_int();
    double stamp = String(parts[2]).to_float();
    double now   = Time::get_singleton()->get_unix_time_from_system();
    if (cnt < 1) return false;
    if (now - stamp > 21600.0) return false;   // 6 h: sesion de otro dia no cuenta
    out_count  = cnt;
    out_device = parts[1];
    return true;
}

// ¿Debe auto-crearse el NetworkService al arrancar el juego?
// Siempre que el juego corra desde el editor (toggle de vista, dispositivo,
// sesiones) o que lleve --glindex (ventanas cliente).
static inline bool gl_mp_autostart_requested() {
    PackedStringArray a = OS::get_singleton()->get_cmdline_user_args();
    for (int i = 0; i < a.size(); i++) if (String(a[i]) == "--glindex") return true;
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
    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        set_anchors_preset(Control::PRESET_FULL_RECT);
        HBoxContainer* hb = memnew(HBoxContainer);
        hb->set_anchors_preset(Control::PRESET_FULL_RECT);
        hb->add_theme_constant_override("separation", 4);
        add_child(hb);
        for (int i = 0; i < 2; i++) {
            SubViewportContainer* svc = memnew(SubViewportContainer);
            svc->set_stretch(true);
            svc->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            svc->set_v_size_flags(Control::SIZE_EXPAND_FILL);
            hb->add_child(svc);
            SubViewport* sv = memnew(SubViewport);
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
        Camera3D* main_cam = root ? root->get_camera_3d() : nullptr;
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

    // Vista de Servidor (cámara libre) — comandada desde el panel Game del
    // editor vía res://.gl_view_cmd ("server|N" / "client|N")
    uint64_t     freecam_id = 0;    // ObjectID de la cámara libre
    uint64_t     prevcam_id = 0;    // ObjectID de la cámara del jugador
    int          view_last_n = -1;  // último comando aplicado
    double       view_poll = 0.0;
    bool         view_watch = false;
    bool         boot_server_view = false;   // la ventana del servidor arranca en cámara libre

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

    void _name_local_player(int display_num) {
        Node* p = _local_player();
        if (p) {
            // Solo DisplayName (NO renombrar el nodo: el nombre "LocalPlayer" lo
            // usa el fallback de workspace.CurrentCamera; renombrarlo lo dejaría nil).
            p->set("DisplayName", String("Player") + String::num_int64(display_num));
            p->set_meta("PlayerIndex", display_num);
        }
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
            gl_mp_log(String("Player") + String::num_int64(idx) + " aparecio en tu mundo");
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
        if (!lp) return;   // la ventana del servidor no tiene personaje: no difunde
        bool server = is_server();
        int display_num = (player_index >= 2) ? player_index - 1 : player_index;   // glindex 2..N+1 → Player1..N
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
            float   y = n->get_rotation().y;
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
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_EXIT_TREE) _clear_puppets();
    }

public:
    // ── Callbacks de señales de red ──────────────────────────────────────
    void _on_peer_connected(int id)    { gl_mp_log(String("otro jugador entro (peer ") + String::num_int64(id) + ")"); _fire(pc_cbs, true, id); }
    void _on_peer_disconnected(int id) {
        _remove_puppet(id);
        if (is_server()) rpc("_recv_remove", id);   // que los clientes también lo quiten
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
        // 0a. La ventana del servidor arranca en camara libre (reintenta hasta
        //     que exista una camara que reemplazar)
        if (boot_server_view && _set_view(true)) boot_server_view = false;
        // 0b. Toggle Server/Client View pedido desde el panel Game del editor
        if (view_watch) _poll_view_cmd(dt);
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
            }
            _lerp_puppets(dt);
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
        for (int i = 0; i < args.size(); i++) {
            String a = args[i];
            if      (a == "--glindex"  && i + 1 < args.size()) idx = String(args[i + 1]).to_int();
            else if (a == "--glcount"  && i + 1 < args.size()) cnt = String(args[i + 1]).to_int();
            else if (a == "--gldevice" && i + 1 < args.size()) device = args[i + 1];
        }
        String role_src = "user args";
        if (idx == 0) {
            // Ventana nativa: la sesion res:// dice si hay prueba multijugador
            // (count>=2 → esta ventana es el SERVIDOR) y el dispositivo emulado.
            int scnt; String sdev;
            if (gl_mp_session_read(scnt, sdev)) {
                device = sdev;
                if (scnt >= 2) { idx = 1; cnt = scnt; role_src = "res://.gl_mp_session"; }
            }
        }
        // Diagnostico (solo Modo Debug): con esto se ve EXACTAMENTE que llego
        gl_mp_log(String("user args = [") + String(", ").join(args) + "]");
        gl_mp_log(String("rol: idx=") + String::num_int64(idx) + " count=" + String::num_int64(cnt)
                  + " device=" + device + " (fuente: " + role_src + ")");
        player_index = idx;
        if (idx >= 1 && cnt >= 2) {
            bool as_client = (idx != 1);
            if (idx == 1) {
                // Eleccion por puerto: si otra instancia ya abrio el 25575,
                // esta se une como cliente (robusto ante un doble servidor).
                if (!start_server(25575, cnt + 2)) { as_client = true; gl_mp_log("puerto 25575 ocupado -> me uno como cliente"); }
            }
            if (as_client) {
                connect_server("127.0.0.1", 25575);
                retry_left = 20;
                _name_local_player(idx - 1);   // glindex 2..N+1 → Player1..PlayerN
                gl_mp_log(String("cliente Player") + String::num_int64(idx - 1) + " conectando a 127.0.0.1:25575...");
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
