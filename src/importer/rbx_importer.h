#ifndef RBX_IMPORTER_H
#define RBX_IMPORTER_H

// Convierte un .rbxl en un arbol de nodos de Godot.
//
// Orden de trabajo (importa respetarlo):
//   1. INST -> crear un nodo por instancia, todavia sueltos
//   2. PROP -> aplicar las propiedades columna a columna
//   3. PRNT -> construir la jerarquia y asignar owner
//   4. resolver las referencias entre instancias (ya existen todos los nodos)
//
// Nada de lo que venga del archivo puede tumbar el editor: si algo no cuadra se
// salta esa columna y se anota en el reporte.

#include "rbx_reader.h"
#include "rbx_prop.h"
#include "rbx_classmap.h"
#include "rbx_instances.h"
#include "luau_script.h"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

class RBXImporter : public RefCounted {
    GDCLASS(RBXImporter, RefCounted);

    String last_error;
    Dictionary report;
    bool verbose = false;

    struct Inst {
        String rbx_class;
        Node *node = nullptr;
    };

    struct PendingRef {
        Node *node = nullptr;
        String prop;
        int32_t target = -1;
    };

    HashMap<int32_t, Inst> by_ref;
    HashMap<String, HashSet<String>> prop_cache;   // clase Godot -> props validas
    Vector<PendingRef> pending_refs;
    Dictionary class_counts, missing_classes, ignored_props;
    Array missing_assets;
    int scripts_imported = 0, tags_applied = 0, orphans = 0, attrs_applied = 0;

    // get_property_list es caro: 38562 instancias no pueden pedirlo una a una.
    const HashSet<String> &_props_of(Node *n) {
        String cls = n->get_class();
        HashMap<String, HashSet<String>>::Iterator it = prop_cache.find(cls);
        if (it) return it->value;
        HashSet<String> set;
        TypedArray<Dictionary> pl = n->get_property_list();
        for (int k = 0; k < pl.size(); k++) {
            Dictionary d = pl[k];
            set.insert(d.get("name", String()));
        }
        prop_cache.insert(cls, set);
        return prop_cache.find(cls)->value;
    }

    // Godot exige que el nombre de un metadato sea un identificador valido, pero
    // en Roblox un atributo puede llamarse "Bevel Roundness". Se sustituye lo no
    // valido por _ y se guarda la equivalencia para poder consultarla.
    static String _safe_meta_name(const String &raw) {
        String out;
        bool changed = false;
        for (int k = 0; k < raw.length(); k++) {
            char32_t c = raw[k];
            bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                      (c >= '0' && c <= '9') || c == '_';
            if (!ok) { c = '_'; changed = true; }
            out += String::chr(c);
        }
        if (out.is_empty()) return String("_");
        // Un identificador no puede empezar por digito.
        char32_t f = out[0];
        if (f >= '0' && f <= '9') out = String("_") + out;
        (void)changed;
        return out;
    }

    void _set_meta_safe(Node *n, const String &name, const Variant &v) {
        String safe = _safe_meta_name(name);
        n->set_meta(StringName(safe), v);
        if (safe != name) {
            Dictionary alias = n->get_meta("__rbx_meta_alias", Dictionary());
            alias[safe] = name;
            n->set_meta("__rbx_meta_alias", alias);
        }
    }

    static String _safe_node_name(const String &raw, const String &fallback) {
        String s = raw.validate_node_name();
        s = s.strip_edges();
        if (s.is_empty()) s = fallback;
        return s;
    }

    void _bump(Dictionary &d, const String &k) {
        d[k] = (int)d.get(k, 0) + 1;
    }

    // Los tags van en una unica entrada de la tabla SSTR, separados por \0.
    void _apply_tags(Node *n, const PackedByteArray &raw) {
        if (raw.size() == 0) return;
        PackedStringArray tags;
        const uint8_t *p = raw.ptr();
        int start = 0;
        for (int k = 0; k <= raw.size(); k++) {
            if (k == raw.size() || p[k] == 0) {
                if (k > start) tags.push_back(String::utf8((const char *)(p + start), k - start));
                start = k + 1;
            }
        }
        if (tags.size() == 0) return;
        n->set_meta("__rbx_tags", tags);
        tags_applied += tags.size();
    }

    // AttributesSerialize: blob con pares nombre->valor tipado.
    //   u32 numero_de_atributos
    //   por cada uno: u32 len + nombre, u8 tipo, valor
    // Solo se decodifican los tipos que de verdad se usan en los juegos; si algo
    // no cuadra se guarda el blob entero y se sigue (nunca se aborta).
    void _apply_attributes(Node *n, const PackedByteArray &raw) {
        if (raw.size() < 4) return;
        rbx::Reader r(raw.ptr(), raw.size());
        uint32_t count = r.u32le();
        if (!r.ok() || count > 4096) { n->set_meta("__rbx_attributes_raw", raw); return; }

        for (uint32_t k = 0; k < count; k++) {
            String name = r.str();
            uint8_t t = r.u8();
            if (!r.ok() || name.is_empty()) { n->set_meta("__rbx_attributes_raw", raw); return; }

            // Los codigos de tipo de un ATRIBUTO no son los del formato de
            // propiedades: son los de rbx_types::Variant. Verificados contra un
            // place real (0x11 = Vector3).
            Variant v;
            bool known = true;
            switch (t) {
                case 0x02: v = r.str(); break;                             // String
                case 0x03: v = (r.u8() != 0); break;                       // Bool
                case 0x05: v = r.f64le(); break;                           // Float64
                case 0x06: v = (int64_t)r.i64le(); break;                  // Int64
                case 0x09: { float a = r.f32le(); int32_t b = r.i32le();   // UDim
                             v = Vector2(a, (float)b); } break;
                case 0x0A: { float a = r.f32le(); int32_t b = r.i32le();   // UDim2
                             float c = r.f32le(); int32_t d = r.i32le();
                             v = Vector4(a, (float)b, c, (float)d); } break;
                case 0x0E: v = (int64_t)r.u32le(); break;                  // BrickColor
                case 0x0F: { float cr = r.f32le(), cg = r.f32le(),         // Color3
                                   cb = r.f32le();
                             v = Color(cr, cg, cb); } break;
                case 0x10: { float a = r.f32le(), b = r.f32le();           // Vector2
                             v = Vector2(a, b); } break;
                case 0x11: { float a = r.f32le(), b = r.f32le(),           // Vector3
                                   c = r.f32le();
                             v = Vector3(a, b, c); } break;
                case 0x14: { float m[9];                                   // CFrame
                             float px = r.f32le(), py = r.f32le(), pz = r.f32le();
                             for (int j = 0; j < 9; j++) m[j] = r.f32le();
                             v = rbx::cframe_to_transform(m, Vector3(px, py, pz)); } break;
                case 0x1B: { float a = r.f32le(), b = r.f32le();           // NumberRange
                             v = Vector2(a, b); } break;
                case 0x1C: { float a = r.f32le(), b = r.f32le(),           // Rect
                                   c = r.f32le(), d = r.f32le();
                             v = Rect2(a, b, c - a, d - b); } break;
                default: known = false; break;
            }
            if (!known || !r.ok()) { n->set_meta("__rbx_attributes_raw", raw); return; }
            _set_meta_safe(n, name, v);   // asi lo lee GetAttribute de luau_api.h
            attrs_applied++;
        }
    }

    void _set_owner_recursive(Node *n, Node *owner) {
        for (int k = 0; k < n->get_child_count(); k++) {
            Node *c = n->get_child(k);
            c->set_owner(owner);
            _set_owner_recursive(c, owner);
        }
    }

    void _cleanup_unparented() {
        for (HashMap<int32_t, Inst>::Iterator it = by_ref.begin(); it; ++it) {
            Node *n = it->value.node;
            if (n && !n->get_parent()) memdelete(n);
        }
        by_ref.clear();
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("ImportFile", "path"),  &RBXImporter::import_file);
        ClassDB::bind_method(D_METHOD("GetLastError"),        &RBXImporter::get_last_error);
        ClassDB::bind_method(D_METHOD("GetReport"),           &RBXImporter::get_report);
        ClassDB::bind_method(D_METHOD("SetVerbose", "v"),     &RBXImporter::set_verbose);
        // Alias en snake_case para GDScript
        ClassDB::bind_method(D_METHOD("import_file", "path"), &RBXImporter::import_file);
        ClassDB::bind_method(D_METHOD("get_last_error"),      &RBXImporter::get_last_error);
        ClassDB::bind_method(D_METHOD("get_report"),          &RBXImporter::get_report);
        ClassDB::bind_method(D_METHOD("set_verbose", "v"),    &RBXImporter::set_verbose);
    }

public:
    void set_verbose(bool v) { verbose = v; }
    String get_last_error() const { return last_error; }
    Dictionary get_report() const { return report; }

    Node *import_file(const String &p_path) {
        const uint64_t t0 = Time::get_singleton()->get_ticks_msec();
        last_error = String();
        report = Dictionary();
        by_ref.clear(); prop_cache.clear(); pending_refs.clear();
        class_counts = Dictionary(); missing_classes = Dictionary();
        ignored_props = Dictionary(); missing_assets = Array();
        scripts_imported = tags_applied = orphans = attrs_applied = 0;

        PackedByteArray bytes = FileAccess::get_file_as_bytes(p_path);
        if (bytes.size() < 32) {
            last_error = "No se pudo leer el archivo o esta vacio: " + p_path;
            return nullptr;
        }
        if (bytes[0] == '<' && bytes[7] != '!') {
            last_error = "Este .rbxlx esta en XML. El lector de XML llega en la "
                         "siguiente version; por ahora guardalo en Studio como .rbxl binario.";
            return nullptr;
        }

        rbx::RBXFile file;
        String err;
        if (!rbx::parse_container(bytes, file, err)) { last_error = err; return nullptr; }

        // ── 1. INST: crear los nodos ────────────────────────────────
        HashMap<uint32_t, String> class_names;
        HashMap<uint32_t, Vector<int32_t>> class_refs;

        for (int ci = 0; ci < file.chunks.size(); ci++) {
            const rbx::Chunk &c = file.chunks[ci];
            if (c.name != "INST") continue;
            rbx::Reader r(c.data.ptr(), c.data.size());
            uint32_t idx = r.u32le();
            String cname = r.str();
            uint8_t fmt = r.u8();
            uint32_t count = r.u32le();
            if (!r.ok() || count > file.instance_count + 1024) continue;

            LocalVector<int32_t> refs;
            refs.resize(count);
            r.read_ref_array(count, refs.ptr());
            if (!r.ok()) continue;
            if (fmt == 1) r.skip(count);            // flags is_service

            class_names[idx] = cname;
            Vector<int32_t> vec;
            for (uint32_t k = 0; k < count; k++) vec.push_back(refs[k]);
            class_refs[idx] = vec;

            String godot_class = rbx::map_class(cname);
            for (uint32_t k = 0; k < count; k++) {
                Node *n = nullptr;
                if (!godot_class.is_empty())
                    n = Object::cast_to<Node>(ClassDB::instantiate(StringName(godot_class)));
                if (!n) {
                    RBXInstance *gi = memnew(RBXInstance);
                    gi->set_rbx_class_name(cname);
                    n = gi;
                    if (godot_class.is_empty()) _bump(missing_classes, cname);
                }
                n->set_name(cname);
                n->set_meta("__rbx_class", cname);
                Inst inst; inst.rbx_class = cname; inst.node = n;
                by_ref.insert(refs[k], inst);
            }
            _bump(class_counts, cname);
            class_counts[cname] = (int)count;
        }

        // ── 2. PROP: aplicar propiedades ────────────────────────────
        for (int ci = 0; ci < file.chunks.size(); ci++) {
            const rbx::Chunk &c = file.chunks[ci];
            if (c.name != "PROP") continue;
            rbx::Reader r(c.data.ptr(), c.data.size());
            uint32_t idx = r.u32le();
            String pname = r.str();
            uint8_t tid = r.u8();
            if (!r.ok()) continue;

            HashMap<uint32_t, Vector<int32_t>>::Iterator rit = class_refs.find(idx);
            if (!rit) continue;
            const Vector<int32_t> &refs = rit->value;
            const uint32_t count = (uint32_t)refs.size();
            if (count == 0) continue;
            const String rbx_class = class_names.has(idx) ? class_names[idx] : String();

            // Tags: hace falta el blob crudo, no la String resuelta.
            if (pname == "Tags" && tid == rbx::PT_SHAREDSTRING) {
                LocalVector<uint32_t> ids;
                if (!rbx::decode_sharedstring_indices(r, count, ids)) continue;
                for (uint32_t k = 0; k < count; k++) {
                    HashMap<int32_t, Inst>::Iterator ii = by_ref.find(refs[k]);
                    if (!ii || (int)ids[k] >= file.shared_raw.size()) continue;
                    _apply_tags(ii->value.node, file.shared_raw[ids[k]]);
                }
                continue;
            }

            // AttributesSerialize es un blob binario disfrazado de String: hay
            // que leerlo en crudo o se pierde al validarlo como texto.
            if (pname == "AttributesSerialize" && tid == rbx::PT_STRING) {
                for (uint32_t k = 0; k < count; k++) {
                    PackedByteArray raw = r.blob();
                    if (!r.ok()) break;
                    if (raw.size() == 0) continue;
                    HashMap<int32_t, Inst>::Iterator ii = by_ref.find(refs[k]);
                    if (ii) _apply_attributes(ii->value.node, raw);
                }
                continue;
            }

            Vector<Variant> vals;
            if (!rbx::decode_column(r, tid, count, file.shared_strings, vals)) {
                _bump(ignored_props, rbx_class + String(".") + pname +
                                     " (" + rbx::prop_type_name(tid) + ")");
                continue;
            }

            for (uint32_t k = 0; k < count; k++) {
                HashMap<int32_t, Inst>::Iterator ii = by_ref.find(refs[k]);
                if (!ii) continue;
                Node *n = ii->value.node;
                const Variant &v = vals[k];

                if (pname == "Name") {
                    String raw = v;
                    String safe = _safe_node_name(raw, rbx_class);
                    n->set_name(safe);
                    if (safe != raw) n->set_meta("__rbx_name", raw);
                    continue;
                }
                if (pname == "Source") {
                    String src = v;
                    Ref<LuauScript> ls; ls.instantiate();
                    ls->_set_source_code(src);
                    n->set("codigo_luau", ls);
                    if (!src.is_empty()) scripts_imported++;
                    continue;
                }
                if (tid == rbx::PT_REF) {
                    int32_t tgt = (int32_t)(int64_t)v;
                    if (tgt >= 0) {
                        PendingRef pr; pr.node = n; pr.prop = pname; pr.target = tgt;
                        pending_refs.push_back(pr);
                    }
                    continue;
                }

                String target = rbx::map_property(rbx_class, pname);
                if (target.is_empty()) continue;

                Variant value = v;
                if (target == "Material")        value = rbx::map_material((int)(int64_t)v);
                else if (target == "Shape")      value = rbx::map_part_type((int)(int64_t)v);
                else if (pname == "BrickColor")  { target = "Color"; value = rbx::brick_color((int)(int64_t)v); }
                else if (target == "CFrame") {
                    // El CFrame de una part es su posicion y rotacion en el mundo.
                    Node3D *n3 = Object::cast_to<Node3D>(n);
                    if (n3) { n3->set_transform(v); continue; }
                }

                // Assets de la nube de Roblox: no se pueden descargar.
                if (value.get_type() == Variant::STRING) {
                    String s = value;
                    if (s.begins_with("rbxassetid://") ||
                        s.begins_with("http://www.roblox.com/asset") ||
                        s.begins_with("rbxasset://")) {
                        Dictionary d;
                        d["Instancia"] = n->get_name();
                        d["Propiedad"] = target;
                        d["AssetId"] = s;
                        missing_assets.push_back(d);
                    }
                }

                if (_props_of(n).has(target)) {
                    n->set(StringName(target), value);
                } else {
                    // No se pierde el dato: queda como metadato inspeccionable.
                    _set_meta_safe(n, target, value);
                    _bump(ignored_props, rbx_class + String(".") + target);
                }
            }
        }

        // ── 3. PRNT: jerarquia ──────────────────────────────────────
        // El DataModel NO es una instancia del archivo: TODOS los servicios
        // (Workspace, Players, Lighting...) llegan con parent == -1. Por eso la
        // raiz se crea aqui, como hace Roblox con game.
        Node *root = memnew(Node);
        root->set_name("Game");
        root->set_meta("__rbx_class", "DataModel");
        for (int ci = 0; ci < file.chunks.size(); ci++) {
            const rbx::Chunk &c = file.chunks[ci];
            if (c.name != "PRNT") continue;
            rbx::Reader r(c.data.ptr(), c.data.size());
            r.u8();                              // version
            uint32_t count = r.u32le();
            if (!r.ok() || count > file.instance_count + 1024) break;

            LocalVector<int32_t> childs, parents;
            childs.resize(count); parents.resize(count);
            r.read_ref_array(count, childs.ptr());
            r.read_ref_array(count, parents.ptr());
            if (!r.ok()) break;

            for (uint32_t k = 0; k < count; k++) {
                HashMap<int32_t, Inst>::Iterator ci2 = by_ref.find(childs[k]);
                if (!ci2) continue;
                Node *child = ci2->value.node;
                if (parents[k] < 0) {           // servicio: cuelga del DataModel
                    root->add_child(child);
                    continue;
                }
                HashMap<int32_t, Inst>::Iterator pi = by_ref.find(parents[k]);
                if (!pi) { root->add_child(child); orphans++; continue; }
                pi->value.node->add_child(child);
            }
            break;
        }

        // Cualquier nodo que el PRNT no haya colocado no puede quedar suelto: se
        // filtraria memoria y ademas se perderia informacion del place.
        for (HashMap<int32_t, Inst>::Iterator it = by_ref.begin(); it; ++it) {
            Node *n = it->value.node;
            if (n && n != root && !n->get_parent()) { root->add_child(n); orphans++; }
        }

        // ── 4. Referencias entre instancias ─────────────────────────
        int refs_ok = 0;
        for (int k = 0; k < pending_refs.size(); k++) {
            const PendingRef &pr = pending_refs[k];
            HashMap<int32_t, Inst>::Iterator ti = by_ref.find(pr.target);
            if (!ti || !ti->value.node) continue;
            Node *tgt = ti->value.node;
            String prop = rbx::map_property(String(), pr.prop);
            if (prop.is_empty()) continue;

            if (_props_of(pr.node).has(prop)) {
                // PrimaryPart y compania esperan un NodePath relativo a la raiz.
                Variant cur = pr.node->get(StringName(prop));
                if (cur.get_type() == Variant::NODE_PATH)
                    pr.node->set(StringName(prop), pr.node->get_path_to(tgt));
                else
                    pr.node->set(StringName(prop), tgt);
            } else {
                _set_meta_safe(pr.node, prop, pr.node->get_path_to(tgt));
            }
            refs_ok++;
        }

        // El owner debe ponerse con el arbol ya montado o la escena no se guarda.
        _set_owner_recursive(root, root);

        const uint64_t ms = Time::get_singleton()->get_ticks_msec() - t0;

        report["instancias_totales"]     = (int)file.instance_count;
        report["nodos_creados"]          = (int)by_ref.size();
        report["clases_en_archivo"]      = (int)file.class_count;
        report["por_clase"]              = class_counts;
        report["clases_sin_equivalente"] = missing_classes;
        report["propiedades_ignoradas"]  = ignored_props;
        report["assets_faltantes"]       = missing_assets;
        report["scripts_importados"]     = scripts_imported;
        report["tags_aplicados"]         = tags_applied;
        report["atributos_aplicados"]    = attrs_applied;
        report["referencias_resueltas"]  = refs_ok;
        report["huerfanos"]              = orphans;
        report["tiempo_ms"]              = (int)ms;

        if (verbose) {
            UtilityFunctions::print("[RBX] ", p_path);
            UtilityFunctions::print("[RBX] ", (int)by_ref.size(), " nodos en ", (int)ms, " ms, ",
                                    scripts_imported, " scripts, ",
                                    missing_assets.size(), " assets sin resolver");
        }

        by_ref.clear();
        return root;
    }
};

#endif // RBX_IMPORTER_H
