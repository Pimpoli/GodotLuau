#ifndef LUAU_API_H
#define LUAU_API_H

#include "folder.h"
#include "humanoid.h"
#include "humanoid2d.h"
#include "roblox_player.h"
#include "roblox_player2d.h"
#include "roblox_part.h"
#include "roblox_services.h"
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

#include "lua.h"
#include "lualib.h"
#include "Luau/Compiler.h"

#include <cmath>
#include <cstring>
#include <functional>

using namespace godot;

// ════════════════════════════════════════════════════════════════════
//  Data structures for Luau types
////
//  Estructuras de datos para los tipos Luau
// ════════════════════════════════════════════════════════════════════

struct GodotObjectWrapper { Node* node_ptr; };

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
    wrap->node_ptr = node;
    luaL_getmetatable(L, "GodotObject");
    lua_setmetatable(L, -2);
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

// ════════════════════════════════════════════════════════════════════
//  Instance methods (accessible from Lua)
////
//  Métodos de Instance (accesibles desde Lua)
// ════════════════════════════════════════════════════════════════════

static int method_destroy(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    if (w && w->node_ptr) w->node_ptr->queue_free();
    return 0;
}

static int method_clone(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    if (w && w->node_ptr) {
        Node* cloned = w->node_ptr->duplicate();
        if (cloned) { wrap_node(L, cloned); return 1; }
    }
    lua_pushnil(L); return 1;
}

static int method_clearallchildren(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    if (w && w->node_ptr) {
        TypedArray<Node> ch = w->node_ptr->get_children();
        for (int i = 0; i < ch.size(); i++) {
            Node* n = Object::cast_to<Node>(ch[i]);
            if (n) n->queue_free();
        }
    }
    return 0;
}

static int method_findfirstchild(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    const char* name = luaL_checkstring(L, 2);
    bool recursive  = lua_isboolean(L, 3) && lua_toboolean(L, 3);
    if (!w || !w->node_ptr) { lua_pushnil(L); return 1; }
    if (!recursive) {
        // Direct children only — Roblox default
        for (int i = 0; i < w->node_ptr->get_child_count(); i++) {
            Node* ch = w->node_ptr->get_child(i);
            if (ch && ch->get_name() == StringName(name)) { wrap_node(L, ch); return 1; }
        }
    } else {
        // Recursive — BFS through all descendants
        std::vector<Node*> queue;
        for (int i = 0; i < w->node_ptr->get_child_count(); i++) {
            Node* ch = w->node_ptr->get_child(i);
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
    if (!w || !w->node_ptr) { lua_pushnil(L); return 1; }
    Node* cur = w->node_ptr->get_parent();
    while (cur) {
        if (cur->get_name() == StringName(name)) { wrap_node(L, cur); return 1; }
        cur = cur->get_parent();
    }
    lua_pushnil(L); return 1;
}

static int method_findfirstancestorofclass(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    const char* cls = luaL_checkstring(L, 2);
    if (!w || !w->node_ptr) { lua_pushnil(L); return 1; }
    Node* cur = w->node_ptr->get_parent();
    while (cur) {
        if (cur->is_class(cls)) { wrap_node(L, cur); return 1; }
        cur = cur->get_parent();
    }
    lua_pushnil(L); return 1;
}

static int method_isancestorof(lua_State* L) {
    GodotObjectWrapper* w  = (GodotObjectWrapper*)lua_touserdata(L, 1);
    GodotObjectWrapper* w2 = (GodotObjectWrapper*)lua_touserdata(L, 2);
    if (!w || !w->node_ptr || !w2 || !w2->node_ptr) { lua_pushboolean(L, false); return 1; }
    Node* cur = w2->node_ptr->get_parent();
    while (cur) {
        if (cur == w->node_ptr) { lua_pushboolean(L, true); return 1; }
        cur = cur->get_parent();
    }
    lua_pushboolean(L, false); return 1;
}

static int method_isdescendantof(lua_State* L) {
    GodotObjectWrapper* w  = (GodotObjectWrapper*)lua_touserdata(L, 1);
    GodotObjectWrapper* w2 = (GodotObjectWrapper*)lua_touserdata(L, 2);
    if (!w || !w->node_ptr || !w2 || !w2->node_ptr) { lua_pushboolean(L, false); return 1; }
    Node* cur = w->node_ptr->get_parent();
    while (cur) {
        if (cur == w2->node_ptr) { lua_pushboolean(L, true); return 1; }
        cur = cur->get_parent();
    }
    lua_pushboolean(L, false); return 1;
}

static int method_getfullname(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    if (!w || !w->node_ptr) { lua_pushstring(L, ""); return 1; }
    // Build path by walking up the tree (stop at root or DataModel)
    std::vector<std::string> parts;
    Node* cur = w->node_ptr;
    while (cur && cur->get_parent()) {
        parts.push_back(String(cur->get_name()).utf8().get_data());
        cur = cur->get_parent();
    }
    std::string result;
    for (int i = (int)parts.size() - 1; i >= 0; i--) {
        if (!result.empty()) result += ".";
        result += parts[i];
    }
    lua_pushstring(L, result.c_str()); return 1;
}

static int method_findfirstchild_whichisa(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    const char* cls = luaL_checkstring(L, 2);
    if (!w || !w->node_ptr) { lua_pushnil(L); return 1; }
    for (int i = 0; i < w->node_ptr->get_child_count(); i++) {
        Node* ch = w->node_ptr->get_child(i);
        if (ch && ch->is_class(cls)) { wrap_node(L, ch); return 1; }
    }
    lua_pushnil(L); return 1;
}

static int method_findfirstchildofclass(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    const char* cls = luaL_checkstring(L, 2);
    if (w && w->node_ptr) {
        TypedArray<Node> ch = w->node_ptr->get_children();
        for (int i = 0; i < ch.size(); i++) {
            Node* n = Object::cast_to<Node>(ch[i]);
            if (n && n->is_class(cls)) { wrap_node(L, n); return 1; }
        }
    }
    lua_pushnil(L); return 1;
}

static int method_getchildren(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    lua_newtable(L);
    if (w && w->node_ptr) {
        TypedArray<Node> ch = w->node_ptr->get_children();
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
    if (w && w->node_ptr) {
        TypedArray<Node> all;
        std::function<void(Node*)> collect = [&](Node* n) {
            TypedArray<Node> ch = n->get_children();
            for (int i = 0; i < ch.size(); i++) {
                Node* child = Object::cast_to<Node>(ch[i]);
                if (child) { all.push_back(child); collect(child); }
            }
        };
        collect(w->node_ptr);
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
    lua_pushboolean(L, w && w->node_ptr && w->node_ptr->is_class(cls));
    return 1;
}

static int method_setattribute(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    if (w && w->node_ptr) {
        const char* key = luaL_checkstring(L, 2);
        if (lua_isnumber(L, 3)) {
            w->node_ptr->set_meta(key, lua_tonumber(L, 3));
        } else if (lua_isboolean(L, 3)) {
            w->node_ptr->set_meta(key, (bool)lua_toboolean(L, 3));
        } else {
            w->node_ptr->set_meta(key, String(lua_tostring(L, 3)));
        }
    }
    return 0;
}

static int method_getattribute(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (w && w->node_ptr && w->node_ptr->has_meta(key)) {
        Variant val = w->node_ptr->get_meta(key);
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
    if (!w || !w->node_ptr) { lua_pushnil(L); return 1; }

    // ── Client/server separation ──────────────────────────────────────────
    //// ── Separacion cliente/servidor ──────────────────────────────────────
    // ServerStorage and ServerScriptService are invisible to LocalScript,
    // just like in Roblox: the client can never access the server's storage.
    //// ServerStorage y ServerScriptService son invisibles para LocalScript,
    //// igual que en Roblox: el cliente nunca puede acceder al almacén del servidor.
    lua_getglobal(L, "__IS_CLIENT");
    const bool is_client = lua_isboolean(L, -1) && (lua_toboolean(L, -1) != 0);
    lua_pop(L, 1);
    if (is_client && (strcmp(svc_name, "ServerStorage") == 0 ||
                      strcmp(svc_name, "ServerScriptService") == 0)) {
        UtilityFunctions::print("[GodotLuau] LocalScript no puede acceder a '",
            svc_name, "' — este servicio es exclusivo del servidor.");
        lua_pushnil(L);
        return 1;
    }

    // 1. Lua pure services stored as _G["__SVC_ServiceName"]
    std::string global_key = std::string("__SVC_") + svc_name;
    lua_getglobal(L, global_key.c_str());
    if (!lua_isnil(L, -1)) return 1;
    lua_pop(L, 1);

    // 2. Direct child of game node by exact name
    for (int i = 0; i < w->node_ptr->get_child_count(); i++) {
        Node* ch = w->node_ptr->get_child(i);
        if (ch && ch->get_name() == StringName(svc_name)) { wrap_node(L, ch); return 1; }
    }

    // 3. Anywhere in subtree
    Node* svc = _find_node_by_name(w->node_ptr, svc_name);
    if (svc) { wrap_node(L, svc); return 1; }

    // 4. Search whole scene tree from root
    if (w->node_ptr->is_inside_tree()) {
        svc = _find_node_by_name((Node*)w->node_ptr->get_tree()->get_root(), svc_name);
        if (svc) { wrap_node(L, svc); return 1; }
    }

    // 5. Auto-create well-known virtual containers
    for (int i = 0; _VIRTUAL_CONTAINERS[i]; i++) {
        if (strcmp(_VIRTUAL_CONTAINERS[i], svc_name) == 0) {
            Node* c = _get_or_create_container(w->node_ptr, svc_name);
            wrap_node(L, c); return 1;
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
    if (!wrapper || !wrapper->node_ptr) { lua_pushnil(L); return 1; }
    Node* n = wrapper->node_ptr;

    if (strcmp(key, "Destroy")          == 0) { lua_pushcfunction(L, method_destroy,                   "Destroy");                return 1; }
    if (strcmp(key, "Clone")            == 0) { lua_pushcfunction(L, method_clone,                     "Clone");                  return 1; }
    if (strcmp(key, "ClearAllChildren") == 0) { lua_pushcfunction(L, method_clearallchildren,          "ClearAllChildren");       return 1; }
    if (strcmp(key, "FindFirstChild")   == 0 ||
        strcmp(key, "WaitForChild")     == 0) { lua_pushcfunction(L, method_findfirstchild,            key);                      return 1; }
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
            lua_pushvalue(pL, 2); int ref = lua_ref(pL, -1); lua_pop(pL, 1);
            // Store in node's meta attribute
            const char* attr = adding ? "__child_added_refs" : "__child_removed_refs";
            // We store in a Lua table in the registry keyed by node pointer
            lua_pushstring(pL, attr);
            lua_rawget(pL, LUA_REGISTRYINDEX);
            if (lua_isnil(pL, -1)) {
                lua_pop(pL, 1);
                lua_newtable(pL);
                lua_pushstring(pL, attr);
                lua_pushvalue(pL, -2);
                lua_rawset(pL, LUA_REGISTRYINDEX);
            }
            // key = lightuserdata(node), value = array of refs
            lua_pushlightuserdata(pL, (void*)nd);
            lua_rawget(pL, -2);
            if (lua_isnil(pL, -1)) {
                lua_pop(pL, 1);
                lua_newtable(pL);
                lua_pushlightuserdata(pL, (void*)nd);
                lua_pushvalue(pL, -2);
                lua_rawset(pL, -4);
            }
            int n2 = (int)lua_objlen(pL, -1) + 1;
            lua_pushinteger(pL, ref);
            lua_rawseti(pL, -2, n2);
            lua_pop(pL, 2);
            lua_pushnil(pL); return 1;
        }, "Connect", 2);
        lua_setfield(L, -2, "Connect");
        return 1;
    }

    // GetPropertyChangedSignal — conecta a Godot "property_list_changed" o polling per-frame
    if (strcmp(key, "GetPropertyChangedSignal") == 0) {
        lua_pushlightuserdata(L, (void*)n);
        lua_pushcclosure(L, [](lua_State* pL) -> int {
            Node* nd  = (Node*)lua_touserdata(pL, lua_upvalueindex(1));
            // const char* prop = luaL_checkstring(pL, 2); // not used yet
            lua_newtable(pL);
            lua_pushlightuserdata(pL, (void*)nd);
            lua_pushcclosure(pL, [](lua_State* LL) -> int {
                Node* nd2 = (Node*)lua_touserdata(LL, lua_upvalueindex(1));
                if (!nd2 || !lua_isfunction(LL, 2)) return 0;
                lua_pushvalue(LL, 2); int ref = lua_ref(LL, -1); lua_pop(LL, 1);
                int64_t ptr_L = (int64_t)LL;
                // Connect to Godot's generic "property_list_changed" signal
                if (nd2->has_signal("property_list_changed")) {
                    nd2->connect("property_list_changed",
                        Callable(nd2, "_on_luau_prop_changed").bind(ref, ptr_L));
                }
                return 0;
            }, "Connect", 1);
            lua_setfield(pL, -2, "Connect");
            // stub Wait/Once for now
            lua_pushcclosure(pL, [](lua_State* LL) -> int { lua_pushnil(LL); return 1; }, "Wait", 0);
            lua_setfield(pL, -2, "Wait");
            return 1;
        }, "GetPropertyChangedSignal", 1);
        return 1;
    }

    // AncestryChanged — conecta al signal "tree_exiting" / parent change (best-effort)
    if (strcmp(key, "AncestryChanged") == 0) {
        lua_newtable(L);
        lua_pushlightuserdata(L, (void*)n);
        lua_pushcclosure(L, [](lua_State* pL) -> int {
            Node* nd = (Node*)lua_touserdata(pL, lua_upvalueindex(1));
            if (!nd || !lua_isfunction(pL, 2)) return 0;
            lua_pushvalue(pL, 2); int ref = lua_ref(pL, -1); lua_pop(pL, 1);
            int64_t ptr_L = (int64_t)pL;
            // Use "tree_entered" as proxy for ancestry change
            if (nd->has_signal("tree_entered"))
                nd->connect("tree_entered", Callable(nd, "_on_luau_ancestry").bind(ref, ptr_L));
            return 0;
        }, "Connect", 1);
        lua_setfield(L, -2, "Connect");
        return 1;
    }

    // DescendantAdded / DescendantRemoving — connected to child_entered/exiting_tree recursive
    if (strcmp(key, "DescendantAdded") == 0 || strcmp(key, "DescendantRemoving") == 0) {
        bool is_added = (strcmp(key, "DescendantAdded") == 0);
        lua_newtable(L);
        lua_pushlightuserdata(L, (void*)n);
        lua_pushboolean(L, is_added ? 1 : 0);
        lua_pushcclosure(L, [](lua_State* pL) -> int {
            Node* nd = (Node*)lua_touserdata(pL, lua_upvalueindex(1));
            bool adding = lua_toboolean(pL, lua_upvalueindex(2)) != 0;
            if (!nd || !lua_isfunction(pL, 2)) return 0;
            lua_pushvalue(pL, 2); int ref = lua_ref(pL, -1); lua_pop(pL, 1);
            const char* attr = adding ? "__desc_added_refs" : "__desc_removed_refs";
            lua_pushstring(pL, attr); lua_rawget(pL, LUA_REGISTRYINDEX);
            if (lua_isnil(pL, -1)) {
                lua_pop(pL, 1); lua_newtable(pL);
                lua_pushstring(pL, attr); lua_pushvalue(pL, -2); lua_rawset(pL, LUA_REGISTRYINDEX);
            }
            lua_pushlightuserdata(pL, (void*)nd); lua_rawget(pL, -2);
            if (lua_isnil(pL, -1)) {
                lua_pop(pL, 1); lua_newtable(pL);
                lua_pushlightuserdata(pL, (void*)nd); lua_pushvalue(pL, -2); lua_rawset(pL, -4);
            }
            int n2 = (int)lua_objlen(pL, -1) + 1;
            lua_pushinteger(pL, ref); lua_rawseti(pL, -2, n2);
            lua_pop(pL, 2);
            // Connect Godot signal once
            const char* gsig = adding ? "child_entered_tree" : "child_exiting_tree";
            if (nd->has_signal(gsig) && !nd->is_connected(gsig, Callable(nd, "_on_luau_desc_change")))
                nd->connect(gsig, Callable(nd, "_on_luau_desc_change").bind(adding ? 1 : 0));
            return 0;
        }, "Connect", 2);
        lua_setfield(L, -2, "Connect");
        return 1;
    }

    // ── Propiedades de Instance ───────────────────────────────────
    if (strcmp(key, "Name")      == 0) { lua_pushstring(L, String(n->get_name()).utf8().get_data()); return 1; }
    if (strcmp(key, "ClassName") == 0) { lua_pushstring(L, String(n->get_class()).utf8().get_data()); return 1; }
    if (strcmp(key, "Parent")    == 0) { wrap_node(L, n->get_parent()); return 1; }

    // ── Players ───────────────────────────────────────────────────
    Players* players_svc = Object::cast_to<Players>(n);
    if (players_svc) {
        if (strcmp(key, "LocalPlayer") == 0) {
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
                if (!ps || !cw || !cw->node_ptr) { lua_pushnil(pL); return 1; }
                Node* character = cw->node_ptr;
                TypedArray<Node> all_players = ps->get_players();
                for (int i = 0; i < all_players.size(); i++) {
                    Node* p = Object::cast_to<Node>(all_players[i]);
                    if (!p) continue;
                    Node* ch = p->get_node_or_null("Character");
                    if (ch == character) { wrap_node(pL, p); return 1; }
                    // Also check if the player IS the character
                    if (p == character) { wrap_node(pL, p); return 1; }
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
                    RobloxPlayer* rp = Object::cast_to<RobloxPlayer>(all_players[i]);
                    if (rp && rp->get_user_id() == uid) { wrap_node(pL, rp); return 1; }
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
                return 0;
            }, "Connect", 2);
            lua_setfield(L, -2, "Connect");
            return 1;
        }
    }

    // ── TextChatService ───────────────────────────────────────────
    TextChatService* tcs = Object::cast_to<TextChatService>(n);
    if (tcs) {
        if (strcmp(key, "SendMessage") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                GodotObjectWrapper* w2 = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                TextChatService* t = w2 ? Object::cast_to<TextChatService>(w2->node_ptr) : nullptr;
                if (t) {
                    String player = luaL_checkstring(pL, 2);
                    String msg    = luaL_checkstring(pL, 3);
                    t->send_message_from_luau(player, msg);
                }
                return 0;
            }, "SendMessage");
            return 1;
        }
        if (strcmp(key, "SetPlayerName") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                GodotObjectWrapper* w2 = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                TextChatService* t = w2 ? Object::cast_to<TextChatService>(w2->node_ptr) : nullptr;
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
                if (!wr || !wr->node_ptr) { lua_pushnil(pL); return 1; }

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

                Node3D* ws3d = Object::cast_to<Node3D>(wr->node_ptr);
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
                Humanoid* h = w2 ? Object::cast_to<Humanoid>(w2->node_ptr) : nullptr;
                if (h) h->take_damage((float)luaL_checknumber(pL, 2));
                return 0;
            }, "TakeDamage");
            return 1;
        }
        if (strcmp(key, "Heal") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                GodotObjectWrapper* w2 = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                Humanoid* h = w2 ? Object::cast_to<Humanoid>(w2->node_ptr) : nullptr;
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
                Humanoid* h = w2 ? Object::cast_to<Humanoid>(w2->node_ptr) : nullptr;
                lua_pushnumber(pL, h ? h->get_state() : 1);
                return 1;
            }, "GetState");
            return 1;
        }
        if (strcmp(key, "ChangeState") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                GodotObjectWrapper* w2 = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                Humanoid* h = w2 ? Object::cast_to<Humanoid>(w2->node_ptr) : nullptr;
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
                if (aw && aw->node_ptr) {
                    AnimationObject* ao = Object::cast_to<AnimationObject>(aw->node_ptr);
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
                tw->node_ptr = track;
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
                if (pw && pw->node_ptr) {
                    Node3D* target = Object::cast_to<Node3D>(pw->node_ptr);
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
                return 0;
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
                return 0;
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
                return 0;
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
                Humanoid2D* h = w2 ? Object::cast_to<Humanoid2D>(w2->node_ptr) : nullptr;
                if (h) h->take_damage((float)luaL_checknumber(pL, 2));
                return 0;
            }, "TakeDamage");
            return 1;
        }
        if (strcmp(key, "Heal") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                GodotObjectWrapper* w2 = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                Humanoid2D* h = w2 ? Object::cast_to<Humanoid2D>(w2->node_ptr) : nullptr;
                if (h) h->heal((float)luaL_checknumber(pL, 2));
                return 0;
            }, "Heal");
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
            lua_pushcfunction(L, [](lua_State* pL) -> int { lua_pushboolean(pL, true); return 1; }, "IsClient");
            return 1;
        }
        if (strcmp(key, "IsServer") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int { lua_pushboolean(pL, true); return 1; }, "IsServer");
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
                    int64_t ptr_L = (int64_t)L;
                    if (target && target->has_signal(StringName(sig))) {
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
                int nargs = lua_gettop(pL) - 2;
                rev->fire_client(pL, 3, nargs < 0 ? 0 : nargs);
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
                rev->fire_client(pL, 2, nargs < 0 ? 0 : nargs);
                return 0;
            }, "FireAllClients", 1);
            return 1;
        }
        if (strcmp(key, "OnServerEvent") == 0) {
            lua_newtable(L);
            lua_pushlightuserdata(L, (void*)re);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RemoteEventNode* rev = (RemoteEventNode*)lua_touserdata(pL, lua_upvalueindex(1));
                int fn_pos = -1;
                for (int ai = 1; ai <= lua_gettop(pL); ai++) {
                    if (lua_isfunction(pL, ai)) { fn_pos = ai; break; }
                }
                if (fn_pos == -1 || !rev) return 0;
                lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                lua_State* mL = (lua_State*)lua_touserdata(pL, -1);
                lua_pop(pL, 1);
                if (!mL) mL = pL;
                lua_pushvalue(pL, fn_pos);
                int ref = lua_ref(pL, -1);
                lua_pop(pL, 1);
                rev->add_server_cb(mL, ref);
                return 0;
            }, "Connect", 1);
            lua_setfield(L, -2, "Connect");
            return 1;
        }
        if (strcmp(key, "OnClientEvent") == 0) {
            lua_newtable(L);
            lua_pushlightuserdata(L, (void*)re);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RemoteEventNode* rev = (RemoteEventNode*)lua_touserdata(pL, lua_upvalueindex(1));
                int fn_pos = -1;
                for (int ai = 1; ai <= lua_gettop(pL); ai++) {
                    if (lua_isfunction(pL, ai)) { fn_pos = ai; break; }
                }
                if (fn_pos == -1 || !rev) return 0;
                lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                lua_State* mL = (lua_State*)lua_touserdata(pL, -1);
                lua_pop(pL, 1);
                if (!mL) mL = pL;
                lua_pushvalue(pL, fn_pos);
                int ref = lua_ref(pL, -1);
                lua_pop(pL, 1);
                rev->add_client_cb(mL, ref);
                return 0;
            }, "Connect", 1);
            lua_setfield(L, -2, "Connect");
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
                int nargs = lua_gettop(pL) - 1;
                return rfn->invoke_server(pL, 2, nargs < 0 ? 0 : nargs);
            }, "InvokeServer", 1);
            return 1;
        }
        if (strcmp(key, "InvokeClient") == 0) {
            lua_pushlightuserdata(L, (void*)rf);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                RemoteFunctionNode* rfn = (RemoteFunctionNode*)lua_touserdata(pL, lua_upvalueindex(1));
                if (!rfn) { lua_pushnil(pL); return 1; }
                int nargs = lua_gettop(pL) - 2;
                return rfn->invoke_client(pL, 3, nargs < 0 ? 0 : nargs);
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
                return 0;
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
                return 0;
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
                return 0;
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
                return 0;
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
                auto* c=(std::vector<AnimationTrack::LuaCallback>*)lua_touserdata(LL,lua_upvalueindex(2));
                if(!c||!lua_isfunction(LL,2)){lua_pushnil(LL);return 1;}
                lua_pushvalue(LL,2); int ref=lua_ref(LL,-1); lua_pop(LL,1);
                c->push_back({LL,ref,true}); lua_pushnil(LL); return 1;
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
                Node* target_node = (w && w->node_ptr) ? w->node_ptr : nullptr;

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
                tw_wrap->node_ptr = tw;
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
                t->add_completed_cb(LL, ref);
                lua_pushnil(LL); return 1;
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
                c->push_back({LL, ref, true});
                lua_pushnil(LL); return 1;
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
                auto* c=(std::vector<ProximityPrompt::LuaCallback>*)lua_touserdata(LL,lua_upvalueindex(2));
                if (!c || !lua_isfunction(LL,2)) { lua_pushnil(LL); return 1; }
                lua_pushvalue(LL,2); int ref=lua_ref(LL,-1); lua_pop(LL,1);
                c->push_back({LL, ref, true});
                lua_pushnil(LL); return 1;
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
                auto* c=(std::vector<RobloxTool::LuaCallback>*)lua_touserdata(LL,lua_upvalueindex(2));
                if (!c || !lua_isfunction(LL,2)){lua_pushnil(LL);return 1;}
                lua_pushvalue(LL,2); int ref=lua_ref(LL,-1); lua_pop(LL,1);
                c->push_back({LL,ref,true});
                lua_pushnil(LL); return 1;
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
        if (strcmp(key,"TouchEnabled")    == 0) { lua_pushboolean(L, false); return 1; }
        if (strcmp(key,"GamepadEnabled")  == 0) { lua_pushboolean(L, false); return 1; }
        if (strcmp(key,"IsKeyDown") == 0 || strcmp(key,"IsKeyboardKeyDown") == 0) {
            lua_pushlightuserdata(L,(void*)uis);
            lua_pushcclosure(L,[](lua_State* LL)->int{
                UserInputService* u=(UserInputService*)lua_touserdata(LL,lua_upvalueindex(1));
                if (!u || !lua_isnumber(LL,2)){ lua_pushboolean(LL,0); return 1; }
                int kc=(int)lua_tonumber(LL,2);
                lua_pushboolean(LL, u->is_key_down(kc)?1:0); return 1;
            },"IsKeyDown",1); return 1;
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
                auto* c=(std::vector<UserInputService::LuaCallback>*)lua_touserdata(LL,lua_upvalueindex(2));
                if(!c||!lua_isfunction(LL,2)){lua_pushnil(LL);return 1;}
                lua_pushvalue(LL,2); int ref=lua_ref(LL,-1); lua_pop(LL,1);
                c->push_back({LL,ref,true}); lua_pushnil(LL); return 1;
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
                if(c&&w&&w->node_ptr) c->add_tag(w->node_ptr,String(tag));
                return 0;
            },"AddTag",1); return 1;
        }
        if (strcmp(key,"RemoveTag") == 0) {
            lua_pushlightuserdata(L,(void*)cs);
            lua_pushcclosure(L,[](lua_State* LL)->int{
                CollectionService* c=(CollectionService*)lua_touserdata(LL,lua_upvalueindex(1));
                GodotObjectWrapper* w=(GodotObjectWrapper*)lua_touserdata(LL,2);
                const char* tag=luaL_checkstring(LL,3);
                if(c&&w&&w->node_ptr) c->remove_tag(w->node_ptr,String(tag));
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
                if(c&&w&&w->node_ptr) r=c->has_tag(w->node_ptr,String(tag));
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
                        wr->node_ptr=nd;
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
                if(!c||!w||!w->node_ptr) return 1;
                PackedStringArray tags=c->get_tags(w->node_ptr);
                for(int i=0;i<tags.size();i++){
                    lua_pushstring(LL,tags[i].utf8().get_data());
                    lua_rawseti(LL,-2,i+1);
                }
                return 1;
            },"GetTags",1); return 1;
        }
    }

    Node* child = n->get_node_or_null(NodePath(key));
    if (child) { wrap_node(L, child); return 1; }

    lua_pushnil(L); return 1;
}

// ════════════════════════════════════════════════════════════════════
//  godot_object_newindex — __newindex of GodotObject
////
//  godot_object_newindex — __newindex de GodotObject
// ════════════════════════════════════════════════════════════════════
static int godot_object_newindex(lua_State* L) {
    GodotObjectWrapper* wrapper = (GodotObjectWrapper*)lua_touserdata(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (!wrapper || !wrapper->node_ptr) return 0;
    Node* n = wrapper->node_ptr;

    if (strcmp(key, "Name") == 0) {
        n->set_name(luaL_checkstring(L, 3));
        return 0;
    }

    if (strcmp(key, "Parent") == 0) {
        if (lua_isnil(L, 3)) {
            if (n->get_parent()) n->get_parent()->remove_child(n);
        } else {
            GodotObjectWrapper* p_wrap = (GodotObjectWrapper*)lua_touserdata(L, 3);
            if (p_wrap && p_wrap->node_ptr && p_wrap->node_ptr != n) {
                if (n->get_parent()) n->get_parent()->remove_child(n);
                p_wrap->node_ptr->add_child(n);
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
        LuauVector3* vec = (LuauVector3*)lua_touserdata(L, 3);
        Node3D* n3d = Object::cast_to<Node3D>(n);
        if (vec && n3d) {
            if (n3d->is_inside_tree()) n3d->set_global_position(Vector3(vec->x, vec->y, vec->z));
            else n3d->set_position(Vector3(vec->x, vec->y, vec->z));
        }
        return 0;
    }

    if (strcmp(key, "Rotation") == 0) {
        LuauVector3* vec = (LuauVector3*)lua_touserdata(L, 3);
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
            LuauColor3* col = (LuauColor3*)lua_touserdata(L, 3);
            if (col) part->set_color(Color(col->r, col->g, col->b));
            return 0;
        }
        if (strcmp(key, "Size") == 0) {
            LuauVector3* vec = (LuauVector3*)lua_touserdata(L, 3);
            if (vec) part->set_size(Vector3(vec->x, vec->y, vec->z));
            return 0;
        }
        if (strcmp(key, "Position") == 0) {
            LuauVector3* v = (LuauVector3*)lua_touserdata(L, 3);
            if (v) part->set_global_position(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key, "Orientation") == 0) {
            LuauVector3* v = (LuauVector3*)lua_touserdata(L, 3);
            if (v) part->set_rotation_degrees(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key, "Velocity") == 0 || strcmp(key, "AssemblyLinearVelocity") == 0) {
            LuauVector3* v = (LuauVector3*)lua_touserdata(L, 3);
            if (v) part->set_linear_velocity(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key, "AngularVelocity") == 0 || strcmp(key, "AssemblyAngularVelocity") == 0) {
            LuauVector3* v = (LuauVector3*)lua_touserdata(L, 3);
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
        if (strcmp(key, "UserId")                  == 0) { rp_w->set_user_id((int)luaL_checknumber(L,3));                    return 0; }
        if (strcmp(key, "DisplayName")             == 0) { rp_w->set_display_name(String(luaL_checkstring(L,3)));            return 0; }
        if (strcmp(key, "AccountAge")              == 0) { rp_w->set_account_age((int)luaL_checknumber(L,3));                return 0; }
        if (strcmp(key, "AutoJumpEnabled")         == 0) { rp_w->set_auto_jump_enabled(lua_toboolean(L,3)!=0);               return 0; }
        if (strcmp(key, "DevCameraOcclusionMode")  == 0) { rp_w->set_dev_camera_occlusion_mode((int)luaL_checknumber(L,3));  return 0; }
        if (strcmp(key, "DevComputerMovementMode") == 0) { rp_w->set_dev_computer_movement_mode((int)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key, "DevTouchMovementMode")    == 0) { rp_w->set_dev_touch_movement_mode((int)luaL_checknumber(L,3));    return 0; }
        if (strcmp(key, "TeamColor") == 0) {
            LuauColor3* col = (LuauColor3*)lua_touserdata(L, 3);
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
        if (strcmp(key, "Ambient")                  == 0) { LuauColor3* c = (LuauColor3*)lua_touserdata(L,3); if(c) light->set_ambient(Color(c->r,c->g,c->b)); return 0; }
        if (strcmp(key, "OutdoorAmbient")           == 0) { LuauColor3* c = (LuauColor3*)lua_touserdata(L,3); if(c) light->set_outdoor_ambient(Color(c->r,c->g,c->b)); return 0; }
        if (strcmp(key, "FogColor")                 == 0) { LuauColor3* c = (LuauColor3*)lua_touserdata(L,3); if(c) light->set_fog_color(Color(c->r,c->g,c->b)); return 0; }
        if (strcmp(key, "ColorShift_Top")           == 0) { LuauColor3* c = (LuauColor3*)lua_touserdata(L,3); if(c) light->set_color_shift_top(Color(c->r,c->g,c->b)); return 0; }
        if (strcmp(key, "ColorShift_Bottom")        == 0) { LuauColor3* c = (LuauColor3*)lua_touserdata(L,3); if(c) light->set_color_shift_bottom(Color(c->r,c->g,c->b)); return 0; }
    }

    // ── BodyMovers — newindex ──────────────────────────────────────
    BodyVelocity* bv_w = Object::cast_to<BodyVelocity>(n);
    if (bv_w) {
        if (strcmp(key,"Velocity") == 0) {
            LuauVector3* v = (LuauVector3*)lua_touserdata(L,3);
            if (v) bv_w->set_velocity(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key,"MaxForce") == 0) {
            LuauVector3* v = (LuauVector3*)lua_touserdata(L,3);
            if (v) bv_w->set_max_force(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key,"P") == 0) { bv_w->set_p((float)luaL_checknumber(L,3)); return 0; }
    }
    BodyPosition* bpos_w = Object::cast_to<BodyPosition>(n);
    if (bpos_w) {
        if (strcmp(key,"Position") == 0) {
            LuauVector3* v = (LuauVector3*)lua_touserdata(L,3);
            if (v) bpos_w->set_position_val(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key,"MaxForce") == 0) {
            LuauVector3* v = (LuauVector3*)lua_touserdata(L,3);
            if (v) bpos_w->set_max_force(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key,"P") == 0) { bpos_w->set_p((float)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key,"D") == 0) { bpos_w->set_d((float)luaL_checknumber(L,3)); return 0; }
    }
    BodyForce* bf_w = Object::cast_to<BodyForce>(n);
    if (bf_w) {
        if (strcmp(key,"Force") == 0) {
            LuauVector3* v = (LuauVector3*)lua_touserdata(L,3);
            if (v) bf_w->set_force(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key,"RelativeTo") == 0) { bf_w->set_relative_to(lua_toboolean(L,3)!=0); return 0; }
    }
    BodyAngularVelocity* bav_w = Object::cast_to<BodyAngularVelocity>(n);
    if (bav_w) {
        if (strcmp(key,"AngularVelocity") == 0) {
            LuauVector3* v = (LuauVector3*)lua_touserdata(L,3);
            if (v) bav_w->set_angular_velocity(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key,"MaxTorque") == 0) {
            LuauVector3* v = (LuauVector3*)lua_touserdata(L,3);
            if (v) bav_w->set_max_torque(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key,"P") == 0) { bav_w->set_p((float)luaL_checknumber(L,3)); return 0; }
    }
    BodyGyro* bgyro_w = Object::cast_to<BodyGyro>(n);
    if (bgyro_w) {
        if (strcmp(key,"MaxTorque") == 0) {
            LuauVector3* v = (LuauVector3*)lua_touserdata(L,3);
            if (v) bgyro_w->set_max_torque(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key,"P") == 0) { bgyro_w->set_p((float)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key,"D") == 0) { bgyro_w->set_d((float)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key,"CFrame") == 0) {
            // Acepta un LuauCFrame (Transform3D) o un Node3D (usa su transform global)
            LuauCFrame* cf = lua_isuserdata(L,3) ? (LuauCFrame*)lua_touserdata(L,3) : nullptr;
            if (cf) bgyro_w->set_cframe(cf->transform);
            else {
                GodotObjectWrapper* wr = lua_isuserdata(L,3) ? (GodotObjectWrapper*)lua_touserdata(L,3) : nullptr;
                if (wr && wr->node_ptr) {
                    Node3D* n3d = Object::cast_to<Node3D>(wr->node_ptr);
                    if (n3d) bgyro_w->set_cframe(n3d->get_global_transform());
                }
            }
            return 0;
        }
    }

    // ── Constraints — newindex ─────────────────────────────────────
    WeldConstraint* weld_w = Object::cast_to<WeldConstraint>(n);
    if (weld_w) {
        if (strcmp(key,"Part0") == 0) {
            GodotObjectWrapper* pw = (GodotObjectWrapper*)lua_touserdata(L,3);
            if (pw) weld_w->set_part0(pw->node_ptr);
            return 0;
        }
        if (strcmp(key,"Part1") == 0) {
            GodotObjectWrapper* pw = (GodotObjectWrapper*)lua_touserdata(L,3);
            if (pw) weld_w->set_part1(pw->node_ptr);
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
        if (strcmp(key,"Part0") == 0) { GodotObjectWrapper* pw=(GodotObjectWrapper*)lua_touserdata(L,3); if(pw) hinge_w->set_part0(pw->node_ptr); return 0; }
        if (strcmp(key,"Part1") == 0) { GodotObjectWrapper* pw=(GodotObjectWrapper*)lua_touserdata(L,3); if(pw) hinge_w->set_part1(pw->node_ptr); return 0; }
    }
    RodConstraint* rod_w = Object::cast_to<RodConstraint>(n);
    if (rod_w) {
        if (strcmp(key,"Enabled") == 0) { rod_w->set_enabled(lua_toboolean(L,3)!=0);          return 0; }
        if (strcmp(key,"Length")  == 0) { rod_w->set_length((float)luaL_checknumber(L,3));    return 0; }
        if (strcmp(key,"Part0")   == 0) { GodotObjectWrapper* pw=(GodotObjectWrapper*)lua_touserdata(L,3); if(pw) rod_w->set_part0(pw->node_ptr); return 0; }
        if (strcmp(key,"Part1")   == 0) { GodotObjectWrapper* pw=(GodotObjectWrapper*)lua_touserdata(L,3); if(pw) rod_w->set_part1(pw->node_ptr); return 0; }
    }
    SpringConstraint* spring_w = Object::cast_to<SpringConstraint>(n);
    if (spring_w) {
        if (strcmp(key,"Enabled")    == 0) { spring_w->set_enabled(lua_toboolean(L,3)!=0);            return 0; }
        if (strcmp(key,"FreeLength") == 0) { spring_w->set_free_length((float)luaL_checknumber(L,3)); return 0; }
        if (strcmp(key,"Stiffness")  == 0) { spring_w->set_stiffness((float)luaL_checknumber(L,3));   return 0; }
        if (strcmp(key,"Damping")    == 0) { spring_w->set_damping((float)luaL_checknumber(L,3));     return 0; }
        if (strcmp(key,"Part0") == 0) { GodotObjectWrapper* pw=(GodotObjectWrapper*)lua_touserdata(L,3); if(pw) spring_w->set_part0(pw->node_ptr); return 0; }
        if (strcmp(key,"Part1") == 0) { GodotObjectWrapper* pw=(GodotObjectWrapper*)lua_touserdata(L,3); if(pw) spring_w->set_part1(pw->node_ptr); return 0; }
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
            if (strcmp(key,"BackgroundColor3")==0) { LuauColor3* c=(LuauColor3*)lua_touserdata(L,3); if(c) var_name->set_bg_color(c->r,c->g,c->b); return 0; } \
            if (strcmp(key,"BackgroundTransparency")==0) { var_name->set_bg_alpha((float)luaL_checknumber(L,3)); return 0; } \
            if (strcmp(key,"BorderColor3")==0) { LuauColor3* c=(LuauColor3*)lua_touserdata(L,3); if(c) var_name->set_border_color(c->r,c->g,c->b); return 0; } \
            if (strcmp(key,"BorderSizePixel")==0) { var_name->set_border_px((int)luaL_checknumber(L,3)); return 0; } \
        }
    GUI_NEWINDEX_COMMON(RobloxFrame, rframe_w)
    GUI_NEWINDEX_COMMON(RobloxTextLabel, rtlabel_w)
    if (rtlabel_w) {
        if (strcmp(key,"Text")==0)        { rtlabel_w->set_text(String(luaL_checkstring(L,3))); return 0; }
        if (strcmp(key,"TextColor3")==0)  { LuauColor3* c=(LuauColor3*)lua_touserdata(L,3); if(c) rtlabel_w->set_text_color(c->r,c->g,c->b); return 0; }
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
        if (strcmp(key,"TextColor3")==0) { LuauColor3* c=(LuauColor3*)lua_touserdata(L,3); if(c) rtbutton_w->set_text_color(c->r,c->g,c->b); return 0; }
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
        if (strcmp(key,"ImageColor3")==0)      { LuauColor3* c=(LuauColor3*)lua_touserdata(L,3); if(c) rimage_w->set_image_color(c->r,c->g,c->b); return 0; }
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
        if (strcmp(key,"Looped")   == 0) { rsound_w->set_looped(lua_toboolean(L,3)!=0);                  return 0; }
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
            if (wr && wr->node_ptr) m6d_w->set_part0(wr->node_ptr);
            return 0;
        }
        if (strcmp(key,"Part1") == 0) {
            GodotObjectWrapper* wr = (GodotObjectWrapper*)lua_touserdata(L,3);
            if (wr && wr->node_ptr) m6d_w->set_part1(wr->node_ptr);
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

    return 0;
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
             strcmp(cls, "Configuration")     == 0 ||
             strcmp(cls, "StringValue")       == 0 ||
             strcmp(cls, "IntValue")          == 0 ||
             strcmp(cls, "NumberValue")       == 0 ||
             strcmp(cls, "BoolValue")         == 0 ||
             strcmp(cls, "ObjectValue")       == 0 ||
             strcmp(cls, "Vector3Value")      == 0 ||
             strcmp(cls, "CFrameValue")       == 0) nn = memnew(Folder);
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
            if (p_wrap && p_wrap->node_ptr && p_wrap->node_ptr != nn) {
                p_wrap->node_ptr->add_child(nn);
            }
        }
        return 1;
    }

    UtilityFunctions::print("[GodotLuau] Instance.new: clase desconocida: '", cls, "'");
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
    LuauVector3* vec = (LuauVector3*)lua_touserdata(L, 1);
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
            LuauVector3* goal = (LuauVector3*)lua_touserdata(pL, 1);
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
            LuauVector3* other = (LuauVector3*)lua_touserdata(pL, 1);
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
            LuauVector3* other = (LuauVector3*)lua_touserdata(pL, 1);
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
    LuauVector3* a = (LuauVector3*)lua_touserdata(L, 1);
    LuauVector3* b = (LuauVector3*)lua_touserdata(L, 2);
    if (a && b) { push_vector3(L, a->x+b->x, a->y+b->y, a->z+b->z); return 1; }
    lua_pushnil(L); return 1;
}

static int godot_vector3_sub(lua_State* L) {
    LuauVector3* a = (LuauVector3*)lua_touserdata(L, 1);
    LuauVector3* b = (LuauVector3*)lua_touserdata(L, 2);
    if (a && b) { push_vector3(L, a->x-b->x, a->y-b->y, a->z-b->z); return 1; }
    lua_pushnil(L); return 1;
}

static int godot_vector3_mul(lua_State* L) {
    if (lua_isnumber(L, 1)) {
        float s = (float)lua_tonumber(L, 1);
        LuauVector3* v = (LuauVector3*)lua_touserdata(L, 2);
        if (v) { push_vector3(L, v->x*s, v->y*s, v->z*s); return 1; }
    } else {
        LuauVector3* v = (LuauVector3*)lua_touserdata(L, 1);
        if (lua_isnumber(L, 2)) {
            float s = (float)lua_tonumber(L, 2);
            if (v) { push_vector3(L, v->x*s, v->y*s, v->z*s); return 1; }
        } else {
            LuauVector3* b = (LuauVector3*)lua_touserdata(L, 2);
            if (v && b) { push_vector3(L, v->x*b->x, v->y*b->y, v->z*b->z); return 1; }
        }
    }
    lua_pushnil(L); return 1;
}

static int godot_vector3_unm(lua_State* L) {
    LuauVector3* v = (LuauVector3*)lua_touserdata(L, 1);
    if (v) { push_vector3(L, -v->x, -v->y, -v->z); return 1; }
    lua_pushnil(L); return 1;
}

static int godot_vector3_tostring(lua_State* L) {
    LuauVector3* v = (LuauVector3*)lua_touserdata(L, 1);
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
    LuauColor3* col = (LuauColor3*)lua_touserdata(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (!col) { lua_pushnil(L); return 1; }
    if (strcmp(key, "R") == 0 || strcmp(key, "r") == 0) { lua_pushnumber(L, col->r); return 1; }
    if (strcmp(key, "G") == 0 || strcmp(key, "g") == 0) { lua_pushnumber(L, col->g); return 1; }
    if (strcmp(key, "B") == 0 || strcmp(key, "b") == 0) { lua_pushnumber(L, col->b); return 1; }
    lua_pushnil(L); return 1;
}

#endif // LUAU_API_H