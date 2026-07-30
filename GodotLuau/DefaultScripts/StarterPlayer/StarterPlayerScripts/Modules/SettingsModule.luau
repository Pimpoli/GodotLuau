-- > GodotLuau — PimpoliDev
-- SettingsModule.lua — ModuleScript — StarterPlayerScripts/Modules
-- Ajustes tipo Roblox, EDITABLE. Lo carga el PlayerModule con Settings.Init(player).
local Settings = {}

local Workspace = game:GetService("Workspace")
local UIS = game:GetService("UserInputService")

-- ── Colores / estilo (edítalos a gusto) ──────────────────────────────
local PANEL_BG   = Color3.fromRGB(24, 27, 34)
local ROW_BG     = Color3.fromRGB(38, 42, 52)
local ACCENT     = Color3.fromRGB(0, 162, 255)
local TEXT       = Color3.fromRGB(240, 242, 245)

local gui, panel, open = nil, nil, false
local quality = Workspace:GetGraphicsQuality()
local camLocked = false
local dynJoy = true

local function rounded(inst, r)
    local c = Instance.new("UICorner"); c.CornerRadius = UDim.new(0, r or 8); c.Parent = inst
end

-- Crea una fila "Etiqueta  [ - ] valor [ + ]"
local function stepperRow(parent, y, label, getText, onMinus, onPlus)
    local row = Instance.new("Frame")
    row.Size = UDim2.new(1, -20, 0, 40)
    row.Position = UDim2.new(0, 10, 0, y)
    row.BackgroundColor3 = ROW_BG
    row.Parent = parent
    rounded(row, 8)

    local lbl = Instance.new("TextLabel")
    lbl.BackgroundTransparency = 1
    lbl.Size = UDim2.new(0.5, 0, 1, 0)
    lbl.Position = UDim2.new(0, 12, 0, 0)
    lbl.Text = label
    lbl.TextColor3 = TEXT
    lbl.TextXAlignment = Enum and Enum.TextXAlignment and Enum.TextXAlignment.Left or 0
    lbl.Parent = row

    local val = Instance.new("TextLabel")
    val.BackgroundTransparency = 1
    val.Size = UDim2.new(0, 70, 1, 0)
    val.Position = UDim2.new(1, -140, 0, 0)
    val.TextColor3 = ACCENT
    val.Text = getText()
    val.Parent = row

    local function mkbtn(text, px, cb)
        local b = Instance.new("TextButton")
        b.Size = UDim2.new(0, 34, 0, 30)
        b.Position = UDim2.new(1, px, 0.5, -15)
        b.BackgroundColor3 = ACCENT
        b.TextColor3 = TEXT
        b.Text = text
        b.Parent = row
        rounded(b, 6)
        b.MouseButton1Click:Connect(function() cb(); val.Text = getText() end)
    end
    mkbtn("-", -70, onMinus)
    mkbtn("+", -34, onPlus)
end

local function toggleRow(parent, y, label, getOn, onToggle)
    local row = Instance.new("Frame")
    row.Size = UDim2.new(1, -20, 0, 40)
    row.Position = UDim2.new(0, 10, 0, y)
    row.BackgroundColor3 = ROW_BG
    row.Parent = parent
    rounded(row, 8)
    local lbl = Instance.new("TextLabel")
    lbl.BackgroundTransparency = 1
    lbl.Size = UDim2.new(0.7, 0, 1, 0)
    lbl.Position = UDim2.new(0, 12, 0, 0)
    lbl.Text = label
    lbl.TextColor3 = TEXT
    lbl.Parent = row
    local b = Instance.new("TextButton")
    b.Size = UDim2.new(0, 60, 0, 30)
    b.Position = UDim2.new(1, -70, 0.5, -15)
    b.Text = getOn() and "ON" or "OFF"
    b.BackgroundColor3 = getOn() and ACCENT or Color3.fromRGB(70, 74, 84)
    b.TextColor3 = TEXT
    b.Parent = row
    rounded(b, 6)
    b.MouseButton1Click:Connect(function()
        onToggle()
        b.Text = getOn() and "ON" or "OFF"
        b.BackgroundColor3 = getOn() and ACCENT or Color3.fromRGB(70, 74, 84)
    end)
end

local function build(player)
    gui = Instance.new("ScreenGui")
    gui.Name = "SettingsGui"
    gui.Parent = player:WaitForChild("PlayerGui")

    -- Botón de engranaje siempre visible (como Roblox móvil)
    local gear = Instance.new("TextButton")
    gear.Name = "SettingsButton"
    gear.Size = UDim2.new(0, 42, 0, 42)
    gear.Position = UDim2.new(1, -54, 0, 12)
    gear.AnchorPoint = Vector2.new(0, 0)
    gear.BackgroundColor3 = PANEL_BG
    gear.TextColor3 = TEXT
    gear.Text = "*"
    gear.Parent = gui
    rounded(gear, 10)

    panel = Instance.new("Frame")
    panel.Name = "Panel"
    panel.Size = UDim2.new(0, 360, 0, 300)
    panel.Position = UDim2.new(0.5, 0, 0.5, 0)
    panel.AnchorPoint = Vector2.new(0.5, 0.5)
    panel.BackgroundColor3 = PANEL_BG
    panel.Visible = false
    panel.Parent = gui
    rounded(panel, 14)

    local title = Instance.new("TextLabel")
    title.BackgroundTransparency = 1
    title.Size = UDim2.new(1, 0, 0, 40)
    title.Text = "Settings"
    title.TextColor3 = TEXT
    title.Parent = panel

    -- Calidad gráfica 1..10
    stepperRow(panel, 50, "Graphics Quality", function() return quality .. "/10" end,
        function() quality = math.max(1, quality - 1); Workspace:SetGraphicsQuality(quality) end,
        function() quality = math.min(10, quality + 1); Workspace:SetGraphicsQuality(quality) end)

    -- Bloquear cámara (3a persona se ve como 1a)
    toggleRow(panel, 100, "Lock Camera (1st person)", function() return camLocked end,
        function()
            camLocked = not camLocked
            player.CameraMode = camLocked and 1 or 0   -- Enum.CameraMode.LockFirstPerson / Classic
        end)

    -- Móvil: joystick dinámico (aparece donde tocas) vs fijo. Solo afecta al táctil.
    toggleRow(panel, 150, "Dynamic Joystick", function() return dynJoy end,
        function()
            dynJoy = not dynJoy
            local char = player.Character
            if char then char.JoystickDynamic = dynJoy end
        end)

    local close = Instance.new("TextButton")
    close.Size = UDim2.new(0, 120, 0, 34)
    close.Position = UDim2.new(0.5, 0, 1, -46)
    close.AnchorPoint = Vector2.new(0.5, 0)
    close.BackgroundColor3 = ACCENT
    close.TextColor3 = TEXT
    close.Text = "Close"
    close.Parent = panel
    rounded(close, 8)

    gear.MouseButton1Click:Connect(function() Settings.Toggle() end)
    close.MouseButton1Click:Connect(function() Settings.Close() end)
end

function Settings.Open()  if panel then open = true;  panel.Visible = true  end end
function Settings.Close() if panel then open = false; panel.Visible = false end end
function Settings.Toggle() if open then Settings.Close() else Settings.Open() end end

function Settings.Init(player)
    if gui then return Settings end
    build(player)
    return Settings
end

return Settings
