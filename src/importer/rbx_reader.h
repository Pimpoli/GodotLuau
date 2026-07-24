#ifndef RBX_READER_H
#define RBX_READER_H

// Lector del contenedor binario .rbxl / .rbxm de Roblox.
// Estructura: header de 32 bytes + chunks (META, SSTR, INST, PROP, PRNT, END).
// Cada chunk va comprimido con ZSTD (archivos modernos) o LZ4 (antiguos).
//
// Los layouts implementados aqui estan verificados byte-exactos contra un place
// real de 38562 instancias: 2995 de sus 3002 columnas se consumen sin sobrar ni
// faltar un solo byte.

#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/templates/local_vector.hpp>

#include <string.h>
#include <stdint.h>

using namespace godot;

namespace rbx {

// Varias propiedades "de texto" (ModelMeshData, PhysicsData, SlimHash...) son en
// realidad blobs binarios. Convertirlos con String::utf8 llena la consola de
// avisos de Unicode, asi que primero se comprueba si el contenido es texto.
inline bool is_valid_utf8(const uint8_t *p, int64_t len) {
    int64_t i = 0;
    while (i < len) {
        uint8_t c = p[i];
        int extra;
        if (c < 0x80) { i++; continue; }
        else if ((c & 0xE0) == 0xC0) { extra = 1; if ((c & 0xFE) == 0xC0) return false; }
        else if ((c & 0xF0) == 0xE0) extra = 2;
        else if ((c & 0xF8) == 0xF0) extra = 3;
        else return false;
        if (i + extra >= len) return false;
        for (int k = 1; k <= extra; k++)
            if ((p[i + k] & 0xC0) != 0x80) return false;
        i += extra + 1;
    }
    return true;
}

// Texto -> String; binario -> String vacia (el dato crudo sigue disponible en
// shared_raw para quien lo necesite, como hace la propiedad Tags).
inline String bytes_to_string(const uint8_t *p, int64_t len) {
    if (len <= 0) return String();
    if (!is_valid_utf8(p, len)) return String();
    return String::utf8((const char *)p, (int)len);
}

// ── LZ4 block format ────────────────────────────────────────────────
// Godot no expone LZ4 (solo FastLZ/Deflate/Zstd/Gzip/Brotli), asi que va aqui.
// Comprueba limites en cada lectura y escritura: un archivo corrupto debe
// devolver false, nunca desbordar.
inline bool lz4_decompress_block(const uint8_t *src, int64_t src_len,
                                 uint8_t *dst, int64_t dst_len) {
    if (!src || !dst || src_len < 0 || dst_len < 0) return false;
    int64_t s = 0, d = 0;
    while (s < src_len) {
        uint8_t token = src[s++];
        int64_t lit = token >> 4;
        if (lit == 15) {
            uint8_t b;
            do {
                if (s >= src_len) return false;
                b = src[s++];
                lit += b;
                if (lit > dst_len) return false;
            } while (b == 255);
        }
        if (s + lit > src_len || d + lit > dst_len) return false;
        memcpy(dst + d, src + s, (size_t)lit);
        s += lit; d += lit;

        // El ultimo bloque termina en literales, sin match.
        if (s >= src_len) break;
        if (s + 2 > src_len) return false;
        int64_t off = (int64_t)src[s] | ((int64_t)src[s + 1] << 8);
        s += 2;
        if (off == 0 || off > d) return false;

        int64_t ml = token & 0xF;
        if (ml == 15) {
            uint8_t b;
            do {
                if (s >= src_len) return false;
                b = src[s++];
                ml += b;
                if (ml > dst_len) return false;
            } while (b == 255);
        }
        ml += 4;
        if (d + ml > dst_len) return false;
        // Copia byte a byte: los matches pueden solaparse (off < ml).
        int64_t from = d - off;
        for (int64_t k = 0; k < ml; k++) dst[d + k] = dst[from + k];
        d += ml;
    }
    return true;
}

struct Chunk {
    String name;
    PackedByteArray data;   // ya descomprimido
};

// clen == 0 -> los datos van en crudo.
// magic 28 B5 2F FD -> ZSTD. Cualquier otra cosa -> LZ4 block.
inline bool decompress_chunk(const uint8_t *src, int64_t clen, int64_t ulen,
                             PackedByteArray &out) {
    if (ulen < 0) return false;
    if (clen == 0) {
        out.resize(ulen);
        if (ulen > 0) memcpy(out.ptrw(), src, (size_t)ulen);
        return true;
    }
    if (clen < 4) return false;

    if (src[0] == 0x28 && src[1] == 0xB5 && src[2] == 0x2F && src[3] == 0xFD) {
        PackedByteArray in;
        in.resize(clen);
        memcpy(in.ptrw(), src, (size_t)clen);
        out = in.decompress(ulen, FileAccess::COMPRESSION_ZSTD);
        return out.size() == ulen;
    }

    out.resize(ulen);
    return lz4_decompress_block(src, clen, out.ptrw(), ulen);
}

// ── Lector de bytes con comprobacion de limites ─────────────────────
// Cualquier lectura fuera de rango marca el reader como fallido y devuelve 0,
// de modo que el error se propaga sin necesidad de comprobar cada llamada.
class Reader {
    const uint8_t *b = nullptr;
    int64_t n = 0;
    int64_t i = 0;
    bool good = true;

    bool need(int64_t k) {
        if (!good || k < 0 || i + k > n) { good = false; return false; }
        return true;
    }

public:
    Reader(const uint8_t *p_data, int64_t p_size) : b(p_data), n(p_size) {}

    bool ok() const { return good; }
    void fail() { good = false; }
    int64_t pos() const { return i; }
    int64_t size() const { return n; }
    int64_t left() const { return good ? n - i : 0; }
    void skip(int64_t k) { if (need(k)) i += k; }

    uint8_t u8() { if (!need(1)) return 0; return b[i++]; }

    uint16_t u16le() {
        if (!need(2)) return 0;
        uint16_t v; memcpy(&v, b + i, 2); i += 2; return v;
    }
    uint32_t u32le() {
        if (!need(4)) return 0;
        uint32_t v; memcpy(&v, b + i, 4); i += 4; return v;
    }
    int32_t i32le() { return (int32_t)u32le(); }
    int64_t i64le() {
        if (!need(8)) return 0;
        int64_t v; memcpy(&v, b + i, 8); i += 8; return v;
    }
    float f32le() {
        if (!need(4)) return 0.0f;
        float v; memcpy(&v, b + i, 4); i += 4; return v;
    }
    double f64le() {
        if (!need(8)) return 0.0;
        double v; memcpy(&v, b + i, 8); i += 8; return v;
    }

    // u32 longitud + bytes. Devuelve String vacia si el contenido es binario.
    String str() {
        uint32_t len = u32le();
        if (!need((int64_t)len)) return String();
        String s = bytes_to_string(b + i, (int64_t)len);
        i += len;
        return s;
    }

    PackedByteArray blob() {
        uint32_t len = u32le();
        PackedByteArray out;
        if (!need((int64_t)len)) return out;
        out.resize(len);
        if (len > 0) memcpy(out.ptrw(), b + i, len);
        i += len;
        return out;
    }

    // Bytes entrelazados: para 'count' valores de 'width' bytes, el byte 'k' de
    // todos los valores va junto. Cada valor se interpreta BIG-ENDIAN.
    // out debe tener count*width bytes; queda en orden normal (valor a valor).
    void read_interleaved(uint32_t count, uint32_t width, uint8_t *out) {
        int64_t total = (int64_t)count * (int64_t)width;
        if (!need(total)) return;
        const uint8_t *src = b + i;
        for (uint32_t v = 0; v < count; v++)
            for (uint32_t k = 0; k < width; k++)
                out[(int64_t)v * width + k] = src[(int64_t)k * count + v];
        i += total;
    }

    void read_u32_array(uint32_t count, uint32_t *out) {
        if (count == 0) return;
        LocalVector<uint8_t> tmp;
        tmp.resize((int64_t)count * 4);
        read_interleaved(count, 4, tmp.ptr());
        if (!good) return;
        for (uint32_t v = 0; v < count; v++) {
            const uint8_t *p = tmp.ptr() + (int64_t)v * 4;
            out[v] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                     ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
        }
    }

    void read_i32_array(uint32_t count, int32_t *out) {
        if (count == 0) return;
        LocalVector<uint32_t> raw;
        raw.resize(count);
        read_u32_array(count, raw.ptr());
        if (!good) return;
        for (uint32_t v = 0; v < count; v++) {
            uint32_t u = raw[v];
            out[v] = (int32_t)((u >> 1) ^ (~(u & 1) + 1));  // zigzag
        }
    }

    void read_f32_array(uint32_t count, float *out) {
        if (count == 0) return;
        LocalVector<uint32_t> raw;
        raw.resize(count);
        read_u32_array(count, raw.ptr());
        if (!good) return;
        for (uint32_t v = 0; v < count; v++) {
            // Roblox rota a la izquierda al escribir: al leer, a la derecha.
            uint32_t u = raw[v];
            u = (u >> 1) | (u << 31);
            float f; memcpy(&f, &u, 4);
            out[v] = f;
        }
    }

    void read_i64_array(uint32_t count, int64_t *out) {
        if (count == 0) return;
        LocalVector<uint8_t> tmp;
        tmp.resize((int64_t)count * 8);
        read_interleaved(count, 8, tmp.ptr());
        if (!good) return;
        for (uint32_t v = 0; v < count; v++) {
            const uint8_t *p = tmp.ptr() + (int64_t)v * 8;
            uint64_t u = 0;
            for (int k = 0; k < 8; k++) u = (u << 8) | p[k];
            out[v] = (int64_t)((u >> 1) ^ (~(u & 1) + 1));  // zigzag 64
        }
    }

    // Los referents van en zigzag Y ademas acumulados respecto al anterior.
    void read_ref_array(uint32_t count, int32_t *out) {
        if (count == 0) return;
        read_i32_array(count, out);
        if (!good) return;
        int32_t acc = 0;
        for (uint32_t v = 0; v < count; v++) { acc += out[v]; out[v] = acc; }
    }
};

struct RBXFile {
    uint32_t class_count = 0;
    uint32_t instance_count = 0;
    Vector<Chunk> chunks;
    Vector<String> shared_strings;          // tabla SSTR, en orden de indice
    // Los mismos datos sin convertir: la propiedad Tags empaqueta varios tags
    // separados por \0, y un String de Godot se cortaria en el primero.
    Vector<PackedByteArray> shared_raw;
};

inline bool parse_container(const PackedByteArray &bytes, RBXFile &out, String &err) {
    const int64_t total = bytes.size();
    if (total < 32) { err = "Archivo demasiado corto para ser un .rbxl"; return false; }

    const uint8_t *d = bytes.ptr();
    static const uint8_t MAGIC[8] = { '<','r','o','b','l','o','x','!' };
    if (memcmp(d, MAGIC, 8) != 0) {
        err = "No es un .rbxl binario (falta la firma <roblox!). "
              "Si es un .rbxlx en XML, hace falta el lector de XML.";
        return false;
    }

    memcpy(&out.class_count,    d + 0x10, 4);
    memcpy(&out.instance_count, d + 0x14, 4);

    int64_t pos = 0x20;
    while (pos + 16 <= total) {
        char name[5] = {0};
        memcpy(name, d + pos, 4);
        uint32_t clen, ulen;
        memcpy(&clen, d + pos + 4, 4);
        memcpy(&ulen, d + pos + 8, 4);
        pos += 16;

        int64_t payload_len = (clen == 0) ? (int64_t)ulen : (int64_t)clen;
        if (pos + payload_len > total) { err = "Chunk truncado"; return false; }

        Chunk c;
        c.name = String(name).strip_edges();
        if (!decompress_chunk(d + pos, clen, ulen, c.data)) {
            err = String("Fallo al descomprimir el chunk ") + c.name;
            return false;
        }
        pos += payload_len;

        bool is_end = (name[0] == 'E' && name[1] == 'N' && name[2] == 'D');
        out.chunks.push_back(c);
        if (is_end) break;
    }

    // Tabla de cadenas compartidas (SSTR): 16 bytes de hash + string por entrada.
    for (int k = 0; k < out.chunks.size(); k++) {
        const Chunk &c = out.chunks[k];
        if (c.name != "SSTR") continue;
        Reader r(c.data.ptr(), c.data.size());
        r.u32le();                       // version
        uint32_t count = r.u32le();
        if (count > 1000000) { err = "SSTR con un numero de entradas absurdo"; return false; }
        for (uint32_t s = 0; s < count && r.ok(); s++) {
            r.skip(16);                  // hash md5, no lo usamos
            PackedByteArray raw = r.blob();
            out.shared_raw.push_back(raw);
            out.shared_strings.push_back(bytes_to_string(raw.ptr(), raw.size()));
        }
        break;
    }

    return true;
}

} // namespace rbx

#endif // RBX_READER_H
