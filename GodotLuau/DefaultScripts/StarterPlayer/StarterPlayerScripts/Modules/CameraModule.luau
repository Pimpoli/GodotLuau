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
