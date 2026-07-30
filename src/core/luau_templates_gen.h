#ifndef GL_LUAU_TEMPLATES_GEN_H
#define GL_LUAU_TEMPLATES_GEN_H

// ════════════════════════════════════════════════════════════════════
//  GENERADO AUTOMATICAMENTE — NO EDITAR A MANO
//
//  Este header lo escribe tools/generar_plantillas_luau.py a partir de
//  los archivos .luau de GodotLuau/DefaultScripts/. Edita el .luau, no
//  esto: cualquier cambio aqui se pierde al regenerar.
//
//  Por que existe: las plantillas viven como .luau de verdad (con
//  autocompletado y faciles de encontrar), pero la DLL tiene que seguir
//  funcionando sin esa carpeta (juego exportado, instalacion parcial).
//  Estas constantes son esa copia embebida de reserva; en ejecucion
//  gl_luau_template() prefiere el archivo de disco si existe.
//
//  Tras tocar un .luau:  python tools/generar_plantillas_luau.py
// ════════════════════════════════════════════════════════════════════

// ── StarterPlayer/StarterCharacterScripts/Health.luau ──
static const char* LUAU_TEMPLATE_HEALTH_PATH = "StarterPlayer/StarterCharacterScripts/Health.luau";
static const char* LUAU_TEMPLATE_HEALTH = R"LUAU(
-- > GodotLuau — PimpoliDev
-- Health.lua — ServerScript — StarterCharacterScripts
-- Regenera la vida con el tiempo. Se clona al personaje al nacer (script.Parent = el jugador).
-- /// Regenerates health over time. Cloned onto the character on spawn (script.Parent = the player).

local RunService = game:GetService("RunService")

local humanoid = script.Parent:FindFirstChild("Humanoid")
if not humanoid then
    dwarn("[Health] Humanoid no encontrado en el personaje")
    return
end

dprint("[Health] Regeneracion activa: " .. script.Parent.Name)

-- La regeneración empieza REGEN_DELAY s tras el ÚLTIMO daño (recibir daño reinicia el contador).
-- /// Regen starts REGEN_DELAY s after the LAST hit (taking damage resets the timer).
local REGEN_DELAY = 3.0    -- s sin daño para regenerar /// seconds without damage before regen
local REGEN_RATE  = 0.01   -- fracción de MaxHealth por segundo (1%) /// fraction of MaxHealth per second (1%)

local last_health = humanoid.Health
local since_damage = REGEN_DELAY  -- permite regenerar desde el inicio /// allow regen from the start

humanoid.HealthChanged:Connect(function(newHP)
    if newHP < last_health then
        since_damage = 0.0  -- recibió daño → reinicia el contador /// took damage → reset the timer
    end
    last_health = newHP
end)

humanoid.Died:Connect(function()
    dprint("[Health] " .. script.Parent.Name .. " ha muerto.")
end)

-- Cada frame: si pasó el retardo y falta vida, sube un poco hacia MaxHealth.
-- /// Each frame: if the delay passed and health is missing, ramp up toward MaxHealth.
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

// ── StarterPlayer/StarterCharacterScripts/Animate.luau ──
static const char* LUAU_TEMPLATE_ANIMATE_PATH = "StarterPlayer/StarterCharacterScripts/Animate.luau";
static const char* LUAU_TEMPLATE_ANIMATE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- Animate.lua — Animaciones del personaje. Se clona al nacer (script.Parent = el jugador).
-- /// Character animations. Cloned onto the character on spawn (script.Parent = the player).

local RunService = game:GetService("RunService")

local humanoid = script.Parent:FindFirstChild("Humanoid")
if not humanoid then
    dwarn("[Animate] Humanoid no encontrado")
    return
end

dprint("[Animate] Sistema de animacion listo")

-- ══ TU CÓDIGO DE ANIMACIÓN AQUÍ /// YOUR ANIMATION CODE HERE ══
-- Descomenta para reaccionar a eventos del Humanoid: /// Uncomment to react to Humanoid events:

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

// ── StarterPlayer/StarterPlayerScripts/PlayerModule.luau ──
static const char* LUAU_TEMPLATE_PLAYER_MODULE_PATH = "StarterPlayer/StarterPlayerScripts/PlayerModule.luau";
static const char* LUAU_TEMPLATE_PLAYER_MODULE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- PlayerModule.lua — LocalScript — StarterPlayerScripts
-- Cerebro del jugador: carga los módulos de Modules/ y los une. Se clona al
-- jugador al nacer (script.Parent = el jugador).
-- /// Player brain: loads the modules in Modules/ and wires them. Cloned onto
-- /// the player on spawn (script.Parent = the player).

local RunService = game:GetService("RunService")

-- El jugador tiene el Humanoid y la carpeta Modules/. /// The player holds the Humanoid and the Modules/ folder.
local character = script.Parent
local humanoid  = character:FindFirstChild("Humanoid")

if not humanoid then
    dwarn("[PlayerModule] Humanoid no encontrado en el jugador")
    return
end

-- Cargar los módulos (movimiento, cámara, chat) desde Modules/.
-- /// Load the modules (movement, camera, chat) from Modules/.
local ControlModule = require(script.Parent.Modules.ControlModule)
local CameraModule  = require(script.Parent.Modules.CameraModule)
local ChatModule    = require(script.Parent.Modules.ChatModule)

-- Arrancar cada módulo. /// Start each module.
ControlModule:Initialize()
CameraModule:Apply()
ChatModule:Initialize()

-- El menú de Escape lo dibuja el MOTOR (nativo). Modules/Menu es solo un punto
-- de extensión opcional; el pcall evita fallar si no existe.
-- /// The Escape menu is drawn by the ENGINE (native). Modules/Menu is just an
-- /// optional hook; the pcall avoids errors if it isn't there.
pcall(function()
    local Menu = require(script.Parent.Modules.Menu)
    Menu.Init(game:GetService("Players").LocalPlayer)
end)

-- Pasar la velocidad y el salto del ControlModule al Humanoid.
-- /// Copy the ControlModule's walk speed and jump power to the Humanoid.
humanoid.WalkSpeed = ControlModule.WalkSpeed
humanoid.JumpPower = ControlModule.JumpPower

dprint("[PlayerModule] Jugador listo! Speed=" .. ControlModule.WalkSpeed)

humanoid.Died:Connect(function()
    dprint("[PlayerModule] El personaje ha muerto.")
end)

-- Cada frame: el ControlModule calcula la velocidad (sprint suave) y la aplica.
-- /// Each frame: ControlModule computes the speed (smooth sprint) and applies it.
RunService.Heartbeat:Connect(function(dt)
    if humanoid.Health <= 0 then return end

    ControlModule:Update(dt)
    local speed = ControlModule:GetCurrentSpeed()
    if math.abs(humanoid.WalkSpeed - speed) > 0.01 then
        humanoid.WalkSpeed = speed
    end

    -- ══ TU CÓDIGO POR FRAME AQUÍ /// YOUR PER-FRAME CODE HERE ══
    -- local x, z = ControlModule:GetMoveVector()   -- dirección del teclado /// keyboard direction
    -- local st, stMax = ControlModule:GetStamina() -- estamina actual /// current stamina
    -- if ChatModule:IsOpen() then ... end
    -- ══════════════════════════════════════════════════════════
end)
)LUAU";

// ── StarterPlayer/StarterPlayerScripts/Modules/ControlModule.luau ──
static const char* LUAU_TEMPLATE_CONTROL_MODULE_PATH = "StarterPlayer/StarterPlayerScripts/Modules/ControlModule.luau";
static const char* LUAU_TEMPLATE_CONTROL_MODULE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- ControlModule.lua — ModuleScript — Modules/
-- Elige la plataforma y reparte el trabajo al sub-módulo correcto (PC / móvil /
-- consola). Es el "router" del movimiento.
-- /// Picks the platform and routes the work to the right sub-module (PC / mobile
-- /// / console). This is the movement "router".

local ControlModule = {}

-- Sub-módulos (hijos de este módulo en el árbol). /// Sub-modules (children of this module in the tree).
local PCModule      = require(script.PCModule)
local MobileModule  = require(script.MobileModule)
local ConsoleModule = require(script.ConsoleModule)

-- CONFIG: Platform "" = auto · "PC" teclado · "Mobile" táctil · "Console" mando.
-- /// CONFIG: Platform "" = auto · "PC" keyboard · "Mobile" touch · "Console" gamepad.
ControlModule.Platform  = "PC"  -- fuerza una plataforma (o "" para auto) /// force a platform (or "" for auto)
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
    -- Copiar velocidad/salto del sub-módulo activo. /// Copy speed/jump from the active sub-module.
    self.WalkSpeed = active.WalkSpeed
    self.JumpPower = active.JumpPower
    if active.Initialize then active:Initialize() end
    dprint("[ControlModule] Plataforma: " .. (p == "" and "Auto(PC)" or p))
end

-- Lo llama PlayerModule cada frame (sprint suave, estamina…). /// Called by PlayerModule each frame (smooth sprint, stamina…).
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

// ── StarterPlayer/StarterPlayerScripts/Modules/ControlModule/PCModule.luau ──
static const char* LUAU_TEMPLATE_PC_MODULE_PATH = "StarterPlayer/StarterPlayerScripts/Modules/ControlModule/PCModule.luau";
static const char* LUAU_TEMPLATE_PC_MODULE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- PCModule.lua — ModuleScript — Modules/ControlModule/
-- Movimiento en PC (teclado + ratón). El motor lee las teclas; aquí defines las
-- velocidades y el comportamiento.
-- /// PC movement (keyboard + mouse). The engine reads the keys; here you set the
-- /// speeds and behavior.

local PCModule = {}

-- Velocidades. /// Speeds.
PCModule.WalkSpeed    = 16    -- caminar (16 = estándar) /// walk (16 = standard)
PCModule.RunSpeed     = 24    -- correr (mantener LeftShift) /// run (hold LeftShift)
PCModule.Acceleration = 8     -- suavidad: mayor = más brusco /// smoothing: higher = snappier
PCModule.JumpPower    = 20    -- fuerza de salto /// jump strength
PCModule.AutoRotate   = true  -- girar hacia el movimiento /// turn toward movement

-- Estamina (opcional): correr la gasta, caminar la recupera; a 0 no puedes correr.
-- /// Stamina (optional): running drains it, walking refills it; at 0 you can't run.
PCModule.StaminaEnabled = false
PCModule.StaminaMax     = 100
PCModule.StaminaDrain   = 20   -- por segundo corriendo /// per second running
PCModule.StaminaRegen   = 15   -- por segundo caminando /// per second walking

local current_speed = 16
local stamina       = 100

function PCModule:Initialize()
    current_speed = self.WalkSpeed
    stamina       = self.StaminaMax
    dprint("[PCModule] Modo PC activo: W/A/S/D para mover, Shift para correr")
end

-- Cada frame (lo llama ControlModule): estamina + acelerar hacia la velocidad objetivo.
-- /// Each frame (called by ControlModule): stamina + accelerate toward target speed.
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

    -- Acelera suave hacia la velocidad objetivo (sin saltos). /// Ease toward the target speed (no jumps).
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
    -- Dirección leída del teclado (el motor ya mueve; útil para animaciones/efectos).
    -- /// Direction read from the keyboard (engine already moves; handy for animations/FX).
    local x, z = 0, 0
    if UserInputService:IsKeyDown("A") then x = x - 1 end
    if UserInputService:IsKeyDown("D") then x = x + 1 end
    if UserInputService:IsKeyDown("W") then z = z - 1 end
    if UserInputService:IsKeyDown("S") then z = z + 1 end
    return x, z
end

return PCModule
)LUAU";

// ── StarterPlayer/StarterPlayerScripts/Modules/ControlModule/MobileModule.luau ──
static const char* LUAU_TEMPLATE_MOBILE_MODULE_PATH = "StarterPlayer/StarterPlayerScripts/Modules/ControlModule/MobileModule.luau";
static const char* LUAU_TEMPLATE_MOBILE_MODULE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- MobileModule.lua — ModuleScript — Modules/ControlModule/
-- Movimiento en móvil (táctil). Cambia TouchMode para el tipo de control.
-- /// Mobile movement (touch). Change TouchMode for the control type.

local MobileModule = {}

-- CONFIG. TouchMode: "Joystick" (stick virtual) · "Console" (D-Pad) · "Tap" (tocar para ir).
-- /// CONFIG. TouchMode: "Joystick" (virtual stick) · "Console" (D-Pad) · "Tap" (tap to go).
MobileModule.WalkSpeed = 16
MobileModule.JumpPower = 20
MobileModule.TouchMode = "Joystick"

function MobileModule:Initialize()
    dprint("[MobileModule] Modo Movil activo: " .. self.TouchMode)
    -- Aquí puedes crear tu joystick con ScreenGui/ImageButton. /// Build your joystick here with ScreenGui/ImageButton.
end

function MobileModule:GetCurrentSpeed()
    return self.WalkSpeed
end

function MobileModule:GetMoveVector()
    return 0, 0  -- Impleméntalo con el táctil (UserInputService.TouchMoved). /// Implement with touch (UserInputService.TouchMoved).
end

return MobileModule
)LUAU";

// ── StarterPlayer/StarterPlayerScripts/Modules/ControlModule/ConsoleModule.luau ──
static const char* LUAU_TEMPLATE_CONSOLE_MODULE_PATH = "StarterPlayer/StarterPlayerScripts/Modules/ControlModule/ConsoleModule.luau";
static const char* LUAU_TEMPLATE_CONSOLE_MODULE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- ConsoleModule.lua — ModuleScript — Modules/ControlModule/
-- Movimiento con mando (Xbox, PlayStation, genéricos).
-- /// Gamepad movement (Xbox, PlayStation, generic pads).

local ConsoleModule = {}

-- CONFIG. Deadzone = zona muerta del stick (0..1). /// CONFIG. Deadzone = stick dead zone (0..1).
ConsoleModule.WalkSpeed = 16
ConsoleModule.JumpPower = 20
ConsoleModule.Deadzone  = 0.2  -- ignora movimientos menores /// ignore smaller movements

function ConsoleModule:Initialize()
    dprint("[ConsoleModule] Modo Consola activo — Joystick izquierdo para mover")
end

function ConsoleModule:GetCurrentSpeed()
    return self.WalkSpeed
end

function ConsoleModule:GetMoveVector()
    return 0, 0  -- Impleméntalo con el gamepad de UserInputService. /// Implement with UserInputService gamepad input.
end

return ConsoleModule
)LUAU";

// ── StarterPlayer/StarterPlayerScripts/Modules/ChatModule.luau ──
static const char* LUAU_TEMPLATE_CHAT_MODULE_PATH = "StarterPlayer/StarterPlayerScripts/Modules/ChatModule.luau";
static const char* LUAU_TEMPLATE_CHAT_MODULE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- ChatModule.lua — ModuleScript — Modules/
-- Lógica del chat, en Luau. Guarda los mensajes y habla con TextChatService.
-- Edítalo a tu gusto.
-- /// Chat logic, in Luau. Keeps the messages and talks to TextChatService.
-- /// Edit it however you like.

local ChatModule = {}

local TextChatService = game:GetService("TextChatService")

-- CONFIG del chat. /// Chat CONFIG.
ChatModule.MaxMessages  = 50    -- máximo de mensajes guardados /// max messages kept
ChatModule.FadeDelay    = 8.0   -- segundos hasta desvanecer /// seconds until fade out
ChatModule.FilterWords  = true  -- filtrar palabras inapropiadas /// filter bad words

local messages  = {}
local chat_open = false
local initialized = false

-- Prepara el chat y escucha los mensajes que llegan. /// Sets up the chat and listens for incoming messages.
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

// ── StarterPlayer/StarterPlayerScripts/Modules/CameraModule.luau ──
static const char* LUAU_TEMPLATE_CAMERA_MODULE_PATH = "StarterPlayer/StarterPlayerScripts/Modules/CameraModule.luau";
static const char* LUAU_TEMPLATE_CAMERA_MODULE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- CameraModule.lua — ModuleScript — Modules/
-- Controla cómo la cámara sigue al jugador. Cambia Mode para el comportamiento.
-- /// Controls how the camera follows the player. Change Mode for the behavior.

local CameraModule = {}

-- MODO: 1 = fija (sigue al instante) · 2 = suave (sigue con retraso) ·
--       3 = combinada (retraso al moverse, centra al parar).
-- /// MODE: 1 = fixed (instant follow) · 2 = smooth (follows with lag) ·
-- ///       3 = blended (lag while moving, recenters when stopped).
CameraModule.Mode = 3   -- ← cambia aquí el modo /// change the mode here

-- Aplica el modo al entrar. /// Applies the mode on start.
function CameraModule:Apply(player)
    if player then
        player.CameraMode = self.Mode
    end
end

-- Cambia el modo en caliente. /// Changes the mode at runtime.
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

// ── StarterPlayer/StarterPlayerScripts/Modules/SettingsModule.luau ──
static const char* LUAU_TEMPLATE_SETTINGS_MODULE_PATH = "StarterPlayer/StarterPlayerScripts/Modules/SettingsModule.luau";
static const char* LUAU_TEMPLATE_SETTINGS_MODULE = R"LUAU(
-- > GodotLuau — PimpoliDev
-- SettingsModule.lua — ModuleScript — StarterPlayerScripts/Modules
-- Ajustes tipo Roblox, EDITABLE. Lo carga el PlayerModule con Settings.Init(player).
local Settings = {}

local Workspace = game:GetService("Workspace")
local UIS = game:GetService("UserInputService")

-- ── Colores / estilo (edítalos a gusto) ──────────────────────────────
local PANEL_BG   = Color3.fromRGB(24, 27, 34)
local ROW_BG     = Color3.fromRGB(38, 42, 52)
local ACCENT     = Color3.fromRGB(0, 162, 255)
local TEXT       = Color3.fromRGB(240, 242, 245)

local gui, panel, open = nil, nil, false
local quality = Workspace:GetGraphicsQuality()
local camLocked = false
local dynJoy = true

local function rounded(inst, r)
    local c = Instance.new("UICorner"); c.CornerRadius = UDim.new(0, r or 8); c.Parent = inst
end

-- Crea una fila "Etiqueta  [ - ] valor [ + ]"
local function stepperRow(parent, y, label, getText, onMinus, onPlus)
    local row = Instance.new("Frame")
    row.Size = UDim2.new(1, -20, 0, 40)
    row.Position = UDim2.new(0, 10, 0, y)
    row.BackgroundColor3 = ROW_BG
    row.Parent = parent
    rounded(row, 8)

    local lbl = Instance.new("TextLabel")
    lbl.BackgroundTransparency = 1
    lbl.Size = UDim2.new(0.5, 0, 1, 0)
    lbl.Position = UDim2.new(0, 12, 0, 0)
    lbl.Text = label
    lbl.TextColor3 = TEXT
    lbl.TextXAlignment = Enum and Enum.TextXAlignment and Enum.TextXAlignment.Left or 0
    lbl.Parent = row

    local val = Instance.new("TextLabel")
    val.BackgroundTransparency = 1
    val.Size = UDim2.new(0, 70, 1, 0)
    val.Position = UDim2.new(1, -140, 0, 0)
    val.TextColor3 = ACCENT
    val.Text = getText()
    val.Parent = row

    local function mkbtn(text, px, cb)
        local b = Instance.new("TextButton")
        b.Size = UDim2.new(0, 34, 0, 30)
        b.Position = UDim2.new(1, px, 0.5, -15)
        b.BackgroundColor3 = ACCENT
        b.TextColor3 = TEXT
        b.Text = text
        b.Parent = row
        rounded(b, 6)
        b.MouseButton1Click:Connect(function() cb(); val.Text = getText() end)
    end
    mkbtn("-", -70, onMinus)
    mkbtn("+", -34, onPlus)
end

local function toggleRow(parent, y, label, getOn, onToggle)
    local row = Instance.new("Frame")
    row.Size = UDim2.new(1, -20, 0, 40)
    row.Position = UDim2.new(0, 10, 0, y)
    row.BackgroundColor3 = ROW_BG
    row.Parent = parent
    rounded(row, 8)
    local lbl = Instance.new("TextLabel")
    lbl.BackgroundTransparency = 1
    lbl.Size = UDim2.new(0.7, 0, 1, 0)
    lbl.Position = UDim2.new(0, 12, 0, 0)
    lbl.Text = label
    lbl.TextColor3 = TEXT
    lbl.Parent = row
    local b = Instance.new("TextButton")
    b.Size = UDim2.new(0, 60, 0, 30)
    b.Position = UDim2.new(1, -70, 0.5, -15)
    b.Text = getOn() and "ON" or "OFF"
    b.BackgroundColor3 = getOn() and ACCENT or Color3.fromRGB(70, 74, 84)
    b.TextColor3 = TEXT
    b.Parent = row
    rounded(b, 6)
    b.MouseButton1Click:Connect(function()
        onToggle()
        b.Text = getOn() and "ON" or "OFF"
        b.BackgroundColor3 = getOn() and ACCENT or Color3.fromRGB(70, 74, 84)
    end)
end

local function build(player)
    gui = Instance.new("ScreenGui")
    gui.Name = "SettingsGui"
    gui.Parent = player:WaitForChild("PlayerGui")

    -- Botón de engranaje siempre visible (como Roblox móvil)
    local gear = Instance.new("TextButton")
    gear.Name = "SettingsButton"
    gear.Size = UDim2.new(0, 42, 0, 42)
    gear.Position = UDim2.new(1, -54, 0, 12)
    gear.AnchorPoint = Vector2.new(0, 0)
    gear.BackgroundColor3 = PANEL_BG
    gear.TextColor3 = TEXT
    gear.Text = "*"
    gear.Parent = gui
    rounded(gear, 10)

    panel = Instance.new("Frame")
    panel.Name = "Panel"
    panel.Size = UDim2.new(0, 360, 0, 300)
    panel.Position = UDim2.new(0.5, 0, 0.5, 0)
    panel.AnchorPoint = Vector2.new(0.5, 0.5)
    panel.BackgroundColor3 = PANEL_BG
    panel.Visible = false
    panel.Parent = gui
    rounded(panel, 14)

    local title = Instance.new("TextLabel")
    title.BackgroundTransparency = 1
    title.Size = UDim2.new(1, 0, 0, 40)
    title.Text = "Settings"
    title.TextColor3 = TEXT
    title.Parent = panel

    -- Calidad gráfica 1..10
    stepperRow(panel, 50, "Graphics Quality", function() return quality .. "/10" end,
        function() quality = math.max(1, quality - 1); Workspace:SetGraphicsQuality(quality) end,
        function() quality = math.min(10, quality + 1); Workspace:SetGraphicsQuality(quality) end)

    -- Bloquear cámara (3a persona se ve como 1a)
    toggleRow(panel, 100, "Lock Camera (1st person)", function() return camLocked end,
        function()
            camLocked = not camLocked
            player.CameraMode = camLocked and 1 or 0   -- Enum.CameraMode.LockFirstPerson / Classic
        end)

    -- Móvil: joystick dinámico (aparece donde tocas) vs fijo. Solo afecta al táctil.
    toggleRow(panel, 150, "Dynamic Joystick", function() return dynJoy end,
        function()
            dynJoy = not dynJoy
            local char = player.Character
            if char then char.JoystickDynamic = dynJoy end
        end)

    local close = Instance.new("TextButton")
    close.Size = UDim2.new(0, 120, 0, 34)
    close.Position = UDim2.new(0.5, 0, 1, -46)
    close.AnchorPoint = Vector2.new(0.5, 0)
    close.BackgroundColor3 = ACCENT
    close.TextColor3 = TEXT
    close.Text = "Close"
    close.Parent = panel
    rounded(close, 8)

    gear.MouseButton1Click:Connect(function() Settings.Toggle() end)
    close.MouseButton1Click:Connect(function() Settings.Close() end)
end

function Settings.Open()  if panel then open = true;  panel.Visible = true  end end
function Settings.Close() if panel then open = false; panel.Visible = false end end
function Settings.Toggle() if open then Settings.Close() else Settings.Open() end end

function Settings.Init(player)
    if gui then return Settings end
    build(player)
    return Settings
end

return Settings
)LUAU";

// ── StarterPlayer/StarterPlayerScripts/Modules/Menu.luau ──
static const char* LUAU_TEMPLATE_MENU_PATH = "StarterPlayer/StarterPlayerScripts/Modules/Menu.luau";
static const char* LUAU_TEMPLATE_MENU = R"LUAU(
-- > GodotLuau — PimpoliDev
-- Menu.lua — ModuleScript — StarterPlayerScripts/Modules/Menu
-- El menú de Escape lo dibuja el MOTOR (nativo, estilo Roblox, con todos los
-- ajustes y el botón-logo arriba-izquierda). Este módulo es un punto de
-- extensión: por defecto no hace nada. Pon tu logo en res://gamelogo.png.
local Menu = {}
function Menu.Init(player)
	-- Aquí puedes añadir tu propia UI extra si quieres. El menú principal ya
	-- lo gestiona el motor, así que este Init se deja vacío a propósito.
end
return Menu
)LUAU";

// ── StarterPlayer/StarterPlayerScripts/Modules/Menu/MenuUi.luau ──
static const char* LUAU_TEMPLATE_MENU_UI_PATH = "StarterPlayer/StarterPlayerScripts/Modules/Menu/MenuUi.luau";
static const char* LUAU_TEMPLATE_MENU_UI = R"LUAU(
-- > GodotLuau — PimpoliDev
-- MenuUi.lua — ModuleScript — StarterPlayerScripts/Modules/Menu
-- Toolkit visual + "shell" del menú de Escape. EDITABLE: cambia colores,
-- tamaños, fuentes o añade widgets. Lo usa Menu para montar cada pestaña.

local MenuUi = {}

-- ── Tema (edítalo a gusto) ────────────────────────────────────────────
MenuUi.Theme = {
	Overlay = Color3.fromRGB(6, 8, 12),
	Panel   = Color3.fromRGB(16, 18, 24),
	Bar     = Color3.fromRGB(11, 13, 18),
	Row     = Color3.fromRGB(30, 33, 42),
	RowSel  = Color3.fromRGB(38, 42, 54),
	Accent  = Color3.fromRGB(0, 120, 255),
	Text    = Color3.fromRGB(238, 240, 244),
	Dim     = Color3.fromRGB(150, 156, 168),
	Green   = Color3.fromRGB(56, 200, 110),
	Red     = Color3.fromRGB(232, 78, 78),
}
local T = MenuUi.Theme

-- ── Helpers de construcción ───────────────────────────────────────────
local function corner(inst, r)
	local c = Instance.new("UICorner")
	c.CornerRadius = UDim.new(0, r or 8)
	c.Parent = inst
	return c
end
MenuUi.corner = corner

local function vlist(parent, gap)
	local l = Instance.new("UIListLayout")
	l.FillDirection = Enum.FillDirection.Vertical
	l.Padding = UDim.new(0, gap or 8)
	l.HorizontalAlignment = Enum.HorizontalAlignment.Center
	l.SortOrder = Enum.SortOrder.LayoutOrder
	l.Parent = parent
	return l
end
MenuUi.vlist = vlist

local function label(parent, text, size, color, xalign)
	local l = Instance.new("TextLabel")
	l.BackgroundTransparency = 1
	l.Text = text
	l.TextColor3 = color or T.Text
	l.TextSize = size or 16
	l.TextXAlignment = xalign or Enum.TextXAlignment.Left
	l.TextYAlignment = Enum.TextYAlignment.Center
	l.Parent = parent
	return l
end
MenuUi.label = label

-- ── Widgets de fila (para el contenido de las pestañas) ───────────────
-- Encabezado de sección: "Audio", "Pantalla y gráficos", etc.
function MenuUi:Section(page, title)
	local l = label(page, title, 20, T.Text)
	l.Size = UDim2.new(1, -8, 0, 34)
	l.TextXAlignment = Enum.TextXAlignment.Left
	return l
end

-- Fila base: fondo suave con la etiqueta a la izquierda; devuelve la fila
-- para que el caller ponga controles a la derecha (con AnchorPoint 1,0.5).
function MenuUi:Row(page, text, subtitle)
	local row = Instance.new("Frame")
	row.Size = UDim2.new(1, -8, 0, subtitle and 56 or 44)
	row.BackgroundColor3 = T.Row
	row.Parent = page
	corner(row, 8)

	local lbl = label(row, text, 16, T.Text)
	lbl.Position = UDim2.new(0, 14, 0, 0)
	lbl.Size = UDim2.new(0.55, 0, subtitle and 0.62 or 1, 0)

	if subtitle then
		local sub = label(row, subtitle, 12, T.Dim)
		sub.Position = UDim2.new(0, 14, 0.55, 0)
		sub.Size = UDim2.new(0.6, 0, 0.42, 0)
	end
	return row
end

-- Fila informativa: etiqueta + valor a la derecha (solo lectura).
function MenuUi:Info(page, text, value)
	local row = self:Row(page, text)
	local v = label(row, value, 16, T.Dim, Enum.TextXAlignment.Right)
	v.AnchorPoint = Vector2.new(1, 0.5)
	v.Position = UDim2.new(1, -16, 0.5, 0)
	v.Size = UDim2.new(0, 220, 1, 0)
	return v
end

-- Stepper estilo Roblox:  etiqueta   [ - ]  valor  [ + ]
function MenuUi:Stepper(page, text, getText, onMinus, onPlus)
	local row = self:Row(page, text)
	local function btn(sym, xoff)
		local b = Instance.new("TextButton")
		b.Size = UDim2.new(0, 34, 0, 30)
		b.AnchorPoint = Vector2.new(1, 0.5)
		b.Position = UDim2.new(1, xoff, 0.5, 0)
		b.BackgroundColor3 = T.Accent
		b.TextColor3 = T.Text
		b.TextSize = 20
		b.Text = sym
		b.Parent = row
		corner(b, 6)
		return b
	end
	local val = label(row, getText(), 16, T.Text, Enum.TextXAlignment.Center)
	val.AnchorPoint = Vector2.new(1, 0.5)
	val.Position = UDim2.new(1, -52, 0.5, 0)
	val.Size = UDim2.new(0, 120, 1, 0)

	local plus  = btn("+", -14)
	local minus = btn("-", -190)
	minus.MouseButton1Click:Connect(function() onMinus(); val.Text = getText() end)
	plus.MouseButton1Click:Connect(function() onPlus(); val.Text = getText() end)
	return val
end

-- Toggle ON/OFF
function MenuUi:Toggle(page, text, subtitle, getOn, onToggle)
	local row = self:Row(page, text, subtitle)
	local b = Instance.new("TextButton")
	b.Size = UDim2.new(0, 64, 0, 30)
	b.AnchorPoint = Vector2.new(1, 0.5)
	b.Position = UDim2.new(1, -16, 0.5, 0)
	b.TextColor3 = T.Text
	b.TextSize = 14
	b.Parent = row
	corner(b, 15)
	local function paint()
		b.Text = getOn() and "ON" or "OFF"
		b.BackgroundColor3 = getOn() and T.Accent or Color3.fromRGB(70, 74, 84)
	end
	paint()
	b.MouseButton1Click:Connect(function() onToggle(); paint() end)
	return b
end

-- ── Shell del menú (overlay + tabs + contenido + barra inferior) ──────
function MenuUi.new(player)
	local self = setmetatable({}, { __index = MenuUi })
	local pg = player:WaitForChild("PlayerGui")

	local gui = Instance.new("ScreenGui")
	gui.Name = "GodotLuauMenu"
	gui.ResetOnSpawn = false
	gui.DisplayOrder = 50
	gui.Parent = pg

	local overlay = Instance.new("Frame")
	overlay.Name = "Overlay"
	overlay.Size = UDim2.new(1, 0, 1, 0)
	overlay.BackgroundColor3 = T.Overlay
	overlay.BackgroundTransparency = 0.15
	overlay.Visible = false
	overlay.Parent = gui

	local panel = Instance.new("Frame")
	panel.Name = "Panel"
	panel.AnchorPoint = Vector2.new(0.5, 0.5)
	panel.Position = UDim2.new(0.5, 0, 0.5, 0)
	panel.Size = UDim2.new(1, -72, 1, -72)
	panel.BackgroundColor3 = T.Panel
	panel.Parent = overlay
	corner(panel, 16)

	-- Barra de pestañas
	local tabbar = Instance.new("Frame")
	tabbar.Name = "TabBar"
	tabbar.Size = UDim2.new(1, -24, 0, 52)
	tabbar.Position = UDim2.new(0, 12, 0, 6)
	tabbar.BackgroundTransparency = 1
	tabbar.Parent = panel
	local tlayout = Instance.new("UIListLayout")
	tlayout.FillDirection = Enum.FillDirection.Horizontal
	tlayout.Padding = UDim.new(0, 10)
	tlayout.VerticalAlignment = Enum.VerticalAlignment.Center
	tlayout.HorizontalAlignment = Enum.HorizontalAlignment.Left
	tlayout.Parent = tabbar

	-- Línea separadora
	local sep = Instance.new("Frame")
	sep.Size = UDim2.new(1, -24, 0, 1)
	sep.Position = UDim2.new(0, 12, 0, 60)
	sep.BackgroundColor3 = Color3.fromRGB(255, 255, 255)
	sep.BackgroundTransparency = 0.9
	sep.Parent = panel

	-- Contenido
	local content = Instance.new("Frame")
	content.Name = "Content"
	content.Position = UDim2.new(0, 12, 0, 70)
	content.Size = UDim2.new(1, -24, 1, -152)
	content.BackgroundTransparency = 1
	content.Parent = panel

	-- Barra inferior (Salir / Regenerar / Reanudar)
	local bottom = Instance.new("Frame")
	bottom.Name = "Bottom"
	bottom.AnchorPoint = Vector2.new(0.5, 1)
	bottom.Position = UDim2.new(0.5, 0, 1, -12)
	bottom.Size = UDim2.new(1, -24, 0, 56)
	bottom.BackgroundTransparency = 1
	bottom.Parent = panel

	self.gui, self.overlay, self.panel = gui, overlay, panel
	self.tabbar, self.content, self.bottom = tabbar, content, bottom
	self.pages, self.tabButtons, self.current = {}, {}, nil
	self.isOpen = false
	return self
end

-- Crea una pestaña (botón arriba + página de contenido). Devuelve la página.
function MenuUi:AddTab(name)
	local btn = Instance.new("TextButton")
	btn.Name = "Tab_" .. name
	btn.Size = UDim2.new(0, 132, 0, 40)
	btn.BackgroundTransparency = 1
	btn.TextColor3 = T.Dim
	btn.TextSize = 17
	btn.Text = name
	btn.Parent = self.tabbar
	btn.MouseButton1Click:Connect(function() self:ShowTab(name) end)
	self.tabButtons[name] = btn

	local page = Instance.new("Frame")
	page.Name = "Page_" .. name
	page.Size = UDim2.new(1, 0, 1, 0)
	page.BackgroundTransparency = 1
	page.Visible = false
	page.Parent = self.content
	vlist(page, 8)
	self.pages[name] = page
	return page
end

function MenuUi:ShowTab(name)
	for n, page in pairs(self.pages) do page.Visible = (n == name) end
	for n, btn in pairs(self.tabButtons) do
		btn.TextColor3 = (n == name) and T.Text or T.Dim
	end
	self.current = name
end

-- Barra inferior: 3 botones tipo Roblox (tecla + texto).
function MenuUi:BuildBottom(specs)
	local n = #specs
	for i, s in ipairs(specs) do
		local b = Instance.new("TextButton")
		b.Size = UDim2.new(1 / n, -10, 1, 0)
		b.Position = UDim2.new((i - 1) / n, 5, 0, 0)
		b.BackgroundColor3 = s.color or T.Row
		b.TextColor3 = T.Text
		b.TextSize = 17
		b.Text = "  " .. (s.key and ("[" .. s.key .. "]  ") or "") .. s.text
		b.Parent = self.bottom
		corner(b, 8)
		b.MouseButton1Click:Connect(s.cb)
	end
end

function MenuUi:Open()  self.overlay.Visible = true;  self.isOpen = true end
function MenuUi:Close() self.overlay.Visible = false; self.isOpen = false end
function MenuUi:Toggle() if self.isOpen then self:Close() else self:Open() end end

return MenuUi
)LUAU";

// ── StarterPlayer/StarterPlayerScripts/Modules/Menu/Settings.luau ──
static const char* LUAU_TEMPLATE_MENU_SETTINGS_PATH = "StarterPlayer/StarterPlayerScripts/Modules/Menu/Settings.luau";
static const char* LUAU_TEMPLATE_MENU_SETTINGS = R"LUAU(
-- > GodotLuau — PimpoliDev
-- Settings.lua — ModuleScript — Modules/Menu
-- Pestaña "Config." del menú. EDITABLE: añade/quita filas y secciones.
-- Aplica de verdad: calidad 1..10, FPS máx, volumen, bloqueo de cámara y
-- joystick dinámico (móvil).

local Settings = {}
local Workspace = game:GetService("Workspace")

function Settings.Build(ui, page, player)
	local quality = 8
	pcall(function() quality = Workspace:GetGraphicsQuality() end)
	local volume = 100
	local fpsOptions = { 30, 60, 120, 144, 240, 0 }   -- 0 = ilimitado
	local fpsIdx = 3
	local camLocked = false
	local dynJoy = true

	-- ── Graphics ─────────────────────────────────────────────────────
	-- EDITABLE: three modes, like Roblox.
	--   Automatic — the engine raises/lowers quality by itself to hold the FPS
	--   Manual    — one fixed quality level (1..10)
	--   Custom    — every setting separately (resolution/FSR, shadows, view...)
	-- Delete any block below if you only want some of them.
	local MODES = { "Automatic", "Manual", "Custom" }
	local modeIdx = 2                      -- 1=Automatic 2=Manual 3=Custom
	local targetFps = 45
	pcall(function() modeIdx = Workspace:GetGraphicsMode() + 1 end)

	ui:Section(page, "Graphics")
	ui:Stepper(page, "Mode",
		function() return MODES[modeIdx] end,
		function() modeIdx = math.max(1, modeIdx - 1); Workspace:SetGraphicsMode(modeIdx - 1) end,
		function() modeIdx = math.min(#MODES, modeIdx + 1); Workspace:SetGraphicsMode(modeIdx - 1) end)

	-- Automatic: target frame rate to hold
	ui:Stepper(page, "Target FPS (Automatic)",
		function() return tostring(targetFps) end,
		function() targetFps = math.max(30, targetFps - 15); Workspace:SetAutoTargetFPS(targetFps) end,
		function() targetFps = math.min(240, targetFps + 15); Workspace:SetAutoTargetFPS(targetFps) end)

	-- Manual: one level for everything
	ui:Stepper(page, "Quality level (Manual)",
		function() return quality .. " / 10" end,
		function() quality = math.max(1, quality - 1); Workspace:SetGraphicsQuality(quality) end,
		function() quality = math.min(10, quality + 1); Workspace:SetGraphicsQuality(quality) end)

	-- Custom: each setting on its own. Remove the ones you don't want exposed.
	local function customStepper(label, key, values, fmt)
		local idx = 1
		pcall(function()
			local cur = Workspace:GetGraphicsSetting(key)
			for i, v in ipairs(values) do if math.abs(v - cur) < 0.001 then idx = i end end
		end)
		ui:Stepper(page, label,
			function() return fmt(values[idx]) end,
			function() idx = math.max(1, idx - 1); Workspace:SetGraphicsSetting(key, values[idx]) end,
			function() idx = math.min(#values, idx + 1); Workspace:SetGraphicsSetting(key, values[idx]) end)
	end

	local function pct(v) return math.floor(v * 100 + 0.5) .. "%" end
	customStepper("Render resolution (Custom)", "RenderScale",
		{ 0.25, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0 }, pct)
	customStepper("Upscaler (Custom)", "Upscaler", { 0, 1, 2 },
		function(v) return ({ [0] = "Bilinear", [1] = "FSR", [2] = "FSR 2" })[v] or "?" end)
	customStepper("Sharpness (Custom)", "Sharpness", { 0, 0.25, 0.5, 0.75, 1.0 }, pct)
	customStepper("Shadows (Custom)", "ShadowQuality", { 0, 1, 2, 3, 4, 5 },
		function(v) return v == 0 and "Off" or (v .. " / 5") end)
	customStepper("View distance (Custom)", "ViewDistance",
		{ 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 }, pct)
	-- Occlusion culling: skips what is hidden behind walls. It is OFF by default
	-- because the culling runs on the CPU: with thousands of small parts it costs
	-- more than it saves. It pays off on maps with few, large walls.
	-- Transparency drawn by DISCARDING PIXELS instead of real blending. Real
	-- transparency makes the GPU sort and blend every part against what is behind
	-- it; discarding pixels draws it like a solid and costs far less (you may see
	-- a fine dither pattern up close). Parts with Transparency = 1 are always
	-- skipped entirely, with or without this option.
	customStepper("Pixel transparency (Custom)", "PixelTransparency", { 0, 1 },
		function(v) return v > 0.5 and "On" or "Off" end)
	customStepper("Occlusion culling (Custom)", "Occlusion", { 0, 1 },
		function(v) return v > 0.5 and "On" or "Off" end)
	customStepper("Occluder min size (Custom)", "OcclusionSize",
		{ 4, 8, 16, 32 }, function(v) return v .. " studs" end)
	customStepper("Occlusion quality (Custom)", "OcclusionQuality", { 0, 1, 2 },
		function(v) return ({ [0] = "Fast", [1] = "Balanced", [2] = "Precise" })[v] or "?" end)
	ui:Stepper(page, "Velocidad máxima de fotogramas",
		function() local f = fpsOptions[fpsIdx]; return f == 0 and "Ilimitado" or (f .. " FPS") end,
		function() fpsIdx = math.max(1, fpsIdx - 1); Workspace:SetMaxFPS(fpsOptions[fpsIdx]) end,
		function() fpsIdx = math.min(#fpsOptions, fpsIdx + 1); Workspace:SetMaxFPS(fpsOptions[fpsIdx]) end)

	-- ── Audio ────────────────────────────────────────────────────────
	ui:Section(page, "Audio")
	ui:Stepper(page, "Volumen",
		function() return volume .. "%" end,
		function() volume = math.max(0, volume - 10); Workspace:SetMasterVolume(volume / 100) end,
		function() volume = math.min(100, volume + 10); Workspace:SetMasterVolume(volume / 100) end)

	-- ── Vista y controles ────────────────────────────────────────────
	ui:Section(page, "Vista y controles")
	ui:Toggle(page, "Bloquear cámara (1ª persona)", "En 3ª persona se verá como en 1ª persona",
		function() return camLocked end,
		function()
			camLocked = not camLocked
			player.CameraMode = camLocked and Enum.CameraMode.LockFirstPerson or Enum.CameraMode.Classic
		end)
	ui:Toggle(page, "Joystick dinámico (móvil)", "Aparece donde tocas, como en Roblox",
		function() return dynJoy end,
		function()
			dynJoy = not dynJoy
			local char = player.Character
			if char then char.JoystickDynamic = dynJoy end
		end)

	return Settings
end

return Settings
)LUAU";

// ── StarterPlayer/StarterPlayerScripts/Modules/Menu/Players.luau ──
static const char* LUAU_TEMPLATE_MENU_PLAYERS_PATH = "StarterPlayer/StarterPlayerScripts/Modules/Menu/Players.luau";
static const char* LUAU_TEMPLATE_MENU_PLAYERS = R"LUAU(
-- > GodotLuau — PimpoliDev
-- Players.lua — ModuleScript — Modules/Menu
-- Pestaña "Personas" del menú: lista de jugadores en el servidor. EDITABLE.

local Players = {}
local PlayersService = game:GetService("Players")

function Players.Build(ui, page, player)
	ui:Section(page, "En este servidor")

	local list = {}
	pcall(function() list = PlayersService:GetPlayers() end)
	if #list == 0 then list = { player } end

	local myName = tostring(player.Name or "")
	for _, p in ipairs(list) do
		local pname = tostring(p.Name or "Player")
		local isMe = (pname == myName)

		local row = ui:Row(page, "")

		-- Avatar (placeholder circular con la inicial: no podemos traer la
		-- miniatura real de Roblox; el usuario puede poner un ImageLabel).
		local av = Instance.new("Frame")
		av.Size = UDim2.new(0, 32, 0, 32)
		av.Position = UDim2.new(0, 8, 0.5, -16)
		av.BackgroundColor3 = isMe and ui.Theme.Accent or Color3.fromRGB(70, 74, 84)
		av.Parent = row
		ui.corner(av, 16)

		local ini = Instance.new("TextLabel")
		ini.BackgroundTransparency = 1
		ini.Size = UDim2.new(1, 0, 1, 0)
		ini.Text = string.upper(string.sub(pname, 1, 1))
		ini.TextColor3 = ui.Theme.Text
		ini.TextSize = 16
		ini.Parent = av

		local nm = ui.label(row, pname .. (isMe and "  (Tú)" or ""), 16, ui.Theme.Text)
		nm.Position = UDim2.new(0, 52, 0, 0)
		nm.Size = UDim2.new(1, -64, 1, 0)
	end

	return Players
end

return Players
)LUAU";

// ── ServerScriptService/GameManager.luau ──
static const char* LUAU_TEMPLATE_GAME_MANAGER_PATH = "ServerScriptService/GameManager.luau";
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

#endif // GL_LUAU_TEMPLATES_GEN_H
