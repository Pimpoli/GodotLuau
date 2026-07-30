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
