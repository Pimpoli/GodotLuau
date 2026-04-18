@tool
extends EditorPlugin

const VERSION_URL  := "https://raw.githubusercontent.com/Pimpoli/GodotLuau/main/Version"
const ZIP_URL      := "https://github.com/Pimpoli/GodotLuau/raw/main/GodotLuau.zip"
const VERSION_FILE := "res://addons/GodotLuauUpdater/installed_version.txt"
const DATA_FILE    := "user://godotluau_usage.json"
const DLL_EXTENSIONS := ["dll", "so", "dylib", "framework"]

var _http_version   : HTTPRequest    = null
var _http_download  : HTTPRequest    = null
var _bar            : PanelContainer = null
var _bar_label      : Label          = null
var _bar_btn        : Button         = null
var _remote_version : String         = ""
var _downloading    : bool           = false
var _settings_panel : Control        = null
var _stats_label    : Label          = null

# ── Lifecycle ────────────────────────────────────────────────────────────────

func _enter_tree() -> void:
	_cleanup_old_dlls()
	_register_settings()
	_build_notification_bar()
	_build_settings_panel()
	_check_for_update()

func _exit_tree() -> void:
	if _http_version  and is_instance_valid(_http_version):  _http_version.queue_free()
	if _http_download and is_instance_valid(_http_download): _http_download.queue_free()
	if _bar and is_instance_valid(_bar):
		remove_control_from_bottom_panel(_bar)
		_bar.queue_free()
	if _settings_panel and is_instance_valid(_settings_panel):
		remove_control_from_bottom_panel(_settings_panel)
		_settings_panel.queue_free()

# ── Startup cleanup — remove old DLL backups left by previous updates ─────────

func _cleanup_old_dlls() -> void:
	var bin_path := ProjectSettings.globalize_path("res://bin/")
	if not DirAccess.dir_exists_absolute(bin_path):
		return
	var dir := DirAccess.open(bin_path)
	if not dir:
		return
	dir.list_dir_begin()
	var f := dir.get_next()
	while f != "":
		if f.ends_with(".dll.old") or f.ends_with(".so.old") or f.ends_with(".dylib.old"):
			DirAccess.remove_absolute(bin_path + f)
		f = dir.get_next()

# ── ProjectSettings registration ──────────────────────────────────────────────

func _register_settings() -> void:
	_add_bool_setting("godot_luau/ai_autocomplete_enabled", false,
		"Enable AI Smart Autocomplete — suggests values based on variable names.")
	_add_bool_setting("godot_luau/share_data_enabled", true,
		"Share anonymous usage data to improve the AI model.")
	_add_bool_setting("godot_luau/debug_mode", false,
		"Show debug output from the GodotLuau runtime in the Output panel.")

func _add_bool_setting(key: String, default_value: bool, hint: String) -> void:
	if not ProjectSettings.has_setting(key):
		ProjectSettings.set_setting(key, default_value)
	ProjectSettings.set_initial_value(key, default_value)
	ProjectSettings.add_property_info({
		"name": key, "type": TYPE_BOOL,
		"hint": PROPERTY_HINT_NONE, "hint_string": hint
	})

# ── AI Settings panel ─────────────────────────────────────────────────────────

func _build_settings_panel() -> void:
	var root := PanelContainer.new()
	root.size_flags_horizontal = Control.SIZE_EXPAND_FILL

	var margin := MarginContainer.new()
	for side in ["margin_left","margin_right","margin_top","margin_bottom"]:
		margin.add_theme_constant_override(side, 12)
	root.add_child(margin)

	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 10)
	margin.add_child(vbox)

	# ── Header ──
	var header := Label.new()
	header.text = "GodotLuau — AI & Autocomplete Settings"
	header.add_theme_color_override("font_color", Color(0.4, 0.8, 1.0))
	header.add_theme_font_size_override("font_size", 15)
	vbox.add_child(header)

	# Version row
	var ver_lbl := Label.new()
	ver_lbl.text = "Installed: %s" % _get_local_version()
	ver_lbl.add_theme_color_override("font_color", Color(0.5, 0.5, 0.5))
	ver_lbl.add_theme_font_size_override("font_size", 11)
	vbox.add_child(ver_lbl)

	vbox.add_child(HSeparator.new())

	# ── AI Autocomplete toggle ──
	vbox.add_child(_make_setting_row(
		"AI Smart Autocomplete",
		"Sugiere valores según tus nombres de variable (ej: escribir 'speed =' sugiere '16').\nDesactivado por defecto — el sistema sigue recolectando datos para mejorar.",
		"godot_luau/ai_autocomplete_enabled", false
	))

	vbox.add_child(HSeparator.new())

	# ── Data sharing toggle ──
	vbox.add_child(_make_setting_row(
		"Compartir datos de uso (anónimos)",
		"Mejora las sugerencias futuras enviando patrones de uso anónimos.\nNunca se comparte contenido de código — solo qué APIs usas más.",
		"godot_luau/share_data_enabled", true
	))

	vbox.add_child(HSeparator.new())

	# ── Debug mode toggle ──
	vbox.add_child(_make_setting_row(
		"Modo Debug",
		"Muestra salida de depuración del runtime GodotLuau en el panel Output.",
		"godot_luau/debug_mode", false
	))

	vbox.add_child(HSeparator.new())

	# ── Data stats & actions ──
	var stats_header := Label.new()
	stats_header.text = "Datos recolectados de uso"
	stats_header.add_theme_font_size_override("font_size", 13)
	vbox.add_child(stats_header)

	_stats_label = Label.new()
	_stats_label.add_theme_color_override("font_color", Color(0.65, 0.65, 0.65))
	_stats_label.add_theme_font_size_override("font_size", 11)
	vbox.add_child(_stats_label)
	_refresh_stats()

	var btn_row := HBoxContainer.new()
	btn_row.add_theme_constant_override("separation", 8)
	vbox.add_child(btn_row)

	var btn_refresh := Button.new()
	btn_refresh.text = "↺ Actualizar stats"
	btn_refresh.pressed.connect(_refresh_stats)
	btn_row.add_child(btn_refresh)

	var btn_open := Button.new()
	btn_open.text = "📂 Abrir carpeta de datos"
	btn_open.pressed.connect(func():
		OS.shell_open(OS.get_user_data_dir())
	)
	btn_row.add_child(btn_open)

	var btn_clear := Button.new()
	btn_clear.text = "🗑 Borrar datos"
	btn_clear.add_theme_color_override("font_color", Color(1.0, 0.5, 0.5))
	btn_clear.pressed.connect(_confirm_clear_data)
	btn_row.add_child(btn_clear)

	vbox.add_child(HSeparator.new())

	# ── Update section ──
	var upd_header := Label.new()
	upd_header.text = "Actualizaciones"
	upd_header.add_theme_font_size_override("font_size", 13)
	vbox.add_child(upd_header)

	var upd_row := HBoxContainer.new()
	upd_row.add_theme_constant_override("separation", 8)
	vbox.add_child(upd_row)

	var btn_check := Button.new()
	btn_check.text = "🔍 Buscar actualizaciones"
	btn_check.pressed.connect(_check_for_update)
	upd_row.add_child(btn_check)

	# ── Info note ──
	var note := RichTextLabel.new()
	note.bbcode_enabled = true
	note.fit_content = true
	note.scroll_active = false
	note.text = "[color=#666666]Los datos se guardan en [b]user://godotluau_usage.json[/b].\nContribuye al modelo en: [color=#4d9de0]github.com/Pimpoli/IALuauAutoCompleted[/color][/color]"
	vbox.add_child(note)

	_settings_panel = root
	add_control_to_bottom_panel(root, "GodotLuau AI")

func _make_setting_row(title: String, description: String, setting_key: String, default_val: bool) -> Control:
	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 4)

	var hbox := HBoxContainer.new()
	hbox.add_theme_constant_override("separation", 10)

	var toggle := CheckButton.new()
	toggle.button_pressed = ProjectSettings.get_setting(setting_key, default_val)
	toggle.toggled.connect(func(pressed: bool) -> void:
		ProjectSettings.set_setting(setting_key, pressed)
		ProjectSettings.save()
	)
	hbox.add_child(toggle)

	var label := Label.new()
	label.text = title
	label.add_theme_font_size_override("font_size", 13)
	hbox.add_child(label)
	vbox.add_child(hbox)

	var desc := Label.new()
	desc.text = description
	desc.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	desc.add_theme_color_override("font_color", Color(0.65, 0.65, 0.65))
	desc.add_theme_font_size_override("font_size", 11)
	vbox.add_child(desc)

	return vbox

func _refresh_stats() -> void:
	if not _stats_label or not is_instance_valid(_stats_label):
		return
	var path := OS.get_user_data_dir() + "/godotluau_usage.json"
	if not FileAccess.file_exists(path):
		_stats_label.text = "Sin datos recolectados todavía."
		return
	var f := FileAccess.open(path, FileAccess.READ)
	if not f:
		_stats_label.text = "No se pudo leer el archivo de datos."
		return
	var text := f.get_as_text()
	var size_kb := f.get_length() / 1024.0
	# Count entries by counting newlines or array items
	var entry_count := text.count("\n")
	_stats_label.text = "Tamaño: %.1f KB  |  Entradas aprox.: %d" % [size_kb, entry_count]

func _confirm_clear_data() -> void:
	var dialog := ConfirmationDialog.new()
	dialog.title = "Borrar datos de uso"
	dialog.dialog_text = "¿Seguro que quieres eliminar todos los datos de uso recolectados?\nEsto no afecta el funcionamiento del plugin."
	dialog.confirmed.connect(_clear_data)
	get_editor_interface().get_base_control().add_child(dialog)
	dialog.popup_centered()

func _clear_data() -> void:
	var path := OS.get_user_data_dir() + "/godotluau_usage.json"
	if FileAccess.file_exists(path):
		DirAccess.remove_absolute(path)
	_refresh_stats()
	_set_bar("🗑 Datos de uso eliminados.", "", Color(0.5, 0.9, 0.5))

# ── Notification bar ──────────────────────────────────────────────────────────

func _build_notification_bar() -> void:
	_bar = PanelContainer.new()
	_bar.visible = false

	var hbox := HBoxContainer.new()
	hbox.add_theme_constant_override("separation", 8)
	_bar.add_child(hbox)

	var brand := Label.new()
	brand.text = "GodotLuau"
	brand.add_theme_color_override("font_color", Color(0.4, 0.8, 1.0))
	hbox.add_child(brand)

	_bar_label = Label.new()
	_bar_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hbox.add_child(_bar_label)

	_bar_btn = Button.new()
	_bar_btn.flat = true
	_bar_btn.pressed.connect(_on_action_pressed)
	hbox.add_child(_bar_btn)

	var close_btn := Button.new()
	close_btn.flat = true
	close_btn.text = "✕"
	close_btn.pressed.connect(func(): _bar.visible = false)
	hbox.add_child(close_btn)

	add_control_to_bottom_panel(_bar, "GodotLuau")

# ── Version check ─────────────────────────────────────────────────────────────

func _check_for_update() -> void:
	if _http_version and is_instance_valid(_http_version):
		return  # already checking
	_http_version = HTTPRequest.new()
	_http_version.timeout = 10.0
	add_child(_http_version)
	_http_version.request_completed.connect(_on_version_received)
	_set_bar("🔍 Buscando actualizaciones...", "", Color(0.5, 0.8, 1.0))
	if _http_version.request(VERSION_URL) != OK:
		_http_version.queue_free()
		_http_version = null
		_bar.visible = false

func _on_version_received(result: int, code: int, _headers: PackedStringArray, body: PackedByteArray) -> void:
	if _http_version and is_instance_valid(_http_version):
		_http_version.queue_free()
		_http_version = null
	if result != HTTPRequest.RESULT_SUCCESS or code != 200:
		_bar.visible = false
		return
	_remote_version = body.get_string_from_utf8().strip_edges()
	var local := _get_local_version()
	if _remote_version == local:
		_set_bar("✅ GodotLuau %s — al día." % local, "", Color(0.4, 0.9, 0.5))
		# Hide after 4 seconds
		await get_tree().create_timer(4.0).timeout
		if _bar and is_instance_valid(_bar):
			_bar.visible = false
		return
	_set_bar("Nueva versión disponible: %s  (instalada: %s)" % [_remote_version, local],
		"⬇ Actualizar", Color(1.0, 0.9, 0.3))

func _get_local_version() -> String:
	if not FileAccess.file_exists(VERSION_FILE):
		return "desconocida"
	var f := FileAccess.open(VERSION_FILE, FileAccess.READ)
	return f.get_as_text().strip_edges() if f else "desconocida"

# ── Download ──────────────────────────────────────────────────────────────────

func _on_action_pressed() -> void:
	match _bar_btn.text:
		"🔄 Reiniciar editor":     _do_editor_restart(); return
		"Apply & Close":           _apply_windows_update_legacy(); return
	if not _downloading:
		_start_download()

func _start_download() -> void:
	_downloading = true
	_set_bar("Descargando GodotLuau %s..." % _remote_version, "", Color(0.4, 0.8, 1.0))
	_bar_btn.visible = false
	_http_download = HTTPRequest.new()
	_http_download.use_threads = true
	_http_download.download_file = OS.get_user_data_dir() + "/godotluau_update.zip"
	add_child(_http_download)
	_http_download.request_completed.connect(_on_download_completed)
	if _http_download.request(ZIP_URL) != OK:
		_set_bar("❌ No se pudo iniciar la descarga.", "Reintentar", Color(1.0, 0.4, 0.4))
		_downloading = false
		_http_download.queue_free()
		_http_download = null

func _on_download_completed(result: int, code: int, _headers: PackedStringArray, _body: PackedByteArray) -> void:
	if _http_download and is_instance_valid(_http_download):
		_http_download.queue_free()
		_http_download = null
	_downloading = false
	if result != HTTPRequest.RESULT_SUCCESS or (code != 200 and code != 0):
		_set_bar("❌ Descarga fallida (código %d)." % code, "Reintentar", Color(1.0, 0.4, 0.4))
		return
	_set_bar("📦 Extrayendo archivos...", "", Color(0.4, 0.8, 1.0))
	_bar_btn.visible = false
	_apply_update.call_deferred()

# ── Apply update — smart strategy ─────────────────────────────────────────────

var _windows_staged_dlls : Array[Dictionary] = []

func _apply_update() -> void:
	var zip_path  := OS.get_user_data_dir() + "/godotluau_update.zip"
	var proj_path := ProjectSettings.globalize_path("res://")
	var zip := ZIPReader.new()
	if zip.open(zip_path) != OK:
		_set_bar("❌ No se pudo abrir el ZIP descargado.", "Reintentar", Color(1.0, 0.4, 0.4))
		return

	var staged_dlls : Array[Dictionary] = []
	var staging_dir := OS.get_user_data_dir() + "/godotluau_staging/"
	DirAccess.make_dir_recursive_absolute(staging_dir)
	var is_windows : bool = (OS.get_name() == "Windows")

	for rel_path in zip.get_files():
		if rel_path.ends_with("/"):
			continue
		var data     : PackedByteArray = zip.read_file(rel_path)
		var abs_dest : String          = proj_path + rel_path
		var dest_dir : String          = abs_dest.get_base_dir()
		if not DirAccess.dir_exists_absolute(dest_dir):
			DirAccess.make_dir_recursive_absolute(dest_dir)
		var ext := rel_path.get_extension().to_lower()
		if is_windows and ext in DLL_EXTENSIONS:
			# Stage DLL — don't write directly while Godot has it loaded
			var stage_path := staging_dir + rel_path.get_file()
			var sf := FileAccess.open(stage_path, FileAccess.WRITE)
			if sf:
				sf.store_buffer(data)
				staged_dlls.append({"src": stage_path, "dst": abs_dest})
		else:
			# Non-DLL files: write immediately, no restart needed
			var f := FileAccess.open(abs_dest, FileAccess.WRITE)
			if f:
				f.store_buffer(data)

	zip.close()

	# Update version file
	var vf := FileAccess.open(VERSION_FILE, FileAccess.WRITE)
	if vf:
		vf.store_string(_remote_version + "\n")

	# Clean up ZIP
	DirAccess.remove_absolute(zip_path)

	if staged_dlls.is_empty():
		# Only non-DLL files — applied instantly, no restart needed
		_set_bar("✅ GodotLuau %s instalado sin reiniciar." % _remote_version,
			"", Color(0.3, 0.9, 0.5))
		_refresh_stats()
		return

	# DLLs need special handling
	_windows_staged_dlls = staged_dlls

	# Try the "rename trick": rename old DLL while loaded, place new one
	# Windows allows renaming a loaded DLL — Godot keeps running with old copy in RAM
	# On restart it loads the new file automatically
	if is_windows and _try_rename_and_replace():
		_set_bar("✅ DLL actualizado. Haz clic para reiniciar el editor (Godot se mantiene abierto).",
			"🔄 Reiniciar editor", Color(0.4, 0.9, 1.0))
	else:
		# Fallback: close Godot, PowerShell copies DLL, Godot relaunches
		_set_bar("✅ Listo. Godot se cerrará, aplicará la DLL y se volverá a abrir automáticamente.",
			"Apply & Close", Color(1.0, 0.7, 0.2))

# ── Rename trick: rename locked DLL → .old, then copy new one ─────────────────

func _try_rename_and_replace() -> bool:
	for item in _windows_staged_dlls:
		var dst : String = item["dst"]
		var src : String = item["src"]
		if not FileAccess.file_exists(dst):
			# DLL doesn't exist yet — just copy directly
			var data := FileAccess.get_file_as_bytes(src)
			if data.is_empty():
				return false
			var f := FileAccess.open(dst, FileAccess.WRITE)
			if not f:
				return false
			f.store_buffer(data)
			continue

		# Try to rename the currently-loaded DLL
		# On Windows this succeeds even when the DLL is in use (file handle stays valid)
		var old_path := dst + ".old"
		var err := DirAccess.rename_absolute(dst, old_path)
		if err != OK:
			# Rename failed — DLL is truly locked (shouldn't happen on modern Windows)
			return false

		# Copy new DLL to original path
		var data := FileAccess.get_file_as_bytes(src)
		if data.is_empty():
			# Restore old DLL
			DirAccess.rename_absolute(old_path, dst)
			return false
		var f := FileAccess.open(dst, FileAccess.WRITE)
		if not f:
			DirAccess.rename_absolute(old_path, dst)
			return false
		f.store_buffer(data)

	return true

# ── Restart editor (keeps Godot, just relaunches with same project) ────────────

func _do_editor_restart() -> void:
	_set_bar("🔄 Reiniciando editor...", "", Color(0.4, 0.8, 1.0))
	# EditorInterface.restart_editor() closes and reopens Godot with the same project
	# No need for external PowerShell — Godot handles its own restart
	await get_tree().create_timer(0.5).timeout
	get_editor_interface().restart_editor()

# ── Legacy fallback: PowerShell close-reopen (only if rename trick fails) ──────

func _apply_windows_update_legacy() -> void:
	var ps_path   := OS.get_user_data_dir() + "/_godotluau_apply.ps1"
	var godot_exe := OS.get_executable_path().replace("/", "\\")
	var proj_dir  := ProjectSettings.globalize_path("res://").rstrip("/\\").replace("/", "\\")
	var my_pid    := str(OS.get_process_id())

	var ps := PackedStringArray()
	ps.append("# GodotLuau auto-updater — generated by plugin")
	ps.append("$ErrorActionPreference = 'Continue'")
	ps.append("$godotPid = %s" % my_pid)
	# Wait for Godot to fully close
	ps.append("$maxWait = 30")
	ps.append("$waited = 0")
	ps.append("while ($waited -lt $maxWait) {")
	ps.append("    if (-not (Get-Process -Id $godotPid -ErrorAction SilentlyContinue)) { break }")
	ps.append("    Start-Sleep -Milliseconds 400")
	ps.append("    $waited++")
	ps.append("}")
	ps.append("Start-Sleep -Milliseconds 600")
	# Copy staged DLLs
	for item in _windows_staged_dlls:
		var src : String = item["src"].replace("/", "\\")
		var dst : String = item["dst"].replace("/", "\\")
		ps.append("try { Copy-Item -Force -Path \"%s\" -Destination \"%s\" } catch { Write-Host $_.Exception.Message }" % [src, dst])
	# Relaunch Godot — use Start-Process with proper quoting
	ps.append("Start-Sleep -Milliseconds 300")
	ps.append("& \"%s\" --path \"%s\"" % [godot_exe, proj_dir])

	var pf := FileAccess.open(ps_path, FileAccess.WRITE)
	if not pf:
		_set_bar("❌ No se pudo escribir el script de actualización.", "", Color(1.0, 0.4, 0.4))
		return
	pf.store_string("\r\n".join(ps))
	pf.close()

	var ps_abs := ps_path.replace("/", "\\")
	OS.create_process("powershell.exe", [
		"-WindowStyle", "Hidden",
		"-NonInteractive",
		"-ExecutionPolicy", "Bypass",
		"-File", ps_abs
	])

	# Give PowerShell a moment to start before we close
	await get_tree().create_timer(0.3).timeout
	get_tree().quit()

func _relaunch_godot() -> void:
	OS.create_process(OS.get_executable_path(),
		["--path", ProjectSettings.globalize_path("res://").rstrip("/")])
	get_tree().quit()

func _set_bar(text: String, btn_text: String, color: Color) -> void:
	if not (_bar and is_instance_valid(_bar)):
		return
	_bar_label.text = text
	_bar_label.add_theme_color_override("font_color", color)
	_bar_btn.visible = not btn_text.is_empty()
	if not btn_text.is_empty():
		_bar_btn.text = btn_text
	_bar.visible = true
	make_bottom_panel_item_visible(_bar)
