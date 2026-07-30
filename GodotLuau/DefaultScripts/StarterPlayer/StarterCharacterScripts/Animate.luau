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
