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
