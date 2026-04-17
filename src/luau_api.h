#ifndef LUAU_API_H
#define LUAU_API_H

#include "folder.h"
#include "humanoid.h"
#include "humanoid2d.h"
#include "roblox_player.h"
#include "roblox_player2d.h"
#include "roblox_part.h"
#include "roblox_services.h"

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
    if (w && w->node_ptr) {
        Node* found = w->node_ptr->get_node_or_null(NodePath(name));
        if (found) { wrap_node(L, found); return 1; }
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

static int method_getservice(lua_State* L) {
    GodotObjectWrapper* w = (GodotObjectWrapper*)lua_touserdata(L, 1);
    const char* svc_name = luaL_checkstring(L, 2);
    if (!w || !w->node_ptr) { lua_pushnil(L); return 1; }

    // Primero buscar servicios Lua puros en _G["__SVC_Nombre"]
    std::string global_key = std::string("__SVC_") + svc_name;
    lua_getglobal(L, global_key.c_str());
    if (!lua_isnil(L, -1)) return 1;
    lua_pop(L, 1);

    // Luego buscar nodo C++ en el árbol de escena
    Node* svc = w->node_ptr->get_node_or_null(NodePath(svc_name));
    if (svc) { wrap_node(L, svc); return 1; }

    for (int i = 0; i < w->node_ptr->get_child_count(); i++) {
        Node* child = w->node_ptr->get_child(i);
        if (!child) continue;
        svc = child->get_node_or_null(NodePath(svc_name));
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

    if (strcmp(key, "Destroy")          == 0) { lua_pushcfunction(L, method_destroy,             "Destroy");          return 1; }
    if (strcmp(key, "Clone")            == 0) { lua_pushcfunction(L, method_clone,               "Clone");            return 1; }
    if (strcmp(key, "ClearAllChildren") == 0) { lua_pushcfunction(L, method_clearallchildren,    "ClearAllChildren"); return 1; }
    if (strcmp(key, "FindFirstChild")   == 0 ||
        strcmp(key, "WaitForChild")     == 0) { lua_pushcfunction(L, method_findfirstchild,      key);                return 1; }
    if (strcmp(key, "FindFirstChildOfClass") == 0) { lua_pushcfunction(L, method_findfirstchildofclass, key);         return 1; }
    if (strcmp(key, "GetChildren")      == 0) { lua_pushcfunction(L, method_getchildren,         "GetChildren");      return 1; }
    if (strcmp(key, "GetDescendants")   == 0) { lua_pushcfunction(L, method_getdescendants,      "GetDescendants");   return 1; }
    if (strcmp(key, "IsA")              == 0) { lua_pushcfunction(L, method_isa,                 "IsA");              return 1; }
    if (strcmp(key, "SetAttribute")     == 0) { lua_pushcfunction(L, method_setattribute,        "SetAttribute");     return 1; }
    if (strcmp(key, "GetAttribute")     == 0) { lua_pushcfunction(L, method_getattribute,        "GetAttribute");     return 1; }
    if (strcmp(key, "GetService")       == 0) { lua_pushcfunction(L, method_getservice,          "GetService");       return 1; }

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
        if (strcmp(key, "Brightness")    == 0) { lua_pushnumber(L, light_svc->get_brightness());    return 1; }
        if (strcmp(key, "ClockTime")     == 0) { lua_pushnumber(L, light_svc->get_clock_time());    return 1; }
        if (strcmp(key, "GlobalShadows") == 0) { lua_pushboolean(L, light_svc->get_global_shadows()); return 1; }
        if (strcmp(key, "FogDensity")    == 0) { lua_pushnumber(L, light_svc->get_fog_density());   return 1; }
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
        if (strcmp(key, "CameraMode") == 0) { lua_pushnumber(L, rp->get_camera_mode()); return 1; }
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
        if (strcmp(key, "Health")    == 0) { lua_pushnumber(L, hum->get_health());     return 1; }
        if (strcmp(key, "MaxHealth") == 0) { lua_pushnumber(L, hum->get_max_health()); return 1; }
        if (strcmp(key, "WalkSpeed") == 0) { lua_pushnumber(L, hum->get_walk_speed()); return 1; }
        if (strcmp(key, "JumpPower") == 0) { lua_pushnumber(L, hum->get_jump_power()); return 1; }
        if (strcmp(key, "IsDead")    == 0) { lua_pushboolean(L, hum->is_character_dead()); return 1; }

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
        if (strcmp(key, "Anchored")     == 0) { lua_pushboolean(L, part->get_anchored());      return 1; }
        if (strcmp(key, "CanCollide")   == 0) { lua_pushboolean(L, part->get_can_collide());   return 1; }
        if (strcmp(key, "Transparency") == 0) { lua_pushnumber(L, part->get_transparency());   return 1; }

        if (strcmp(key, "Color") == 0) {
            Color c = part->get_color();
            push_color3(L, c.r, c.g, c.b); return 1;
        }
        if (strcmp(key, "Size") == 0) {
            Vector3 s = part->get_size();
            push_vector3(L, s.x, s.y, s.z); return 1;
        }

        if (strcmp(key, "Touched") == 0) {
            lua_newtable(L);
            lua_pushlightuserdata(L, n); lua_setfield(L, -2, "_node");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getfield(L, 1, "_node");
                Node* target = (Node*)lua_touserdata(L, -1);
                lua_pop(L, 1);

                if (lua_isfunction(L, 2)) {
                    lua_pushvalue(L, 2);
                    int ref = lua_ref(L, -1);
                    int64_t ptr_L = (int64_t)L;

                    if (target && target->has_signal("Touched")) {
                        target->connect("Touched",
                            Callable(target, "_on_luau_part_touched").bind(ref, ptr_L));
                    }
                }
                return 0;
            }, "Connect");
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

    if (strcmp(key, "MovementType") == 0) {
        RobloxPlayer2D* rp2d = Object::cast_to<RobloxPlayer2D>(n);
        if (rp2d) rp2d->set_movement_type((int)luaL_checknumber(L, 3));
        return 0;
    }

    Humanoid* hum = Object::cast_to<Humanoid>(n);
    if (hum) {
        if (strcmp(key, "Health")    == 0) { hum->set_health((float)luaL_checknumber(L, 3));    return 0; }
        if (strcmp(key, "WalkSpeed") == 0) { hum->set_walk_speed((float)luaL_checknumber(L, 3)); return 0; }
        if (strcmp(key, "JumpPower") == 0) { hum->set_jump_power((float)luaL_checknumber(L, 3)); return 0; }
        if (strcmp(key, "MaxHealth") == 0) { hum->set_max_health((float)luaL_checknumber(L, 3)); return 0; }
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
        if (strcmp(key, "Anchored")     == 0) { part->set_anchored(lua_toboolean(L, 3) != 0);                  return 0; }
        if (strcmp(key, "CanCollide")   == 0) { part->set_can_collide(lua_toboolean(L, 3) != 0);               return 0; }
        if (strcmp(key, "Transparency") == 0) { part->set_transparency((float)luaL_checknumber(L, 3));         return 0; }
        if (strcmp(key, "Color") == 0) {
            LuauColor3* col = (LuauColor3*)lua_touserdata(L, 3);
            if (col) part->set_color(Color(col->r, col->g, col->b));
            return 0;
        }
        if (strcmp(key, "Size") == 0) {
            LuauVector3* vec = (LuauVector3*)lua_touserdata(L, 3);
            if (vec) part->set_size(Vector3(vec->x, vec->y, vec->z));
            return 0;
        }
    }

    Lighting* light = Object::cast_to<Lighting>(n);
    if (light) {
        if (strcmp(key, "Brightness")    == 0) { light->set_brightness((float)luaL_checknumber(L, 3));    return 0; }
        if (strcmp(key, "ClockTime")     == 0) { light->set_clock_time((float)luaL_checknumber(L, 3));    return 0; }
        if (strcmp(key, "GlobalShadows") == 0) { light->set_global_shadows(lua_toboolean(L, 3) != 0);    return 0; }
        if (strcmp(key, "FogDensity")    == 0) { light->set_fog_density((float)luaL_checknumber(L, 3));  return 0; }
    }

    return 0;
}

// ════════════════════════════════════════════════════════════════════
//  Instance.new
// ════════════════════════════════════════════════════════════════════
static int godot_instance_new(lua_State* L) {
    const char* cls = luaL_checkstring(L, 1);
    Node* nn = nullptr;

    if      (strcmp(cls, "Part")           == 0) nn = memnew(RobloxPart);
    else if (strcmp(cls, "Folder")         == 0) nn = memnew(Folder);
    else if (strcmp(cls, "Model")          == 0) nn = memnew(Node3D);
    else if (strcmp(cls, "Humanoid")       == 0) nn = memnew(Humanoid);
    else if (strcmp(cls, "Humanoid2D")     == 0) nn = memnew(Humanoid2D);
    else if (strcmp(cls, "PointLight")     == 0) nn = memnew(OmniLight3D);
    else if (strcmp(cls, "SpotLight")      == 0) nn = memnew(SpotLight3D);
    else if (strcmp(cls, "DirectionalLight") == 0) nn = memnew(DirectionalLight3D);

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