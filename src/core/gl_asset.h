#ifndef GL_ASSET_H
#define GL_ASSET_H

// Ids de assets que viven en la nube de Roblox (rbxassetid:// y las URLs de
// roblox.com). NO son rutas de Godot: pasarlos por ResourceLoader suelta dos
// errores rojos por cada uno y, al abrir un place importado con cientos de
// sonidos o imagenes, la consola queda inservible.
//
// El id se conserva igual en el nodo (metadato __rbx_missing_asset) y el
// importador de .rbxl los lista todos en su reporte, para poder exportar el
// asset desde Studio y enchufarlo a mano.

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/classes/resource_loader.hpp>

using namespace godot;

inline bool gl_is_roblox_asset(const String& id) {
    return id.begins_with("rbxassetid://")               ||
           id.begins_with("rbxasset://")                 ||
           id.begins_with("http://www.roblox.com/asset") ||
           id.begins_with("https://www.roblox.com/asset")||
           id.begins_with("http://roblox.com/asset")     ||
           id.begins_with("https://roblox.com/asset");
}

// ── Sustitucion LOCAL de assets de la nube ───────────────────────────
// Las imagenes y sonidos de un place viven en la nube de Roblox y no se pueden
// descargar, asi que las UIs importadas salian con huecos en blanco. Con esto se
// pueden "enchufar" a mano: basta guardar el archivo con el NUMERO del id como
// nombre en
//        res://GodotLuau/assets/rbx/<id>.<ext>
// y cualquier ImageLabel, ImageButton, Decal, Sound o Animation que use ese id lo
// encontrara solo. Ejemplo: rbxassetid://1234567 -> assets/rbx/1234567.png
// Asi se exporta el asset desde Studio (o se pone otro propio) y la UI del place
// aparece con su aspecto real, sin tocar ni un script del juego.
inline String gl_asset_id_number(const String& id) {
    // Se queda con el ULTIMO tramo numerico (sirve para rbxassetid://123 y para
    // las URLs tipo .../asset/?id=123).
    String digits;
    for (int i = 0; i < id.length(); i++) {
        const char32_t c = id[i];
        if (c >= '0' && c <= '9') digits += String::chr(c);
        else if (!digits.is_empty()) digits = "";
    }
    return digits;
}

inline String gl_local_asset_path(const String& id) {
    const String num = gl_asset_id_number(id);
    if (num.is_empty()) return String();
    ResourceLoader* rl = ResourceLoader::get_singleton();
    if (!rl) return String();
    static const char* kExts[] = { ".png", ".jpg", ".jpeg", ".webp", ".svg",
                                   ".ogg", ".wav", ".mp3", ".tres", ".res" };
    for (int i = 0; i < 10; i++) {
        const String p = String("res://GodotLuau/assets/rbx/") + num + kExts[i];
        if (rl->exists(p)) return p;
    }
    return String();
}

#endif // GL_ASSET_H
