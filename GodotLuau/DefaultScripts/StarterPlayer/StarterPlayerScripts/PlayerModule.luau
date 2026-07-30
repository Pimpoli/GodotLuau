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
