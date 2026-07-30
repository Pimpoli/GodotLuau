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

	-- ── Graphics ─────────────────────────────────────────────────────
	-- EDITABLE: three modes, like Roblox.
	--   Automatic — the engine raises/lowers quality by itself to hold the FPS
	--   Manual    — one fixed quality level (1..10)
	--   Custom    — every setting separately (resolution/FSR, shadows, view...)
	-- Delete any block below if you only want some of them.
	local MODES = { "Automatic", "Manual", "Custom" }
	local modeIdx = 2                      -- 1=Automatic 2=Manual 3=Custom
	local targetFps = 45
	pcall(function() modeIdx = Workspace:GetGraphicsMode() + 1 end)

	ui:Section(page, "Graphics")
	ui:Stepper(page, "Mode",
		function() return MODES[modeIdx] end,
		function() modeIdx = math.max(1, modeIdx - 1); Workspace:SetGraphicsMode(modeIdx - 1) end,
		function() modeIdx = math.min(#MODES, modeIdx + 1); Workspace:SetGraphicsMode(modeIdx - 1) end)

	-- Automatic: target frame rate to hold
	ui:Stepper(page, "Target FPS (Automatic)",
		function() return tostring(targetFps) end,
		function() targetFps = math.max(30, targetFps - 15); Workspace:SetAutoTargetFPS(targetFps) end,
		function() targetFps = math.min(240, targetFps + 15); Workspace:SetAutoTargetFPS(targetFps) end)

	-- Manual: one level for everything
	ui:Stepper(page, "Quality level (Manual)",
		function() return quality .. " / 10" end,
		function() quality = math.max(1, quality - 1); Workspace:SetGraphicsQuality(quality) end,
		function() quality = math.min(10, quality + 1); Workspace:SetGraphicsQuality(quality) end)

	-- Custom: each setting on its own. Remove the ones you don't want exposed.
	local function customStepper(label, key, values, fmt)
		local idx = 1
		pcall(function()
			local cur = Workspace:GetGraphicsSetting(key)
			for i, v in ipairs(values) do if math.abs(v - cur) < 0.001 then idx = i end end
		end)
		ui:Stepper(page, label,
			function() return fmt(values[idx]) end,
			function() idx = math.max(1, idx - 1); Workspace:SetGraphicsSetting(key, values[idx]) end,
			function() idx = math.min(#values, idx + 1); Workspace:SetGraphicsSetting(key, values[idx]) end)
	end

	local function pct(v) return math.floor(v * 100 + 0.5) .. "%" end
	customStepper("Render resolution (Custom)", "RenderScale",
		{ 0.25, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0 }, pct)
	customStepper("Upscaler (Custom)", "Upscaler", { 0, 1, 2 },
		function(v) return ({ [0] = "Bilinear", [1] = "FSR", [2] = "FSR 2" })[v] or "?" end)
	customStepper("Sharpness (Custom)", "Sharpness", { 0, 0.25, 0.5, 0.75, 1.0 }, pct)
	customStepper("Shadows (Custom)", "ShadowQuality", { 0, 1, 2, 3, 4, 5 },
		function(v) return v == 0 and "Off" or (v .. " / 5") end)
	customStepper("View distance (Custom)", "ViewDistance",
		{ 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 }, pct)
	-- Occlusion culling: skips what is hidden behind walls. It is OFF by default
	-- because the culling runs on the CPU: with thousands of small parts it costs
	-- more than it saves. It pays off on maps with few, large walls.
	-- Transparency drawn by DISCARDING PIXELS instead of real blending. Real
	-- transparency makes the GPU sort and blend every part against what is behind
	-- it; discarding pixels draws it like a solid and costs far less (you may see
	-- a fine dither pattern up close). Parts with Transparency = 1 are always
	-- skipped entirely, with or without this option.
	customStepper("Pixel transparency (Custom)", "PixelTransparency", { 0, 1 },
		function(v) return v > 0.5 and "On" or "Off" end)
	customStepper("Occlusion culling (Custom)", "Occlusion", { 0, 1 },
		function(v) return v > 0.5 and "On" or "Off" end)
	customStepper("Occluder min size (Custom)", "OcclusionSize",
		{ 4, 8, 16, 32 }, function(v) return v .. " studs" end)
	customStepper("Occlusion quality (Custom)", "OcclusionQuality", { 0, 1, 2 },
		function(v) return ({ [0] = "Fast", [1] = "Balanced", [2] = "Precise" })[v] or "?" end)
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
