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
