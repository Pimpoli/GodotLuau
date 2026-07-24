#ifndef RBX_PROP_H
#define RBX_PROP_H

// Decodificadores de los tipos de propiedad del formato binario de Roblox.
// Cada propiedad se guarda POR COLUMNAS: un chunk PROP trae el valor de esa
// propiedad para TODAS las instancias de la clase, no un objeto tras otro.
//
// Layouts verificados byte-exactos contra un place real (2995/3002 columnas).
// Los dos que no se pudieron verificar (PhysicalProperties y Content) devuelven
// false y el importador se limita a saltarse esa columna: como cada PROP es un
// chunk independiente, ignorar uno no desalinea nada.

#include "rbx_reader.h"

#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector4.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/quaternion.hpp>

using namespace godot;

namespace rbx {

enum PropType {
    PT_STRING = 0x01, PT_BOOL = 0x02, PT_INT32 = 0x03, PT_FLOAT32 = 0x04,
    PT_FLOAT64 = 0x05, PT_UDIM = 0x06, PT_UDIM2 = 0x07, PT_RAY = 0x08,
    PT_FACES = 0x09, PT_AXES = 0x0A, PT_BRICKCOLOR = 0x0B, PT_COLOR3 = 0x0C,
    PT_VECTOR2 = 0x0D, PT_VECTOR3 = 0x0E, PT_VECTOR2INT16 = 0x0F,
    PT_CFRAME = 0x10, PT_QUATERNION = 0x11, PT_ENUM = 0x12, PT_REF = 0x13,
    PT_VECTOR3INT16 = 0x14, PT_NUMBERSEQUENCE = 0x15, PT_COLORSEQUENCE = 0x16,
    PT_NUMBERRANGE = 0x17, PT_RECT = 0x18, PT_PHYSICALPROPERTIES = 0x19,
    PT_COLOR3UINT8 = 0x1A, PT_INT64 = 0x1B, PT_SHAREDSTRING = 0x1C,
    PT_OPTIONALCFRAME = 0x1E, PT_UNIQUEID = 0x1F, PT_FONT = 0x20,
    PT_SECURITYCAPABILITIES = 0x21, PT_CONTENT = 0x22
};

inline const char *prop_type_name(uint8_t t) {
    switch (t) {
        case PT_STRING: return "String";        case PT_BOOL: return "Bool";
        case PT_INT32: return "Int32";          case PT_FLOAT32: return "Float32";
        case PT_FLOAT64: return "Float64";      case PT_UDIM: return "UDim";
        case PT_UDIM2: return "UDim2";          case PT_RAY: return "Ray";
        case PT_FACES: return "Faces";          case PT_AXES: return "Axes";
        case PT_BRICKCOLOR: return "BrickColor";case PT_COLOR3: return "Color3";
        case PT_VECTOR2: return "Vector2";      case PT_VECTOR3: return "Vector3";
        case PT_VECTOR2INT16: return "Vector2int16";
        case PT_CFRAME: return "CFrame";        case PT_QUATERNION: return "Quaternion";
        case PT_ENUM: return "Enum";            case PT_REF: return "Ref";
        case PT_VECTOR3INT16: return "Vector3int16";
        case PT_NUMBERSEQUENCE: return "NumberSequence";
        case PT_COLORSEQUENCE: return "ColorSequence";
        case PT_NUMBERRANGE: return "NumberRange";
        case PT_RECT: return "Rect";
        case PT_PHYSICALPROPERTIES: return "PhysicalProperties";
        case PT_COLOR3UINT8: return "Color3uint8"; case PT_INT64: return "Int64";
        case PT_SHAREDSTRING: return "SharedString";
        case PT_OPTIONALCFRAME: return "OptionalCFrame";
        case PT_UNIQUEID: return "UniqueId";    case PT_FONT: return "Font";
        case PT_SECURITYCAPABILITIES: return "SecurityCapabilities";
        case PT_CONTENT: return "Content";
        default: return "Desconocido";
    }
}

// Las 24 orientaciones que Roblox codifica con un solo byte en vez de 9 floats.
// Matriz row-major: R00,R01,R02, R10,R11,R12, R20,R21,R22.
inline bool rot_id_matrix(uint8_t id, float *m) {
    static const int8_t T[24][10] = {
        {0x02, 1,0,0, 0,1,0, 0,0,1},    {0x03, 1,0,0, 0,0,-1, 0,1,0},
        {0x05, 1,0,0, 0,-1,0, 0,0,-1},  {0x06, 1,0,0, 0,0,1, 0,-1,0},
        {0x07, 0,1,0, 1,0,0, 0,0,-1},   {0x09, 0,0,1, 1,0,0, 0,1,0},
        {0x0A, 0,-1,0, 1,0,0, 0,0,1},   {0x0C, 0,0,-1, 1,0,0, 0,-1,0},
        {0x0D, 0,1,0, 0,0,1, 1,0,0},    {0x0E, 0,0,-1, 0,1,0, 1,0,0},
        {0x10, 0,-1,0, 0,0,-1, 1,0,0},  {0x11, 0,0,1, 0,-1,0, 1,0,0},
        {0x14, -1,0,0, 0,1,0, 0,0,-1},  {0x15, -1,0,0, 0,0,1, 0,1,0},
        {0x17, -1,0,0, 0,-1,0, 0,0,1},  {0x18, -1,0,0, 0,0,-1, 0,-1,0},
        {0x19, 0,1,0, -1,0,0, 0,0,1},   {0x1B, 0,0,-1, -1,0,0, 0,1,0},
        {0x1C, 0,-1,0, -1,0,0, 0,0,-1}, {0x1E, 0,0,1, -1,0,0, 0,-1,0},
        {0x1F, 0,1,0, 0,0,-1, -1,0,0},  {0x20, 0,0,1, 0,1,0, -1,0,0},
        {0x22, 0,-1,0, 0,0,1, -1,0,0},  {0x23, 0,0,-1, 0,-1,0, -1,0,0},
    };
    for (int k = 0; k < 24; k++) {
        if ((uint8_t)T[k][0] == id) {
            for (int j = 0; j < 9; j++) m[j] = (float)T[k][j + 1];
            return true;
        }
    }
    return false;
}

// Roblox guarda la rotacion row-major y sus COLUMNAS son los ejes X/Y/Z.
// El constructor Basis(x,y,z) de Godot toma columnas, asi que la conversion es
// directa. Ambos motores son Y-up con -Z hacia delante: no hay que voltear
// ejes ni escalar (1 stud = 1 unidad).
inline Transform3D cframe_to_transform(const float *m, const Vector3 &pos) {
    Basis b(Vector3(m[0], m[3], m[6]),
            Vector3(m[1], m[4], m[7]),
            Vector3(m[2], m[5], m[8]));
    return Transform3D(b, pos);
}

// Lee las N posiciones de un bloque de CFrames (van despues de las rotaciones).
inline void read_vector3_column(Reader &r, uint32_t count, LocalVector<Vector3> &out) {
    LocalVector<float> x, y, z;
    x.resize(count); y.resize(count); z.resize(count);
    r.read_f32_array(count, x.ptr());
    r.read_f32_array(count, y.ptr());
    r.read_f32_array(count, z.ptr());
    out.resize(count);
    for (uint32_t k = 0; k < count; k++) out[k] = Vector3(x[k], y[k], z[k]);
}

inline bool read_cframe_column(Reader &r, uint32_t count, LocalVector<Transform3D> &out) {
    LocalVector<float> mats;
    mats.resize((int64_t)count * 9);
    for (uint32_t k = 0; k < count; k++) {
        uint8_t rid = r.u8();
        if (!r.ok()) return false;
        float *m = mats.ptr() + (int64_t)k * 9;
        if (rid == 0) {
            for (int j = 0; j < 9; j++) m[j] = r.f32le();
        } else if (!rot_id_matrix(rid, m)) {
            return false;   // orientacion desconocida: mejor saltar la columna
        }
        if (!r.ok()) return false;
    }
    LocalVector<Vector3> pos;
    read_vector3_column(r, count, pos);
    if (!r.ok()) return false;
    out.resize(count);
    for (uint32_t k = 0; k < count; k++)
        out[k] = cframe_to_transform(mats.ptr() + (int64_t)k * 9, pos[k]);
    return true;
}

inline bool decode_string_column(Reader &r, uint32_t count, Vector<Variant> &out);
inline bool decode_content_column(Reader &r, uint32_t count, Vector<Variant> &out);

// Decodifica la columna completa de una propiedad.
// Para PT_REF los valores salen como int (el referent crudo); el importador los
// resuelve en una segunda pasada, cuando ya existen todos los nodos.
inline bool decode_column(Reader &r, uint8_t type_id, uint32_t count,
                          const Vector<String> &shared, Vector<Variant> &out) {
    out.clear();
    if (count == 0) return true;

    // Cota de cordura: ningun tipo ocupa menos de 1 byte por valor, asi que un
    // count corrupto no puede provocar una reserva gigante.
    if ((int64_t)count > r.left() * 64 + 1024) return false;

    if (type_id == PT_STRING)  return decode_string_column(r, count, out);
    if (type_id == PT_CONTENT) return decode_content_column(r, count, out);

    out.resize((int)count);

    switch (type_id) {

    case PT_BOOL: {
        for (uint32_t k = 0; k < count; k++) out.write[k] = (r.u8() != 0);
        break;
    }

    case PT_INT32: {
        LocalVector<int32_t> v; v.resize(count);
        r.read_i32_array(count, v.ptr());
        for (uint32_t k = 0; k < count; k++) out.write[k] = (int64_t)v[k];
        break;
    }

    case PT_FLOAT32: {
        LocalVector<float> v; v.resize(count);
        r.read_f32_array(count, v.ptr());
        for (uint32_t k = 0; k < count; k++) out.write[k] = (double)v[k];
        break;
    }

    case PT_FLOAT64: {   // little-endian crudo, NO entrelazado
        for (uint32_t k = 0; k < count; k++) out.write[k] = r.f64le();
        break;
    }

    case PT_UDIM: {
        LocalVector<float> sc; sc.resize(count);
        LocalVector<int32_t> of; of.resize(count);
        r.read_f32_array(count, sc.ptr());
        r.read_i32_array(count, of.ptr());
        for (uint32_t k = 0; k < count; k++) out.write[k] = Vector2(sc[k], (float)of[k]);
        break;
    }

    case PT_UDIM2: {
        LocalVector<float> sx, sy; sx.resize(count); sy.resize(count);
        LocalVector<int32_t> ox, oy; ox.resize(count); oy.resize(count);
        r.read_f32_array(count, sx.ptr());
        r.read_f32_array(count, sy.ptr());
        r.read_i32_array(count, ox.ptr());
        r.read_i32_array(count, oy.ptr());
        // Vector4(scaleX, offsetX, scaleY, offsetY)
        for (uint32_t k = 0; k < count; k++)
            out.write[k] = Vector4(sx[k], (float)ox[k], sy[k], (float)oy[k]);
        break;
    }

    case PT_RAY: {
        for (uint32_t k = 0; k < count; k++) {
            float o[3], d[3];
            for (int j = 0; j < 3; j++) o[j] = r.f32le();
            for (int j = 0; j < 3; j++) d[j] = r.f32le();
            Dictionary dd;
            dd["Origin"] = Vector3(o[0], o[1], o[2]);
            dd["Direction"] = Vector3(d[0], d[1], d[2]);
            out.write[k] = dd;
        }
        break;
    }

    case PT_FACES:
    case PT_AXES: {
        for (uint32_t k = 0; k < count; k++) out.write[k] = (int64_t)r.u8();
        break;
    }

    case PT_BRICKCOLOR:
    case PT_ENUM: {
        LocalVector<uint32_t> v; v.resize(count);
        r.read_u32_array(count, v.ptr());
        for (uint32_t k = 0; k < count; k++) out.write[k] = (int64_t)v[k];
        break;
    }

    case PT_COLOR3: {
        LocalVector<float> cr, cg, cb;
        cr.resize(count); cg.resize(count); cb.resize(count);
        r.read_f32_array(count, cr.ptr());
        r.read_f32_array(count, cg.ptr());
        r.read_f32_array(count, cb.ptr());
        for (uint32_t k = 0; k < count; k++) out.write[k] = Color(cr[k], cg[k], cb[k]);
        break;
    }

    case PT_COLOR3UINT8: {   // tres arrays PLANOS de bytes, sin entrelazar
        LocalVector<uint8_t> cr, cg, cb;
        cr.resize(count); cg.resize(count); cb.resize(count);
        for (uint32_t k = 0; k < count; k++) cr[k] = r.u8();
        for (uint32_t k = 0; k < count; k++) cg[k] = r.u8();
        for (uint32_t k = 0; k < count; k++) cb[k] = r.u8();
        for (uint32_t k = 0; k < count; k++)
            out.write[k] = Color(cr[k] / 255.0f, cg[k] / 255.0f, cb[k] / 255.0f);
        break;
    }

    case PT_VECTOR2: {
        LocalVector<float> x, y; x.resize(count); y.resize(count);
        r.read_f32_array(count, x.ptr());
        r.read_f32_array(count, y.ptr());
        for (uint32_t k = 0; k < count; k++) out.write[k] = Vector2(x[k], y[k]);
        break;
    }

    case PT_VECTOR3: {
        LocalVector<Vector3> v;
        read_vector3_column(r, count, v);
        for (uint32_t k = 0; k < count; k++) out.write[k] = v[k];
        break;
    }

    case PT_VECTOR2INT16: {
        for (uint32_t k = 0; k < count; k++) {
            int16_t a = (int16_t)r.u16le(), b = (int16_t)r.u16le();
            out.write[k] = Vector2((float)a, (float)b);
        }
        break;
    }

    case PT_VECTOR3INT16: {
        for (uint32_t k = 0; k < count; k++) {
            int16_t a = (int16_t)r.u16le(), b = (int16_t)r.u16le(), c = (int16_t)r.u16le();
            out.write[k] = Vector3((float)a, (float)b, (float)c);
        }
        break;
    }

    case PT_CFRAME: {
        LocalVector<Transform3D> v;
        if (!read_cframe_column(r, count, v)) return false;
        for (uint32_t k = 0; k < count; k++) out.write[k] = v[k];
        break;
    }

    case PT_OPTIONALCFRAME: {
        if (r.u8() != PT_CFRAME) return false;
        LocalVector<Transform3D> v;
        if (!read_cframe_column(r, count, v)) return false;
        if (r.u8() != PT_BOOL) return false;
        for (uint32_t k = 0; k < count; k++) {
            bool has = (r.u8() != 0);
            out.write[k] = has ? Variant(v[k]) : Variant();
        }
        break;
    }

    case PT_REF: {
        LocalVector<int32_t> v; v.resize(count);
        r.read_ref_array(count, v.ptr());
        for (uint32_t k = 0; k < count; k++) out.write[k] = (int64_t)v[k];
        break;
    }

    case PT_NUMBERSEQUENCE: {
        for (uint32_t k = 0; k < count; k++) {
            uint32_t n = r.u32le();
            if (!r.ok() || (int64_t)n * 12 > r.left()) return false;
            Array keys;
            for (uint32_t j = 0; j < n; j++) {
                Dictionary kp;
                kp["Time"] = r.f32le();
                kp["Value"] = r.f32le();
                kp["Envelope"] = r.f32le();
                keys.push_back(kp);
            }
            out.write[k] = keys;
        }
        break;
    }

    case PT_COLORSEQUENCE: {
        for (uint32_t k = 0; k < count; k++) {
            uint32_t n = r.u32le();
            if (!r.ok() || (int64_t)n * 20 > r.left()) return false;
            Array keys;
            for (uint32_t j = 0; j < n; j++) {
                Dictionary kp;
                kp["Time"] = r.f32le();
                float cr = r.f32le(), cg = r.f32le(), cb = r.f32le();
                kp["Color"] = Color(cr, cg, cb);
                kp["Envelope"] = r.f32le();
                keys.push_back(kp);
            }
            out.write[k] = keys;
        }
        break;
    }

    case PT_NUMBERRANGE: {
        for (uint32_t k = 0; k < count; k++) {
            float a = r.f32le(), b = r.f32le();
            out.write[k] = Vector2(a, b);
        }
        break;
    }

    case PT_RECT: {
        LocalVector<float> a, b, c, d;
        a.resize(count); b.resize(count); c.resize(count); d.resize(count);
        r.read_f32_array(count, a.ptr());
        r.read_f32_array(count, b.ptr());
        r.read_f32_array(count, c.ptr());
        r.read_f32_array(count, d.ptr());
        for (uint32_t k = 0; k < count; k++)
            out.write[k] = Rect2(a[k], b[k], c[k] - a[k], d[k] - b[k]);
        break;
    }

    case PT_PHYSICALPROPERTIES: {
        // Layout NO confirmado: en los places de prueba con overrides reales
        // sobran bytes al final. Se decodifica lo seguro (todo "sin override")
        // y si queda cola sin consumir se descarta la columna entera.
        for (uint32_t k = 0; k < count; k++) {
            uint8_t ov = r.u8();
            if (!r.ok()) return false;
            if (ov == 1) {
                Dictionary d;
                d["Density"] = r.f32le();
                d["Friction"] = r.f32le();
                d["Elasticity"] = r.f32le();
                d["FrictionWeight"] = r.f32le();
                d["ElasticityWeight"] = r.f32le();
                out.write[k] = d;
            } else {
                out.write[k] = Variant();
            }
        }
        if (r.left() != 0) return false;
        break;
    }

    case PT_INT64: {
        LocalVector<int64_t> v; v.resize(count);
        r.read_i64_array(count, v.ptr());
        for (uint32_t k = 0; k < count; k++) out.write[k] = v[k];
        break;
    }

    case PT_SHAREDSTRING: {
        LocalVector<uint32_t> idx; idx.resize(count);
        r.read_u32_array(count, idx.ptr());
        for (uint32_t k = 0; k < count; k++) {
            out.write[k] = ((int)idx[k] < shared.size()) ? Variant(shared[idx[k]])
                                                         : Variant(String());
        }
        break;
    }

    // Internos de Studio: hay que LEERLOS para no desalinear el stream, pero el
    // valor no interesa.
    case PT_UNIQUEID: {
        LocalVector<uint8_t> tmp; tmp.resize((int64_t)count * 16);
        r.read_interleaved(count, 16, tmp.ptr());
        for (uint32_t k = 0; k < count; k++) out.write[k] = Variant();
        break;
    }

    case PT_SECURITYCAPABILITIES: {
        LocalVector<uint8_t> tmp; tmp.resize((int64_t)count * 8);
        r.read_interleaved(count, 8, tmp.ptr());
        for (uint32_t k = 0; k < count; k++) out.write[k] = Variant();
        break;
    }

    case PT_FONT: {
        for (uint32_t k = 0; k < count; k++) {
            Dictionary d;
            d["Family"] = r.str();
            d["Weight"] = (int64_t)r.u16le();
            d["Style"] = (int64_t)r.u8();
            r.str();                       // cached face id, no lo usamos
            out.write[k] = d;
        }
        break;
    }

    case PT_QUATERNION: {
        LocalVector<float> qx, qy, qz, qw;
        qx.resize(count); qy.resize(count); qz.resize(count); qw.resize(count);
        r.read_f32_array(count, qx.ptr());
        r.read_f32_array(count, qy.ptr());
        r.read_f32_array(count, qz.ptr());
        r.read_f32_array(count, qw.ptr());
        LocalVector<Vector3> pos;
        read_vector3_column(r, count, pos);
        if (!r.ok()) return false;
        for (uint32_t k = 0; k < count; k++)
            out.write[k] = Transform3D(Basis(Quaternion(qx[k], qy[k], qz[k], qw[k])), pos[k]);
        break;
    }

    default:
        return false;
    }

    return r.ok();
}

// Devuelve los INDICES crudos de una columna SharedString, sin resolverlos a
// String. Lo necesita la propiedad Tags, que empaqueta varios tags separados
// por \0 dentro de una sola entrada de la tabla.
inline bool decode_sharedstring_indices(Reader &r, uint32_t count, LocalVector<uint32_t> &out) {
    out.resize(count);
    if (count == 0) return true;
    r.read_u32_array(count, out.ptr());
    return r.ok();
}

// String y Content necesitan tratarse fuera del switch porque uno devuelve
// texto y el otro mezcla texto con referencias.
inline bool decode_string_column(Reader &r, uint32_t count, Vector<Variant> &out) {
    out.clear();
    out.resize((int)count);
    for (uint32_t k = 0; k < count; k++) {
        out.write[k] = r.str();
        if (!r.ok()) return false;
    }
    return true;
}

// 0x22 Content: tipo nuevo de Roblox (2025).
//   u32 tipo_fuente[n] entrelazado (0=None, 1=Uri, 2=Object)
//   u32 n_uris + n_uris cadenas
//   u32 n_objetos + referents
inline bool decode_content_column(Reader &r, uint32_t count, Vector<Variant> &out) {
    out.clear();
    out.resize((int)count);
    LocalVector<uint32_t> kinds; kinds.resize(count);
    r.read_u32_array(count, kinds.ptr());
    if (!r.ok()) return false;

    uint32_t nuri = r.u32le();
    if (!r.ok() || (int64_t)nuri * 4 > r.left()) return false;
    Vector<String> uris;
    for (uint32_t k = 0; k < nuri; k++) uris.push_back(r.str());
    if (!r.ok()) return false;

    uint32_t nobj = r.u32le();
    LocalVector<int32_t> objs;
    if (nobj > 0) {
        if ((int64_t)nobj * 4 > r.left()) return false;
        objs.resize(nobj);
        r.read_ref_array(nobj, objs.ptr());
    }
    if (!r.ok()) return false;

    uint32_t ui = 0, oi = 0;
    for (uint32_t k = 0; k < count; k++) {
        if (kinds[k] == 1 && (int)ui < uris.size())      out.write[k] = uris[ui++];
        else if (kinds[k] == 2 && oi < objs.size())      out.write[k] = (int64_t)objs[oi++];
        else                                             out.write[k] = String();
    }
    return true;
}

} // namespace rbx

#endif // RBX_PROP_H
