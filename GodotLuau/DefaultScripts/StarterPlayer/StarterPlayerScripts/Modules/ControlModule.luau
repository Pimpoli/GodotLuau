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
