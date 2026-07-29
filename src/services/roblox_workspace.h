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
#include "gl_chunks.h"

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
    // 0 = automatico (30 Hz en mapas grandes, 60 en pequeños); >0 = fijo
    int   physics_fps          = 0;
    bool  auto_fall_height     = true;    // true = calcular desde el mapa
    float fall_height_margin   = 150.0f;  // se resta a la Y anclada más baja
    double _fall_scan_timer     = 0.0;    // recomputar el suelo cada ~1s, no por frame
    bool   _ground_cached       = false;  // el suelo se calcula una vez
    float  _ground_value        = -500.0f;
    double _fall_destroy_timer  = 0.0;    // barrer partes caidas cada ~0.3s, no por frame
    float  _fall_effective      = -500.0f; // umbral vigente (cache)

    void _apply_gravity() {
        if (!is_inside_tree()) return;
        Viewport* vp = get_viewport();
        if (!vp) return;
        Ref<World3D> world = vp->find_world_3d();
        if (!world.is_valid()) return;
        RID space = world->get_space();
        // El servidor de fisica puede estar ya destruido (al cerrar el juego):
        // sin esta comprobacion salian miles de errores "PhysicsServer3D
        // get_singleton() is null" en la consola durante el apagado.
        PhysicsServer3D* ps = PhysicsServer3D::get_singleton();
        if (space.is_valid() && ps)
            ps->area_set_param(space, PhysicsServer3D::AREA_PARAM_GRAVITY, Variant(gravity));
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

    // Crea el material de cielo azul clasico de Roblox (azul suave, no saturado).
    // Se usa para entornos nuevos y para REPARAR uno cuyo cielo se perdio.
    Ref<Sky> _gl_make_roblox_sky() {
        Ref<Sky> sky; sky.instantiate();
        Ref<ProceduralSkyMaterial> m; m.instantiate();
        m->set_sky_top_color(Color(0.306, 0.588, 0.804));
        m->set_sky_horizon_color(Color(0.667, 0.804, 0.902));
        m->set_ground_bottom_color(Color(0.529, 0.549, 0.588));
        m->set_ground_horizon_color(Color(0.667, 0.804, 0.902));
        sky->set_material(m);
        return sky;
    }

    // Aplica el look de iluminacion estilo Roblox "Future" a un Environment + sol:
    // tonemapping filmico (antes era Linear plano), glow sutil (bloom real para
    // Neon y brillos), SSAO (profundidad barata), luz ambiental del cielo y
    // sombras suaves del sol (PCSS, Forward+). Idempotente: se llama al crear la
    // escena en el editor Y en runtime sobre un Environment ya existente, asi el
    // look mejora tambien en proyectos viejos sin recrear la escena.
    //
    // ── ANTIGRIS (1.16.6) ────────────────────────────────────────────────
    // El bug de "todo gris" al crear un Game 3D era un Environment guardado con
    // el AJUSTE de color a saturacion 0.2 (grisaceo) + niebla densa, que se
    // recargaba desde el .tres "editable" en cada escena nueva. Ahora esta
    // funcion GARANTIZA que jamas pueda salir gris: apaga el adjustment, fuerza
    // el fondo a CIELO valido y deja la niebla suave. Como se llama sobre CADA
    // entorno (nuevo o existente), repara tambien escenas ya hechas.
    static bool _gl_find_class_in(Node* n, const char* cls) {
        if (!n) return false;
        if (n->is_class(cls)) return true;
        for (int i = 0; i < n->get_child_count(); i++)
            if (_gl_find_class_in(n->get_child(i), cls)) return true;
        return false;
    }
    // ¿Hay un efecto de Lighting que gobierne esta caracteristica del Environment?
    // Si lo hay, el Workspace NO debe forzarla: asi un BloomEffect/Atmosphere con
    // Enabled=false se queda apagado en vez de reactivarse cada vez que se entra a
    // la escena o al jugar. (glow/fog encendidos NO pueden dar gris, asi que aqui
    // si es seguro "respetar" el efecto — al reves del adjustment, que siempre off.)
    bool _gl_lighting_has(const char* cls) const {
        Node* dm = get_parent();
        if (!dm) return false;
        Node* lighting = dm->get_node_or_null(NodePath("Lighting"));
        return _gl_find_class_in(lighting ? lighting : dm, cls);
    }
    // ¿Existe el servicio Lighting? Si existe, ES EL quien manda sobre el ambiente
    // y la niebla (como en Roblox): el Workspace solo pone el look base cuando no
    // hay Lighting, para no pisar Ambient/OutdoorAmbient/Fog del juego.
    bool _gl_has_lighting() const {
        Node* dm = get_parent();
        if (!dm) return false;
        Node* l = dm->get_node_or_null(NodePath("Lighting"));
        return l && l->is_class("Lighting");
    }

    void _apply_roblox_render(const Ref<Environment>& env, DirectionalLight3D* sun) {
        if (env.is_valid()) {
            // 1) NUNCA gris: el ajuste de color (saturacion baja) es lo que dejaba
            //    todo gris, ya venga del .tres podrido o de un ColorCorrectionEffect
            //    importado. Se apaga SIEMPRE. No merece la pena "respetar" un
            //    ColorCorrection si el precio es que el mundo salga gris.
            env->set_adjustment_enabled(false);
            // 2) Fondo = cielo SIEMPRE. Un BG_COLOR/CANVAS dejaba pantalla plana.
            if (env->get_background() != Environment::BG_SKY)
                env->set_background(Environment::BG_SKY);
            Ref<Sky> sky = env->get_sky();
            if (!sky.is_valid() || !sky->get_material().is_valid())
                env->set_sky(_gl_make_roblox_sky());

            env->set_tonemapper(Environment::TONE_MAPPER_ACES);
            env->set_tonemap_white(6.0f);
            env->set_tonemap_exposure(1.0f);

            // Glow: solo se fuerza si NO hay Bloom/SunRays que lo controle (asi un
            // Bloom con Enabled=false no se reactiva).
            if (!_gl_lighting_has("BloomEffect") && !_gl_lighting_has("SunRaysNode")) {
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
            }

            env->set_ssao_enabled(true);
            env->set_ssao_radius(1.0f);
            env->set_ssao_intensity(1.5f);
            env->set_ssao_power(1.5f);

            // Ambiente: solo lo fija el Workspace si NO hay servicio Lighting.
            // Con Lighting presente manda el (Ambient/OutdoorAmbient de Roblox).
            if (!_gl_has_lighting()) {
                env->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
                env->set_ambient_light_sky_contribution(1.0f);
                env->set_ambient_light_energy(1.0f);
            }

            // Atmosfera sutil de distancia (niebla muy leve): solo se fuerza si NO
            // hay un Atmosphere ni un Lighting que la controlen (asi un Atmosphere
            // apagado no revive y la niebla de Roblox no se pisa).
            if (!_gl_lighting_has("AtmosphereNode") && !_gl_has_lighting()) {
                env->set_fog_enabled(true);
                env->set_fog_light_color(Color(0.76f, 0.85f, 0.94f));
                env->set_fog_density(0.0006f);
                env->set_fog_sky_affect(0.0f);
            }
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
        ClassDB::bind_method(D_METHOD("_gl_tune_for_map_size"),                &RobloxWorkspace::_gl_tune_for_map_size);
        ClassDB::bind_method(D_METHOD("_gl_rebuild_chunks"),                   &RobloxWorkspace::_gl_rebuild_chunks);
        ClassDB::bind_method(D_METHOD("RebuildStaticChunks"),                  &RobloxWorkspace::_gl_rebuild_chunks);
        ClassDB::bind_method(D_METHOD("set_graphics_quality","level"),         &RobloxWorkspace::set_graphics_quality);
        ClassDB::bind_method(D_METHOD("get_graphics_quality"),                 &RobloxWorkspace::get_graphics_quality);
        ClassDB::bind_method(D_METHOD("SetGraphicsMode","mode"),               &RobloxWorkspace::set_graphics_mode);
        ClassDB::bind_method(D_METHOD("GetGraphicsMode"),                      &RobloxWorkspace::get_graphics_mode);
        ClassDB::bind_method(D_METHOD("SetAutoTargetFPS","fps"),               &RobloxWorkspace::set_auto_target_fps);
        ClassDB::bind_method(D_METHOD("GetAutoTargetFPS"),                     &RobloxWorkspace::get_auto_target_fps);
        ClassDB::bind_method(D_METHOD("SetGraphicsSetting","name","value"),    &RobloxWorkspace::set_custom_setting);
        ClassDB::bind_method(D_METHOD("GetGraphicsSetting","name"),            &RobloxWorkspace::get_custom_setting);
        // Rehace las piezas del mundo que falten (cielo, sol, camara, terrain).
        // La llama el importador de .rbxl despues de llenar el Workspace.
        ClassDB::bind_method(D_METHOD("EnsureEnvironment"),                    &RobloxWorkspace::gl_ensure_environment);
        ClassDB::bind_method(D_METHOD("set_physics_fps","v"),                  &RobloxWorkspace::set_physics_fps);
        ClassDB::bind_method(D_METHOD("get_physics_fps"),                      &RobloxWorkspace::get_physics_fps);
        ADD_PROPERTY(PropertyInfo(Variant::INT,"PhysicsFPS",PROPERTY_HINT_RANGE,"0,240,1"),
            "set_physics_fps","get_physics_fps");
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

    // Busca un hijo directo del tipo pedido. Se comprueba por TIPO y no por
    // "¿tiene hijos?" porque un Workspace que llega poblado (por ejemplo desde
    // el importador de .rbxl) tambien necesita cielo, sol y camara: con la
    // comprobacion antigua se quedaba sin Environment y la escena salia GRIS.
    template <class T>
    T* _gl_find_child_of_type() const {
        for (int i = 0; i < get_child_count(); i++)
            if (T* t = Object::cast_to<T>(get_child(i))) return t;
        return nullptr;
    }

    void _notification(int p_what) {
        if (p_what == NOTIFICATION_ENTER_TREE && Engine::get_singleton()->is_editor_hint()) {
            gl_ensure_environment();
        }
    }

public:
    // Crea las piezas del mundo que falten (cielo, sol, baseplate, terrain,
    // camara). Es IDEMPOTENTE: cada pieza se crea solo si no hay ya una de su
    // tipo, asi que se puede llamar sobre un Workspace vacio o sobre uno recien
    // rellenado por el importador.
    void gl_ensure_environment() {
        // No exige estar dentro del arbol: el importador puede preparar el
        // Workspace antes de colgarlo. Solo el owner necesita la escena.
        Node* root = nullptr;
        if (is_inside_tree() && get_tree()) root = get_tree()->get_edited_scene_root();
        if (!root) root = this;

        WorldEnvironment*   we  = _gl_find_child_of_type<WorldEnvironment>();
        DirectionalLight3D*  sol = _gl_find_child_of_type<DirectionalLight3D>();

        // ── REPARAR el entorno existente SIEMPRE (antigris 1.16.6) ──────────
        // Antes esto solo corria si faltaba alguna pieza; una escena YA COMPLETA
        // (Workspace importado o game.tscn viejo) con el Environment gris salia
        // por el return de abajo sin tocarse y se quedaba gris. Ahora, si hay un
        // WorldEnvironment, se le aplica el look garantizado-no-gris antes de
        // cualquier atajo. Si su Environment vive en un .tres (el "editable"
        // rotado) y estaba gris, se reescribe reparado para que no vuelva nunca.
        if (we) {
            Ref<Environment> env = we->get_environment();
            if (!env.is_valid()) {
                env.instantiate();
                env->set_sky(_gl_make_roblox_sky());
                env->set_background(Environment::BG_SKY);
                we->set_environment(env);
            }
            Ref<Sky> sky_cur = env->get_sky();
            const bool was_gray = env->is_adjustment_enabled()
                               || env->get_background() != Environment::BG_SKY
                               || !sky_cur.is_valid()
                               || (env->is_fog_enabled() && env->get_fog_density() > 0.01f);
            env->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
            env->set_ambient_light_energy(1.15);
            _apply_roblox_render(env, sol);
            if (was_gray && Engine::get_singleton()->is_editor_hint()) {
                const String rp = env->get_path();
                if (!rp.is_empty())
                    ResourceSaver::get_singleton()->save(env, rp);
            }
        }

        const bool need_env  = (we  == nullptr);
        const bool need_sun  = (sol == nullptr);
        const bool need_cam  = _gl_find_child_of_type<Camera3D>() == nullptr;
        const bool need_terr = get_node_or_null(NodePath("Terrain")) == nullptr;
        // El Baseplate solo se crea en un Workspace VACIO: si ya hay geometria
        // (mapa importado) una placa de 1000x1000 studs estorbaria.
        const bool need_base = (get_child_count() == 0);

        if (!need_env && !need_sun && !need_cam && !need_terr && !need_base) return;

        // 2. SOL (se crea antes que el entorno para que el look se aplique a ambos)
        if (need_sun) {
            sol = memnew(DirectionalLight3D);
            sol->set_name("SunLight");
            sol->set_rotation_degrees(Vector3(-45, 45, 0));
            sol->set_shadow(true);
            add_child(sol);
            sol->set_owner(root);
        }

        // 1. CIELO AZUL ROBLOX + ENVIRONMENT — embebido en la escena (no un .tres
        // compartido que se corrompia y volvia todo gris). Editable inline en el
        // inspector del WorldEnvironment.
        if (need_env) {
            Ref<Environment> env; env.instantiate();
            env->set_sky(_gl_make_roblox_sky());
            env->set_background(Environment::BG_SKY);
            env->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
            env->set_ambient_light_energy(1.15);
            _apply_roblox_render(env, sol);

            WorldEnvironment* env_node = memnew(WorldEnvironment);
            env_node->set_name("WorldEnvironment");
            env_node->set_environment(env);
            add_child(env_node);
            env_node->set_owner(root);
        }

        // 3. BASEPLATE — una RobloxPart de verdad, COMO EN ROBLOX: seleccionas
        // "Baseplate" y cambias su propiedad Color en el inspector; la textura de
        // cuadricula (archivo editable en GodotLuau/assets/textures/) se tiñe con
        // ese color. Solo en un mundo vacio: sobre un mapa importado estorbaria.
        if (need_base) {
            RobloxPart* bp = memnew(RobloxPart);
            bp->set_name("Baseplate");
            add_child(bp);            // ENTER_TREE crea su material interno
            bp->set_owner(root);
            bp->set_size(Vector3(1000, 1, 1000));
            bp->set_anchored(true);
            bp->set_color(Color(0.42f, 0.42f, 0.45f));   // gris Studio (cambiable)
            ResourceLoader* rload = ResourceLoader::get_singleton();
            const String tex_path = "res://GodotLuau/assets/textures/baseplate_grid.png";
            Ref<Texture2D> grid_tex;
            if (rload && rload->exists(tex_path)) grid_tex = rload->load(tex_path);
            if (grid_tex.is_null()) grid_tex = generar_textura_grid();
            bp->gl_apply_grid_texture(grid_tex, 125.0f);   // celda menor = 2 studs
        }

        // 3b. TERRAIN — Workspace.Terrain (como Roblox): vacío al inicio, se
        // llena por código con Terrain:FillBlock/FillBall (p.ej. MundoVoxel).
        if (need_terr) {
            RobloxTerrain* terr = memnew(RobloxTerrain);
            terr->set_name("Terrain");
            add_child(terr);
            terr->set_owner(root);
        }

        // 4. CÁMARA ACTUAL — visible en el árbol como en Roblox.
        // workspace.CurrentCamera es la cámara que renderiza el juego: en runtime
        // la controla el jugador, en el editor ves su posición.
        if (need_cam) {
            Camera3D* current_cam = memnew(Camera3D);
            current_cam->set_name("CurrentCamera");
            current_cam->set_position(Vector3(0.0f, 5.0f, 10.0f));
            current_cam->set_rotation_degrees(Vector3(-15.0f, 0.0f, 0.0f));
            add_child(current_cam);
            current_cam->set_owner(root);
        }

        GL_DEBUG_PRINT("[GodotLuau] RobloxWorkspace: entorno asegurado en el editor.");
    }


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

        // Reparar el entorno al jugar (antigris 1.16.6): aunque una escena vieja
        // tenga su Environment guardado gris (adjustment de saturacion baja, niebla
        // densa), _apply_roblox_render lo fuerza a un look correcto — jamas gris.
        // Ya no se recarga ningun .tres compartido (era la fuente del gris que
        // volvia una y otra vez).
        {
            WorldEnvironment* we = nullptr;
            DirectionalLight3D* sun = nullptr;
            for (int i = 0; i < get_child_count(); i++) {
                Node* c = get_child(i);
                if (!we)  we  = Object::cast_to<WorldEnvironment>(c);
                if (!sun) sun = Object::cast_to<DirectionalLight3D>(c);
            }
            Ref<Environment> env = we ? we->get_environment() : Ref<Environment>();
            if (!env.is_valid() && we) {
                env.instantiate();
                env->set_sky(_gl_make_roblox_sky());
                env->set_background(Environment::BG_SKY);
                we->set_environment(env);
            }
            if (env.is_valid() || sun) _apply_roblox_render(env, sun);
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

        // ── Ritmo de física adaptativo (rendimiento) ─────────────────────
        // El coste del tick de física crece con el numero de piezas. En un mapa
        // grande bajar de 60 a 30 Hz da FPS sin que se note en el juego (el
        // movimiento se interpola). En mapas pequeños se mantiene 60 Hz para
        // conservar la sensacion exacta de Roblox.
        call_deferred("_gl_tune_for_map_size");
        call_deferred("_gl_rebuild_chunks");

        // Calidad gráfica (1.15): aplica sombras suaves REALES, bias anti-acne,
        // SSAO/glow y escala según el nivel guardado. Diferido para que el sol y
        // el WorldEnvironment ya estén en el árbol. Esto arregla las sombras feas.
        call_deferred("_gl_apply_quality_deferred");
    }

    // Ajustes que dependen del TAMAÑO del mapa. Va DIFERIDO: en _ready el arbol
    // puede no estar completo todavia y el conteo salia bajo, con lo que la
    // fisica se quedaba en 60 Hz (se perdia ~16% de FPS sin que se notara).
    void _gl_tune_for_map_size() {
        const int nparts = _gl_count_parts(this);
        if (physics_fps > 0) {
            Engine::get_singleton()->set_physics_ticks_per_second(physics_fps);
        } else {
            // Manda el NUMERO TOTAL de piezas, que es un dato robusto.
            // (Antes habia aqui una "proteccion" extra que exigia ademas un
            // minimo de piezas dinamicas: como el conteo de dinamicas depende de
            // que Anchored ya este aplicado, daba 0 y ANULABA el ajuste — la
            // fisica se quedaba en 60 Hz y se perdia ~16% de FPS. Medido:
            // nparts=3384 y aun asi salia a 60.)
            Engine::get_singleton()->set_physics_ticks_per_second(nparts > 1500 ? 30 : 60);
        }

        // ── Distancia de sombra adaptativa en mapas grandes ──────────────
        // Un rango de sombra amplio obliga a redibujar medio mundo en el shadow
        // map cada frame. (El SSAO no se toca aqui: lo gobierna Lighting segun
        // Technology, y ponerlo aqui solo creaba un conflicto — Lighting reaplica
        // despues y lo volvia a encender.)
        if (nparts > 1500) {
            for (int i = 0; i < get_child_count(); i++)
                if (DirectionalLight3D* dl = Object::cast_to<DirectionalLight3D>(get_child(i)))
                    dl->set_param(Light3D::PARAM_SHADOW_MAX_DISTANCE, 120.0f);
        }

    }

    // Agrupa la geometria estatica en zonas (ver gl_chunks.h). Se puede volver a
    // llamar en cualquier momento: primero deshace el agrupado anterior.
    void _gl_rebuild_chunks() {
        if (Engine::get_singleton()->is_editor_hint()) return;
        if (!gl_custom().static_batching) { gl_clear_static_chunks(this); return; }
        const int n = gl_build_static_chunks(this, gl_custom().chunk_size);
        GL_DEBUG_PRINT("[GodotLuau] Chunks: ", n, " piezas agrupadas.");
    }

    void _gl_apply_quality_deferred() {
        gl_apply_graphics_quality(gl_graphics_level(), this);
    }

    static int _gl_count_parts(Node* n) {
        int c = Object::cast_to<RigidBody3D>(n) ? 1 : 0;
        for (int i = 0; i < n->get_child_count(); i++) c += _gl_count_parts(n->get_child(i));
        return c;
    }
    // Lo que cuesta en el tick de fisica son las piezas DINAMICAS: un mapa con
    // 5.000 piezas ancladas y 10 sueltas no necesita bajar el ritmo. Antes se
    // miraba el total y se penalizaba a mapas grandes pero quietos.
    static int _gl_count_dynamic_parts(Node* n) {
        int c = 0;
        if (Object::cast_to<RigidBody3D>(n)) {
            Variant a = n->get("Anchored");
            if (a.get_type() == Variant::BOOL && !(bool)a) c = 1;
        }
        for (int i = 0; i < n->get_child_count(); i++) c += _gl_count_dynamic_parts(n->get_child(i));
        return c;
    }
    void set_physics_fps(int v) {
        physics_fps = Math::clamp(v, 0, 240);
        if (is_node_ready() && !Engine::get_singleton()->is_editor_hint() && physics_fps > 0)
            Engine::get_singleton()->set_physics_ticks_per_second(physics_fps);
    }
    int get_physics_fps() const { return physics_fps; }

    // API de calidad gráfica para Lua (1.15): el módulo de ajustes en Modules la
    // usa. workspace:SetGraphicsQuality(1..10) / workspace:GetGraphicsQuality().
    void set_graphics_quality(int level) { gl_apply_graphics_quality(level, this); }
    int  get_graphics_quality() const    { return gl_graphics_level(); }

    // ── Modos de calidad (Automatic / Manual / Custom) ───────────────────
    // 0 = Automatic (ajusta solo para sostener los FPS), 1 = Manual (nivel fijo),
    // 2 = Custom (cada ajuste por separado). La UI vive en el Module de Luau.
    void set_graphics_mode(int mode) {
        gl_quality_mode() = CLAMP(mode, 0, 2);
        gl_apply_graphics_quality(gl_graphics_level(), this);
    }
    int  get_graphics_mode() const { return gl_quality_mode(); }
    void set_auto_target_fps(int fps) { gl_auto_target_fps() = CLAMP(fps, 15, 240); }
    int  get_auto_target_fps() const  { return gl_auto_target_fps(); }

    // Ajustes del modo Custom (cada uno independiente).
    void set_custom_setting(String name, float value) {
        GLCustomSettings& c = gl_custom();
        if      (name == "RenderScale")   c.render_scale   = CLAMP(value, 0.25f, 1.0f);
        else if (name == "Upscaler")      c.upscaler       = (int)CLAMP(value, 0.0f, 2.0f);
        else if (name == "Sharpness")     c.sharpness      = CLAMP(value, 0.0f, 2.0f);
        else if (name == "ShadowQuality") c.shadow_quality = (int)CLAMP(value, 0.0f, 5.0f);
        else if (name == "ViewDistance")  c.view_distance  = CLAMP(value, 0.25f, 2.0f);
        else if (name == "SSAO")          c.ssao           = value > 0.5f;
        else if (name == "Glow")          c.glow           = value > 0.5f;
        else if (name == "Fog")           c.fog            = value > 0.5f;
        // Occlusion culling: activarlo, desde que tamaño una pieza tapa, y como
        // de precisa es la estructura (mas preciso = descarta mejor pero cuesta
        // mas CPU; ese es el equilibrio del culling por software).
        else if (name == "Occlusion")     { c.occlusion      = value > 0.5f; _gl_refresh_occluders(this); }
        else if (name == "OcclusionSize") { c.occlusion_size = CLAMP(value, 2.0f, 64.0f); _gl_refresh_occluders(this); }
        else if (name == "OcclusionQuality") c.occlusion_bvh = (int)CLAMP(value, 0.0f, 2.0f);
        // Transparencia por borrado de pixeles: hay que rehacer los materiales
        // (cambia el modo de transparencia de cada pieza translucida).
        else if (name == "PixelTransparency") { c.pixel_transparency = value > 0.5f; _gl_refresh_transparency(this); }
        else if (name == "StaticBatching") { c.static_batching = value > 0.5f; _gl_rebuild_chunks(); }
        else if (name == "ChunkSize")      { c.chunk_size = CLAMP(value, 16.0f, 512.0f); _gl_rebuild_chunks(); }
        else return;
        gl_apply_graphics_quality(gl_graphics_level(), this);
    }
    float get_custom_setting(String name) const {
        const GLCustomSettings& c = gl_custom();
        if (name == "RenderScale")   return c.render_scale;
        if (name == "Upscaler")      return (float)c.upscaler;
        if (name == "Sharpness")     return c.sharpness;
        if (name == "ShadowQuality") return (float)c.shadow_quality;
        if (name == "ViewDistance")  return c.view_distance;
        if (name == "SSAO")          return c.ssao ? 1.0f : 0.0f;
        if (name == "Glow")          return c.glow ? 1.0f : 0.0f;
        if (name == "Fog")           return c.fog ? 1.0f : 0.0f;
        if (name == "Occlusion")        return c.occlusion ? 1.0f : 0.0f;
        if (name == "OcclusionSize")    return c.occlusion_size;
        if (name == "OcclusionQuality") return (float)c.occlusion_bvh;
        if (name == "PixelTransparency") return c.pixel_transparency ? 1.0f : 0.0f;
        if (name == "StaticBatching")    return c.static_batching ? 1.0f : 0.0f;
        if (name == "ChunkSize")         return c.chunk_size;
        return 0.0f;
    }

    // Rehace los occluders de todas las piezas (al cambiar los ajustes de
    // occlusion hay que recalcular cuales tapan y cuales ya no).
    // Rehace el material de las piezas translucidas al cambiar el modo de
    // transparencia (las opacas no se tocan: no cambian).
    static void _gl_refresh_transparency(Node* n) {
        if (n->is_class("RobloxPart")) {
            Variant t = n->get("Transparency");
            if (t.get_type() == Variant::FLOAT && (float)t > 0.001f)
                n->set("Transparency", (float)t);   // el setter rehace el material
        }
        for (int i = 0; i < n->get_child_count(); i++) _gl_refresh_transparency(n->get_child(i));
    }

    static void _gl_refresh_occluders(Node* n) {
        if (n->is_class("RobloxPart") && n->has_method("_gl_apply_occluder"))
            n->call("_gl_apply_occluder");
        for (int i = 0; i < n->get_child_count(); i++) _gl_refresh_occluders(n->get_child(i));
    }

    // ── Muerte por vacío (1.15) ──────────────────────────────────────────
    void _process(double delta) override {
        if (Engine::get_singleton()->is_editor_hint()) return;

        // Modo Automatic: ajusta la calidad sola para sostener los FPS objetivo
        // (no hace nada en Manual/Custom).
        gl_auto_quality_tick(delta, this);

        // Recalcular el umbral cada ~1s (no por frame: escanear el mapa entero
        // sale caro y el suelo casi nunca cambia).
        _fall_scan_timer -= delta;
        if (_fall_scan_timer <= 0.0) {
            _fall_scan_timer = 1.0;
            if (auto_fall_height) {
                // El suelo se calcula UNA vez y se reutiliza: recorrer todo el
                // arbol cada segundo costaba caro y el punto mas bajo del mapa no
                // cambia (salvo que se construya por debajo, caso raro; se
                // recalcula al respawnear con _gl_invalidate_ground).
                if (_ground_cached) {
                    _fall_effective = _ground_value;
                } else {
                bool found = false;
                float lowest = 0.0f;
                _scan_lowest_anchored(this, lowest, found);
                // Sin geometría anclada (mapa solo de terreno o dinámico): mantener
                // el fijo por defecto, para no matar a todos en +infinito.
                _fall_effective = found ? (lowest - fall_height_margin) : fallen_parts_destroy_height;
                _ground_value = _fall_effective;
                _ground_cached = true;
                }
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
        // THROTTLE (rendimiento): recorrer TODO el arbol cada frame costaba carisimo
        // en mapas grandes (38K nodos -> un vector por nodo por frame). Cae por
        // debajo del suelo no necesita precision de frame: basta cada ~0.3s.
        _fall_destroy_timer -= delta;
        if (_fall_destroy_timer <= 0.0) {
            _fall_destroy_timer = 0.3;
            _destroy_fallen_parts(this);
        }
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