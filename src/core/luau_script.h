#ifndef LUAU_SCRIPT_NODE_H
#define LUAU_SCRIPT_NODE_H

// ════════════════════════════════════════════════════════════════════
//  Script templates by node name
//  When ScriptNodeBase creates a .lua file in the editor, it uses the
//  node name to choose the correct template code.
//  e.g. Health → regen code, ControlModule → movement module, etc.
////
//  Templates de scripts por nombre de nodo
//  Cuando ScriptNodeBase crea un archivo .lua en el editor, usa el
//  nombre del nodo para elegir el código plantilla correcto.
//  Así Health → código de regen, ControlModule → módulo de movimiento, etc.
// ════════════════════════════════════════════════════════════════════

// Health.lua — Health regeneration (StarterCharacterScripts, ServerScript)
//// Health.lua — Regeneración de vida (StarterCharacterScripts, ServerScript)
static const char* LUAU_TEMPLATE_HEALTH = R"LUAU(
-- > GodotLuau — PimpoliDev
-- Health.lua — ServerScript — StarterCharacterScripts
-- Se clona al personaje al spawnear. script.Parent = el jugador (LocalPlayer).

local RunService = game:GetService("RunService")

local humanoid = script.Parent:FindFirstChild("Humanoid")
if not humanoid then
    dwarn("[Health] Humanoid no encontrado en el personaje")
    return
end

dprint("[Health] Regeneracion activa: " .. script.Parent.Name)

-- ── Configuración ─────────────────────────────────────────────────
-- Igual que Roblox: la regeneracion empieza REGEN_DELAY segundos
-- despues del ULTIMO daño recibido (recibir daño reinicia el contador).
local REGEN_DELAY = 3.0    -- segundos sin recibir daño para regenerar
local REGEN_RATE  = 0.01   -- fraccion de MaxHealth por segundo (1% como Roblox)

local last_health = humanoid.Health
local since_damage = REGEN_DELAY  -- permite regenerar desde el inicio

humanoid.HealthChanged:Connect(function(newHP)
    if newHP < last_health then
        since_damage = 0.0  -- recibio daño → reiniciar el contador
    end
    last_health = newHP
end)

humanoid.Died:Connect(function()
    dprint("[Health] " .. script.Parent.Name .. " ha muerto.")
end)

RunService.Heartbeat:Connect(function(dt)
    since_damage = since_damage + dt
    if since_damage < REGEN_DELAY then return end
    if humanoid.Health > 0 and humanoid.Health < humanoid.MaxHealth then
        local nuevo = humanoid.Health + humanoid.MaxHealth * REGEN_RATE * dt
        humanoid.Health = math.min(humanoid.MaxHealth, nuevo)
        last_health = humanoid.Health
    end
end)
)LUAU";

// Animate.lua — Character animations (StarterCharacterScripts)
//// Animate.lua — Animaciones del personaje (StarterCharacterScripts)
static const char* LUAU_TEMPLATE_ANIMATE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- Animate.lua — Control de animaciones del personaje
-- Se clona al personaje al spawnear. script.Parent = el jugador.

local RunService = game:GetService("RunService")

local humanoid = script.Parent:FindFirstChild("Humanoid")
if not humanoid then
    dwarn("[Animate] Humanoid no encontrado")
    return
end

dprint("[Animate] Sistema de animacion listo")

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

// PlayerModule.lua — Main loader (StarterPlayerScripts, LocalScript)
//// PlayerModule.lua — Loader principal (StarterPlayerScripts, LocalScript)
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
    dwarn("[PlayerModule] Humanoid no encontrado en el jugador")
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

dprint("[PlayerModule] Jugador listo! Speed=" .. ControlModule.WalkSpeed)

-- ── Eventos del personaje ─────────────────────────────────────────
humanoid.Died:Connect(function()
    dprint("[PlayerModule] El personaje ha muerto.")
end)

-- ── Heartbeat: sprint suave, estamina y velocidad en tiempo real ──
RunService.Heartbeat:Connect(function(dt)
    if humanoid.Health <= 0 then return end

    -- El ControlModule calcula la velocidad (sprint con aceleracion suave)
    ControlModule:Update(dt)
    local speed = ControlModule:GetCurrentSpeed()
    if math.abs(humanoid.WalkSpeed - speed) > 0.01 then
        humanoid.WalkSpeed = speed
    end

    -- ══ ZONA PERSONALIZABLE ═══════════════════════════════════
    -- Aqui puedes agregar logica por frame del jugador:
    --
    -- local x, z = ControlModule:GetMoveVector()      -- direccion teclado
    -- local st, stMax = ControlModule:GetStamina()    -- estamina actual
    --
    -- if ChatModule:IsOpen() then ... end
    -- ══════════════════════════════════════════════════════════
end)
)LUAU";

// ControlModule.lua — Movement router (delegates to sub-modules)
//// ControlModule.lua — Router de movimiento (delega a sub-módulos)
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
    dprint("[ControlModule] Plataforma: " .. (p == "" and "Auto(PC)" or p))
end

-- Llamado cada frame por PlayerModule (sprint suave, estamina, etc.)
function ControlModule:Update(dt)
    if active and active.Update then
        active:Update(dt)
    end
end

function ControlModule:GetCurrentSpeed()
    if active and active.GetCurrentSpeed then
        return active:GetCurrentSpeed()
    end
    return self.WalkSpeed
end

function ControlModule:GetStamina()
    if active and active.GetStamina then
        return active:GetStamina()
    end
    return 100, 100
end

function ControlModule:GetMoveVector()
    if active and active.GetMoveVector then
        return active:GetMoveVector()
    end
    return 0, 0
end

return ControlModule
)LUAU";

// PCModule.lua — PC movement (keyboard + mouse)
//// PCModule.lua — Movimiento PC (teclado + ratón)
static const char* LUAU_TEMPLATE_PC_MODULE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- PCModule.lua — ModuleScript — Modules/ControlModule/
-- Configuracion de movimiento para PC (teclado + raton).
-- El C++ lee las teclas directamente; aqui defines velocidades y comportamiento.

local PCModule = {}

-- ┌────────────────────────────────────────────────────────────────┐
-- │  VELOCIDADES PC                                                │
-- │  WalkSpeed:    unidades/seg (16 = velocidad Roblox estandar)  │
-- │  RunSpeed:     velocidad al mantener LeftShift                 │
-- │  Acceleration: que tan rapido cambia la velocidad (suavidad)  │
-- │  JumpPower:    fuerza de salto                                 │
-- └────────────────────────────────────────────────────────────────┘
PCModule.WalkSpeed    = 16
PCModule.RunSpeed     = 24
PCModule.Acceleration = 8     -- mayor = cambio mas brusco, menor = mas suave
PCModule.JumpPower    = 20
PCModule.AutoRotate   = true  -- Girar el personaje hacia la direccion del movimiento

-- ┌────────────────────────────────────────────────────────────────┐
-- │  ESTAMINA (opcional) — desactivada por defecto                 │
-- │  Si la activas, correr consume estamina y se regenera al      │
-- │  caminar. Cuando llega a 0 no se puede correr.                │
-- └────────────────────────────────────────────────────────────────┘
PCModule.StaminaEnabled = false
PCModule.StaminaMax     = 100
PCModule.StaminaDrain   = 20   -- por segundo corriendo
PCModule.StaminaRegen   = 15   -- por segundo caminando

local current_speed = 16
local stamina       = 100

function PCModule:Initialize()
    current_speed = self.WalkSpeed
    stamina       = self.StaminaMax
    dprint("[PCModule] Modo PC activo: W/A/S/D para mover, Shift para correr")
end

-- Llamado cada frame por ControlModule:Update(dt)
function PCModule:Update(dt)
    local wants_run = UserInputService:IsKeyDown("LeftShift")

    if self.StaminaEnabled then
        if wants_run and stamina > 0 then
            stamina = math.max(0, stamina - self.StaminaDrain * dt)
        else
            stamina = math.min(self.StaminaMax, stamina + self.StaminaRegen * dt)
        end
        if stamina <= 0 then wants_run = false end
    end

    -- Aceleracion suave hacia la velocidad objetivo (sin saltos bruscos)
    local target = wants_run and self.RunSpeed or self.WalkSpeed
    local t = math.min(1, self.Acceleration * dt)
    current_speed = current_speed + (target - current_speed) * t
end

function PCModule:GetCurrentSpeed()
    return current_speed
end

function PCModule:GetStamina()
    return stamina, self.StaminaMax
end

function PCModule:GetMoveVector()
    -- Vector de movimiento leido del teclado (el C++ mueve al personaje;
    -- esto es util para logica propia: animaciones, efectos, etc.)
    local x, z = 0, 0
    if UserInputService:IsKeyDown("A") then x = x - 1 end
    if UserInputService:IsKeyDown("D") then x = x + 1 end
    if UserInputService:IsKeyDown("W") then z = z - 1 end
    if UserInputService:IsKeyDown("S") then z = z + 1 end
    return x, z
end

return PCModule
)LUAU";

// MobileModule.lua — Mobile movement (touch)
//// MobileModule.lua — Movimiento movil (tactil)
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
    dprint("[MobileModule] Modo Movil activo: " .. self.TouchMode)
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

// ConsoleModule.lua — Console movement (gamepad)
//// ConsoleModule.lua — Movimiento consola (gamepad)
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
    dprint("[ConsoleModule] Modo Consola activo — Joystick izquierdo para mover")
end

function ConsoleModule:GetCurrentSpeed()
    return self.WalkSpeed
end

function ConsoleModule:GetMoveVector()
    return 0, 0  -- Implementar con UserInputService gamepad input
end

return ConsoleModule
)LUAU";

// ChatModule.lua — Chat system in Luau
//// ChatModule.lua — Sistema de chat en Luau
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

    dprint("[ChatModule] Chat listo. / o Enter para abrir.")
end

function ChatModule:_addMessage(text)
    if type(text) ~= "string" then return end
    if #messages >= self.MaxMessages then
        table.remove(messages, 1)
    end
    table.insert(messages, { text = text, time = 0.0 })
    dprint("[Chat] " .. text)
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

// CameraModule.lua — Camera control module
//// CameraModule.lua — Módulo de control de cámara
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

// GameManager.lua — Main server script (ServerScriptService)
//// GameManager.lua — Script principal del servidor (ServerScriptService)
static const char* LUAU_TEMPLATE_GAME_MANAGER = R"LUAU(
-- GameManager.lua — ServerScript
-- El cerebro del servidor: jugadores, eventos remotos y rondas.
-- TODO lo que pase aqui es autoridad del servidor (anti-exploit).
local Players    = game:GetService("Players")
local RunService = game:GetService("RunService")
local RS         = game:GetService("ReplicatedStorage")

dprint("[Server] GameManager iniciado!")

-- ── Jugadores: entrada y salida ───────────────────────────────────
Players.PlayerAdded:Connect(function(player)
    dprint("[Server] + " .. player.Name .. " entro a la partida")
end)

Players.PlayerRemoving:Connect(function(player)
    dprint("[Server] - " .. player.Name .. " salio de la partida")
end)

-- ── RemoteEvents compartidos en ReplicatedStorage ─────────────────
local DamageEvent = Instance.new("RemoteEvent")
DamageEvent.Name   = "DamageEvent"
DamageEvent.Parent = RS

-- REGLA DE ORO multijugador: NUNCA confiar en lo que manda el cliente.
-- Validar siempre los datos antes de aplicarlos.
local MAX_DAMAGE = 35

DamageEvent.OnServerEvent:Connect(function(player, targetName, amount)
    -- Validacion anti-exploit
    if type(targetName) ~= "string" or type(amount) ~= "number" then return end
    amount = math.clamp(amount, 0, MAX_DAMAGE)

    dprint("[Server]", player.Name, "-> daño a", targetName, "(" .. amount .. ")")
    -- Aqui aplicarias el daño al objetivo, por ejemplo:
    -- local target = workspace:FindFirstChild(targetName)
    -- local hum = target and target:FindFirstChild("Humanoid")
    -- if hum then hum:TakeDamage(amount) end
end)

-- ── Sistema de rondas (esqueleto) ─────────────────────────────────
local ROUND_TIME = 120  -- segundos por ronda

task.spawn(function()
    while true do
        dprint("[Server] Ronda nueva! (" .. ROUND_TIME .. "s)")
        task.wait(ROUND_TIME)
        dprint("[Server] Fin de la ronda.")
        task.wait(5)  -- intermedio entre rondas
    end
end)

dprint("[Server] Juego listo!")
)LUAU";

// Default LocalScript / ServerScript template — mínimo estilo Roblox Studio.
// %SCRIPT_ID% se sustituye por el ID único del script al crearlo.
static const char* LUAU_TEMPLATE_LOCAL_SCRIPT =
    "-->GodotLuau Developed by PimpoliDev\n"
    "print(\"Hello World\" .. \" the script \" .. \"%SCRIPT_ID%\" .. \" he was executed\")\n";

static const char* LUAU_TEMPLATE_SERVER_SCRIPT =
    "-->GodotLuau Developed by PimpoliDev\n"
    "print(\"Hello World\" .. \" the script \" .. \"%SCRIPT_ID%\" .. \" he was executed\")\n";

// Default ModuleScript template — mínimo + boilerplate de módulo.
static const char* LUAU_TEMPLATE_MODULE_OOP =
    "-->GodotLuau Developed by PimpoliDev\n"
    "print(\"Hello World\" .. \" the script \" .. \"%SCRIPT_ID%\" .. \" he was executed\")\n"
    "\n"
    "local module = {}\n"
    "\n"
    "return module\n";

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
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/http_client.hpp>
#include <godot_cpp/classes/tls_options.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "lua.h"
#include "lualib.h"
#include "Luau/Compiler.h"

#include <godot_cpp/classes/os.hpp>

#include "gl_debug.h"   // GL_DEBUG_PRINT + gl_tr3

#include <vector>
#include <string>

using namespace godot;

// ── Helpers for _JSON: Variant ↔ Lua conversion ───────────────────────────
//// ── Helpers para _JSON: conversión Variant ↔ Lua ──────────────────────────
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

// ── Helpers para _HTTP: cliente HTTP real (bloqueante) sobre HTTPClient ─────
//// Fase 1 de la capa de red: HttpService:GetAsync/PostAsync/RequestAsync ya
//// no son un stub. Bloquea el hilo hasta completar o timeout; contra Nakama
//// local son milisegundos. La versión NO bloqueante (yield) llegará con el
//// NakamaService en C++ (WebSocket + HTTP asíncrono).

struct GLHttpResult {
    int    status = 0;      // código HTTP (200, 404…); 0 = fallo de red/conexión
    String status_msg;      // "OK", "Timeout al conectar"…
    String body;            // cuerpo de la respuesta (o texto de error si status==0)
    bool   ok    = false;   // true si status ∈ [200, 299]
};

// ¿Está presente el argumento idx (no none ni nil)? Evita depender de luaL_opt*.
static bool gl_arg_present(lua_State* L, int idx) {
    int t = lua_type(L, idx);
    return t != LUA_TNONE && t != LUA_TNIL;
}

static HTTPClient::Method gl_http_method_from_string(const String& m) {
    String u = m.to_upper();
    if (u == "GET")     return HTTPClient::METHOD_GET;
    if (u == "POST")    return HTTPClient::METHOD_POST;
    if (u == "PUT")     return HTTPClient::METHOD_PUT;
    if (u == "DELETE")  return HTTPClient::METHOD_DELETE;
    if (u == "PATCH")   return HTTPClient::METHOD_PATCH;
    if (u == "HEAD")    return HTTPClient::METHOD_HEAD;
    if (u == "OPTIONS") return HTTPClient::METHOD_OPTIONS;
    return HTTPClient::METHOD_GET;
}

// Convierte una tabla Luau {["Nombre"]="Valor", …} en el formato "Nombre: Valor"
// que espera HTTPClient::request. Ignora claves/valores no representables.
static PackedStringArray gl_lua_headers_to_array(lua_State* L, int idx) {
    PackedStringArray out;
    if (idx < 0) idx = lua_gettop(L) + idx + 1;
    if (!lua_istable(L, idx)) return out;
    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            String key = String(lua_tostring(L, -2));
            String val;
            int vt = lua_type(L, -1);
            if (vt == LUA_TSTRING)      val = String(lua_tostring(L, -1));
            else if (vt == LUA_TNUMBER) val = String::num(lua_tonumber(L, -1));
            else { lua_pop(L, 1); continue; }
            out.push_back(key + ": " + val);
        }
        lua_pop(L, 1);  // deja la clave para el próximo lua_next
    }
    return out;
}

// Petición HTTP síncrona. Parsea la URL (http/https, host[:port]/path), conecta,
// envía y lee el body, con un tope de tiempo total (timeout_sec).
static GLHttpResult gl_http_perform(HTTPClient::Method method, const String& url,
                                    const PackedStringArray& headers, const String& body,
                                    double timeout_sec = 10.0) {
    GLHttpResult res;

    // ── Parsear URL: scheme://host[:port]/path?query ──────────────────────
    String u = url.strip_edges();
    bool use_tls = false;
    if (u.begins_with("https://"))     { use_tls = true;  u = u.substr(8); }
    else if (u.begins_with("http://")) { use_tls = false; u = u.substr(7); }
    else { res.status_msg = "URL sin esquema http(s)://"; res.body = res.status_msg; return res; }

    int slash = u.find("/");
    String authority = (slash == -1) ? u : u.substr(0, slash);
    String path      = (slash == -1) ? String("/") : u.substr(slash);
    if (path.is_empty()) path = "/";

    String host = authority;
    int port = use_tls ? 443 : 80;
    int colon = authority.rfind(":");   // host:puerto (no soporta IPv6 con corchetes)
    if (colon != -1) {
        String p = authority.substr(colon + 1);
        if (p.is_valid_int()) { host = authority.substr(0, colon); port = (int)p.to_int(); }
    }

    // ── Conectar ──────────────────────────────────────────────────────────
    Ref<HTTPClient> client; client.instantiate();
    Ref<TLSOptions> tls = use_tls ? TLSOptions::client() : Ref<TLSOptions>();
    if (client->connect_to_host(host, port, tls) != OK) {
        res.status_msg = "connect_to_host falló"; res.body = res.status_msg; return res;
    }

    OS* os = OS::get_singleton();
    Time* clock = Time::get_singleton();   // en Godot 4 get_ticks_msec vive en Time, no en OS
    uint64_t start = clock->get_ticks_msec();
    uint64_t timeout_ms = (uint64_t)(timeout_sec * 1000.0);

    // Esperar a que la conexión quede establecida
    while (true) {
        client->poll();
        HTTPClient::Status st = client->get_status();
        if (st == HTTPClient::STATUS_CONNECTED) break;
        if (st == HTTPClient::STATUS_RESOLVING || st == HTTPClient::STATUS_CONNECTING) {
            if (clock->get_ticks_msec() - start > timeout_ms) {
                res.status_msg = "Timeout al conectar"; res.body = res.status_msg; return res;
            }
            os->delay_msec(1);
            continue;
        }
        res.status_msg = "No se pudo conectar (status " + String::num_int64((int)st) + ")";
        res.body = res.status_msg; return res;
    }

    // ── Enviar petición ───────────────────────────────────────────────────
    if (client->request(method, path, headers, body) != OK) {
        res.status_msg = "request() falló"; res.body = res.status_msg; return res;
    }
    while (true) {
        client->poll();
        HTTPClient::Status st = client->get_status();
        if (st == HTTPClient::STATUS_BODY || st == HTTPClient::STATUS_CONNECTED) break;
        if (st == HTTPClient::STATUS_REQUESTING) {
            if (clock->get_ticks_msec() - start > timeout_ms) {
                res.status_msg = "Timeout esperando respuesta"; res.body = res.status_msg; return res;
            }
            os->delay_msec(1);
            continue;
        }
        res.status_msg = "Error de conexión en la petición (status " + String::num_int64((int)st) + ")";
        res.body = res.status_msg; return res;
    }

    res.status = client->get_response_code();
    res.ok = (res.status >= 200 && res.status < 300);

    // ── Leer el body por trozos ───────────────────────────────────────────
    PackedByteArray data;
    while (client->get_status() == HTTPClient::STATUS_BODY) {
        client->poll();
        PackedByteArray chunk = client->read_response_body_chunk();
        if (chunk.size() == 0) {
            if (clock->get_ticks_msec() - start > timeout_ms) break;
            os->delay_msec(1);
        } else {
            data.append_array(chunk);
        }
    }
    client->close();

    res.body = data.size() > 0 ? String::utf8((const char*)data.ptr(), (int)data.size()) : String();
    if (res.status_msg.is_empty())
        res.status_msg = res.ok ? "OK" : ("HTTP " + String::num_int64(res.status));
    return res;
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
    Error _reload(bool p_keep_state) override { return OK; }
};

// ════════════════════════════════════════════════════════════════════
//  ScriptNodeBase — Base class for LocalScript, ServerScript, ModuleScript
////  ScriptNodeBase — Base de LocalScript, ServerScript, ModuleScript
// ════════════════════════════════════════════════════════════════════
class ScriptNodeBase : public Node {
    GDCLASS(ScriptNodeBase, Node);

public:
    // Luau thread spawned via task.spawn()
    // timer == -1.0  →  suspended by external condition (WaitForChild, Signal:Wait)
    //// Hilo de Luau spawneado via task.spawn()
    //// timer == -1.0  →  suspendido por condición externa (WaitForChild, Signal:Wait)
    struct SpawnedThread {
        lua_State* L;
        int        ref;
        double     timer;
        int        start_args = 0;     // args en la pila del hilo a pasar en el 1er resume (task.delay/defer)
        bool       started    = true;  // false hasta el 1er resume (delay/defer aún no arrancados)
    };

    // Coroutine waiting for a child to appear in the tree
    //// Coroutina esperando a que aparezca un hijo en el árbol
    struct WaitingChild {
        lua_State* thread;
        lua_State* main_L;
        int        ref;
        uint64_t   parent_id;   // ObjectID del padre (no Node* crudo: el padre puede morir mientras se espera)
        String     child_name;
        double     timeout;   // -1 = sin timeout
        double     elapsed;
        String     trace;          // stack en el momento de la llamada (para el warning)
        bool       warned = false; // "Infinite yield possible" ya impreso
    };

    // Coroutina esperando la respuesta de un RemoteFunction:InvokeServer/Client por
    // red. El call_id la casa con la respuesta que deposita el NetworkService en
    // gl_net_responses(); _process la recoge, decodifica y reanuda el hilo.
    struct WaitingInvoke {
        lua_State* thread;
        lua_State* main_L;
        int        ref;
        int64_t    call_id;
        double     timeout;   // s antes de rendirse (devuelve nil)
        double     elapsed;
    };

private:
    Ref<LuauScript> codigo_luau;
    lua_State* L_main   = nullptr;
    lua_State* L_thread = nullptr;

    double wait_timer      = 0.0;
    bool   is_waiting      = false;
    bool   is_external_wait = false;  // true = no auto-resume via timer
    bool   script_finished = false;

public:
    std::vector<WaitingChild> waiting_children;
    std::vector<SpawnedThread> spawned_threads;
    std::vector<WaitingInvoke> waiting_invokes;

    // Registrar un hilo que cede esperando la respuesta de un InvokeServer/Client
    // por red (mismo mecanismo de suspensión que WaitForChild).
    void add_waiting_invoke(lua_State* th, lua_State* mL, int ref, int64_t call_id, double timeout) {
        waiting_invokes.push_back({th, mL, ref, call_id, timeout, 0.0});
        if (th == L_thread) {
            is_external_wait = true;
        } else {
            for (auto& st : spawned_threads)
                if (st.L == th) { st.timer = -1.0; break; }
        }
    }

    // Called by WaitForChild closures in luau_api.h (or the local __index override)
    void add_waiting_child(lua_State* th, lua_State* mL, int ref,
                           Node* parent, const String& name, double timeout) {
        uint64_t pid = parent ? (uint64_t)parent->get_instance_id() : 0;
        // El stack se captura AQUI (aun no cedimos): es el del caller de WaitForChild
        waiting_children.push_back({th, mL, ref, pid, name, timeout, 0.0,
                                    String::utf8(lua_debugtrace(th)), false});
        // Suspend the coroutine so _process won't auto-resume it via timer
        if (th == L_thread) {
            is_external_wait = true;
        } else {
            for (auto& st : spawned_threads)
                if (st.L == th) { st.timer = -1.0; return; }
        }
    }

    // Resume a coroutine externally (Signal:Wait / WaitForChild resolved).
    // Updates spawned_threads timer or main-thread flags based on new yield.
    void resume_external_thread(lua_State* th, int nargs) {
        int status = lua_resume(th, nullptr, nargs);
        bool is_main = (th == L_thread);

        if (is_main) {
            if (status == LUA_YIELD) {
                is_external_wait = false;
                if (lua_gettop(th) > 0 && lua_isnumber(th, -1)) {
                    wait_timer = lua_tonumber(th, -1);
                    lua_pop(th, 1);
                } else {
                    if (lua_gettop(th) > 0) lua_pop(th, 1);
                    wait_timer = 1e9;  // another external wait set by the call
                }
                is_waiting = true;
            } else {
                is_waiting      = false;
                is_external_wait = false;
                if (status != LUA_OK) gl_report_script_error(th);
                script_finished = true;
            }
        } else {
            for (int i = (int)spawned_threads.size()-1; i >= 0; --i) {
                auto& st = spawned_threads[i];
                if (st.L != th) continue;
                if (status == LUA_YIELD) {
                    if (lua_gettop(th) > 0 && lua_isnumber(th, -1)) {
                        st.timer = lua_tonumber(th, -1);
                        lua_pop(th, 1);
                    } else {
                        if (lua_gettop(th) > 0) lua_pop(th, 1);
                        // another external wait – remains at -1 (set by add_waiting_child)
                    }
                } else {
                    if (status != LUA_OK) gl_report_script_error(th);
                    if (L_main) lua_unref(L_main, st.ref);
                    spawned_threads.erase(spawned_threads.begin() + i);
                }
                return;
            }
        }
    }

    // ID persistente e inmutable del script (ej. "ServerScript_ID_12").
    // Vincula el nodo con su archivo .lua para poder borrarlo/restaurarlo junto al nodo.
    String script_id;

    // Enabled (como Roblox): true por defecto; con false el script NO se
    // ejecuta. Si en runtime vuelve a true y no habia corrido, corre entonces.
    bool script_enabled = true;
    bool script_started = false;

    // Duplicado (Ctrl+D / copiar-pegar) detectado en ENTER_TREE: código heredado
    // del original que se escribe en el archivo NUEVO en vez de un template.
    String _gl_dup_source;
    bool   _gl_is_dup = false;

    // ¿Otro ScriptNodeBase VIVO (distinto de este) ya usa este script_id?
    // Es la señal inequívoca de un DUPLICADO en el editor: Godot copió el
    // script_id exportado y compartió la MISMA Ref<LuauScript> (mismo .lua),
    // por eso "abren el mismo script". Un proyecto recién abierto no dispara
    // esto: cada nodo trae su id único persistido.
    ScriptNodeBase* _gl_find_id_twin() {
        if (!get_tree() || !get_tree()->get_root()) return nullptr;
        ScriptNodeBase* found = nullptr;
        std::function<void(Node*)> rec = [&](Node* n) {
            if (found || !n) return;
            if (n != (Node*)this) {
                ScriptNodeBase* s = Object::cast_to<ScriptNodeBase>(n);
                if (s && !s->script_id.is_empty() && s->script_id == script_id) { found = s; return; }
            }
            for (int i = 0; i < n->get_child_count(); i++) rec(n->get_child(i));
        };
        rec(get_tree()->get_root());
        return found;
    }

public:
    void set_enabled(bool b) {
        script_enabled = b;
        if (script_enabled && !script_started && is_inside_tree()
            && !Engine::get_singleton()->is_editor_hint()) {
            call_deferred("iniciar_corrutina");
        }
    }
    bool get_enabled() const { return script_enabled; }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_codigo_luau", "s"), &ScriptNodeBase::set_codigo_luau);
        ClassDB::bind_method(D_METHOD("get_codigo_luau"), &ScriptNodeBase::get_codigo_luau);
        ClassDB::bind_method(D_METHOD("iniciar_corrutina"), &ScriptNodeBase::iniciar_corrutina);
        ClassDB::bind_method(D_METHOD("set_script_id", "id"), &ScriptNodeBase::set_script_id);
        ClassDB::bind_method(D_METHOD("get_script_id"), &ScriptNodeBase::get_script_id);
        ClassDB::bind_method(D_METHOD("set_enabled", "b"), &ScriptNodeBase::set_enabled);
        ClassDB::bind_method(D_METHOD("get_enabled"), &ScriptNodeBase::get_enabled);
        // Relay de ChildAdded/ChildRemoved: Godot lo invoca con el hijo + el ref guardado.
        ClassDB::bind_method(D_METHOD("_gl_child_event", "child", "ref"), &ScriptNodeBase::_gl_child_event);

        ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "codigo_luau", PROPERTY_HINT_RESOURCE_TYPE, "LuauScript"),
                     "set_codigo_luau", "get_codigo_luau");
        // Como en Roblox Studio: Enabled true por defecto; desmarcado no corre
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "Enabled"), "set_enabled", "get_enabled");
        // Visible en el inspector pero NO editable por el usuario
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "script_id", PROPERTY_HINT_NONE, "",
                     PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY),
                     "set_script_id", "get_script_id");
    }

    // Contador secuencial por tipo en res://.luau_ids.cfg
    // (NO se usa project.godot: guardarlo hace que el editor recargue los plugins)
    // Cacheado en memoria y con los handles SIEMPRE cerrados antes de reabrir:
    // en Windows, leer y escribir el mismo archivo con el handle abierto
    // produce "archivo en uso, bloqueado" cuando entran varios scripts a la vez.
    static int64_t _next_script_counter(const String& cls_lower) {
        static Dictionary counters;
        static bool loaded = false;
        const String path = "res://.luau_ids.cfg";

        if (!loaded) {
            loaded = true;
            Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
            if (f.is_valid()) {
                while (!f->eof_reached()) {
                    String line = f->get_line().strip_edges();
                    int eq = line.find("=");
                    if (eq > 0) counters[line.substr(0, eq)] = line.substr(eq + 1).to_int();
                }
                f->close();  // cerrar ANTES de cualquier escritura posterior
            }
        }
        // Migración: contadores antiguos guardados en project.godot
        if (!counters.has(cls_lower)) {
            String legacy_key = "godot_luau/script_id_counter_" + cls_lower;
            if (ProjectSettings::get_singleton()->has_setting(legacy_key))
                counters[cls_lower] = (int64_t)ProjectSettings::get_singleton()->get_setting(legacy_key, 0);
        }
        int64_t next = (int64_t)counters.get(cls_lower, 0) + 1;
        counters[cls_lower] = next;

        Ref<FileAccess> w = FileAccess::open(path, FileAccess::WRITE);
        if (w.is_valid()) {
            Array keys = counters.keys();
            for (int i = 0; i < keys.size(); i++)
                w->store_line(String(keys[i]) + "=" + String::num_int64((int64_t)counters[keys[i]]));
            w->close();
        }
        return next;
    }

    void _notification(int p_what) {
        if (Engine::get_singleton()->is_editor_hint()) {
            if (p_what == NOTIFICATION_ENTER_TREE) {
                // ── Detección de DUPLICADO (Ctrl+D / copiar-pegar) ─────────
                // Si otro nodo vivo ya usa este script_id, este es una copia:
                // se le da identidad y archivo PROPIOS copiando el código actual
                // del original, para que dejen de "abrir el mismo script" y cada
                // uno ejecute sus funciones de forma independiente.
                if (!script_id.is_empty() && _gl_find_id_twin()) {
                    if (codigo_luau.is_valid()) {
                        _gl_dup_source = codigo_luau->_get_source_code();
                        if (_gl_dup_source.is_empty() && !codigo_luau->get_path().is_empty()) {
                            Ref<FileAccess> rf = FileAccess::open(codigo_luau->get_path(), FileAccess::READ);
                            if (rf.is_valid()) { _gl_dup_source = rf->get_as_text(); rf->close(); }
                        }
                    }
                    _gl_is_dup = true;
                    script_id = String();               // fuerza id nuevo abajo
                    codigo_luau = Ref<LuauScript>();     // suelta el recurso compartido
                }

                // ── Asignar ID persistente la primera vez ──────────────────
                if (script_id.is_empty()) {
                    String cls_id = get_class();
                    script_id = cls_id + "_ID_" + String::num_int64(_next_script_counter(cls_id.to_lower()));
                }

                bool necesita_archivo = codigo_luau.is_null() ||
                    (!codigo_luau->get_path().is_empty() &&
                     !FileAccess::file_exists(codigo_luau->get_path()));

                // ── Restauración (Ctrl+Z): papelera o memoria, sin crear recursos
                //    duplicados (evita "Another resource is loaded from path") ──
                if (necesita_archivo && codigo_luau.is_valid() && !codigo_luau->get_path().is_empty()) {
                    String path  = codigo_luau->get_path();
                    String trash = "res://.luau_trash/" + script_id + ".lua";
                    Ref<DirAccess> root_dir = DirAccess::open("res://");
                    if (root_dir.is_valid() && !root_dir->dir_exists(path.get_base_dir()))
                        root_dir->make_dir_recursive(path.get_base_dir());
                    if (FileAccess::file_exists(trash)) {
                        // El archivo está en la papelera (nodo borrado + Ctrl+Z) → moverlo de vuelta
                        if (DirAccess::rename_absolute(
                                ProjectSettings::get_singleton()->globalize_path(trash),
                                ProjectSettings::get_singleton()->globalize_path(path)) == OK) {
                            GL_DEBUG_PRINT("[GodotLuau] Script restaurado de la papelera: ", path);
                            necesita_archivo = false;
                        }
                    } else if (!codigo_luau->_get_source_code().is_empty()) {
                        // El recurso aún conserva el código en memoria → reescribirlo a disco
                        Ref<FileAccess> wf = FileAccess::open(path, FileAccess::WRITE);
                        if (wf.is_valid()) {
                            wf->store_string(codigo_luau->_get_source_code());
                            GL_DEBUG_PRINT("[GodotLuau] Script reescrito desde memoria: ", path);
                            necesita_archivo = false;
                        }
                    }
                }

                if (necesita_archivo) {
                    String cls = get_class();
                    // Los scripts van DENTRO de GodotLuau/ (orden pedido por el
                    // usuario): res://GodotLuau/LocalScripts, /ServerScripts, etc.
                    String folder = "res://GodotLuau/" + cls + "s";
                    Ref<DirAccess> dir = DirAccess::open("res://");
                    if (dir.is_valid() && !dir->dir_exists(folder)) {
                        dir->make_dir_recursive(folder);
                    }

                    // El archivo se nombra por el ID → estable entre sesiones
                    String file_path = folder + "/" + script_id + ".lua";

                    Ref<LuauScript> new_script;
                    new_script.instantiate();

                    // ── Template selection based on node name ─────────────────
                    // e.g. Health → regen, ControlModule → movement, etc.
                    //// ── Selección de template según nombre del nodo ────────────
                    //// Así Health → regen, ControlModule → movimiento, etc.
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
                        template_code = String(LUAU_TEMPLATE_LOCAL_SCRIPT);
                    }

                    // Un duplicado hereda el código EXACTO del original (no un
                    // template): así la copia conserva lo que el usuario escribió.
                    if (_gl_is_dup && !_gl_dup_source.is_empty())
                        template_code = _gl_dup_source;

                    // Sustituir el ID único del script en la plantilla (para poder
                    // localizarlo al buscar por su ID). Los templates de sistema
                    // (Health/Animate…) no tienen el placeholder → quedan intactos.
                    template_code = template_code.replace("%SCRIPT_ID%", script_id);
                    new_script->_set_source_code(template_code);
                    // take_over_path reclama la ruta aunque haya un recurso viejo cacheado
                    // (evita "Another resource is loaded from path")
                    new_script->take_over_path(file_path);
                    ResourceSaver::get_singleton()->save(new_script, file_path);
                    set_codigo_luau(new_script);
                    GL_DEBUG_PRINT("[GodotLuau] Script creado: ", file_path);
                    if (_gl_is_dup)
                        GL_DEBUG_PRINT("[GodotLuau] Duplicado con identidad propia: ", script_id);
                    _gl_is_dup = false;          // consumido: no repetir en re-entradas
                    _gl_dup_source = String();
                }
            }
            // NOTA: el borrado/restauración del archivo al borrar el nodo (con soporte
            // de Ctrl+Z vía papelera res://.luau_trash/) lo gestiona el addon
            // GodotLuauUpdater, que puede distinguir un borrado real de un cierre de escena.
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
                    gl_run_close_cbs_for(L_main);  // BindToClose: guardar datos antes de cerrar
                    gl_unregister_state(L_main);   // baja del registro ANTES de cerrar la VM
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
            gl_run_close_cbs_for(L_main);   // por si no pasó por EXIT_TREE (ya vacío si sí)
            gl_unregister_state(L_main);
            lua_close(L_main);
            L_main = nullptr;
        }
    }

    void set_codigo_luau(const Ref<LuauScript> &s) { codigo_luau = s; }
    Ref<LuauScript> get_codigo_luau() const { return codigo_luau; }

    void set_script_id(const String &p_id) { script_id = p_id; }
    String get_script_id() const { return script_id; }

    void iniciar_corrutina() {
        if (Engine::get_singleton()->is_editor_hint()) return;
        if (get_class() == "ModuleScript") return;
        // Modelo server-authoritative (1.14.5): un ServerScript solo corre cuando
        // esta ventana es CONFIRMADAMENTE el servidor (gl_net_role()==1). En una
        // sesión de red (cliente O servidor cuyo rol aún se está fijando) se
        // APLAZA y lo ejecuta start_server → gl_run_deferred_server_scripts() con
        // role=1 ya puesto (así el mundo que crea SÍ se replica; y un cliente que
        // nunca es servidor no lo corre — recibe el mundo por replicación). En
        // single-player gl_startup_defer_server()==false → corre de inmediato. No
        // marca script_started para poder re-ejecutarlo al confirmarse el rol.
        if (get_class() == "ServerScript" && gl_net_role() != 1 && gl_startup_defer_server()) {
            uint64_t oid = (uint64_t)get_instance_id();
            std::vector<uint64_t>& dl = gl_deferred_server_scripts();
            bool dup = false;
            for (uint64_t x : dl) if (x == oid) { dup = true; break; }
            if (!dup) dl.push_back(oid);
            GL_DEBUG_PRINT("[GodotLuau] ServerScript '", get_name(), "' aplazado (ventana cliente; el mundo llega por replicacion).");
            return;
        }
        if (!script_enabled) {
            // Enabled=false (como Roblox): no ejecutar. Aviso en Modo Debug para
            // que "mi script no corre y no dice nada" se diagnostique solo.
            GL_DEBUG_PRINT("[GodotLuau] Script '", get_name(), "' NO ejecutado: Enabled = false.");
            return;
        }
        if (script_started) return;    // ya corrio (evita doble arranque al re-habilitar)
        script_started = true;

        if (codigo_luau.is_null()) {
            UtilityFunctions::push_error("[GodotLuau] ", gl_tr3("Node '", "El nodo '", "O nó '"), get_name(), gl_tr3(
                "' has no Luau script assigned.",
                "' no tiene un archivo Luau asignado.",
                "' não tem um script Luau atribuído."));
            return;
        }

        L_main = luaL_newstate();
        gl_register_state(L_main);   // marca esta VM como viva (guard anti use-after-free)
        luaL_openlibs(L_main);

        // Guardar referencias al main state y al ScriptNodeBase en el registro
        lua_pushlightuserdata(L_main, (void*)L_main);
        lua_setfield(L_main, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
        lua_pushlightuserdata(L_main, (void*)this);
        lua_setfield(L_main, LUA_REGISTRYINDEX, "GODOTLUAU_SCRIPT_NODE");

        // ── Contexto cliente/servidor ─────────────────────────────────────────
        // LocalScript = cliente, ServerScript = servidor, ModuleScript = neutro
        lua_pushboolean(L_main, (get_class() == "LocalScript")  ? 1 : 0);
        lua_setglobal(L_main, "__IS_CLIENT");
        lua_pushboolean(L_main, (get_class() == "ServerScript") ? 1 : 0);
        lua_setglobal(L_main, "__IS_SERVER");

        // ── Metatable: GodotObject ────────────────────────────────────────────
        luaL_newmetatable(L_main, "GodotObject");

        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            GodotObjectWrapper* ptr = (GodotObjectWrapper*)lua_touserdata(L, 1);
            String key = luaL_checkstring(L, 2);

            if (ptr && gow_node(ptr)) {
                Node* node = gow_node(ptr);

                if (key == "FindFirstChild") {
                    lua_pushcfunction(L, [](lua_State* pL) -> int {
                        GodotObjectWrapper* pptr = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                        if (!pptr || !gow_node(pptr)) { lua_pushnil(pL); return 1; }
                        const char* cname = luaL_checkstring(pL, 2);
                        bool recursive = lua_isboolean(pL, 3) && lua_toboolean(pL, 3);
                        if (!recursive) {
                            for (int i = 0; i < gow_node(pptr)->get_child_count(); i++) {
                                Node* ch = gow_node(pptr)->get_child(i);
                                if (ch && ch->get_name() == StringName(cname)) { wrap_node(pL, ch); return 1; }
                            }
                        } else {
                            Node* t = gow_node(pptr)->find_child(String(cname), true, false);
                            if (t) { wrap_node(pL, t); return 1; }
                        }
                        lua_pushnil(pL); return 1;
                    }, "FindFirstChild");
                    return 1;
                }
                if (key == "WaitForChild") {
                    lua_pushlightuserdata(L, (void*)node);
                    lua_pushcclosure(L, [](lua_State* pL) -> int {
                        Node* parent = (Node*)lua_touserdata(pL, lua_upvalueindex(1));
                        if (!parent) { lua_pushnil(pL); return 1; }
                        const char* cname = luaL_checkstring(pL, 2);
                        double timeout = lua_isnumber(pL, 3) ? lua_tonumber(pL, 3) : -1.0;
                        // Try immediate find
                        for (int i = 0; i < parent->get_child_count(); i++) {
                            Node* ch = parent->get_child(i);
                            if (ch && ch->get_name() == StringName(cname)) { wrap_node(pL, ch); return 1; }
                        }
                        // Not found — register async wait
                        lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_SCRIPT_NODE");
                        ScriptNodeBase* sn = (ScriptNodeBase*)lua_touserdata(pL, -1);
                        lua_pop(pL, 1);
                        if (!sn) { lua_pushnil(pL); return 1; }
                        lua_getfield(pL, LUA_REGISTRYINDEX, "GODOTLUAU_MAIN_STATE");
                        lua_State* main_L = (lua_State*)lua_touserdata(pL, -1);
                        lua_pop(pL, 1);
                        if (!main_L) main_L = pL;
                        lua_pushthread(pL);
                        int ref = lua_ref(pL, -1);
                        lua_pop(pL, 1);
                        sn->add_waiting_child(pL, main_L, ref, parent, String(cname), timeout);
                        lua_pushstring(pL, "__WAIT_CHILD__");
                        return lua_yield(pL, 1);
                    }, "WaitForChild", 1);
                    return 1;
                }

                if (key == "GetChildren") {
                    lua_pushcfunction(L, [](lua_State* pL) -> int {
                        GodotObjectWrapper* pptr = (GodotObjectWrapper*)lua_touserdata(pL, 1);
                        lua_newtable(pL);
                        if (pptr && gow_node(pptr)) {
                            TypedArray<Node> children = gow_node(pptr)->get_children();
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

        // Replicación (1.14.5): instala el aplicador de propiedades del cliente
        // (reentra al __newindex real). Idempotente; el NetworkService lo llama
        // por puntero al recibir _rep_new/_rep_prop.
        gl_apply_prop_hook() = gl_rep_apply_prop;
        // Hook para que InvokeServer/Client (en luau_api.h, sin ver ScriptNodeBase)
        // registre el hilo que cede esperando la respuesta por red.
        gl_add_invoke_wait_hook() = [](Node* n, lua_State* th, lua_State* mL, int ref, int64_t cid, double to) {
            if (ScriptNodeBase* sn = Object::cast_to<ScriptNodeBase>(n))
                sn->add_waiting_invoke(th, mL, ref, cid, to);
        };

        // ── Math metatables ───────────────────────────────────────────────────
        //// ── Metatables matemáticas ────────────────────────────────────────────
        luaL_newmetatable(L_main, "Vector3");
        lua_pushcfunction(L_main, godot_vector3_index, "__index");   lua_setfield(L_main, -2, "__index");
        lua_pushcfunction(L_main, godot_vector3_add,   "__add");     lua_setfield(L_main, -2, "__add");
        lua_pushcfunction(L_main, godot_vector3_sub,   "__sub");     lua_setfield(L_main, -2, "__sub");
        lua_pushcfunction(L_main, godot_vector3_mul,   "__mul");     lua_setfield(L_main, -2, "__mul");
        lua_pushcfunction(L_main, godot_vector3_div,   "__div");     lua_setfield(L_main, -2, "__div");
        lua_pushcfunction(L_main, godot_vector3_unm,   "__unm");     lua_setfield(L_main, -2, "__unm");
        lua_pushcfunction(L_main, godot_vector3_eq,    "__eq");      lua_setfield(L_main, -2, "__eq");
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
            // Buscar nodo "game": RobloxTemplate, RobloxGame3D, RobloxGame2D,
            // o cualquier nodo llamado "Game"
            for (int _i = 0; _i < root->get_child_count() && !game_node; _i++) {
                Node* ch = root->get_child(_i);
                if (ch && (ch->is_class("RobloxTemplate") ||
                            ch->is_class("RobloxGame3D")   ||
                            ch->is_class("RobloxGame2D")   ||
                            ch->get_name() == StringName("Game"))) {
                    game_node = ch;
                } else if (ch) {
                    for (int _j = 0; _j < ch->get_child_count() && !game_node; _j++) {
                        Node* gc = ch->get_child(_j);
                        if (gc && (gc->is_class("RobloxTemplate") ||
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
            return lua_yield(L, 1);   // Luau: hay que DEVOLVER lua_yield para que ceda de verdad
        }, "wait");
        lua_setfield(L_main, -2, "wait");

        // task.spawn(f, ...) — corre f(...) YA en un hilo; devuelve el hilo (como Roblox)
        lua_pushlightuserdata(L_main, (void*)this);
        lua_pushcclosure(L_main, [](lua_State* L) -> int {
            ScriptNodeBase* self = static_cast<ScriptNodeBase*>(lua_touserdata(L, lua_upvalueindex(1)));
            if (!self || !lua_isfunction(L, 1)) return 0;

            int base = lua_gettop(L);                 // [func(1), args(2..base)]
            lua_State* NL = lua_newthread(L);          // NL en la cima
            int nl_idx = lua_gettop(L);
            lua_pushvalue(L, 1); lua_xmove(L, NL, 1);  // función → hilo
            int n_args = base - 1;
            for (int i = 2; i <= base; i++) lua_pushvalue(L, i);
            if (n_args > 0) lua_xmove(L, NL, n_args);

            int status = lua_resume(NL, nullptr, n_args);
            int thread_ref = lua_ref(L, nl_idx);       // ref DESPUES del resume (sin sacarlo de la pila)

            if (status == LUA_YIELD) {
                double wait_time = 0.0;
                if (lua_gettop(NL) > 0) {
                    // Numero = task.wait(n); sentinel (__WAIT_CHILD__...) = espera
                    // externa: timer -1 para que _process no lo re-resuma solo.
                    wait_time = lua_isnumber(NL, -1) ? lua_tonumber(NL, -1) : -1.0;
                    lua_pop(NL, 1);
                }
                SpawnedThread st; st.L = NL; st.ref = thread_ref; st.timer = wait_time; st.started = true;
                self->spawned_threads.push_back(st);
            } else {
                if (status != LUA_OK) gl_report_script_error(NL);
                if (self->L_main) lua_unref(self->L_main, thread_ref);
            }
            lua_settop(L, nl_idx);                     // devolver el hilo
            return 1;
        }, "spawn", 1);
        lua_setfield(L_main, -2, "spawn");

        // task.delay(n, f, ...) — corre f(...) tras n segundos; devuelve el hilo
        lua_pushlightuserdata(L_main, (void*)this);
        lua_pushcclosure(L_main, [](lua_State* L) -> int {
            ScriptNodeBase* self = static_cast<ScriptNodeBase*>(lua_touserdata(L, lua_upvalueindex(1)));
            if (!self) return 0;
            double delay_time = luaL_checknumber(L, 1);
            if (!lua_isfunction(L, 2)) return 0;

            int base = lua_gettop(L);                 // [delay(1), func(2), args(3..base)]
            lua_State* NL = lua_newthread(L);
            int nl_idx = lua_gettop(L);
            lua_pushvalue(L, 2); lua_xmove(L, NL, 1);  // función → hilo
            int n_args = base - 2;
            for (int i = 3; i <= base; i++) lua_pushvalue(L, i);
            if (n_args > 0) lua_xmove(L, NL, n_args);

            int thread_ref = lua_ref(L, nl_idx);
            SpawnedThread st; st.L = NL; st.ref = thread_ref; st.timer = delay_time;
            st.start_args = n_args; st.started = false;
            self->spawned_threads.push_back(st);
            lua_settop(L, nl_idx);
            return 1;
        }, "delay", 1);
        lua_setfield(L_main, -2, "delay");

        // task.defer(f, ...) — encola f(...) para el final del frame actual; devuelve el hilo
        lua_pushlightuserdata(L_main, (void*)this);
        lua_pushcclosure(L_main, [](lua_State* L) -> int {
            ScriptNodeBase* self = static_cast<ScriptNodeBase*>(lua_touserdata(L, lua_upvalueindex(1)));
            if (!self || !lua_isfunction(L, 1)) return 0;

            int base = lua_gettop(L);                 // [func(1), args(2..base)]
            lua_State* NL = lua_newthread(L);
            int nl_idx = lua_gettop(L);
            lua_pushvalue(L, 1); lua_xmove(L, NL, 1);
            int n_args = base - 1;
            for (int i = 2; i <= base; i++) lua_pushvalue(L, i);
            if (n_args > 0) lua_xmove(L, NL, n_args);

            int thread_ref = lua_ref(L, nl_idx);
            SpawnedThread st; st.L = NL; st.ref = thread_ref; st.timer = 0.0;  // 0 → próximo _process
            st.start_args = n_args; st.started = false;
            self->spawned_threads.push_back(st);
            lua_settop(L, nl_idx);
            return 1;
        }, "defer", 1);
        lua_setfield(L_main, -2, "defer");

        // task.cancel(thread) — cancela una corrutina spawneada
        lua_pushlightuserdata(L_main, (void*)this);
        lua_pushcclosure(L_main, [](lua_State* L) -> int {
            ScriptNodeBase* self = static_cast<ScriptNodeBase*>(lua_touserdata(L, lua_upvalueindex(1)));
            if (!self) return 0;
            lua_State* target = nullptr;
            if (lua_isthread(L, 1)) target = lua_tothread(L, 1);
            if (!target) return 0;
            for (int i = (int)self->spawned_threads.size()-1; i >= 0; --i) {
                if (self->spawned_threads[i].L == target) {
                    if (self->L_main) lua_unref(self->L_main, self->spawned_threads[i].ref);
                    self->spawned_threads.erase(self->spawned_threads.begin() + i);
                    return 0;
                }
            }
            return 0;
        }, "cancel", 1);
        lua_setfield(L_main, -2, "cancel");

        lua_setglobal(L_main, "task");

        // ── print / warn / error ──────────────────────────────────────────────
        // Visible por defecto; se desactiva con godot_luau/script_output = false
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            if (!(bool)ProjectSettings::get_singleton()->get_setting("godot_luau/script_output", true))
                return 0;
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
            if (!(bool)ProjectSettings::get_singleton()->get_setting("godot_luau/script_output", true))
                return 0;
            int n = lua_gettop(L);
            String msg;
            for (int i = 1; i <= n; i++) {
                msg += String(luaL_tolstring(L, i, nullptr));
                if (i < n) msg += "\t";
            }
            UtilityFunctions::push_warning(msg);   // canal de advertencia real (antes era print)
            return 0;
        }, "warn");
        lua_setglobal(L_main, "warn");

        // ── dprint / dwarn: salida de los scripts internos del sistema ────────
        // Solo imprimen si godot_luau/debug_mode esta activo (Modo Debug).
        // Los templates del sistema (PlayerModule, Health, Chat...) los usan
        // para no ensuciar la consola del usuario.
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            if (!(bool)ProjectSettings::get_singleton()->get_setting("godot_luau/debug_mode", false))
                return 0;
            int n = lua_gettop(L);
            String msg;
            for (int i = 1; i <= n; i++) {
                msg += String(luaL_tolstring(L, i, nullptr));
                if (i < n) msg += "\t";
            }
            UtilityFunctions::print("[Luau Debug] ", msg);
            return 0;
        }, "dprint");
        lua_setglobal(L_main, "dprint");

        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            if (!(bool)ProjectSettings::get_singleton()->get_setting("godot_luau/debug_mode", false))
                return 0;
            int n = lua_gettop(L);
            String msg;
            for (int i = 1; i <= n; i++) {
                msg += String(luaL_tolstring(L, i, nullptr));
                if (i < n) msg += "\t";
            }
            UtilityFunctions::print("[Luau Debug WARN] ", msg);
            return 0;
        }, "dwarn");
        lua_setglobal(L_main, "dwarn");

        // error(message, level) — como en Lua/Roblox: level 0 omite la posición
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            int level = (int)luaL_optinteger(L, 2, 1);
            lua_settop(L, 1);
            if (lua_isstring(L, 1) && level > 0) {
                lua_Debug ar;
                if (lua_getinfo(L, level, "sl", &ar) && ar.currentline > 0) {
                    lua_pushfstring(L, "%s:%d: %s", ar.short_src, ar.currentline, lua_tostring(L, 1));
                    lua_replace(L, 1);
                }
            }
            lua_error(L);
            return 0;
        }, "error");
        lua_setglobal(L_main, "error");

        // ── wait (alias global de task.wait) ─────────────────────────────────
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            double sec = luaL_optnumber(L, 1, 0.0);
            lua_pushnumber(L, sec);
            return lua_yield(L, 1);   // Luau: hay que DEVOLVER lua_yield para que ceda de verdad
        }, "wait");
        lua_setglobal(L_main, "wait");

        // ── tick — tiempo Unix actual (segundos) ──────────────────────────────
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            lua_pushnumber(L, (double)Time::get_singleton()->get_unix_time_from_system());
            return 1;
        }, "tick");
        lua_setglobal(L_main, "tick");

        // ── time — game execution time (seconds) ─────────────────────────────
        //// ── time — tiempo de ejecución del juego (segundos) ───────────────────
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
            // Tipos basados en tabla (Vector2, UDim2, CFrame, TweenInfo...) llevan
            // un campo __type en su metatable (lo pone la stdlib) → como Roblox.
            if (t == LUA_TTABLE && lua_getmetatable(L, 1)) {
                lua_getfield(L, -1, "__type");
                if (lua_isstring(L, -1)) { const char* tn = lua_tostring(L, -1); lua_pushstring(L, tn); return 1; }
                lua_pop(L, 2);
            }
            lua_pushstring(L, lua_typename(L, t));
            return 1;
        }, "typeof");
        lua_setglobal(L_main, "typeof");

        // ── pcall mejorado (ya viene de luaL_openlibs pero lo sobreescribimos) ─
        // El pcall nativo de Luau ya funciona correctamente, solo lo dejamos.

        // ── tostring / tonumber ───────────────────────────────────────────────
        // luaL_tolstring respeta __tostring y SIEMPRE deja un string en la cima
        // (el lua_tostring anterior devolvía la tabla/nil tal cual para no-strings).
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            luaL_tolstring(L, 1, nullptr);
            return 1;
        }, "tostring");
        lua_setglobal(L_main, "tostring");

        // tonumber(v, [base]) — soporta base (2..36) y hex 0x; rechaza basura final.
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            if (!lua_isnoneornil(L, 2)) {
                int base = (int)luaL_checkinteger(L, 2);
                const char* s = luaL_checkstring(L, 1);
                if (base < 2 || base > 36) { lua_pushnil(L); return 1; }
                char* endp = nullptr;
                long val = strtol(s, &endp, base);
                // saltar espacios finales
                while (endp && (*endp == ' ' || *endp == '\t' || *endp == '\n')) endp++;
                if (endp && *endp == '\0' && endp != s) { lua_pushnumber(L, (double)val); return 1; }
                lua_pushnil(L); return 1;
            }
            if (lua_type(L, 1) == LUA_TNUMBER) { lua_pushvalue(L, 1); return 1; }
            if (lua_type(L, 1) == LUA_TSTRING) {
                const char* s = luaL_checkstring(L, 1);
                char* endp = nullptr;
                double n = strtod(s, &endp);
                while (endp && (*endp == ' ' || *endp == '\t' || *endp == '\n')) endp++;
                if (endp && *endp == '\0' && endp != s) { lua_pushnumber(L, n); return 1; }
            }
            lua_pushnil(L); return 1;
        }, "tonumber");
        lua_setglobal(L_main, "tonumber");

        // ── UserInputService ─────────────────────────────────────────────────
        lua_newtable(L_main);

        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            int n = lua_gettop(L);
            int arg = (n >= 2) ? 2 : 1;
            Input* inp = Input::get_singleton();
            Key code = KEY_UNKNOWN;

            // Accepts Enum.KeyCode (number) OR legacy string ("LeftShift", "W", etc.)
            //// Acepta Enum.KeyCode (número) O string legacy ("LeftShift", "W", etc.)
            if (lua_type(L, arg) == LUA_TNUMBER) {
                code = roblox_keycode_to_godot((int)lua_tonumber(L, arg));
            } else {
                const char* key_name = luaL_checkstring(L, arg);
                if      (strcmp(key_name, "W") == 0 || strcmp(key_name, "Up") == 0)       code = KEY_W;
                else if (strcmp(key_name, "A") == 0 || strcmp(key_name, "Left") == 0)     code = KEY_A;
                else if (strcmp(key_name, "S") == 0 || strcmp(key_name, "Down") == 0)     code = KEY_S;
                else if (strcmp(key_name, "D") == 0 || strcmp(key_name, "Right") == 0)    code = KEY_D;
                else if (strcmp(key_name, "Space") == 0)     code = KEY_SPACE;
                else if (strcmp(key_name, "E") == 0)         code = KEY_E;
                else if (strcmp(key_name, "Q") == 0)         code = KEY_Q;
                else if (strcmp(key_name, "F") == 0)         code = KEY_F;
                else if (strcmp(key_name, "G") == 0)         code = KEY_G;
                else if (strcmp(key_name, "H") == 0)         code = KEY_H;
                else if (strcmp(key_name, "I") == 0)         code = KEY_I;
                else if (strcmp(key_name, "J") == 0)         code = KEY_J;
                else if (strcmp(key_name, "K") == 0)         code = KEY_K;
                else if (strcmp(key_name, "L") == 0)         code = KEY_L;
                else if (strcmp(key_name, "M") == 0)         code = KEY_M;
                else if (strcmp(key_name, "N") == 0)         code = KEY_N;
                else if (strcmp(key_name, "O") == 0)         code = KEY_O;
                else if (strcmp(key_name, "P") == 0)         code = KEY_P;
                else if (strcmp(key_name, "R") == 0)         code = KEY_R;
                else if (strcmp(key_name, "T") == 0)         code = KEY_T;
                else if (strcmp(key_name, "U") == 0)         code = KEY_U;
                else if (strcmp(key_name, "V") == 0)         code = KEY_V;
                else if (strcmp(key_name, "X") == 0)         code = KEY_X;
                else if (strcmp(key_name, "Y") == 0)         code = KEY_Y;
                else if (strcmp(key_name, "Z") == 0)         code = KEY_Z;
                else if (strcmp(key_name, "C") == 0)         code = KEY_C;
                else if (strcmp(key_name, "B") == 0)         code = KEY_B;
                else if (strcmp(key_name, "0") == 0)         code = KEY_0;
                else if (strcmp(key_name, "1") == 0)         code = KEY_1;
                else if (strcmp(key_name, "2") == 0)         code = KEY_2;
                else if (strcmp(key_name, "3") == 0)         code = KEY_3;
                else if (strcmp(key_name, "4") == 0)         code = KEY_4;
                else if (strcmp(key_name, "5") == 0)         code = KEY_5;
                else if (strcmp(key_name, "6") == 0)         code = KEY_6;
                else if (strcmp(key_name, "7") == 0)         code = KEY_7;
                else if (strcmp(key_name, "8") == 0)         code = KEY_8;
                else if (strcmp(key_name, "9") == 0)         code = KEY_9;
                else if (strcmp(key_name, "LeftShift")   == 0 || strcmp(key_name, "RightShift")   == 0) code = KEY_SHIFT;
                else if (strcmp(key_name, "LeftControl") == 0 || strcmp(key_name, "RightControl") == 0) code = KEY_CTRL;
                else if (strcmp(key_name, "LeftAlt")     == 0 || strcmp(key_name, "RightAlt")     == 0) code = KEY_ALT;
                else if (strcmp(key_name, "Return")      == 0 || strcmp(key_name, "Enter")        == 0) code = KEY_ENTER;
                else if (strcmp(key_name, "Escape")      == 0) code = KEY_ESCAPE;
                else if (strcmp(key_name, "Tab")         == 0) code = KEY_TAB;
                else if (strcmp(key_name, "Backspace")   == 0) code = KEY_BACKSPACE;
                else if (strcmp(key_name, "Delete")      == 0) code = KEY_DELETE;
                else if (strcmp(key_name, "Insert")      == 0) code = KEY_INSERT;
                else if (strcmp(key_name, "Home")        == 0) code = KEY_HOME;
                else if (strcmp(key_name, "End")         == 0) code = KEY_END;
                else if (strcmp(key_name, "PageUp")      == 0) code = KEY_PAGEUP;
                else if (strcmp(key_name, "PageDown")    == 0) code = KEY_PAGEDOWN;
                else if (strcmp(key_name, "Up")          == 0) code = KEY_UP;
                else if (strcmp(key_name, "Down")        == 0) code = KEY_DOWN;
                else if (strcmp(key_name, "Left")        == 0) code = KEY_LEFT;
                else if (strcmp(key_name, "Right")       == 0) code = KEY_RIGHT;
                else if (strcmp(key_name, "F1")  == 0) code = KEY_F1;
                else if (strcmp(key_name, "F2")  == 0) code = KEY_F2;
                else if (strcmp(key_name, "F3")  == 0) code = KEY_F3;
                else if (strcmp(key_name, "F4")  == 0) code = KEY_F4;
                else if (strcmp(key_name, "F5")  == 0) code = KEY_F5;
                else if (strcmp(key_name, "F6")  == 0) code = KEY_F6;
                else if (strcmp(key_name, "F7")  == 0) code = KEY_F7;
                else if (strcmp(key_name, "F8")  == 0) code = KEY_F8;
                else if (strcmp(key_name, "F9")  == 0) code = KEY_F9;
                else if (strcmp(key_name, "F10") == 0) code = KEY_F10;
                else if (strcmp(key_name, "F11") == 0) code = KEY_F11;
                else if (strcmp(key_name, "F12") == 0) code = KEY_F12;
            }

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

        // Un ServerScript no puede leer entrada de hardware (igual que Roblox):
        // sustituimos la tabla por un proxy que lanza un error claro al usarse.
        if (get_class() == "ServerScript") {
            lua_pop(L_main, 1);  // descartar la tabla real de UserInputService
            lua_newtable(L_main);
            lua_newtable(L_main);  // metatable
            lua_pushcfunction(L_main, [](lua_State* L) -> int {
                luaL_error(L, "UserInputService no esta disponible en un ServerScript: "
                              "la entrada (teclado/raton/tactil) solo existe en el cliente (LocalScript).");
                return 0;
            }, "__index");
            lua_setfield(L_main, -2, "__index");
            lua_setmetatable(L_main, -2);
        }
        lua_setglobal(L_main, "UserInputService");

        // ── require ───────────────────────────────────────────────────────────
        setup_require_system(L_main);
        setup_remotefunction_system(L_main);

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

        // ── _JSON — JSON serialization ────────────────────────────────────────
        //// ── _JSON — serialización JSON ────────────────────────────────────────
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

        // ── _HTTP — cliente HTTP real (bloqueante, vía HTTPClient) ────────────
        lua_newtable(L_main);
        // _HTTP.request(method, url, body?, headers?) → {Success,StatusCode,StatusMessage,Body}
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            String method = gl_arg_present(L, 1) ? String(luaL_checkstring(L, 1)) : String("GET");
            const char* url = luaL_checkstring(L, 2);
            String body = gl_arg_present(L, 3) ? String(luaL_checkstring(L, 3)) : String();
            PackedStringArray headers = gl_lua_headers_to_array(L, 4);
            GLHttpResult r = gl_http_perform(gl_http_method_from_string(method), String(url), headers, body);
            lua_newtable(L);
            lua_pushboolean(L, r.ok ? 1 : 0);                  lua_setfield(L, -2, "Success");
            lua_pushnumber(L, (double)r.status);               lua_setfield(L, -2, "StatusCode");
            lua_pushstring(L, r.status_msg.utf8().get_data()); lua_setfield(L, -2, "StatusMessage");
            lua_pushstring(L, r.body.utf8().get_data());       lua_setfield(L, -2, "Body");
            return 1;
        }, "request");  lua_setfield(L_main, -2, "request");
        // _HTTP.get(url, headers?) → body
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            const char* url = luaL_checkstring(L, 1);
            PackedStringArray headers = gl_lua_headers_to_array(L, 2);
            GLHttpResult r = gl_http_perform(HTTPClient::METHOD_GET, String(url), headers, String());
            lua_pushstring(L, r.body.utf8().get_data());
            return 1;
        }, "get");     lua_setfield(L_main, -2, "get");
        // _HTTP.post(url, body?, contentType?, headers?) → body
        lua_pushcfunction(L_main, [](lua_State* L) -> int {
            const char* url = luaL_checkstring(L, 1);
            String body  = gl_arg_present(L, 2) ? String(luaL_checkstring(L, 2)) : String();
            String ctype = gl_arg_present(L, 3) ? String(luaL_checkstring(L, 3)) : String("application/json");
            PackedStringArray headers = gl_lua_headers_to_array(L, 4);
            headers.push_back(String("Content-Type: ") + ctype);
            GLHttpResult r = gl_http_perform(HTTPClient::METHOD_POST, String(url), headers, body);
            lua_pushstring(L, r.body.utf8().get_data());
            return 1;
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

        // Chunkname = ruta completa estilo Roblox ("ServerScriptService.Server"):
        // asi los errores salen como alla ("ServerScriptService.Server:5: ...").
        // El prefijo '=' evita que Luau lo envuelva en [string "..."].
        String chunkname = String("=") + gl_instance_fullname(this);
        if (luau_load(L_thread, chunkname.utf8().get_data(), bytecode.data(), bytecode.size(), 0) == 0) {
            resume();
        } else {
            // Error de sintaxis: linea roja identica a Roblox, sin stack.
            gl_report_error_line(String::utf8(lua_tostring(L_thread, -1)));
        }
    }

    void setup_require_system(lua_State* L) {
        lua_pushcfunction(L, [](lua_State* L) -> int {
            // Mensajes EXACTOS de Roblox en cada caso de require().
            GodotObjectWrapper* wrapper = (GodotObjectWrapper*)lua_touserdata(L, 1);
            if (!wrapper || !gow_node(wrapper)) {
                luaL_error(L, "Attempted to call require with invalid argument(s).");
                return 0;
            }

            Node* mod_node = gow_node(wrapper);
            if (mod_node->get_class() != "ModuleScript") {
                luaL_error(L, "Attempted to call require with invalid argument(s).");
                return 0;
            }

            String cache_id = "CACHED_MOD_" + String::num(mod_node->get_instance_id());
            lua_getfield(L, LUA_REGISTRYINDEX, cache_id.utf8().get_data());
            if (!lua_isnil(L, -1)) { lua_pushboolean(L, 1); lua_insert(L, -2); return 2; }   // (true, valorCacheado)
            lua_pop(L, 1);

            ScriptNodeBase* mod_ptr = Object::cast_to<ScriptNodeBase>(mod_node);
            if (!mod_ptr) { luaL_error(L, "Attempted to call require with invalid argument(s)."); return 0; }

            Ref<LuauScript> res = mod_ptr->get_codigo_luau();
            if (res.is_null()) {
                // Modulo sin codigo = modulo que no devuelve nada (como en Roblox)
                luaL_error(L, "Module code did not return exactly one value");
                return 0;
            }

            std::string bc = Luau::compile(res->_get_source_code().utf8().get_data());
            String mod_chunkname = String("=") + gl_instance_fullname(mod_node);
            if (luau_load(L, mod_chunkname.utf8().get_data(), bc.data(), bc.size(), 0) != 0) {
                // Como Roblox: se imprime el error de sintaxis del modulo (rojo) y
                // el require lanza su error generico en la linea del caller.
                gl_report_error_line(String::utf8(lua_tostring(L, -1)));
                lua_pop(L, 1);
                luaL_error(L, "Requested module experienced an error while loading");
                return 0;
            }

            // La EJECUCIÓN del cuerpo del módulo la hace el wrapper Lua `require`
            // con pcall (yieldable en Luau) — aquí solo devolvemos la función
            // cargada. Correrla desde C con lua_pcall rompía cualquier yield
            // interno (WaitForChild/task.wait) con "yield across C-call boundary".
            // stack: [target, moduleFn]
            lua_pushboolean(L, 0);                            // isCached = false
            lua_insert(L, -2);                                // [target, false, moduleFn]
            lua_pushstring(L, cache_id.utf8().get_data());    // cacheKey
            wrap_node(L, mod_node);                           // modScript (para el 'script' del módulo)
            return 4;                                         // (false, moduleFn, cacheKey, modScript)
        }, "__gl_req_load");
        lua_setglobal(L, "__gl_req_load");

        // Guardar el valor devuelto por un módulo en la caché de la VM.
        lua_pushcfunction(L, [](lua_State* L) -> int {
            const char* key = luaL_checkstring(L, 1);
            lua_pushvalue(L, 2);
            lua_setfield(L, LUA_REGISTRYINDEX, key);
            return 0;
        }, "__gl_req_store");
        lua_setglobal(L, "__gl_req_store");

        // Reportar en rojo (como Roblox) el error de un módulo, con su ubicación.
        lua_pushcfunction(L, [](lua_State* L) -> int {
            gl_report_error_line(String::utf8(luaL_optstring(L, 1, "")));
            return 0;
        }, "__gl_req_err");
        lua_setglobal(L, "__gl_req_err");

        // `require` en LUA: corre el cuerpo del módulo con pcall (yieldable), así
        // WaitForChild/task.wait DENTRO de un módulo ceden como corrutina normal.
        static const char* GL_REQUIRE_LUA =
            "local _load, _store, _err = __gl_req_load, __gl_req_store, __gl_req_err\n"
            "__gl_req_load, __gl_req_store, __gl_req_err = nil, nil, nil\n"
            "function require(target)\n"
            "  local cached, fn, key, mod = _load(target)\n"
            "  if cached then return fn end\n"
            "  local prev = script\n"
            "  script = mod\n"
            "  local ok, result = pcall(fn)\n"
            "  script = prev\n"
            "  if not ok then _err(tostring(result)) error('Requested module experienced an error while loading', 0) end\n"
            "  if result == nil then error('Module code did not return exactly one value', 0) end\n"
            "  _store(key, result)\n"
            "  return result\n"
            "end\n";
        std::string _rbc = Luau::compile(GL_REQUIRE_LUA);
        if (luau_load(L, "=__gl_require", _rbc.data(), _rbc.size(), 0) == 0) {
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) { UtilityFunctions::print("[GodotLuau require setup] ", lua_tostring(L, -1)); lua_pop(L, 1); }
        } else { UtilityFunctions::print("[GodotLuau require syntax] ", lua_tostring(L, -1)); lua_pop(L, 1); }
    }

    // RemoteFunction por red (1.14.6): corre el handler (OnServerInvoke/
    // OnClientInvoke) en la máquina remota con soporte de yield y devuelve el
    // resultado al invocador. El invocador cede su hilo (add_waiting_invoke) y
    // _process lo reanuda con la respuesta (gl_net_responses).
    void setup_remotefunction_system(lua_State* L) {
        // C: tras correr el handler, empaqueta sus retornos y pide al
        // NetworkService que los mande al invocador. (callId, replyPeer, ok, ...rets)
        lua_pushcfunction(L, [](lua_State* L) -> int {
            int64_t call_id = (int64_t)luaL_checknumber(L, 1);
            int     reply_peer = (int)luaL_checknumber(L, 2);
            bool    ok = lua_toboolean(L, 3) != 0;
            Array rets;
            int top = lua_gettop(L);
            for (int i = 4; i <= top; i++) rets.push_back(gl_net_encode(L, i));
            if (Node* ns = Object::cast_to<Node>(ObjectDB::get_instance(gl_net_service_id())))
                ns->call("net_rf_respond", reply_peer, (int)call_id, ok, rets);
            return 0;
        }, "__gl_rf_respond");
        lua_setglobal(L, "__gl_rf_respond");

        // Lua: corre el handler en un task.spawn (así puede ceder: WaitForChild,
        // task.wait, DataStore...) y al terminar responde. isServer decide si el
        // handler recibe el Player (OnServerInvoke(player,...) vs OnClientInvoke(...)).
        static const char* GL_RF_LUA =
            "function __gl_rf_serve(handler, player, callId, replyPeer, isServer, ...)\n"
            "  local args = table.pack(...)\n"
            "  task.spawn(function()\n"
            "    local packed\n"
            "    if isServer then packed = table.pack(pcall(handler, player, table.unpack(args, 1, args.n)))\n"
            "    else packed = table.pack(pcall(handler, table.unpack(args, 1, args.n))) end\n"
            "    if not packed[1] then warn('RemoteFunction handler error: ' .. tostring(packed[2])) end\n"
            "    __gl_rf_respond(callId, replyPeer, table.unpack(packed, 1, packed.n))\n"
            "  end)\n"
            "end\n";
        std::string bc = Luau::compile(GL_RF_LUA);
        if (luau_load(L, "=__gl_rf", bc.data(), bc.size(), 0) == 0) {
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) { UtilityFunctions::print("[GodotLuau rf setup] ", lua_tostring(L, -1)); lua_pop(L, 1); }
        } else { UtilityFunctions::print("[GodotLuau rf syntax] ", lua_tostring(L, -1)); lua_pop(L, 1); }
    }

    void resume() {
        int status = lua_resume(L_thread, nullptr, 0);
        if (status == LUA_YIELD) {
            if (lua_gettop(L_thread) > 0) {
                if (lua_isnumber(L_thread, -1)) {
                    wait_timer = lua_tonumber(L_thread, -1);
                    is_external_wait = false;
                } else {
                    // Sentinel yield (__WAIT_CHILD__, __WAIT_SIGNAL__, etc.)
                    // is_external_wait already set by add_waiting_child / Signal:Wait
                    wait_timer = 1e9;
                }
                lua_pop(L_thread, 1);
            } else {
                wait_timer = 0.0;
                is_external_wait = false;
            }
            is_waiting = true;
        } else if (status != LUA_OK) {
            gl_report_script_error(L_thread);
            script_finished = true;
        } else {
            script_finished = true;
        }
    }

    // Relay de ChildAdded/ChildRemoved. Godot lo conecta a child_entered_tree /
    // child_exiting_tree del nodo objetivo y lo desconecta solo al liberar
    // cualquiera de los dos extremos (sin fugas ni use-after-free). El ref es la
    // función Luau conectada, guardada en el registro de esta VM.
    void _gl_child_event(Node* child, int ref) {
        if (!gl_state_alive(L_main) || !child) return;
        lua_State* th = lua_newthread(L_main);
        lua_rawgeti(L_main, LUA_REGISTRYINDEX, ref);
        if (lua_isfunction(L_main, -1)) {
            lua_xmove(L_main, th, 1);
            wrap_node(th, child);
            int status = lua_resume(th, nullptr, 1);
            if (status != LUA_OK && status != LUA_YIELD) gl_report_script_error(th);
        } else {
            lua_pop(L_main, 1);
        }
        lua_pop(L_main, 1);
    }

    void _process(double delta) override {
        // ── 1. Main thread timer ────────────────────────────────────────
        if (!script_finished && is_waiting && !is_external_wait) {
            wait_timer -= delta;
            if (wait_timer <= 0.0) {
                is_waiting = false;
                resume();
            }
        }

        // ── 2. Spawned threads ──────────────────────────────────────────
        for (int i = (int)spawned_threads.size() - 1; i >= 0; i--) {
            SpawnedThread& st = spawned_threads[i];
            if (st.timer < 0.0) continue;  // suspended by external condition
            st.timer -= delta;

            if (st.timer <= 0.0) {
                // 1er resume de task.delay/defer: pasar sus args; despues, 0.
                int na = st.started ? 0 : st.start_args;
                st.started = true;
                int status = lua_resume(st.L, nullptr, na);

                if (status == LUA_YIELD) {
                    if (lua_gettop(st.L) > 0) {
                        if (lua_isnumber(st.L, -1)) {
                            st.timer = lua_tonumber(st.L, -1);
                        } else {
                            // Sentinel yield — mark as suspended (externally managed)
                            st.timer = -1.0;
                        }
                        lua_pop(st.L, 1);
                    } else {
                        st.timer = 0.0;
                    }
                } else {
                    if (status != LUA_OK) gl_report_script_error(st.L);
                    if (L_main) lua_unref(L_main, st.ref);
                    spawned_threads.erase(spawned_threads.begin() + i);
                }
            }
        }

        // ── 3. Poll WaitForChild entries ────────────────────────────────
        for (int i = (int)waiting_children.size()-1; i >= 0; --i) {
            WaitingChild& wc = waiting_children[i];
            wc.elapsed += delta;
            Node* found = nullptr;
            // Resolver el padre por ObjectID: nullptr si fue destruido (sin UAF).
            Node* parent = Object::cast_to<Node>(ObjectDB::get_instance(wc.parent_id));
            bool parent_gone = (wc.parent_id != 0 && parent == nullptr);
            if (parent && parent->is_inside_tree()) {
                for (int ci = 0; ci < parent->get_child_count(); ci++) {
                    Node* ch = parent->get_child(ci);
                    if (ch && ch->get_name() == StringName(wc.child_name)) { found = ch; break; }
                }
            }
            // Warning de Roblox: WaitForChild sin timeout que lleva 5s sin
            // resolverse — el clasico script colgado "en silencio".
            if (!found && !parent_gone && wc.timeout <= 0 && !wc.warned && wc.elapsed >= 5.0) {
                wc.warned = true;
                String pname = parent ? gl_instance_fullname(parent) : String(get_name());
                UtilityFunctions::print_rich(String("[color=") + GL_WARN_COLOR +
                    "]Infinite yield possible on '" + pname + ":WaitForChild(\"" + wc.child_name + "\")'[/color]");
                UtilityFunctions::print_rich(gl_format_roblox_stack(wc.trace));
            }
            // Si el padre desaparece, no dejamos la corrutina colgada para siempre.
            bool timed_out = (wc.timeout > 0 && wc.elapsed >= wc.timeout) || parent_gone;
            if (found || timed_out) {
                if (found) { wrap_node(wc.thread, found); resume_external_thread(wc.thread, 1); }
                else       { lua_pushnil(wc.thread);       resume_external_thread(wc.thread, 1); }
                if (wc.main_L) lua_unref(wc.main_L, wc.ref);
                waiting_children.erase(waiting_children.begin() + i);
            }
        }

        // ── 4. Drain shared pending-resume queue (Signal:Wait etc.) ────
        auto& pending = get_pending_resumes();
        for (int i = (int)pending.size()-1; i >= 0; --i) {
            LuauPendingResume& pr = pending[i];
            // Only process entries that belong to this ScriptNode's main state
            if (pr.main_L != L_main) continue;
            if (pr.use_args) {   // RemoteEvent:Wait → devolver los args del evento
                Node* ctx = pr.ctx_id ? Object::cast_to<Node>(ObjectDB::get_instance(pr.ctx_id)) : nullptr;
                int n = gl_net_decode_args(pr.thread, pr.args, ctx);
                resume_external_thread(pr.thread, n);
            }
            else if (pr.node_arg) { wrap_node(pr.thread, pr.node_arg); resume_external_thread(pr.thread, 1); }
            else                  { lua_pushnumber(pr.thread, pr.delta); resume_external_thread(pr.thread, 1); }
            pending.erase(pending.begin() + i);
        }

        // ── 5. Respuestas de RemoteFunction por red (InvokeServer/Client) ──
        if (!waiting_invokes.empty()) {
            auto& resp = gl_net_responses();
            Node* nsctx = gl_net_service_id() ? Object::cast_to<Node>(ObjectDB::get_instance(gl_net_service_id())) : nullptr;
            for (int i = (int)waiting_invokes.size()-1; i >= 0; --i) {
                WaitingInvoke& wi = waiting_invokes[i];
                wi.elapsed += delta;
                auto it = resp.find(wi.call_id);
                if (it != resp.end() && it->second.ready) {
                    if (it->second.ok) {
                        int n = gl_net_decode_args(wi.thread, it->second.rets, nsctx);
                        resp.erase(it);
                        if (wi.main_L) lua_unref(wi.main_L, wi.ref);
                        resume_external_thread(wi.thread, n);
                    } else {
                        // El handler remoto falló: InvokeServer/Client devuelve nil
                        // (v1: el error no se propaga como error; se avisó del lado remoto).
                        resp.erase(it);
                        if (wi.main_L) lua_unref(wi.main_L, wi.ref);
                        lua_pushnil(wi.thread);
                        resume_external_thread(wi.thread, 1);
                    }
                    waiting_invokes.erase(waiting_invokes.begin() + i);
                } else if (wi.timeout > 0 && wi.elapsed >= wi.timeout) {
                    if (wi.main_L) lua_unref(wi.main_L, wi.ref);
                    lua_pushnil(wi.thread);
                    resume_external_thread(wi.thread, 1);
                    waiting_invokes.erase(waiting_invokes.begin() + i);
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
//  Concrete script types
////  Tipos concretos de script
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