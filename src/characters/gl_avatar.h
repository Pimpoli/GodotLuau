#ifndef GL_AVATAR_H
#define GL_AVATAR_H

// ════════════════════════════════════════════════════════════════════
//  Avatar R6 por defecto (RobloxAvatarR6) + animador clasico de Roblox.
//
//  El personaje se arma por PARTES (Head/Torso/Left Arm/Right Arm/
//  Left Leg/Right Leg) cada una colgada de un pivote colocado en su
//  articulacion (los mismos joints C0 del R6 real), asi las animaciones
//  giran desde el hombro/cadera exactamente como en Roblox.
//
//  GLR6Animator reproduce las animaciones clasicas del R6 por codigo:
//  caminar (brazos y piernas en pendulo opuesto), idle (respiracion) y
//  salto/caida (brazos arriba). Los scripts pueden leer las partes por
//  su nombre Roblox ("Left Arm", "Torso", ...).
//
//  StarterCharacter: si existe StarterPlayer/StarterCharacter en el
//  juego, ESE modelo se clona como personaje (igual que Roblox).
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <vector>
#include "gl_debug.h"

using namespace godot;

// ── Animador R6 (hijo del root del personaje) ────────────────────────
class GLR6Animator : public Node {
    GDCLASS(GLR6Animator, Node);

    // ObjectIDs de los pivotes (seguros ante frees)
    uint64_t p_larm = 0, p_rarm = 0, p_lleg = 0, p_rleg = 0, p_head = 0;
    bool  ready_ok = false;
    float phase = 0.0f;
    float walk_amp = 0.0f;     // amplitud actual del pendulo (suavizada)
    float air_amp = 0.0f;      // 0..1 brazos arriba (suavizada)
    float idle_t = 0.0f;
    // Estado pedido desde fuera (Humanoid local / NetworkService en muñecos)
    double in_speed = 0.0;     // 0..1
    bool   in_air = false;

    static constexpr float REST_L = -1.5707963f;   // brazo izq colgando (z)
    static constexpr float REST_R =  1.5707963f;   // brazo der colgando (z)

    Node3D* _pivot(uint64_t id) {
        Object* o = ObjectDB::get_instance(id);
        return o ? Object::cast_to<Node3D>(o) : nullptr;
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_move_state", "speed01", "airborne"), &GLR6Animator::set_move_state);
    }

public:
    void set_move_state(double speed01, bool airborne) {
        in_speed = Math::clamp(speed01, 0.0, 1.0);
        in_air = airborne;
    }

    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        Node* par = get_parent();
        if (!par) return;
        Node* n;
        // Partes por su nombre Roblox (hijas del mismo padre que el animador, ya
        // sea el rig o el personaje aplanado). Antes eran "*Pivot".
        if ((n = par->get_node_or_null("Left Arm")))  p_larm = n->get_instance_id();
        if ((n = par->get_node_or_null("Right Arm"))) p_rarm = n->get_instance_id();
        if ((n = par->get_node_or_null("Left Leg")))  p_lleg = n->get_instance_id();
        if ((n = par->get_node_or_null("Right Leg"))) p_rleg = n->get_instance_id();
        if ((n = par->get_node_or_null("Head")))      p_head = n->get_instance_id();
        ready_ok = p_larm && p_rarm && p_lleg && p_rleg;
        if (ready_ok) GL_DEBUG_PRINT("[GodotLuau] R6 rig listo (animaciones clasicas activas).");
        set_process(ready_ok);
    }

    void _process(double dt) override {
        if (!ready_ok) return;
        Node3D* la = _pivot(p_larm); Node3D* ra = _pivot(p_rarm);
        Node3D* ll = _pivot(p_lleg); Node3D* rl = _pivot(p_rleg);
        if (!la || !ra || !ll || !rl) { ready_ok = false; set_process(false); return; }

        float k = 1.0f - (float)Math::pow(0.002, dt);   // suavizado de transiciones

        // Fase del pendulo: el ciclo de caminata R6 (~2 pasos/seg a WalkSpeed 16)
        bool moving = in_speed > 0.05 && !in_air;
        if (moving) phase += (float)dt * (float)(10.0 * Math::max(in_speed, 0.35));
        idle_t += (float)dt;

        walk_amp = Math::lerp(walk_amp, moving ? (float)(0.9 * Math::max(in_speed, 0.5)) : 0.0f, k);
        // Salto (1.15): el gesto entra rapido y SALE rapido al aterrizar. Antes
        // subida y bajada compartian la misma constante (~0.16 s), asi que los
        // brazos seguian arriba casi medio segundo despues de tocar el suelo: el
        // salto no dura mas, lo que dura de mas es la postura.
        float air_k = 1.0f - Math::exp(-(in_air ? 22.0f : 16.0f) * (float)dt);
        air_amp  = Math::lerp(air_amp, in_air ? 1.0f : 0.0f, air_k);

        float swing = Math::sin(phase) * walk_amp;
        float idle_sway = Math::sin(idle_t * 1.6f) * 0.025f;   // respiracion sutil

        // Brazos: pendulo OPUESTO a la pierna del mismo lado + subida en el aire
        // (rotacion z desde colgando hasta casi arriba, el salto clasico del R6)
        float up = air_amp * 2.9f;
        la->set_rotation(Vector3(-swing * 0.85f, 0.0f, REST_L - idle_sway + up));
        ra->set_rotation(Vector3( swing * 0.85f, 0.0f, REST_R + idle_sway - up));
        // Piernas: pendulo + leve tijera en el aire
        float leg_air = air_amp * 0.35f;
        ll->set_rotation(Vector3( swing + leg_air, 0.0f, 0.0f));
        rl->set_rotation(Vector3(-swing - leg_air * 0.6f, 0.0f, 0.0f));
        // Cabeza quieta (como el R6 clasico)
    }

    GLR6Animator() {}
};

// ── Construccion del personaje ───────────────────────────────────────
// 1) StarterPlayer/StarterCharacter (clonado, como Roblox)
// 2) RobloxAvatarR6 por partes con animador
// Devuelve nullptr si no hay nada (el caller pone su fallback).
static inline Node3D* gl_build_r6_rig() {
    ResourceLoader* rl = ResourceLoader::get_singleton();
    if (!rl) return nullptr;
    struct PartDef { const char* file; const char* name; const char* pivot; Vector3 joint; float rest_z; };
    static const PartDef PARTS[6] = {
        { "res://GodotLuau/assets/avatars/r6/R6_Torso.obj",    "Torso",     "TorsoPivot",    Vector3(0.0f,   0.13f, 0.0f), 0.0f },
        { "res://GodotLuau/assets/avatars/r6/R6_Head.obj",     "Head",      "HeadPivot",     Vector3(0.0f,   1.13f, 0.0f), 0.0f },
        { "res://GodotLuau/assets/avatars/r6/R6_LeftArm.obj",  "Left Arm",  "LeftArmPivot",  Vector3(1.5f,   0.63f, 0.0f), -1.5707963f },
        { "res://GodotLuau/assets/avatars/r6/R6_RightArm.obj", "Right Arm", "RightArmPivot", Vector3(-1.5f,  0.63f, 0.0f),  1.5707963f },
        { "res://GodotLuau/assets/avatars/r6/R6_LeftLeg.obj",  "Left Leg",  "LeftLegPivot",  Vector3(0.55f, -0.87f, 0.0f), 0.0f },
        { "res://GodotLuau/assets/avatars/r6/R6_RightLeg.obj", "Right Leg", "RightLegPivot", Vector3(-0.55f,-0.87f, 0.0f), 0.0f },
    };
    if (!rl->exists(PARTS[0].file)) return nullptr;

    Node3D* root = memnew(Node3D);
    root->set_name("Character");
    root->set_meta("_gl_r6rig", true);   // marca: personaje R6 propio → aplanable (partes/HRP suben a hijos directos)
    int built = 0;
    for (int i = 0; i < 6; i++) {
        if (!rl->exists(PARTS[i].file)) continue;
        Ref<Mesh> mesh = rl->load(PARTS[i].file);
        if (mesh.is_null()) continue;
        Node3D* pivot = memnew(Node3D);
        pivot->set_name(PARTS[i].name);   // el nodo-parte lleva el nombre Roblox (hijo directo del personaje al aplanar)
        pivot->set_position(PARTS[i].joint);
        if (PARTS[i].rest_z != 0.0f) pivot->set_rotation(Vector3(0, 0, PARTS[i].rest_z));
        root->add_child(pivot);
        MeshInstance3D* mi = memnew(MeshInstance3D);
        mi->set_name("Mesh");
        mi->set_mesh(mesh);
        pivot->add_child(mi);
        built++;
    }
    if (built < 4) { root->queue_free(); return nullptr; }

    // HumanoidRootPart (como Roblox R6): nodo invisible en el centro del torso;
    // los scripts leen su .Position (global) para saber donde esta el jugador.
    Node3D* hrp = memnew(Node3D);
    hrp->set_name("HumanoidRootPart");
    hrp->set_position(Vector3(0.0f, 0.13f, 0.0f));
    root->add_child(hrp);

    // Escala Roblox->fisica: el modelo mide 5.1 (pies -2.87, cabeza 2.23);
    // la capsula del cuerpo mide 2 con los pies en y=-1.
    float s = 2.0f / 5.1f;
    root->set_scale(Vector3(s, s, s));
    root->set_position(Vector3(0.0f, -1.0f + 2.87f * s, 0.0f));

    GLR6Animator* anim = memnew(GLR6Animator);
    anim->set_name("R6Animator");
    root->add_child(anim);
    return root;
}

// Busca StarterPlayer/StarterCharacter desde cualquier nodo del arbol.
static inline Node3D* gl_clone_starter_character(Node* ctx) {
    if (!ctx || !ctx->is_inside_tree()) return nullptr;
    Node* root = (Node*)ctx->get_tree()->get_root();
    // StarterPlayer puede estar en cualquier DataModel de la escena
    struct Finder {
        static Node* find(Node* n) {
            if (!n) return nullptr;
            if (n->get_name() == StringName("StarterPlayer")) return n;
            for (int i = 0; i < n->get_child_count(); i++)
                if (Node* r = find(n->get_child(i))) return r;
            return nullptr;
        }
    };
    Node* sp = Finder::find(root);
    if (!sp) return nullptr;
    Node* sc = sp->get_node_or_null("StarterCharacter");
    if (!sc) return nullptr;
    Node* dup = sc->duplicate();
    if (!dup) return nullptr;
    Node3D* d3 = Object::cast_to<Node3D>(dup);
    if (!d3) {
        // Envolver en un Node3D si la raiz clonada no es 3D
        Node3D* wrap = memnew(Node3D);
        wrap->add_child(dup);
        d3 = wrap;
    }
    d3->set_name("Character");
    d3->set_position(Vector3());
    GL_DEBUG_PRINT("[GodotLuau] StarterCharacter encontrado: usado como personaje.");
    return d3;
}

// Personaje por defecto: StarterCharacter > RobloxAvatarR6 > nullptr(caller fallback)
static inline Node3D* gl_build_character(Node* ctx) {
    if (Node3D* sc = gl_clone_starter_character(ctx)) return sc;
    return gl_build_r6_rig();
}

// ── Aplanado estilo Roblox ───────────────────────────────────────────
// En Roblox el Character es UN Model plano: HumanoidRootPart y las partes
// (Head/Torso/Left Arm/...) son hijos DIRECTOS. El rig R6 propio los tenía
// anidados (Character > *Pivot > Mesh) y HumanoidRootPart bajo el rig, así que
// Character:FindFirstChild("HumanoidRootPart") daba nil y rompía frameworks
// portados de Roblox. Esto sube las partes + HRP a hijos directos del "wrapper"
// (el RobloxPlayer local, que es lo que devuelve .Character) y descarta el root
// vacío. reparent(keep_global) hornea la escala/offset del rig → SIN cambio
// visual. Solo actúa sobre el rig R6 propio (meta _gl_r6rig); un StarterCharacter
// del usuario se deja intacto.
static inline void gl_flatten_r6_character(Node* wrapper, Node* character) {
    if (!wrapper || !character) return;
    if (!character->has_meta("_gl_r6rig")) return;
    if (!wrapper->is_inside_tree() || !character->is_inside_tree()) return;
    std::vector<Node*> kids;
    for (int i = 0; i < character->get_child_count(); i++)
        if (Node* c = character->get_child(i)) kids.push_back(c);
    for (Node* c : kids) c->reparent(wrapper, true);   // keep_global: hornea la transform del rig
    character->queue_free();
}

// Para el muñeco del jugador remoto: sube SOLO el HumanoidRootPart del rig
// visual a hijo directo del cuerpo (para que .Character:FindFirstChild(
// "HumanoidRootPart") resuelva en el servidor), dejando el rig "Visual" intacto
// (animador y bob del muñeco siguen colgando de "Visual").
static inline void gl_lift_hrp(Node* wrapper, Node* visual) {
    if (!wrapper || !visual || !wrapper->is_inside_tree()) return;
    if (Node* hrp = visual->get_node_or_null("HumanoidRootPart")) hrp->reparent(wrapper, true);
}

#endif // GL_AVATAR_H
