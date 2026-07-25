#ifndef RBX_IMPORTER_H
#define RBX_IMPORTER_H

// Convierte un .rbxl en un arbol de nodos de Godot.
//
// Trabaja POR FASES y POR LOTES para no congelar el editor con un place de
// 90.000 instancias. El flujo es:
//
//   Begin(ruta)            abre el archivo y prepara los indices
//   Step()  -> bool        procesa un lote; false cuando ya no queda nada
//   GetProgress()/GetPhaseName()/GetCurrentItem()  para pintar la barra
//   GetResult()            el nodo raiz
//
// Fases: Leyendo -> Importando -> Propiedades -> Jerarquia -> Referencias ->
//        Comprobando -> Recolocando -> Listo
//
// "Comprobando" verifica que TODA instancia del archivo acabo en el arbol.
// "Recolocando" fusiona el resultado con la estructura Roblox que ya tengas en
// la escena (reutiliza tu Workspace, Players, etc. en vez de duplicarlos) y crea
// la que falte.
//
// Nada de lo que venga del archivo puede tumbar el editor: si algo no cuadra se
// salta esa columna y se anota en el reporte.

#include "rbx_reader.h"
#include "rbx_prop.h"
#include "rbx_classmap.h"
#include "rbx_instances.h"
#include "luau_script.h"
#include "roblox_workspace.h"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

class RBXImporter : public RefCounted {
    GDCLASS(RBXImporter, RefCounted);

public:
    enum Phase {
        PH_IDLE = 0, PH_PARSE, PH_INSTANCES, PH_PROPERTIES, PH_HIERARCHY,
        PH_REFS, PH_VERIFY, PH_PLACE, PH_DONE, PH_ERROR
    };

private:
    // Tamaño de lote por fase. Ajustados para que un step ronde los pocos ms:
    // suficiente para avanzar rapido sin que se note el tiron en el editor.
    static const int BATCH_INSTANCES = 2500;
    static const int BATCH_VALUES    = 4000;
    static const int BATCH_LINKS     = 6000;
    static const int BATCH_REFS      = 3000;
    static const int BATCH_VERIFY    = 6000;

    struct Inst {
        String rbx_class;
        Node *node = nullptr;
    };
    struct PendingRef {
        Node *node = nullptr;
        String prop;
        int32_t target = -1;
    };

    // ── Estado que sobrevive entre Step() ───────────────────────────
    rbx::RBXFile file;
    String path;
    Phase phase = PH_IDLE;
    int cursor = 0;          // indice dentro de la fase
    int sub_cursor = 0;      // indice dentro del chunk actual (fase PROPERTIES)

    Vector<int> inst_chunks, prop_chunks, prnt_chunks;
    HashMap<uint32_t, String> class_names;
    HashMap<uint32_t, Vector<int32_t>> class_refs;
    Vector<uint32_t> flat_class;     // por instancia: su class_index
    Vector<int32_t>  flat_ref;       // por instancia: su referent

    // Chunk PROP en curso
    bool prop_loaded = false;
    Vector<Variant> cur_vals;
    String cur_prop, cur_class;
    uint8_t cur_type = 0;
    Vector<int32_t> cur_refs;

    // Enlaces del PRNT ya decodificados
    Vector<int32_t> link_child, link_parent;

    HashMap<int32_t, Inst> by_ref;
    HashMap<String, HashSet<String>> prop_cache;
    Vector<PendingRef> pending_refs;

    Node *root = nullptr;
    Node *target_game = nullptr;     // Game existente con el que fusionar

    String last_error, current_item;
    Dictionary report, class_counts, missing_classes, ignored_props;
    Array missing_assets, verify_errors;
    int scripts_imported = 0, scripts_total = 0, scripts_seen = 0;
    int tags_applied = 0, orphans = 0, attrs_applied = 0, refs_ok = 0;
    int reused_services = 0, created_services = 0, verified_ok = 0;
    uint64_t t_start = 0;
    bool verbose = false;

    // ── Utilidades ──────────────────────────────────────────────────
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

    static String _safe_node_name(const String &raw, const String &fallback) {
        String s = raw.validate_node_name().strip_edges();
        if (s.is_empty()) s = fallback.is_empty() ? String("Instance") : fallback;
        return s;
    }

    // Godot exige identificador valido en los metadatos, pero un atributo de
    // Roblox puede llamarse "Bevel Roundness".
    static String _safe_meta_name(const String &raw) {
        String out;
        for (int k = 0; k < raw.length(); k++) {
            char32_t c = raw[k];
            bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                      (c >= '0' && c <= '9') || c == '_';
            out += String::chr(ok ? c : '_');
        }
        if (out.is_empty()) return String("_");
        char32_t f = out[0];
        if (f >= '0' && f <= '9') out = String("_") + out;
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

    void _bump(Dictionary &d, const String &k) { d[k] = (int)d.get(k, 0) + 1; }

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

    // AttributesSerialize: u32 numero, luego (u32 len + nombre, u8 tipo, valor).
    // Los codigos de tipo son los de rbx_types::Variant, NO los del formato de
    // propiedades (0x11 = Vector3, verificado contra un place real).
    void _apply_attributes(Node *n, const PackedByteArray &raw) {
        if (raw.size() < 4) return;
        rbx::Reader r(raw.ptr(), raw.size());
        uint32_t count = r.u32le();
        if (!r.ok() || count > 4096) { n->set_meta("__rbx_attributes_raw", raw); return; }

        for (uint32_t k = 0; k < count; k++) {
            String name = r.str();
            uint8_t t = r.u8();
            if (!r.ok() || name.is_empty()) { n->set_meta("__rbx_attributes_raw", raw); return; }

            Variant v;
            bool known = true;
            switch (t) {
                case 0x02: v = r.str(); break;                              // String
                case 0x03: v = (r.u8() != 0); break;                        // Bool
                case 0x05: v = r.f64le(); break;                            // Float64
                case 0x06: v = (int64_t)r.i64le(); break;                   // Int64
                case 0x09: { float a = r.f32le(); int32_t b = r.i32le();    // UDim
                             v = Vector2(a, (float)b); } break;
                case 0x0A: { float a = r.f32le(); int32_t b = r.i32le();    // UDim2
                             float c = r.f32le(); int32_t d = r.i32le();
                             v = Vector4(a, (float)b, c, (float)d); } break;
                case 0x0E: v = (int64_t)r.u32le(); break;                   // BrickColor
                case 0x0F: { float cr = r.f32le(), cg = r.f32le(),          // Color3
                                   cb = r.f32le();
                             v = Color(cr, cg, cb); } break;
                case 0x10: { float a = r.f32le(), b = r.f32le();            // Vector2
                             v = Vector2(a, b); } break;
                case 0x11: { float a = r.f32le(), b = r.f32le(),            // Vector3
                                   c = r.f32le();
                             v = Vector3(a, b, c); } break;
                case 0x14: { float m[9];                                    // CFrame
                             float px = r.f32le(), py = r.f32le(), pz = r.f32le();
                             for (int j = 0; j < 9; j++) m[j] = r.f32le();
                             v = rbx::cframe_to_transform(m, Vector3(px, py, pz)); } break;
                case 0x1B: { float a = r.f32le(), b = r.f32le();            // NumberRange
                             v = Vector2(a, b); } break;
                case 0x1C: { float a = r.f32le(), b = r.f32le(),            // Rect
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

    void _fail(const String &msg) {
        last_error = msg;
        phase = PH_ERROR;
        // Liberar lo que no llegara a colgarse de ningun sitio.
        for (HashMap<int32_t, Inst>::Iterator it = by_ref.begin(); it; ++it) {
            Node *n = it->value.node;
            if (n && !n->get_parent()) memdelete(n);
        }
        by_ref.clear();
        if (root && !root->get_parent()) { memdelete(root); root = nullptr; }
    }

    // ── Fases ───────────────────────────────────────────────────────
    bool _do_parse() {
        PackedByteArray bytes = FileAccess::get_file_as_bytes(path);
        if (bytes.size() < 32) { _fail("No se pudo leer el archivo o esta vacio: " + path); return false; }
        if (bytes[0] == '<' && bytes[7] != '!') {
            _fail("Este .rbxlx esta en XML. El lector de XML llega en la siguiente "
                  "version; por ahora guardalo en Studio como .rbxl binario.");
            return false;
        }
        String err;
        if (!rbx::parse_container(bytes, file, err)) { _fail(err); return false; }

        for (int k = 0; k < file.chunks.size(); k++) {
            const String &n = file.chunks[k].name;
            if (n == "INST") inst_chunks.push_back(k);
            else if (n == "PROP") prop_chunks.push_back(k);
            else if (n == "PRNT") prnt_chunks.push_back(k);
        }

        // Cabeceras de los INST: clases, referents y lista aplanada.
        for (int i = 0; i < inst_chunks.size(); i++) {
            const rbx::Chunk &c = file.chunks[inst_chunks[i]];
            rbx::Reader r(c.data.ptr(), c.data.size());
            uint32_t idx = r.u32le();
            String cname = r.str();
            uint8_t fmt = r.u8();
            uint32_t count = r.u32le();
            if (!r.ok() || count > file.instance_count + 1024) continue;

            LocalVector<int32_t> refs; refs.resize(count);
            r.read_ref_array(count, refs.ptr());
            if (!r.ok()) continue;

            class_names[idx] = cname;
            Vector<int32_t> vec;
            for (uint32_t k = 0; k < count; k++) {
                vec.push_back(refs[k]);
                flat_class.push_back(idx);
                flat_ref.push_back(refs[k]);
            }
            class_refs[idx] = vec;
            class_counts[cname] = (int)count;
        }

        // Cuantos scripts hay, para poder decir "script 12 de 202".
        for (int i = 0; i < prop_chunks.size(); i++) {
            const rbx::Chunk &c = file.chunks[prop_chunks[i]];
            rbx::Reader r(c.data.ptr(), c.data.size());
            uint32_t idx = r.u32le();
            String pname = r.str();
            if (pname != "Source") continue;
            HashMap<uint32_t, Vector<int32_t>>::Iterator it = class_refs.find(idx);
            if (it) scripts_total += it->value.size();
        }

        // Los servicios cuelgan de un DataModel que NO existe como instancia.
        root = memnew(Node);
        root->set_name("Game");
        root->set_meta("__rbx_class", "DataModel");

        cursor = 0;
        phase = PH_INSTANCES;
        return true;
    }

    bool _do_instances() {
        const int total = flat_ref.size();
        int end = MIN(cursor + BATCH_INSTANCES, total);
        for (; cursor < end; cursor++) {
            const String cname = class_names.has(flat_class[cursor])
                                 ? class_names[flat_class[cursor]] : String("Instance");
            String godot_class = rbx::map_class(cname);
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
            by_ref.insert(flat_ref[cursor], inst);
        }
        current_item = String::num_int64(cursor) + " / " + String::num_int64(total) + " instancias";
        if (cursor >= total) { cursor = 0; sub_cursor = 0; prop_loaded = false; phase = PH_PROPERTIES; }
        return true;
    }

    // Carga la siguiente columna de propiedades. Devuelve false si ya no quedan.
    bool _load_next_prop() {
        while (cursor < prop_chunks.size()) {
            const rbx::Chunk &c = file.chunks[prop_chunks[cursor]];
            rbx::Reader r(c.data.ptr(), c.data.size());
            uint32_t idx = r.u32le();
            cur_prop = r.str();
            cur_type = r.u8();
            if (!r.ok()) { cursor++; continue; }

            HashMap<uint32_t, Vector<int32_t>>::Iterator rit = class_refs.find(idx);
            if (!rit || rit->value.size() == 0) { cursor++; continue; }
            cur_refs = rit->value;
            cur_class = class_names.has(idx) ? class_names[idx] : String();
            const uint32_t count = (uint32_t)cur_refs.size();

            // Tags y AttributesSerialize son blobs binarios: se aplican enteros
            // aqui porque necesitan los bytes crudos, no el valor decodificado.
            if (cur_prop == "Tags" && cur_type == rbx::PT_SHAREDSTRING) {
                LocalVector<uint32_t> ids;
                if (rbx::decode_sharedstring_indices(r, count, ids)) {
                    for (uint32_t k = 0; k < count; k++) {
                        HashMap<int32_t, Inst>::Iterator ii = by_ref.find(cur_refs[k]);
                        if (ii && (int)ids[k] < file.shared_raw.size())
                            _apply_tags(ii->value.node, file.shared_raw[ids[k]]);
                    }
                }
                cursor++; continue;
            }
            if (cur_prop == "AttributesSerialize" && cur_type == rbx::PT_STRING) {
                for (uint32_t k = 0; k < count; k++) {
                    PackedByteArray raw = r.blob();
                    if (!r.ok()) break;
                    if (raw.size() == 0) continue;
                    HashMap<int32_t, Inst>::Iterator ii = by_ref.find(cur_refs[k]);
                    if (ii) _apply_attributes(ii->value.node, raw);
                }
                cursor++; continue;
            }

            if (!rbx::decode_column(r, cur_type, count, file.shared_strings, cur_vals)) {
                _bump(ignored_props, cur_class + String(".") + cur_prop +
                                     " (" + rbx::prop_type_name(cur_type) + ")");
                cursor++; continue;
            }
            sub_cursor = 0;
            prop_loaded = true;
            return true;
        }
        return false;
    }

    // Traduce las propiedades de GUI de Roblox a los METODOS de las clases Roblox*
    // (RobloxFrame/TextLabel/...): en Godot no se llaman "Position"/"Size"/etc.
    // como propiedades sino set_udim2_pos()/set_bg_color()/... Sin esto, NINGUNA
    // propiedad de UI se aplicaba: todo Frame quedaba con su default (blanco,
    // ~100x50, pegado arriba-izquierda) — de ahi los recuadros blancos raros.
    // Devuelve true si la manejo (para no seguir con la ruta generica).
    bool _apply_gui_prop(Node *n, const String &prop, const Variant &val) {
        const int t = val.get_type();
        // Posicion / tamano (UDim2 -> Vector4(sx,ox,sy,oy))
        if (prop == "Position" && t == Variant::VECTOR4 && n->has_method("set_udim2_pos")) {
            Vector4 u = val; n->call("set_udim2_pos", u.x, u.y, u.z, u.w); return true;
        }
        if (prop == "Size" && t == Variant::VECTOR4 && n->has_method("set_udim2_size")) {
            Vector4 u = val; n->call("set_udim2_size", u.x, u.y, u.z, u.w); return true;
        }
        if (prop == "AnchorPoint" && t == Variant::VECTOR2 && n->has_method("set_anchor_point")) {
            Vector2 a = val; n->call("set_anchor_point", a.x, a.y); return true;
        }
        // Fondo
        if (prop == "BackgroundColor3" && t == Variant::COLOR && n->has_method("set_bg_color")) {
            Color c = val; n->call("set_bg_color", c.r, c.g, c.b); return true;
        }
        if (prop == "BackgroundTransparency" && n->has_method("set_bg_alpha")) {
            n->call("set_bg_alpha", (float)val); return true;
        }
        if (prop == "BorderColor3" && t == Variant::COLOR && n->has_method("set_border_color")) {
            Color c = val; n->call("set_border_color", c.r, c.g, c.b); return true;
        }
        if (prop == "BorderSizePixel" && n->has_method("set_border_px")) {
            n->call("set_border_px", (int)(int64_t)val); return true;
        }
        // Texto
        if (prop == "Text" && n->has_method("set_text")) {
            n->call("set_text", String(val)); return true;
        }
        if (prop == "TextColor3" && t == Variant::COLOR && n->has_method("set_text_color")) {
            Color c = val; n->call("set_text_color", c.r, c.g, c.b); return true;
        }
        if (prop == "TextSize" && n->has_method("set_text_size")) {
            n->call("set_text_size", (int)(float)val); return true;
        }
        if (prop == "TextScaled" && n->has_method("set_text_scaled")) {
            n->call("set_text_scaled", (bool)val); return true;
        }
        // Imagen (rbxassetid:// no se puede bajar; set_image marca el asset faltante)
        if ((prop == "Image" || prop == "Texture") && n->has_method("set_image")) {
            n->call("set_image", String(val)); return true;
        }
        if (prop == "ImageColor3" && t == Variant::COLOR && n->has_method("set_image_color")) {
            Color c = val; n->call("set_image_color", c.r, c.g, c.b); return true;
        }
        if (prop == "ImageTransparency" && n->has_method("set_image_transparency")) {
            n->call("set_image_transparency", (float)val); return true;
        }
        return false;
    }

    void _apply_one_prop(int k) {
        HashMap<int32_t, Inst>::Iterator ii = by_ref.find(cur_refs[k]);
        if (!ii) return;
        Node *n = ii->value.node;
        const Variant &v = cur_vals[k];

        if (cur_prop == "Name") {
            String raw = v;
            String safe = _safe_node_name(raw, cur_class);
            n->set_name(safe);
            if (safe != raw) n->set_meta("__rbx_name", raw);
            return;
        }
        if (cur_prop == "Source") {
            String src = v;
            Ref<LuauScript> ls; ls.instantiate();
            ls->_set_source_code(src);
            n->set("codigo_luau", ls);
            scripts_seen++;
            if (!src.is_empty()) scripts_imported++;
            current_item = "Script " + String::num_int64(scripts_seen) + " de " +
                           String::num_int64(scripts_total) + ": " + n->get_name();
            return;
        }
        if (cur_type == rbx::PT_REF) {
            int32_t tgt = (int32_t)(int64_t)v;
            if (tgt >= 0) { PendingRef pr; pr.node = n; pr.prop = cur_prop; pr.target = tgt;
                            pending_refs.push_back(pr); }
            return;
        }

        String target = rbx::map_property(cur_class, cur_prop);
        if (target.is_empty()) return;

        Variant value = v;
        if (target == "Material")       value = rbx::map_material((int)(int64_t)v);
        else if (target == "Shape")     value = rbx::map_part_type((int)(int64_t)v);
        else if (cur_prop == "BrickColor") { target = "Color"; value = rbx::brick_color((int)(int64_t)v); }
        else if (target == "CFrame") {
            Node3D *n3 = Object::cast_to<Node3D>(n);
            if (n3) { n3->set_transform(v); return; }
        }

        if (value.get_type() == Variant::STRING) {
            String s = value;
            if (s.begins_with("rbxassetid://") || s.begins_with("rbxasset://") ||
                s.begins_with("http://www.roblox.com/asset")) {
                Dictionary d;
                d["Instancia"] = n->get_name();
                d["Propiedad"] = target;
                d["AssetId"] = s;
                missing_assets.push_back(d);
            }
        }

        // GUI: traducir Position/Size/BackgroundColor3/Text/... a los metodos de
        // las clases Roblox* (se usa el nombre ORIGINAL de Roblox, no el mapeado).
        if (_apply_gui_prop(n, cur_prop, value)) return;

        if (_props_of(n).has(target)) {
            n->set(StringName(target), value);
            return;
        }
        // La propiedad puede existir en Godot con otro nombre ("Visible" ->
        // "visible"). Sin esta pasada, un place importado enseña TODOS los
        // frames aunque en Roblox estuvieran ocultos.
        const String native = rbx::map_native_property(target);
        if (!native.is_empty() && _props_of(n).has(native)) {
            // Godot acota z_index a +-4096 y Roblox usa valores enormes
            // (999999999 para "siempre encima"): sin recortar suelta un error
            // por cada nodo.
            if (native == "z_index")
                value = CLAMP((int64_t)value, (int64_t)-4096, (int64_t)4096);
            n->set(StringName(native), value);
            return;
        }
        _set_meta_safe(n, target, value);
        _bump(ignored_props, cur_class + String(".") + target);
    }

    bool _do_properties() {
        int budget = BATCH_VALUES;
        while (budget > 0) {
            if (!prop_loaded && !_load_next_prop()) {
                cursor = 0; phase = PH_HIERARCHY; return true;
            }
            const int total = cur_vals.size();
            int end = MIN(sub_cursor + budget, total);
            budget -= (end - sub_cursor);
            for (; sub_cursor < end; sub_cursor++) _apply_one_prop(sub_cursor);
            if (sub_cursor >= total) {
                if (cur_prop != "Source")
                    current_item = cur_class + "." + cur_prop;
                prop_loaded = false;
                cursor++;
            }
        }
        return true;
    }

    bool _do_hierarchy() {
        if (link_child.is_empty()) {
            if (prnt_chunks.is_empty()) { cursor = 0; phase = PH_REFS; return true; }
            const rbx::Chunk &c = file.chunks[prnt_chunks[0]];
            rbx::Reader r(c.data.ptr(), c.data.size());
            r.u8();
            uint32_t count = r.u32le();
            if (!r.ok() || count > file.instance_count + 1024) { cursor = 0; phase = PH_REFS; return true; }
            LocalVector<int32_t> ch, pa; ch.resize(count); pa.resize(count);
            r.read_ref_array(count, ch.ptr());
            r.read_ref_array(count, pa.ptr());
            if (!r.ok()) { cursor = 0; phase = PH_REFS; return true; }
            for (uint32_t k = 0; k < count; k++) { link_child.push_back(ch[k]); link_parent.push_back(pa[k]); }
            cursor = 0;
        }

        const int total = link_child.size();
        int end = MIN(cursor + BATCH_LINKS, total);
        for (; cursor < end; cursor++) {
            HashMap<int32_t, Inst>::Iterator ci = by_ref.find(link_child[cursor]);
            if (!ci) continue;
            Node *child = ci->value.node;
            if (child->get_parent()) continue;
            if (link_parent[cursor] < 0) { root->add_child(child); continue; }
            HashMap<int32_t, Inst>::Iterator pi = by_ref.find(link_parent[cursor]);
            if (!pi) { root->add_child(child); orphans++; continue; }
            pi->value.node->add_child(child);
        }
        current_item = String::num_int64(cursor) + " / " + String::num_int64(total) + " enlaces";
        if (cursor >= total) {
            // Nada puede quedar suelto: seria una fuga y ademas se perderia.
            for (HashMap<int32_t, Inst>::Iterator it = by_ref.begin(); it; ++it) {
                Node *n = it->value.node;
                if (n && n != root && !n->get_parent()) { root->add_child(n); orphans++; }
            }
            cursor = 0; phase = PH_REFS;
        }
        return true;
    }

    bool _do_refs() {
        const int total = pending_refs.size();
        int end = MIN(cursor + BATCH_REFS, total);
        for (; cursor < end; cursor++) {
            const PendingRef &pr = pending_refs[cursor];
            HashMap<int32_t, Inst>::Iterator ti = by_ref.find(pr.target);
            if (!ti || !ti->value.node) continue;
            Node *tgt = ti->value.node;
            String prop = rbx::map_property(String(), pr.prop);
            if (prop.is_empty()) continue;
            if (_props_of(pr.node).has(prop)) {
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
        current_item = String::num_int64(cursor) + " / " + String::num_int64(total) + " referencias";
        if (cursor >= total) { cursor = 0; phase = PH_VERIFY; }
        return true;
    }

    // Comprobado: cada instancia del archivo debe tener nodo y estar colgada
    // dentro del arbol, por si algo se saltó en las fases anteriores.
    bool _do_verify() {
        const int total = flat_ref.size();
        int end = MIN(cursor + BATCH_VERIFY, total);
        for (; cursor < end; cursor++) {
            HashMap<int32_t, Inst>::Iterator it = by_ref.find(flat_ref[cursor]);
            const String cname = class_names.has(flat_class[cursor])
                                 ? class_names[flat_class[cursor]] : String("?");
            if (!it || !it->value.node) {
                if (verify_errors.size() < 200)
                    verify_errors.push_back(String("Sin nodo: ") + cname);
                continue;
            }
            Node *n = it->value.node;
            if (n != root && !n->get_parent()) {
                root->add_child(n);
                orphans++;
                if (verify_errors.size() < 200)
                    verify_errors.push_back(String("Recuperado suelto: ") + cname + " / " + n->get_name());
                continue;
            }
            verified_ok++;
        }
        current_item = String::num_int64(cursor) + " / " + String::num_int64(total) + " comprobadas";
        if (cursor >= total) { cursor = 0; phase = PH_PLACE; }
        return true;
    }

public:
    // ── Recolocado ──────────────────────────────────────────────────
    // Busca un Game (DataModel) ya montado en la escena para reutilizarlo.
    static Node *find_existing_game(Node *scene_root) {
        if (!scene_root) return nullptr;
        if (scene_root->is_class("RobloxTemplate") || scene_root->is_class("RobloxGame3D"))
            return scene_root;
        // Un Game valido es el que tiene un Workspace dentro.
        if (scene_root->get_node_or_null(NodePath("Workspace"))) return scene_root;
        for (int k = 0; k < scene_root->get_child_count(); k++) {
            Node *c = scene_root->get_child(k);
            if (c->is_class("RobloxTemplate") || c->is_class("RobloxGame3D")) return c;
            if (c->get_node_or_null(NodePath("Workspace"))) return c;
        }
        return nullptr;
    }

private:
    // Punto de aparicion "sobre el mapa": centroide XZ de las Parts ancladas
    // importadas y por encima de todo (tope Y). Asi el jugador cae en el centro
    // del mapa en vez de quedarse en la placa blanca del origen. Devuelve false
    // si no hay geometria anclada (nada que medir).
    bool _map_spawn_point(Node *ws, Vector3 &out) {
        if (!ws || !ws->is_inside_tree()) return false;
        bool found = false;
        double sx = 0, sz = 0;
        int n = 0;
        float maxY = 0, minY = 0;
        Vector<Node *> stack;
        stack.push_back(ws);
        while (!stack.is_empty()) {
            Node *nd = stack[stack.size() - 1];
            stack.remove_at(stack.size() - 1);
            if (nd->is_class("RobloxPart") && nd->has_meta("__rbx_class")) {
                Variant anc = nd->get("Anchored");
                if (anc.get_type() == Variant::BOOL && (bool)anc) {
                    if (Node3D *p = Object::cast_to<Node3D>(nd)) {
                        Vector3 gp = p->get_global_position();
                        sx += gp.x; sz += gp.z; n++;
                        if (!found) { maxY = minY = gp.y; found = true; }
                        else { if (gp.y > maxY) maxY = gp.y; if (gp.y < minY) minY = gp.y; }
                    }
                }
            }
            for (int k = 0; k < nd->get_child_count(); k++) stack.push_back(nd->get_child(k));
        }
        if (!found || n == 0) return false;
        float topY = maxY + 5.0f;
        // Evitar caidas absurdas cuando hay un skybox altisimo: si el mapa es muy
        // alto, aparecer a media altura y no desde el techo.
        if (maxY - minY > 300.0f) topY = minY + (maxY - minY) * 0.5f;
        out = Vector3((float)(sx / n), topY, (float)(sz / n));
        return true;
    }

    // Los nodos que vienen del .rbxl llevan la marca __rbx_class; los que creo
    // el template no. Si el place aporta puntos de aparicion propios, se quitan
    // los de relleno. Si NO trae ninguno (spawn por script, comun en muchos
    // juegos), la placa de relleno se REUBICA sobre el mapa en vez de dejarla en
    // el origen (0,0,0) lejos del mapa, que es por lo que el jugador aparecia en
    // una placa blanca solitaria en lugar de dentro del juego.
    void _drop_default_spawns(Node *ws) {
        Vector<Node *> imported, defaults;
        Vector<Node *> stack;
        stack.push_back(ws);
        while (!stack.is_empty()) {
            Node *n = stack[stack.size() - 1];
            stack.remove_at(stack.size() - 1);
            if (n->is_class("SpawnLocation")) {
                if (n->has_meta("__rbx_class")) imported.push_back(n);
                else                            defaults.push_back(n);
            }
            for (int k = 0; k < n->get_child_count(); k++) stack.push_back(n->get_child(k));
        }
        if (!imported.is_empty()) {
            // El place trae sus propios spawns: los de relleno sobran.
            for (int k = 0; k < defaults.size(); k++) {
                Node *d = defaults[k];
                if (d->get_parent()) d->get_parent()->remove_child(d);
                memdelete(d);
            }
            report["spawns_de_relleno_quitados"] = defaults.size();
            return;
        }
        // Sin spawn propio: reubicar UNA placa de relleno sobre el mapa.
        if (defaults.is_empty()) return;
        Vector3 spawn;
        if (!_map_spawn_point(ws, spawn)) return;   // no hay mapa que medir
        if (Node3D *keep = Object::cast_to<Node3D>(defaults[0]))
            keep->set_position(spawn);
        for (int k = 1; k < defaults.size(); k++) {   // sobran las demas
            Node *d = defaults[k];
            if (d->get_parent()) d->get_parent()->remove_child(d);
            memdelete(d);
        }
        report["spawn_de_relleno_reubicado"] = spawn;
    }

    // Vuelca el contenido del arbol importado en la estructura destino:
    // los servicios que ya existan se REUTILIZAN (solo se mueven sus hijos) y
    // los que falten se crean. Asi no salen Workspace duplicados.
    bool _do_place() {
        if (!target_game || target_game == root) { phase = PH_DONE; _finish(); return true; }

        // Solo 3D: un Workspace 2D no puede recibir un place de Roblox.
        Node *tws = target_game->get_node_or_null(NodePath("Workspace"));
        if (tws && tws->is_class("RobloxWorkspace2D")) {
            _fail("La escena destino usa un Workspace 2D. El importador de .rbxl "
                  "solo funciona en proyectos 3D.");
            return false;
        }

        Vector<Node *> services;
        for (int k = 0; k < root->get_child_count(); k++) services.push_back(root->get_child(k));

        for (int k = 0; k < services.size(); k++) {
            Node *svc = services[k];
            const String svc_name = svc->get_name();
            Node *dst = target_game->get_node_or_null(NodePath(svc_name));

            if (dst && dst != svc) {
                // Ya existe: movemos su contenido y descartamos el importado.
                Vector<Node *> kids;
                for (int j = 0; j < svc->get_child_count(); j++) kids.push_back(svc->get_child(j));
                for (int j = 0; j < kids.size(); j++) {
                    Node *kid = kids[j];
                    svc->remove_child(kid);
                    dst->add_child(kid);
                }
                root->remove_child(svc);
                memdelete(svc);
                reused_services++;
            } else {
                // No existe: el servicio importado pasa tal cual al destino.
                root->remove_child(svc);
                target_game->add_child(svc);
                created_services++;
            }
        }

        // El Game importado ya esta vacio.
        if (root && !root->get_parent()) { memdelete(root); }
        root = target_game;

        if (RobloxWorkspace *ws = Object::cast_to<RobloxWorkspace>(
                target_game->get_node_or_null(NodePath("Workspace")))) {
            // Si el place trae sus propios puntos de aparicion, el SpawnLocation
            // de relleno del template (en el origen) sobra: el mapa importado
            // suele estar lejisimos del 0,0,0 y el jugador aparecia en el vacio.
            _drop_default_spawns(ws);
            // El Workspace llego lleno de golpe y pudo quedarse sin cielo ni
            // sol: se lo pedimos ahora que ya tiene todo dentro.
            ws->gl_ensure_environment();
        }

        phase = PH_DONE;
        _finish();
        return true;
    }

    void _finish() {
        const uint64_t ms = Time::get_singleton()->get_ticks_msec() - t_start;
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
        report["comprobadas_ok"]         = verified_ok;
        report["errores_comprobacion"]   = verify_errors;
        report["servicios_reutilizados"] = reused_services;
        report["servicios_creados"]      = created_services;
        report["tiempo_ms"]              = (int)ms;
        current_item = "Listo";
        if (verbose)
            UtilityFunctions::print("[RBX] ", (int)by_ref.size(), " nodos en ", (int)ms,
                                    " ms | reutilizados ", reused_services,
                                    " servicios, creados ", created_services);
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("Begin", "path"),        &RBXImporter::begin_import);
        ClassDB::bind_method(D_METHOD("Step"),                 &RBXImporter::step);
        ClassDB::bind_method(D_METHOD("GetProgress"),          &RBXImporter::get_progress);
        ClassDB::bind_method(D_METHOD("GetPhaseName"),         &RBXImporter::get_phase_name);
        ClassDB::bind_method(D_METHOD("GetCurrentItem"),       &RBXImporter::get_current_item);
        ClassDB::bind_method(D_METHOD("GetResult"),            &RBXImporter::get_result);
        ClassDB::bind_method(D_METHOD("SetTarget", "game"),    &RBXImporter::set_target);
        ClassDB::bind_method(D_METHOD("ImportFile", "path"),   &RBXImporter::import_file);
        ClassDB::bind_method(D_METHOD("GetLastError"),         &RBXImporter::get_last_error);
        ClassDB::bind_method(D_METHOD("GetReport"),            &RBXImporter::get_report);
        ClassDB::bind_method(D_METHOD("SetVerbose", "v"),      &RBXImporter::set_verbose);
        ClassDB::bind_method(D_METHOD("IsDone"),               &RBXImporter::is_done);
        ClassDB::bind_method(D_METHOD("HasFailed"),            &RBXImporter::has_failed);
        // Alias snake_case para GDScript
        ClassDB::bind_method(D_METHOD("import_file", "path"),  &RBXImporter::import_file);
        ClassDB::bind_method(D_METHOD("get_last_error"),       &RBXImporter::get_last_error);
        ClassDB::bind_method(D_METHOD("get_report"),           &RBXImporter::get_report);
        ClassDB::bind_method(D_METHOD("set_verbose", "v"),     &RBXImporter::set_verbose);
    }

public:
    void set_verbose(bool v) { verbose = v; }
    String get_last_error() const { return last_error; }
    Dictionary get_report() const { return report; }
    Node *get_result() { return root; }
    bool is_done() const { return phase == PH_DONE; }
    bool has_failed() const { return phase == PH_ERROR; }
    String get_current_item() const { return current_item; }

    // Game existente con el que fusionar. Si es null, el resultado se queda como
    // un arbol suelto que el llamante colgara donde quiera.
    void set_target(Node *game) { target_game = game; }

    String get_phase_name() const {
        switch (phase) {
            case PH_IDLE:       return "Preparando";
            case PH_PARSE:      return "Leyendo archivo";
            case PH_INSTANCES:  return "Importando instancias";
            case PH_PROPERTIES: return "Importando propiedades";
            case PH_HIERARCHY:  return "Armando jerarquia";
            case PH_REFS:       return "Enlazando referencias";
            case PH_VERIFY:     return "Comprobado";
            case PH_PLACE:      return "Recolocado";
            case PH_DONE:       return "Listo";
            default:            return "Error";
        }
    }

    // Progreso global 0..1, ponderado por lo que cuesta cada fase.
    float get_progress() const {
        struct W { float base, span; };
        auto frac = [](int a, int b) -> float { return b > 0 ? (float)a / (float)b : 1.0f; };
        switch (phase) {
            case PH_IDLE:   return 0.0f;
            case PH_PARSE:  return 0.02f;
            case PH_INSTANCES:  return 0.05f + 0.20f * frac(cursor, flat_ref.size());
            case PH_PROPERTIES: return 0.25f + 0.45f * frac(cursor, prop_chunks.size());
            case PH_HIERARCHY:  return 0.70f + 0.10f * frac(cursor, link_child.size());
            case PH_REFS:       return 0.80f + 0.05f * frac(cursor, pending_refs.size());
            case PH_VERIFY:     return 0.85f + 0.10f * frac(cursor, flat_ref.size());
            case PH_PLACE:      return 0.95f;
            case PH_DONE:       return 1.0f;
            default:            return 1.0f;
        }
    }

    // Prepara la importacion. Devuelve false si el archivo no sirve.
    bool begin_import(const String &p_path) {
        path = p_path;
        last_error = String();
        report = Dictionary(); class_counts = Dictionary(); missing_classes = Dictionary();
        ignored_props = Dictionary(); missing_assets = Array(); verify_errors = Array();
        by_ref.clear(); prop_cache.clear(); pending_refs.clear();
        inst_chunks.clear(); prop_chunks.clear(); prnt_chunks.clear();
        flat_class.clear(); flat_ref.clear();
        link_child.clear(); link_parent.clear();
        class_names.clear(); class_refs.clear();
        file = rbx::RBXFile();
        cursor = sub_cursor = 0;
        prop_loaded = false;
        scripts_imported = scripts_total = scripts_seen = 0;
        tags_applied = orphans = attrs_applied = refs_ok = 0;
        reused_services = created_services = verified_ok = 0;
        root = nullptr;
        current_item = "";
        t_start = Time::get_singleton()->get_ticks_msec();
        phase = PH_PARSE;
        return _do_parse();
    }

    // Procesa un lote. Devuelve true mientras quede trabajo.
    bool step() {
        switch (phase) {
            case PH_PARSE:      return _do_parse();
            case PH_INSTANCES:  return _do_instances();
            case PH_PROPERTIES: return _do_properties();
            case PH_HIERARCHY:  return _do_hierarchy();
            case PH_REFS:       return _do_refs();
            case PH_VERIFY:     return _do_verify();
            case PH_PLACE:      return _do_place();
            default:            return false;   // DONE o ERROR
        }
    }

    // Importacion de una sola llamada (bloquea). Util desde codigo y para tests.
    Node *import_file(const String &p_path) {
        if (!begin_import(p_path)) return nullptr;
        int guard = 0;
        while (phase != PH_DONE && phase != PH_ERROR) {
            if (!step()) break;
            if (++guard > 2000000) { _fail("El importador no termina (bucle infinito)"); break; }
        }
        if (phase == PH_ERROR) return nullptr;
        if (phase != PH_DONE) { phase = PH_DONE; _finish(); }
        return root;
    }
};

VARIANT_ENUM_CAST(RBXImporter::Phase);

#endif // RBX_IMPORTER_H
