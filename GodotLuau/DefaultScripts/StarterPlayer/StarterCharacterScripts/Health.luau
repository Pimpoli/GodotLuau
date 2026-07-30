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
