#ifndef GL_CHUNKS_H
#define GL_CHUNKS_H

// ════════════════════════════════════════════════════════════════════
//  Agrupado de geometria estatica por ZONAS (chunks) — rendimiento
//
//  El problema medido: un place importado tiene miles de piezas y CADA una es
//  un objeto que la GPU dibuja por separado. Ocultar toda la geometria daba
//  +22% de FPS, asi que ese dibujado es una de las dos grandes facturas.
//
//  La idea (la misma que usa Roblox): las piezas ANCLADAS no se mueven, asi que
//  se pueden dibujar EN GRUPO. Se agrupan por:
//        zona del mapa  +  misma malla  +  mismo material
//  y cada grupo pasa a ser UN MultiMesh: una sola llamada de dibujado para
//  cientos de piezas, en vez de cientos de llamadas.
//
//  Los nodos Part NO se tocan ni se borran: siguen existiendo con todas sus
//  propiedades para los scripts. Solo se OCULTA su MeshInstance (lo dibuja ya
//  el grupo). Por eso es reversible: gl_clear_static_chunks() lo deshace.
//
//  Solo entran piezas ancladas y visibles. Una pieza que se mueva, cambie de
//  color o se rompa deja su grupo desactualizado, por eso el sistema se puede
//  reconstruir en cualquier momento (gl_build_static_chunks) y se ofrece como
//  ajuste que el juego puede apagar.
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/shape3d.hpp>
#include "gl_graphics.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// Nombre del nodo que cuelga del Workspace y contiene todos los grupos, para
// poder encontrarlos y borrarlos sin tocar nada mas.
#define GL_CHUNKS_ROOT "__GLStaticChunks"

// Mallas UNITARIAS (una por forma). La clave del agrupado deja de ser "la misma
// malla" y pasa a ser "la misma FORMA": el tamaño de cada pieza viaja en la
// escala de su instancia. Asi entran juntas piezas que miden distinto, que es
// lo que dejaba fuera a la mayoria de la geometria en la fase 1.
inline Ref<Mesh>& gl_unit_box() {
    static Ref<Mesh> m;
    if (!m.is_valid()) { Ref<BoxMesh> b; b.instantiate(); b->set_size(Vector3(1,1,1)); m = b; }
    return m;
}
inline Ref<Mesh>& gl_unit_sphere() {
    static Ref<Mesh> m;
    if (!m.is_valid()) { Ref<SphereMesh> sp; sp.instantiate(); sp->set_radius(0.5f); sp->set_height(1.0f); m = sp; }
    return m;
}
inline Ref<Mesh>& gl_unit_cylinder() {
    static Ref<Mesh> m;
    if (!m.is_valid()) { Ref<CylinderMesh> c; c.instantiate(); c->set_top_radius(0.5f); c->set_bottom_radius(0.5f); c->set_height(1.0f); m = c; }
    return m;
}
inline bool gl_unit_form(const Ref<Mesh>& src, Ref<Mesh>& out_unit, Vector3& out_scale, int& out_kind) {
    if (BoxMesh* b = Object::cast_to<BoxMesh>(src.ptr())) {
        out_unit = gl_unit_box(); out_scale = b->get_size(); out_kind = 0; return true;
    }
    if (SphereMesh* sp = Object::cast_to<SphereMesh>(src.ptr())) {
        const float d = sp->get_radius() * 2.0f;
        out_unit = gl_unit_sphere(); out_scale = Vector3(d, sp->get_height(), d); out_kind = 1; return true;
    }
    if (CylinderMesh* c = Object::cast_to<CylinderMesh>(src.ptr())) {
        const float d = c->get_top_radius() * 2.0f;
        out_unit = gl_unit_cylinder(); out_scale = Vector3(d, c->get_height(), d); out_kind = 2; return true;
    }
    return false;
}

struct GLChunkGroup {
    Ref<Mesh>          mesh;
    Ref<Material>      material;
    LocalVector<Transform3D> xforms;
};

// Recolecta las piezas ancladas y las reparte en grupos (zona+malla+material).
inline void _gl_collect_static(Node* n, float chunk_size,
                               HashMap<String, GLChunkGroup>& groups,
                               LocalVector<MeshInstance3D*>& hidden) {
    if (!n) return;

    // Solo piezas ancladas: las dinamicas se mueven y no se pueden agrupar.
    if (Object::cast_to<RigidBody3D>(n)) {
        Variant anc = n->get("Anchored");
        Variant tr  = n->get("Transparency");
        const bool anchored = (anc.get_type() == Variant::BOOL) && (bool)anc;
        const bool solid    = (tr.get_type() != Variant::FLOAT) || ((float)tr < 0.999f);
        Node3D* n3 = Object::cast_to<Node3D>(n);
        if (anchored && solid && n3) {
            for (int i = 0; i < n->get_child_count(); i++) {
                MeshInstance3D* mi = Object::cast_to<MeshInstance3D>(n->get_child(i));
                if (!mi || !mi->is_visible()) continue;
                Ref<Mesh> mesh = mi->get_mesh();
                Ref<Material> mat = mi->get_material_override();
                if (!mesh.is_valid()) continue;

                Ref<Mesh> unit; Vector3 mscale; int kind = 0;
                if (!gl_unit_form(mesh, unit, mscale, kind)) continue;

                const Vector3 p = n3->get_global_position();
                const String key =
                    String::num_int64((int64_t)Math::floor(p.x / chunk_size)) + "_" +
                    String::num_int64((int64_t)Math::floor(p.y / chunk_size)) + "_" +
                    String::num_int64((int64_t)Math::floor(p.z / chunk_size)) + "|" +
                    String::num_int64(kind) + "|" +
                    String::num_int64((int64_t)mat.ptr());

                GLChunkGroup* g = groups.getptr(key);
                if (!g) {
                    GLChunkGroup fresh;
                    fresh.mesh = unit;
                    fresh.material = mat;
                    groups.insert(key, fresh);
                    g = groups.getptr(key);
                }
                Transform3D t = n3->get_global_transform() * mi->get_transform();
                // scaled_local: la escala va en el espacio LOCAL de la pieza. Con
                // scaled() se aplicaria sobre los ejes del mundo y las piezas
                // rotadas saldrian deformadas.
                t.basis = t.basis.scaled_local(mscale);
                g->xforms.push_back(t);
                hidden.push_back(mi);
                break;   // una malla por pieza
            }
        }
    }
    for (int i = 0; i < n->get_child_count(); i++)
        _gl_collect_static(n->get_child(i), chunk_size, groups, hidden);
}

// Deshace el agrupado: borra los grupos y vuelve a mostrar las mallas propias.
inline void gl_clear_static_chunks(Node* workspace) {
    if (!workspace) return;
    Node* root = workspace->get_node_or_null(NodePath(GL_CHUNKS_ROOT));
    if (!root) return;
    // Volver a hacer visibles las mallas que este sistema oculto.
    Array marked = root->get_meta("hidden", Array());
    for (int i = 0; i < marked.size(); i++) {
        Object* o = ObjectDB::get_instance((ObjectID)(uint64_t)(int64_t)marked[i]);
        if (MeshInstance3D* mi = Object::cast_to<MeshInstance3D>(o)) mi->set_visible(true);
    }
    // Reactivar los colliders individuales que se apagaron al agruparlos.
    Array cols = root->get_meta("disabled_cols", Array());
    for (int i = 0; i < cols.size(); i++) {
        Object* o = ObjectDB::get_instance((ObjectID)(uint64_t)(int64_t)cols[i]);
        if (CollisionShape3D* cs = Object::cast_to<CollisionShape3D>(o)) cs->set_disabled(false);
    }
    workspace->remove_child(root);
    root->queue_free();
}

// ── FISICA por zonas ─────────────────────────────────────────────────
// Medido: el tick de fisica se lleva ~29% del tiempo, y el motivo es que hay
// MILES de cuerpos registrados (una pieza = un cuerpo). Aqui se juntan las formas
// de las piezas ancladas de una zona en UN SOLO cuerpo estatico y se apagan los
// colliders individuales: el broadphase pasa de miles de cuerpos a unas decenas.
//
// Se excluyen a proposito:
//   · piezas NO ancladas (se mueven: necesitan su propio cuerpo)
//   · piezas con Touched conectado (contact monitor activo): su collider tiene
//     que seguir siendo suyo o el evento dejaria de dispararse
//   · piezas sin colision (su CollisionShape ya viene desactivado)
inline void _gl_collect_colliders(Node* n, float chunk_size,
                                  HashMap<String, LocalVector<CollisionShape3D*>>& zones) {
    if (!n) return;
    if (RigidBody3D* rb = Object::cast_to<RigidBody3D>(n)) {
        Variant anc = n->get("Anchored");
        const bool anchored = (anc.get_type() == Variant::BOOL) && (bool)anc;
        const bool touch = rb->is_contact_monitor_enabled();
        if (anchored && !touch) {
            for (int i = 0; i < n->get_child_count(); i++) {
                CollisionShape3D* cs = Object::cast_to<CollisionShape3D>(n->get_child(i));
                if (!cs || cs->is_disabled() || !cs->get_shape().is_valid()) continue;
                const Vector3 p = rb->get_global_position();
                const String key =
                    String::num_int64((int64_t)Math::floor(p.x / chunk_size)) + "_" +
                    String::num_int64((int64_t)Math::floor(p.y / chunk_size)) + "_" +
                    String::num_int64((int64_t)Math::floor(p.z / chunk_size));
                LocalVector<CollisionShape3D*>* z = zones.getptr(key);
                if (!z) { zones.insert(key, LocalVector<CollisionShape3D*>()); z = zones.getptr(key); }
                z->push_back(cs);
                break;   // un collider por pieza
            }
        }
    }
    for (int i = 0; i < n->get_child_count(); i++)
        _gl_collect_colliders(n->get_child(i), chunk_size, zones);
}

// Crea un cuerpo estatico por zona. Devuelve cuantos colliders se agruparon.
inline int _gl_build_zone_bodies(Node* workspace, Node* root, float chunk_size,
                                 int min_group, Array& disabled_out) {
    HashMap<String, LocalVector<CollisionShape3D*>> zones;
    _gl_collect_colliders(workspace, chunk_size, zones);

    int merged = 0;
    for (HashMap<String, LocalVector<CollisionShape3D*>>::Iterator it = zones.begin(); it; ++it) {
        LocalVector<CollisionShape3D*>& list = it->value;
        if ((int)list.size() < min_group) continue;

        StaticBody3D* body = memnew(StaticBody3D);
        root->add_child(body);
        for (uint32_t i = 0; i < list.size(); i++) {
            CollisionShape3D* src = list[i];
            if (!src || !src->is_inside_tree()) continue;
            const Transform3D gx = src->get_global_transform();
            CollisionShape3D* copy = memnew(CollisionShape3D);
            copy->set_shape(src->get_shape());   // la forma ya viene del cache compartido
            body->add_child(copy);
            copy->set_global_transform(gx);
            src->set_disabled(true);             // su cuerpo deja de colisionar
            disabled_out.push_back((int64_t)(uint64_t)src->get_instance_id());
            merged++;
        }
    }
    return merged;
}

// Construye los grupos. Devuelve cuantas piezas se agruparon.
inline int gl_build_static_chunks(Node* workspace, float chunk_size = 128.0f, int min_group = 4) {
    if (!workspace) return 0;
    gl_clear_static_chunks(workspace);   // idempotente: primero se deshace lo anterior

    HashMap<String, GLChunkGroup> groups;
    LocalVector<MeshInstance3D*> candidates;
    _gl_collect_static(workspace, chunk_size, groups, candidates);

    Node3D* root = memnew(Node3D);
    root->set_name(GL_CHUNKS_ROOT);
    workspace->add_child(root);

    Array hidden_ids;
    int batched = 0;
    for (HashMap<String, GLChunkGroup>::Iterator it = groups.begin(); it; ++it) {
        GLChunkGroup& g = it->value;
        // Grupos muy pequeños no compensan: un MultiMesh de 2 piezas no ahorra
        // nada y añade un nodo mas.
        if ((int)g.xforms.size() < min_group) continue;

        Ref<MultiMesh> mm;
        mm.instantiate();
        mm->set_transform_format(MultiMesh::TRANSFORM_3D);
        mm->set_mesh(g.mesh);
        mm->set_instance_count((int)g.xforms.size());
        for (uint32_t i = 0; i < g.xforms.size(); i++)
            mm->set_instance_transform((int)i, g.xforms[i]);

        MultiMeshInstance3D* mmi = memnew(MultiMeshInstance3D);
        mmi->set_multimesh(mm);
        if (g.material.is_valid()) mmi->set_material_override(g.material);
        root->add_child(mmi);
        batched += (int)g.xforms.size();
    }

    // Ocultar SOLO las mallas de los grupos que de verdad se crearon. Se hace en
    // una segunda pasada: si un grupo se descarto por pequeño, su pieza debe
    // seguir dibujandose ella misma.
    if (batched > 0) {
        for (uint32_t i = 0; i < candidates.size(); i++) {
            MeshInstance3D* mi = candidates[i];
            if (!mi) continue;
            Node* parent = mi->get_parent();
            Node3D* p3 = Object::cast_to<Node3D>(parent);
            if (!p3) continue;
            Ref<Mesh> mesh = mi->get_mesh();
            Ref<Material> mat = mi->get_material_override();
            Ref<Mesh> unit; Vector3 mscale; int kind = 0;
            if (!mesh.is_valid() || !gl_unit_form(mesh, unit, mscale, kind)) continue;
            const Vector3 p = p3->get_global_position();
            const String key =
                String::num_int64((int64_t)Math::floor(p.x / chunk_size)) + "_" +
                String::num_int64((int64_t)Math::floor(p.y / chunk_size)) + "_" +
                String::num_int64((int64_t)Math::floor(p.z / chunk_size)) + "|" +
                String::num_int64(kind) + "|" +
                String::num_int64((int64_t)mat.ptr());
            GLChunkGroup* g = groups.getptr(key);
            if (g && (int)g->xforms.size() >= min_group) {
                mi->set_visible(false);
                hidden_ids.push_back((int64_t)(uint64_t)mi->get_instance_id());
            }
        }
    }
    root->set_meta("hidden", hidden_ids);

    // ── Fisica: un cuerpo estatico por zona ──────────────────────────────
    // APAGADA por defecto: MEDIDO que empeora (22.3 -> 17.8 fps). Un cuerpo con
    // cientos de formas dispersas tiene una caja envolvente enorme, asi que el
    // motor lo considera candidato de colision casi siempre, y encima se crean
    // miles de nodos de forma extra. Se deja disponible por si en un mapa muy
    // compacto (formas juntas, cajas pequeñas) si compensa.
    Array disabled_cols;
    const int merged = gl_custom().merge_zone_physics
        ? _gl_build_zone_bodies(workspace, root, chunk_size, min_group, disabled_cols) : 0;
    root->set_meta("disabled_cols", disabled_cols);
    if (merged > 0)
        UtilityFunctions::print("[GodotLuau] Chunks: ", batched, " piezas dibujadas en grupo, ",
                                merged, " colisiones unidas por zona.");
    return batched;
}

#endif // GL_CHUNKS_H
