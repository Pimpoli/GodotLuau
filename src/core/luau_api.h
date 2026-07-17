#ifndef LUAU_API_H
#define LUAU_API_H

#include "gl_errors.h"
#include "folder.h"
#include "humanoid.h"
#include "humanoid2d.h"
#include "roblox_player.h"
#include "roblox_player2d.h"
#include "roblox_part.h"
#include "roblox_terrain.h"
#include "roblox_services.h"
#include "roblox_network.h"
#include "roblox_remote.h"
#include "roblox_workspace.h"
#include "roblox_bodymovers.h"
#include "roblox_constraints.h"
#include "roblox_gui.h"
#include "roblox_tween.h"
#include "roblox_sound.h"
#include "roblox_interactive.h"
#include "roblox_billboard.h"
#include "roblox_input.h"
#include "roblox_animation.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/spot_light3d.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include "lua.h"
#include "lualib.h"
#include "Luau/Compiler.h"

#include "gl_runtime.h"   // GodotObjectWrapper (ObjectID), gow_node/gow_set, registro de estados vivos

#include <cmath>
#include <cstring>
#include <functional>

using namespace godot;

// ════════════════════════════════════════════════════════════════════
//  Data structures for Luau types
////
//  Estructuras de datos para los tipos Luau
//  (GodotObjectWrapper vive en gl_runtime.h: guarda ObjectID, no Node* crudo)
// ════════════════════════════════════════════════════════════════════

struct LuauVector3 {
    float x, y, z;
    float magnitude() const { return std::sqrt(x*x + y*y + z*z); }
    LuauVector3 unit() const {
        float m = magnitude();
        if (m < 0.0001f) return {0.0f, 0.0f, 0.0f};
        return {x/m, y/m, z/m};
    }
    LuauVector3 lerp(const LuauVector3& to, float alpha) const {
        return {x + (to.x - x)*alpha, y + (to.y - y)*alpha, z + (to.z - z)*alpha};
    }
    float dot(const LuauVector3& b) const { return x*b.x + y*b.y + z*b.z; }
    LuauVector3 cross(const LuauVector3& b) const {
        return {y*b.z - z*b.y, z*b.x - x*b.z, x*b.y - y*b.x};
    }
};

struct LuauColor3 { float r, g, b; };
struct LuauCFrame { Transform3D transform; };

// ════════════════════════════════════════════════════════════════════
//  Internal helpers
////
//  Helpers internos
// ════════════════════════════════════════════════════════════════════

static void wrap_node(lua_State* L, Node* node) {
    if (!node) { lua_pushnil(L); return; }
    GodotObjectWrapper* wrap = (GodotObjectWrapper*)lua_newuserdata(L, sizeof(GodotObjectWrapper));
    gow_set(wrap, node);
    luaL_getmetatable(L, "GodotObject");
    lua_setmetatable(L, -2);
}

// Empuja un CFrame de POSICIÓN. Los CFrame son tablas tipadas creadas en Lua,
// así que se construye llamando al constructor global (1.14.10 Finish).
static void _gl_push_cframe_pos(lua_State* L, const Vector3& p) {
    lua_getglobal(L, "CFrame");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_pushnil(L); return; }
    lua_getfield(L, -1, "new");
    if (!lua_isfunction(L, -1)) { lua_pop(L, 2); lua_pushnil(L); return; }
    lua_pushnumber(L, p.x); lua_pushnumber(L, p.y); lua_pushnumber(L, p.z);
    if (lua_pcall(L, 3, 1, 0) != LUA_OK) { lua_pop(L, 2); lua_pushnil(L); return; }
    lua_remove(L, -2);   // quitar la tabla CFrame, dejar solo el resultado
}

static LuauVector3* push_vector3(lua_State* L, float x, float y, float z) {
    LuauVector3* v = (LuauVector3*)lua_newuserdata(L, sizeof(LuauVector3));
    v->x = x; v->y = y; v->z = z;
    luaL_getmetatable(L, "Vector3");
    lua_setmetatable(L, -2);
    return v;
}

static LuauColor3* push_color3(lua_State* L, float r, float g, float b) {
    LuauColor3* c = (LuauColor3*)lua_newuserdata(L, sizeof(LuauColor3));
    c->r = r; c->g = g; c->b = b;
    luaL_getmetatable(L, "Color3");
    lua_setmetatable(L, -2);
    return c;
}

// ── Comprobación de tipo segura: solo devuelve el puntero si el userdata
//    tiene EXACTAMENTE la metatable esperada. Evita reinterpretar un tipo
//    por otro (p.ej. pasar un Color3 donde se espera un Vector3, o un nodo
//    donde se espera un Vector3 → lectura fuera de límites). ────────────────
static inline bool _gl_has_metatable(lua_State* L, int idx, const char* mt) {
    if (lua_type(L, idx) != LUA_TUSERDATA) return false;
    if (!lua_getmetatable(L, idx)) return false;
    luaL_getmetatable(L, mt);
    bool ok = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);
    return ok;
}
static inline LuauVector3* check_vector3(lua_State* L, int idx) {
    return _gl_has_metatable(L, idx, "Vector3") ? (LuauVector3*)lua_touserdata(L, idx) : nullptr;
}
static inline LuauColor3* check_color3(lua_State* L, int idx) {
    return _gl_has_metatable(L, idx, "Color3") ? (LuauColor3*)lua_touserdata(L, idx) : nullptr;
}

// ════════════════════════════════════════════════════════════════════
//  Puente genérico de propiedades (clases nuevas con meta "_gl_bridge")
//  Permite leer/escribir cualquier propiedad registrada en el ClassDB con
//  nombre estilo Roblox (ADD_PROPERTY) sin cablearla a mano en el index/
//  newindex. Solo se activa en nodos que ponen set_meta("_gl_bridge", true)
//  en su constructor, así que NO afecta a las clases existentes.
// ════════════════════════════════════════════════════════════════════
static bool gl_has_property(Object* o, const String& pname) {
    if (!o) return false;
    TypedArray<Dictionary> pl = o->get_property_list();
    for (int i = 0; i < pl.size(); i++) {
        Dictionary d = pl[i];
        if (String(d.get("name", String())) == pname) return true;
    }
    return false;
}
static bool gl_luau_to_variant(lua_State* L, int idx, Variant& out) {
    switch (lua_type(L, idx)) {
        case LUA_TNUMBER:  out = (double)lua_tonumber(L, idx); return true;
        case LUA_TBOOLEAN: out = (bool)(lua_toboolean(L, idx) != 0); return true;
        case LUA_TSTRING:  out = String(lua_tostring(L, idx)); return true;
        case LUA_TUSERDATA:
            if (LuauVector3* v = check_vector3(L, idx)) { out = Vector3(v->x, v->y, v->z); return true; }
            if (LuauColor3*  c = check_color3(L, idx))  { out = Color(c->r, c->g, c->b);   return true; }
            return false;
        default: return false;
    }
}
static bool gl_variant_to_luau(lua_State* L, const Variant& v) {
    switch (v.get_type()) {
        case Variant::BOOL:    lua_pushboolean(L, (bool)v);              return true;
        case Variant::INT:     lua_pushnumber(L, (double)(int64_t)v);    return true;
        case Variant::FLOAT:   lua_pushnumber(L, (double)v);             return true;
        case Variant::STRING:  { String s = v; lua_pushstring(L, s.utf8().get_data()); return true; }
        case Variant::VECTOR3: { Vector3 q = v; push_vector3(L, q.x, q.y, q.z); return true; }
        case Variant::COLOR:   { Color q = v; push_color3(L, q.r, q.g, q.b);    return true; }
        default: return false;
    }
}

// Señal "inerte": objeto con Connect/Once/Wait que NO hace nada y NO filtra
// referencias. Se usa para señales aún no soportadas (antes conectaban a
// métodos inexistentes y acumulaban refs sin liberarse nunca).
static inline void _gl_push_inert_signal(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, [](lua_State* pL) -> int {
        // Devuelve una conexión con Disconnect()/Connected = false
        lua_newtable(pL);
        lua_pushcfunction(pL, [](lua_State*) -> int { return 0; }, "Disconnect");
        lua_setfield(pL, -2, "Disconnect");
        lua_pushboolean(pL, 0); lua_setfield(pL, -2, "Connected");
        return 1;
    }, "Connect");
    lua_setfield(L, -2, "Connect");
    lua_pushcfunction(L, [](lua_State* pL) -> int { return 0; }, "Once");
    lua_setfield(L, -2, "Once");
    lua_pushcfunction(L, [](lua_State* pL) -> int { lua_pushnil(pL); return 1; }, "Wait");
    lua_setfield(L, -2, "Wait");
}

// Devuelve un objeto conexión estilo Roblox: { Disconnect(), Connected }.
// Disconnect resuelve el nodo por ObjectID (seguro tras destruir) y llama al
// método _gl_disconnect(ref) de la clase, que desactiva ese callback.
static void _gl_push_connection(lua_State* L, Node* node, int ref) {
    lua_newtable(L);
    lua_pushlightuserdata(L, (void*)(uintptr_t)(node ? (uint64_t)node->get_instance_id() : 0));
    lua_pushinteger(L, ref);
    lua_pushcclosure(L, [](lua_State* dL) -> int {
        uint64_t id = (uint64_t)(uintptr_t)lua_touserdata(dL, lua_upvalueindex(1));
        int r = (int)lua_tointeger(dL, lua_upvalueindex(2));
        Node* n = Object::cast_to<Node>(ObjectDB::get_instance(id));
        if (n && n->has_method("_gl_disconnect")) n->call("_gl_disconnect", r);
        return 0;
    }, "Disconnect", 2);
    lua_setfield(L, -2, "Disconnect");
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "Connected");
}

// ── ClassName / IsA estilo Roblox ────────────────────────────────────────────
// Devuelve el nombre de clase Roblox a partir de la clase Godot interna.
static const char* gl_roblox_classname(Node* n) {
    if (!n) return "Instance";
    String c = n->get_class();
    struct M { const char* g; const char* r; };
    static const M map[] = {
        {"RobloxPart","Part"},{"Folder","Folder"},{"Humanoid","Humanoid"},{"Humanoid2D","Humanoid"},
        {"RobloxPlayer","Player"},{"RobloxPlayer2D","Player"},
        {"OmniLight3D","PointLight"},{"SpotLight3D","SpotLight"},{"DirectionalLight3D","DirectionalLight"},
        {"RemoteEventNode","RemoteEvent"},{"RemoteFunctionNode","RemoteFunction"},{"BindableEventNode","BindableEvent"},
        {"RobloxSound","Sound"},{"RobloxSoundGroup","SoundGroup"},{"RobloxTween","Tween"},{"TweenService","TweenService"},
        {"ScreenGui","ScreenGui"},{"RobloxFrame","Frame"},{"RobloxTextLabel","TextLabel"},
        {"RobloxTextButton","TextButton"},{"RobloxTextBox","TextBox"},{"RobloxImageLabel","ImageLabel"},
        {"RobloxScrollingFrame","ScrollingFrame"},{"ServerScript","Script"},{"LocalScript","LocalScript"},
        {"ModuleScript","ModuleScript"},{"RobloxTool","Tool"},{"ClickDetector","ClickDetector"},
        {"ProximityPrompt","ProximityPrompt"},{"SpawnLocation","SpawnLocation"},{"BillboardGui","BillboardGui"},
        {"SurfaceGui","SurfaceGui"},{"Motor6D","Motor6D"},{"AnimationObject","Animation"},{"AnimationTrack","AnimationTrack"},
        {"RobloxWorkspace","Workspace"},{"RobloxWorkspace2D","Workspace"},{"RobloxChat","TextChatService"},
        {"RobloxTerrain","Terrain"},{"RobloxValue","NumberValue"},{"PlayerObject","Player"},
        {"BodyVelocity","BodyVelocity"},{"BodyPosition","BodyPosition"},{"BodyForce","BodyForce"},
        {"BodyAngularVelocity","BodyAngularVelocity"},{"BodyGyro","BodyGyro"},
        {"WeldConstraint","WeldConstraint"},{"HingeConstraint","HingeConstraint"},
        {"BallSocketConstraint","BallSocketConstraint"},{"RodConstraint","RodConstraint"},{"SpringConstraint","SpringConstraint"},
        {nullptr,nullptr}
    };
    for (int i = 0; map[i].g; i++) if (c == map[i].g) return map[i].r;
    // Servicios / nodos no mapeados conservan su nombre (Players, Lighting, ReplicatedStorage...)
    static CharString buf; buf = c.utf8(); return buf.get_data();
}

// IsA estilo Roblox: nombre exacto + jerarquía pragmática + compat con clase Godot.
static bool gl_isa(Node* n, const char* q) {
    if (!n || !q) return false;
    if (strcmp(q, "Instance") == 0) return true;          // todo es Instance
    const char* rc = gl_roblox_classname(n);
    if (strcmp(rc, q) == 0) return true;
    if (strcmp(rc, "Part") == 0 && (strcmp(q,"BasePart")==0 || strcmp(q,"PVInstance")==0)) return true;
    bool gui = (strcmp(rc,"Frame")==0||strcmp(rc,"TextLabel")==0||strcmp(rc,"TextButton")==0||
                strcmp(rc,"TextBox")==0||strcmp(rc,"ImageLabel")==0||strcmp(rc,"ScrollingFrame")==0);
    if (gui && (strcmp(q,"GuiObject")==0 || strcmp(q,"GuiBase2d")==0)) return true;
    if ((strcmp(rc,"Script")==0||strcmp(rc,"LocalScript")==0||strcmp(rc,"ModuleScript")==0)
        && strcmp(q,"LuaSourceContainer")==0) return true;
    // Compat: aceptar también el nombre de clase Godot (IsA("RobloxPart"), IsA("Node")...)
    if (n->is_class(q)) return true;
    return false;
}

// ════════════════════════════════════════════════════════════════════
//  Instance methods (accessible from Lua)
////
//  Métodos de Instance (accesibles desde Lua)
// ════════════════════════════════════════════════════════════════════

static int method_destroy(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    Node* n = w ? gow_node(w) : nullptr;
    if (n) {
        // Replicación (1.14.5): si somos servidor y la instancia está replicada,
        // avisar a los clientes ANTES de liberarla aquí.
        if (gl_net_role() == 1 && n->has_meta("_gl_netid")) {
            if (NetworkService* ns = Object::cast_to<NetworkService>(ObjectDB::get_instance(gl_net_service_id())))
                ns->rep_unreplicate(n);   // avisa a los clientes + olvida el subárbol (recursivo)
        }
        // Como Roblox: la instancia desaparece del árbol de inmediato y se
        // libera la memoria. Sacarla del padre primero garantiza que deje de
        // verse/colisionar al instante (queue_free por sí solo difiere al final
        // del frame).
        Node* parent = n->get_parent();
        if (parent) parent->remove_child(n);
        n->queue_free();
    }
    return 0;
}

// Quita el netId de replicación de un nodo y todo su subárbol. Node::duplicate()
// copia la metadata (props STORAGE), así que un :Clone() en el servidor heredaría
// el "_gl_netid" del original y le secuestraría la identidad de red. Los shadow
// "_glrep_*" SÍ se conservan: sirven al snapshot cuando el clon se replique.
static void gl_strip_netid_recursive(Node* n) {
    if (!n) return;
    if (n->has_meta("_gl_netid")) n->remove_meta("_gl_netid");
    for (int i = 0; i < n->get_child_count(); i++) gl_strip_netid_recursive(n->get_child(i));
}

static int method_clone(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    if (w && gow_node(w)) {
        Node* cloned = gow_node(w)->duplicate();
        if (cloned) {
            gl_strip_netid_recursive(cloned);   // el clon nace SIN netId (se le asigna uno nuevo al replicarse)
            wrap_node(L, cloned); return 1;
        }
    }
    lua_pushnil(L); return 1;
}

static int method_clearallchildren(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    if (w && gow_node(w)) {
        // Replicación (1.14.5): en el servidor, avisar a los clientes que destruyan
        // cada hijo replicado (ClearAllChildren no pasa por :Destroy()).
        NetworkService* rns = (gl_net_role() == 1)
            ? Object::cast_to<NetworkService>(ObjectDB::get_instance(gl_net_service_id())) : nullptr;
        TypedArray<Node> ch = gow_node(w)->get_children();
        for (int i = 0; i < ch.size(); i++) {
            Node* n = Object::cast_to<Node>(ch[i]);
            if (!n) continue;
            if (rns && n->has_meta("_gl_netid")) rns->rep_unreplicate(n);
            n->queue_free();
        }
    }
    return 0;
}

static int method_findfirstchild(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    const char* name = luaL_checkstring(L, 2);
    bool recursive  = lua_isboolean(L, 3) && lua_toboolean(L, 3);
    if (!w || !gow_node(w)) { lua_pushnil(L); return 1; }
    if (!recursive) {
        // Direct children only — Roblox default
        for (int i = 0; i < gow_node(w)->get_child_count(); i++) {
            Node* ch = gow_node(w)->get_child(i);
            if (ch && ch->get_name() == StringName(name)) { wrap_node(L, ch); return 1; }
        }
    } else {
        // Recursive — BFS through all descendants
        std::vector<Node*> queue;
        for (int i = 0; i < gow_node(w)->get_child_count(); i++) {
            Node* ch = gow_node(w)->get_child(i);
            if (ch) queue.push_back(ch);
        }
        while (!queue.empty()) {
            Node* cur = queue.back(); queue.pop_back();
            if (cur->get_name() == StringName(name)) { wrap_node(L, cur); return 1; }
            for (int i = 0; i < cur->get_child_count(); i++) {
                Node* ch = cur->get_child(i);
                if (ch) queue.push_back(ch);
            }
        }
    }
    lua_pushnil(L); return 1;
}

static int method_findfirstancestor(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    const char* name = luaL_checkstring(L, 2);
    if (!w || !gow_node(w)) { lua_pushnil(L); return 1; }
    Node* cur = gow_node(w)->get_parent();
    while (cur) {
        if (cur->get_name() == StringName(name)) { wrap_node(L, cur); return 1; }
        cur = cur->get_parent();
    }
    lua_pushnil(L); return 1;
}

static int method_findfirstancestorofclass(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    const char* cls = luaL_checkstring(L, 2);
    if (!w || !gow_node(w)) { lua_pushnil(L); return 1; }
    Node* cur = gow_node(w)->get_parent();
    while (cur) {
        if (gl_isa(cur, cls)) { wrap_node(L, cur); return 1; }
        cur = cur->get_parent();
    }
    lua_pushnil(L); return 1;
}

static int method_isancestorof(lua_State* L) {
    GodotObjectWrapper* w  = (GodotObjectWrapper*)lua_touserdata(L, 1);
    GodotObjectWrapper* w2 = (GodotObjectWrapper*)lua_touserdata(L, 2);
    if (!w || !gow_node(w) || !w2 || !gow_node(w2)) { lua_pushboolean(L, false); return 1; }
    Node* cur = gow_node(w2)->get_parent();
    while (cur) {
        if (cur == gow_node(w)) { lua_pushboolean(L, true); return 1; }
        cur = cur->get_parent();
    }
    lua_pushboolean(L, false); return 1;
}

static int method_isdescendantof(lua_State* L) {
    GodotObjectWrapper* w  = (GodotObjectWrapper*)lua_touserdata(L, 1);
    GodotObjectWrapper* w2 = (GodotObjectWrapper*)lua_touserdata(L, 2);
    if (!w || !gow_node(w) || !w2 || !gow_node(w2)) { lua_pushboolean(L, false); return 1; }
    Node* cur = gow_node(w)->get_parent();
    while (cur) {
        if (cur == gow_node(w2)) { lua_pushboolean(L, true); return 1; }
        cur = cur->get_parent();
    }
    lua_pushboolean(L, false); return 1;
}

static int method_getfullname(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    if (!w || !gow_node(w)) { lua_pushstring(L, ""); return 1; }
    lua_pushstring(L, gl_instance_fullname(gow_node(w)).utf8().get_data());
    return 1;
}

static int method_findfirstchild_whichisa(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    const char* cls = luaL_checkstring(L, 2);
    if (!w || !gow_node(w)) { lua_pushnil(L); return 1; }
    for (int i = 0; i < gow_node(w)->get_child_count(); i++) {
        Node* ch = gow_node(w)->get_child(i);
        if (ch && gl_isa(ch, cls)) { wrap_node(L, ch); return 1; }
    }
    lua_pushnil(L); return 1;
}

static int method_findfirstchildofclass(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    const char* cls = luaL_checkstring(L, 2);
    if (w && gow_node(w)) {
        TypedArray<Node> ch = gow_node(w)->get_children();
        for (int i = 0; i < ch.size(); i++) {
            Node* n = Object::cast_to<Node>(ch[i]);
            if (n && gl_isa(n, cls)) { wrap_node(L, n); return 1; }
        }
    }
    lua_pushnil(L); return 1;
}

static int method_getchildren(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    lua_newtable(L);
    if (w && gow_node(w)) {
        TypedArray<Node> ch = gow_node(w)->get_children();
        for (int i = 0; i < ch.size(); i++) {
            wrap_node(L, Object::cast_to<Node>(ch[i]));
            lua_rawseti(L, -2, i + 1);
        }
    }
    return 1;
}

static int method_getdescendants(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    lua_newtable(L);
    if (w && gow_node(w)) {
        TypedArray<Node> all;
        std::function<void(Node*)> collect = [&](Node* n) {
            TypedArray<Node> ch = n->get_children();
            for (int i = 0; i < ch.size(); i++) {
                Node* child = Object::cast_to<Node>(ch[i]);
                if (child) { all.push_back(child); collect(child); }
            }
        };
        collect(gow_node(w));
        for (int i = 0; i < all.size(); i++) {
            wrap_node(L, Object::cast_to<Node>(all[i]));
            lua_rawseti(L, -2, i + 1);
        }
    }
    return 1;
}

static int method_isa(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    const char* cls = luaL_checkstring(L, 2);
    lua_pushboolean(L, w && gl_isa(gow_node(w), cls));
    return 1;
}

static int method_setattribute(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    if (w && gow_node(w)) {
        const char* key = luaL_checkstring(L, 2);
        if (lua_isnumber(L, 3)) {
            gow_node(w)->set_meta(key, lua_tonumber(L, 3));
        } else if (lua_isboolean(L, 3)) {
            gow_node(w)->set_meta(key, (bool)lua_toboolean(L, 3));
        } else {
            gow_node(w)->set_meta(key, String(lua_tostring(L, 3)));
        }
    }
    return 0;
}

static int method_getattribute(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (w && gow_node(w) && gow_node(w)->has_meta(key)) {
        Variant val = gow_node(w)->get_meta(key);
        if (val.get_type() == Variant::FLOAT || val.get_type() == Variant::INT) {
            lua_pushnumber(L, (double)val);
        } else if (val.get_type() == Variant::BOOL) {
            lua_pushboolean(L, (bool)val);
        } else {
            lua_pushstring(L, String(val).utf8().get_data());
        }
        return 1;
    }
    lua_pushnil(L); return 1;
}

// BFS helper: find a child node by name anywhere in subtree
static Node* _find_node_by_name(Node* root, const char* name) {
    if (!root) return nullptr;
    std::vector<Node*> queue;
    queue.push_back(root);
    while (!queue.empty()) {
        Node* cur = queue.back(); queue.pop_back();
        for (int i = 0; i < cur->get_child_count(); i++) {
            Node* ch = cur->get_child(i);
            if (!ch) continue;
            if (ch->get_name() == StringName(name)) return ch;
            queue.push_back(ch);
        }
    }
    return nullptr;
}

// ── Virtual service containers ────────────────────────────────────────
// Returns (or creates if missing) a Node child of 'game' with a given name.
// Used for ReplicatedStorage, ServerStorage, StarterPack, etc.
static Node* _get_or_create_container(Node* game, const char* name) {
    if (!game) return nullptr;
    Node* existing = game->get_node_or_null(NodePath(String(name)));
    if (existing) return existing;
    Node* container = memnew(Node);
    container->set_name(StringName(name));
    game->add_child(container);
    return container;
}

static const char* _VIRTUAL_CONTAINERS[] = {
    "ReplicatedStorage", "ServerStorage", "StarterPack",
    "StarterGui", "StarterPlayerScripts", "StarterCharacterScripts",
    "Teams", "ServerScriptService", nullptr
};

static int method_getservice(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    const char* svc_name = luaL_checkstring(L, 2);
    if (!w || !gow_node(w)) { lua_pushnil(L); return 1; }

    // ── Client/server separation ──────────────────────────────────────────
    //// ── Separacion cliente/servidor ──────────────────────────────────────
    // ServerStorage and ServerScriptService are invisible to LocalScript,
    // just like in Roblox: the client can never access the server's storage.
    //// ServerStorage y ServerScriptService son invisibles para LocalScript,
    //// igual que en Roblox: el cliente nunca puede acceder al almacén del servidor.
    lua_getglobal(L, "__IS_CLIENT");
    const bool is_client = lua_isboolean(L, -1) && (lua_toboolean(L, -1) != 0);
    lua_pop(L, 1);
    lua_getglobal(L, "__IS_SERVER");
    const bool is_server = lua_isboolean(L, -1) && (lua_toboolean(L, -1) != 0);
    lua_pop(L, 1);
    if (is_client && (strcmp(svc_name, "ServerStorage") == 0 ||
                      strcmp(svc_name, "ServerScriptService") == 0)) {
        UtilityFunctions::push_error("[GodotLuau] LocalScript no puede acceder a '",
            svc_name, "' — este servicio es exclusivo del servidor.");
        lua_pushnil(L);
        return 1;
    }
    // Los servicios de ENTRADA son exclusivos del cliente: un ServerScript no
    // puede leer teclado/ratón/pantalla (igual que en Roblox).
    if (is_server && (strcmp(svc_name, "UserInputService")   == 0 ||
                      strcmp(svc_name, "ContextActionService") == 0)) {
        UtilityFunctions::push_error("[GodotLuau] ServerScript no puede acceder a '",
            svc_name, "' — la entrada de hardware solo existe en el cliente (LocalScript).");
        lua_pushnil(L);
        return 1;
    }

    // 1. Lua pure services stored as _G["__SVC_ServiceName"]
    std::string global_key = std::string("__SVC_") + svc_name;
    lua_getglobal(L, global_key.c_str());
    if (!lua_isnil(L, -1)) return 1;
    lua_pop(L, 1);

    // 2. Direct child of game node by exact name
    for (int i = 0; i < gow_node(w)->get_child_count(); i++) {
        Node* ch = gow_node(w)->get_child(i);
        if (ch && ch->get_name() == StringName(svc_name)) { wrap_node(L, ch); return 1; }
    }

    // 3. Anywhere in subtree
    Node* svc = _find_node_by_name(gow_node(w), svc_name);
    if (svc) { wrap_node(L, svc); return 1; }

    // 4. Search whole scene tree from root
    if (gow_node(w)->is_inside_tree()) {
        svc = _find_node_by_name((Node*)gow_node(w)->get_tree()->get_root(), svc_name);
        if (svc) { wrap_node(L, svc); return 1; }
    }

    // 5. Auto-create well-known virtual containers
    for (int i = 0; _VIRTUAL_CONTAINERS[i]; i++) {
        if (strcmp(_VIRTUAL_CONTAINERS[i], svc_name) == 0) {
            Node* c = _get_or_create_container(gow_node(w), svc_name);
            wrap_node(L, c); return 1;
        }
    }

    // 6. Servicios singleton C++ que se auto-crean bajo game la primera vez
    if (strcmp(svc_name, "NetworkService") == 0) {
        NetworkService* ns = memnew(NetworkService);
        ns->set_name("NetworkService");
        gow_node(w)->add_child(ns);
        wrap_node(L, ns); return 1;
    }
    // Players SIEMPRE existe (como en Roblox): si el juego no tiene el nodo,
    // se crea al pedirlo. Sin esto game:GetService("Players") devolvia nil y
    // cualquier framework con Players:GetPlayers() moria al instante.
    if (strcmp(svc_name, "Players") == 0) {
        Node* ps = Object::cast_to<Node>(ClassDB::instantiate(StringName("Players")));
        if (ps) {
            ps->set_name("Players");
            gow_node(w)->add_child(ps);
            wrap_node(L, ps); return 1;
        }
    }

    lua_pushnil(L); return 1;
}

// ════════════════════════════════════════════════════════════════════
//  godot_object_index — __index of GodotObject
////
//  godot_object_index — __index de GodotObject
// ════════════════════════════════════════════════════════════════════
static int godot_object_index(lua_State* L) {
    GodotObjectWrapper* wrapper = (GodotObjectWrapper*)lua_touserdata(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (!wrapper || !gow_node(wrapper)) { lua_pushnil(L); return 1; }
    Node* n = gow_node(wrapper);

    // ── Utilidades de Instance (Ola 2) ────────────────────────────────
    if (strcmp(key, "GetDebugId") == 0) {
        lua_pushcfunction(L, [](lua_State* pL) -> int {
            GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(pL, 1);
            Node* nd = w ? gow_node(w) : nullptr;
            String s = String::num_uint64(nd ? (uint64_t)nd->get_instance_id() : 0);
            lua_pushstring(pL, s.utf8().get_data()); return 1;
        }, "GetDebugId"); return 1;
    }
    if (strcmp(key, "GetActor") == 0) {
        lua_pushcfunction(L, [](lua_State* pL) -> int { lua_pushnil(pL); return 1; }, "GetActor"); return 1;
    }

    // ── DataModel (game): PlaceId/JobId/CreatorId/BindToClose… (Ola 2) ──
    if (strcmp(key,"PlaceId")==0 || strcmp(key,"GameId")==0 || strcmp(key,"JobId")==0 ||
        strcmp(key,"CreatorId")==0 || strcmp(key,"CreatorType")==0 || strcmp(key,"PrivateServerId")==0 ||
        strcmp(key,"PrivateServerOwnerId")==0 || strcmp(key,"PlaceVersion")==0 ||
        strcmp(key,"BindToClose")==0 || strcmp(key,"IsLoaded")==0) {
        String _cls = n->get_class();
        if (_cls == "RobloxGame3D" || _cls == "RobloxGame2D" || _cls == "RobloxTemplate") {
            if (strcmp(key,"PlaceId")==0 || strcmp(key,"GameId")==0 || strcmp(key,"CreatorId")==0 ||
                strcmp(key,"PrivateServerOwnerId")==0)      { lua_pushnumber(L, 0); return 1; }
            if (strcmp(key,"PlaceVersion")==0)              { lua_pushnumber(L, 1); return 1; }
            if (strcmp(key,"CreatorType")==0)               { lua_pushstring(L, "User"); return 1; }
            if (strcmp(key,"JobId")==0 || strcmp(key,"PrivateServerId")==0) { lua_pushstring(L, ""); return 1; }
            if (strcmp(key,"IsLoaded")==0) {
                lua_pushcfunction(L, [](lua_State* pL) -> int { lua_pushboolean(pL, 1); return 1; }, "IsLoaded"); return 1;
            }
            if (strcmp(key,"BindToClose")==0) {
                lua_pushcfunction(L, [](lua_State* pL) -> int {
                    // Acepta game:BindToClose(fn) (self+fn) o game.BindToClose(fn)
                    int fnidx = lua_isfunction(pL, 2) ? 2 : (lua_isfunction(pL, 1) ? 1 : 0);
                    if (!fnidx) return 0;
                    lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                    lua_State* mL = (lua_State*)lua_touserdata(pL, -1);
                    lua_pop(pL, 1);
                    if (!mL) mL = pL;
                    int ref = lua_ref(pL, fnidx);   // ref en el registro compartido
                    gl_add_close_cb(mL, ref);
                    return 0;
                }, "BindToClose"); return 1;
            }
        }
    }

    // ── PlayerObject ("Player", Player ≠ Character) ────────────────────────
    {
        PlayerObject* plr = Object::cast_to<PlayerObject>(n);
        if (plr) {
            if (strcmp(key, "UserId") == 0)      { lua_pushnumber(L, (double)plr->user_id); return 1; }
            if (strcmp(key, "DisplayName") == 0) { lua_pushstring(L, plr->display_name.utf8().get_data()); return 1; }
            if (strcmp(key, "AccountAge") == 0)  { lua_pushnumber(L, (double)plr->account_age); return 1; }
            if (strcmp(key, "ClassName") == 0)   { lua_pushstring(L, "Player"); return 1; }
            if (strcmp(key, "Team") == 0) {
                Node* t = plr->get_team();
                if (t) wrap_node(L, t); else lua_pushnil(L);
                return 1;
            }
            if (strcmp(key, "Neutral") == 0)     { lua_pushboolean(L, plr->get_team() ? 0 : 1); return 1; }
            if (strcmp(key, "TeamColor") == 0) {
                Node* t = plr->get_team();
                lua_pushnumber(L, t ? (double)(int)t->get("TeamColor") : (double)plr->team_color);
                return 1;
            }
            if (strcmp(key, "FollowUserId") == 0){ lua_pushnumber(L, 0); return 1; }
            // Miembros reales de Player que antes vivían en el personaje (compat):
            if (strcmp(key, "CameraMode") == 0)  { lua_pushnumber(L, plr->camera_mode); return 1; }
            if (strcmp(key, "CameraMinZoomDistance") == 0) { lua_pushnumber(L, 0.5); return 1; }
            if (strcmp(key, "CameraMaxZoomDistance") == 0) { lua_pushnumber(L, 400); return 1; }
            if (strcmp(key, "AutoJumpEnabled") == 0)       { lua_pushboolean(L, 1); return 1; }
            if (strcmp(key, "DevComputerMovementMode") == 0 || strcmp(key, "DevTouchMovementMode") == 0 ||
                strcmp(key, "DevCameraOcclusionMode") == 0 || strcmp(key, "DevComputerCameraMode") == 0)
                                                 { lua_pushnumber(L, 0); return 1; }
            if (strcmp(key, "MembershipType") == 0) { lua_pushnumber(L, 0); return 1; }
            if (strcmp(key, "GetRankInGroup") == 0) {
                lua_pushcfunction(L, [](lua_State* pL) -> int { lua_pushnumber(pL, 0); return 1; }, "GetRankInGroup"); return 1;
            }
            if (strcmp(key, "GetRoleInGroup") == 0) {
                lua_pushcfunction(L, [](lua_State* pL) -> int { lua_pushstring(pL, "Guest"); return 1; }, "GetRoleInGroup"); return 1;
            }
            if (strcmp(key, "IsInGroup") == 0) {
                lua_pushcfunction(L, [](lua_State* pL) -> int { lua_pushboolean(pL, 0); return 1; }, "IsInGroup"); return 1;
            }
            if (strcmp(key, "GetFriendStatus") == 0) {
                lua_pushcfunction(L, [](lua_State* pL) -> int { lua_pushstring(pL, "NotFriend"); return 1; }, "GetFriendStatus"); return 1;
            }
            if (strcmp(key, "GetFriendsOnline") == 0 || strcmp(key, "GetCharacterAppearanceInfoAsync") == 0 ||
                strcmp(key, "GetCharacterAppearanceAsync") == 0 || strcmp(key, "GetJoinData") == 0) {
                lua_pushcfunction(L, [](lua_State* pL) -> int { lua_newtable(pL); return 1; }, "PlayerStub"); return 1;
            }
            if (strcmp(key, "Character") == 0) {
                Node* c = plr->get_character();
                if (c) wrap_node(L, c); else lua_pushnil(L);
                return 1;
            }
            if (strcmp(key, "GetCharacter") == 0) {
                lua_pushlightuserdata(L, (void*)plr);
                lua_pushcclosure(L, [](lua_State* pL) -> int {
                    PlayerObject* p = (PlayerObject*)lua_touserdata(pL, lua_upvalueindex(1));
                    Node* c = p ? p->get_character() : nullptr;
                    if (c) wrap_node(pL, c); else lua_pushnil(pL);
                    return 1;
                }, "GetCharacter", 1); return 1;
            }
            if (strcmp(key, "IsA") == 0) {
                lua_pushcfunction(L, [](lua_State* pL) -> int {
                    const char* q = luaL_checkstring(pL, 2);
                    lua_pushboolean(pL, (strcmp(q, "Player") == 0 || strcmp(q, "Instance") == 0) ? 1 : 0);
                    return 1;
                }, "IsA"); return 1;
            }
            if (strcmp(key, "Kick") == 0) {
                lua_pushlightuserdata(L, (void*)plr);
                lua_pushcclosure(L, [](lua_State* pL) -> int {
                    PlayerObject* p = (PlayerObject*)lua_touserdata(pL, lua_upvalueindex(1));
                    if (!p) return 0;
                    String msg = lua_isstring(pL, 2) ? String::utf8(lua_tostring(pL, 2)) : String();
                    if (Node* ns = _gl_find_network_service(p)) ns->call("net_kick_player", (Object*)p, msg);
                    return 0;
                }, "Kick", 1);
                return 1;
            }
            // GetMouse (1.14.10 Finish): crea (una vez) el RobloxMouse del
            // jugador y lo devuelve. Solo tiene sentido en el jugador LOCAL:
            // el ratón es de esta máquina (en Roblox igual, es de LocalScript).
            if (strcmp(key, "GetMouse") == 0) {
                lua_pushlightuserdata(L, (void*)plr);
                lua_pushcclosure(L, [](lua_State* pL) -> int {
                    PlayerObject* p = (PlayerObject*)lua_touserdata(pL, lua_upvalueindex(1));
                    if (!p || !p->is_inside_tree()) { lua_pushnil(pL); return 1; }
                    if (!p->is_local) {
                        luaL_error(pL, "GetMouse: only the LocalPlayer has a Mouse");
                        return 0;
                    }
                    RobloxMouse* m = Object::cast_to<RobloxMouse>(p->get_node_or_null(NodePath("Mouse")));
                    if (!m) {
                        m = memnew(RobloxMouse);
                        m->set_name("Mouse");
                        p->add_child(m);
                    }
                    m->character_id = (uint64_t)0;
                    if (Node* ch = p->get_character()) m->character_id = (uint64_t)ch->get_instance_id();
                    wrap_node(pL, m);
                    return 1;
                }, "GetMouse", 1);
                return 1;
            }
            if (strcmp(key, "LoadCharacter") == 0) {
                lua_pushlightuserdata(L, (void*)plr);
                lua_pushcclosure(L, [](lua_State* pL) -> int {
                    PlayerObject* p = (PlayerObject*)lua_touserdata(pL, lua_upvalueindex(1));
                    if (!p) return 0;
                    if (Node* ns = _gl_find_network_service(p)) { ns->call("net_load_character", (Object*)p); return 0; }
                    // Sin NetworkService (juego normal sin red): respawn local.
                    Node* ps = p->get_parent();
                    if (ps && ps->has_method("load_local_character")) ps->call("load_local_character");
                    return 0;
                }, "LoadCharacter", 1);
                return 1;
            }
            // Ping en SEGUNDOS (como Roblox). Antes no existía en el runtime pese
            // a estar anunciado en el autocompletado: llamarlo reventaba.
            if (strcmp(key, "GetNetworkPing") == 0) {
                lua_pushlightuserdata(L, (void*)plr);
                lua_pushcclosure(L, [](lua_State* pL) -> int {
                    PlayerObject* p = (PlayerObject*)lua_touserdata(pL, lua_upvalueindex(1));
                    Node* ns = p ? _gl_find_network_service(p) : nullptr;
                    lua_pushnumber(pL, ns ? (double)(ns->call("net_ping_for_player", (Object*)p)) : 0.0);
                    return 1;
                }, "GetNetworkPing", 1);
                return 1;
            }
            if (strcmp(key, "CharacterAdded") == 0) {
                lua_newtable(L);
                lua_pushlightuserdata(L, (void*)plr);
                lua_pushcclosure(L, [](lua_State* pL) -> int {
                    PlayerObject* p = (PlayerObject*)lua_touserdata(pL, lua_upvalueindex(1));
                    if (!p || !lua_isfunction(pL, 2)) { lua_pushnil(pL); return 1; }
                    lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                    lua_State* mL = (lua_State*)lua_touserdata(pL, -1); lua_pop(pL, 1); if (!mL) mL = pL;
                    int ref = lua_ref(pL, 2); p->add_char_added_cb(mL, ref); _gl_push_connection(pL, p, ref); return 1;
                }, "Connect", 1);
                lua_setfield(L, -2, "Connect");
                lua_pushlightuserdata(L, (void*)plr);
                lua_pushcclosure(L, [](lua_State* pL) -> int {
                    PlayerObject* p = (PlayerObject*)lua_touserdata(pL, lua_upvalueindex(1));
                    if (!p) { lua_pushnil(pL); return 1; }
                    lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                    lua_State* mL = (lua_State*)lua_touserdata(pL, -1); lua_pop(pL, 1); if (!mL) mL = pL;
                    p->add_char_added_wait(pL, mL);
                    lua_pushstring(pL, "__WAIT_SIGNAL__"); return lua_yield(pL, 1);
                }, "Wait", 1);
                lua_setfield(L, -2, "Wait");
                return 1;
            }
            if (strcmp(key, "CharacterRemoving") == 0 || strcmp(key, "CharacterAppearanceLoaded") == 0) {
                bool removing = (strcmp(key, "CharacterRemoving") == 0);
                lua_newtable(L);
                lua_pushlightuserdata(L, (void*)plr);
                lua_pushboolean(L, removing ? 1 : 0);
                lua_pushcclosure(L, [](lua_State* pL) -> int {
                    PlayerObject* p = (PlayerObject*)lua_touserdata(pL, lua_upvalueindex(1));
                    bool rem = lua_toboolean(pL, lua_upvalueindex(2));
                    if (!p || !lua_isfunction(pL, 2)) { lua_pushnil(pL); return 1; }
                    lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                    lua_State* mL = (lua_State*)lua_touserdata(pL, -1); lua_pop(pL, 1); if (!mL) mL = pL;
                    int ref = lua_ref(pL, 2);
                    if (rem) p->add_char_removing_cb(mL, ref); else p->add_char_appearance_cb(mL, ref);
                    _gl_push_connection(pL, p, ref); return 1;
                }, "Connect", 2);
                lua_setfield(L, -2, "Connect");
                return 1;
            }
            // Name, Parent, GetChildren, WaitForChild… caen al manejo genérico.
        }
    }

    // ── RobloxTerrain (Workspace.Terrain): FillBlock/FillBall/FillRegion/Clear ─
    {
        RobloxTerrain* terr = Object::cast_to<RobloxTerrain>(n);
        if (terr) {
            if (strcmp(key, "FillBlock") == 0) {
                lua_pushlightuserdata(L, (void*)terr);
                lua_pushcclosure(L, [](lua_State* LL) -> int {
                    RobloxTerrain* t = (RobloxTerrain*)lua_touserdata(LL, lua_upvalueindex(1));
                    if (!t) return 0;
                    // arg2 = CFrame (tabla con X/Y/Z), arg3 = size (Vector3), arg4 = material
                    float px = 0; float py = 0; float pz = 0;
                    if (lua_istable(LL, 2)) {
                        lua_getfield(LL, 2, "X"); px = (float)lua_tonumber(LL, -1); lua_pop(LL, 1);
                        lua_getfield(LL, 2, "Y"); py = (float)lua_tonumber(LL, -1); lua_pop(LL, 1);
                        lua_getfield(LL, 2, "Z"); pz = (float)lua_tonumber(LL, -1); lua_pop(LL, 1);
                    }
                    Vector3 size(4, 4, 4);
                    if (LuauVector3* v = check_vector3(LL, 3)) size = Vector3(v->x, v->y, v->z);
                    t->fill_block(Vector3(px, py, pz), size, (int)luaL_optnumber(LL, 4, 0));
                    return 0;
                }, "FillBlock", 1); return 1;
            }
            if (strcmp(key, "FillBall") == 0) {
                lua_pushlightuserdata(L, (void*)terr);
                lua_pushcclosure(L, [](lua_State* LL) -> int {
                    RobloxTerrain* t = (RobloxTerrain*)lua_touserdata(LL, lua_upvalueindex(1));
                    if (!t) return 0;
                    Vector3 c(0, 0, 0);
                    if (LuauVector3* v = check_vector3(LL, 2)) c = Vector3(v->x, v->y, v->z);
                    t->fill_ball(c, (float)luaL_optnumber(LL, 3, 4), (int)luaL_optnumber(LL, 4, 0));
                    return 0;
                }, "FillBall", 1); return 1;
            }
            if (strcmp(key, "FillRegion") == 0) {
                lua_pushlightuserdata(L, (void*)terr);
                lua_pushcclosure(L, [](lua_State* LL) -> int {
                    RobloxTerrain* t = (RobloxTerrain*)lua_touserdata(LL, lua_upvalueindex(1));
                    if (!t) return 0;
                    Vector3 mn(0, 0, 0); Vector3 mx(0, 0, 0);
                    if (LuauVector3* v = check_vector3(LL, 2)) mn = Vector3(v->x, v->y, v->z);
                    if (LuauVector3* v = check_vector3(LL, 3)) mx = Vector3(v->x, v->y, v->z);
                    t->fill_region(mn, mx, (int)luaL_optnumber(LL, 4, 0));
                    return 0;
                }, "FillRegion", 1); return 1;
            }
            if (strcmp(key, "Clear") == 0) {
                lua_pushlightuserdata(L, (void*)terr);
                lua_pushcclosure(L, [](lua_State* LL) -> int {
                    RobloxTerrain* t = (RobloxTerrain*)lua_touserdata(LL, lua_upvalueindex(1));
                    if (t) t->clear_terrain();
                    return 0;
                }, "Clear", 1); return 1;
            }
            // ReadVoxels devuelve (materials, occupancy) vacíos; el resto no-op.
            // (MundoVoxel construye su propia malla; estos evitan que scripts que
            //  los llamen se rompan.)
            if (strcmp(key, "ReadVoxels") == 0) {
                lua_pushcfunction(L, [](lua_State* LL) -> int {
                    lua_newtable(LL); lua_newtable(LL); return 2;
                }, "ReadVoxels"); return 1;
            }
            if (strcmp(key, "WriteVoxels") == 0 || strcmp(key, "SetMaterialColor") == 0 ||
                strcmp(key, "GetMaterialColor") == 0 || strcmp(key, "ReplaceMaterial") == 0) {
                lua_pushcfunction(L, [](lua_State* LL) -> int { return 0; }, "TerrainNoop"); return 1;
            }
        }
    }

    // ── RobloxValue (NumberValue/IntValue/StringValue/…): .Changed, ClassName, IsA ─
    // .Value se resuelve por el puente _gl_bridge más abajo; aquí solo lo específico.
    {
        RobloxValue* rval = Object::cast_to<RobloxValue>(n);
        if (rval) {
            if (strcmp(key, "ClassName") == 0) {
                lua_pushstring(L, rval->roblox_class.utf8().get_data()); return 1;
            }
            if (strcmp(key, "Changed") == 0) {
                lua_newtable(L);
                lua_pushlightuserdata(L, (void*)rval);
                lua_pushcclosure(L, [](lua_State* LL) -> int {
                    RobloxValue* d = (RobloxValue*)lua_touserdata(LL, lua_upvalueindex(1));
                    if (!d || !lua_isfunction(LL, 2)) { lua_pushnil(LL); return 1; }
                    int ref = lua_ref(LL, 2);
                    d->add_changed_cb(gl_main_of(LL), ref); _gl_push_connection(LL, d, ref); return 1;
                }, "Connect", 1);
                lua_setfield(L, -2, "Connect"); return 1;
            }
            if (strcmp(key, "GetPropertyChangedSignal") == 0) {
                lua_pushlightuserdata(L, (void*)rval);
                lua_pushcclosure(L, [](lua_State* LL) -> int {
                    RobloxValue* d = (RobloxValue*)lua_touserdata(LL, lua_upvalueindex(1));
                    lua_newtable(LL);
                    lua_pushlightuserdata(LL, (void*)d);
                    lua_pushcclosure(LL, [](lua_State* L2) -> int {
                        RobloxValue* d2 = (RobloxValue*)lua_touserdata(L2, lua_upvalueindex(1));
                        if (!d2 || !lua_isfunction(L2, 2)) { lua_pushnil(L2); return 1; }
                        int ref = lua_ref(L2, 2);
                        d2->add_changed_cb(gl_main_of(L2), ref); _gl_push_connection(L2, d2, ref); return 1;
                    }, "Connect", 1);
                    lua_setfield(LL, -2, "Connect"); return 1;
                }, "GetPropertyChangedSignal", 1); return 1;
            }
            if (strcmp(key, "IsA") == 0) {
                lua_pushlightuserdata(L, (void*)rval);
                lua_pushcclosure(L, [](lua_State* LL) -> int {
                    RobloxValue* d = (RobloxValue*)lua_touserdata(LL, lua_upvalueindex(1));
                    const char* q = luaL_checkstring(LL, 2);
                    bool ok = d && (d->roblox_class == String(q) ||
                                    strcmp(q, "Instance") == 0 || strcmp(q, "ValueBase") == 0);
                    lua_pushboolean(LL, ok ? 1 : 0); return 1;
                }, "IsA", 1); return 1;
            }
        }
    }

    if (strcmp(key, "Destroy")          == 0) { lua_pushcfunction(L, method_destroy,                   "Destroy");                return 1; }
    if (strcmp(key, "Clone")            == 0) { lua_pushcfunction(L, method_clone,                     "Clone");                  return 1; }
    if (strcmp(key, "ClearAllChildren") == 0) { lua_pushcfunction(L, method_clearallchildren,          "ClearAllChildren");       return 1; }
    // WaitForChild NO va aqui: lo implementa el __index local de luau_script.h
    // (suspende la corrutina de verdad, como Roblox).
    if (strcmp(key, "FindFirstChild")   == 0) { lua_pushcfunction(L, method_findfirstchild,            key);                      return 1; }
    if (strcmp(key, "FindFirstChildOfClass") == 0 ||
        strcmp(key, "FindFirstChildWhichIsA")== 0) { lua_pushcfunction(L, method_findfirstchild_whichisa, key);                   return 1; }
    if (strcmp(key, "FindFirstAncestor")     == 0) { lua_pushcfunction(L, method_findfirstancestor,    "FindFirstAncestor");      return 1; }
    if (strcmp(key, "FindFirstAncestorOfClass") == 0 ||
        strcmp(key, "FindFirstAncestorWhichIsA") == 0) { lua_pushcfunction(L, method_findfirstancestorofclass, key);              return 1; }
    if (strcmp(key, "IsAncestorOf")     == 0) { lua_pushcfunction(L, method_isancestorof,             "IsAncestorOf");            return 1; }
    if (strcmp(key, "IsDescendantOf")   == 0) { lua_pushcfunction(L, method_isdescendantof,           "IsDescendantOf");          return 1; }
    if (strcmp(key, "GetFullName")      == 0) { lua_pushcfunction(L, method_getfullname,              "GetFullName");             return 1; }
    if (strcmp(key, "GetChildren")      == 0) { lua_pushcfunction(L, method_getchildren,              "GetChildren");             return 1; }
    if (strcmp(key, "GetDescendants")   == 0) { lua_pushcfunction(L, method_getdescendants,           "GetDescendants");          return 1; }
    if (strcmp(key, "IsA")              == 0) { lua_pushcfunction(L, method_isa,                      "IsA");                     return 1; }
    if (strcmp(key, "SetAttribute")     == 0) { lua_pushcfunction(L, method_setattribute,             "SetAttribute");            return 1; }
    if (strcmp(key, "GetAttribute")     == 0) { lua_pushcfunction(L, method_getattribute,             "GetAttribute");            return 1; }
    if (strcmp(key, "GetService")       == 0) { lua_pushcfunction(L, method_getservice,               "GetService");              return 1; }

    // ── Universal instance signals ────────────────────────────────
    //// ── Señales de instancia universales ─────────────────────────
    // ChildAdded / ChildRemoved — connect to Godot child_entered_tree / child_exiting_tree
    //// ChildAdded / ChildRemoved — conectan a Godot child_entered_tree / child_exiting_tree
    if (strcmp(key, "ChildAdded") == 0 || strcmp(key, "ChildRemoved") == 0) {
        bool is_added = (strcmp(key, "ChildAdded") == 0);
        lua_newtable(L);
        lua_pushlightuserdata(L, (void*)n);
        lua_pushboolean(L, is_added ? 1 : 0);
        lua_pushcclosure(L, [](lua_State* pL) -> int {
            Node* nd = (Node*)lua_touserdata(pL, lua_upvalueindex(1));
            bool adding = lua_toboolean(pL, lua_upvalueindex(2)) != 0;
            if (!nd || !lua_isfunction(pL, 2)) { lua_pushnil(pL); return 1; }
            // Dueño del callback = el ScriptNodeBase. Godot desconecta solo al
            // liberar cualquiera de los dos extremos → sin fugas ni use-after-free.
            lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_SCRIPT_NODE");
            Node* sn = (Node*)lua_touserdata(pL, -1); lua_pop(pL, 1);
            if (!sn) { lua_pushnil(pL); return 1; }
            lua_pushvalue(pL, 2); int ref = lua_ref(pL, -1); lua_pop(pL, 1);
            const char* gsig = adding ? "child_entered_tree" : "child_exiting_tree";
            if (nd->has_signal(gsig))
                nd->connect(gsig, Callable(sn, "_gl_child_event").bind(ref));
            // Objeto conexión con Disconnect() (resuelve por ObjectID: seguro
            // aunque los nodos se hayan destruido cuando se llame).
            lua_newtable(pL);
            lua_pushlightuserdata(pL, (void*)(uintptr_t)nd->get_instance_id());
            lua_pushlightuserdata(pL, (void*)(uintptr_t)sn->get_instance_id());
            lua_pushstring(pL, gsig);
            lua_pushinteger(pL, ref);
            lua_pushcclosure(pL, [](lua_State* dL) -> int {
                uint64_t tid = (uint64_t)(uintptr_t)lua_touserdata(dL, lua_upvalueindex(1));
                uint64_t sid = (uint64_t)(uintptr_t)lua_touserdata(dL, lua_upvalueindex(2));
                const char* sig = lua_tostring(dL, lua_upvalueindex(3));
                int r = (int)lua_tointeger(dL, lua_upvalueindex(4));
                Node* tnd = Object::cast_to<Node>(ObjectDB::get_instance(tid));
                Node* snn = Object::cast_to<Node>(ObjectDB::get_instance(sid));
                if (tnd && snn) {
                    Callable c = Callable(snn, "_gl_child_event").bind(r);
                    if (tnd->is_connected(sig, c)) tnd->disconnect(sig, c);
                }
                return 0;
            }, "Disconnect", 4);
            lua_setfield(pL, -2, "Disconnect");
            return 1;
        }, "Connect", 2);
        lua_setfield(L, -2, "Connect");
        return 1;
    }

    // GetPropertyChangedSignal(prop) → señal (aún no soportada: inerte, sin fugas)
    if (strcmp(key, "GetPropertyChangedSignal") == 0) {
        lua_pushcfunction(L, [](lua_State* pL) -> int {
            _gl_push_inert_signal(pL);
            return 1;
        }, "GetPropertyChangedSignal");
        return 1;
    }

    // AncestryChanged / DescendantAdded / DescendantRemoving — aún no soportadas.
    // Devolvemos una señal inerte (Connect/Once/Wait no-op) en vez de conectar a
    // métodos inexistentes y filtrar referencias como antes.
    if (strcmp(key, "AncestryChanged")    == 0 ||
        strcmp(key, "DescendantAdded")    == 0 ||
        strcmp(key, "DescendantRemoving") == 0) {
        _gl_push_inert_signal(L);
        return 1;
    }

    // ── Propiedades de Instance ───────────────────────────────────
    if (strcmp(key, "Name")      == 0) { lua_pushstring(L, String(n->get_name()).utf8().get_data()); return 1; }
    if (strcmp(key, "ClassName") == 0) { lua_pushstring(L, gl_roblox_classname(n)); return 1; }
    if (strcmp(key, "Parent")    == 0) { wrap_node(L, n->get_parent()); return 1; }

    // ── Players ───────────────────────────────────────────────────
    Players* players_svc = Object::cast_to<Players>(n);
    if (players_svc) {
        if (strcmp(key, "LocalPlayer") == 0) {
            // El servidor no tiene "jugador local": LocalPlayer = nil en un ServerScript.
            lua_getglobal(L, "__IS_SERVER");
            bool srv = lua_isboolean(L, -1) && (lua_toboolean(L, -1) != 0);
            lua_pop(L, 1);
            if (srv) { lua_pushnil(L); return 1; }
            wrap_node(L, players_svc->get_local_player()); return 1;
        }
        if (strcmp(key, "MaxPlayers") == 0) { lua_pushnumber(L, players_svc->get_max_players()); return 1; }
        if (strcmp(key, "NumPlayers") == 0) {
            TypedArray<Node> pl = players_svc->get_players();
            lua_pushnumber(L, pl.size()); return 1;
        }
        if (strcmp(key, "GetPlayers") == 0) {
            lua_pushlightuserdata(L, (void*)players_svc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                Players* ps = (Players*)lua_touserdata(pL, lua_upvalueindex(1));
                lua_newtable(pL);
                if (ps) {
                    TypedArray<Node> pl = ps->get_players();
                    for (int i = 0; i < pl.size(); i++) {
                        wrap_node(pL, Object::cast_to<Node>(pl[i]));
                        lua_rawseti(pL, -2, i + 1);
                    }
                }
                return 1;
            }, "GetPlayers", 1);
            return 1;
        }
        if (strcmp(key, "GetPlayerFromCharacter") == 0) {
            lua_pushlightuserdata(L, (void*)players_svc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                Players* ps = (Players*)lua_touserdata(pL, lua_upvalueindex(1));
                GodotObjectWrapper* cw = (GodotObjectWrapper*)lua_touserdata(pL, 2);
                if (!ps || !cw || !gow_node(cw)) { lua_pushnil(pL); return 1; }
                Node* character = gow_node(cw);
                TypedArray<Node> all_players = ps->get_players();
                for (int i = 0; i < all_players.size(); i++) {
                    PlayerObject* po = Object::cast_to<PlayerObject>(all_players[i]);
                    if (po && po->get_character() == character) { wrap_node(pL, po); return 1; }
                }
                lua_pushnil(pL); return 1;
            }, "GetPlayerFromCharacter", 1);
            return 1;
        }
        if (strcmp(key, "GetPlayerByUserId") == 0) {
            lua_pushlightuserdata(L, (void*)players_svc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                Players* ps = (Players*)lua_touserdata(pL, lua_upvalueindex(1));
                int uid = (int)luaL_checknumber(pL, 2);
                if (!ps) { lua_pushnil(pL); return 1; }
                TypedArray<Node> all_players = ps->get_players();
                for (int i = 0; i < all_players.size(); i++) {
                    PlayerObject* po = Object::cast_to<PlayerObject>(all_players[i]);
                    if (po && po->user_id == uid) { wrap_node(pL, po); return 1; }
                }
                lua_pushnil(pL); return 1;
            }, "GetPlayerByUserId", 1);
            return 1;
        }
        // PlayerAdded / PlayerRemoving signals
        if (strcmp(key, "PlayerAdded") == 0 || strcmp(key, "PlayerRemoving") == 0) {
            const char* sig = key;
            lua_newtable(L);
            lua_pushlightuserdata(L, (void*)players_svc);
            lua_pushstring(L, sig);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                Players* ps = (Players*)lua_touserdata(pL, lua_upvalueindex(1));
                const char* ev = lua_tostring(pL, lua_upvalueindex(2));
                if (!lua_isfunction(pL, 2) || !ps) return 0;
                lua_pushvalue(pL, 2); int ref = lua_ref(pL, -1); lua_pop(pL, 1);
                lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                lua_State* mL = (lua_State*)lua_touserdata(pL, -1);
                lua_pop(pL, 1);
                if (!mL) mL = pL;
                if (strcmp(ev, "PlayerAdded") == 0)    ps->add_player_added_cb(mL, ref);
                else                                    ps->add_player_removing_cb(mL, ref);
                _gl_push_connection(pL, ps, ref); return 1;
            }, "Connect", 2);
            lua_setfield(L, -2, "Connect");
            // :Once — igual que Connect pero se desconecta tras el primer disparo.
            lua_pushlightuserdata(L, (void*)players_svc);
            lua_pushstring(L, sig);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                Players* ps = (Players*)lua_touserdata(pL, lua_upvalueindex(1));
                const char* ev = lua_tostring(pL, lua_upvalueindex(2));
                if (!lua_isfunction(pL, 2) || !ps) return 0;
                lua_pushvalue(pL, 2); int ref = lua_ref(pL, -1); lua_pop(pL, 1);
                lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                lua_State* mL = (lua_State*)lua_touserdata(pL, -1);
                lua_pop(pL, 1);
                if (!mL) mL = pL;
                if (strcmp(ev, "PlayerAdded") == 0)    ps->add_player_added_cb(mL, ref, true);
                else                                    ps->add_player_removing_cb(mL, ref, true);
                _gl_push_connection(pL, ps, ref); return 1;
            }, "Once", 2);
            lua_setfield(L, -2, "Once");
            // :Wait — cede el hilo y lo reanuda con el Player del próximo disparo.
            lua_pushlightuserdata(L, (void*)players_svc);
            lua_pushstring(L, sig);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                Players* ps = (Players*)lua_touserdata(pL, lua_upvalueindex(1));
                const char* ev = lua_tostring(pL, lua_upvalueindex(2));
                if (!ps) { lua_pushnil(pL); return 1; }
                lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                lua_State* mL = (lua_State*)lua_touserdata(pL, -1);
                lua_pop(pL, 1);
                if (!mL) mL = pL;
                if (strcmp(ev, "PlayerAdded") == 0)    ps->add_player_added_wait(pL, mL);
                else                                    ps->add_player_removing_wait(pL, mL);
                lua_pushstring(pL, "__WAIT_SIGNAL__"); return lua_yield(pL, 1);
            }, "Wait", 2);
            lua_setfield(L, -2, "Wait");
            return 1;
        }
    }

    // ── RobloxMouse — Player:GetMouse() (1.14.10 Finish) ──────────
    if (RobloxMouse* mo = Object::cast_to<RobloxMouse>(n)) {
        if (strcmp(key, "X") == 0)         { lua_pushnumber(L, (double)mo->pos2d().x); return 1; }
        if (strcmp(key, "Y") == 0)         { lua_pushnumber(L, (double)mo->pos2d().y); return 1; }
        if (strcmp(key, "ViewSizeX") == 0) { lua_pushnumber(L, (double)mo->view_size().x); return 1; }
        if (strcmp(key, "ViewSizeY") == 0) { lua_pushnumber(L, (double)mo->view_size().y); return 1; }
        if (strcmp(key, "ClassName") == 0) { lua_pushstring(L, "Mouse"); return 1; }
        // Hit: CFrame en el punto de impacto. Si el rayo no da a nada, Roblox
        // devuelve un punto lejano sobre el rayo (no nil) → mismo comportamiento.
        if (strcmp(key, "Hit") == 0) {
            Dictionary r = mo->cast();
            Vector3 p = r.is_empty() ? (mo->unit_ray_origin() + mo->unit_ray_dir() * 1000.0f)
                                     : (Vector3)r.get("position", Vector3());
            _gl_push_cframe_pos(L, p);
            return 1;
        }
        if (strcmp(key, "Target") == 0) {
            Dictionary r = mo->cast();
            Variant cv = r.get("collider", Variant());
            Node* hit = (cv.get_type() == Variant::OBJECT) ? Object::cast_to<Node>(cv.operator Object*()) : nullptr;
            if (hit) wrap_node(L, hit); else lua_pushnil(L);
            return 1;
        }
        if (strcmp(key, "TargetSurface") == 0) {
            Dictionary r = mo->cast();
            Vector3 nrm = r.get("normal", Vector3(0,1,0));
            push_vector3(L, nrm.x, nrm.y, nrm.z);   // Roblox da un Enum.NormalId; aquí la normal cruda
            return 1;
        }
        if (strcmp(key, "UnitRay") == 0) {
            lua_getglobal(L, "Ray");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "new");
                if (lua_isfunction(L, -1)) {
                    Vector3 o = mo->unit_ray_origin(), d = mo->unit_ray_dir();
                    push_vector3(L, o.x, o.y, o.z);
                    push_vector3(L, d.x, d.y, d.z);
                    if (lua_pcall(L, 2, 1, 0) == LUA_OK) { lua_remove(L, -2); return 1; }
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
            lua_pushnil(L);
            return 1;
        }
        if (strcmp(key, "TargetFilter") == 0) {
            Node* tf = mo->target_filter_id ? Object::cast_to<Node>(ObjectDB::get_instance(mo->target_filter_id)) : nullptr;
            if (tf) wrap_node(L, tf); else lua_pushnil(L);
            return 1;
        }
        const char* MOUSE_EVENTS[] = { "Button1Down","Button1Up","Button2Down","Button2Up","Move","WheelForward","WheelBackward", nullptr };
        for (int i = 0; MOUSE_EVENTS[i]; i++) {
            if (strcmp(key, MOUSE_EVENTS[i]) != 0) continue;
            lua_newtable(L);
            for (int once = 0; once < 2; once++) {
                lua_pushlightuserdata(L, (void*)mo);
                lua_pushinteger(L, i);
                lua_pushboolean(L, once);
                lua_pushcclosure(L, [](lua_State* pL) -> int {
                    RobloxMouse* m = (RobloxMouse*)lua_touserdata(pL, lua_upvalueindex(1));
                    int idx = (int)lua_tointeger(pL, lua_upvalueindex(2));
                    bool once_f = lua_toboolean(pL, lua_upvalueindex(3)) != 0;
                    if (!m || !lua_isfunction(pL, 2)) { lua_pushnil(pL); return 1; }
                    lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                    lua_State* mL = (lua_State*)lua_touserdata(pL, -1); lua_pop(pL, 1);
                    if (!mL) mL = pL;
                    int ref = lua_ref(pL, 2);
                    if (auto* lst = m->cb_list(idx)) m->add_cb(*lst, mL, ref, once_f);
                    _gl_push_connection(pL, m, ref);
                    return 1;
                }, once ? "Once" : "Connect", 3);
                lua_setfield(L, -2, once ? "Once" : "Connect");
            }
            return 1;
        }
    }

    // ── Teams / Team (1.14.10) ────────────────────────────────────
    if (Object::cast_to<Teams>(n) && strcmp(key, "GetTeams") == 0) {
        lua_pushlightuserdata(L, (void*)n);
        lua_pushcclosure(L, [](lua_State* pL) -> int {
            Teams* ts = Object::cast_to<Teams>((Node*)lua_touserdata(pL, lua_upvalueindex(1)));
            lua_newtable(pL);
            if (!ts) return 1;
            TypedArray<Node> arr = ts->get_teams();
            for (int i = 0; i < arr.size(); i++) {
                wrap_node(pL, Object::cast_to<Node>(arr[i]));
                lua_rawseti(pL, -2, i + 1);
            }
            return 1;
        }, "GetTeams", 1);
        return 1;
    }
    if (Object::cast_to<Team>(n) && strcmp(key, "GetPlayers") == 0) {
        lua_pushlightuserdata(L, (void*)n);
        lua_pushcclosure(L, [](lua_State* pL) -> int {
            Node* team = (Node*)lua_touserdata(pL, lua_upvalueindex(1));
            lua_newtable(pL);
            if (!team) return 1;
            // Team > Teams > DataModel: Players es un hermano de Teams.
            Node* teams_svc = team->get_parent();
            Node* dm = teams_svc ? teams_svc->get_parent() : nullptr;
            Players* ps = dm ? Object::cast_to<Players>(dm->get_node_or_null("Players")) : nullptr;
            if (!ps) return 1;
            uint64_t tid = (uint64_t)team->get_instance_id();
            int out = 0;
            for (int i = 0; i < ps->get_child_count(); i++) {
                PlayerObject* po = Object::cast_to<PlayerObject>(ps->get_child(i));
                if (!po || po->team_id != tid) continue;
                wrap_node(pL, po);
                lua_rawseti(pL, -2, ++out);
            }
            return 1;
        }, "GetPlayers", 1);
        return 1;
    }

    // ── TextChatService ───────────────────────────────────────────
    TextChatService* tcs = Object::cast_to<TextChatService>(n);
    if (tcs) {
        if (strcmp(key, "SendMessage") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                GodotObjectWrapper* w2 = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                TextChatService* t = w2 ? Object::cast_to<TextChatService>(gow_node(w2)) : nullptr;
                if (t) {
                    // SendMessage(texto) usa el nombre del jugador local;
                    // SendMessage(nombre, texto) sigue funcionando.
                    if (lua_gettop(pL) >= 3 && !lua_isnil(pL, 3)) {
                        t->send_message_from_luau(luaL_checkstring(pL, 2), luaL_checkstring(pL, 3));
                    } else {
                        String msg = luaL_checkstring(pL, 2);
                        String who = t->get_chat() ? t->get_chat()->gl_get_player_name() : String("Player");
                        t->send_message_from_luau(who, msg);
                    }
                }
                return 0;
            }, "SendMessage");
            return 1;
        }
        if (strcmp(key, "MessageReceived") == 0) {
            // Evento como Roblox: TextChatService.MessageReceived:Connect(f)
            // El callback recibe el mensaje como string "Nombre: texto".
            lua_newtable(L);
            lua_pushlightuserdata(L, (void*)tcs);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                TextChatService* t = (TextChatService*)lua_touserdata(pL, lua_upvalueindex(1));
                if (t && lua_isfunction(pL, 2)) {
                    lua_pushvalue(pL, 2);
                    int ref = lua_ref(pL, -1);
                    lua_pop(pL, 1);
                    t->gl_connect_message_received(gl_main_of(pL), ref);
                }
                return 0;
            }, "Connect", 1);
            lua_setfield(L, -2, "Connect");
            return 1;
        }
        if (strcmp(key, "SetPlayerName") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                GodotObjectWrapper* w2 = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                TextChatService* t = w2 ? Object::cast_to<TextChatService>(gow_node(w2)) : nullptr;
                if (t) t->set_player_name(luaL_checkstring(pL, 2));
                return 0;
            }, "SetPlayerName");
            return 1;
        }
    }

    // ── Lighting ──────────────────────────────────────────────────
    Lighting* light_svc = Object::cast_to<Lighting>(n);
    if (light_svc) {
        if (strcmp(key, "Brightness")               == 0) { lua_pushnumber(L,  light_svc->get_brightness());           return 1; }
        if (strcmp(key, "ClockTime")                == 0) { lua_pushnumber(L,  light_svc->get_clock_time());           return 1; }
        if (strcmp(key, "GeographicLatitude")       == 0) { lua_pushnumber(L,  light_svc->get_geographic_latitude());  return 1; }
        if (strcmp(key, "GlobalShadows")            == 0) { lua_pushboolean(L, light_svc->get_global_shadows());       return 1; }
        if (strcmp(key, "ShadowSoftness")           == 0) { lua_pushnumber(L,  light_svc->get_shadow_softness());      return 1; }
        if (strcmp(key, "FogDensity")               == 0) { lua_pushnumber(L,  light_svc->get_fog_density());          return 1; }
        if (strcmp(key, "FogStart")                 == 0) { lua_pushnumber(L,  light_svc->get_fog_start());            return 1; }
        if (strcmp(key, "FogEnd")                   == 0) { lua_pushnumber(L,  light_svc->get_fog_end());              return 1; }
        if (strcmp(key, "ExposureCompensation")     == 0) { lua_pushnumber(L,  light_svc->get_exposure_comp());        return 1; }
        if (strcmp(key, "EnvironmentDiffuseScale")  == 0) { lua_pushnumber(L,  light_svc->get_env_diffuse_scale());    return 1; }
        if (strcmp(key, "EnvironmentSpecularScale") == 0) { lua_pushnumber(L,  light_svc->get_env_specular_scale());   return 1; }
        if (strcmp(key, "Technology")               == 0) { lua_pushnumber(L,  light_svc->get_technology());            return 1; }
        if (strcmp(key, "Ambient")                  == 0) { Color c = light_svc->get_ambient();        push_color3(L, c.r, c.g, c.b); return 1; }
        if (strcmp(key, "OutdoorAmbient")           == 0) { Color c = light_svc->get_outdoor_ambient();push_color3(L, c.r, c.g, c.b); return 1; }
        if (strcmp(key, "FogColor")                 == 0) { Color c = light_svc->get_fog_color();      push_color3(L, c.r, c.g, c.b); return 1; }
        if (strcmp(key, "ColorShift_Top")           == 0) { Color c = light_svc->get_color_shift_top();   push_color3(L, c.r, c.g, c.b); return 1; }
        if (strcmp(key, "ColorShift_Bottom")        == 0) { Color c = light_svc->get_color_shift_bottom();push_color3(L, c.r, c.g, c.b); return 1; }
        if (strcmp(key, "TimeOfDay")                == 0) { lua_pushstring(L, light_svc->get_time_of_day().utf8().get_data()); return 1; }
        if (strcmp(key, "DayNightCycle")            == 0) { lua_pushboolean(L, light_svc->get_day_night_cycle());     return 1; }
        if (strcmp(key, "DayLengthMinutes")         == 0) { lua_pushnumber(L,  light_svc->get_day_length_minutes()); return 1; }
        if (strcmp(key, "GetMinutesAfterMidnight")  == 0) {
            lua_pushlightuserdata(L, (void*)light_svc);
            lua_pushcclosure(L, [](lua_State* L) -> int {
                Lighting* lt = (Lighting*)lua_touserdata(L, lua_upvalueindex(1));
                lua_pushnumber(L, lt ? lt->get_minutes_after_midnight() : 0.0);
                return 1;
            }, "GetMinutesAfterMidnight", 1); return 1;
        }
        if (strcmp(key, "SetMinutesAfterMidnight")  == 0) {
            lua_pushlightuserdata(L, (void*)light_svc);
            lua_pushcclosure(L, [](lua_State* L) -> int {
                Lighting* lt = (Lighting*)lua_touserdata(L, lua_upvalueindex(1));
                if (lt) lt->set_minutes_after_midnight((float)luaL_checknumber(L, 2));
                return 0;
            }, "SetMinutesAfterMidnight", 1); return 1;
        }
    }

    // ── Node3D ────────────────────────────────────────────────────
    Node3D* n3d = Object::cast_to<Node3D>(n);
    if (n3d) {
        if (strcmp(key, "Position") == 0) {
            Vector3 p = n3d->get_global_position();
            push_vector3(L, p.x, p.y, p.z); return 1;
        }
        if (strcmp(key, "Rotation") == 0) {
            Vector3 r = n3d->get_rotation_degrees();
            push_vector3(L, r.x, r.y, r.z); return 1;
        }
        if (strcmp(key, "Scale") == 0) {
            Vector3 s = n3d->get_scale();
            push_vector3(L, s.x, s.y, s.z); return 1;
        }
        if (strcmp(key, "CFrame") == 0) {
            Transform3D t = n3d->get_global_transform();
            // Push as a Lua table matching CFrame structure
            lua_newtable(L);
            lua_pushnumber(L, t.origin.x); lua_setfield(L, -2, "X");
            lua_pushnumber(L, t.origin.y); lua_setfield(L, -2, "Y");
            lua_pushnumber(L, t.origin.z); lua_setfield(L, -2, "Z");
            lua_pushnumber(L, t.basis.rows[0][0]); lua_setfield(L, -2, "m00");
            lua_pushnumber(L, t.basis.rows[0][1]); lua_setfield(L, -2, "m01");
            lua_pushnumber(L, t.basis.rows[0][2]); lua_setfield(L, -2, "m02");
            lua_pushnumber(L, t.basis.rows[1][0]); lua_setfield(L, -2, "m10");
            lua_pushnumber(L, t.basis.rows[1][1]); lua_setfield(L, -2, "m11");
            lua_pushnumber(L, t.basis.rows[1][2]); lua_setfield(L, -2, "m12");
            lua_pushnumber(L, t.basis.rows[2][0]); lua_setfield(L, -2, "m20");
            lua_pushnumber(L, t.basis.rows[2][1]); lua_setfield(L, -2, "m21");
            lua_pushnumber(L, t.basis.rows[2][2]); lua_setfield(L, -2, "m22");
            // Position sub-table
            lua_newtable(L);
            lua_pushnumber(L, t.origin.x); lua_setfield(L, -2, "X");
            lua_pushnumber(L, t.origin.y); lua_setfield(L, -2, "Y");
            lua_pushnumber(L, t.origin.z); lua_setfield(L, -2, "Z");
            lua_pushnumber(L, t.origin.x); lua_setfield(L, -2, "x");
            lua_pushnumber(L, t.origin.y); lua_setfield(L, -2, "y");
            lua_pushnumber(L, t.origin.z); lua_setfield(L, -2, "z");
            lua_setfield(L, -2, "Position");
            // Set CFrame metatable from Lua registry
            lua_getglobal(L, "CFrame");
            if (lua_istable(L, -1)) lua_setmetatable(L, -2);
            else lua_pop(L, 1);
            return 1;
        }
        // workspace:Raycast(origin, direction, [RaycastParams]) — igual que Roblox
        if (strcmp(key, "Raycast") == 0 || strcmp(key, "FindPartOnRay") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                GodotObjectWrapper* wr = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                if (!wr || !gow_node(wr)) { lua_pushnil(pL); return 1; }

                auto read_v3 = [&](int idx) -> Vector3 {
                    if (lua_isuserdata(pL, idx)) {
                        LuauVector3* v = (LuauVector3*)lua_touserdata(pL, idx);
                        return Vector3(v->x, v->y, v->z);
                    } else if (lua_istable(pL, idx)) {
                        auto gf = [&](const char* k) -> float {
                            lua_getfield(pL, idx, k); float v = (float)lua_tonumber(pL,-1); lua_pop(pL,1); return v;
                        };
                        return Vector3(gf("X"), gf("Y"), gf("Z"));
                    }
                    return Vector3();
                };

                Vector3 from = read_v3(2);
                Vector3 dir  = read_v3(3);
                float max_d  = 1000.0f;
                // RaycastParams table at arg 4
                if (lua_istable(pL, 4)) {
                    lua_getfield(pL, 4, "MaxDistance");
                    if (lua_isnumber(pL, -1)) max_d = (float)lua_tonumber(pL, -1);
                    lua_pop(pL, 1);
                } else if (lua_isnumber(pL, 4)) {
                    max_d = (float)lua_tonumber(pL, 4);
                }

                Node3D* ws3d = Object::cast_to<Node3D>(gow_node(wr));
                if (!ws3d || !ws3d->is_inside_tree()) { lua_pushnil(pL); return 1; }
                PhysicsDirectSpaceState3D* space = ws3d->get_world_3d()->get_direct_space_state();
                if (!space) { lua_pushnil(pL); return 1; }

                Vector3 dir_norm = dir.length_squared() > 1e-6f ? dir.normalized() : Vector3(0,0,-1);
                Ref<PhysicsRayQueryParameters3D> params;
                params.instantiate();
                params->set_from(from);
                params->set_to(from + dir_norm * max_d);
                Dictionary res = space->intersect_ray(params);
                if (res.is_empty()) { lua_pushnil(pL); return 1; }
                lua_newtable(pL);
                Variant cv = res.get("collider", Variant());
                Node* hit = nullptr;
                if (cv.get_type() == Variant::OBJECT) hit = Object::cast_to<Node>(cv.operator Object*());
                if (hit) wrap_node(pL, hit); else lua_pushnil(pL);
                lua_setfield(pL, -2, "Instance");
                Vector3 pos    = res.get("position", Vector3());
                Vector3 normal = res.get("normal",   Vector3());
                push_vector3(pL, pos.x,    pos.y,    pos.z);    lua_setfield(pL, -2, "Position");
                push_vector3(pL, normal.x, normal.y, normal.z); lua_setfield(pL, -2, "Normal");
                lua_pushnumber(pL, (double)from.distance_to(pos)); lua_setfield(pL, -2, "Distance");
                lua_pushnil(pL); lua_setfield(pL, -2, "Material");
                return 1;
            }, key);
            return 1;
        }
    }

    // ── RobloxPlayer ──────────────────────────────────────────────
    RobloxPlayer* rp = Object::cast_to<RobloxPlayer>(n);
    if (rp) {
        if (strcmp(key, "CameraMode")              == 0) { lua_pushnumber(L, rp->get_camera_mode());               return 1; }
        if (strcmp(key, "MouseSensitivity")        == 0) { lua_pushnumber(L, rp->get_mouse_sensitivity());         return 1; }
        if (strcmp(key, "UserId")                  == 0) { lua_pushnumber(L, rp->get_user_id());                   return 1; }
        if (strcmp(key, "DisplayName")             == 0) { lua_pushstring(L, rp->get_display_name().utf8().get_data()); return 1; }
        if (strcmp(key, "Name")                    == 0) { lua_pushstring(L, rp->get_display_name().utf8().get_data()); return 1; }
        if (strcmp(key, "AccountAge")              == 0) { lua_pushnumber(L, rp->get_account_age());               return 1; }
        if (strcmp(key, "AutoJumpEnabled")         == 0) { lua_pushboolean(L, rp->get_auto_jump_enabled());        return 1; }
        if (strcmp(key, "DevCameraOcclusionMode")  == 0) { lua_pushnumber(L, rp->get_dev_camera_occlusion_mode()); return 1; }
        if (strcmp(key, "DevComputerMovementMode") == 0) { lua_pushnumber(L, rp->get_dev_computer_movement_mode()); return 1; }
        if (strcmp(key, "DevTouchMovementMode")    == 0) { lua_pushnumber(L, rp->get_dev_touch_movement_mode());   return 1; }
        if (strcmp(key, "TeamColor") == 0) {
            Color c = rp->get_team_color();
            push_color3(L, c.r, c.g, c.b); return 1;
        }
        // Character: find the RobloxPlayer node itself as the character model
        if (strcmp(key, "Character") == 0) {
            // The RobloxPlayer IS the character in our implementation
            wrap_node(L, rp); return 1;
        }
        if (strcmp(key, "GetCharacterAppearanceInfoAsync") == 0 ||
            strcmp(key, "GetCharacterAppearanceAsync") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int { lua_newtable(pL); return 1; }, key);
            return 1;
        }
        if (strcmp(key, "GetFriendStatus") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int { lua_pushnumber(pL, 0); return 1; }, "GetFriendStatus");
            return 1;
        }
        if (strcmp(key, "GetFriendsOnline") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int { lua_newtable(pL); return 1; }, "GetFriendsOnline");
            return 1;
        }
        if (strcmp(key, "Kick") == 0) {
            lua_pushlightuserdata(L, (void*)rp);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RobloxPlayer* p = (RobloxPlayer*)lua_touserdata(pL, lua_upvalueindex(1));
                const char* reason = lua_isstring(pL,2) ? lua_tostring(pL,2) : "Kicked";
                if (p) UtilityFunctions::print("[Player] Kicked: ", String(reason));
                return 0;
            }, "Kick", 1);
            return 1;
        }
        if (strcmp(key, "CharacterAdded") == 0 || strcmp(key, "CharacterRemoving") == 0) {
            const char* sig = key;
            lua_newtable(L);
            lua_pushcclosure(L, [](lua_State* pL) -> int { return 0; }, "Connect", 0);
            lua_setfield(L, -2, "Connect");
            return 1;
        }
    }

    // ── RobloxPlayer2D ────────────────────────────────────────────
    RobloxPlayer2D* rp2d = Object::cast_to<RobloxPlayer2D>(n);
    if (rp2d) {
        if (strcmp(key, "CameraMode")    == 0) { lua_pushnumber(L, rp2d->get_camera_mode());    return 1; }
        if (strcmp(key, "MovementType")  == 0) { lua_pushnumber(L, rp2d->get_movement_type());  return 1; }
        if (strcmp(key, "WalkSpeed")     == 0) { lua_pushnumber(L, rp2d->get_walk_speed());     return 1; }
        if (strcmp(key, "JumpPower")     == 0) { lua_pushnumber(L, rp2d->get_jump_power());     return 1; }
    }

    // ── Humanoid 3D ───────────────────────────────────────────────
    Humanoid* hum = Object::cast_to<Humanoid>(n);
    if (hum) {
        if (strcmp(key, "Health")                 == 0) { lua_pushnumber(L, hum->get_health());                  return 1; }
        if (strcmp(key, "MaxHealth")              == 0) { lua_pushnumber(L, hum->get_max_health());              return 1; }
        if (strcmp(key, "WalkSpeed")              == 0) { lua_pushnumber(L, hum->get_walk_speed());              return 1; }
        if (strcmp(key, "JumpPower")              == 0) { lua_pushnumber(L, hum->get_jump_power());              return 1; }
        if (strcmp(key, "JumpHeight")             == 0) { lua_pushnumber(L, hum->get_jump_height());             return 1; }
        if (strcmp(key, "HipHeight")              == 0) { lua_pushnumber(L, hum->get_hip_height());              return 1; }
        if (strcmp(key, "Gravity")                == 0) { lua_pushnumber(L, hum->get_gravity_val());             return 1; }
        if (strcmp(key, "IsDead")                 == 0) { lua_pushboolean(L, hum->is_character_dead());          return 1; }
        if (strcmp(key, "FloorMaterial")          == 0) { lua_pushnumber(L, 256);                               return 1; } // Plastic por defecto
        if (strcmp(key, "MoveDirection")          == 0) { Vector3 md = hum->get_move_direction(); push_vector3(L, md.x, md.y, md.z); return 1; }
        if (strcmp(key, "AutoRotate")             == 0) { lua_pushboolean(L, hum->get_auto_rotate());            return 1; }
        if (strcmp(key, "AutoJumpEnabled")        == 0) { lua_pushboolean(L, hum->get_auto_jump_enabled());      return 1; }
        if (strcmp(key, "DisplayName")            == 0) { lua_pushstring(L, hum->get_display_name().utf8().get_data()); return 1; }
        if (strcmp(key, "NameDisplayDistance")    == 0) { lua_pushnumber(L, hum->get_name_display_distance());   return 1; }
        if (strcmp(key, "HealthDisplayDistance")  == 0) { lua_pushnumber(L, hum->get_health_display_distance()); return 1; }
        if (strcmp(key, "RigType")                == 0) { lua_pushnumber(L, hum->get_rig_type());                return 1; }
        if (strcmp(key, "EvaluateStateMachine")   == 0) { lua_pushboolean(L, hum->get_evaluate_state_machine()); return 1; }
        if (strcmp(key, "BreakJointsOnDeath")     == 0) { lua_pushboolean(L, hum->get_break_joints_on_death());  return 1; }
        if (strcmp(key, "RequiresNeck")           == 0) { lua_pushboolean(L, hum->get_requires_neck());          return 1; }

        if (strcmp(key, "TakeDamage") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                GodotObjectWrapper* w2 = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                Humanoid* h = w2 ? Object::cast_to<Humanoid>(gow_node(w2)) : nullptr;
                if (h) h->take_damage((float)luaL_checknumber(pL, 2));
                return 0;
            }, "TakeDamage");
            return 1;
        }
        if (strcmp(key, "Heal") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                GodotObjectWrapper* w2 = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                Humanoid* h = w2 ? Object::cast_to<Humanoid>(gow_node(w2)) : nullptr;
                if (h) h->heal((float)luaL_checknumber(pL, 2));
                return 0;
            }, "Heal");
            return 1;
        }
        // ── Additional Humanoid methods ───────────────────────────
        //// ── Métodos adicionales de Humanoid ───────────────────────
        if (strcmp(key, "Jump") == 0) {
            lua_pushlightuserdata(L, (void*)hum);
            lua_pushcclosure(L, [](lua_State* pL)->int{
                Humanoid* h=(Humanoid*)lua_touserdata(pL,lua_upvalueindex(1));
                if(h) h->change_state(2); // Jumping
                return 0;
            },"Jump",1); return 1;
        }
        if (strcmp(key, "Sit") == 0) {
            lua_pushlightuserdata(L, (void*)hum);
            lua_pushcclosure(L, [](lua_State* pL)->int{
                Humanoid* h=(Humanoid*)lua_touserdata(pL,lua_upvalueindex(1));
                if(h) h->change_state(6); // Seated
                return 0;
            },"Sit",1); return 1;
        }
        if (strcmp(key, "Move") == 0) {
            lua_pushlightuserdata(L, (void*)hum);
            lua_pushcclosure(L, [](lua_State* pL)->int{
                // Humanoid:Move(direction, relativeToCamera)
                // Apply movement direction — calls change_state(1)
                Humanoid* h=(Humanoid*)lua_touserdata(pL,lua_upvalueindex(1));
                if(h) h->change_state(1);
                return 0;
            },"Move",1); return 1;
        }
        if (strcmp(key, "ApplyDescription") == 0) {
            lua_pushcfunction(L, [](lua_State* pL)->int{ return 0; }, "ApplyDescription"); return 1;
        }
        if (strcmp(key, "GetAppliedDescription") == 0) {
            lua_pushcfunction(L, [](lua_State* pL)->int{ lua_pushnil(pL); return 1; }, "GetAppliedDescription"); return 1;
        }

        if (strcmp(key, "GetState") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                GodotObjectWrapper* w2 = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                Humanoid* h = w2 ? Object::cast_to<Humanoid>(gow_node(w2)) : nullptr;
                lua_pushnumber(pL, h ? h->get_state() : 1);
                return 1;
            }, "GetState");
            return 1;
        }
        if (strcmp(key, "ChangeState") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                GodotObjectWrapper* w2 = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                Humanoid* h = w2 ? Object::cast_to<Humanoid>(gow_node(w2)) : nullptr;
                if (h) h->change_state((int)luaL_checknumber(pL, 2));
                return 0;
            }, "ChangeState");
            return 1;
        }
        if (strcmp(key, "LoadAnimation") == 0) {
            lua_pushlightuserdata(L, (void*)hum);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                Humanoid* h = (Humanoid*)lua_touserdata(pL, lua_upvalueindex(1));
                if (!h) { lua_pushnil(pL); return 1; }
                // Find or create AnimationPlayer in parent
                Node* parent = h->get_parent();
                if (!parent) { lua_pushnil(pL); return 1; }
                AnimationPlayer* ap = nullptr;
                for (int i = 0; i < parent->get_child_count(); i++) {
                    ap = Object::cast_to<AnimationPlayer>(parent->get_child(i));
                    if (ap) break;
                }
                if (!ap) {
                    ap = memnew(AnimationPlayer);
                    ap->set_name("AnimationPlayer");
                    parent->add_child(ap);
                }
                // Get animation name from arg (AnimationObject or string)
                String anim_name_s = "default";
                GodotObjectWrapper* aw = (GodotObjectWrapper*)lua_touserdata(pL, 2);
                if (aw && gow_node(aw)) {
                    AnimationObject* ao = Object::cast_to<AnimationObject>(gow_node(aw));
                    if (ao) {
                        anim_name_s = ao->get_anim_name();
                        // Load the animation from file if specified
                        String aid = ao->get_animation_id();
                        if (!aid.is_empty() && !ap->has_animation(StringName(anim_name_s))) {
                            Ref<Animation> loaded_anim = ResourceLoader::get_singleton()->load(aid, "Animation");
                            if (loaded_anim.is_valid()) {
                                Ref<AnimationLibrary> lib;
                                lib.instantiate();
                                lib->add_animation(StringName(anim_name_s), loaded_anim);
                                ap->add_animation_library(StringName(""), lib);
                            }
                        }
                    }
                } else if (lua_isstring(pL, 2)) {
                    anim_name_s = String(lua_tostring(pL, 2));
                }
                AnimationTrack* track = memnew(AnimationTrack);
                track->set_name("__anim_track__");
                track->setup(anim_name_s, ap);
                h->add_child(track);
                // Wrap and return
                GodotObjectWrapper* tw = (GodotObjectWrapper*)lua_newuserdata(pL, sizeof(GodotObjectWrapper));
                gow_set(tw, track);
                luaL_getmetatable(pL, "GodotObject");
                lua_setmetatable(pL, -2);
                return 1;
            }, "LoadAnimation", 1);
            return 1;
        }
        if (strcmp(key, "WalkToPoint") == 0) {
            lua_pushlightuserdata(L, (void*)hum);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                Humanoid* h = (Humanoid*)lua_touserdata(pL, lua_upvalueindex(1));
                if (!h) return 0;
                float tx = 0; float ty = 0; float tz = 0;
                if (lua_istable(pL,2)) {
                    lua_getfield(pL,2,"X"); tx=(float)lua_tonumber(pL,-1); lua_pop(pL,1);
                    lua_getfield(pL,2,"Y"); ty=(float)lua_tonumber(pL,-1); lua_pop(pL,1);
                    lua_getfield(pL,2,"Z"); tz=(float)lua_tonumber(pL,-1); lua_pop(pL,1);
                }
                Node* parent = h->get_parent();
                Node3D* body3d = parent ? Object::cast_to<Node3D>(parent) : nullptr;
                if (body3d) {
                    Vector3 pos = body3d->get_global_position();
                    Vector3 dir = Vector3(tx - pos.x, 0, tz - pos.z);
                    if (dir.length_squared() > 0.01f) h->set_npc_move_dir(dir.normalized());
                    else h->stop_npc_movement();
                }
                return 0;
            }, "WalkToPoint", 1);
            return 1;
        }
        if (strcmp(key, "WalkToPart") == 0) {
            lua_pushlightuserdata(L, (void*)hum);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                Humanoid* h = (Humanoid*)lua_touserdata(pL, lua_upvalueindex(1));
                if (!h) return 0;
                GodotObjectWrapper* pw = (GodotObjectWrapper*)lua_touserdata(pL, 2);
                if (pw && gow_node(pw)) {
                    Node3D* target = Object::cast_to<Node3D>(gow_node(pw));
                    Node* parent = h->get_parent();
                    Node3D* body3d = parent ? Object::cast_to<Node3D>(parent) : nullptr;
                    if (target && body3d) {
                        Vector3 dir = (target->get_global_position() - body3d->get_global_position());
                        dir.y = 0;
                        if (dir.length_squared() > 0.01f) h->set_npc_move_dir(dir.normalized());
                        else h->stop_npc_movement();
                    }
                }
                return 0;
            }, "WalkToPart", 1);
            return 1;
        }
        if (strcmp(key, "StopNPCMovement") == 0 || strcmp(key, "MoveTo") == 0) {
            lua_pushlightuserdata(L, (void*)hum);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                Humanoid* h = (Humanoid*)lua_touserdata(pL, lua_upvalueindex(1));
                if (h) h->stop_npc_movement();
                return 0;
            }, key, 1);
            return 1;
        }
        if (strcmp(key, "GetHumanoidDescriptionFromUserId") == 0 ||
            strcmp(key, "GetHumanoidDescriptionFromOutfitId") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int { lua_newtable(pL); return 1; }, key);
            return 1;
        }

        // ── Humanoid signals ──────────────────────────────────────
        //// ── Señales del Humanoid ──────────────────────────────────
        if (strcmp(key, "Died") == 0) {
            lua_newtable(L);
            lua_pushlightuserdata(L, (void*)hum);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                Humanoid* h = static_cast<Humanoid*>(lua_touserdata(pL, lua_upvalueindex(1)));
                int fn_pos = -1;
                for (int ai = 1; ai <= lua_gettop(pL); ai++) {
                    if (lua_isfunction(pL, ai)) { fn_pos = ai; break; }
                }
                if (fn_pos == -1 || !h) return 0;
                lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                lua_State* main_L = static_cast<lua_State*>(lua_touserdata(pL, -1));
                lua_pop(pL, 1);
                if (!main_L) main_L = pL;
                lua_pushvalue(pL, fn_pos);
                int ref = lua_ref(pL, -1);
                lua_pop(pL, 1);
                h->add_died_callback(main_L, ref);
                _gl_push_connection(pL, h, ref); return 1;
            }, "Connect", 1);
            lua_setfield(L, -2, "Connect");
            return 1;
        }
        if (strcmp(key, "HealthChanged") == 0) {
            lua_newtable(L);
            lua_pushlightuserdata(L, (void*)hum);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                Humanoid* h = static_cast<Humanoid*>(lua_touserdata(pL, lua_upvalueindex(1)));
                int fn_pos = -1;
                for (int ai = 1; ai <= lua_gettop(pL); ai++) {
                    if (lua_isfunction(pL, ai)) { fn_pos = ai; break; }
                }
                if (fn_pos == -1 || !h) return 0;
                lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                lua_State* main_L = static_cast<lua_State*>(lua_touserdata(pL, -1));
                lua_pop(pL, 1);
                if (!main_L) main_L = pL;
                lua_pushvalue(pL, fn_pos);
                int ref = lua_ref(pL, -1);
                lua_pop(pL, 1);
                h->add_health_changed_callback(main_L, ref);
                _gl_push_connection(pL, h, ref); return 1;
            }, "Connect", 1);
            lua_setfield(L, -2, "Connect");
            return 1;
        }
        if (strcmp(key, "StateChanged") == 0) {
            lua_newtable(L);
            lua_pushlightuserdata(L, (void*)hum);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                Humanoid* h = static_cast<Humanoid*>(lua_touserdata(pL, lua_upvalueindex(1)));
                int fn_pos = -1;
                for (int ai = 1; ai <= lua_gettop(pL); ai++) {
                    if (lua_isfunction(pL, ai)) { fn_pos = ai; break; }
                }
                if (fn_pos == -1 || !h) return 0;
                lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                lua_State* main_L = static_cast<lua_State*>(lua_touserdata(pL, -1));
                lua_pop(pL, 1);
                if (!main_L) main_L = pL;
                lua_pushvalue(pL, fn_pos);
                int ref = lua_ref(pL, -1);
                lua_pop(pL, 1);
                h->add_state_changed_callback(main_L, ref);
                _gl_push_connection(pL, h, ref); return 1;
            }, "Connect", 1);
            lua_setfield(L, -2, "Connect");
            return 1;
        }
    }

    // ── Humanoid 2D ───────────────────────────────────────────────
    Humanoid2D* hum2d = Object::cast_to<Humanoid2D>(n);
    if (hum2d) {
        if (strcmp(key, "Health")    == 0) { lua_pushnumber(L, hum2d->get_health());     return 1; }
        if (strcmp(key, "MaxHealth") == 0) { lua_pushnumber(L, hum2d->get_max_health()); return 1; }
        if (strcmp(key, "IsDead")    == 0) { lua_pushboolean(L, hum2d->get_health() <= 0.0f); return 1; }

        // WalkSpeed/JumpPower delegan al RobloxPlayer2D padre
        if (strcmp(key, "WalkSpeed") == 0) {
            RobloxPlayer2D* parent_player = Object::cast_to<RobloxPlayer2D>(n->get_parent());
            lua_pushnumber(L, parent_player ? parent_player->get_walk_speed() : 200.0);
            return 1;
        }
        if (strcmp(key, "JumpPower") == 0) {
            RobloxPlayer2D* parent_player = Object::cast_to<RobloxPlayer2D>(n->get_parent());
            // Return positive even if internally stored as negative (same convention as Roblox) / Devolver positivo aunque internamente sea negativo (misma convención que Roblox)
            lua_pushnumber(L, parent_player ? (double)(-parent_player->get_jump_power()) : 500.0);
            return 1;
        }

        if (strcmp(key, "TakeDamage") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                GodotObjectWrapper* w2 = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                Humanoid2D* h = w2 ? Object::cast_to<Humanoid2D>(gow_node(w2)) : nullptr;
                if (h) h->take_damage((float)luaL_checknumber(pL, 2));
                return 0;
            }, "TakeDamage");
            return 1;
        }
        if (strcmp(key, "Heal") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                GodotObjectWrapper* w2 = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                Humanoid2D* h = w2 ? Object::cast_to<Humanoid2D>(gow_node(w2)) : nullptr;
                if (h) h->heal((float)luaL_checknumber(pL, 2));
                return 0;
            }, "Heal");
            return 1;
        }
    }

    // ── RunService ────────────────────────────────────────────────
    // ── NetworkService (multijugador, Fase 0) ─────────────────────────
    NetworkService* nsvc = Object::cast_to<NetworkService>(n);
    if (nsvc) {
        if (strcmp(key, "StartServer") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                int port = (int)luaL_optnumber(pL, 2, 25565);
                int maxp = (int)luaL_optnumber(pL, 3, 32);
                lua_pushboolean(pL, s && s->start_server(port, maxp)); return 1;
            }, "StartServer", 1); return 1;
        }
        if (strcmp(key, "Connect") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                const char* ip = luaL_checkstring(pL, 2);
                int port = (int)luaL_optnumber(pL, 3, 25565);
                lua_pushboolean(pL, s && s->connect_server(String(ip), port)); return 1;
            }, "Connect", 1); return 1;
        }
        if (strcmp(key, "Disconnect") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                if (s) s->disconnect_net(); return 0;
            }, "Disconnect", 1); return 1;
        }
        if (strcmp(key, "IsServer") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                lua_pushboolean(pL, s && s->is_server()); return 1;
            }, "IsServer", 1); return 1;
        }
        if (strcmp(key, "IsClient") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                lua_pushboolean(pL, s && s->is_client()); return 1;
            }, "IsClient", 1); return 1;
        }
        if (strcmp(key, "GetPeerId") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                lua_pushnumber(pL, s ? s->get_peer_id() : 0); return 1;
            }, "GetPeerId", 1); return 1;
        }
        if (strcmp(key, "GetPlayerCount") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                lua_pushnumber(pL, s ? s->get_player_count() : 1); return 1;
            }, "GetPlayerCount", 1); return 1;
        }
        if (strcmp(key, "GetDevice") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                lua_pushstring(pL, s ? s->get_device().utf8().get_data() : "PC"); return 1;
            }, "GetDevice", 1); return 1;
        }
        if (strcmp(key, "GetPlayerIndex") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                lua_pushnumber(pL, s ? s->get_player_index() : 0); return 1;
            }, "GetPlayerIndex", 1); return 1;
        }
        if (strcmp(key, "IsConnected") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                lua_pushboolean(pL, s && s->is_connected_net()); return 1;
            }, "IsConnected", 1); return 1;
        }
        if (strcmp(key, "GetServerAddress") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                lua_pushstring(pL, s ? s->get_server_address().utf8().get_data() : ""); return 1;
            }, "GetServerAddress", 1); return 1;
        }
        if (strcmp(key, "GetConnectionState") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                lua_pushstring(pL, s ? s->get_connection_state().utf8().get_data() : "Disconnected"); return 1;
            }, "GetConnectionState", 1); return 1;
        }
        if (strcmp(key, "IsDedicatedServer") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                lua_pushboolean(pL, s && s->is_dedicated_server()); return 1;
            }, "IsDedicatedServer", 1); return 1;
        }
        if (strcmp(key, "GetLocalIP") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                lua_pushstring(pL, s ? s->get_local_ip().utf8().get_data() : "127.0.0.1"); return 1;
            }, "GetLocalIP", 1); return 1;
        }
        // ── Lista de servidores guardados (estilo lista de Minecraft) ──
        if (strcmp(key, "GetServers") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                lua_newtable(pL);
                if (s) {
                    Array list = s->get_server_list();
                    for (int i = 0; i < list.size(); i++) {
                        Dictionary d = list[i];
                        lua_newtable(pL);
                        lua_pushstring(pL, String(d.get("name", "")).utf8().get_data()); lua_setfield(pL, -2, "Name");
                        lua_pushstring(pL, String(d.get("ip", "")).utf8().get_data());   lua_setfield(pL, -2, "IP");
                        lua_pushnumber(pL, (double)(int)d.get("port", 25565));           lua_setfield(pL, -2, "Port");
                        lua_rawseti(pL, -2, i + 1);
                    }
                }
                return 1;
            }, "GetServers", 1); return 1;
        }
        if (strcmp(key, "AddServer") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                const char* name = luaL_checkstring(pL, 2);
                const char* ip   = luaL_checkstring(pL, 3);
                int port = (int)luaL_optnumber(pL, 4, 25565);
                if (s) s->add_server(String(name), String(ip), port);
                return 0;
            }, "AddServer", 1); return 1;
        }
        if (strcmp(key, "RemoveServer") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                if (s) s->remove_server(String(luaL_checkstring(pL, 2)));
                return 0;
            }, "RemoveServer", 1); return 1;
        }
        if (strcmp(key, "JoinServer") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                lua_pushboolean(pL, s && s->join_server(String(luaL_checkstring(pL, 2)))); return 1;
            }, "JoinServer", 1); return 1;
        }
        // ── Coordinador de mundos (matchmaking estilo Roblox) ──
        if (strcmp(key, "StartHost") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                int port = (int)luaL_optnumber(pL, 2, 25565);
                int maxp = (int)luaL_optnumber(pL, 3, 8);
                lua_pushboolean(pL, s && s->start_coordinator(port, maxp)); return 1;
            }, "StartHost", 1); return 1;
        }
        if (strcmp(key, "JoinMatchmaking") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                const char* ip = luaL_checkstring(pL, 2);
                int port = (int)luaL_optnumber(pL, 3, 25565);
                lua_pushboolean(pL, s && s->join_matchmaking(String(ip), port)); return 1;
            }, "JoinMatchmaking", 1); return 1;
        }
        if (strcmp(key, "GenerateHostKey") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                lua_pushstring(pL, s ? s->read_or_make_host_key().utf8().get_data() : ""); return 1;
            }, "GenerateHostKey", 1); return 1;
        }
        if (strcmp(key, "GetHostKey") == 0) {
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                lua_pushstring(pL, s ? s->get_host_key().utf8().get_data() : ""); return 1;
            }, "GetHostKey", 1); return 1;
        }
        if (strcmp(key, "PlayerConnected") == 0 || strcmp(key, "PlayerDisconnected") == 0 ||
            strcmp(key, "Connected") == 0       || strcmp(key, "ConnectionFailed") == 0) {
            int which = (strcmp(key,"PlayerConnected")==0) ? 0 :
                        (strcmp(key,"PlayerDisconnected")==0) ? 1 :
                        (strcmp(key,"Connected")==0) ? 2 : 3;
            lua_newtable(L);
            lua_pushlightuserdata(L, (void*)nsvc);
            lua_pushinteger(L, which);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                NetworkService* s = (NetworkService*)lua_touserdata(pL, lua_upvalueindex(1));
                int w = (int)lua_tointeger(pL, lua_upvalueindex(2));
                if (!s || !lua_isfunction(pL, 2)) { lua_pushnil(pL); return 1; }
                lua_pushvalue(pL, 2); int ref = lua_ref(pL, -1); lua_pop(pL, 1);
                if      (w == 0) s->add_pc_cb(pL, ref);
                else if (w == 1) s->add_pd_cb(pL, ref);
                else if (w == 2) s->add_conn_cb(pL, ref);
                else             s->add_fail_cb(pL, ref);
                lua_newtable(pL);
                lua_pushcfunction(pL, [](lua_State*) -> int { return 0; }, "Disconnect");
                lua_setfield(pL, -2, "Disconnect");
                return 1;
            }, "Connect", 2);
            lua_setfield(L, -2, "Connect");
            return 1;
        }
    }

    // ── RunService ────────────────────────────────────────────────
    RunService* rs = Object::cast_to<RunService>(n);
    if (rs) {
        if (strcmp(key, "IsRunning") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                lua_pushboolean(pL, !Engine::get_singleton()->is_editor_hint()); return 1;
            }, "IsRunning");
            return 1;
        }
        if (strcmp(key, "IsClient") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                lua_getglobal(pL, "__IS_CLIENT"); bool v = lua_toboolean(pL, -1) != 0; lua_pop(pL, 1);
                lua_pushboolean(pL, v); return 1;
            }, "IsClient");
            return 1;
        }
        if (strcmp(key, "IsServer") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                lua_getglobal(pL, "__IS_SERVER"); bool v = lua_toboolean(pL, -1) != 0; lua_pop(pL, 1);
                lua_pushboolean(pL, v); return 1;
            }, "IsServer");
            return 1;
        }
        if (strcmp(key, "IsStudio") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                lua_pushboolean(pL, Engine::get_singleton()->is_editor_hint()); return 1;
            }, "IsStudio");
            return 1;
        }
        // Signals: Heartbeat, RenderStepped, Stepped / Señales: Heartbeat, RenderStepped, Stepped
        if (strcmp(key, "Heartbeat") == 0 || strcmp(key, "RenderStepped") == 0 || strcmp(key, "Stepped") == 0) {
            lua_newtable(L);
            lua_pushlightuserdata(L, (void*)rs);
            lua_pushstring(L, key);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RunService* rs_ptr = static_cast<RunService*>(lua_touserdata(pL, lua_upvalueindex(1)));
                const char* ev = lua_tostring(pL, lua_upvalueindex(2));
                int fn_pos = -1;
                for (int ai = 1; ai <= lua_gettop(pL); ai++) {
                    if (lua_isfunction(pL, ai)) { fn_pos = ai; break; }
                }
                if (fn_pos == -1 || !rs_ptr) return 0;
                lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                lua_State* main_L = static_cast<lua_State*>(lua_touserdata(pL, -1));
                lua_pop(pL, 1);
                if (!main_L) main_L = pL;
                lua_pushvalue(pL, fn_pos);
                int ref = lua_ref(pL, -1);
                lua_pop(pL, 1);
                if      (strcmp(ev, "Heartbeat")     == 0) rs_ptr->add_heartbeat(main_L, ref);
                else if (strcmp(ev, "RenderStepped") == 0) rs_ptr->add_render_stepped(main_L, ref);
                else if (strcmp(ev, "Stepped")       == 0) rs_ptr->add_stepped(main_L, ref);
                // Return connection object: { Disconnect(), Connected }
                lua_newtable(pL);
                lua_pushlightuserdata(pL, (void*)rs_ptr);
                lua_pushstring(pL, ev);
                lua_pushinteger(pL, ref);
                lua_pushcclosure(pL, [](lua_State* dL) -> int {
                    RunService* rs = static_cast<RunService*>(lua_touserdata(dL, lua_upvalueindex(1)));
                    const char* signal = lua_tostring(dL, lua_upvalueindex(2));
                    int r = (int)lua_tointeger(dL, lua_upvalueindex(3));
                    if (rs) rs->disconnect_by_ref(signal, r);
                    return 0;
                }, "Disconnect", 3);
                lua_setfield(pL, -2, "Disconnect");
                lua_pushboolean(pL, true);
                lua_setfield(pL, -2, "Connected");
                return 1;
            }, "Connect", 2);
            lua_setfield(L, -2, "Connect");
            // Once — calls the function only the first time, then disconnects / llama la función solo la primera vez, luego se desconecta
            lua_pushlightuserdata(L, (void*)rs);
            lua_pushstring(L, key);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RunService* rs_ptr = static_cast<RunService*>(lua_touserdata(pL, lua_upvalueindex(1)));
                const char* ev = lua_tostring(pL, lua_upvalueindex(2));
                int fn_pos = -1;
                for (int ai = 1; ai <= lua_gettop(pL); ai++) {
                    if (lua_isfunction(pL, ai)) { fn_pos = ai; break; }
                }
                if (fn_pos == -1 || !rs_ptr) return 0;
                lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                lua_State* main_L = static_cast<lua_State*>(lua_touserdata(pL, -1));
                lua_pop(pL, 1);
                if (!main_L) main_L = pL;
                lua_pushvalue(pL, fn_pos);
                int ref = lua_ref(pL, -1);
                lua_pop(pL, 1);
                if      (strcmp(ev, "Heartbeat")     == 0) rs_ptr->add_heartbeat(main_L, ref, true);
                else if (strcmp(ev, "RenderStepped") == 0) rs_ptr->add_render_stepped(main_L, ref, true);
                else if (strcmp(ev, "Stepped")       == 0) rs_ptr->add_stepped(main_L, ref, true);
                // Return connection object (same shape as Connect for consistency)
                lua_newtable(pL);
                lua_pushlightuserdata(pL, (void*)rs_ptr);
                lua_pushstring(pL, ev);
                lua_pushinteger(pL, ref);
                lua_pushcclosure(pL, [](lua_State* dL) -> int {
                    RunService* rs = static_cast<RunService*>(lua_touserdata(dL, lua_upvalueindex(1)));
                    const char* signal = lua_tostring(dL, lua_upvalueindex(2));
                    int r = (int)lua_tointeger(dL, lua_upvalueindex(3));
                    if (rs) rs->disconnect_by_ref(signal, r);
                    return 0;
                }, "Disconnect", 3);
                lua_setfield(pL, -2, "Disconnect");
                lua_pushboolean(pL, true);
                lua_setfield(pL, -2, "Connected");
                return 1;
            }, "Once", 2);
            lua_setfield(L, -2, "Once");
            // Wait() — yields until the next signal fires, returns delta
            lua_pushlightuserdata(L, (void*)rs);
            lua_pushstring(L, key);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RunService* rs_ptr = static_cast<RunService*>(lua_touserdata(pL, lua_upvalueindex(1)));
                const char* ev = lua_tostring(pL, lua_upvalueindex(2));
                if (!rs_ptr) { lua_pushnumber(pL, 0); return 1; }
                lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                lua_State* main_L = (lua_State*)lua_touserdata(pL, -1);
                lua_pop(pL, 1);
                if (!main_L) main_L = pL;
                // Register this coroutine for one-shot resumption on next fire
                if      (strcmp(ev, "Heartbeat")     == 0) rs_ptr->add_heartbeat_wait(pL, main_L);
                else if (strcmp(ev, "RenderStepped") == 0) rs_ptr->add_render_stepped_wait(pL, main_L);
                else if (strcmp(ev, "Stepped")       == 0) rs_ptr->add_stepped_wait(pL, main_L);
                // Yield with sentinel so _process won't auto-resume via timer
                lua_pushstring(pL, "__WAIT_SIGNAL__");
                return lua_yield(pL, 1);
            }, "Wait", 2);
            lua_setfield(L, -2, "Wait");
            return 1;
        }
    }

    // ── RobloxPart ────────────────────────────────────────────────
    RobloxPart* part = Object::cast_to<RobloxPart>(n);
    if (part) {
        if (strcmp(key, "Anchored")          == 0) { lua_pushboolean(L, part->get_anchored());      return 1; }
        if (strcmp(key, "CanCollide")        == 0) { lua_pushboolean(L, part->get_can_collide());   return 1; }
        if (strcmp(key, "CanTouch")          == 0) { lua_pushboolean(L, part->get_can_touch());     return 1; }
        if (strcmp(key, "CanQuery")          == 0) { lua_pushboolean(L, part->get_can_query());     return 1; }
        if (strcmp(key, "CastShadow")        == 0) { lua_pushboolean(L, part->get_cast_shadow());   return 1; }
        if (strcmp(key, "Locked")            == 0) { lua_pushboolean(L, part->get_locked());        return 1; }
        if (strcmp(key, "Massless")          == 0) { lua_pushboolean(L, part->get_massless());      return 1; }
        if (strcmp(key, "Transparency")      == 0) { lua_pushnumber(L, part->get_transparency());   return 1; }
        if (strcmp(key, "Reflectance")       == 0) { lua_pushnumber(L, part->get_reflectance());    return 1; }
        if (strcmp(key, "Material")          == 0) { lua_pushnumber(L, part->get_roblox_material());return 1; }
        if (strcmp(key, "Shape")             == 0) { lua_pushnumber(L, part->get_roblox_shape());   return 1; }
        if (strcmp(key, "Density")           == 0) { lua_pushnumber(L, part->get_density());        return 1; }
        if (strcmp(key, "Friction")          == 0) { lua_pushnumber(L, part->get_friction_val());   return 1; }
        if (strcmp(key, "Elasticity")        == 0) { lua_pushnumber(L, part->get_elasticity());     return 1; }
        if (strcmp(key, "FrictionWeight")    == 0) { lua_pushnumber(L, part->get_friction_weight());return 1; }
        if (strcmp(key, "ElasticityWeight")  == 0) { lua_pushnumber(L, part->get_elasticity_weight()); return 1; }
        if (strcmp(key, "Mass")              == 0) { lua_pushnumber(L, part->get_mass_roblox());    return 1; }
        if (strcmp(key, "TopSurface")        == 0) { lua_pushnumber(L, part->get_top_surface());    return 1; }
        if (strcmp(key, "BottomSurface")     == 0) { lua_pushnumber(L, part->get_bottom_surface()); return 1; }
        if (strcmp(key, "FrontSurface")      == 0) { lua_pushnumber(L, part->get_front_surface());  return 1; }
        if (strcmp(key, "BackSurface")       == 0) { lua_pushnumber(L, part->get_back_surface());   return 1; }
        if (strcmp(key, "LeftSurface")       == 0) { lua_pushnumber(L, part->get_left_surface());   return 1; }
        if (strcmp(key, "RightSurface")      == 0) { lua_pushnumber(L, part->get_right_surface());  return 1; }

        if (strcmp(key, "Color") == 0 || strcmp(key, "BrickColor") == 0) {
            Color c = part->get_color();
            push_color3(L, c.r, c.g, c.b); return 1;
        }
        if (strcmp(key, "Size") == 0) {
            Vector3 s = part->get_size();
            push_vector3(L, s.x, s.y, s.z); return 1;
        }
        if (strcmp(key, "Position") == 0) {
            Vector3 p = part->get_global_position();
            push_vector3(L, p.x, p.y, p.z); return 1;
        }
        if (strcmp(key, "Orientation") == 0) {
            Vector3 r = part->get_rotation_degrees();
            push_vector3(L, r.x, r.y, r.z); return 1;
        }
        if (strcmp(key, "Velocity") == 0) {
            Vector3 v = part->get_linear_velocity();
            push_vector3(L, v.x, v.y, v.z); return 1;
        }
        if (strcmp(key, "AngularVelocity") == 0) {
            Vector3 v = part->get_angular_velocity();
            push_vector3(L, v.x, v.y, v.z); return 1;
        }

        if (strcmp(key, "Touched") == 0 || strcmp(key, "TouchEnded") == 0) {
            const char* sig_name = key; // capture the signal name
            lua_newtable(L);
            lua_pushlightuserdata(L, n);
            lua_pushstring(L, sig_name);
            lua_pushcclosure(L, [](lua_State* L) -> int {
                Node* target = (Node*)lua_touserdata(L, lua_upvalueindex(1));
                const char* sig  = lua_tostring(L, lua_upvalueindex(2));
                if (lua_isfunction(L, 2)) {
                    lua_pushvalue(L, 2);
                    int ref = lua_ref(L, -1);
                    int64_t ptr_L = (int64_t)gl_main_of(L);
                    if (target && target->has_signal(StringName(sig))) {
                        // El contact monitor es caro con miles de parts: se
                        // enciende recien aqui, cuando el juego usa Touched.
                        if (RobloxPart* rp = Object::cast_to<RobloxPart>(target))
                            rp->_ensure_touch_monitoring();
                        target->connect(StringName(sig),
                            Callable(target, "_on_luau_part_touched").bind(ref, ptr_L));
                    }
                }
                return 0;
            }, "Connect", 2);
            lua_setfield(L, -2, "Connect");
            return 1;
        }

        if (strcmp(key, "AssemblyLinearVelocity") == 0) {
            Vector3 v = part->get_linear_velocity();
            push_vector3(L, v.x, v.y, v.z); return 1;
        }
        if (strcmp(key, "AssemblyAngularVelocity") == 0) {
            Vector3 v = part->get_angular_velocity();
            push_vector3(L, v.x, v.y, v.z); return 1;
        }

        // Methods
        if (strcmp(key, "ApplyImpulse") == 0) {
            lua_pushlightuserdata(L, (void*)part);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RobloxPart* p = (RobloxPart*)lua_touserdata(pL, lua_upvalueindex(1));
                if (!p) return 0;
                float ix = 0; float iy = 0; float iz = 0;
                if (lua_istable(pL,2)) {
                    lua_getfield(pL,2,"X"); ix=(float)lua_tonumber(pL,-1); lua_pop(pL,1);
                    lua_getfield(pL,2,"Y"); iy=(float)lua_tonumber(pL,-1); lua_pop(pL,1);
                    lua_getfield(pL,2,"Z"); iz=(float)lua_tonumber(pL,-1); lua_pop(pL,1);
                }
                p->apply_central_impulse(Vector3(ix,iy,iz));
                return 0;
            }, "ApplyImpulse", 1);
            return 1;
        }
        if (strcmp(key, "ApplyAngularImpulse") == 0) {
            lua_pushlightuserdata(L, (void*)part);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RobloxPart* p = (RobloxPart*)lua_touserdata(pL, lua_upvalueindex(1));
                if (!p) return 0;
                float ix = 0; float iy = 0; float iz = 0;
                if (lua_istable(pL,2)) {
                    lua_getfield(pL,2,"X"); ix=(float)lua_tonumber(pL,-1); lua_pop(pL,1);
                    lua_getfield(pL,2,"Y"); iy=(float)lua_tonumber(pL,-1); lua_pop(pL,1);
                    lua_getfield(pL,2,"Z"); iz=(float)lua_tonumber(pL,-1); lua_pop(pL,1);
                }
                p->apply_torque_impulse(Vector3(ix,iy,iz));
                return 0;
            }, "ApplyAngularImpulse", 1);
            return 1;
        }
        // ── Network Ownership (1.14.7) ──
        if (strcmp(key, "SetNetworkOwner") == 0) {
            lua_pushlightuserdata(L, (void*)part);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RobloxPart* p = (RobloxPart*)lua_touserdata(pL, lua_upvalueindex(1));
                if (!p || !p->has_meta("_gl_netid")) return 0;
                Node* ns = _gl_find_network_service(p);
                if (!ns || _gl_net_mode(ns) != 1) return 0;   // solo el servidor asigna dueño
                Node* player = _gl_node_from_lua(pL, 2);       // nil = servidor
                int peer = player ? (int)(ns->call("peer_for_player_node", player)) : 0;
                ns->call("net_set_owner", (int64_t)p->get_meta("_gl_netid"), peer, false);
                return 0;
            }, "SetNetworkOwner", 1);
            return 1;
        }
        if (strcmp(key, "GetNetworkOwner") == 0) {
            lua_pushlightuserdata(L, (void*)part);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RobloxPart* p = (RobloxPart*)lua_touserdata(pL, lua_upvalueindex(1));
                if (!p) { lua_pushnil(pL); return 1; }
                int owner = p->has_meta("_gl_owner") ? (int)p->get_meta("_gl_owner") : 0;
                if (owner == 0) { lua_pushnil(pL); return 1; }   // servidor → nil
                Node* ns = _gl_find_network_service(p);
                Object* o = ns ? (Object*)ns->call("player_node_for_peer", owner) : nullptr;
                Node* plr = Object::cast_to<Node>(o);
                if (plr) _gl_push_node(pL, plr); else lua_pushnil(pL);
                return 1;
            }, "GetNetworkOwner", 1);
            return 1;
        }
        if (strcmp(key, "SetNetworkOwnershipAuto") == 0) {
            lua_pushlightuserdata(L, (void*)part);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RobloxPart* p = (RobloxPart*)lua_touserdata(pL, lua_upvalueindex(1));
                if (!p || !p->has_meta("_gl_netid")) return 0;
                Node* ns = _gl_find_network_service(p);
                if (!ns || _gl_net_mode(ns) != 1) return 0;
                ns->call("net_set_owner", (int64_t)p->get_meta("_gl_netid"), 0, true);   // auto = servidor (v1)
                return 0;
            }, "SetNetworkOwnershipAuto", 1);
            return 1;
        }
        if (strcmp(key, "GetNetworkOwnershipAuto") == 0) {
            lua_pushlightuserdata(L, (void*)part);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RobloxPart* p = (RobloxPart*)lua_touserdata(pL, lua_upvalueindex(1));
                // Auto es el modo por defecto en Roblox: true si no se fijó dueño a mano.
                bool a = p && (!p->has_meta("_gl_owner_auto") || (bool)p->get_meta("_gl_owner_auto"));
                lua_pushboolean(pL, a ? 1 : 0); return 1;
            }, "GetNetworkOwnershipAuto", 1);
            return 1;
        }
        if (strcmp(key, "IsNetworkOwner") == 0) {
            lua_pushlightuserdata(L, (void*)part);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RobloxPart* p = (RobloxPart*)lua_touserdata(pL, lua_upvalueindex(1));
                if (!p) { lua_pushboolean(pL, 0); return 1; }
                Node* ns = _gl_find_network_service(p);
                if (!ns || _gl_net_mode(ns) == 0) { lua_pushboolean(pL, 1); return 1; }   // single-player: soy dueño
                int64_t netid = p->has_meta("_gl_netid") ? (int64_t)p->get_meta("_gl_netid") : 0;
                bool own = (bool)(ns->call("net_is_owner", netid));
                lua_pushboolean(pL, own ? 1 : 0); return 1;
            }, "IsNetworkOwner", 1);
            return 1;
        }
        if (strcmp(key, "CanSetNetworkOwnership") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int { lua_pushboolean(pL, 1); return 1; }, "CanSetNetworkOwnership");
            return 1;
        }
        if (strcmp(key, "GetMass") == 0) {
            lua_pushlightuserdata(L, (void*)part);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RobloxPart* p = (RobloxPart*)lua_touserdata(pL, lua_upvalueindex(1));
                lua_pushnumber(pL, p ? p->get_mass_roblox() : 0.0f);
                return 1;
            }, "GetMass", 1);
            return 1;
        }
        if (strcmp(key, "SetNetworkOwner") == 0) {
            lua_pushcclosure(L, [](lua_State* pL) -> int { return 0; }, "SetNetworkOwner", 0);
            return 1;
        }
        if (strcmp(key, "GetNetworkOwner") == 0) {
            lua_pushcclosure(L, [](lua_State* pL) -> int { lua_pushnil(pL); return 1; }, "GetNetworkOwner", 0);
            return 1;
        }
        if (strcmp(key, "IntersectsWith") == 0 || strcmp(key, "GetTouchingParts") == 0) {
            lua_pushlightuserdata(L, (void*)part);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                lua_newtable(pL); return 1;
            }, key, 1);
            return 1;
        }
    }

    // ── RobloxWorkspace ────────────────────────────────────────────
    RobloxWorkspace* ws = Object::cast_to<RobloxWorkspace>(n);
    if (ws) {
        if (strcmp(key, "Gravity")                  == 0) { lua_pushnumber(L, ws->get_gravity());                      return 1; }
        if (strcmp(key, "FallenPartsDestroyHeight")  == 0) { lua_pushnumber(L, ws->get_fallen_parts_destroy_height());  return 1; }
        if (strcmp(key, "StreamingEnabled")          == 0) { lua_pushboolean(L, ws->get_streaming_enabled());           return 1; }
        if (strcmp(key, "AirDensity")                == 0) { lua_pushnumber(L, ws->get_air_density());                  return 1; }
        if (strcmp(key, "TouchesUseCollisionGroups") == 0) { lua_pushboolean(L, ws->get_touches_use_collision_groups());return 1; }
        // Reloj sincronizado (1.14.8)
        if (strcmp(key, "DistributedGameTime")       == 0) { lua_pushnumber(L, gl_distributed_game_time());            return 1; }
        if (strcmp(key, "GetServerTimeNow") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int { lua_pushnumber(pL, gl_server_time_now()); return 1; }, "GetServerTimeNow");
            return 1;
        }
    }

    // ── RemoteEventNode ───────────────────────────────────────────────────
    RemoteEventNode* re = Object::cast_to<RemoteEventNode>(n);
    if (re) {
        if (strcmp(key, "FireServer") == 0) {
            lua_pushlightuserdata(L, (void*)re);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RemoteEventNode* rev = (RemoteEventNode*)lua_touserdata(pL, lua_upvalueindex(1));
                if (!rev) return 0;
                int nargs = lua_gettop(pL) - 1;
                rev->fire_server(pL, 2, nargs < 0 ? 0 : nargs);
                return 0;
            }, "FireServer", 1);
            return 1;
        }
        if (strcmp(key, "FireClient") == 0) {
            lua_pushlightuserdata(L, (void*)re);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RemoteEventNode* rev = (RemoteEventNode*)lua_touserdata(pL, lua_upvalueindex(1));
                if (!rev) return 0;
                // Como Roblox: el 1er argumento DEBE ser un Player (1.14.9).
                Node* fc_plr = _gl_node_from_lua(pL, 2);
                if (!fc_plr || !fc_plr->is_class("PlayerObject")) { luaL_error(pL, "FireClient: first argument must be a Player"); return 0; }
                int nargs = lua_gettop(pL) - 2;
                rev->fire_client(pL, 2, 3, nargs < 0 ? 0 : nargs);
                return 0;
            }, "FireClient", 1);
            return 1;
        }
        if (strcmp(key, "FireAllClients") == 0) {
            lua_pushlightuserdata(L, (void*)re);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RemoteEventNode* rev = (RemoteEventNode*)lua_touserdata(pL, lua_upvalueindex(1));
                if (!rev) return 0;
                int nargs = lua_gettop(pL) - 1;
                rev->fire_all_clients(pL, 2, nargs < 0 ? 0 : nargs);
                return 0;
            }, "FireAllClients", 1);
            return 1;
        }
        if (strcmp(key, "OnServerEvent") == 0 || strcmp(key, "OnClientEvent") == 0) {
            bool is_server = (strcmp(key, "OnServerEvent") == 0);
            lua_newtable(L);
            // Connect(fn) / Once(fn) comparten closure (upvalue2 = ¿once?)
            for (int variant = 0; variant < 2; variant++) {
                lua_pushlightuserdata(L, (void*)re);
                lua_pushboolean(L, variant);            // 0 = Connect, 1 = Once
                lua_pushboolean(L, is_server ? 1 : 0);  // lado
                lua_pushcclosure(L, [](lua_State* pL) -> int {
                    RemoteEventNode* rev = (RemoteEventNode*)lua_touserdata(pL, lua_upvalueindex(1));
                    bool once = lua_toboolean(pL, lua_upvalueindex(2));
                    bool srv  = lua_toboolean(pL, lua_upvalueindex(3));
                    int fn_pos = -1;
                    for (int ai = 1; ai <= lua_gettop(pL); ai++)
                        if (lua_isfunction(pL, ai)) { fn_pos = ai; break; }
                    if (fn_pos == -1 || !rev) return 0;
                    lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                    lua_State* mL = (lua_State*)lua_touserdata(pL, -1);
                    lua_pop(pL, 1);
                    if (!mL) mL = pL;
                    int ref = lua_ref(pL, fn_pos);
                    if (srv) rev->add_server_cb(mL, ref, once);
                    else     rev->add_client_cb(mL, ref, once);
                    _gl_push_connection(pL, rev, ref); return 1;
                }, variant ? "Once" : "Connect", 3);
                lua_setfield(L, -2, variant ? "Once" : "Connect");
            }
            // Wait() — cede la corrutina hasta el próximo disparo; devuelve los args
            lua_pushlightuserdata(L, (void*)re);
            lua_pushboolean(L, is_server ? 1 : 0);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RemoteEventNode* rev = (RemoteEventNode*)lua_touserdata(pL, lua_upvalueindex(1));
                bool srv = lua_toboolean(pL, lua_upvalueindex(2));
                if (!rev) { lua_pushnil(pL); return 1; }
                lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                lua_State* mL = (lua_State*)lua_touserdata(pL, -1);
                lua_pop(pL, 1);
                if (!mL) mL = pL;
                if (srv) rev->add_server_wait(pL, mL);
                else     rev->add_client_wait(pL, mL);
                lua_pushstring(pL, "__WAIT_SIGNAL__");
                return lua_yield(pL, 1);
            }, "Wait", 2);
            lua_setfield(L, -2, "Wait");
            return 1;
        }
    }

    // ── RemoteFunctionNode ────────────────────────────────────────────────
    RemoteFunctionNode* rf = Object::cast_to<RemoteFunctionNode>(n);
    if (rf) {
        if (strcmp(key, "InvokeServer") == 0) {
            lua_pushlightuserdata(L, (void*)rf);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RemoteFunctionNode* rfn = (RemoteFunctionNode*)lua_touserdata(pL, lua_upvalueindex(1));
                if (!rfn) { lua_pushnil(pL); return 1; }
                int nargs = lua_gettop(pL) - 1; if (nargs < 0) nargs = 0;
                Node* ns = _gl_find_network_service(rfn);
                // Cliente en sesión de red: mandar al servidor, ceder el hilo y
                // reanudar con la respuesta (1.14.6). Host/single: correr local.
                if (_gl_net_mode(ns) == 2 && rfn->is_inside_tree() && gl_add_invoke_wait_hook()) {
                    lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_SCRIPT_NODE");
                    Node* sn = (Node*)lua_touserdata(pL, -1); lua_pop(pL, 1);
                    lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                    lua_State* main_L = (lua_State*)lua_touserdata(pL, -1); lua_pop(pL, 1);
                    if (!sn) { lua_pushnil(pL); return 1; }
                    if (!main_L) main_L = pL;
                    int64_t call_id = gl_net_new_call_id();
                    Array args = gl_net_encode_args(pL, 2, nargs);
                    ns->call("net_invoke_server", String(rfn->get_path()), (int)call_id, args);
                    lua_pushthread(pL); int ref = lua_ref(pL, -1); lua_pop(pL, 1);
                    gl_add_invoke_wait_hook()(sn, pL, main_L, ref, call_id, 0.0);   // 0 = esperar como Roblox (el fallo llega si el receptor se cae)
                    lua_pushstring(pL, "__WAIT_INVOKE__");
                    return lua_yield(pL, 1);
                }
                return rfn->invoke_server(pL, 2, nargs);
            }, "InvokeServer", 1);
            return 1;
        }
        if (strcmp(key, "InvokeClient") == 0) {
            lua_pushlightuserdata(L, (void*)rf);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RemoteFunctionNode* rfn = (RemoteFunctionNode*)lua_touserdata(pL, lua_upvalueindex(1));
                if (!rfn) { lua_pushnil(pL); return 1; }
                int nargs = lua_gettop(pL) - 2; if (nargs < 0) nargs = 0;
                Node* ns = _gl_find_network_service(rfn);
                // Servidor invocando a un cliente concreto por red (1.14.6). Si el
                // destino es el jugador del host, se corre local.
                if (_gl_net_mode(ns) == 1 && rfn->is_inside_tree() && gl_add_invoke_wait_hook()) {
                    Node* player = _gl_node_from_lua(pL, 2);
                    if (!player) { lua_pushnil(pL); return 1; }
                    int peer = (int)(ns->call("peer_for_player_node", player));
                    int myid = (int)(ns->call("get_peer_id"));
                    if (peer == myid && myid != 0) return rfn->invoke_client(pL, 3, nargs);
                    if (peer <= 0) { lua_pushnil(pL); return 1; }
                    lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_SCRIPT_NODE");
                    Node* sn = (Node*)lua_touserdata(pL, -1); lua_pop(pL, 1);
                    lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                    lua_State* main_L = (lua_State*)lua_touserdata(pL, -1); lua_pop(pL, 1);
                    if (!sn) { lua_pushnil(pL); return 1; }
                    if (!main_L) main_L = pL;
                    int64_t call_id = gl_net_new_call_id();
                    Array args = gl_net_encode_args(pL, 3, nargs);
                    ns->call("net_invoke_client", peer, String(rfn->get_path()), (int)call_id, args);
                    lua_pushthread(pL); int ref = lua_ref(pL, -1); lua_pop(pL, 1);
                    gl_add_invoke_wait_hook()(sn, pL, main_L, ref, call_id, 0.0);   // 0 = esperar como Roblox (el fallo llega si el receptor se cae)
                    lua_pushstring(pL, "__WAIT_INVOKE__");
                    return lua_yield(pL, 1);
                }
                return rfn->invoke_client(pL, 3, nargs);
            }, "InvokeClient", 1);
            return 1;
        }
    }

    // ── BindableEventNode ─────────────────────────────────────────────────
    BindableEventNode* be = Object::cast_to<BindableEventNode>(n);
    if (be) {
        if (strcmp(key, "Fire") == 0) {
            lua_pushlightuserdata(L, (void*)be);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                BindableEventNode* bev = (BindableEventNode*)lua_touserdata(pL, lua_upvalueindex(1));
                if (!bev) return 0;
                int nargs = lua_gettop(pL) - 1;
                bev->fire(pL, 2, nargs < 0 ? 0 : nargs);
                return 0;
            }, "Fire", 1);
            return 1;
        }
        if (strcmp(key, "Event") == 0) {
            lua_newtable(L);
            lua_pushlightuserdata(L, (void*)be);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                BindableEventNode* bev = (BindableEventNode*)lua_touserdata(pL, lua_upvalueindex(1));
                int fn_pos = -1;
                for (int ai = 1; ai <= lua_gettop(pL); ai++) {
                    if (lua_isfunction(pL, ai)) { fn_pos = ai; break; }
                }
                if (fn_pos == -1 || !bev) return 0;
                lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                lua_State* mL = (lua_State*)lua_touserdata(pL, -1);
                lua_pop(pL, 1);
                if (!mL) mL = pL;
                lua_pushvalue(pL, fn_pos);
                int ref = lua_ref(pL, -1);
                lua_pop(pL, 1);
                bev->add_cb(mL, ref);
                _gl_push_connection(pL, bev, ref); return 1;
            }, "Connect", 1);
            lua_setfield(L, -2, "Connect");
            return 1;
        }
    }

    // ── BodyMovers ─────────────────────────────────────────────────
    BodyVelocity* bv = Object::cast_to<BodyVelocity>(n);
    if (bv) {
        if (strcmp(key,"Velocity")  == 0) { Vector3 v = bv->get_velocity();  push_vector3(L, v.x, v.y, v.z); return 1; }
        if (strcmp(key,"MaxForce")  == 0) { Vector3 v = bv->get_max_force(); push_vector3(L, v.x, v.y, v.z); return 1; }
        if (strcmp(key,"P")         == 0) { lua_pushnumber(L, bv->get_p()); return 1; }
    }
    BodyPosition* bpos = Object::cast_to<BodyPosition>(n);
    if (bpos) {
        if (strcmp(key,"Position") == 0) { Vector3 v = bpos->get_position_val(); push_vector3(L, v.x, v.y, v.z); return 1; }
        if (strcmp(key,"MaxForce") == 0) { Vector3 v = bpos->get_max_force();    push_vector3(L, v.x, v.y, v.z); return 1; }
        if (strcmp(key,"P")        == 0) { lua_pushnumber(L, bpos->get_p()); return 1; }
        if (strcmp(key,"D")        == 0) { lua_pushnumber(L, bpos->get_d()); return 1; }
    }
    BodyForce* bf = Object::cast_to<BodyForce>(n);
    if (bf) {
        if (strcmp(key,"Force")      == 0) { Vector3 v = bf->get_force();  push_vector3(L, v.x, v.y, v.z); return 1; }
        if (strcmp(key,"RelativeTo") == 0) { lua_pushboolean(L, bf->get_relative_to()); return 1; }
    }
    BodyAngularVelocity* bav = Object::cast_to<BodyAngularVelocity>(n);
    if (bav) {
        if (strcmp(key,"AngularVelocity") == 0) { Vector3 v = bav->get_angular_velocity(); push_vector3(L, v.x, v.y, v.z); return 1; }
        if (strcmp(key,"MaxTorque")       == 0) { Vector3 v = bav->get_max_torque();        push_vector3(L, v.x, v.y, v.z); return 1; }
        if (strcmp(key,"P")               == 0) { lua_pushnumber(L, bav->get_p()); return 1; }
    }
    BodyGyro* bgyro = Object::cast_to<BodyGyro>(n);
    if (bgyro) {
        if (strcmp(key,"MaxTorque") == 0) { Vector3 v = bgyro->get_max_torque(); push_vector3(L, v.x, v.y, v.z); return 1; }
        if (strcmp(key,"P")        == 0) { lua_pushnumber(L, bgyro->get_p()); return 1; }
        if (strcmp(key,"D")        == 0) { lua_pushnumber(L, bgyro->get_d()); return 1; }
    }

    // ── Constraints ────────────────────────────────────────────────
    WeldConstraint* weld = Object::cast_to<WeldConstraint>(n);
    if (weld) {
        if (strcmp(key,"Enabled") == 0) { lua_pushboolean(L, weld->get_enabled()); return 1; }
        if (strcmp(key,"Part0")   == 0) { wrap_node(L, weld->get_part0());          return 1; }
        if (strcmp(key,"Part1")   == 0) { wrap_node(L, weld->get_part1());          return 1; }
    }
    HingeConstraint* hinge = Object::cast_to<HingeConstraint>(n);
    if (hinge) {
        if (strcmp(key,"Enabled")             == 0) { lua_pushboolean(L, hinge->get_enabled());              return 1; }
        if (strcmp(key,"LimitsEnabled")       == 0) { lua_pushboolean(L, hinge->get_limits_enabled());       return 1; }
        if (strcmp(key,"UpperAngle")          == 0) { lua_pushnumber(L,  hinge->get_upper_angle());          return 1; }
        if (strcmp(key,"LowerAngle")          == 0) { lua_pushnumber(L,  hinge->get_lower_angle());          return 1; }
        if (strcmp(key,"MotorEnabled")        == 0) { lua_pushboolean(L, hinge->get_motor_enabled());        return 1; }
        if (strcmp(key,"MotorMaxTorque")      == 0) { lua_pushnumber(L,  hinge->get_motor_max_torque());     return 1; }
        if (strcmp(key,"MotorTargetVelocity") == 0) { lua_pushnumber(L,  hinge->get_motor_target_velocity());return 1; }
        if (strcmp(key,"CurrentAngle")        == 0) { lua_pushnumber(L,  hinge->get_current_angle());        return 1; }
    }
    RodConstraint* rod = Object::cast_to<RodConstraint>(n);
    if (rod) {
        if (strcmp(key,"Enabled") == 0) { lua_pushboolean(L, rod->get_enabled()); return 1; }
        if (strcmp(key,"Length")  == 0) { lua_pushnumber(L,  rod->get_length());  return 1; }
    }
    SpringConstraint* spring = Object::cast_to<SpringConstraint>(n);
    if (spring) {
        if (strcmp(key,"Enabled")    == 0) { lua_pushboolean(L, spring->get_enabled());     return 1; }
        if (strcmp(key,"FreeLength") == 0) { lua_pushnumber(L,  spring->get_free_length()); return 1; }
        if (strcmp(key,"Stiffness")  == 0) { lua_pushnumber(L,  spring->get_stiffness());   return 1; }
        if (strcmp(key,"Damping")    == 0) { lua_pushnumber(L,  spring->get_damping());     return 1; }
    }

    // ── GUI ────────────────────────────────────────────────────────
    ScreenGui* sg = Object::cast_to<ScreenGui>(n);
    if (sg) {
        if (strcmp(key,"Enabled")       == 0) { lua_pushboolean(L, sg->get_sg_enabled());   return 1; }
        if (strcmp(key,"DisplayOrder")  == 0) { lua_pushnumber(L,  sg->get_display_order()); return 1; }
        if (strcmp(key,"ResetOnSpawn")  == 0) { lua_pushboolean(L, sg->get_reset_on_spawn());return 1; }
    }

    // Helper lambda para leer UDim2 como tabla Lua
    auto push_udim2_table = [&](const GuiUDim2& d) {
        lua_newtable(L);
        lua_newtable(L);
        lua_pushnumber(L, d.xs); lua_setfield(L, -2, "Scale");
        lua_pushnumber(L, d.xo); lua_setfield(L, -2, "Offset");
        lua_pushvalue(L, -1);    lua_setfield(L, -3, "X");  lua_pop(L, 1);
        lua_newtable(L);
        lua_pushnumber(L, d.ys); lua_setfield(L, -2, "Scale");
        lua_pushnumber(L, d.yo); lua_setfield(L, -2, "Offset");
        lua_pushvalue(L, -1);    lua_setfield(L, -3, "Y");  lua_pop(L, 1);
    };

    // Propiedades comunes de GUI (Frame, Label, Button, TextBox, ImageLabel, ScrollingFrame)
    RobloxFrame* rframe = Object::cast_to<RobloxFrame>(n);
    if (rframe) {
        if (strcmp(key,"Size")     == 0) { push_udim2_table(rframe->get_udim2_size()); return 1; }
        if (strcmp(key,"Position") == 0) { push_udim2_table(rframe->get_udim2_pos());  return 1; }
        if (strcmp(key,"BackgroundColor3")      == 0) { push_color3(L, rframe->get_bg_r(), rframe->get_bg_g(), rframe->get_bg_b()); return 1; }
        if (strcmp(key,"BackgroundTransparency")== 0) { lua_pushnumber(L, rframe->get_bg_alpha()); return 1; }
        if (strcmp(key,"Visible")  == 0) { lua_pushboolean(L, rframe->is_visible()); return 1; }
        if (strcmp(key,"ZIndex")   == 0) { lua_pushnumber(L, rframe->get_z_index()); return 1; }
        if (strcmp(key,"MouseEnter") == 0 || strcmp(key,"MouseLeave") == 0) {
            lua_newtable(L);
            lua_pushstring(L, key);
            lua_pushlightuserdata(L, (void*)rframe);
            lua_pushcclosure(L, [](lua_State* pL)->int{
                const char* sig = lua_tostring(pL, lua_upvalueindex(1));
                RobloxFrame* f  = (RobloxFrame*)lua_touserdata(pL, lua_upvalueindex(2));
                if (f && lua_isfunction(pL, 2)) {
                    lua_pushvalue(pL, 2);
                    int ref = lua_ref(pL, -1); lua_pop(pL, 1);
                    // connect Godot signal / conectar señal de Godot
                    f->connect(StringName(sig), Callable(f, sig == String("MouseEnter") ? "_emit_mouse_enter" : "_emit_mouse_leave"));
                }
                return 0;
            }, "Connect", 2);
            lua_setfield(L, -2, "Connect");
            return 1;
        }
    }
    RobloxTextLabel* rtlabel = Object::cast_to<RobloxTextLabel>(n);
    if (rtlabel) {
        if (strcmp(key,"Size")     == 0) { push_udim2_table(rtlabel->get_udim2_size()); return 1; }
        if (strcmp(key,"Position") == 0) { push_udim2_table(rtlabel->get_udim2_pos());  return 1; }
        if (strcmp(key,"BackgroundColor3")      == 0) { push_color3(L, rtlabel->get_bg_r(), rtlabel->get_bg_g(), rtlabel->get_bg_b()); return 1; }
        if (strcmp(key,"BackgroundTransparency")== 0) { lua_pushnumber(L, rtlabel->get_bg_alpha()); return 1; }
        if (strcmp(key,"Text")     == 0) { lua_pushstring(L, rtlabel->get_text().utf8().get_data()); return 1; }
        if (strcmp(key,"Visible")  == 0) { lua_pushboolean(L, rtlabel->is_visible()); return 1; }
        if (strcmp(key,"ZIndex")   == 0) { lua_pushnumber(L, rtlabel->get_z_index()); return 1; }
    }
    RobloxTextButton* rtbutton = Object::cast_to<RobloxTextButton>(n);
    if (rtbutton) {
        if (strcmp(key,"Size")     == 0) { push_udim2_table(rtbutton->get_udim2_size()); return 1; }
        if (strcmp(key,"Position") == 0) { push_udim2_table(rtbutton->get_udim2_pos());  return 1; }
        if (strcmp(key,"BackgroundColor3")      == 0) { push_color3(L, rtbutton->get_bg_r(), rtbutton->get_bg_g(), rtbutton->get_bg_b()); return 1; }
        if (strcmp(key,"BackgroundTransparency")== 0) { lua_pushnumber(L, rtbutton->get_bg_alpha()); return 1; }
        if (strcmp(key,"Text")     == 0) { lua_pushstring(L, rtbutton->get_text().utf8().get_data()); return 1; }
        if (strcmp(key,"Visible")  == 0) { lua_pushboolean(L, rtbutton->is_visible()); return 1; }
        if (strcmp(key,"ZIndex")   == 0) { lua_pushnumber(L, rtbutton->get_z_index()); return 1; }
        if (strcmp(key,"MouseButton1Click") == 0 || strcmp(key,"Activated") == 0) {
            lua_newtable(L);
            lua_pushlightuserdata(L, (void*)rtbutton);
            lua_pushcclosure(L, [](lua_State* pL)->int{
                RobloxTextButton* btn = (RobloxTextButton*)lua_touserdata(pL, lua_upvalueindex(1));
                int fn_pos = -1;
                for (int ai=1; ai<=lua_gettop(pL); ai++) if (lua_isfunction(pL,ai)) { fn_pos=ai; break; }
                if (fn_pos==-1 || !btn) return 0;
                lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                lua_State* mL = (lua_State*)lua_touserdata(pL, -1); lua_pop(pL,1);
                if (!mL) mL = pL;
                lua_pushvalue(pL, fn_pos);
                int ref = lua_ref(pL, -1); lua_pop(pL,1);
                btn->add_click_cb(mL, ref);
                _gl_push_connection(pL, btn, ref); return 1;
            }, "Connect", 1);
            lua_setfield(L, -2, "Connect");
            return 1;
        }
        if (strcmp(key,"MouseEnter") == 0) {
            lua_newtable(L);
            lua_pushlightuserdata(L, (void*)rtbutton);
            lua_pushcclosure(L, [](lua_State* pL)->int{
                RobloxTextButton* btn = (RobloxTextButton*)lua_touserdata(pL, lua_upvalueindex(1));
                int fn_pos = -1;
                for (int ai=1; ai<=lua_gettop(pL); ai++) if (lua_isfunction(pL,ai)) { fn_pos=ai; break; }
                if (fn_pos==-1 || !btn) return 0;
                lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                lua_State* mL = (lua_State*)lua_touserdata(pL, -1); lua_pop(pL,1);
                if (!mL) mL = pL;
                lua_pushvalue(pL, fn_pos);
                int ref = lua_ref(pL, -1); lua_pop(pL,1);
                btn->add_enter_cb(mL, ref);
                _gl_push_connection(pL, btn, ref); return 1;
            }, "Connect", 1);
            lua_setfield(L, -2, "Connect");
            return 1;
        }
    }
    RobloxTextBox* rtbox = Object::cast_to<RobloxTextBox>(n);
    if (rtbox) {
        if (strcmp(key,"Size")     == 0) { push_udim2_table(rtbox->get_udim2_size()); return 1; }
        if (strcmp(key,"Position") == 0) { push_udim2_table(rtbox->get_udim2_pos());  return 1; }
        if (strcmp(key,"Text")     == 0) { lua_pushstring(L, rtbox->get_text().utf8().get_data()); return 1; }
        if (strcmp(key,"PlaceholderText") == 0) { lua_pushstring(L, rtbox->get_placeholder().utf8().get_data()); return 1; }
        if (strcmp(key,"Visible")  == 0) { lua_pushboolean(L, rtbox->is_visible()); return 1; }
        if (strcmp(key,"FocusLost") == 0) {
            lua_newtable(L);
            lua_pushlightuserdata(L, (void*)rtbox);
            lua_pushcclosure(L, [](lua_State* pL)->int{
                RobloxTextBox* tb = (RobloxTextBox*)lua_touserdata(pL, lua_upvalueindex(1));
                int fn_pos = -1;
                for (int ai=1; ai<=lua_gettop(pL); ai++) if (lua_isfunction(pL,ai)) { fn_pos=ai; break; }
                if (fn_pos==-1 || !tb) return 0;
                lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                lua_State* mL = (lua_State*)lua_touserdata(pL, -1); lua_pop(pL,1);
                if (!mL) mL = pL;
                lua_pushvalue(pL, fn_pos);
                int ref = lua_ref(pL, -1); lua_pop(pL,1);
                tb->add_focus_lost_cb(mL, ref);
                _gl_push_connection(pL, tb, ref); return 1;
            }, "Connect", 1);
            lua_setfield(L, -2, "Connect");
            return 1;
        }
    }
    RobloxImageLabel* rimage = Object::cast_to<RobloxImageLabel>(n);
    if (rimage) {
        if (strcmp(key,"Size")     == 0) { push_udim2_table(rimage->get_udim2_size()); return 1; }
        if (strcmp(key,"Position") == 0) { push_udim2_table(rimage->get_udim2_pos());  return 1; }
        if (strcmp(key,"BackgroundColor3")      == 0) { push_color3(L, rimage->get_bg_r(), rimage->get_bg_g(), rimage->get_bg_b()); return 1; }
        if (strcmp(key,"BackgroundTransparency")== 0) { lua_pushnumber(L, rimage->get_bg_alpha()); return 1; }
        if (strcmp(key,"Visible")  == 0) { lua_pushboolean(L, rimage->is_visible()); return 1; }
    }

    // ── CurrentCamera — workspace.CurrentCamera returns the active camera ─
    //// ── CurrentCamera — workspace.CurrentCamera devuelve la cámara activa ─
    // Looks for Camera3D or Camera2D in the workspace or in the local player
    //// Busca Camera3D o Camera2D en el workspace o en el jugador local
    if (strcmp(key, "CurrentCamera") == 0) {
        // Search in the current node (Workspace may have CurrentCamera as a child) / Buscar en el nodo actual (Workspace puede tener CurrentCamera como hijo)
        Node* cam = n->get_node_or_null("CurrentCamera");
        if (cam) { wrap_node(L, cam); return 1; }
        // Fallback: look in LocalPlayer / buscar en el LocalPlayer
        Node* lp = n->get_node_or_null("LocalPlayer");
        if (lp) {
            cam = lp->get_node_or_null("Camera3D");
            if (!cam) cam = lp->get_node_or_null("Camera2D");
            if (cam) { wrap_node(L, cam); return 1; }
        }
        lua_pushnil(L); return 1;
    }

    // ── AnimationTrack ────────────────────────────────────────────────
    AnimationTrack* atrack = Object::cast_to<AnimationTrack>(n);
    if (atrack) {
        if (strcmp(key,"IsPlaying")    == 0) { lua_pushboolean(L, atrack->is_playing()); return 1; }
        if (strcmp(key,"Looped")       == 0) { lua_pushboolean(L, atrack->get_looped()); return 1; }
        if (strcmp(key,"Speed")        == 0) { lua_pushnumber(L, atrack->get_speed());   return 1; }
        if (strcmp(key,"WeightCurrent")== 0) { lua_pushnumber(L, atrack->get_weight());  return 1; }
        if (strcmp(key,"Length")       == 0) { lua_pushnumber(L, atrack->get_length());  return 1; }
        if (strcmp(key,"TimePosition") == 0) { lua_pushnumber(L, atrack->get_time_position()); return 1; }
        if (strcmp(key,"Play")  == 0) {
            lua_pushlightuserdata(L,(void*)atrack);
            lua_pushcclosure(L,[](lua_State* LL)->int{ AnimationTrack* t=(AnimationTrack*)lua_touserdata(LL,lua_upvalueindex(1)); if(t)t->play(); return 0; },"Play",1); return 1;
        }
        if (strcmp(key,"Stop")  == 0) {
            lua_pushlightuserdata(L,(void*)atrack);
            lua_pushcclosure(L,[](lua_State* LL)->int{ AnimationTrack* t=(AnimationTrack*)lua_touserdata(LL,lua_upvalueindex(1)); if(t)t->stop(); return 0; },"Stop",1); return 1;
        }
        if (strcmp(key,"AdjustSpeed")  == 0) {
            lua_pushlightuserdata(L,(void*)atrack);
            lua_pushcclosure(L,[](lua_State* LL)->int{
                AnimationTrack* t=(AnimationTrack*)lua_touserdata(LL,lua_upvalueindex(1));
                if(t&&lua_isnumber(LL,2)) t->adjust_speed((float)lua_tonumber(LL,2)); return 0;
            },"AdjustSpeed",1); return 1;
        }
        auto make_atrack_sig = [&](std::vector<AnimationTrack::LuaCallback>& cbs) -> int {
            lua_newtable(L);
            lua_pushlightuserdata(L,(void*)atrack);
            lua_pushlightuserdata(L,(void*)&cbs);
            lua_pushcclosure(L,[](lua_State* LL)->int{
                AnimationTrack* d=(AnimationTrack*)lua_touserdata(LL,lua_upvalueindex(1));
                auto* c=(std::vector<AnimationTrack::LuaCallback>*)lua_touserdata(LL,lua_upvalueindex(2));
                if(!c||!lua_isfunction(LL,2)){lua_pushnil(LL);return 1;}
                lua_pushvalue(LL,2); int ref=lua_ref(LL,-1); lua_pop(LL,1);
                c->push_back({gl_main_of(LL),ref,true}); _gl_push_connection(LL, d, ref); return 1;
            },"Connect",2);
            lua_setfield(L,-2,"Connect"); return 1;
        };
        if (strcmp(key,"Ended")   == 0) return make_atrack_sig(atrack->ended_cbs);
        if (strcmp(key,"Stopped") == 0) return make_atrack_sig(atrack->stopped_cbs);
    }

    // ── AnimationObject ───────────────────────────────────────────────
    AnimationObject* aobj = Object::cast_to<AnimationObject>(n);
    if (aobj) {
        if (strcmp(key,"AnimationId") == 0) { lua_pushstring(L, aobj->get_animation_id().utf8().get_data()); return 1; }
    }

    // ── RobloxSound ──────────────────────────────────────────────────
    RobloxSound* rsound = Object::cast_to<RobloxSound>(n);
    if (rsound) {
        if (strcmp(key,"SoundId")    == 0) { lua_pushstring(L, rsound->get_sound_id().utf8().get_data()); return 1; }
        if (strcmp(key,"Volume")     == 0) { lua_pushnumber(L, rsound->get_volume());     return 1; }
        if (strcmp(key,"Pitch")      == 0) { lua_pushnumber(L, rsound->get_pitch());      return 1; }
        if (strcmp(key,"Looped")     == 0) { lua_pushboolean(L, rsound->get_looped());    return 1; }
        if (strcmp(key,"IsPlaying")  == 0) { lua_pushboolean(L, rsound->is_playing());    return 1; }
        if (strcmp(key,"TimePosition")== 0){ lua_pushnumber(L, rsound->get_time_position()); return 1; }
        if (strcmp(key,"PlaybackSpeed")==0){ lua_pushnumber(L, rsound->get_pitch());          return 1; }
        if (strcmp(key,"IsPaused")   == 0) { lua_pushboolean(L, rsound->is_paused());          return 1; }
        if (strcmp(key,"IsLoaded")   == 0) { lua_pushboolean(L, rsound->is_loaded());          return 1; }
        if (strcmp(key,"RollOffMode")       ==0){ lua_pushnumber(L, rsound->get_rolloff_mode());       return 1; }
        if (strcmp(key,"RollOffMinDistance")==0){ lua_pushnumber(L, rsound->get_rolloff_min());        return 1; }
        if (strcmp(key,"RollOffMaxDistance")==0){ lua_pushnumber(L, rsound->get_rolloff_max());        return 1; }
        if (strcmp(key,"EmitterSize")       ==0){ lua_pushnumber(L, rsound->get_emitter_size());       return 1; }
        if (strcmp(key,"PlaybackLoudness")  ==0){ lua_pushnumber(L, rsound->get_playback_loudness());  return 1; }
        if (strcmp(key,"Resume") == 0) {
            lua_pushlightuserdata(L, (void*)rsound);
            lua_pushcclosure(L, [](lua_State* LL) -> int {
                RobloxSound* s = (RobloxSound*)lua_touserdata(LL, lua_upvalueindex(1));
                if (s) s->resume(); return 0;
            }, "Resume", 1); return 1;
        }
        if (strcmp(key,"Play")  == 0) {
            lua_pushlightuserdata(L, (void*)rsound);
            lua_pushcclosure(L, [](lua_State* LL) -> int {
                RobloxSound* s = (RobloxSound*)lua_touserdata(LL, lua_upvalueindex(1));
                if (s) s->play(); return 0;
            }, "Play", 1); return 1;
        }
        if (strcmp(key,"Stop")  == 0) {
            lua_pushlightuserdata(L, (void*)rsound);
            lua_pushcclosure(L, [](lua_State* LL) -> int {
                RobloxSound* s = (RobloxSound*)lua_touserdata(LL, lua_upvalueindex(1));
                if (s) s->stop(); return 0;
            }, "Stop", 1); return 1;
        }
        if (strcmp(key,"Pause") == 0) {
            lua_pushlightuserdata(L, (void*)rsound);
            lua_pushcclosure(L, [](lua_State* LL) -> int {
                RobloxSound* s = (RobloxSound*)lua_touserdata(LL, lua_upvalueindex(1));
                if (s) s->pause(); return 0;
            }, "Pause", 1); return 1;
        }
        if (strcmp(key,"Ended") == 0) {
            lua_newtable(L);
            lua_pushlightuserdata(L, (void*)rsound);
            lua_pushcclosure(L, [](lua_State* LL) -> int {
                RobloxSound* s = (RobloxSound*)lua_touserdata(LL, lua_upvalueindex(1));
                if (!s || !lua_isfunction(LL, 2)) { lua_pushnil(LL); return 1; }
                lua_pushvalue(LL, 2);
                int ref = lua_ref(LL, -1); lua_pop(LL, 1);
                // Ended fires when IsPlaying goes false; handled via polling in Luau or signal
                lua_pushnil(LL); return 1;
            }, "Connect", 1);
            lua_setfield(L, -2, "Connect");
            return 1;
        }
    }

    // ── TweenService ─────────────────────────────────────────────────
    TweenService* tws = Object::cast_to<TweenService>(n);
    if (tws) {
        if (strcmp(key,"Create") == 0) {
            lua_pushlightuserdata(L, (void*)tws);
            lua_pushcclosure(L, [](lua_State* LL) -> int {
                TweenService* ts = (TweenService*)lua_touserdata(LL, lua_upvalueindex(1));
                if (!ts) { lua_pushnil(LL); return 1; }
                // args: (instance, tweenInfo_table, goals_table)
                GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(LL, 1);
                Node* target_node = (w && gow_node(w)) ? gow_node(w) : nullptr;

                // Parse TweenInfo table (arg 2)
                TweenInfo tinfo;
                if (lua_istable(LL, 2)) {
                    lua_getfield(LL, 2, "Time");       if (!lua_isnil(LL,-1)) tinfo.time          = (float)lua_tonumber(LL,-1); lua_pop(LL,1);
                    lua_getfield(LL, 2, "EasingStyle"); if (!lua_isnil(LL,-1)) tinfo.easing_style  = (int)lua_tonumber(LL,-1);  lua_pop(LL,1);
                    lua_getfield(LL, 2, "EasingDirection"); if(!lua_isnil(LL,-1)) tinfo.easing_dir = (int)lua_tonumber(LL,-1);  lua_pop(LL,1);
                    lua_getfield(LL, 2, "RepeatCount");if (!lua_isnil(LL,-1)) tinfo.repeat_count   = (int)lua_tonumber(LL,-1);  lua_pop(LL,1);
                    lua_getfield(LL, 2, "Reverses");   if (!lua_isnil(LL,-1)) tinfo.reverses       = lua_toboolean(LL,-1)!=0;   lua_pop(LL,1);
                    lua_getfield(LL, 2, "DelayTime");  if (!lua_isnil(LL,-1)) tinfo.delay_time     = (float)lua_tonumber(LL,-1);lua_pop(LL,1);
                }

                RobloxTween* tw = ts->create_tween(target_node ? target_node->get_parent() : nullptr);
                tw->info = tinfo;

                // Parse goals table (arg 3) — {PropertyName = targetValue}
                if (lua_istable(LL, 3) && target_node) {
                    lua_pushnil(LL);
                    while (lua_next(LL, 3) != 0) {
                        if (lua_isstring(LL, -2)) {
                            const char* prop = lua_tostring(LL, -2);
                            TweenTrack tr;
                            tr.target = target_node;
                            tr.property = String(prop);
                            tr.from_current = true;
                            // Read target value
                            int vt = lua_type(LL, -1);
                            if (vt == LUA_TNUMBER) {
                                tr.to_val = TweenValue((float)lua_tonumber(LL, -1));
                            } else if (vt == LUA_TTABLE) {
                                // Could be Color3 or Vector3
                                lua_getfield(LL, -1, "r");
                                if (!lua_isnil(LL,-1)) {
                                    float r=(float)lua_tonumber(LL,-1); lua_pop(LL,1);
                                    lua_getfield(LL,-1,"g"); float g=(float)lua_tonumber(LL,-1); lua_pop(LL,1);
                                    lua_getfield(LL,-1,"b"); float b=(float)lua_tonumber(LL,-1); lua_pop(LL,1);
                                    tr.to_val = TweenValue(Color(r,g,b));
                                } else { lua_pop(LL,1);
                                    lua_getfield(LL,-1,"X");
                                    float x=(float)lua_tonumber(LL,-1); lua_pop(LL,1);
                                    lua_getfield(LL,-1,"Y"); float y=(float)lua_tonumber(LL,-1); lua_pop(LL,1);
                                    lua_getfield(LL,-1,"Z");
                                    if (!lua_isnil(LL,-1)) { float z=(float)lua_tonumber(LL,-1); lua_pop(LL,1);
                                        tr.to_val = TweenValue(Vector3(x,y,z));
                                    } else { lua_pop(LL,1);
                                        tr.to_val = TweenValue(Vector2(x,y));
                                    }
                                }
                            }
                            tw->tracks.push_back(tr);
                        }
                        lua_pop(LL, 1);
                    }
                }

                // Return Tween object
                GodotObjectWrapper* tw_wrap = (GodotObjectWrapper*)lua_newuserdata(LL, sizeof(GodotObjectWrapper));
                gow_set(tw_wrap, tw);
                luaL_getmetatable(LL, "GodotObject");
                lua_setmetatable(LL, -2);
                return 1;
            }, "Create", 1); return 1;
        }
    }

    // ── RobloxTween ───────────────────────────────────────────────────
    RobloxTween* rtween = Object::cast_to<RobloxTween>(n);
    if (rtween) {
        if (strcmp(key,"Play")   == 0) {
            lua_pushlightuserdata(L, (void*)rtween);
            lua_pushcclosure(L, [](lua_State* LL)->int{ RobloxTween* t=(RobloxTween*)lua_touserdata(LL,lua_upvalueindex(1)); if(t)t->play(); return 0; },"Play",1); return 1;
        }
        if (strcmp(key,"Pause")  == 0) {
            lua_pushlightuserdata(L, (void*)rtween);
            lua_pushcclosure(L, [](lua_State* LL)->int{ RobloxTween* t=(RobloxTween*)lua_touserdata(LL,lua_upvalueindex(1)); if(t)t->pause(); return 0; },"Pause",1); return 1;
        }
        if (strcmp(key,"Cancel") == 0) {
            lua_pushlightuserdata(L, (void*)rtween);
            lua_pushcclosure(L, [](lua_State* LL)->int{ RobloxTween* t=(RobloxTween*)lua_touserdata(LL,lua_upvalueindex(1)); if(t)t->cancel(); return 0; },"Cancel",1); return 1;
        }
        if (strcmp(key,"Completed") == 0) {
            lua_newtable(L);
            lua_pushlightuserdata(L, (void*)rtween);
            lua_pushcclosure(L, [](lua_State* LL)->int{
                RobloxTween* t=(RobloxTween*)lua_touserdata(LL,lua_upvalueindex(1));
                if (!t || !lua_isfunction(LL,2)) { lua_pushnil(LL); return 1; }
                lua_pushvalue(LL,2); int ref=lua_ref(LL,-1); lua_pop(LL,1);
                t->add_completed_cb(gl_main_of(LL), ref);
                _gl_push_connection(LL, t, ref); return 1;
            }, "Connect", 1);
            lua_setfield(L, -2, "Connect"); return 1;
        }
        if (strcmp(key,"PlaybackState") == 0) {
            if (rtween->finished)     lua_pushstring(L, "Completed");
            else if (!rtween->playing) lua_pushstring(L, "Paused");
            else                       lua_pushstring(L, "Playing");
            return 1;
        }
    }

    // ── ClickDetector ─────────────────────────────────────────────────
    ClickDetector* cd = Object::cast_to<ClickDetector>(n);
    if (cd) {
        if (strcmp(key,"MaxActivationDistance") == 0) { lua_pushnumber(L, cd->get_max_distance()); return 1; }
        auto make_click_signal = [&](const char* ev_name, std::vector<ClickDetector::LuaCallback>& cbs) -> int {
            lua_newtable(L);
            lua_pushlightuserdata(L, (void*)cd);
            lua_pushlightuserdata(L, (void*)&cbs);
            lua_pushcclosure(L, [](lua_State* LL)->int{
                ClickDetector* d=(ClickDetector*)lua_touserdata(LL,lua_upvalueindex(1));
                auto* c=(std::vector<ClickDetector::LuaCallback>*)lua_touserdata(LL,lua_upvalueindex(2));
                if (!d || !c || !lua_isfunction(LL,2)) { lua_pushnil(LL); return 1; }
                lua_pushvalue(LL,2); int ref=lua_ref(LL,-1); lua_pop(LL,1);
                c->push_back({gl_main_of(LL), ref, true});
                _gl_push_connection(LL, d, ref); return 1;
            }, "Connect", 2);
            lua_setfield(L, -2, "Connect"); return 1;
        };
        if (strcmp(key,"MouseClick")      == 0) return make_click_signal("MouseClick",      cd->mouse_click_cbs);
        if (strcmp(key,"MouseHoverEnter") == 0) return make_click_signal("MouseHoverEnter",  cd->hover_enter_cbs);
        if (strcmp(key,"MouseHoverLeave") == 0) return make_click_signal("MouseHoverLeave",  cd->hover_leave_cbs);
    }

    // ── ProximityPrompt ───────────────────────────────────────────────
    ProximityPrompt* pp = Object::cast_to<ProximityPrompt>(n);
    if (pp) {
        if (strcmp(key,"ActionText")            == 0) { lua_pushstring(L, pp->get_action_text().utf8().get_data()); return 1; }
        if (strcmp(key,"KeyboardKeyCode")       == 0) { lua_pushstring(L, pp->get_key_code().utf8().get_data()); return 1; }
        if (strcmp(key,"MaxActivationDistance") == 0) { lua_pushnumber(L, pp->get_max_distance()); return 1; }
        if (strcmp(key,"HoldDuration")          == 0) { lua_pushnumber(L, pp->get_hold_duration()); return 1; }
        if (strcmp(key,"Enabled")               == 0) { lua_pushboolean(L, pp->get_enabled()); return 1; }
        auto make_pp_signal = [&](std::vector<ProximityPrompt::LuaCallback>& cbs) -> int {
            lua_newtable(L);
            lua_pushlightuserdata(L, (void*)pp);
            lua_pushlightuserdata(L, (void*)&cbs);
            lua_pushcclosure(L, [](lua_State* LL)->int{
                ProximityPrompt* d=(ProximityPrompt*)lua_touserdata(LL,lua_upvalueindex(1));
                auto* c=(std::vector<ProximityPrompt::LuaCallback>*)lua_touserdata(LL,lua_upvalueindex(2));
                if (!c || !lua_isfunction(LL,2)) { lua_pushnil(LL); return 1; }
                lua_pushvalue(LL,2); int ref=lua_ref(LL,-1); lua_pop(LL,1);
                c->push_back({gl_main_of(LL), ref, true});
                _gl_push_connection(LL, d, ref); return 1;
            }, "Connect", 2);
            lua_setfield(L, -2, "Connect"); return 1;
        };
        if (strcmp(key,"Triggered")  == 0) return make_pp_signal(pp->triggered_cbs);
        if (strcmp(key,"HoldBegan")  == 0) return make_pp_signal(pp->hold_began_cbs);
        if (strcmp(key,"HoldEnded")  == 0) return make_pp_signal(pp->hold_ended_cbs);
    }

    // ── SpawnLocation ─────────────────────────────────────────────────
    SpawnLocation* spwn = Object::cast_to<SpawnLocation>(n);
    if (spwn) {
        if (strcmp(key,"Enabled")  == 0) { lua_pushboolean(L, spwn->get_enabled()); return 1; }
        if (strcmp(key,"Duration") == 0) { lua_pushnumber(L, spwn->get_duration()); return 1; }
        if (strcmp(key,"Neutral")  == 0) { lua_pushboolean(L, spwn->get_neutral()); return 1; }
    }

    // ── BillboardGui ──────────────────────────────────────────────────
    BillboardGui* bbgui = Object::cast_to<BillboardGui>(n);
    if (bbgui) {
        if (strcmp(key,"Enabled")     == 0) { lua_pushboolean(L, bbgui->get_enabled()); return 1; }
        if (strcmp(key,"Size")        == 0) {
            lua_newtable(L);
            lua_pushnumber(L, bbgui->get_size_x()); lua_setfield(L,-2,"X");
            lua_pushnumber(L, bbgui->get_size_y()); lua_setfield(L,-2,"Y");
            return 1;
        }
        if (strcmp(key,"MaxDistance") == 0) { lua_pushnumber(L, bbgui->get_max_distance()); return 1; }
        if (strcmp(key,"AlwaysOnTop") == 0) { lua_pushboolean(L, bbgui->get_always_on_top()); return 1; }
    }

    // ── Motor6D ───────────────────────────────────────────────────────
    Motor6D* m6d = Object::cast_to<Motor6D>(n);
    if (m6d) {
        if (strcmp(key,"C0")           == 0) { lua_pushlightuserdata(L,(void*)m6d); lua_pushnil(L); return 1; }
        if (strcmp(key,"C1")           == 0) { lua_pushlightuserdata(L,(void*)m6d); lua_pushnil(L); return 1; }
        if (strcmp(key,"DesiredAngle") == 0) { lua_pushnumber(L, m6d->get_desired_angle()); return 1; }
        if (strcmp(key,"MaxVelocity")  == 0) { lua_pushnumber(L, m6d->get_max_velocity());  return 1; }
        if (strcmp(key,"Enabled")      == 0) { lua_pushboolean(L, m6d->get_enabled());       return 1; }
        if (strcmp(key,"Part0")        == 0) {
            Node* p = m6d->get_part0(); if (p) wrap_node(L, p); else lua_pushnil(L); return 1;
        }
        if (strcmp(key,"Part1")        == 0) {
            Node* p = m6d->get_part1(); if (p) wrap_node(L, p); else lua_pushnil(L); return 1;
        }
    }

    // ── RobloxTool ────────────────────────────────────────────────────
    RobloxTool* rtool = Object::cast_to<RobloxTool>(n);
    if (rtool) {
        if (strcmp(key,"ToolTip")      == 0) { lua_pushstring(L, rtool->get_tool_tip().utf8().get_data()); return 1; }
        if (strcmp(key,"CanBeDropped") == 0) { lua_pushboolean(L, rtool->get_can_be_dropped()); return 1; }
        auto make_tool_sig = [&](std::vector<RobloxTool::LuaCallback>& cbs) -> int {
            lua_newtable(L);
            lua_pushlightuserdata(L,(void*)rtool);
            lua_pushlightuserdata(L,(void*)&cbs);
            lua_pushcclosure(L,[](lua_State* LL)->int{
                RobloxTool* d=(RobloxTool*)lua_touserdata(LL,lua_upvalueindex(1));
                auto* c=(std::vector<RobloxTool::LuaCallback>*)lua_touserdata(LL,lua_upvalueindex(2));
                if (!c || !lua_isfunction(LL,2)){lua_pushnil(LL);return 1;}
                lua_pushvalue(LL,2); int ref=lua_ref(LL,-1); lua_pop(LL,1);
                c->push_back({gl_main_of(LL),ref,true});
                _gl_push_connection(LL, d, ref); return 1;
            },"Connect",2);
            lua_setfield(L,-2,"Connect"); return 1;
        };
        if (strcmp(key,"Equipped")    == 0) return make_tool_sig(rtool->equipped_cbs);
        if (strcmp(key,"Unequipped")  == 0) return make_tool_sig(rtool->unequipped_cbs);
        if (strcmp(key,"Activated")   == 0) return make_tool_sig(rtool->activated_cbs);
        if (strcmp(key,"Deactivated") == 0) return make_tool_sig(rtool->deactivated_cbs);
        if (strcmp(key,"Activate")    == 0) {
            lua_pushlightuserdata(L,(void*)rtool);
            lua_pushcclosure(L,[](lua_State* LL)->int{
                RobloxTool* t=(RobloxTool*)lua_touserdata(LL,lua_upvalueindex(1));
                if(t) t->activate(); return 0;
            },"Activate",1); return 1;
        }
    }

    // ── UserInputService ──────────────────────────────────────────────
    UserInputService* uis = Object::cast_to<UserInputService>(n);
    if (uis) {
        if (strcmp(key,"MouseEnabled")    == 0) { lua_pushboolean(L, true); return 1; }
        if (strcmp(key,"KeyboardEnabled") == 0) { lua_pushboolean(L, true); return 1; }
        if (strcmp(key,"TouchEnabled")    == 0) { lua_pushboolean(L, uis->get_touch_enabled()?1:0); return 1; }
        if (strcmp(key,"GamepadEnabled")  == 0) { lua_pushboolean(L, uis->get_gamepad_connected()?1:0); return 1; }
        if (strcmp(key,"IsKeyDown") == 0 || strcmp(key,"IsKeyboardKeyDown") == 0) {
            lua_pushlightuserdata(L,(void*)uis);
            lua_pushcclosure(L,[](lua_State* LL)->int{
                UserInputService* u=(UserInputService*)lua_touserdata(LL,lua_upvalueindex(1));
                if (!u){ lua_pushboolean(LL,0); return 1; }
                int kc = 0;
                if (lua_isnumber(LL,2)) {
                    kc = (int)lua_tonumber(LL,2);
                } else if (lua_isstring(LL,2)) {
                    // Acepta tambien el nombre de la tecla ("W", "LeftShift"…)
                    const char* s = lua_tostring(LL,2);
                    if (strlen(s) == 1) { char c=s[0]; if(c>='a'&&c<='z') c-=32; kc=(int)c; }
                    else if (!strcmp(s,"LeftShift")||!strcmp(s,"RightShift"))     kc=304;
                    else if (!strcmp(s,"Space"))                                  kc=32;
                    else if (!strcmp(s,"LeftControl")||!strcmp(s,"RightControl")) kc=306;
                    else if (!strcmp(s,"LeftAlt")||!strcmp(s,"RightAlt"))         kc=308;
                    else if (!strcmp(s,"Return")||!strcmp(s,"Enter"))            kc=13;
                    else if (!strcmp(s,"Tab"))    kc=9;
                    else if (!strcmp(s,"Escape")) kc=27;
                    else if (!strcmp(s,"Up"))     kc=273;
                    else if (!strcmp(s,"Down"))   kc=274;
                    else if (!strcmp(s,"Left"))   kc=276;
                    else if (!strcmp(s,"Right"))  kc=275;
                }
                lua_pushboolean(LL, (kc!=0 && u->is_key_down(kc))?1:0); return 1;
            },"IsKeyDown",1); return 1;
        }
        if (strcmp(key,"GetMouseDelta") == 0) {
            lua_pushlightuserdata(L,(void*)uis);
            lua_pushcclosure(L,[](lua_State* LL)->int{
                UserInputService* u=(UserInputService*)lua_touserdata(LL,lua_upvalueindex(1));
                Vector2 d = u ? u->get_mouse_delta_v() : Vector2();
                push_vector3(LL, d.x, d.y, 0.0f); return 1;
            },"GetMouseDelta",1); return 1;
        }
        if (strcmp(key,"IsGamepadButtonDown") == 0) {
            lua_pushlightuserdata(L,(void*)uis);
            lua_pushcclosure(L,[](lua_State* LL)->int{
                UserInputService* u=(UserInputService*)lua_touserdata(LL,lua_upvalueindex(1));
                int btn = lua_isnumber(LL,3) ? (int)lua_tonumber(LL,3) : 0;
                lua_pushboolean(LL, (u && u->is_gamepad_button_down(0, btn))?1:0); return 1;
            },"IsGamepadButtonDown",1); return 1;
        }
        if (strcmp(key,"GetLastInputType") == 0) {
            lua_pushlightuserdata(L,(void*)uis);
            lua_pushcclosure(L,[](lua_State* LL)->int{
                UserInputService* u=(UserInputService*)lua_touserdata(LL,lua_upvalueindex(1));
                lua_pushinteger(LL, u ? u->get_last_input_type() : 0); return 1;
            },"GetLastInputType",1); return 1;
        }
        if (strcmp(key,"IsMouseButtonDown") == 0) {
            lua_pushlightuserdata(L,(void*)uis);
            lua_pushcclosure(L,[](lua_State* LL)->int{
                UserInputService* u=(UserInputService*)lua_touserdata(LL,lua_upvalueindex(1));
                if (!u || !lua_isnumber(LL,2)){ lua_pushboolean(LL,0); return 1; }
                int btn=(int)lua_tonumber(LL,2);
                lua_pushboolean(LL, u->is_mouse_button_down(btn)?1:0); return 1;
            },"IsMouseButtonDown",1); return 1;
        }
        if (strcmp(key,"GetMouseLocation") == 0) {
            lua_pushlightuserdata(L,(void*)uis);
            lua_pushcclosure(L,[](lua_State* LL)->int{
                UserInputService* u=(UserInputService*)lua_touserdata(LL,lua_upvalueindex(1));
                if (!u){ lua_pushnil(LL); return 1; }
                Vector2 pos = u->get_mouse_location_v();
                // Return as Vector2 table
                lua_newtable(LL);
                lua_pushnumber(LL,pos.x); lua_setfield(LL,-2,"X");
                lua_pushnumber(LL,pos.y); lua_setfield(LL,-2,"Y");
                lua_pushnumber(LL,pos.x); lua_setfield(LL,-2,"x");
                lua_pushnumber(LL,pos.y); lua_setfield(LL,-2,"y");
                return 1;
            },"GetMouseLocation",1); return 1;
        }
        // Signals
        auto make_uis_sig = [&](std::vector<UserInputService::LuaCallback>& cbs) -> int {
            lua_newtable(L);
            lua_pushlightuserdata(L,(void*)uis);
            lua_pushlightuserdata(L,(void*)&cbs);
            lua_pushcclosure(L,[](lua_State* LL)->int{
                UserInputService* d=(UserInputService*)lua_touserdata(LL,lua_upvalueindex(1));
                auto* c=(std::vector<UserInputService::LuaCallback>*)lua_touserdata(LL,lua_upvalueindex(2));
                if(!c||!lua_isfunction(LL,2)){lua_pushnil(LL);return 1;}
                lua_pushvalue(LL,2); int ref=lua_ref(LL,-1); lua_pop(LL,1);
                c->push_back({gl_main_of(LL),ref,true}); _gl_push_connection(LL, d, ref); return 1;
            },"Connect",2);
            lua_setfield(L,-2,"Connect"); return 1;
        };
        if (strcmp(key,"InputBegan")   == 0) return make_uis_sig(uis->input_began_cbs);
        if (strcmp(key,"InputEnded")   == 0) return make_uis_sig(uis->input_ended_cbs);
        if (strcmp(key,"InputChanged") == 0) return make_uis_sig(uis->input_changed_cbs);
    }

    // ── CollectionService ─────────────────────────────────────────────
    CollectionService* cs = Object::cast_to<CollectionService>(n);
    if (cs) {
        if (strcmp(key,"AddTag") == 0) {
            lua_pushlightuserdata(L,(void*)cs);
            lua_pushcclosure(L,[](lua_State* LL)->int{
                CollectionService* c=(CollectionService*)lua_touserdata(LL,lua_upvalueindex(1));
                GodotObjectWrapper* w=(GodotObjectWrapper*)lua_touserdata(LL,2);
                const char* tag=luaL_checkstring(LL,3);
                if(c&&w&&gow_node(w)) c->add_tag(gow_node(w),String(tag));
                return 0;
            },"AddTag",1); return 1;
        }
        if (strcmp(key,"RemoveTag") == 0) {
            lua_pushlightuserdata(L,(void*)cs);
            lua_pushcclosure(L,[](lua_State* LL)->int{
                CollectionService* c=(CollectionService*)lua_touserdata(LL,lua_upvalueindex(1));
                GodotObjectWrapper* w=(GodotObjectWrapper*)lua_touserdata(LL,2);
                const char* tag=luaL_checkstring(LL,3);
                if(c&&w&&gow_node(w)) c->remove_tag(gow_node(w),String(tag));
                return 0;
            },"RemoveTag",1); return 1;
        }
        if (strcmp(key,"HasTag") == 0) {
            lua_pushlightuserdata(L,(void*)cs);
            lua_pushcclosure(L,[](lua_State* LL)->int{
                CollectionService* c=(CollectionService*)lua_touserdata(LL,lua_upvalueindex(1));
                GodotObjectWrapper* w=(GodotObjectWrapper*)lua_touserdata(LL,2);
                const char* tag=luaL_checkstring(LL,3);
                bool r=false;
                if(c&&w&&gow_node(w)) r=c->has_tag(gow_node(w),String(tag));
                lua_pushboolean(LL,r?1:0); return 1;
            },"HasTag",1); return 1;
        }
        if (strcmp(key,"GetTagged") == 0) {
            lua_pushlightuserdata(L,(void*)cs);
            lua_pushcclosure(L,[](lua_State* LL)->int{
                CollectionService* c=(CollectionService*)lua_touserdata(LL,lua_upvalueindex(1));
                if(!c||!lua_isstring(LL,2)){lua_newtable(LL);return 1;}
                String tag(lua_tostring(LL,2));
                TypedArray<Node> arr=c->get_tagged(tag);
                lua_newtable(LL);
                for(int i=0;i<arr.size();i++){
                    Node* nd=Object::cast_to<Node>(arr[i]);
                    if(nd){ lua_pushlightuserdata(LL,(void*)nd);
                        GodotObjectWrapper* wr=(GodotObjectWrapper*)lua_newuserdata(LL,sizeof(GodotObjectWrapper));
                        gow_set(wr, nd);
                        luaL_getmetatable(LL,"GodotObject"); lua_setmetatable(LL,-2);
                        lua_rawseti(LL,-3,i+1);
                    } else { lua_pop(LL,1); }
                }
                return 1;
            },"GetTagged",1); return 1;
        }
        if (strcmp(key,"GetTags") == 0) {
            lua_pushlightuserdata(L,(void*)cs);
            lua_pushcclosure(L,[](lua_State* LL)->int{
                CollectionService* c=(CollectionService*)lua_touserdata(LL,lua_upvalueindex(1));
                GodotObjectWrapper* w=(GodotObjectWrapper*)lua_touserdata(LL,2);
                lua_newtable(LL);
                if(!c||!w||!gow_node(w)) return 1;
                PackedStringArray tags=c->get_tags(gow_node(w));
                for(int i=0;i<tags.size();i++){
                    lua_pushstring(LL,tags[i].utf8().get_data());
                    lua_rawseti(LL,-2,i+1);
                }
                return 1;
            },"GetTags",1); return 1;
        }
    }

    // ── Puente genérico de lectura (solo clases nuevas con meta _gl_bridge) ──
    if (n->has_meta("_gl_bridge")) {
        if (gl_has_property(n, String(key))) {
            Variant gv = n->get(String(key));
            if (gl_variant_to_luau(L, gv)) return 1;
        }
        String _mk = String("_glp_") + String(key);
        if (n->has_meta(_mk) && gl_variant_to_luau(L, n->get_meta(_mk))) return 1;
    }

    Node* child = n->get_node_or_null(NodePath(key));
    if (child) { wrap_node(L, child); return 1; }

    // COMO ROBLOX: indexar un miembro que no existe es un ERROR, no nil.
    // (Para sondear sin error existen FindFirstChild / pcall, igual que alla.)
    luaL_error(L, "%s is not a valid member of %s \"%s\"",
               key, gl_roblox_classname(n), gl_instance_fullname(n).utf8().get_data());
    return 0;
}

// ════════════════════════════════════════════════════════════════════
//  godot_object_newindex — __newindex of GodotObject
////
//  godot_object_newindex — __newindex de GodotObject
// ════════════════════════════════════════════════════════════════════
static int godot_object_newindex_impl(lua_State* L) {
    GodotObjectWrapper* wrapper = (GodotObjectWrapper*)lua_touserdata(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (!wrapper || !gow_node(wrapper)) return 0;
    Node* n = gow_node(wrapper);

    // Mouse.TargetFilter — subárbol que el rayo ignora (1.14.10 Finish).
    if (RobloxMouse* mo = Object::cast_to<RobloxMouse>(n)) {
        if (strcmp(key, "TargetFilter") == 0) {
            Node* tf = lua_isnil(L, 3) ? nullptr : _gl_node_from_lua(L, 3);
            mo->target_filter_id = tf ? (uint64_t)tf->get_instance_id() : 0;
            return 0;
        }
    }

    // ── PlayerObject: escrituras de miembros de Player (CameraMode, etc.) ──
    // Antes LocalPlayer ERA el personaje y estas escrituras iban al character;
    // ahora LocalPlayer es el Player. Se aceptan aquí para no romper scripts
    // (p.ej. el CameraModule incluido hace LocalPlayer.CameraMode = mode).
    if (PlayerObject* plr = Object::cast_to<PlayerObject>(n)) {
        if (strcmp(key, "CameraMode") == 0) {
            plr->camera_mode = (int)luaL_checknumber(L, 3);
            Node* ch = plr->get_character();
            if (ch && ch->has_method("set_camera_mode")) ch->call("set_camera_mode", plr->camera_mode);
            return 0;
        }
        if (strcmp(key, "DisplayName") == 0) { plr->display_name = String(luaL_checkstring(L, 3)); return 0; }
        // Team (1.14.10): antes se aceptaba y se DESCARTABA en silencio. Ahora
        // asigna de verdad y el servidor lo difunde (cada máquina construye su
        // propio objeto Player, así que el equipo hay que replicarlo aparte).
        if (strcmp(key, "Team") == 0) {
            Node* t = lua_isnil(L, 3) ? nullptr : _gl_node_from_lua(L, 3);
            plr->team_id = t ? (uint64_t)t->get_instance_id() : 0;
            if (Node* ns = _gl_find_network_service(plr))
                ns->call("net_set_team", (Object*)plr, (Object*)t);
            return 0;
        }
        if (strcmp(key, "Neutral") == 0) {
            if (lua_toboolean(L, 3)) {   // Neutral = true → sin equipo
                plr->team_id = 0;
                if (Node* ns = _gl_find_network_service(plr))
                    ns->call("net_set_team", (Object*)plr, (Object*)nullptr);
            }
            return 0;
        }
        if (strcmp(key, "TeamColor") == 0) { plr->team_color = (int)luaL_checknumber(L, 3); return 0; }
        if (strcmp(key, "CameraMinZoomDistance") == 0 || strcmp(key, "CameraMaxZoomDistance") == 0 ||
            strcmp(key, "AutoJumpEnabled") == 0 || strcmp(key, "DevComputerMovementMode") == 0 ||
            strcmp(key, "DevTouchMovementMode") == 0 || strcmp(key, "DevCameraOcclusionMode") == 0 ||
            strcmp(key, "DevComputerCameraMode") == 0)
            return 0;   // aceptar y descartar (no romper)
    }

    if (strcmp(key, "Name") == 0) {
        n->set_name(luaL_checkstring(L, 3));
        return 0;
    }

    if (strcmp(key, "Parent") == 0) {
        if (lua_isnil(L, 3)) {
            if (n->get_parent()) n->get_parent()->remove_child(n);
        } else {
            GodotObjectWrapper* p_wrap = (GodotObjectWrapper*)lua_touserdata(L, 3);
            if (p_wrap && gow_node(p_wrap) && gow_node(p_wrap) != n) {
                if (n->get_parent()) n->get_parent()->remove_child(n);
                gow_node(p_wrap)->add_child(n);
                // Set owner so the node is visible/saveable in the editor
                if (n->is_inside_tree()) {
                    Node* scene_root = n->get_tree()->get_edited_scene_root();
                    if (!scene_root) scene_root = (Node*)n->get_tree()->get_root(); // <-- ADDED (Node*) / AÑADIDO (Node*)
                    if (!n->get_owner()) n->set_owner(scene_root);
                }
            }
        }
        return 0;
    }

    if (strcmp(key, "Position") == 0) {
        LuauVector3* vec = check_vector3(L, 3);
        Node3D* n3d = Object::cast_to<Node3D>(n);
        if (vec && n3d) {
            if (n3d->is_inside_tree()) n3d->set_global_position(Vector3(vec->x, vec->y, vec->z));
            else n3d->set_position(Vector3(vec->x, vec->y, vec->z));
        }
        return 0;
    }

    if (strcmp(key, "Rotation") == 0) {
        LuauVector3* vec = check_vector3(L, 3);
        Node3D* n3d = Object::cast_to<Node3D>(n);
        if (vec && n3d) {
            if (n3d->is_inside_tree()) n3d->set_global_rotation_degrees(Vector3(vec->x, vec->y, vec->z));
            else n3d->set_rotation_degrees(Vector3(vec->x, vec->y, vec->z));
        }
        return 0;
    }

    if (strcmp(key, "CFrame") == 0 && lua_istable(L, 3)) {
        Node3D* n3d_cf = Object::cast_to<Node3D>(n);
        if (n3d_cf) {
            lua_getfield(L, 3, "X"); float tx = (float)lua_tonumber(L,-1); lua_pop(L,1);
            lua_getfield(L, 3, "Y"); float ty = (float)lua_tonumber(L,-1); lua_pop(L,1);
            lua_getfield(L, 3, "Z"); float tz = (float)lua_tonumber(L,-1); lua_pop(L,1);
            lua_getfield(L, 3, "m00"); float m00 = (float)lua_tonumber(L,-1); lua_pop(L,1);
            lua_getfield(L, 3, "m01"); float m01 = (float)lua_tonumber(L,-1); lua_pop(L,1);
            lua_getfield(L, 3, "m02"); float m02 = (float)lua_tonumber(L,-1); lua_pop(L,1);
            lua_getfield(L, 3, "m10"); float m10 = (float)lua_tonumber(L,-1); lua_pop(L,1);
            lua_getfield(L, 3, "m11"); float m11 = (float)lua_tonumber(L,-1); lua_pop(L,1);
            lua_getfield(L, 3, "m12"); float m12 = (float)lua_tonumber(L,-1); lua_pop(L,1);
            lua_getfield(L, 3, "m20"); float m20 = (float)lua_tonumber(L,-1); lua_pop(L,1);
            lua_getfield(L, 3, "m21"); float m21 = (float)lua_tonumber(L,-1); lua_pop(L,1);
            lua_getfield(L, 3, "m22"); float m22 = (float)lua_tonumber(L,-1); lua_pop(L,1);
            Transform3D t;
            t.origin = Vector3(tx, ty, tz);
            t.basis.rows[0] = Vector3(m00, m01, m02);
            t.basis.rows[1] = Vector3(m10, m11, m12);
            t.basis.rows[2] = Vector3(m20, m21, m22);
            if (n3d_cf->is_inside_tree()) n3d_cf->set_global_transform(t);
            else n3d_cf->set_transform(t);
        }
        return 0;
    }

    if (strcmp(key, "CameraMode") == 0) {
        RobloxPlayer* rp = Object::cast_to<RobloxPlayer>(n);
        if (rp) rp->set_camera_mode((int)luaL_checknumber(L, 3));
        RobloxPlayer2D* rp2d = Object::cast_to<RobloxPlayer2D>(n);
        if (rp2d) rp2d->set_camera_mode((int)luaL_checknumber(L, 3));
        return 0;
    }
    if (strcmp(key, "MouseSensitivity") == 0) {
        RobloxPlayer* rp = Object::cast_to<RobloxPlayer>(n);
        if (rp) rp->set_mouse_sensitivity((float)luaL_checknumber(L, 3));
        return 0;
    }

    if (strcmp(key, "MovementType") == 0) {
        RobloxPlayer2D* rp2d = Object::cast_to<RobloxPlayer2D>(n);
        if (rp2d) rp2d->set_movement_type((int)luaL_checknumber(L, 3));
        return 0;
    }

    Humanoid* hum = Object::cast_to<Humanoid>(n);
    if (hum) {
        if (strcmp(key, "Health")               == 0) { hum->set_health((float)luaL_checknumber(L, 3));               return 0; }
        if (strcmp(key, "MaxHealth")            == 0) { hum->set_max_health((float)luaL_checknumber(L, 3));            return 0; }
        if (strcmp(key, "WalkSpeed")            == 0) { hum->set_walk_speed((float)luaL_checknumber(L, 3));            return 0; }
        if (strcmp(key, "JumpPower")            == 0) { hum->set_jump_power((float)luaL_checknumber(L, 3));            return 0; }
        if (strcmp(key, "JumpHeight")           == 0) { hum->set_jump_height((float)luaL_checknumber(L, 3));           return 0; }
        if (strcmp(key, "HipHeight")            == 0) { hum->set_hip_height((float)luaL_checknumber(L, 3));            return 0; }
        if (strcmp(key, "Gravity")              == 0) { hum->set_gravity_val((float)luaL_checknumber(L, 3));           return 0; }
        if (strcmp(key, "AutoRotate")           == 0) { hum->set_auto_rotate(lua_toboolean(L, 3) != 0);                return 0; }
        if (strcmp(key, "AutoJumpEnabled")      == 0) { hum->set_auto_jump_enabled(lua_toboolean(L, 3) != 0);          return 0; }
        if (strcmp(key, "DisplayName")          == 0) { hum->set_display_name(String(luaL_checkstring(L, 3)));         return 0; }
        if (strcmp(key, "NameDisplayDistance")  == 0) { hum->set_name_display_distance((float)luaL_checknumber(L, 3)); return 0; }
        if (strcmp(key, "HealthDisplayDistance")== 0) { hum->set_health_display_distance((float)luaL_checknumber(L,3));return 0; }
        if (strcmp(key, "RigType")              == 0) { hum->set_rig_type((int)luaL_checknumber(L, 3));                return 0; }
        if (strcmp(key, "EvaluateStateMachine") == 0) { hum->set_evaluate_state_machine(lua_toboolean(L, 3) != 0);    return 0; }
        if (strcmp(key, "BreakJointsOnDeath")   == 0) { hum->set_break_joints_on_death(lua_toboolean(L, 3) != 0);     return 0; }
        if (strcmp(key, "RequiresNeck")         == 0) { hum->set_requires_neck(lua_toboolean(L, 3) != 0);              return 0; }
    }

    Humanoid2D* hum2d = Object::cast_to<Humanoid2D>(n);
    if (hum2d) {
        if (strcmp(key, "Health")    == 0) { hum2d->set_health((float)luaL_checknumber(L, 3));    return 0; }
        if (strcmp(key, "MaxHealth") == 0) { hum2d->set_max_health((float)luaL_checknumber(L, 3)); return 0; }
        // WalkSpeed/JumpPower delegate to the parent RobloxPlayer2D (movement lives in the player) / delegan al RobloxPlayer2D padre (movimiento está en el player)
        if (strcmp(key, "WalkSpeed") == 0) {
            RobloxPlayer2D* pp = Object::cast_to<RobloxPlayer2D>(n->get_parent());
            if (pp) pp->set_walk_speed((float)luaL_checknumber(L, 3));
            return 0;
        }
        if (strcmp(key, "JumpPower") == 0) {
            RobloxPlayer2D* pp = Object::cast_to<RobloxPlayer2D>(n->get_parent());
            // Convertir a negativo internamente (en 2D Y crece hacia abajo)
            if (pp) pp->set_jump_power(-(float)luaL_checknumber(L, 3));
            return 0;
        }
    }

    RobloxPart* part = Object::cast_to<RobloxPart>(n);
    if (part) {
        if (strcmp(key, "Anchored")         == 0) { part->set_anchored(lua_toboolean(L,3)!=0);                   return 0; }
        if (strcmp(key, "CanCollide")       == 0) { part->set_can_collide(lua_toboolean(L,3)!=0);                return 0; }
        if (strcmp(key, "CanTouch")         == 0) { part->set_can_touch(lua_toboolean(L,3)!=0);                  return 0; }
        if (strcmp(key, "CanQuery")         == 0) { part->set_can_query(lua_toboolean(L,3)!=0);                  return 0; }
        if (strcmp(key, "CastShadow")       == 0) { part->set_cast_shadow(lua_toboolean(L,3)!=0);                return 0; }
        if (strcmp(key, "Locked")           == 0) { part->set_locked(lua_toboolean(L,3)!=0);                     return 0; }
        if (strcmp(key, "Massless")         == 0) { part->set_massless(lua_toboolean(L,3)!=0);                   return 0; }
        if (strcmp(key, "Transparency")     == 0) { part->set_transparency((float)luaL_checknumber(L,3));        return 0; }
        if (strcmp(key, "Reflectance")      == 0) { part->set_reflectance((float)luaL_checknumber(L,3));         return 0; }
        if (strcmp(key, "Material")         == 0) { part->set_roblox_material((int)luaL_checknumber(L,3));       return 0; }
        if (strcmp(key, "Shape")            == 0) { part->set_roblox_shape((int)luaL_checknumber(L,3));          return 0; }
        if (strcmp(key, "Density")          == 0) { part->set_density((float)luaL_checknumber(L,3));             return 0; }
        if (strcmp(key, "Friction")         == 0) { part->set_friction_val((float)luaL_checknumber(L,3));        return 0; }
        if (strcmp(key, "Elasticity")       == 0) { part->set_elasticity((float)luaL_checknumber(L,3));          return 0; }
        if (strcmp(key, "FrictionWeight")   == 0) { part->set_friction_weight((float)luaL_checknumber(L,3));     return 0; }
        if (strcmp(key, "ElasticityWeight") == 0) { part->set_elasticity_weight((float)luaL_checknumber(L,3));   return 0; }
        if (strcmp(key, "TopSurface")       == 0) { part->set_top_surface((int)luaL_checknumber(L,3));           return 0; }
        if (strcmp(key, "BottomSurface")    == 0) { part->set_bottom_surface((int)luaL_checknumber(L,3));        return 0; }
        if (strcmp(key, "FrontSurface")     == 0) { part->set_front_surface((int)luaL_checknumber(L,3));         return 0; }
        if (strcmp(key, "BackSurface")      == 0) { part->set_back_surface((int)luaL_checknumber(L,3));          return 0; }
        if (strcmp(key, "LeftSurface")      == 0) { part->set_left_surface((int)luaL_checknumber(L,3));          return 0; }
        if (strcmp(key, "RightSurface")     == 0) { part->set_right_surface((int)luaL_checknumber(L,3));         return 0; }
        if (strcmp(key, "Color") == 0 || strcmp(key, "BrickColor") == 0) {
            LuauColor3* col = check_color3(L, 3);
            if (col) part->set_color(Color(col->r, col->g, col->b));
            return 0;
        }
        if (strcmp(key, "Size") == 0) {
            LuauVector3* vec = check_vector3(L, 3);
            if (vec) part->set_size(Vector3(vec->x, vec->y, vec->z));
            return 0;
        }
        if (strcmp(key, "Position") == 0) {
            LuauVector3* v = check_vector3(L, 3);
            if (v) part->set_global_position(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key, "Orientation") == 0) {
            LuauVector3* v = check_vector3(L, 3);
            if (v) part->set_rotation_degrees(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key, "Velocity") == 0 || strcmp(key, "AssemblyLinearVelocity") == 0) {
            LuauVector3* v = check_vector3(L, 3);
            if (v) part->set_linear_velocity(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key, "AngularVelocity") == 0 || strcmp(key, "AssemblyAngularVelocity") == 0) {
            LuauVector3* v = check_vector3(L, 3);
            if (v) part->set_angular_velocity(Vector3(v->x, v->y, v->z));
            return 0;
        }
    }

    // ── RobloxWorkspace write ─────────────────────────────────────
    RobloxWorkspace* ws = Object::cast_to<RobloxWorkspace>(n);
    if (ws) {
        if (strcmp(key, "Gravity")                   == 0) { ws->set_gravity((float)luaL_checknumber(L,3));                      return 0; }
        if (strcmp(key, "FallenPartsDestroyHeight")  == 0) { ws->set_fallen_parts_destroy_height((float)luaL_checknumber(L,3));  return 0; }
        if (strcmp(key, "StreamingEnabled")          == 0) { ws->set_streaming_enabled(lua_toboolean(L,3)!=0);                   return 0; }
        if (strcmp(key, "AirDensity")                == 0) { ws->set_air_density((float)luaL_checknumber(L,3));                  return 0; }
        if (strcmp(key, "TouchesUseCollisionGroups") == 0) { ws->set_touches_use_collision_groups(lua_toboolean(L,3)!=0);        return 0; }
    }

    // ── RobloxPlayer write ────────────────────────────────────────
    RobloxPlayer* rp_w = Object::cast_to<RobloxPlayer>(n);
    if (rp_w) {
        if (strcmp(key, "UserId")                  == 0) { return 0; }   // inmutable: lo asigna el servidor (host=1, 2,3,...)
        if (strcmp(key, "DisplayName")             == 0) { rp_w->set_display_name(String(luaL_checkstring(L,3)));            return 0; }
        if (strcmp(key, "AccountAge")              == 0) { rp_w->set_account_age((int)luaL_checknumber(L,3));                return 0; }
        if (strcmp(key, "AutoJumpEnabled")         == 0) { rp_w->set_auto_jump_enabled(lua_toboolean(L,3)!=0);               return 0; }
        if (strcmp(key, "DevCameraOcclusionMode")  == 0) { rp_w->set_dev_camera_occlusion_mode((int)luaL_checknumber(L,3));  return 0; }
        if (strcmp(key, "DevComputerMovementMode") == 0) { rp_w->set_dev_computer_movement_mode((int)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key, "DevTouchMovementMode")    == 0) { rp_w->set_dev_touch_movement_mode((int)luaL_checknumber(L,3));    return 0; }
        if (strcmp(key, "TeamColor") == 0) {
            LuauColor3* col = check_color3(L, 3);
            if (col) rp_w->set_team_color(Color(col->r, col->g, col->b));
            return 0;
        }
    }

    Lighting* light = Object::cast_to<Lighting>(n);
    if (light) {
        if (strcmp(key, "Brightness")               == 0) { light->set_brightness((float)luaL_checknumber(L,3));          return 0; }
        if (strcmp(key, "ClockTime")                == 0) { light->set_clock_time((float)luaL_checknumber(L,3));          return 0; }
        if (strcmp(key, "GeographicLatitude")       == 0) { light->set_geographic_latitude((float)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key, "GlobalShadows")            == 0) { light->set_global_shadows(lua_toboolean(L,3) != 0);          return 0; }
        if (strcmp(key, "ShadowSoftness")           == 0) { light->set_shadow_softness((float)luaL_checknumber(L,3));    return 0; }
        if (strcmp(key, "FogDensity")               == 0) { light->set_fog_density((float)luaL_checknumber(L,3));        return 0; }
        if (strcmp(key, "FogStart")                 == 0) { light->set_fog_start((float)luaL_checknumber(L,3));          return 0; }
        if (strcmp(key, "FogEnd")                   == 0) { light->set_fog_end((float)luaL_checknumber(L,3));            return 0; }
        if (strcmp(key, "ExposureCompensation")     == 0) { light->set_exposure_comp((float)luaL_checknumber(L,3));      return 0; }
        if (strcmp(key, "EnvironmentDiffuseScale")  == 0) { light->set_env_diffuse_scale((float)luaL_checknumber(L,3));  return 0; }
        if (strcmp(key, "EnvironmentSpecularScale") == 0) { light->set_env_specular_scale((float)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key, "Technology")               == 0) { light->set_technology((int)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key, "Ambient")                  == 0) { LuauColor3* c = check_color3(L, 3); if(c) light->set_ambient(Color(c->r,c->g,c->b)); return 0; }
        if (strcmp(key, "OutdoorAmbient")           == 0) { LuauColor3* c = check_color3(L, 3); if(c) light->set_outdoor_ambient(Color(c->r,c->g,c->b)); return 0; }
        if (strcmp(key, "FogColor")                 == 0) { LuauColor3* c = check_color3(L, 3); if(c) light->set_fog_color(Color(c->r,c->g,c->b)); return 0; }
        if (strcmp(key, "ColorShift_Top")           == 0) { LuauColor3* c = check_color3(L, 3); if(c) light->set_color_shift_top(Color(c->r,c->g,c->b)); return 0; }
        if (strcmp(key, "ColorShift_Bottom")        == 0) { LuauColor3* c = check_color3(L, 3); if(c) light->set_color_shift_bottom(Color(c->r,c->g,c->b)); return 0; }
        if (strcmp(key, "TimeOfDay")                == 0) { light->set_time_of_day(String(luaL_checkstring(L,3)));         return 0; }
        if (strcmp(key, "DayNightCycle")            == 0) { light->set_day_night_cycle(lua_toboolean(L,3) != 0);           return 0; }
        if (strcmp(key, "DayLengthMinutes")         == 0) { light->set_day_length_minutes((float)luaL_checknumber(L,3));   return 0; }
    }

    // ── BodyMovers — newindex ──────────────────────────────────────
    BodyVelocity* bv_w = Object::cast_to<BodyVelocity>(n);
    if (bv_w) {
        if (strcmp(key,"Velocity") == 0) {
            LuauVector3* v = check_vector3(L, 3);
            if (v) bv_w->set_velocity(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key,"MaxForce") == 0) {
            LuauVector3* v = check_vector3(L, 3);
            if (v) bv_w->set_max_force(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key,"P") == 0) { bv_w->set_p((float)luaL_checknumber(L,3)); return 0; }
    }
    BodyPosition* bpos_w = Object::cast_to<BodyPosition>(n);
    if (bpos_w) {
        if (strcmp(key,"Position") == 0) {
            LuauVector3* v = check_vector3(L, 3);
            if (v) bpos_w->set_position_val(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key,"MaxForce") == 0) {
            LuauVector3* v = check_vector3(L, 3);
            if (v) bpos_w->set_max_force(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key,"P") == 0) { bpos_w->set_p((float)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key,"D") == 0) { bpos_w->set_d((float)luaL_checknumber(L,3)); return 0; }
    }
    BodyForce* bf_w = Object::cast_to<BodyForce>(n);
    if (bf_w) {
        if (strcmp(key,"Force") == 0) {
            LuauVector3* v = check_vector3(L, 3);
            if (v) bf_w->set_force(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key,"RelativeTo") == 0) { bf_w->set_relative_to(lua_toboolean(L,3)!=0); return 0; }
    }
    BodyAngularVelocity* bav_w = Object::cast_to<BodyAngularVelocity>(n);
    if (bav_w) {
        if (strcmp(key,"AngularVelocity") == 0) {
            LuauVector3* v = check_vector3(L, 3);
            if (v) bav_w->set_angular_velocity(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key,"MaxTorque") == 0) {
            LuauVector3* v = check_vector3(L, 3);
            if (v) bav_w->set_max_torque(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key,"P") == 0) { bav_w->set_p((float)luaL_checknumber(L,3)); return 0; }
    }
    BodyGyro* bgyro_w = Object::cast_to<BodyGyro>(n);
    if (bgyro_w) {
        if (strcmp(key,"MaxTorque") == 0) {
            LuauVector3* v = check_vector3(L, 3);
            if (v) bgyro_w->set_max_torque(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key,"P") == 0) { bgyro_w->set_p((float)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key,"D") == 0) { bgyro_w->set_d((float)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key,"CFrame") == 0) {
            // CFrame es una TABLA en este motor (no userdata). Aceptamos:
            //  - un nodo (GodotObject) → usa su transform global
            //  - una tabla CFrame con componentes m00..m22 + X/Y/Z
            if (_gl_has_metatable(L, 3, "GodotObject")) {
                GodotObjectWrapper* wr = (GodotObjectWrapper*)lua_touserdata(L, 3);
                Node3D* n3d = Object::cast_to<Node3D>(gow_node(wr));
                if (n3d) bgyro_w->set_cframe(n3d->get_global_transform());
            } else if (lua_istable(L, 3)) {
                auto gf = [&](const char* k) -> float {
                    lua_getfield(L, 3, k); float r = (float)lua_tonumber(L, -1); lua_pop(L, 1); return r;
                };
                Transform3D t;
                t.origin = Vector3(gf("X"), gf("Y"), gf("Z"));
                t.basis.rows[0] = Vector3(gf("m00"), gf("m01"), gf("m02"));
                t.basis.rows[1] = Vector3(gf("m10"), gf("m11"), gf("m12"));
                t.basis.rows[2] = Vector3(gf("m20"), gf("m21"), gf("m22"));
                bgyro_w->set_cframe(t);
            }
            return 0;
        }
    }

    // ── Constraints — newindex ─────────────────────────────────────
    WeldConstraint* weld_w = Object::cast_to<WeldConstraint>(n);
    if (weld_w) {
        if (strcmp(key,"Part0") == 0) {
            GodotObjectWrapper* pw = (GodotObjectWrapper*)lua_touserdata(L,3);
            if (pw) weld_w->set_part0(gow_node(pw));
            return 0;
        }
        if (strcmp(key,"Part1") == 0) {
            GodotObjectWrapper* pw = (GodotObjectWrapper*)lua_touserdata(L,3);
            if (pw) weld_w->set_part1(gow_node(pw));
            return 0;
        }
        if (strcmp(key,"Enabled") == 0) { weld_w->set_enabled(lua_toboolean(L,3)!=0); return 0; }
    }
    HingeConstraint* hinge_w = Object::cast_to<HingeConstraint>(n);
    if (hinge_w) {
        if (strcmp(key,"Enabled")             == 0) { hinge_w->set_enabled(lua_toboolean(L,3)!=0);               return 0; }
        if (strcmp(key,"LimitsEnabled")       == 0) { hinge_w->set_limits_enabled(lua_toboolean(L,3)!=0);        return 0; }
        if (strcmp(key,"UpperAngle")          == 0) { hinge_w->set_upper_angle((float)luaL_checknumber(L,3));    return 0; }
        if (strcmp(key,"LowerAngle")          == 0) { hinge_w->set_lower_angle((float)luaL_checknumber(L,3));    return 0; }
        if (strcmp(key,"MotorEnabled")        == 0) { hinge_w->set_motor_enabled(lua_toboolean(L,3)!=0);         return 0; }
        if (strcmp(key,"MotorMaxTorque")      == 0) { hinge_w->set_motor_max_torque((float)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key,"MotorTargetVelocity") == 0) { hinge_w->set_motor_target_velocity((float)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key,"Part0") == 0) { GodotObjectWrapper* pw=(GodotObjectWrapper*)lua_touserdata(L,3); if(pw) hinge_w->set_part0(gow_node(pw)); return 0; }
        if (strcmp(key,"Part1") == 0) { GodotObjectWrapper* pw=(GodotObjectWrapper*)lua_touserdata(L,3); if(pw) hinge_w->set_part1(gow_node(pw)); return 0; }
    }
    RodConstraint* rod_w = Object::cast_to<RodConstraint>(n);
    if (rod_w) {
        if (strcmp(key,"Enabled") == 0) { rod_w->set_enabled(lua_toboolean(L,3)!=0);          return 0; }
        if (strcmp(key,"Length")  == 0) { rod_w->set_length((float)luaL_checknumber(L,3));    return 0; }
        if (strcmp(key,"Part0")   == 0) { GodotObjectWrapper* pw=(GodotObjectWrapper*)lua_touserdata(L,3); if(pw) rod_w->set_part0(gow_node(pw)); return 0; }
        if (strcmp(key,"Part1")   == 0) { GodotObjectWrapper* pw=(GodotObjectWrapper*)lua_touserdata(L,3); if(pw) rod_w->set_part1(gow_node(pw)); return 0; }
    }
    SpringConstraint* spring_w = Object::cast_to<SpringConstraint>(n);
    if (spring_w) {
        if (strcmp(key,"Enabled")    == 0) { spring_w->set_enabled(lua_toboolean(L,3)!=0);            return 0; }
        if (strcmp(key,"FreeLength") == 0) { spring_w->set_free_length((float)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key,"Stiffness")  == 0) { spring_w->set_stiffness((float)luaL_checknumber(L,3));   return 0; }
        if (strcmp(key,"Damping")    == 0) { spring_w->set_damping((float)luaL_checknumber(L,3));     return 0; }
        if (strcmp(key,"Part0") == 0) { GodotObjectWrapper* pw=(GodotObjectWrapper*)lua_touserdata(L,3); if(pw) spring_w->set_part0(gow_node(pw)); return 0; }
        if (strcmp(key,"Part1") == 0) { GodotObjectWrapper* pw=(GodotObjectWrapper*)lua_touserdata(L,3); if(pw) spring_w->set_part1(gow_node(pw)); return 0; }
    }

    // ── GUI — newindex ─────────────────────────────────────────────
    // Helper: leer UDim2 de tabla Lua {X={Scale,Offset}, Y={Scale,Offset}}
    auto read_udim2_lua = [&]() -> GuiUDim2 {
        GuiUDim2 d;
        if (!lua_istable(L, 3)) return d;
        lua_getfield(L, 3, "X");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "Scale");  d.xs = lua_isnumber(L,-1)?(float)lua_tonumber(L,-1):0; lua_pop(L,1);
            lua_getfield(L, -1, "Offset"); d.xo = lua_isnumber(L,-1)?(float)lua_tonumber(L,-1):0; lua_pop(L,1);
        } lua_pop(L,1);
        lua_getfield(L, 3, "Y");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "Scale");  d.ys = lua_isnumber(L,-1)?(float)lua_tonumber(L,-1):0; lua_pop(L,1);
            lua_getfield(L, -1, "Offset"); d.yo = lua_isnumber(L,-1)?(float)lua_tonumber(L,-1):0; lua_pop(L,1);
        } lua_pop(L,1);
        return d;
    };

    ScreenGui* sg_w = Object::cast_to<ScreenGui>(n);
    if (sg_w) {
        if (strcmp(key,"Enabled")       == 0) { sg_w->set_sg_enabled(lua_toboolean(L,3)!=0);          return 0; }
        if (strcmp(key,"DisplayOrder")  == 0) { sg_w->set_display_order((int)luaL_checknumber(L,3));   return 0; }
        if (strcmp(key,"ResetOnSpawn")  == 0) { sg_w->set_reset_on_spawn(lua_toboolean(L,3)!=0);       return 0; }
    }

    // Aplicar propiedades comunes de GUI para los tipos de GUI
    #define GUI_NEWINDEX_COMMON(cast_type, var_name) \
        cast_type* var_name = Object::cast_to<cast_type>(n); \
        if (var_name) { \
            if (strcmp(key,"Visible")   == 0) { var_name->set_visible(lua_toboolean(L,3)!=0); return 0; } \
            if (strcmp(key,"ZIndex")    == 0) { var_name->set_z_index((int)luaL_checknumber(L,3)); return 0; } \
            if (strcmp(key,"Size")      == 0) { GuiUDim2 d=read_udim2_lua(); var_name->set_udim2_size(d.xs,d.xo,d.ys,d.yo); return 0; } \
            if (strcmp(key,"Position")  == 0) { GuiUDim2 d=read_udim2_lua(); var_name->set_udim2_pos(d.xs,d.xo,d.ys,d.yo);  return 0; } \
            if (strcmp(key,"AnchorPoint")==0 && lua_istable(L,3)) { \
                lua_getfield(L,3,"X"); float ax = lua_isnumber(L,-1)?(float)lua_tonumber(L,-1):0; lua_pop(L,1); \
                lua_getfield(L,3,"Y"); float ay = lua_isnumber(L,-1)?(float)lua_tonumber(L,-1):0; lua_pop(L,1); \
                var_name->set_anchor_point(ax,ay); return 0; \
            } \
            if (strcmp(key,"BackgroundColor3")==0) { LuauColor3* c=check_color3(L, 3); if(c) var_name->set_bg_color(c->r,c->g,c->b); return 0; } \
            if (strcmp(key,"BackgroundTransparency")==0) { var_name->set_bg_alpha((float)luaL_checknumber(L,3)); return 0; } \
            if (strcmp(key,"BorderColor3")==0) { LuauColor3* c=check_color3(L, 3); if(c) var_name->set_border_color(c->r,c->g,c->b); return 0; } \
            if (strcmp(key,"BorderSizePixel")==0) { var_name->set_border_px((int)luaL_checknumber(L,3)); return 0; } \
        }
    GUI_NEWINDEX_COMMON(RobloxFrame, rframe_w)
    GUI_NEWINDEX_COMMON(RobloxTextLabel, rtlabel_w)
    if (rtlabel_w) {
        if (strcmp(key,"Text")==0)        { rtlabel_w->set_text(String(luaL_checkstring(L,3))); return 0; }
        if (strcmp(key,"TextColor3")==0)  { LuauColor3* c=check_color3(L, 3); if(c) rtlabel_w->set_text_color(c->r,c->g,c->b); return 0; }
        if (strcmp(key,"TextSize")==0)    { rtlabel_w->set_text_size((int)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key,"TextScaled")==0)  { rtlabel_w->set_text_scaled(lua_toboolean(L,3)!=0); return 0; }
        if (strcmp(key,"TextXAlignment")==0) {
            int align = (int)luaL_checknumber(L,3);
            rtlabel_w->set_horizontal_alignment(align==0?HORIZONTAL_ALIGNMENT_LEFT:align==1?HORIZONTAL_ALIGNMENT_CENTER:HORIZONTAL_ALIGNMENT_RIGHT);
            return 0;
        }
        if (strcmp(key,"TextYAlignment")==0) {
            int align = (int)luaL_checknumber(L,3);
            rtlabel_w->set_vertical_alignment(align==0?VERTICAL_ALIGNMENT_TOP:align==1?VERTICAL_ALIGNMENT_CENTER:VERTICAL_ALIGNMENT_BOTTOM);
            return 0;
        }
    }
    GUI_NEWINDEX_COMMON(RobloxTextButton, rtbutton_w)
    if (rtbutton_w) {
        if (strcmp(key,"Text")==0)       { rtbutton_w->set_text(String(luaL_checkstring(L,3)));  return 0; }
        if (strcmp(key,"TextColor3")==0) { LuauColor3* c=check_color3(L, 3); if(c) rtbutton_w->set_text_color(c->r,c->g,c->b); return 0; }
        if (strcmp(key,"TextSize")==0)   { rtbutton_w->set_text_size((int)luaL_checknumber(L,3)); return 0; }
    }
    GUI_NEWINDEX_COMMON(RobloxTextBox, rtbox_w)
    if (rtbox_w) {
        if (strcmp(key,"Text")==0)           { rtbox_w->set_text(String(luaL_checkstring(L,3)));             return 0; }
        if (strcmp(key,"PlaceholderText")==0){ rtbox_w->set_placeholder(String(luaL_checkstring(L,3))); return 0; }
        if (strcmp(key,"ClearTextOnFocus")==0){ rtbox_w->set_select_all_on_focus(lua_toboolean(L,3)!=0);     return 0; }
    }
    GUI_NEWINDEX_COMMON(RobloxImageLabel, rimage_w)
    if (rimage_w) {
        if (strcmp(key,"Image")==0)            { rimage_w->set_image(String(luaL_checkstring(L,3)));        return 0; }
        if (strcmp(key,"ImageColor3")==0)      { LuauColor3* c=check_color3(L, 3); if(c) rimage_w->set_image_color(c->r,c->g,c->b); return 0; }
        if (strcmp(key,"ImageTransparency")==0){ rimage_w->set_image_transparency((float)luaL_checknumber(L,3)); return 0; }
    }
    GUI_NEWINDEX_COMMON(RobloxScrollingFrame, rscroll_w)
    if (rscroll_w) {
        if (strcmp(key,"ScrollingEnabled")==0){ rscroll_w->set_scrolling_enabled_val(lua_toboolean(L,3)!=0); return 0; }
    }
    #undef GUI_NEWINDEX_COMMON

    RemoteFunctionNode* rfall = Object::cast_to<RemoteFunctionNode>(n);
    if (rfall && lua_isfunction(L, 3)) {
        if (strcmp(key, "OnServerInvoke") == 0) {
            lua_pushvalue(L, 3);
            int ref = lua_ref(L, -1);
            lua_pop(L, 1);
            rfall->set_server_invoke(L, ref);
            return 0;
        }
        if (strcmp(key, "OnClientInvoke") == 0) {
            lua_pushvalue(L, 3);
            int ref = lua_ref(L, -1);
            lua_pop(L, 1);
            rfall->set_client_invoke(L, ref);
            return 0;
        }
    }

    // ── RobloxSound newindex ──────────────────────────────────────────
    RobloxSound* rsound_w = Object::cast_to<RobloxSound>(n);
    if (rsound_w) {
        if (strcmp(key,"SoundId")  == 0) { rsound_w->set_sound_id(String(luaL_checkstring(L,3)));        return 0; }
        if (strcmp(key,"Volume")   == 0) { rsound_w->set_volume((float)luaL_checknumber(L,3));           return 0; }
        if (strcmp(key,"Pitch")    == 0) { rsound_w->set_pitch((float)luaL_checknumber(L,3));            return 0; }
        if (strcmp(key,"PlaybackSpeed")==0){ rsound_w->set_pitch((float)luaL_checknumber(L,3));          return 0; }
        if (strcmp(key,"Looped")   == 0) { rsound_w->set_looped(lua_toboolean(L,3)!=0);                  return 0; }
        if (strcmp(key,"TimePosition")==0){ rsound_w->set_time_position((float)luaL_checknumber(L,3));   return 0; }
        if (strcmp(key,"RollOffMode")==0){ rsound_w->set_rolloff_mode((int)luaL_checknumber(L,3));       return 0; }
        if (strcmp(key,"RollOffMinDistance")==0){ rsound_w->set_rolloff_min((float)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key,"RollOffMaxDistance")==0){ rsound_w->set_rolloff_max((float)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key,"EmitterSize")==0){ rsound_w->set_emitter_size((float)luaL_checknumber(L,3));     return 0; }
        if (strcmp(key,"SoundGroup")== 0){ rsound_w->set_sound_group(String(luaL_checkstring(L,3)));     return 0; }
    }

    // ── ClickDetector newindex ─────────────────────────────────────────
    ClickDetector* cd_w = Object::cast_to<ClickDetector>(n);
    if (cd_w) {
        if (strcmp(key,"MaxActivationDistance")==0){ cd_w->set_max_distance((float)luaL_checknumber(L,3)); return 0; }
    }

    // ── ProximityPrompt newindex ───────────────────────────────────────
    ProximityPrompt* pp_w = Object::cast_to<ProximityPrompt>(n);
    if (pp_w) {
        if (strcmp(key,"ActionText")            == 0) { pp_w->set_action_text(String(luaL_checkstring(L,3)));  return 0; }
        if (strcmp(key,"KeyboardKeyCode")       == 0) { pp_w->set_key_code(String(luaL_checkstring(L,3)));     return 0; }
        if (strcmp(key,"MaxActivationDistance") == 0) { pp_w->set_max_distance((float)luaL_checknumber(L,3));  return 0; }
        if (strcmp(key,"HoldDuration")          == 0) { pp_w->set_hold_duration((float)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key,"Enabled")               == 0) { pp_w->set_enabled(lua_toboolean(L,3)!=0);              return 0; }
    }

    // ── SpawnLocation newindex ─────────────────────────────────────────
    SpawnLocation* spwn_w = Object::cast_to<SpawnLocation>(n);
    if (spwn_w) {
        if (strcmp(key,"Enabled")  == 0) { spwn_w->set_enabled(lua_toboolean(L,3)!=0);           return 0; }
        if (strcmp(key,"Duration") == 0) { spwn_w->set_duration((float)luaL_checknumber(L,3));    return 0; }
        if (strcmp(key,"Neutral")  == 0) { spwn_w->set_neutral(lua_toboolean(L,3)!=0);            return 0; }
    }

    // ── BillboardGui newindex ─────────────────────────────────────────
    BillboardGui* bbgui_w = Object::cast_to<BillboardGui>(n);
    if (bbgui_w) {
        if (strcmp(key,"Enabled")     == 0) { bbgui_w->set_enabled(lua_toboolean(L,3)!=0);           return 0; }
        if (strcmp(key,"AlwaysOnTop") == 0) { bbgui_w->set_always_on_top(lua_toboolean(L,3)!=0);     return 0; }
        if (strcmp(key,"MaxDistance") == 0) { bbgui_w->set_max_distance((float)luaL_checknumber(L,3)); return 0; }
    }

    // ── Motor6D newindex ──────────────────────────────────────────────
    Motor6D* m6d_w = Object::cast_to<Motor6D>(n);
    if (m6d_w) {
        if (strcmp(key,"DesiredAngle") == 0) { m6d_w->set_desired_angle((float)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key,"MaxVelocity")  == 0) { m6d_w->set_max_velocity((float)luaL_checknumber(L,3));  return 0; }
        if (strcmp(key,"Enabled")      == 0) { m6d_w->set_enabled(lua_toboolean(L,3)!=0);               return 0; }
        if (strcmp(key,"Part0") == 0) {
            GodotObjectWrapper* wr = (GodotObjectWrapper*)lua_touserdata(L,3);
            if (wr && gow_node(wr)) m6d_w->set_part0(gow_node(wr));
            return 0;
        }
        if (strcmp(key,"Part1") == 0) {
            GodotObjectWrapper* wr = (GodotObjectWrapper*)lua_touserdata(L,3);
            if (wr && gow_node(wr)) m6d_w->set_part1(gow_node(wr));
            return 0;
        }
        if (strcmp(key,"C0") == 0 && lua_istable(L,3)) {
            lua_getfield(L,3,"X"); float tx=(float)lua_tonumber(L,-1);lua_pop(L,1);
            lua_getfield(L,3,"Y"); float ty=(float)lua_tonumber(L,-1);lua_pop(L,1);
            lua_getfield(L,3,"Z"); float tz=(float)lua_tonumber(L,-1);lua_pop(L,1);
            lua_getfield(L,3,"m00");float m00=(float)lua_tonumber(L,-1);lua_pop(L,1);
            lua_getfield(L,3,"m01");float m01=(float)lua_tonumber(L,-1);lua_pop(L,1);
            lua_getfield(L,3,"m02");float m02=(float)lua_tonumber(L,-1);lua_pop(L,1);
            lua_getfield(L,3,"m10");float m10=(float)lua_tonumber(L,-1);lua_pop(L,1);
            lua_getfield(L,3,"m11");float m11=(float)lua_tonumber(L,-1);lua_pop(L,1);
            lua_getfield(L,3,"m12");float m12=(float)lua_tonumber(L,-1);lua_pop(L,1);
            lua_getfield(L,3,"m20");float m20=(float)lua_tonumber(L,-1);lua_pop(L,1);
            lua_getfield(L,3,"m21");float m21=(float)lua_tonumber(L,-1);lua_pop(L,1);
            lua_getfield(L,3,"m22");float m22=(float)lua_tonumber(L,-1);lua_pop(L,1);
            Transform3D t; t.origin=Vector3(tx,ty,tz);
            t.basis.rows[0]=Vector3(m00,m01,m02); t.basis.rows[1]=Vector3(m10,m11,m12); t.basis.rows[2]=Vector3(m20,m21,m22);
            m6d_w->set_c0(t); return 0;
        }
    }

    // ── RobloxTool newindex ───────────────────────────────────────────
    RobloxTool* rtool_w = Object::cast_to<RobloxTool>(n);
    if (rtool_w) {
        if (strcmp(key,"ToolTip")      == 0) { rtool_w->set_tool_tip(String(luaL_checkstring(L,3)));  return 0; }
        if (strcmp(key,"CanBeDropped") == 0) { rtool_w->set_can_be_dropped(lua_toboolean(L,3)!=0);    return 0; }
    }

    // ── AnimationTrack newindex ───────────────────────────────────────
    AnimationTrack* atrack_w = Object::cast_to<AnimationTrack>(n);
    if (atrack_w) {
        if (strcmp(key,"Looped") == 0) { atrack_w->set_looped(lua_toboolean(L,3)!=0);           return 0; }
        if (strcmp(key,"Speed")  == 0) { atrack_w->set_speed((float)luaL_checknumber(L,3));      return 0; }
    }

    // ── AnimationObject newindex ──────────────────────────────────────
    AnimationObject* aobj_w = Object::cast_to<AnimationObject>(n);
    if (aobj_w) {
        if (strcmp(key,"AnimationId") == 0) { aobj_w->set_animation_id(String(luaL_checkstring(L,3))); return 0; }
    }

    // ── Puente genérico de escritura (solo clases nuevas con meta _gl_bridge) ──
    if (n->has_meta("_gl_bridge")) {
        Variant gv;
        if (gl_luau_to_variant(L, 3, gv)) {
            if (gl_has_property(n, String(key))) n->set(String(key), gv);
            else n->set_meta(String("_glp_") + String(key), gv);
        } else if (lua_istable(L, 3)) {
            // UDim / UDim2 → usar Offset (p.ej. CornerRadius, Padding numéricos)
            lua_getfield(L, 3, "Offset");
            if (lua_isnumber(L, -1)) {
                int off = (int)lua_tonumber(L, -1);
                if (gl_has_property(n, String(key))) n->set(String(key), off);
                else n->set_meta(String("_glp_") + String(key), off);
            }
            lua_pop(L, 1);
        }
    }
    return 0;
}

// ════════════════════════════════════════════════════════════════════
//  Replicación automática (1.14.5) — lado SERVIDOR + aplicación en cliente
//  Engancha en godot_object_newindex (propiedad / Parent) y method_destroy para
//  difundir deltas del mundo a los clientes (NetworkService::rep_* en
//  roblox_network.h). Coste cero fuera de un servidor de red: la guarda
//  gl_net_role()==1 corta al instante en single-player/cliente.
// ════════════════════════════════════════════════════════════════════
// GL_REP_SHADOW vive en gl_runtime.h: lo comparten este hook y el NetworkService.

static NetworkService* gl_rep_service() {
    if (gl_net_role() != 1) return nullptr;
    return Object::cast_to<NetworkService>(ObjectDB::get_instance(gl_net_service_id()));
}

// ¿el nodo cuelga de un contenedor replicable (Workspace / ReplicatedStorage)?
// Sube por los padres comparando por NOMBRE: los contenedores virtuales pueden
// ser Node genérico, así que is_class no siempre basta.
static bool gl_rep_is_replicable(Node* n) {
    for (Node* p = n ? n->get_parent() : nullptr; p; p = p->get_parent()) {
        StringName nm = p->get_name();
        if (nm == StringName("ServerStorage") || nm == StringName("ServerScriptService")) return false;
        // Teams (1.14.10): en Roblox los equipos se replican, así que un Team
        // creado por el servidor tiene que aparecer solo en los clientes.
        if (nm == StringName("Workspace") || nm == StringName("ReplicatedStorage") || nm == StringName("Teams")) return true;
        if (p->is_class("RobloxWorkspace") || p->is_class("RobloxWorkspace2D")) return true;
    }
    return false;
}

// Ref del padre para el protocolo: "#netid" si el padre ya está replicado, o su
// ruta absoluta si es un contenedor (Workspace/ReplicatedStorage ya existen en
// cada ventana por el place cargado).
static String gl_rep_parent_ref(Node* parent) {
    if (parent && parent->has_meta("_gl_netid"))
        return String("#") + String::num_int64((int64_t)parent->get_meta("_gl_netid"));
    if (parent) return String(parent->get_path());
    return String();
}

// Replica el nodo y su subárbol como creaciones nuevas (asigna netId + snapshot
// vía NetworkService::rep_build_props, que resuelve el transform global).
static void gl_rep_replicate_subtree(NetworkService* ns, Node* n) {
    if (!ns || !n || n->has_meta("_gl_netid")) return;
    int64_t id = ns->rep_next_id();
    n->set_meta("_gl_netid", id);
    ns->rep_map(id, n);
    ns->rep_send_new(id, n->get_class(), String(n->get_name()), gl_rep_parent_ref(n->get_parent()), ns->rep_build_props(n));
    for (int i = 0; i < n->get_child_count(); i++)
        gl_rep_replicate_subtree(ns, n->get_child(i));
}

// Hook tras aplicar una escritura. Pila: [1]=obj [2]=key [3]=valor.
static void gl_rep_on_newindex(lua_State* L) {
    NetworkService* ns = gl_rep_service();
    if (!ns) return;
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    Node* n = w ? gow_node(w) : nullptr;
    if (!n || !lua_isstring(L, 2)) return;
    String key = String(lua_tostring(L, 2));
    if (key == "Parent") {
        if (gl_rep_is_replicable(n)) {
            if (n->has_meta("_gl_netid"))
                ns->rep_send_reparent((int64_t)n->get_meta("_gl_netid"), gl_rep_parent_ref(n->get_parent()));   // ya replicado → mover
            else
                gl_rep_replicate_subtree(ns, n);                                                                // entra al árbol replicado → crear
        } else if (n->has_meta("_gl_netid")) {
            ns->rep_unreplicate(n);   // salió del árbol replicado → clientes destruyen su copia + olvida el subárbol (el nodo del servidor sigue vivo)
        }
        return;
    }
    // El valor (idx 3) puede ser userdata (Vector3/Color3) o tabla tipada (CFrame,
    // UDim2…): gl_net_encode captura ambos como Variant transmisible.
    Variant enc = gl_net_encode(L, 3);
    n->set_meta(StringName(String(GL_REP_SHADOW) + key), enc);   // shadow para el snapshot al parentar
    if (n->has_meta("_gl_netid"))
        ns->rep_send_prop((int64_t)n->get_meta("_gl_netid"), key, enc);
}

// Aplica una propiedad replicada en el CLIENTE reentrando al __newindex real.
// Instalado en gl_apply_prop_hook(). Hilo Luau fresco: el impl lee índices 1/2/3.
static void gl_rep_apply_prop(Node* target, const String& key, const Variant& enc) {
    if (!target) return;
    lua_State* L = gl_any_state();
    if (!L) return;
    lua_State* th = lua_newthread(L);
    wrap_node(th, target);                          // idx 1
    lua_pushstring(th, key.utf8().get_data());      // idx 2
    gl_net_decode(th, enc, target);                 // idx 3
    godot_object_newindex_impl(th);                 // aplica SIN volver a replicar
    lua_pop(L, 1);                                  // quita el hilo
}

// __newindex real: aplica y (si somos servidor de red) difunde el delta.
static int godot_object_newindex(lua_State* L) {
    int r = godot_object_newindex_impl(L);
    gl_rep_on_newindex(L);
    return r;
}

// ════════════════════════════════════════════════════════════════════
//  Instance.new
// ════════════════════════════════════════════════════════════════════
static int godot_instance_new(lua_State* L) {
    const char* cls = luaL_checkstring(L, 1);
    Node* nn = nullptr;

    // Primary Roblox class names → Godot nodes
    if      (strcmp(cls, "Part")              == 0 ||
             strcmp(cls, "BasePart")          == 0 ||
             strcmp(cls, "MeshPart")          == 0 ||
             strcmp(cls, "WedgePart")         == 0 ||
             strcmp(cls, "CornerWedgePart")   == 0 ||
             strcmp(cls, "TrussPart")         == 0 ||
             strcmp(cls, "UnionOperation")    == 0) nn = memnew(RobloxPart);
    else if (strcmp(cls, "Folder")            == 0 ||
             strcmp(cls, "Configuration")     == 0) nn = memnew(Folder);
    else if (strcmp(cls, "StringValue")       == 0 ||
             strcmp(cls, "IntValue")          == 0 ||
             strcmp(cls, "NumberValue")       == 0 ||
             strcmp(cls, "BoolValue")         == 0 ||
             strcmp(cls, "ObjectValue")       == 0 ||
             strcmp(cls, "Vector3Value")      == 0 ||
             strcmp(cls, "Color3Value")       == 0 ||
             strcmp(cls, "CFrameValue")       == 0) {
        RobloxValue* rv = memnew(RobloxValue);
        rv->roblox_class = String(cls);
        // Valor por defecto tipado (como Roblox): número 0, texto "", bool false.
        if (strcmp(cls,"NumberValue")==0 || strcmp(cls,"IntValue")==0) rv->set_value((double)0);
        else if (strcmp(cls,"StringValue")==0) rv->set_value(String(""));
        else if (strcmp(cls,"BoolValue")==0)   rv->set_value(false);
        nn = rv;
    }
    else if (strcmp(cls, "Model")             == 0) nn = memnew(Node3D);
    else if (strcmp(cls, "Humanoid")          == 0) nn = memnew(Humanoid);
    else if (strcmp(cls, "Humanoid2D")        == 0) nn = memnew(Humanoid2D);
    else if (strcmp(cls, "PointLight")        == 0 ||
             strcmp(cls, "SurfaceLight")      == 0) nn = memnew(OmniLight3D);
    else if (strcmp(cls, "SpotLight")         == 0) nn = memnew(SpotLight3D);
    else if (strcmp(cls, "DirectionalLight")  == 0) nn = memnew(DirectionalLight3D);
    else if (strcmp(cls, "RemoteEvent")       == 0) nn = memnew(RemoteEventNode);
    else if (strcmp(cls, "RemoteFunction")    == 0) nn = memnew(RemoteFunctionNode);
    else if (strcmp(cls, "BindableEvent")     == 0 ||
             strcmp(cls, "BindableFunction")  == 0) nn = memnew(BindableEventNode);

    if (nn) {
        nn->set_name(cls);
        wrap_node(L, nn);

        if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) {
            GodotObjectWrapper* p_wrap = (GodotObjectWrapper*)lua_touserdata(L, 2);
            if (p_wrap && gow_node(p_wrap) && gow_node(p_wrap) != nn) {
                gow_node(p_wrap)->add_child(nn);
            }
        }
        return 1;
    }

    // Clase no mapeada aquí: devolvemos nil SIN avisar. La capa Lua de stdlib
    // intenta luego _InstanceNew con el mapeo completo y, si tampoco existe,
    // emite un único warning. (Antes esto ensuciaba la consola con clases válidas.)
    lua_pushnil(L);
    return 1;
}

// ════════════════════════════════════════════════════════════════════
//  Vector3
// ════════════════════════════════════════════════════════════════════

static int godot_vector3_new(lua_State* L) {
    float x = (float)luaL_optnumber(L, 1, 0.0);
    float y = (float)luaL_optnumber(L, 2, 0.0);
    float z = (float)luaL_optnumber(L, 3, 0.0);
    push_vector3(L, x, y, z);
    return 1;
}

static int godot_vector3_index(lua_State* L) {
    LuauVector3* vec = check_vector3(L, 1);
    const char* key  = luaL_checkstring(L, 2);
    if (!vec) { lua_pushnil(L); return 1; }

    if (strcmp(key, "X") == 0 || strcmp(key, "x") == 0) { lua_pushnumber(L, vec->x); return 1; }
    if (strcmp(key, "Y") == 0 || strcmp(key, "y") == 0) { lua_pushnumber(L, vec->y); return 1; }
    if (strcmp(key, "Z") == 0 || strcmp(key, "z") == 0) { lua_pushnumber(L, vec->z); return 1; }

    if (strcmp(key, "Magnitude") == 0) { lua_pushnumber(L, vec->magnitude()); return 1; }

    if (strcmp(key, "Unit") == 0) {
        LuauVector3 u = vec->unit();
        push_vector3(L, u.x, u.y, u.z);
        return 1;
    }

    if (strcmp(key, "Lerp") == 0) {
        lua_pushlightuserdata(L, vec);
        lua_pushcclosure(L, [](lua_State* pL) -> int {
            LuauVector3* self = (LuauVector3*)lua_touserdata(pL, lua_upvalueindex(1));
            LuauVector3* goal = check_vector3(pL, 1);
            float alpha = (float)luaL_checknumber(pL, 2);
            if (!self || !goal) { lua_pushnil(pL); return 1; }
            LuauVector3 r = self->lerp(*goal, alpha);
            push_vector3(pL, r.x, r.y, r.z);
            return 1;
        }, "Lerp", 1);
        return 1;
    }

    if (strcmp(key, "Dot") == 0) {
        lua_pushlightuserdata(L, vec);
        lua_pushcclosure(L, [](lua_State* pL) -> int {
            LuauVector3* self = (LuauVector3*)lua_touserdata(pL, lua_upvalueindex(1));
            LuauVector3* other = check_vector3(pL, 1);
            if (!self || !other) { lua_pushnumber(pL, 0); return 1; }
            lua_pushnumber(pL, self->dot(*other));
            return 1;
        }, "Dot", 1);
        return 1;
    }

    if (strcmp(key, "Cross") == 0) {
        lua_pushlightuserdata(L, vec);
        lua_pushcclosure(L, [](lua_State* pL) -> int {
            LuauVector3* self = (LuauVector3*)lua_touserdata(pL, lua_upvalueindex(1));
            LuauVector3* other = check_vector3(pL, 1);
            if (!self || !other) { lua_pushnil(pL); return 1; }
            LuauVector3 c = self->cross(*other);
            push_vector3(pL, c.x, c.y, c.z);
            return 1;
        }, "Cross", 1);
        return 1;
    }

    lua_pushnil(L); return 1;
}

static int godot_vector3_add(lua_State* L) {
    LuauVector3* a = check_vector3(L, 1);
    LuauVector3* b = check_vector3(L, 2);
    if (a && b) { push_vector3(L, a->x+b->x, a->y+b->y, a->z+b->z); return 1; }
    lua_pushnil(L); return 1;
}

static int godot_vector3_sub(lua_State* L) {
    LuauVector3* a = check_vector3(L, 1);
    LuauVector3* b = check_vector3(L, 2);
    if (a && b) { push_vector3(L, a->x-b->x, a->y-b->y, a->z-b->z); return 1; }
    lua_pushnil(L); return 1;
}

static int godot_vector3_mul(lua_State* L) {
    if (lua_isnumber(L, 1)) {
        float s = (float)lua_tonumber(L, 1);
        LuauVector3* v = check_vector3(L, 2);
        if (v) { push_vector3(L, v->x*s, v->y*s, v->z*s); return 1; }
    } else {
        LuauVector3* v = check_vector3(L, 1);
        if (lua_isnumber(L, 2)) {
            float s = (float)lua_tonumber(L, 2);
            if (v) { push_vector3(L, v->x*s, v->y*s, v->z*s); return 1; }
        } else {
            LuauVector3* b = check_vector3(L, 2);
            if (v && b) { push_vector3(L, v->x*b->x, v->y*b->y, v->z*b->z); return 1; }
        }
    }
    lua_pushnil(L); return 1;
}

static int godot_vector3_unm(lua_State* L) {
    LuauVector3* v = check_vector3(L, 1);
    if (v) { push_vector3(L, -v->x, -v->y, -v->z); return 1; }
    lua_pushnil(L); return 1;
}

// Vector3 / number  ó  Vector3 / Vector3 (componente a componente), como Roblox
static int godot_vector3_div(lua_State* L) {
    LuauVector3* a = check_vector3(L, 1);
    if (a && lua_isnumber(L, 2)) {
        float s = (float)lua_tonumber(L, 2);
        push_vector3(L, a->x/s, a->y/s, a->z/s); return 1;
    }
    LuauVector3* b = check_vector3(L, 2);
    if (a && b) { push_vector3(L, a->x/b->x, a->y/b->y, a->z/b->z); return 1; }
    lua_pushnil(L); return 1;
}

// Vector3 == Vector3 por valor (sin esto, == comparaba identidad de userdata)
static int godot_vector3_eq(lua_State* L) {
    LuauVector3* a = check_vector3(L, 1);
    LuauVector3* b = check_vector3(L, 2);
    lua_pushboolean(L, (a && b && a->x == b->x && a->y == b->y && a->z == b->z) ? 1 : 0);
    return 1;
}

static int godot_vector3_tostring(lua_State* L) {
    LuauVector3* v = check_vector3(L, 1);
    if (v) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f, %.3f, %.3f", v->x, v->y, v->z);
        lua_pushstring(L, buf);
        return 1;
    }
    lua_pushstring(L, "Vector3(nil)"); return 1;
}

// ════════════════════════════════════════════════════════════════════
//  Color3
// ════════════════════════════════════════════════════════════════════

static int godot_color3_new(lua_State* L) {
    float r = (float)luaL_optnumber(L, 1, 0.0);
    float g = (float)luaL_optnumber(L, 2, 0.0);
    float b = (float)luaL_optnumber(L, 3, 0.0);
    push_color3(L, r, g, b);
    return 1;
}

static int godot_color3_from_rgb(lua_State* L) {
    float r = (float)luaL_optnumber(L, 1, 0.0) / 255.0f;
    float g = (float)luaL_optnumber(L, 2, 0.0) / 255.0f;
    float b = (float)luaL_optnumber(L, 3, 0.0) / 255.0f;
    push_color3(L, r, g, b);
    return 1;
}

static int godot_color3_index(lua_State* L) {
    LuauColor3* col = check_color3(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (!col) { lua_pushnil(L); return 1; }
    if (strcmp(key, "R") == 0 || strcmp(key, "r") == 0) { lua_pushnumber(L, col->r); return 1; }
    if (strcmp(key, "G") == 0 || strcmp(key, "g") == 0) { lua_pushnumber(L, col->g); return 1; }
    if (strcmp(key, "B") == 0 || strcmp(key, "b") == 0) { lua_pushnumber(L, col->b); return 1; }
    lua_pushnil(L); return 1;
}

#endif // LUAU_API_H