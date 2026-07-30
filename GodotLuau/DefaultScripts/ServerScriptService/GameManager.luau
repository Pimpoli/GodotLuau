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
