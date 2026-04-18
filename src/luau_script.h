#ifndef LUAU_SCRIPT_NODE_H
#define LUAU_SCRIPT_NODE_H

// ════════════════════════════════════════════════════════════════════
//  Templates de scripts por nombre de nodo
//  Cuando ScriptNodeBase crea un archivo .lua en el editor, usa el
//  nombre del nodo para elegir el código plantilla correcto.
//  Así Health → código de regen, ControlModule → módulo de movimiento, etc.
// ════════════════════════════════════════════════════════════════════

// Health.lua — Regeneración de vida (StarterCharacterScripts, ServerScript)
static const char* LUAU_TEMPLATE_HEALTH = R"LUAU(
-- > GodotLuau — PimpoliDev
-- Health.lua — ServerScript — StarterCharacterScripts
-- Se clona al personaje al spawnear. script.Parent = el jugador (LocalPlayer).

local RunService = game:GetService("RunService")

local humanoid = script.Parent:FindFirstChild("Humanoid")
if not humanoid then
    warn("[Health] Humanoid no encontrado en el personaje")
    return
end

print("[Health] Regeneracion activa: " .. script.Parent.Name)

-- ── Configuración ─────────────────────────────────────────────────
local REGEN_DELAY = 3.0   -- segundos antes de empezar a regenerar
local REGEN_RATE  = 1.0   -- HP por segundo

local timer = 0.0

RunService.Heartbeat:Connect(function(dt)
    timer = timer + dt
    if timer < REGEN_DELAY then return end
    if humanoid.Health > 0 and humanoid.Health < humanoid.MaxHealth then
        humanoid.Health = math.min(humanoid.MaxHealth, humanoid.Health + REGEN_RATE * dt)
    end
end)
)LUAU";

// Animate.lua — Animaciones del personaje (StarterCharacterScripts)
static const char* LUAU_TEMPLATE_ANIMATE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- Animate.lua — Control de animaciones del personaje
-- Se clona al personaje al spawnear. script.Parent = el jugador.

local RunService = game:GetService("RunService")

local humanoid = script.Parent:FindFirstChild("Humanoid")
if not humanoid then
    warn("[Animate] Humanoid no encontrado")
    return
end

print("[Animate] Sistema de animacion listo")

-- ══ ZONA DE ANIMACIÓN PERSONALIZADA ══════════════════════════════
-- Descomenta para reaccionar a eventos del Humanoid:

-- humanoid.Died:Connect(function()
--     print("[Animate] El personaje murio!")
-- end)

-- humanoid.HealthChanged:Connect(function(newHP)
--     if newHP < humanoid.MaxHealth * 0.3 then
--         print("[Animate] Salud baja!")
--     end
-- end)

-- RunService.Heartbeat:Connect(function(dt)
--     -- Logica de animacion por frame
-- end)
-- ═════════════════════════════════════════════════════════════════
)LUAU";

// PlayerModule.lua — Loader principal (StarterPlayerScripts, LocalScript)
static const char* LUAU_TEMPLATE_PLAYER_MODULE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- PlayerModule.lua — LocalScript — StarterPlayerScripts
-- Se clona al jugador en Play. script.Parent = el jugador (LocalPlayer).
-- Carga todos los modulos de Modules/ y los inicializa.

local RunService = game:GetService("RunService")

-- script.Parent = el jugador (tiene Humanoid, Modules/, etc.)
local character = script.Parent
local humanoid  = character:FindFirstChild("Humanoid")

if not humanoid then
    warn("[PlayerModule] Humanoid no encontrado en el jugador")
    return
end

-- ── Cargar modulos desde Modules/ ────────────────────────────────
-- script.Parent.Modules = carpeta Modules en el jugador
local ControlModule = require(script.Parent.Modules.ControlModule)
local CameraModule  = require(script.Parent.Modules.CameraModule)
local ChatModule    = require(script.Parent.Modules.ChatModule)

-- ── Inicializar modulos ───────────────────────────────────────────
ControlModule:Initialize()
CameraModule:Apply()
ChatModule:Initialize()

-- ── Aplicar configuracion al Humanoid ────────────────────────────
humanoid.WalkSpeed = ControlModule.WalkSpeed
humanoid.JumpPower = ControlModule.JumpPower

print("[PlayerModule] Jugador listo! Speed=" .. ControlModule.WalkSpeed)

-- ── Heartbeat: actualizar velocidad en tiempo real ───────────────
RunService.Heartbeat:Connect(function(dt)
    if humanoid.Health <= 0 then return end

    local speed = ControlModule:GetCurrentSpeed()
    if humanoid.WalkSpeed ~= speed then
        humanoid.WalkSpeed = speed
    end

    -- ══ ZONA PERSONALIZABLE ═══════════════════════════════════
    -- Aqui puedes agregar logica por frame del jugador:
    --
    -- local x, z = ControlModule:GetMoveVector()
    -- print("Dir:", x, z)
    --
    -- if ChatModule:IsOpen() then ... end
    -- ══════════════════════════════════════════════════════════
end)
)LUAU";

// ControlModule.lua — Router de movimiento (delega a sub-módulos)
static const char* LUAU_TEMPLATE_CONTROL_MODULE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- ControlModule.lua — ModuleScript — Modules/
-- Detecta la plataforma y delega al sub-modulo correcto.
-- Sub-modulos: script.PCModule / script.MobileModule / script.ConsoleModule
-- Cambia Platform para forzar una plataforma especifica.

local ControlModule = {}

-- ── Sub-modulos (children de ControlModule en el arbol) ───────────
-- script aqui = ControlModule (gracias al require con contexto correcto)
local PCModule      = require(script.PCModule)
local MobileModule  = require(script.MobileModule)
local ConsoleModule = require(script.ConsoleModule)

-- ┌────────────────────────────────────────────────────────────────┐
-- │  CONFIGURACION  (modifica aqui)                                │
-- │  Platform: "" = auto  "PC" = teclado  "Mobile" = tactil       │
-- │            "Console" = mando                                   │
-- └────────────────────────────────────────────────────────────────┘
ControlModule.Platform  = "PC"  -- Forzar plataforma (o "" para auto)
ControlModule.WalkSpeed = 16
ControlModule.JumpPower = 20

local active = PCModule

function ControlModule:Initialize()
    local p = self.Platform
    if p == "Mobile" then
        active = MobileModule
    elseif p == "Console" then
        active = ConsoleModule
    else
        active = PCModule
    end
    -- Sincronizar velocidad desde sub-modulo activo
    self.WalkSpeed = active.WalkSpeed
    self.JumpPower = active.JumpPower
    if active.Initialize then active:Initialize() end
    print("[ControlModule] Plataforma: " .. (p == "" and "Auto(PC)" or p))
end

function ControlModule:GetCurrentSpeed()
    if active and active.GetCurrentSpeed then
        return active:GetCurrentSpeed()
    end
    return self.WalkSpeed
end

function ControlModule:GetMoveVector()
    if active and active.GetMoveVector then
        return active:GetMoveVector()
    end
    return 0, 0
end

return ControlModule
)LUAU";

// PCModule.lua — Movimiento PC (teclado + ratón)
static const char* LUAU_TEMPLATE_PC_MODULE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- PCModule.lua — ModuleScript — Modules/ControlModule/
-- Configuracion de movimiento para PC (teclado + raton).
-- El C++ lee las teclas directamente; aqui defines velocidades y comportamiento.

local PCModule = {}

-- ┌────────────────────────────────────────────────────────────────┐
-- │  VELOCIDADES PC                                                │
-- │  WalkSpeed: unidades/seg (16 = velocidad Roblox estandar)     │
-- │  RunSpeed:  velocidad al mantener LeftShift                    │
-- │  JumpPower: fuerza de salto                                   │
-- └────────────────────────────────────────────────────────────────┘
PCModule.WalkSpeed  = 16
PCModule.RunSpeed   = 24
PCModule.JumpPower  = 20
PCModule.AutoRotate = true  -- Girar el personaje hacia la direccion del movimiento

function PCModule:Initialize()
    print("[PCModule] Modo PC activo: W/A/S/D para mover, Shift para correr")
end

function PCModule:GetCurrentSpeed()
    -- El C++ maneja el Shift internamente; devolvemos WalkSpeed como base
    return self.WalkSpeed
end

function PCModule:GetMoveVector()
    return 0, 0  -- El C++ (Humanoid._physics_process) lee el input directamente
end

return PCModule
)LUAU";

// MobileModule.lua — Movimiento movil (tactil)
static const char* LUAU_TEMPLATE_MOBILE_MODULE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- MobileModule.lua — ModuleScript — Modules/ControlModule/
-- Configuracion de movimiento para movil/tactil.
-- Cambia TouchMode para elegir el tipo de control tactil.

local MobileModule = {}

-- ┌────────────────────────────────────────────────────────────────┐
-- │  CONFIGURACION MOVIL                                           │
-- │  TouchMode: "Joystick"  = joystick virtual en pantalla        │
-- │             "Console"   = estilo D-Pad (como consola)         │
-- │             "Tap"       = tocar para moverse al punto         │
-- └────────────────────────────────────────────────────────────────┘
MobileModule.WalkSpeed = 16
MobileModule.JumpPower = 20
MobileModule.TouchMode = "Joystick"

function MobileModule:Initialize()
    print("[MobileModule] Modo Movil activo: " .. self.TouchMode)
    -- Aqui puedes crear el joystick virtual con ScreenGui / ImageButton
end

function MobileModule:GetCurrentSpeed()
    return self.WalkSpeed
end

function MobileModule:GetMoveVector()
    return 0, 0  -- Implementar con input tactil (UserInputService.TouchMoved)
end

return MobileModule
)LUAU";

// ConsoleModule.lua — Movimiento consola (gamepad)
static const char* LUAU_TEMPLATE_CONSOLE_MODULE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- ConsoleModule.lua — ModuleScript — Modules/ControlModule/
-- Configuracion de movimiento para mando/gamepad.
-- Compatible con Xbox, PlayStation y mandos genéricos.

local ConsoleModule = {}

-- ┌────────────────────────────────────────────────────────────────┐
-- │  CONFIGURACION MANDO                                           │
-- │  Deadzone: zona muerta del joystick (0.0 a 1.0)               │
-- └────────────────────────────────────────────────────────────────┘
ConsoleModule.WalkSpeed = 16
ConsoleModule.JumpPower = 20
ConsoleModule.Deadzone  = 0.2  -- Ignorar movimientos menores a este valor

function ConsoleModule:Initialize()
    print("[ConsoleModule] Modo Consola activo — Joystick izquierdo para mover")
end

function ConsoleModule:GetCurrentSpeed()
    return self.WalkSpeed
end

function ConsoleModule:GetMoveVector()
    return 0, 0  -- Implementar con UserInputService gamepad input
end

return ConsoleModule
)LUAU";

// ChatModule.lua — Sistema de chat en Luau
static const char* LUAU_TEMPLATE_CHAT_MODULE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- ChatModule.lua — ModuleScript — Modules/
-- Sistema de chat completamente en Luau. Modificalo libremente.

local ChatModule = {}

local TextChatService = game:GetService("TextChatService")

-- ┌────────────────────────────────────────────────────────────────┐
-- │  CONFIGURACION DEL CHAT                                        │
-- └────────────────────────────────────────────────────────────────┘
ChatModule.MaxMessages  = 50     -- Maximo de mensajes visibles
ChatModule.FadeDelay    = 8.0    -- Segundos hasta desvanecer mensajes
ChatModule.FilterWords  = true  -- Filtrar palabras inapropiadas

local messages  = {}
local chat_open = false
local initialized = false

function ChatModule:Initialize()
    if initialized then return end
    initialized = true

    if TextChatService and TextChatService.MessageReceived then
        TextChatService.MessageReceived:Connect(function(msg)
            self:_addMessage(msg)
        end)
    end

    print("[ChatModule] Chat listo. / o Enter para abrir.")
end

function ChatModule:_addMessage(text)
    if type(text) ~= "string" then return end
    if #messages >= self.MaxMessages then
        table.remove(messages, 1)
    end
    table.insert(messages, { text = text, time = 0.0 })
    print("[Chat] " .. text)
end

function ChatModule:SendMessage(text)
    if not text or text == "" then return end
    if TextChatService then
        TextChatService:SendMessage(text)
    end
    self:_addMessage(text)
end

function ChatModule:IsOpen()
    return chat_open
end

function ChatModule:Open()
    chat_open = true
end

function ChatModule:Close()
    chat_open = false
end

function ChatModule:GetMessages()
    return messages
end

return ChatModule
)LUAU";

// CameraModule.lua — Módulo de control de cámara
static const char* LUAU_TEMPLATE_CAMERA_MODULE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- CameraModule — Módulo de control de cámara
-- Cambia Mode para modificar el comportamiento de la cámara.

local CameraModule = {}

-- ┌───────────────────────────────────────────────────────────────┐
-- │  MODO DE CÁMARA                                               │
-- │  1 = Fija      → Sigue al jugador al instante                │
-- │  2 = Suave     → Sigue con un pequeño retraso elegante       │
-- │  3 = Combinada → Retraso al moverse, centra al detenerse     │
-- └───────────────────────────────────────────────────────────────┘
CameraModule.Mode = 3   -- ← Cambia aquí el modo de cámara

-- Aplica el modo de cámara al jugador al iniciar
function CameraModule:Apply(player)
    if player then
        player.CameraMode = self.Mode
    end
end

-- Cambia el modo de cámara en tiempo real
function CameraModule:SetMode(mode)
    self.Mode = mode
    local Players = game:GetService("Players")
    local player  = Players.LocalPlayer
    if player then
        player.CameraMode = mode
    end
end

return CameraModule
)LUAU";

// GameManager.lua — Script principal del servidor (ServerScriptService)
static const char* LUAU_TEMPLATE_GAME_MANAGER = R"LUAU(
-- GameManager.lua — ServerScript
local Players    = game:GetService("Players")
local RunService = game:GetService("RunService")
local RS         = game:GetService("ReplicatedStorage")

print("[Server] GameManager started!")

-- Create shared RemoteEvents in ReplicatedStorage
local DamageEvent = Instance.new("RemoteEvent")
DamageEvent.Name   = "DamageEvent"
DamageEvent.Parent = RS

-- Client fires DamageEvent:FireServer(targetName, amount)
DamageEvent.OnServerEvent:Connect(function(player, targetName, amount)
    print("[Server]", player.Name, "-> damage", targetName, amount)
end)

-- Round timer example
local roundTime = 0
task.spawn(function()
    while true do
        task.wait(60)
        roundTime = roundTime + 1
        print("[Server] Round time:", roundTime, "min")
    end
end)

print("[Server] Game ready!")
)LUAU";

// Default LocalScript template
static const char* LUAU_TEMPLATE_LOCAL_SCRIPT = R"LUAU(
-- LocalScript — runs on the client
local Players          = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService       = game:GetService("RunService")

local player    = Players.LocalPlayer

-- Listen for a RemoteEvent sent from the server:
-- local MyEvent = ReplicatedStorage:WaitForChild("MyEvent")
-- MyEvent.OnClientEvent:Connect(function(data)
--     print("Received from server:", data)
-- end)

-- Fire an event to the server (e.g. on button click):
-- MyEvent:FireServer("hello from client")

-- Per-frame logic:
-- RunService.RenderStepped:Connect(function(dt)
--     -- client-side update
-- end)
)LUAU";

// Default ServerScript template
static const char* LUAU_TEMPLATE_SERVER_SCRIPT = R"LUAU(
-- ServerScript — runs on the server
local Players          = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService       = game:GetService("RunService")

-- Create a RemoteEvent so clients can communicate with this script:
-- local MyEvent = Instance.new("RemoteEvent")
-- MyEvent.Name   = "MyEvent"
-- MyEvent.Parent = ReplicatedStorage

-- Handle messages from clients:
-- MyEvent.OnServerEvent:Connect(function(player, data)
--     print(player.Name, "sent:", data)
--     MyEvent:FireClient(player, "server reply")
--     MyEvent:FireAllClients("broadcast to everyone")
-- end)

-- RemoteFunction — synchronous call from client, returns a value:
-- local GetData = Instance.new("RemoteFunction")
-- GetData.Name   = "GetData"
-- GetData.Parent = ReplicatedStorage
-- GetData.OnServerInvoke = function(player, key)
--     return "value_for_" .. key
-- end

print("[Server] Script started!")
)LUAU";

// Default ModuleScript OOP template
static const char* LUAU_TEMPLATE_MODULE_OOP = R"LUAU(
-- ModuleScript — OOP class pattern
local MyClass = {}
MyClass.__index = MyClass

-- Constructor
function MyClass.new(name, value)
    local self   = setmetatable({}, MyClass)
    self.Name    = name
    self.Value   = value or 0
    return self
end

-- Methods
function MyClass:GetValue()
    return self.Value
end

function MyClass:SetValue(val)
    self.Value = val
end

function MyClass:ToString()
    return "MyClass(" .. tostring(self.Name) .. ", " .. tostring(self.Value) .. ")"
end

-- Static helper
function MyClass.create(name)
    return MyClass.new(name, 0)
end

return MyClass
)LUAU";

#include "luau_api.h"
#include "roblox_part.h"
#include "humanoid.h"
#include "luau_stdlib.h"

#include <godot_cpp/classes/script_extension.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "lua.h"
#include "lualib.h"
#include "Luau/Compiler.h"

#include <vector>
#include <string>

using namespace godot;

// ── Helpers para _JSON: conversión Variant ↔ Lua ──────────────────────────
static void _gdluau_variant_to_lua(lua_State* L, const Variant& v) {
    switch (v.get_type()) {
        case Variant::NIL:    lua_pushnil(L); return;
        case Variant::BOOL:   lua_pushboolean(L, (bool)v); return;
        case Variant::INT:    lua_pushnumber(L, (double)(int64_t)v); return;
        case Variant::FLOAT:  lua_pushnumber(L, (double)v); return;
        case Variant::STRING: { String s = v; lua_pushstring(L, s.utf8().get_data()); return; }
        case Variant::ARRAY: {
            Array arr = v; lua_newtable(L);
            for (int i = 0; i < arr.size(); i++) {
                _gdluau_variant_to_lua(L, arr[i]);
                lua_rawseti(L, -2, i + 1);
            }
            return;
        }
        case Variant::DICTIONARY: {
            Dictionary dict = v; lua_newtable(L);
            Array keys = dict.keys();
            for (int i = 0; i < keys.size(); i++) {
                String k = String(keys[i]);
                lua_pushstring(L, k.utf8().get_data());
                _gdluau_variant_to_lua(L, dict[keys[i]]);
                lua_rawset(L, -3);
            }
            return;
        }
        default: lua_pushnil(L); return;
    }
}

static Variant _gdluau_lua_to_variant(lua_State* L, int idx, int depth = 0) {
    if (depth > 20) return Variant();
    if (idx < 0) idx = lua_gettop(L) + idx + 1;
    int t = lua_type(L, idx);
    if (t == LUA_TNIL)     return Variant();
    if (t == LUA_TBOOLEAN) return Variant((bool)lua_toboolean(L, idx));
    if (t == LUA_TNUMBER)  {
        double n = lua_tonumber(L, idx);
        return (n == (int64_t)n) ? Variant((int64_t)n) : Variant(n);
    }
    if (t == LUA_TSTRING)  return Variant(String(lua_tostring(L, idx)));
    if (t == LUA_TTABLE) {
        int seq_len = (int)lua_objlen(L, idx);
        if (seq_len > 0) {
            Array arr;
            for (int i = 1; i <= seq_len; i++) {
                lua_rawgeti(L, idx, i);
                arr.push_back(_gdluau_lua_to_variant(L, -1, depth + 1));
                lua_pop(L, 1);
            }
            return arr;
        }
        Dictionary dict;
        lua_pushnil(L);
        while (lua_next(L, idx) != 0) {
            String key;
            if (lua_type(L, -2) == LUA_TSTRING) key = String(lua_tostring(L, -2));
            else if (lua_type(L, -2) == LUA_TNUMBER) key = String::num(lua_tonumber(L, -2));
            dict[key] = _gdluau_lua_to_variant(L, -1, depth + 1);
            lua_pop(L, 1);
        }
        return dict;
    }
    return Variant();
}

class LuauScript : public ScriptExtension {
    GDCLASS(LuauScript, ScriptExtension);

protected:
    static void _bind_methods() {}
    String source_code;

public:
    String _get_source_code() const override { return source_code; }
    void _set_source_code(const String &p_code) override { source_code = p_code; }
    bool _is_tool() const override { return false; }
    bool _is_valid() const override { return true; }
    bool _has_source_code() const override { return true; }
    bool _can_instantiate() const override { return true; }
    bool _has_static_method(const StringName &p_method) const override { return false; }
    void _update_exports() override {}
    Ref<Script> _get_base_script() const override { return Ref<Script>(); }
    TypedArray<Dictionary> _get_documentation() const override { return TypedArray<Dictionary>(); }
    StringName _get_instance_base_type() const override { return StringName("Object"); }
    ScriptLanguage *_get_language() const override;
    void *_instance_create(Object *p_for_object) const override { return nullptr; }
};

// ════════════════════════════════════════════════════════════════════
//  ScriptNodeBase — Base de LocalScript, ServerScript, ModuleScript
// ════════════════════════════════════════════════════════════════════
class ScriptNodeBase : public Node {
    GDCLASS(ScriptNodeBase, Node);

public:
    // Hilo de Luau spawneado via task.spawn()
    struct SpawnedThread {
        lua_State* L;   
        int ref;        
        double timer;   
    };

private:
    Ref<LuauScript> codigo_luau;
    lua_State* L_main = nullptr;
    lua_State* L_thread = nullptr;

    double wait_timer = 0.0;
    bool is_waiting = false;
    bool script_finished = false;

    bool _auto_created_script = false;

public:
    std::vector<SpawnedThread> spawned_threads;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_codigo_luau", "s"), &ScriptNodeBase::set_codigo_luau);
        ClassDB::bind_method(D_METHOD("get_codigo_luau"), &ScriptNodeBase::get_codigo_luau);
        ClassDB::bind_method(D_METHOD("iniciar_corrutina"), &ScriptNodeBase::iniciar_corrutina);

        ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "codigo_luau", PROPERTY_HINT_RESOURCE_TYPE, "LuauScript"),
                     "set_codigo_luau", "get_codigo_luau");
    }

    void _notification(int p_what) {
        if (Engine::get_singleton()->is_editor_hint()) {
            if (p_what == NOTIFICATION_ENTER_TREE) {
                bool necesita_archivo = codigo_luau.is_null() ||
                    (!codigo_luau->get_path().is_empty() &&
                     !FileAccess::file_exists(codigo_luau->get_path()));

                if (necesita_archivo) {
                    String cls = get_class();
                    String folder = "res://" + cls + "s";
                    Ref<DirAccess> dir = DirAccess::open("res://");
                    if (dir.is_valid() && !dir->dir_exists(folder)) {
                        dir->make_dir(folder);
                    }

                    String file_path = folder + "/" + cls + "_" + String::num(get_instance_id()) + ".lua";

                    Ref<LuauScript> new_script;
                    new_script.instantiate();

                    // ── Selección de template según nombre del nodo ────────────
                    // Así Health → regen, ControlModule → movimiento, etc.
                    String template_code;
                    String node_name = get_name();

                    if (node_name == "Health") {
                        template_code = String(LUAU_TEMPLATE_HEALTH);
                    } else if (node_name == "Animate") {
                        template_code = String(LUAU_TEMPLATE_ANIMATE);
                    } else if (node_name == "PlayerModule") {
                        template_code = String(LUAU_TEMPLATE_PLAYER_MODULE);
                    } else if (node_name == "ControlModule") {
                        template_code = String(LUAU_TEMPLATE_CONTROL_MODULE);
                    } else if (node_name == "PCModule") {
                        template_code = String(LUAU_TEMPLATE_PC_MODULE);
                    } else if (node_name == "MobileModule") {
                        template_code = String(LUAU_TEMPLATE_MOBILE_MODULE);
                    } else if (node_name == "ConsoleModule") {
                        template_code = String(LUAU_TEMPLATE_CONSOLE_MODULE);
                    } else if (node_name == "CameraModule") {
                        template_code = String(LUAU_TEMPLATE_CAMERA_MODULE);
                    } else if (node_name == "ChatModule") {
                        template_code = String(LUAU_TEMPLATE_CHAT_MODULE);
                    } else if (node_name == "GameManager") {
                        template_code = String(LUAU_TEMPLATE_GAME_MANAGER);
                    } else if (cls == "LocalScript") {
                        template_code = String(LUAU_TEMPLATE_LOCAL_SCRIPT);
                    } else if (cls == "ServerScript") {
                        template_code = String(LUAU_TEMPLATE_SERVER_SCRIPT);
                    } else if (cls == "ModuleScript") {
                        template_code = String(LUAU_TEMPLATE_MODULE_OOP);
                    } else {
                        template_code = "-- > GodotLuau\nprint(\"Hello from " + cls + "\")\n";
                    }

                    new_script->_set_source_code(template_code);
                    new_script->set_path(file_path);
                    ResourceSaver::get_singleton()->save(new_script, file_path);
                    set_codigo_luau(new_script);
                    _auto_created_script = true;
                    UtilityFunctions::print("[GodotLuau] Script creado: ", file_path);
                }
            }
            else if (p_what == NOTIFICATION_EXIT_TREE) {
                if (_auto_created_script && is_queued_for_deletion() && codigo_luau.is_valid()) {
                    String path = codigo_luau->get_path();
                    if (!path.is_empty() && path.begins_with("res://") && FileAccess::file_exists(path)) {
                        Ref<DirAccess> dir = DirAccess::open(path.get_base_dir());
                        if (dir.is_valid()) {
                            dir->remove(path.get_file());
                            UtilityFunctions::print("[GodotLuau] Script eliminado: ", path.get_file());
                        }
                    }
                }
            }
        } else {
            if (p_what == NOTIFICATION_EXIT_TREE) {
                if (L_main) {
                    Node* root = get_tree()->get_root();
                    // Clean RunService callbacks
                    for (int _ri = 0; _ri < root->get_child_count(); _ri++) {
                        Node* game = root->get_child(_ri);
                        if (!game) continue;
                        Node* rs_node = game->get_node_or_null("RunService");
                        if (rs_node) {
                            RunService* rs = Object::cast_to<RunService>(rs_node);
                            if (rs) rs->remove_by_state(L_main);
                        }
                    }
                    // Clean Remote* callbacks — traverse entire tree
                    std::function<void(Node*)> cleanup_remotes = [&](Node* nd) {
                        if (!nd) return;
                        if (RemoteEventNode*    re = Object::cast_to<RemoteEventNode>(nd))    re->cleanup_state(L_main);
                        if (RemoteFunctionNode* rf = Object::cast_to<RemoteFunctionNode>(nd)) rf->cleanup_state(L_main);
                        if (BindableEventNode*  be = Object::cast_to<BindableEventNode>(nd))  be->cleanup_state(L_main);
                        for (int _ci = 0; _ci < nd->get_child_count(); _ci++)
                            cleanup_remotes(nd->get_child(_ci));
                    };
                    cleanup_remotes(root);
                }

                for (auto& st : spawned_threads) {
                    if (L_main) lua_unref(L_main, st.ref);
                }
                spawned_threads.clear();

                if (L_main) {
                    lua_close(L_main);
                    L_main = nullptr;
                    L_thread = nullptr;
                }
                script_finished = true;
                is_waiting = false;
            }
        }
    }

public:
    ScriptNodeBase() {}
    ~ScriptNodeBase() {
        if (L_main) {
            lua_close(L_main);
            L_main = nullptr;
        }
    }

    void set_codigo_luau(const Ref<LuauScript> &s) { codigo_luau = s; }
    Ref<LuauScript> get_codigo_luau() const { return codigo_luau; }

    void iniciar_corrutina() {
        if (Engine::get_singleton()->is_editor_hint()) return;
        if (get_class() == "ModuleScript") return;

        if (codigo_luau.is_null()) {
            UtilityFunctions::print("[GodotLuau Error] El nodo '", get_name(), "' no tiene un archivo Luau asignado.");
            return;
        }

        L_main = luaL_newstate();
        luaL_openlibs(L_main);

        // Guardar referencia al main state para que todas las corrutinas puedan encontrarlo
        lua_pushlightuserdata(L_main, (void*)L_main);
        lua_setfield(L_main, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");

        // ── Metatable: GodotObject ────────────────────────────────────────────
        luaL_newmetatable(L_main, "GodotObject");

        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            GodotObjectWrapper* ptr = (GodotObjectWrapper*)lua_touserdata(L, 1);
            String key = luaL_checkstring(L, 2);

            if (ptr && ptr->node_ptr) {
                Node* node = ptr->node_ptr;

                if (key == "FindFirstChild" || key == "WaitForChild") {
                    lua_pushcfunction(L, [](lua_State* pL) -> int {
                        GodotObjectWrapper* pptr = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                        String child_name = luaL_checkstring(pL, 2);
                        if (pptr && pptr->node_ptr) {
                            Node* target = pptr->node_ptr->get_node_or_null(NodePath(child_name));
                            if (target) { wrap_node(pL, target); return 1; }
                        }
                        lua_pushnil(pL); return 1;
                    }, key.utf8().get_data());
                    return 1;
                }

                if (key == "GetChildren") {
                    lua_pushcfunction(L, [](lua_State* pL) -> int {
                        GodotObjectWrapper* pptr = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                        lua_newtable(pL);
                        if (pptr && pptr->node_ptr) {
                            TypedArray<Node> children = pptr->node_ptr->get_children();
                            for (int i = 0; i < children.size(); i++) {
                                wrap_node(pL, Object::cast_to<Node>(children[i]));
                                lua_rawseti(pL, -2, i + 1);
                            }
                        }
                        return 1;
                    }, "GetChildren");
                    return 1;
                }

                Node* child = node->get_node_or_null(NodePath(key));
                if (child) { wrap_node(L, child); return 1; }
            }

            return godot_object_index(L);
        }, "__index");
        lua_setfield(L_main, -2, "__index");

        lua_pushcfunction(L_main, godot_object_newindex, "__newindex");
        lua_setfield(L_main, -2, "__newindex");
        lua_pop(L_main, 1);

        // ── Metatables matemáticas ────────────────────────────────────────────
        luaL_newmetatable(L_main, "Vector3");
        lua_pushcfunction(L_main, godot_vector3_index, "__index");   lua_setfield(L_main, -2, "__index");
        lua_pushcfunction(L_main, godot_vector3_add,   "__add");     lua_setfield(L_main, -2, "__add");
        lua_pushcfunction(L_main, godot_vector3_sub,   "__sub");     lua_setfield(L_main, -2, "__sub");
        lua_pushcfunction(L_main, godot_vector3_mul,   "__mul");     lua_setfield(L_main, -2, "__mul");
        lua_pushcfunction(L_main, godot_vector3_unm,   "__unm");     lua_setfield(L_main, -2, "__unm");
        lua_pushcfunction(L_main, godot_vector3_tostring, "__tostring"); lua_setfield(L_main, -2, "__tostring");
        lua_pop(L_main, 1);

        luaL_newmetatable(L_main, "Color3");
        lua_pushcfunction(L_main, godot_color3_index, "__index"); lua_setfield(L_main, -2, "__index");
        lua_pop(L_main, 1);

        // ── Buscar game y workspace ───────────────────────────────────────────
        Node* game_node = nullptr;
        Node* ws_node = nullptr;
        {
            Node* root = get_tree()->get_root();
            // Buscar nodo "game": RobloxDataModel, RobloxGame3D, RobloxGame2D,
            // o cualquier nodo llamado "Game"
            for (int _i = 0; _i < root->get_child_count() && !game_node; _i++) {
                Node* ch = root->get_child(_i);
                if (ch && (ch->is_class("RobloxDataModel") ||
                            ch->is_class("RobloxGame3D")   ||
                            ch->is_class("RobloxGame2D")   ||
                            ch->get_name() == StringName("Game"))) {
                    game_node = ch;
                } else if (ch) {
                    for (int _j = 0; _j < ch->get_child_count() && !game_node; _j++) {
                        Node* gc = ch->get_child(_j);
                        if (gc && (gc->is_class("RobloxDataModel") ||
                                    gc->is_class("RobloxGame3D")   ||
                                    gc->is_class("RobloxGame2D")   ||
                                    gc->get_name() == StringName("Game"))) {
                            game_node = gc;
                        }
                    }
                }
            }
            if (!game_node) game_node = root;
            ws_node = game_node->get_node_or_null("Workspace");
            if (!ws_node) ws_node = (Node*)get_viewport();
        }
        wrap_node(L_main, game_node); lua_setglobal(L_main, "game");
        wrap_node(L_main, ws_node);   lua_setglobal(L_main, "workspace");
        wrap_node(L_main, this);      lua_setglobal(L_main, "script");

        // ── Instance ──────────────────────────────────────────────────────────
        lua_newtable(L_main);
        lua_pushcfunction(L_main, godot_instance_new, "new");
        lua_setfield(L_main, -2, "new");
        lua_setglobal(L_main, "Instance");

        // ── Vector3 ───────────────────────────────────────────────────────────
        lua_newtable(L_main);
        lua_pushcfunction(L_main, godot_vector3_new,  "new");    lua_setfield(L_main, -2, "new");
        {
            LuauVector3* z = (LuauVector3*)lua_newuserdata(L_main, sizeof(LuauVector3));
            z->x = 0; z->y = 0; z->z = 0;
            luaL_getmetatable(L_main, "Vector3"); lua_setmetatable(L_main, -2);
            lua_setfield(L_main, -2, "zero");

            LuauVector3* o = (LuauVector3*)lua_newuserdata(L_main, sizeof(LuauVector3));
            o->x = 1; o->y = 1; o->z = 1;
            luaL_getmetatable(L_main, "Vector3"); lua_setmetatable(L_main, -2);
            lua_setfield(L_main, -2, "one");
        }
        lua_setglobal(L_main, "Vector3");

        // ── Color3 ────────────────────────────────────────────────────────────
        lua_newtable(L_main);
        lua_pushcfunction(L_main, godot_color3_new,      "new");     lua_setfield(L_main, -2, "new");
        lua_pushcfunction(L_main, godot_color3_from_rgb, "fromRGB"); lua_setfield(L_main, -2, "fromRGB");
        lua_setglobal(L_main, "Color3");

        // ── task ─────────────────────────────────────────────────────────────
        lua_newtable(L_main);

        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            double sec = luaL_optnumber(L, 1, 0.0);
            lua_pushnumber(L, sec);
            lua_yield(L, 1);
            return 0;
        }, "wait");
        lua_setfield(L_main, -2, "wait");

        lua_pushlightuserdata(L_main, (void*)this);
        lua_pushcclosure(L_main, [](lua_State* L) -> int {
            ScriptNodeBase* self = static_cast<ScriptNodeBase*>(lua_touserdata(L, lua_upvalueindex(1)));
            if (!self || !lua_isfunction(L, 1)) return 0;

            lua_State* NL = lua_newthread(L);
            lua_pushvalue(L, 1);
            lua_xmove(L, NL, 1);

            int n_args = lua_gettop(L) - 2; 
            if (n_args > 0) {
                for (int i = 2; i <= n_args + 1; i++) lua_pushvalue(L, i);
                lua_xmove(L, NL, n_args);
            }

            int status = lua_resume(NL, nullptr, n_args);

            if (status == LUA_YIELD) {
                int thread_ref = lua_ref(L, lua_gettop(L));
                lua_pop(L, 1); 

                double wait_time = 0.0;
                if (lua_gettop(NL) > 0 && lua_isnumber(NL, -1)) {
                    wait_time = lua_tonumber(NL, -1);
                    lua_pop(NL, 1);
                }

                SpawnedThread st;
                st.L = NL;
                st.ref = thread_ref;
                st.timer = wait_time;
                self->spawned_threads.push_back(st);
            } else {
                if (status != LUA_OK) {
                    UtilityFunctions::print("[task.spawn Error] ", lua_tostring(NL, -1));
                }
                lua_pop(L, 1); 
            }
            return 0;
        }, "spawn", 1);
        lua_setfield(L_main, -2, "spawn");

        lua_pushlightuserdata(L_main, (void*)this);
        lua_pushcclosure(L_main, [](lua_State* L) -> int {
            ScriptNodeBase* self = static_cast<ScriptNodeBase*>(lua_touserdata(L, lua_upvalueindex(1)));
            if (!self) return 0;
            double delay_time = luaL_checknumber(L, 1);
            if (!lua_isfunction(L, 2)) return 0;

            lua_State* NL = lua_newthread(L);
            lua_pushvalue(L, 2);
            lua_xmove(L, NL, 1);

            int thread_ref = lua_ref(L, lua_gettop(L));
            lua_pop(L, 1);

            SpawnedThread st;
            st.L = NL;
            st.ref = thread_ref;
            st.timer = delay_time;
            self->spawned_threads.push_back(st);
            return 0;
        }, "delay", 1);
        lua_setfield(L_main, -2, "delay");

        lua_setglobal(L_main, "task");

        // ── print / warn / error ──────────────────────────────────────────────
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            int n = lua_gettop(L);
            String msg;
            for (int i = 1; i <= n; i++) {
                msg += String(luaL_tolstring(L, i, nullptr));
                if (i < n) msg += "\t";
            }
            UtilityFunctions::print("[Luau] ", msg);
            return 0;
        }, "print");
        lua_setglobal(L_main, "print");

        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            int n = lua_gettop(L);
            String msg;
            for (int i = 1; i <= n; i++) {
                msg += String(luaL_tolstring(L, i, nullptr));
                if (i < n) msg += "\t";
            }
            UtilityFunctions::print("[Luau WARN] ", msg);
            return 0;
        }, "warn");
        lua_setglobal(L_main, "warn");

        // AQUÍ ESTÁ EL ARREGLO PRINCIPAL DEL LUA_ERROR
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            const char* msg = luaL_checkstring(L, 1);
            luaL_error(L, "%s", msg);
            return 0;
        }, "error");
        lua_setglobal(L_main, "error");

        // ── wait (alias global de task.wait) ─────────────────────────────────
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            double sec = luaL_optnumber(L, 1, 0.0);
            lua_pushnumber(L, sec);
            lua_yield(L, 1);
            return 0;
        }, "wait");
        lua_setglobal(L_main, "wait");

        // ── tick — tiempo Unix actual (segundos) ──────────────────────────────
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            lua_pushnumber(L, (double)Time::get_singleton()->get_unix_time_from_system());
            return 1;
        }, "tick");
        lua_setglobal(L_main, "tick");

        // ── time — tiempo de ejecución del juego (segundos) ───────────────────
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            lua_pushnumber(L, (double)Time::get_singleton()->get_ticks_msec() / 1000.0);
            return 1;
        }, "time");
        lua_setglobal(L_main, "time");

        // ── typeof — tipo extendido (reconoce Vector3, Color3, Instance) ──────
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            int t = lua_type(L, 1);
            if (t == LUA_TUSERDATA && lua_getmetatable(L, 1)) {
                luaL_getmetatable(L, "Vector3");
                if (lua_rawequal(L, -1, -2)) { lua_pop(L, 2); lua_pushstring(L, "Vector3"); return 1; }
                lua_pop(L, 1);
                luaL_getmetatable(L, "Color3");
                if (lua_rawequal(L, -1, -2)) { lua_pop(L, 2); lua_pushstring(L, "Color3"); return 1; }
                lua_pop(L, 1);
                luaL_getmetatable(L, "GodotObject");
                if (lua_rawequal(L, -1, -2)) { lua_pop(L, 2); lua_pushstring(L, "Instance"); return 1; }
                lua_pop(L, 2);
                lua_pushstring(L, "userdata"); return 1;
            }
            lua_pushstring(L, lua_typename(L, t));
            return 1;
        }, "typeof");
        lua_setglobal(L_main, "typeof");

        // ── pcall mejorado (ya viene de luaL_openlibs pero lo sobreescribimos) ─
        // El pcall nativo de Luau ya funciona correctamente, solo lo dejamos.

        // ── tostring / tonumber ───────────────────────────────────────────────
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            lua_tostring(L, 1); 
            return 1;
        }, "tostring");
        lua_setglobal(L_main, "tostring");

        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            if (lua_type(L, 1) == LUA_TNUMBER) { lua_pushvalue(L, 1); return 1; }
            if (lua_type(L, 1) == LUA_TSTRING) {
                double n = 0;
                if (sscanf(luaL_checkstring(L, 1), "%lf", &n) == 1) {
                    lua_pushnumber(L, n); return 1;
                }
            }
            lua_pushnil(L); return 1;
        }, "tonumber");
        lua_setglobal(L_main, "tonumber");

        // ── UserInputService ─────────────────────────────────────────────────
        lua_newtable(L_main);

        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            int n = lua_gettop(L);
            const char* key_name;
            if (n >= 2 && lua_isstring(L, 2)) {
                key_name = luaL_checkstring(L, 2); 
            } else {
                key_name = luaL_checkstring(L, 1); 
            }

            Input* inp = Input::get_singleton();
            Key code = KEY_UNKNOWN;

            if      (strcmp(key_name, "W") == 0 || strcmp(key_name, "Up") == 0)       code = KEY_W;
            else if (strcmp(key_name, "A") == 0 || strcmp(key_name, "Left") == 0)     code = KEY_A;
            else if (strcmp(key_name, "S") == 0 || strcmp(key_name, "Down") == 0)     code = KEY_S;
            else if (strcmp(key_name, "D") == 0 || strcmp(key_name, "Right") == 0)    code = KEY_D;
            else if (strcmp(key_name, "Space") == 0)    code = KEY_SPACE;
            else if (strcmp(key_name, "E") == 0)        code = KEY_E;
            else if (strcmp(key_name, "Q") == 0)        code = KEY_Q;
            else if (strcmp(key_name, "F") == 0)        code = KEY_F;
            else if (strcmp(key_name, "G") == 0)        code = KEY_G;
            else if (strcmp(key_name, "R") == 0)        code = KEY_R;
            else if (strcmp(key_name, "T") == 0)        code = KEY_T;
            else if (strcmp(key_name, "LeftShift") == 0 || strcmp(key_name, "RightShift") == 0) code = KEY_SHIFT;
            else if (strcmp(key_name, "LeftControl") == 0 || strcmp(key_name, "RightControl") == 0) code = KEY_CTRL;
            else if (strcmp(key_name, "LeftAlt") == 0 || strcmp(key_name, "RightAlt") == 0) code = KEY_ALT;
            else if (strcmp(key_name, "Return") == 0 || strcmp(key_name, "Enter") == 0) code = KEY_ENTER;
            else if (strcmp(key_name, "Escape") == 0)   code = KEY_ESCAPE;
            else if (strcmp(key_name, "Tab") == 0)      code = KEY_TAB;
            else if (strcmp(key_name, "X") == 0)        code = KEY_X;
            else if (strcmp(key_name, "C") == 0)        code = KEY_C;
            else if (strcmp(key_name, "V") == 0)        code = KEY_V;
            else if (strcmp(key_name, "Z") == 0)        code = KEY_Z;
            else if (strcmp(key_name, "1") == 0)        code = KEY_1;
            else if (strcmp(key_name, "2") == 0)        code = KEY_2;
            else if (strcmp(key_name, "3") == 0)        code = KEY_3;
            else if (strcmp(key_name, "4") == 0)        code = KEY_4;
            else if (strcmp(key_name, "5") == 0)        code = KEY_5;

            lua_pushboolean(L, inp ? inp->is_key_pressed(code) : false);
            return 1;
        }, "IsKeyDown");
        lua_setfield(L_main, -2, "IsKeyDown");

        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            int n = lua_gettop(L);
            int btn = (n >= 2) ? (int)luaL_checknumber(L, 2) : (int)luaL_checknumber(L, 1);
            Input* inp = Input::get_singleton();
            MouseButton mb = (btn == 1) ? MOUSE_BUTTON_LEFT :
                             (btn == 2) ? MOUSE_BUTTON_RIGHT :
                                          MOUSE_BUTTON_MIDDLE;
            lua_pushboolean(L, inp ? inp->is_mouse_button_pressed(mb) : false);
            return 1;
        }, "IsMouseButtonPressed");
        lua_setfield(L_main, -2, "IsMouseButtonPressed");

        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            Input* inp = Input::get_singleton();
            Vector2 delta = inp ? inp->get_last_mouse_velocity() * 0.001f : Vector2();
            lua_newtable(L);
            lua_pushnumber(L, delta.x); lua_setfield(L, -2, "X");
            lua_pushnumber(L, delta.y); lua_setfield(L, -2, "Y");
            return 1;
        }, "GetMouseDelta");
        lua_setfield(L_main, -2, "GetMouseDelta");

        lua_setglobal(L_main, "UserInputService");

        // ── require ───────────────────────────────────────────────────────────
        setup_require_system(L_main);

        // ── _FileAccess — I/O de archivos para DataStoreService ───────────────
        lua_newtable(L_main);
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            const char* path = luaL_checkstring(L, 1);
            Ref<FileAccess> f = FileAccess::open(String(path), FileAccess::READ);
            if (f.is_null()) { lua_pushnil(L); return 1; }
            String s = f->get_as_text();
            lua_pushstring(L, s.utf8().get_data());
            return 1;
        }, "read");     lua_setfield(L_main, -2, "read");
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            const char* path = luaL_checkstring(L, 1);
            const char* data = luaL_checkstring(L, 2);
            Ref<FileAccess> f = FileAccess::open(String(path), FileAccess::WRITE);
            if (!f.is_null()) f->store_string(String(data));
            return 0;
        }, "write");    lua_setfield(L_main, -2, "write");
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            lua_pushboolean(L, FileAccess::file_exists(String(luaL_checkstring(L, 1))) ? 1 : 0);
            return 1;
        }, "exists");   lua_setfield(L_main, -2, "exists");
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            DirAccess::remove_absolute(String(luaL_checkstring(L, 1)));
            return 0;
        }, "delete");   lua_setfield(L_main, -2, "delete");
        lua_setglobal(L_main, "_FileAccess");

        // ── _JSON — serialización JSON ────────────────────────────────────────
        lua_newtable(L_main);
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            Variant v = _gdluau_lua_to_variant(L, 1);
            String result = JSON::stringify(v, "", false, true);
            lua_pushstring(L, result.utf8().get_data());
            return 1;
        }, "encode");   lua_setfield(L_main, -2, "encode");
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            const char* s = luaL_checkstring(L, 1);
            Variant v = JSON::parse_string(String(s));
            _gdluau_variant_to_lua(L, v);
            return 1;
        }, "decode");   lua_setfield(L_main, -2, "decode");
        lua_setglobal(L_main, "_JSON");

        // ── _HTTP — stub (sin HTTPClient bloqueante) ──────────────────────────
        lua_newtable(L_main);
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            lua_pushstring(L, ""); return 1;
        }, "get");     lua_setfield(L_main, -2, "get");
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            lua_pushstring(L, ""); return 1;
        }, "post");    lua_setfield(L_main, -2, "post");
        lua_setglobal(L_main, "_HTTP");

        // ── _InstanceNew — crear instancias en runtime ────────────────────────
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            const char* class_name = luaL_checkstring(L, 1);
            Object* obj = ClassDB::instantiate(StringName(class_name));
            Node* node  = Object::cast_to<Node>(obj);
            if (!node) { if (obj) memdelete(obj); lua_pushnil(L); return 1; }
            wrap_node(L, node);
            return 1;
        }, "_InstanceNew");
        lua_setglobal(L_main, "_InstanceNew");

        // ── Ejecutar stdlib (servicios Lua, class(), Enum, TweenInfo…) ────────
        {
            std::string sbc = Luau::compile(LUAU_STDLIB_CODE);
            if (luau_load(L_main, "__GodotLuauStdlib", sbc.data(), sbc.size(), 0) == 0) {
                if (lua_pcall(L_main, 0, 0, 0) != LUA_OK) {
                    UtilityFunctions::print("[GodotLuau Stdlib Error] ", lua_tostring(L_main, -1));
                    lua_pop(L_main, 1);
                }
            } else {
                UtilityFunctions::print("[GodotLuau Stdlib Syntax] ", lua_tostring(L_main, -1));
                lua_pop(L_main, 1);
            }
        }

        // ── Compilar y ejecutar el script ─────────────────────────────────────
        L_thread = lua_newthread(L_main);
        String source = codigo_luau->_get_source_code();

        std::string bytecode = Luau::compile(source.utf8().get_data());

        if (luau_load(L_thread, String(get_name()).utf8().get_data(), bytecode.data(), bytecode.size(), 0) == 0) {
            resume();
        } else {
            UtilityFunctions::print("[GodotLuau Sintaxis] ", get_name(), ": ", lua_tostring(L_thread, -1));
        }
    }

    void setup_require_system(lua_State* L) {
        lua_pushcfunction(L, [](lua_State* L) -> int {
            GodotObjectWrapper* wrapper = (GodotObjectWrapper*)lua_touserdata(L, 1);
            if (!wrapper || !wrapper->node_ptr) {
                UtilityFunctions::print("[Error] require() requiere un ModuleScript válido.");
                return 0;
            }

            Node* mod_node = wrapper->node_ptr;
            if (mod_node->get_class() != "ModuleScript") {
                UtilityFunctions::print("[Error] require() solo acepta ModuleScripts.");
                return 0;
            }

            String cache_id = "CACHED_MOD_" + String::num(mod_node->get_instance_id());
            lua_getfield(L, LUA_REGISTRYINDEX, cache_id.utf8().get_data());
            if (!lua_isnil(L, -1)) return 1;
            lua_pop(L, 1);

            ScriptNodeBase* mod_ptr = Object::cast_to<ScriptNodeBase>(mod_node);
            if (!mod_ptr) return 0;

            Ref<LuauScript> res = mod_ptr->get_codigo_luau();
            if (res.is_null()) {
                UtilityFunctions::print("[Error] require(): ModuleScript '", mod_node->get_name(), "' sin código asignado.");
                return 0;
            }

            std::string bc = Luau::compile(res->_get_source_code().utf8().get_data());
            if (luau_load(L, String(mod_node->get_name()).utf8().get_data(), bc.data(), bc.size(), 0) != 0) {
                return 0;
            }

            // Save script global, set it to this module so script.X works inside it
            lua_getglobal(L, "script");
            int saved_script = lua_ref(L, -1); // <-- Usamos la API nativa de Luau
            lua_pop(L, 1); // <-- Luau requiere sacar el valor manualmente

            wrap_node(L, mod_node);
            lua_setglobal(L, "script");

            int call_status = lua_pcall(L, 0, 1, 0);

            // Restore script global (always, even on error)
            lua_rawgeti(L, LUA_REGISTRYINDEX, saved_script);
            lua_setglobal(L, "script");
            lua_unref(L, saved_script); // <-- Usamos lua_unref de Luau

            if (call_status == LUA_OK) {
                lua_pushvalue(L, -1);
                lua_setfield(L, LUA_REGISTRYINDEX, cache_id.utf8().get_data());
                return 1;
            }
            UtilityFunctions::print("[Module Error] ", lua_tostring(L, -1));
            lua_pop(L, 1);
            return 0;
        }, "require");
        lua_setglobal(L, "require");
    }

    void resume() {
        int status = lua_resume(L_thread, nullptr, 0);
        if (status == LUA_YIELD) {
            if (lua_gettop(L_thread) > 0 && lua_isnumber(L_thread, -1)) {
                wait_timer = lua_tonumber(L_thread, -1);
                lua_pop(L_thread, 1);
            } else {
                wait_timer = 0.0;
            }
            is_waiting = true;
        } else if (status != LUA_OK) {
            UtilityFunctions::print("[GodotLuau Runtime] ", get_name(), ": ", lua_tostring(L_thread, -1));
            script_finished = true;
        } else {
            script_finished = true;
        }
    }

    void _process(double delta) override {
        if (!script_finished && is_waiting) {
            wait_timer -= delta;
            if (wait_timer <= 0.0) {
                is_waiting = false;
                resume();
            }
        }

        for (int i = (int)spawned_threads.size() - 1; i >= 0; i--) {
            SpawnedThread& st = spawned_threads[i];
            st.timer -= delta;

            if (st.timer <= 0.0) {
                int status = lua_resume(st.L, nullptr, 0);

                if (status == LUA_YIELD) {
                    if (lua_gettop(st.L) > 0 && lua_isnumber(st.L, -1)) {
                        st.timer = lua_tonumber(st.L, -1);
                        lua_pop(st.L, 1);
                    } else {
                        st.timer = 0.0;
                    }
                } else {
                    if (status != LUA_OK) {
                        UtilityFunctions::print("[task.spawn Thread] ", get_name(), ": ",
                            lua_tostring(st.L, -1));
                    }
                    if (L_main) lua_unref(L_main, st.ref);
                    spawned_threads.erase(spawned_threads.begin() + i);
                }
            }
        }
    }

    void _ready() override {
        if (!Engine::get_singleton()->is_editor_hint()) {
            call_deferred("iniciar_corrutina");
        }
    }
};

// ════════════════════════════════════════════════════════════════════
//  Tipos concretos de script
// ════════════════════════════════════════════════════════════════════
class ServerScript : public ScriptNodeBase {
    GDCLASS(ServerScript, ScriptNodeBase);
protected:
    static void _bind_methods() {}
};

class LocalScript : public ScriptNodeBase {
    GDCLASS(LocalScript, ScriptNodeBase);
protected:
    static void _bind_methods() {}
};

class ModuleScript : public ScriptNodeBase {
    GDCLASS(ModuleScript, ScriptNodeBase);
protected:
    static void _bind_methods() {}
};

#endif // LUAU_SCRIPT_NODE_H