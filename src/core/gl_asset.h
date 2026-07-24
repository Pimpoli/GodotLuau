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

using namespace godot;

inline bool gl_is_roblox_asset(const String& id) {
    return id.begins_with("rbxassetid://")               ||
           id.begins_with("rbxasset://")                 ||
           id.begins_with("http://www.roblox.com/asset") ||
           id.begins_with("https://www.roblox.com/asset")||
           id.begins_with("http://roblox.com/asset")     ||
           id.begins_with("https://roblox.com/asset");
}

#endif // GL_ASSET_H
