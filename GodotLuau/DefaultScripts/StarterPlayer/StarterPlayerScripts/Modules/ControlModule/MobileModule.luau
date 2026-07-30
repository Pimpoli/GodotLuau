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
