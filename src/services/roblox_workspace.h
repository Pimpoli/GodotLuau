#ifndef ROBLOX_WORKSPACE_H
#define ROBLOX_WORKSPACE_H

#include <vector>
#include <godot_cpp/classes/node3d.hpp>
#include "gl_graphics.h"
#include <godot_cpp/classes/engine.hpp>
#include "roblox_network.h"
#include "roblox_terrain.h"
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>      // <-- INDISPENSABLE
#include <godot_cpp/classes/capsule_shape3d.hpp>   // <-- INDISPENSABLE
#include <godot_cpp/classes/resource_loader.hpp>   // malla de avatar por defecto
#include <godot_cpp/classes/resource_saver.hpp>    // guardar environment editable
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>      // personaje del usuario (.glb cara arreglada)
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/light3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/classes/procedural_sky_material.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include "roblox_player.h"
#include "humanoid.h"
#include "gl_avatar.h"
#include "roblox_part.h"
#include "roblox_services.h"
#include "roblox_interactive.h"

using namespace godot;

class RobloxWorkspace : public Node3D {
    GDCLASS(RobloxWorkspace, Node3D);

private:
    // Roblox Workspace properties
    float gravity                      = 196.2f;
    float fallen_parts_destroy_height  = -500.0f;
    bool  streaming_enabled            = false;
    float air_density                  = 0.0f;
    bool  touches_use_collision_groups = false;

    // ── Muerte por vacío (1.15) ──────────────────────────────────────────
    // Antes caer no hacía nada: el jugador atravesaba el suelo y seguía cayendo
    // para siempre. En Roblox FallenPartsDestroyHeight destruye lo que cae por
    // debajo (y mata al personaje cuyo HRP baja de ahí). Aquí, además, se puede
    // AUTODETECTAR: tomar la Y del objeto ANCLADO más bajo del mundo y restarle
    // un margen — así funciona en cualquier mapa sin calcular la altura a mano.
    // Configurable: apagar el auto y usar el límite fijo, o cambiar el margen.
    bool  auto_fall_height     = true;    // true = calcular desde el mapa
    float fall_height_margin   = 150.0f;  // se resta a la Y anclada más baja
    double _fall_scan_timer     = 0.0;    // recomputar el suelo cada ~1s, no por frame
    float  _fall_effective      = -500.0f; // umbral vigente (cache)

    void _apply_gravity() {
        if (!is_inside_tree()) return;
        Viewport* vp = get_viewport();
        if (!vp) return;
        Ref<World3D> world = vp->find_world_3d();
        if (!world.is_valid()) return;
        RID space = world->get_space();
        if (space.is_valid())
            PhysicsServer3D::get_singleton()->area_set_param(
                space, PhysicsServer3D::AREA_PARAM_GRAVITY, Variant(gravity));
    }

    // Generates the Roblox-style grid texture
    //// Genera la textura de cuadros tipo Roblox
    Ref<ImageTexture> generar_textura_grid() {
        Ref<Image> img = Image::create(64, 64, false, Image::FORMAT_RGBA8);
        Color color_fondo(0.64, 0.635, 0.647);  // gris piedra medio de Roblox (RGB 163,162,165)
        Color color_linea(0.5, 0.5, 0.52);       // lineas de stud, mas oscuras para que se vean
        
        img->fill(color_fondo);
        for (int i = 0; i < 64; i++) {
            img->set_pixel(i, 0, color_linea);
            img->set_pixel(0, i, color_linea);
        }
        return ImageTexture::create_from_image(img);
    }

    // Aplica el look de iluminacion estilo Roblox "Future" a un Environment + sol:
    // tonemapping filmico (antes era Linear plano), glow sutil (bloom real para
    // Neon y brillos), SSAO (profundidad barata), luz ambiental del cielo y
    // sombras suaves del sol (PCSS, Forward+). Idempotente: se llama al crear la
    // escena en el editor Y en runtime sobre un Environment ya existente, asi el
    // look mejora tambien en proyectos viejos sin recrear la escena.
    void _apply_roblox_render(const Ref<Environment>& env, DirectionalLight3D* sun) {
        if (env.is_valid()) {
            env->set_tonemapper(Environment::TONE_MAPPER_ACES);
            env->set_tonemap_white(6.0f);
            env->set_tonemap_exposure(1.0f);

            env->set_glow_enabled(true);
            env->set_glow_blend_mode(Environment::GLOW_BLEND_MODE_SCREEN);
            env->set_glow_intensity(0.5f);
            env->set_glow_strength(1.0f);
            env->set_glow_bloom(0.1f);
            env->set_glow_hdr_bleed_threshold(0.95f);
            env->set_glow_hdr_bleed_scale(2.0f);
            env->set_glow_level(2, 0.6f);
            env->set_glow_level(3, 0.4f);
            env->set_glow_level(4, 0.2f);

            env->set_ssao_enabled(true);
            env->set_ssao_radius(1.0f);
            env->set_ssao_intensity(1.5f);
            env->set_ssao_power(1.5f);

            env->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
            env->set_ambient_light_sky_contribution(1.0f);
            env->set_ambient_light_energy(1.0f);

            // Atmosfera sutil de distancia (como el "atmosphere" de Roblox):
            // niebla muy leve que da profundidad sin costar rendimiento.
            env->set_fog_enabled(true);
            env->set_fog_light_color(Color(0.76f, 0.85f, 0.94f));
            env->set_fog_density(0.0006f);
            env->set_fog_sky_affect(0.0f);
        }
        if (sun) {
            sun->set_shadow(true);
            sun->set_param(Light3D::PARAM_ENERGY, 1.0f);
            // El resto de params de sombra (tamaño, bias, filtro) los fija
            // gl_apply_graphics_quality según el nivel — un solo sitio (1.15).
        }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("build_local_character"),                &RobloxWorkspace::build_local_character);   // LoadCharacter (1.14.10)
        ClassDB::bind_method(D_METHOD("_gl_apply_quality_deferred"),           &RobloxWorkspace::_gl_apply_quality_deferred);
        ClassDB::bind_method(D_METHOD("set_gravity","g"),                      &RobloxWorkspace::set_gravity);
        ClassDB::bind_method(D_METHOD("get_gravity"),                          &RobloxWorkspace::get_gravity);
        ClassDB::bind_method(D_METHOD("set_fallen_parts_destroy_height","h"),  &RobloxWorkspace::set_fallen_parts_destroy_height);
        ClassDB::bind_method(D_METHOD("get_fallen_parts_destroy_height"),      &RobloxWorkspace::get_fallen_parts_destroy_height);
        ClassDB::bind_method(D_METHOD("set_auto_fall_height","b"),             &RobloxWorkspace::set_auto_fall_height);
        ClassDB::bind_method(D_METHOD("get_auto_fall_height"),                 &RobloxWorkspace::get_auto_fall_height);
        ClassDB::bind_method(D_METHOD("set_fall_height_margin","m"),           &RobloxWorkspace::set_fall_height_margin);
        ClassDB::bind_method(D_METHOD("get_fall_height_margin"),               &RobloxWorkspace::get_fall_height_margin);
        ClassDB::bind_method(D_METHOD("set_streaming_enabled","b"),            &RobloxWorkspace::set_streaming_enabled);
        ClassDB::bind_method(D_METHOD("get_streaming_enabled"),                &RobloxWorkspace::get_streaming_enabled);
        ClassDB::bind_method(D_METHOD("set_air_density","d"),                  &RobloxWorkspace::set_air_density);
        ClassDB::bind_method(D_METHOD("get_air_density"),                      &RobloxWorkspace::get_air_density);
        ClassDB::bind_method(D_METHOD("set_touches_use_collision_groups","b"), &RobloxWorkspace::set_touches_use_collision_groups);
        ClassDB::bind_method(D_METHOD("get_touches_use_collision_groups"),     &RobloxWorkspace::get_touches_use_collision_groups);

        ADD_GROUP("Mundo","");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"Gravity",PROPERTY_HINT_RANGE,"0,1000,0.1"),
            "set_gravity","get_gravity");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"FallenPartsDestroyHeight",PROPERTY_HINT_RANGE,"-10000,0,1"),
            "set_fallen_parts_destroy_height","get_fallen_parts_destroy_height");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"AutoFallHeight"),
            "set_auto_fall_height","get_auto_fall_height");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"FallHeightMargin",PROPERTY_HINT_RANGE,"0,5000,1"),
            "set_fall_height_margin","get_fall_height_margin");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"StreamingEnabled"),
            "set_streaming_enabled","get_streaming_enabled");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"AirDensity",PROPERTY_HINT_RANGE,"0,10,0.01"),
            "set_air_density","get_air_density");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL,"TouchesUseCollisionGroups"),
            "set_touches_use_collision_groups","get_touches_use_collision_groups");
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE && Engine::get_singleton()->is_editor_hint()) {
            if (get_child_count() == 0) {
                Node* root = get_tree()->get_edited_scene_root();
                if (!root) root = this;

                // 1. ROBLOX BLUE SKY
                //// 1. CIELO AZUL ROBLOX
                WorldEnvironment* env_node = memnew(WorldEnvironment);
                env_node->set_name("WorldEnvironment");
                Ref<Environment> env; env.instantiate();
                Ref<Sky> sky; sky.instantiate();
                Ref<ProceduralSkyMaterial> sky_mat; sky_mat.instantiate();
                
                // Colores del cielo clasico de Roblox (azul suave, no tan saturado):
                // top RGB(78,150,205), horizonte RGB(170,205,230), suelo RGB(135,140,150)
                sky_mat->set_sky_top_color(Color(0.306, 0.588, 0.804));
                sky_mat->set_sky_horizon_color(Color(0.667, 0.804, 0.902));
                sky_mat->set_ground_bottom_color(Color(0.529, 0.549, 0.588));
                sky_mat->set_ground_horizon_color(Color(0.667, 0.804, 0.902));

                sky->set_material(sky_mat);
                env->set_sky(sky);
                env->set_background(Environment::BG_SKY);
                env->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
                // Mas relleno ambiental para que los lados en sombra (p.ej. el
                // personaje) no salgan casi negros.
                env->set_ambient_light_energy(1.15);

                // 2. SUN
                //// 2. SOL
                DirectionalLight3D* sol = memnew(DirectionalLight3D);
                sol->set_name("SunLight");
                sol->set_rotation_degrees(Vector3(-45, 45, 0));
                sol->set_shadow(true);
                add_child(sol);
                sol->set_owner(root);

                // Look de iluminacion estilo Roblox "Future" sobre el entorno + sol
                _apply_roblox_render(env, sol);

                // El Environment vive como ARCHIVO EDITABLE en GodotLuau/shaders/:
                // si ya existe se usa ese (respetando los cambios del usuario);
                // si no, se guarda el recien creado para que pueda modificarlo.
                {
                    const String env_path = "res://GodotLuau/shaders/environment_roblox.tres";
                    ResourceLoader* rload = ResourceLoader::get_singleton();
                    if (rload && rload->exists(env_path)) {
                        Ref<Environment> user_env = rload->load(env_path);
                        if (user_env.is_valid()) env = user_env;
                    } else {
                        Ref<DirAccess> d = DirAccess::open("res://");
                        if (d.is_valid() && !d->dir_exists("GodotLuau/shaders"))
                            d->make_dir_recursive("GodotLuau/shaders");
                        ResourceSaver::get_singleton()->save(env, env_path);
                    }
                }
                env_node->set_environment(env);
                add_child(env_node);
                env_node->set_owner(root);

                // 3. BASEPLATE — una RobloxPart de verdad, COMO EN ROBLOX:
                // seleccionas "Baseplate" y cambias su propiedad Color en el
                // inspector; la textura de cuadricula (archivo editable en
                // GodotLuau/assets/textures/) se tiñe con ese color.
                RobloxPart* bp = memnew(RobloxPart);
                bp->set_name("Baseplate");
                add_child(bp);            // ENTER_TREE crea su material interno
                bp->set_owner(root);
                bp->set_size(Vector3(1000, 1, 1000));
                bp->set_anchored(true);
                bp->set_color(Color(0.42f, 0.42f, 0.45f));   // gris Studio (cambiable)
                {
                    ResourceLoader* rload = ResourceLoader::get_singleton();
                    const String tex_path = "res://GodotLuau/assets/textures/baseplate_grid.png";
                    Ref<Texture2D> grid_tex;
                    if (rload && rload->exists(tex_path)) grid_tex = rload->load(tex_path);
                    if (grid_tex.is_null()) grid_tex = generar_textura_grid();
                    bp->gl_apply_grid_texture(grid_tex, 125.0f);   // celda menor = 2 studs
                }

                // 3b. TERRAIN — Workspace.Terrain (como Roblox): vacío al inicio,
                // se llena por código con Terrain:FillBlock/FillBall (p.ej. MundoVoxel).
                {
                    RobloxTerrain* terr = memnew(RobloxTerrain);
                    terr->set_name("Terrain");
                    add_child(terr);
                    terr->set_owner(root);
                }

                // 4. CURRENT CAMERA — visible in the scene tree like Roblox
                // workspace.CurrentCamera is the camera that renders the game.
                // At runtime the player controls it; in the editor you can see its position.
                //// 4. CÁMARA ACTUAL — visible en el árbol de escena como en Roblox
                //// workspace.CurrentCamera es la cámara que renderiza el juego.
                //// En runtime el jugador la controla; en el editor puedes ver su posición.
                Camera3D* current_cam = memnew(Camera3D);
                current_cam->set_name("CurrentCamera");
                current_cam->set_position(Vector3(0.0f, 5.0f, 10.0f));
                current_cam->set_rotation_degrees(Vector3(-15.0f, 0.0f, 0.0f));
                add_child(current_cam);
                current_cam->set_owner(root);

                GL_DEBUG_PRINT("[GodotLuau] RobloxWorkspace initialized in editor.");
            }
        }
    }

public:
    // ── Getters / Setters (properties match Roblox Workspace API) ─
    void  set_gravity(float g)                     { gravity = Math::max(g, 0.0f); _apply_gravity(); }
    float get_gravity() const                      { return gravity; }
    // Fijar la altura a mano = quiero un límite FIJO (apaga el auto), como pedir
    // explícitamente ese valor. Durante la carga de escena el nodo aún no está
    // "ready" y solo se restaura el valor guardado (sin intención del usuario),
    // así que ahí NO se toca el auto; tras ready, un setter sí es intención.
    void  set_fallen_parts_destroy_height(float h) {
        fallen_parts_destroy_height = h;
        _fall_effective = h;
        if (is_node_ready()) auto_fall_height = false;
    }
    float get_fallen_parts_destroy_height() const  { return fallen_parts_destroy_height; }
    void  set_auto_fall_height(bool b)   { auto_fall_height = b; }
    bool  get_auto_fall_height() const   { return auto_fall_height; }
    // Umbral VIGENTE (lo que ve un script): en auto es el calculado del mapa, no
    // el -500 guardado. El getter de arriba se queda con el valor guardado para
    // que la escena conserve el ajuste fijo del usuario.
    float get_effective_fall_height() const { return auto_fall_height ? _fall_effective : fallen_parts_destroy_height; }
    void  set_fall_height_margin(float m){ fall_height_margin = Math::max(m, 0.0f); }
    float get_fall_height_margin() const { return fall_height_margin; }
    void  set_streaming_enabled(bool b)            { streaming_enabled = b; }
    bool  get_streaming_enabled() const            { return streaming_enabled; }
    void  set_air_density(float d)                 { air_density = Math::max(d, 0.0f); }
    float get_air_density() const                  { return air_density; }
    void  set_touches_use_collision_groups(bool b) { touches_use_collision_groups = b; }
    bool  get_touches_use_collision_groups() const { return touches_use_collision_groups; }

    SpawnLocation* _find_spawn_r(Node* n) {
        if (!n) return nullptr;
        if (SpawnLocation* s = Object::cast_to<SpawnLocation>(n)) return s;
        for (int i = 0; i < n->get_child_count(); i++)
            if (SpawnLocation* f = _find_spawn_r(n->get_child(i))) return f;
        return nullptr;
    }

    void _collect_spawns_r(Node* n, std::vector<SpawnLocation*>& out) {
        if (!n) return;
        if (SpawnLocation* s = Object::cast_to<SpawnLocation>(n)) { out.push_back(s); return; }
        for (int i = 0; i < n->get_child_count(); i++) _collect_spawns_r(n->get_child(i), out);
    }

    // Elige el spawn del jugador según su equipo (1.15). Antes se cogía SIEMPRE
    // el primer SpawnLocation del árbol, así que en un juego por equipos todos
    // aparecían en el mismo sitio y había que recolocarlos a mano. Regla de
    // Roblox: un spawn con Neutral=true acepta a cualquiera; con Neutral=false
    // solo a quien tenga su TeamColor. Entre los válidos se elige al azar.
    SpawnLocation* _pick_spawn_for_local_player() {
        std::vector<SpawnLocation*> all;
        _collect_spawns_r(this, all);
        if (all.empty()) return nullptr;

        String team_color;
        if (Node* dm = get_parent()) {
            if (Node* players = dm->get_node_or_null("Players")) {
                for (int i = 0; i < players->get_child_count(); i++) {
                    Node* po = players->get_child(i);
                    if (!po || !po->is_class("PlayerObject")) continue;
                    if (!po->has_method("_gl_is_local") || !(bool)po->call("_gl_is_local")) continue;
                    team_color = String(po->call("_gl_team_color_name"));
                    break;
                }
            }
        }

        std::vector<SpawnLocation*> ok;
        for (SpawnLocation* s : all) {
            if (!s->get_enabled()) continue;
            if (s->get_neutral() || (!team_color.is_empty() && s->get_team_color() == team_color))
                ok.push_back(s);
        }
        // Sin spawn válido para su equipo: mejor aparecer en cualquiera que caer
        // al vacío en (0,5,0) sin explicación.
        std::vector<SpawnLocation*>& pool = ok.empty() ? all : ok;
        return pool[(int)(UtilityFunctions::randi() % pool.size())];
    }

    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;

        _apply_gravity();

        // Aplicar el look de iluminacion Roblox al entorno YA existente: las
        // escenas viejas no se recrean (su WorldEnvironment se guardo con los
        // ajustes antiguos), pero al jugar deben verse con tonemap/glow/SSAO/
        // sombras suaves igual. Idempotente.
        {
            WorldEnvironment* we = nullptr;
            DirectionalLight3D* sun = nullptr;
            for (int i = 0; i < get_child_count(); i++) {
                Node* c = get_child(i);
                if (!we)  we  = Object::cast_to<WorldEnvironment>(c);
                if (!sun) sun = Object::cast_to<DirectionalLight3D>(c);
            }
            Ref<Environment> env = we ? we->get_environment() : Ref<Environment>();
            // Si el usuario tiene su environment editable en GodotLuau/shaders/,
            // usarlo en TODAS las escenas (viejas incluidas).
            ResourceLoader* rload = ResourceLoader::get_singleton();
            const String env_path = "res://GodotLuau/shaders/environment_roblox.tres";
            if (we && rload && rload->exists(env_path)) {
                Ref<Environment> user_env = rload->load(env_path);
                if (user_env.is_valid()) { we->set_environment(user_env); env = user_env; }
                if (sun) { sun->set_shadow(true); }
            } else if (env.is_valid() || sun) {
                _apply_roblox_render(env, sun);
                // Primera vez que se juega: dejar el environment como archivo
                // editable (solo corriendo desde el editor; un export no escribe res://)
                if (env.is_valid() && OS::get_singleton()->has_feature("editor")) {
                    Ref<DirAccess> d = DirAccess::open("res://");
                    if (d.is_valid() && !d->dir_exists("GodotLuau/shaders"))
                        d->make_dir_recursive("GodotLuau/shaders");
                    ResourceSaver::get_singleton()->save(env, env_path);
                }
            }
        }

        // ── Terrain: asegurar que exista (escenas viejas sin Terrain guardado) ──
        if (!get_node_or_null("Terrain")) {
            RobloxTerrain* terr = memnew(RobloxTerrain);
            terr->set_name("Terrain");
            call_deferred("add_child", terr);   // el padre está montando hijos en _ready
        }

        // ── RunService oculto: como en Roblox, existe pero no se ve en el editor ──
        if (get_parent() && !get_parent()->get_node_or_null("RunService")) {
            Node* rs = Object::cast_to<Node>(ClassDB::instantiate(StringName("RunService")));
            // add_child diferido: en _ready el padre (game) aun esta montando sus
            // hijos y Godot rechaza un add_child directo ("Parent node is busy").
            // El deferred corre antes que los scripts (iniciar_corrutina tambien
            // es diferido y se encola despues), asi RunService existe a tiempo.
            if (rs) { rs->set_name("RunService"); get_parent()->call_deferred("add_child", rs); }
        }

        // ── NetworkService: SOLO se auto-crea si el juego se lanzó desde la barra
        //    "Jugadores" del editor (multijugador local). Los juegos normales no
        //    reciben un servicio de red que no pidieron. ──
        if (gl_mp_autostart_requested() && get_parent() && !get_parent()->get_node_or_null("NetworkService")) {
            Node* ns = Object::cast_to<Node>(ClassDB::instantiate(StringName("NetworkService")));
            if (ns) { ns->set_name("NetworkService"); get_parent()->call_deferred("add_child", ns); }
        }

        // ── Menú de ajustes (Esc), como el de Roblox: existe en TODO juego ──
        if (get_parent() && !get_parent()->get_node_or_null("SettingsMenu")) {
            Node* sm = Object::cast_to<Node>(ClassDB::instantiate(StringName("GLSettingsMenu")));
            if (sm) { sm->set_name("SettingsMenu"); get_parent()->call_deferred("add_child", sm); }
        }

        build_local_character();
        set_process(true);   // muerte por vacío (1.15)

        // Calidad gráfica (1.15): aplica sombras suaves REALES, bias anti-acne,
        // SSAO/glow y escala según el nivel guardado. Diferido para que el sol y
        // el WorldEnvironment ya estén en el árbol. Esto arregla las sombras feas.
        call_deferred("_gl_apply_quality_deferred");
    }

    void _gl_apply_quality_deferred() {
        gl_apply_graphics_quality(gl_graphics_level(), this);
    }

    // ── Muerte por vacío (1.15) ──────────────────────────────────────────
    void _process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint()) return;

        // Recalcular el umbral cada ~1s (no por frame: escanear el mapa entero
        // sale caro y el suelo casi nunca cambia).
        _fall_scan_timer -= delta;
        if (_fall_scan_timer <= 0.0) {
            _fall_scan_timer = 1.0;
            if (auto_fall_height) {
                bool found = false;
                float lowest = 0.0f;
                _scan_lowest_anchored(this, lowest, found);
                // Sin geometría anclada (mapa solo de terreno o dinámico): mantener
                // el fijo por defecto, para no matar a todos en +infinito.
                _fall_effective = found ? (lowest - fall_height_margin) : fallen_parts_destroy_height;
            } else {
                _fall_effective = fallen_parts_destroy_height;
            }
        }

        // Matar al personaje LOCAL si su HRP cae por debajo. Los muñecos remotos
        // no se tocan aquí (los mata su propia máquina). El auto-respawn (F1.2)
        // lo revive tras RespawnTime.
        for (int i = 0; i < get_child_count(); i++) {
            Node* ch = get_child(i);
            if (!ch || !(ch->is_class("RobloxPlayer") || ch->is_class("RobloxPlayer2D"))) continue;
            Node3D* ref = Object::cast_to<Node3D>(ch->get_node_or_null(NodePath("HumanoidRootPart")));
            Node3D* body = ref ? ref : Object::cast_to<Node3D>(ch);
            if (!body) continue;
            if (body->get_global_position().y >= _fall_effective) continue;
            Node* hum = ch->get_node_or_null(NodePath("Humanoid"));
            if (hum && hum->has_method("get_health") && (double)hum->call("get_health") > 0.0)
                hum->call("set_health", 0.0);
        }

        // Destruir Parts sueltas (no ancladas, fuera de un personaje) que caen por
        // debajo — es lo que dice el nombre FallenPartsDestroyHeight en Roblox.
        _destroy_fallen_parts(this);
    }

private:
    void _scan_lowest_anchored(Node* n, float& lowest, bool& found) {
        if (!n) return;
        if (n->is_class("RobloxPart")) {
            Variant anc = n->get("Anchored");
            if (anc.get_type() == Variant::BOOL && (bool)anc) {
                if (Node3D* p = Object::cast_to<Node3D>(n)) {
                    float y = p->get_global_position().y;
                    if (!found || y < lowest) { lowest = y; found = true; }
                }
            }
        }
        for (int i = 0; i < n->get_child_count(); i++) _scan_lowest_anchored(n->get_child(i), lowest, found);
    }

    // ¿este nodo está dentro de un personaje? (tiene un Humanoid por ancestro)
    static bool _under_character(Node* n) {
        for (Node* a = n ? n->get_parent() : nullptr; a; a = a->get_parent())
            if (a->get_node_or_null(NodePath("Humanoid"))) return true;
        return false;
    }
    void _destroy_fallen_parts(Node* n) {
        if (!n) return;
        // Copiar la lista: se van a liberar hijos mientras se itera.
        std::vector<Node*> kids;
        for (int i = 0; i < n->get_child_count(); i++) kids.push_back(n->get_child(i));
        for (Node* c : kids) {
            if (!c) continue;
            if (c->is_class("RobloxPart")) {
                Variant anc = c->get("Anchored");
                bool anchored = anc.get_type() == Variant::BOOL && (bool)anc;
                Node3D* p = Object::cast_to<Node3D>(c);
                if (!anchored && p && !_under_character(c) &&
                    p->get_global_position().y < _fall_effective) {
                    c->queue_free();
                    continue;
                }
            }
            _destroy_fallen_parts(c);
        }
    }

public:
    // Construye el personaje local (spawn + rig + Humanoid + scripts). Extraído
    // de _ready para que Player:LoadCharacter() pueda rehacerlo (1.14.10).
    void build_local_character() {
        // ── 1. Find StarterPlayer (sibling of Workspace in the scene) ──────────
        //// ── 1. Buscar StarterPlayer (hermano del Workspace en la escena) ────────
        Node* starter_player = nullptr;
        if (get_parent()) {
            starter_player = get_parent()->get_node_or_null("StarterPlayer");
        }

        // ── 2. Create the player character ──────────────────────────────────────
        //// ── 2. Crear el personaje del jugador ────────────────────────────────────
        RobloxPlayer* p = memnew(RobloxPlayer);
        p->set_name("LocalPlayer");
        // Spawnear en un SpawnLocation acorde a su equipo (como Roblox)
        Vector3 spawn_pos(0, 5, 0);
        if (SpawnLocation* sl = _pick_spawn_for_local_player()) spawn_pos = sl->get_spawn_position();
        p->set_position(spawn_pos);
        add_child(p);

        // ── 3. Personaje por defecto (como Roblox) ──────────────────────
        // Prioridad: StarterPlayer/StarterCharacter (clonado, como Roblox) →
        // avatar R6 por partes con animaciones clasicas → capsula gris.
        // La FISICA siempre es la capsula.
        {
            Node3D* character = gl_build_character(this);
            if (character) {
                p->add_child(character);
                // Aplanar el rig R6 propio: partes + HumanoidRootPart pasan a ser
                // hijos DIRECTOS del personaje (como el Model plano de Roblox), así
                // Character:FindFirstChild("HumanoidRootPart") resuelve.
                gl_flatten_r6_character(p, character);
            } else {
                MeshInstance3D* m = memnew(MeshInstance3D);
                m->set_name("Mesh");
                Ref<CapsuleMesh> cm; cm.instantiate();
                m->set_mesh(cm);
                Ref<StandardMaterial3D> mat; mat.instantiate();
                mat->set_albedo(Color(0.55, 0.55, 0.55));
                mat->set_roughness(0.95);
                m->set_material_override(mat);
                p->add_child(m);
            }

            CollisionShape3D* c = memnew(CollisionShape3D);
            Ref<CapsuleShape3D> cs; cs.instantiate();
            c->set_shape(cs);
            p->add_child(c);
        }

        // ── 4. Always ensure a Humanoid exists (movement) ───────────────────────
        //// ── 4. Siempre garantizar Humanoid (movimiento) ──────────────────────────
        if (!p->get_node_or_null("Humanoid")) {
            Humanoid* hum = memnew(Humanoid);
            hum->set_name("Humanoid");
            p->add_child(hum);
        }

        // ── 5. Clone StarterCharacterScripts onto the character ─────────────────
        //// ── 5. Clonar StarterCharacterScripts al personaje ───────────────────────
        if (starter_player) {
            Node* char_scripts = starter_player->get_node_or_null("StarterCharacterScripts");
            if (char_scripts) {
                TypedArray<Node> hijos = char_scripts->get_children();
                for (int i = 0; i < hijos.size(); i++) {
                    Node* hijo = Object::cast_to<Node>(hijos[i]);
                    if (hijo) {
                        p->add_child(hijo->duplicate());
                    }
                }
            }

            // ── 7. Clone StarterPlayerScripts onto the player ───────────────────
            //// ── 7. Clonar StarterPlayerScripts al jugador ────────────────────────
            Node* player_scripts = starter_player->get_node_or_null("StarterPlayerScripts");
            if (player_scripts) {
                TypedArray<Node> hijos = player_scripts->get_children();
                for (int i = 0; i < hijos.size(); i++) {
                    Node* hijo = Object::cast_to<Node>(hijos[i]);
                    if (hijo) {
                        p->add_child(hijo->duplicate());
                    }
                }
            }

            GL_DEBUG_PRINT("[GodotLuau] Player scripts loaded from StarterPlayer.");
        }
    }
};

#endif