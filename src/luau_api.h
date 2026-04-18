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

static int method_getservice(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    const char* svc_name = luaL_checkstring(L, 2);
    if (!w || !w->node_ptr) { lua_pushnil(L); return 1; }

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

    // 3. Anywhere in subtree (handles nested scenes)
    Node* svc = _find_node_by_name(w->node_ptr, svc_name);
    if (svc) { wrap_node(L, svc); return 1; }

    // 4. Search whole scene tree from root
    if (w->node_ptr->is_inside_tree()) {
        svc = _find_node_by_name((Node*)w->node_ptr->get_tree()->get_root(), svc_name); // <-- AÑADIDO (Node*)
        if (svc) { wrap_node(L, svc); return 1; }
    }

    lua_pushnil(L); return 1;
}

// ════════════════════════════════════════════════════════════════════
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
        if (strcmp(key, "GetPlayers") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                GodotObjectWrapper* w2 = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                Players* ps = w2 ? Object::cast_to<Players>(w2->node_ptr) : nullptr;
                lua_newtable(pL);
                if (ps) {
                    TypedArray<Node> pl = ps->get_players();
                    for (int i = 0; i < pl.size(); i++) {
                        wrap_node(pL, Object::cast_to<Node>(pl[i]));
                        lua_rawseti(pL, -2, i + 1);
                    }
                }
                return 1;
            }, "GetPlayers");
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
        // workspace:Raycast(origin, direction, [maxDist]) — igual que Roblox
        if (strcmp(key, "Raycast") == 0) {
            lua_pushcfunction(L, [](lua_State* pL) -> int {
                GodotObjectWrapper* wr = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                LuauVector3* orig = lua_isuserdata(pL, 2) ? (LuauVector3*)lua_touserdata(pL, 2) : nullptr;
                LuauVector3* dv   = lua_isuserdata(pL, 3) ? (LuauVector3*)lua_touserdata(pL, 3) : nullptr;
                float max_d = lua_isnumber(pL, 4) ? (float)lua_tonumber(pL, 4) : 1000.0f;
                if (!wr || !wr->node_ptr || !orig || !dv) { lua_pushnil(pL); return 1; }
                Node3D* ws3d = Object::cast_to<Node3D>(wr->node_ptr);
                if (!ws3d || !ws3d->is_inside_tree()) { lua_pushnil(pL); return 1; }
                PhysicsDirectSpaceState3D* space = ws3d->get_world_3d()->get_direct_space_state();
                if (!space) { lua_pushnil(pL); return 1; }
                Vector3 from(orig->x, orig->y, orig->z);
                Vector3 d_norm = Vector3(dv->x, dv->y, dv->z).normalized();
                Ref<PhysicsRayQueryParameters3D> params;
                params.instantiate();
                params->set_from(from);
                params->set_to(from + d_norm * max_d);
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
            }, "Raycast");
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
        if (strcmp(key, "AccountAge")              == 0) { lua_pushnumber(L, rp->get_account_age());               return 1; }
        if (strcmp(key, "AutoJumpEnabled")         == 0) { lua_pushboolean(L, rp->get_auto_jump_enabled());        return 1; }
        if (strcmp(key, "DevCameraOcclusionMode")  == 0) { lua_pushnumber(L, rp->get_dev_camera_occlusion_mode()); return 1; }
        if (strcmp(key, "DevComputerMovementMode") == 0) { lua_pushnumber(L, rp->get_dev_computer_movement_mode()); return 1; }
        if (strcmp(key, "DevTouchMovementMode")    == 0) { lua_pushnumber(L, rp->get_dev_touch_movement_mode());   return 1; }
        if (strcmp(key, "TeamColor") == 0) {
            Color c = rp->get_team_color();
            push_color3(L, c.r, c.g, c.b); return 1;
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
        // ── Señales del Humanoid ──────────────────────────────────
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
            // Devolver positivo aunque internamente sea negativo (misma convención que Roblox)
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
        // Señales: Heartbeat, RenderStepped, Stepped
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
                // Obtener main_L desde el registro
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
                return 0;
            }, "Connect", 2);
            lua_setfield(L, -2, "Connect");
            // Once — llama la función solo la primera vez
            lua_pushlightuserdata(L, (void*)rs);
            lua_pushstring(L, key);
            lua_pushcclosure(L, [](lua_State* pL) -> int {
                // Similar a Connect pero marca la conexión para desactivarse después del primer disparo
                // Por simplicidad, hacemos un Connect normal (el usuario puede Disconnect manualmente)
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
                return 0;
            }, "Once", 2);
            lua_setfield(L, -2, "Once");
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

    // ── CurrentCamera — workspace.CurrentCamera devuelve la cámara activa ─
    // Busca Camera3D o Camera2D en el workspace o en el jugador local
    if (strcmp(key, "CurrentCamera") == 0) {
        // Buscar en el nodo actual (Workspace puede tener CurrentCamera como hijo)
        Node* cam = n->get_node_or_null("CurrentCamera");
        if (cam) { wrap_node(L, cam); return 1; }
        // Fallback: buscar en el LocalPlayer
        Node* lp = n->get_node_or_null("LocalPlayer");
        if (lp) {
            cam = lp->get_node_or_null("Camera3D");
            if (!cam) cam = lp->get_node_or_null("Camera2D");
            if (cam) { wrap_node(L, cam); return 1; }
        }
        lua_pushnil(L); return 1;
    }

    Node* child = n->get_node_or_null(NodePath(key));
    if (child) { wrap_node(L, child); return 1; }

    lua_pushnil(L); return 1;
}

// ════════════════════════════════════════════════════════════════════
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
                    if (!scene_root) scene_root = (Node*)n->get_tree()->get_root(); // <-- AÑADIDO (Node*)
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
        // WalkSpeed/JumpPower delegan al RobloxPlayer2D padre (movimiento está en el player)
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
        if (strcmp(key, "Velocity") == 0) {
            LuauVector3* v = (LuauVector3*)lua_touserdata(L, 3);
            if (v) part->set_linear_velocity(Vector3(v->x, v->y, v->z));
            return 0;
        }
        if (strcmp(key, "AngularVelocity") == 0) {
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