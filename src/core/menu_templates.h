#ifndef GL_MENU_TEMPLATES_H
#define GL_MENU_TEMPLATES_H

// ══════════════════════════════════════════════════════════════════════
//  Plantillas Luau del Menú (Escape) modular — 1.15
//
//  Estructura (StarterPlayer/StarterPlayerScripts/Modules/):
//    Menu (ModuleScript)          principal: arma el shell y las pestañas
//      MenuUi   (ModuleScript)    toolkit visual + shell (tabs, barra inferior)
//      Settings (ModuleScript)    pestaña "Config." (audio, gráficos, controles)
//      Players  (ModuleScript)    pestaña "Personas" (lista de jugadores)
//
//  TODO es EDITABLE por el usuario: colores, filas, pestañas, textos. El
//  PlayerModule hace require(Menu) y Menu.Init(player). El menú se abre con
//  Escape (UserInputService) y suprime el menú nativo del motor
//  (game:SetNativeMenuEnabled(false)).
//
//  Nota: estos strings compilan en RUNTIME (Luau). Validar SIEMPRE con
//  scratchpad/checkluau.exe antes de compilar el C++.
// ══════════════════════════════════════════════════════════════════════

// ── MenuUi: toolkit visual + shell ────────────────────────────────────
static const char* LUAU_TEMPLATE_MENU_UI = R"LUAU(
-- > GodotLuau — PimpoliDev
-- MenuUi.lua — ModuleScript — StarterPlayerScripts/Modules/Menu
-- Toolkit visual + "shell" del menú de Escape. EDITABLE: cambia colores,
-- tamaños, fuentes o añade widgets. Lo usa Menu para montar cada pestaña.

local MenuUi = {}

-- ── Tema (edítalo a gusto) ────────────────────────────────────────────
MenuUi.Theme = {
	Overlay = Color3.fromRGB(6, 8, 12),
	Panel   = Color3.fromRGB(16, 18, 24),
	Bar     = Color3.fromRGB(11, 13, 18),
	Row     = Color3.fromRGB(30, 33, 42),
	RowSel  = Color3.fromRGB(38, 42, 54),
	Accent  = Color3.fromRGB(0, 120, 255),
	Text    = Color3.fromRGB(238, 240, 244),
	Dim     = Color3.fromRGB(150, 156, 168),
	Green   = Color3.fromRGB(56, 200, 110),
	Red     = Color3.fromRGB(232, 78, 78),
}
local T = MenuUi.Theme

-- ── Helpers de construcción ───────────────────────────────────────────
local function corner(inst, r)
	local c = Instance.new("UICorner")
	c.CornerRadius = UDim.new(0, r or 8)
	c.Parent = inst
	return c
end
MenuUi.corner = corner

local function vlist(parent, gap)
	local l = Instance.new("UIListLayout")
	l.FillDirection = Enum.FillDirection.Vertical
	l.Padding = UDim.new(0, gap or 8)
	l.HorizontalAlignment = Enum.HorizontalAlignment.Center
	l.SortOrder = Enum.SortOrder.LayoutOrder
	l.Parent = parent
	return l
end
MenuUi.vlist = vlist

local function label(parent, text, size, color, xalign)
	local l = Instance.new("TextLabel")
	l.BackgroundTransparency = 1
	l.Text = text
	l.TextColor3 = color or T.Text
	l.TextSize = size or 16
	l.TextXAlignment = xalign or Enum.TextXAlignment.Left
	l.TextYAlignment = Enum.TextYAlignment.Center
	l.Parent = parent
	return l
end
MenuUi.label = label

-- ── Widgets de fila (para el contenido de las pestañas) ───────────────
-- Encabezado de sección: "Audio", "Pantalla y gráficos", etc.
function MenuUi:Section(page, title)
	local l = label(page, title, 20, T.Text)
	l.Size = UDim2.new(1, -8, 0, 34)
	l.TextXAlignment = Enum.TextXAlignment.Left
	return l
end

-- Fila base: fondo suave con la etiqueta a la izquierda; devuelve la fila
-- para que el caller ponga controles a la derecha (con AnchorPoint 1,0.5).
function MenuUi:Row(page, text, subtitle)
	local row = Instance.new("Frame")
	row.Size = UDim2.new(1, -8, 0, subtitle and 56 or 44)
	row.BackgroundColor3 = T.Row
	row.Parent = page
	corner(row, 8)

	local lbl = label(row, text, 16, T.Text)
	lbl.Position = UDim2.new(0, 14, 0, 0)
	lbl.Size = UDim2.new(0.55, 0, subtitle and 0.62 or 1, 0)

	if subtitle then
		local sub = label(row, subtitle, 12, T.Dim)
		sub.Position = UDim2.new(0, 14, 0.55, 0)
		sub.Size = UDim2.new(0.6, 0, 0.42, 0)
	end
	return row
end

-- Fila informativa: etiqueta + valor a la derecha (solo lectura).
function MenuUi:Info(page, text, value)
	local row = self:Row(page, text)
	local v = label(row, value, 16, T.Dim, Enum.TextXAlignment.Right)
	v.AnchorPoint = Vector2.new(1, 0.5)
	v.Position = UDim2.new(1, -16, 0.5, 0)
	v.Size = UDim2.new(0, 220, 1, 0)
	return v
end

-- Stepper estilo Roblox:  etiqueta   [ - ]  valor  [ + ]
function MenuUi:Stepper(page, text, getText, onMinus, onPlus)
	local row = self:Row(page, text)
	local function btn(sym, xoff)
		local b = Instance.new("TextButton")
		b.Size = UDim2.new(0, 34, 0, 30)
		b.AnchorPoint = Vector2.new(1, 0.5)
		b.Position = UDim2.new(1, xoff, 0.5, 0)
		b.BackgroundColor3 = T.Accent
		b.TextColor3 = T.Text
		b.TextSize = 20
		b.Text = sym
		b.Parent = row
		corner(b, 6)
		return b
	end
	local val = label(row, getText(), 16, T.Text, Enum.TextXAlignment.Center)
	val.AnchorPoint = Vector2.new(1, 0.5)
	val.Position = UDim2.new(1, -52, 0.5, 0)
	val.Size = UDim2.new(0, 120, 1, 0)

	local plus  = btn("+", -14)
	local minus = btn("-", -190)
	minus.MouseButton1Click:Connect(function() onMinus(); val.Text = getText() end)
	plus.MouseButton1Click:Connect(function() onPlus(); val.Text = getText() end)
	return val
end

-- Toggle ON/OFF
function MenuUi:Toggle(page, text, subtitle, getOn, onToggle)
	local row = self:Row(page, text, subtitle)
	local b = Instance.new("TextButton")
	b.Size = UDim2.new(0, 64, 0, 30)
	b.AnchorPoint = Vector2.new(1, 0.5)
	b.Position = UDim2.new(1, -16, 0.5, 0)
	b.TextColor3 = T.Text
	b.TextSize = 14
	b.Parent = row
	corner(b, 15)
	local function paint()
		b.Text = getOn() and "ON" or "OFF"
		b.BackgroundColor3 = getOn() and T.Accent or Color3.fromRGB(70, 74, 84)
	end
	paint()
	b.MouseButton1Click:Connect(function() onToggle(); paint() end)
	return b
end

-- ── Shell del menú (overlay + tabs + contenido + barra inferior) ──────
function MenuUi.new(player)
	local self = setmetatable({}, { __index = MenuUi })
	local pg = player:WaitForChild("PlayerGui")

	local gui = Instance.new("ScreenGui")
	gui.Name = "GodotLuauMenu"
	gui.ResetOnSpawn = false
	gui.DisplayOrder = 50
	gui.Parent = pg

	local overlay = Instance.new("Frame")
	overlay.Name = "Overlay"
	overlay.Size = UDim2.new(1, 0, 1, 0)
	overlay.BackgroundColor3 = T.Overlay
	overlay.BackgroundTransparency = 0.15
	overlay.Visible = false
	overlay.Parent = gui

	local panel = Instance.new("Frame")
	panel.Name = "Panel"
	panel.AnchorPoint = Vector2.new(0.5, 0.5)
	panel.Position = UDim2.new(0.5, 0, 0.5, 0)
	panel.Size = UDim2.new(1, -72, 1, -72)
	panel.BackgroundColor3 = T.Panel
	panel.Parent = overlay
	corner(panel, 16)

	-- Barra de pestañas
	local tabbar = Instance.new("Frame")
	tabbar.Name = "TabBar"
	tabbar.Size = UDim2.new(1, -24, 0, 52)
	tabbar.Position = UDim2.new(0, 12, 0, 6)
	tabbar.BackgroundTransparency = 1
	tabbar.Parent = panel
	local tlayout = Instance.new("UIListLayout")
	tlayout.FillDirection = Enum.FillDirection.Horizontal
	tlayout.Padding = UDim.new(0, 10)
	tlayout.VerticalAlignment = Enum.VerticalAlignment.Center
	tlayout.HorizontalAlignment = Enum.HorizontalAlignment.Left
	tlayout.Parent = tabbar

	-- Línea separadora
	local sep = Instance.new("Frame")
	sep.Size = UDim2.new(1, -24, 0, 1)
	sep.Position = UDim2.new(0, 12, 0, 60)
	sep.BackgroundColor3 = Color3.fromRGB(255, 255, 255)
	sep.BackgroundTransparency = 0.9
	sep.Parent = panel

	-- Contenido
	local content = Instance.new("Frame")
	content.Name = "Content"
	content.Position = UDim2.new(0, 12, 0, 70)
	content.Size = UDim2.new(1, -24, 1, -152)
	content.BackgroundTransparency = 1
	content.Parent = panel

	-- Barra inferior (Salir / Regenerar / Reanudar)
	local bottom = Instance.new("Frame")
	bottom.Name = "Bottom"
	bottom.AnchorPoint = Vector2.new(0.5, 1)
	bottom.Position = UDim2.new(0.5, 0, 1, -12)
	bottom.Size = UDim2.new(1, -24, 0, 56)
	bottom.BackgroundTransparency = 1
	bottom.Parent = panel

	self.gui, self.overlay, self.panel = gui, overlay, panel
	self.tabbar, self.content, self.bottom = tabbar, content, bottom
	self.pages, self.tabButtons, self.current = {}, {}, nil
	self.isOpen = false
	return self
end

-- Crea una pestaña (botón arriba + página de contenido). Devuelve la página.
function MenuUi:AddTab(name)
	local btn = Instance.new("TextButton")
	btn.Name = "Tab_" .. name
	btn.Size = UDim2.new(0, 132, 0, 40)
	btn.BackgroundTransparency = 1
	btn.TextColor3 = T.Dim
	btn.TextSize = 17
	btn.Text = name
	btn.Parent = self.tabbar
	btn.MouseButton1Click:Connect(function() self:ShowTab(name) end)
	self.tabButtons[name] = btn

	local page = Instance.new("Frame")
	page.Name = "Page_" .. name
	page.Size = UDim2.new(1, 0, 1, 0)
	page.BackgroundTransparency = 1
	page.Visible = false
	page.Parent = self.content
	vlist(page, 8)
	self.pages[name] = page
	return page
end

function MenuUi:ShowTab(name)
	for n, page in pairs(self.pages) do page.Visible = (n == name) end
	for n, btn in pairs(self.tabButtons) do
		btn.TextColor3 = (n == name) and T.Text or T.Dim
	end
	self.current = name
end

-- Barra inferior: 3 botones tipo Roblox (tecla + texto).
function MenuUi:BuildBottom(specs)
	local n = #specs
	for i, s in ipairs(specs) do
		local b = Instance.new("TextButton")
		b.Size = UDim2.new(1 / n, -10, 1, 0)
		b.Position = UDim2.new((i - 1) / n, 5, 0, 0)
		b.BackgroundColor3 = s.color or T.Row
		b.TextColor3 = T.Text
		b.TextSize = 17
		b.Text = "  " .. (s.key and ("[" .. s.key .. "]  ") or "") .. s.text
		b.Parent = self.bottom
		corner(b, 8)
		b.MouseButton1Click:Connect(s.cb)
	end
end

function MenuUi:Open()  self.overlay.Visible = true;  self.isOpen = true end
function MenuUi:Close() self.overlay.Visible = false; self.isOpen = false end
function MenuUi:Toggle() if self.isOpen then self:Close() else self:Open() end end

return MenuUi
)LUAU";

// ── Settings: pestaña "Config." ───────────────────────────────────────
static const char* LUAU_TEMPLATE_MENU_SETTINGS = R"LUAU(
-- > GodotLuau — PimpoliDev
-- Settings.lua — ModuleScript — Modules/Menu
-- Pestaña "Config." del menú. EDITABLE: añade/quita filas y secciones.
-- Aplica de verdad: calidad 1..10, FPS máx, volumen, bloqueo de cámara y
-- joystick dinámico (móvil).

local Settings = {}
local Workspace = game:GetService("Workspace")

function Settings.Build(ui, page, player)
	local quality = 8
	pcall(function() quality = Workspace:GetGraphicsQuality() end)
	local volume = 100
	local fpsOptions = { 30, 60, 120, 144, 240, 0 }   -- 0 = ilimitado
	local fpsIdx = 3
	local camLocked = false
	local dynJoy = true

	-- ── Pantalla y gráficos ──────────────────────────────────────────
	ui:Section(page, "Pantalla y gráficos")
	ui:Stepper(page, "Calidad de gráficos",
		function() return quality .. " / 10" end,
		function() quality = math.max(1, quality - 1); Workspace:SetGraphicsQuality(quality) end,
		function() quality = math.min(10, quality + 1); Workspace:SetGraphicsQuality(quality) end)
	ui:Stepper(page, "Velocidad máxima de fotogramas",
		function() local f = fpsOptions[fpsIdx]; return f == 0 and "Ilimitado" or (f .. " FPS") end,
		function() fpsIdx = math.max(1, fpsIdx - 1); Workspace:SetMaxFPS(fpsOptions[fpsIdx]) end,
		function() fpsIdx = math.min(#fpsOptions, fpsIdx + 1); Workspace:SetMaxFPS(fpsOptions[fpsIdx]) end)

	-- ── Audio ────────────────────────────────────────────────────────
	ui:Section(page, "Audio")
	ui:Stepper(page, "Volumen",
		function() return volume .. "%" end,
		function() volume = math.max(0, volume - 10); Workspace:SetMasterVolume(volume / 100) end,
		function() volume = math.min(100, volume + 10); Workspace:SetMasterVolume(volume / 100) end)

	-- ── Vista y controles ────────────────────────────────────────────
	ui:Section(page, "Vista y controles")
	ui:Toggle(page, "Bloquear cámara (1ª persona)", "En 3ª persona se verá como en 1ª persona",
		function() return camLocked end,
		function()
			camLocked = not camLocked
			player.CameraMode = camLocked and Enum.CameraMode.LockFirstPerson or Enum.CameraMode.Classic
		end)
	ui:Toggle(page, "Joystick dinámico (móvil)", "Aparece donde tocas, como en Roblox",
		function() return dynJoy end,
		function()
			dynJoy = not dynJoy
			local char = player.Character
			if char then char.JoystickDynamic = dynJoy end
		end)

	return Settings
end

return Settings
)LUAU";

// ── Players: pestaña "Personas" ───────────────────────────────────────
static const char* LUAU_TEMPLATE_MENU_PLAYERS = R"LUAU(
-- > GodotLuau — PimpoliDev
-- Players.lua — ModuleScript — Modules/Menu
-- Pestaña "Personas" del menú: lista de jugadores en el servidor. EDITABLE.

local Players = {}
local PlayersService = game:GetService("Players")

function Players.Build(ui, page, player)
	ui:Section(page, "En este servidor")

	local list = {}
	pcall(function() list = PlayersService:GetPlayers() end)
	if #list == 0 then list = { player } end

	local myName = tostring(player.Name or "")
	for _, p in ipairs(list) do
		local pname = tostring(p.Name or "Player")
		local isMe = (pname == myName)

		local row = ui:Row(page, "")

		-- Avatar (placeholder circular con la inicial: no podemos traer la
		-- miniatura real de Roblox; el usuario puede poner un ImageLabel).
		local av = Instance.new("Frame")
		av.Size = UDim2.new(0, 32, 0, 32)
		av.Position = UDim2.new(0, 8, 0.5, -16)
		av.BackgroundColor3 = isMe and ui.Theme.Accent or Color3.fromRGB(70, 74, 84)
		av.Parent = row
		ui.corner(av, 16)

		local ini = Instance.new("TextLabel")
		ini.BackgroundTransparency = 1
		ini.Size = UDim2.new(1, 0, 1, 0)
		ini.Text = string.upper(string.sub(pname, 1, 1))
		ini.TextColor3 = ui.Theme.Text
		ini.TextSize = 16
		ini.Parent = av

		local nm = ui.label(row, pname .. (isMe and "  (Tú)" or ""), 16, ui.Theme.Text)
		nm.Position = UDim2.new(0, 52, 0, 0)
		nm.Size = UDim2.new(1, -64, 1, 0)
	end

	return Players
end

return Players
)LUAU";

// ── Menu: principal ───────────────────────────────────────────────────
// El menú de Escape (Personas/Config./Galería/Denunciar/Ayuda, con todos los
// ajustes, el botón-logo arriba-izquierda y el toggle de chat) lo gestiona el
// MOTOR de forma nativa: siempre funciona y se ve como Roblox. Este módulo queda
// como punto de extensión FUTURO y por defecto NO hace nada (así no compite con
// el menú nativo). MenuUi/Settings/Players quedan como ejemplos editables.
static const char* LUAU_TEMPLATE_MENU = R"LUAU(
-- > GodotLuau — PimpoliDev
-- Menu.lua — ModuleScript — StarterPlayerScripts/Modules/Menu
-- El menú de Escape lo dibuja el MOTOR (nativo, estilo Roblox, con todos los
-- ajustes y el botón-logo arriba-izquierda). Este módulo es un punto de
-- extensión: por defecto no hace nada. Pon tu logo en res://gamelogo.png.
local Menu = {}
function Menu.Init(player)
	-- Aquí puedes añadir tu propia UI extra si quieres. El menú principal ya
	-- lo gestiona el motor, así que este Init se deja vacío a propósito.
end
return Menu
)LUAU";

#endif // GL_MENU_TEMPLATES_H
