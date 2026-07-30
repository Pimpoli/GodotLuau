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
