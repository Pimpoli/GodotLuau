-- Corpus de entrenamiento: patrones idiomáticos de código Roblox/Luau
-- Usado por entrenar_modelo.py para generar LuauGram-Plus.
-- Cuanto más variado y realista, mejores predicciones.

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local TweenService = game:GetService("TweenService")
local UserInputService = game:GetService("UserInputService")
local SoundService = game:GetService("SoundService")
local Lighting = game:GetService("Lighting")
local CollectionService = game:GetService("CollectionService")
local TextChatService = game:GetService("TextChatService")
local Debris = game:GetService("Debris")

local player = Players.LocalPlayer
local character = player.Character
local humanoid = character:FindFirstChild("Humanoid")
local humanoid = character:WaitForChild("Humanoid")
local rootPart = character:WaitForChild("HumanoidRootPart")
local camera = workspace.CurrentCamera

local part = Instance.new("Part")
part.Name = "MiParte"
part.Position = Vector3.new(0, 5, 0)
part.Size = Vector3.new(4, 1, 2)
part.Anchored = true
part.CanCollide = true
part.Transparency = 0.5
part.Color = Color3.fromRGB(255, 0, 0)
part.Parent = workspace

local spawnLocation = workspace:FindFirstChild("SpawnLocation")
local baseplate = workspace:FindFirstChild("Baseplate")

humanoid.WalkSpeed = 16
humanoid.JumpPower = 50
humanoid.Health = humanoid.MaxHealth
humanoid.MaxHealth = 100

humanoid.Died:Connect(function()
    print("El jugador ha muerto")
    task.wait(3)
end)

humanoid.HealthChanged:Connect(function(newHealth)
    if newHealth < humanoid.MaxHealth * 0.25 then
        warn("Salud baja")
    end
end)

Players.PlayerAdded:Connect(function(newPlayer)
    print(newPlayer.Name .. " ha entrado")
end)

Players.PlayerRemoving:Connect(function(oldPlayer)
    print(oldPlayer.Name .. " ha salido")
end)

RunService.Heartbeat:Connect(function(deltaTime)
    local speed = humanoid.WalkSpeed
    if UserInputService:IsKeyDown("LeftShift") then
        humanoid.WalkSpeed = 24
    else
        humanoid.WalkSpeed = 16
    end
end)

RunService.RenderStepped:Connect(function(deltaTime)
    camera.Position = rootPart.Position + Vector3.new(0, 10, 10)
end)

part.Touched:Connect(function(hit)
    local hitHumanoid = hit.Parent:FindFirstChild("Humanoid")
    if hitHumanoid then
        hitHumanoid:TakeDamage(10)
    end
end)

local killBrick = workspace:FindFirstChild("KillBrick")
killBrick.Touched:Connect(function(hit)
    local hitHumanoid = hit.Parent:FindFirstChild("Humanoid")
    if hitHumanoid then
        hitHumanoid.Health = 0
    end
end)

local remoteEvent = Instance.new("RemoteEvent")
remoteEvent.Name = "DamageEvent"
remoteEvent.Parent = ReplicatedStorage

remoteEvent.OnServerEvent:Connect(function(firingPlayer, targetName, amount)
    if type(amount) ~= "number" then return end
    amount = math.clamp(amount, 0, 50)
    local target = workspace:FindFirstChild(targetName)
    if target then
        local targetHumanoid = target:FindFirstChild("Humanoid")
        if targetHumanoid then
            targetHumanoid:TakeDamage(amount)
        end
    end
end)

remoteEvent.OnClientEvent:Connect(function(message)
    print("Mensaje del servidor: " .. message)
end)

remoteEvent:FireServer("Enemigo", 25)
remoteEvent:FireAllClients("La ronda ha comenzado")

local bindable = Instance.new("BindableEvent")
bindable.Event:Connect(function(data)
    print(data)
end)
bindable:Fire("hola")

local tweenInfo = TweenInfo.new(1, Enum.EasingStyle.Quad, Enum.EasingDirection.Out)
local tween = TweenService:Create(part, tweenInfo, { Position = Vector3.new(0, 10, 0) })
tween:Play()

local sound = Instance.new("Sound")
sound.Name = "Musica"
sound.Volume = 0.5
sound.Looped = true
sound.Parent = workspace
sound:Play()
sound:Stop()

Lighting.ClockTime = 14
Lighting.Brightness = 2
Lighting.FogEnd = 500
Lighting.FogColor = Color3.fromRGB(200, 200, 255)
Lighting.DayNightCycle = true
Lighting.DayLengthMinutes = 10
Lighting:SetMinutesAfterMidnight(8 * 60)

local function onTouched(hit)
    local hitCharacter = hit.Parent
    local hitPlayer = Players:GetPlayerFromCharacter(hitCharacter)
    if hitPlayer then
        print(hitPlayer.Name .. " toco la parte")
    end
end
part.Touched:Connect(onTouched)

local function crearParte(posicion, color)
    local nueva = Instance.new("Part")
    nueva.Position = posicion
    nueva.Color = color
    nueva.Anchored = true
    nueva.Parent = workspace
    return nueva
end

for i = 1, 10 do
    local escalon = crearParte(Vector3.new(0, i * 2, i * 4), Color3.fromRGB(100, 100, 100))
    escalon.Size = Vector3.new(6, 1, 4)
end

for index, child in ipairs(workspace:GetChildren()) do
    if child:IsA("Part") then
        child.Transparency = 0
    end
end

for key, value in pairs(workspace:GetDescendants()) do
    if value:IsA("Sound") then
        value:Stop()
    end
end

local enemigos = workspace:FindFirstChild("Enemigos")
if enemigos then
    for index, enemigo in ipairs(enemigos:GetChildren()) do
        local enemigoHumanoid = enemigo:FindFirstChild("Humanoid")
        if enemigoHumanoid and enemigoHumanoid.Health > 0 then
            enemigoHumanoid:TakeDamage(5)
        end
    end
end

while true do
    task.wait(1)
    print("tick")
end

while task.wait(0.5) do
    if humanoid.Health <= 0 then
        break
    end
end

task.spawn(function()
    task.wait(5)
    print("Pasaron 5 segundos")
end)

task.delay(2, function()
    part:Destroy()
end)

local ok, err = pcall(function()
    local riesgoso = workspace.NoExiste.Tampoco
end)
if not ok then
    warn("Error: " .. tostring(err))
end

local clone = part:Clone()
clone.Parent = workspace
clone.Position = part.Position + Vector3.new(5, 0, 0)
Debris:AddItem(clone, 10)

part:SetAttribute("Dueno", player.Name)
local dueno = part:GetAttribute("Dueno")
if part:GetAttribute("Activo") then
    part:SetAttribute("Activo", false)
end

local distancia = (rootPart.Position - part.Position).Magnitude
if distancia < 10 then
    print("Cerca de la parte")
end

local direccion = (part.Position - rootPart.Position).Unit
local nuevaPos = rootPart.Position + direccion * 5

local Modulo = {}
Modulo.__index = Modulo

function Modulo.new(nombre)
    local self = setmetatable({}, Modulo)
    self.Nombre = nombre
    self.Nivel = 1
    return self
end

function Modulo:SubirNivel()
    self.Nivel = self.Nivel + 1
    return self.Nivel
end

function Modulo:GetNombre()
    return self.Nombre
end

return Modulo

local Utils = require(script.Parent.Modules.Utils)
local config = require(script.Parent.Config)

local datos = {
    nombre = "Jugador1",
    nivel = 5,
    monedas = 100,
}
table.insert(datos, "nuevo")
table.remove(datos, 1)
local encontrado = table.find(datos, "nuevo")

local texto = string.format("Nivel: %d", datos.nivel)
local mayus = string.upper(texto)
local sub = string.sub(texto, 1, 5)

local aleatorio = math.random(1, 100)
local redondeado = math.floor(3.7)
local limitado = math.clamp(aleatorio, 0, 50)
local mayor = math.max(10, 20)
local menor = math.min(10, 20)

if humanoid.Health > 50 and humanoid.WalkSpeed > 10 then
    print("Sano y rapido")
elseif humanoid.Health > 0 or humanoid.MaxHealth > 100 then
    print("Vivo")
else
    print("Muerto")
end

if not character then
    character = player.CharacterAdded:Wait()
end

local mensaje = player.Name .. " tiene " .. tostring(humanoid.Health) .. " de vida"
print(mensaje)
warn("Cuidado con esto")

UserInputService.InputBegan:Connect(function(input, gameProcessed)
    if gameProcessed then return end
    if input.KeyCode == Enum.KeyCode.E then
        print("Tecla E presionada")
    end
end)

if UserInputService:IsKeyDown("Space") then
    humanoid.Jump = true
end

local prompt = Instance.new("ProximityPrompt")
prompt.ActionText = "Abrir"
prompt.Parent = part
prompt.Triggered:Connect(function(triggeringPlayer)
    print(triggeringPlayer.Name .. " activo el prompt")
end)

local detector = Instance.new("ClickDetector")
detector.Parent = part
detector.MouseClick:Connect(function(clickingPlayer)
    part.Color = Color3.fromRGB(0, 255, 0)
end)

CollectionService:AddTag(part, "Recolectable")
for index, etiquetado in ipairs(CollectionService:GetTagged("Recolectable")) do
    etiquetado.Transparency = 0.2
end

TextChatService:SendMessage("Hola a todos")
TextChatService.MessageReceived:Connect(function(mensajeRecibido)
    print(mensajeRecibido)
end)

player.CameraMode = 2
camera.FieldOfView = 70

local leaderstats = Instance.new("Folder")
leaderstats.Name = "leaderstats"
leaderstats.Parent = player

local monedas = Instance.new("IntValue")
monedas.Name = "Monedas"
monedas.Value = 0
monedas.Parent = leaderstats
monedas.Value = monedas.Value + 10
