@tool
extends EditorPlugin

const VERSION_URL  := "https://raw.githubusercontent.com/Pimpoli/GodotLuau/main/Version"
const ZIP_URL      := "https://github.com/Pimpoli/GodotLuau/raw/main/GodotLuau.zip"
const VERSION_FILE := "res://addons/GodotLuauUpdater/installed_version.txt"
const DLL_EXTENSIONS := ["dll", "so", "dylib", "framework"]

var _http_version   : HTTPRequest    = null
var _http_download  : HTTPRequest    = null
var _bar            : PanelContainer = null
var _bar_label      : Label          = null
var _bar_btn        : Button         = null
var _remote_version : String         = ""
var _downloading    : bool           = false
var _settings_panel : Control        = null

# ── Lifecycle ────────────────────────────────────────────────────────────────

func _enter_tree() -> void:
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

# ── ProjectSettings registration (readable from C++ extension) ───────────────

func _register_settings() -> void:
	_add_bool_setting("godot_luau/ai_autocomplete_enabled", false,
		"Enable AI Smart Autocomplete — suggests values based on variable names. OFF by default while data is being collected.")
	_add_bool_setting("godot_luau/share_data_enabled", true,
		"Share anonymous usage data to improve the AI model. Disable if you prefer not to share.")

func _add_bool_setting(key: String, default_value: bool, hint: String) -> void:
	if not ProjectSettings.has_setting(key):
		ProjectSettings.set_setting(key, default_value)
	ProjectSettings.set_initial_value(key, default_value)
	var info := {
		"name": key,
		"type": TYPE_BOOL,
		"hint": PROPERTY_HINT_NONE,
		"hint_string": hint
	}
	ProjectSettings.add_property_info(info)

# ── Settings panel (bottom panel tab "GodotLuau AI") ─────────────────────────

func _build_settings_panel() -> void:
	var root := PanelContainer.new()
	root.size_flags_horizontal = Control.SIZE_EXPAND_FILL

	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left",  12)
	margin.add_theme_constant_override("margin_right", 12)
	margin.add_theme_constant_override("margin_top",   10)
	margin.add_theme_constant_override("margin_bottom",10)
	root.add_child(margin)

	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 10)
	margin.add_child(vbox)

	# Header
	var header := Label.new()
	header.text = "GodotLuau — AI & Autocomplete Settings"
	header.add_theme_color_override("font_color", Color(0.4, 0.8, 1.0))
	header.add_theme_font_size_override("font_size", 15)
	vbox.add_child(header)

	var sep := HSeparator.new()
	vbox.add_child(sep)

	# ── AI Autocomplete toggle ──
	vbox.add_child(_make_setting_row(
		"AI Smart Autocomplete",
		"When enabled, the AI suggests values based on your variable names (e.g. typing 'speed =' hints '16').\nCurrently OFF by default — the system is still collecting data to improve suggestions.",
		"godot_luau/ai_autocomplete_enabled",
		false
	))

	vbox.add_child(HSeparator.new())

	# ── Data sharing toggle ──
	vbox.add_child(_make_setting_row(
		"Share anonymous usage data",
		"Helps improve future AI suggestions by sending anonymous usage patterns.\nNo code content is ever shared — only which APIs you use most often.\nYou can opt out at any time.",
		"godot_luau/share_data_enabled",
		true
	))

	vbox.add_child(HSeparator.new())

	# ── Info note ──
	var note := RichTextLabel.new()
	note.bbcode_enabled = true
	note.fit_content = true
	note.scroll_active = false
	note.text = "[color=#888888]Collected data is stored locally at [b]user://godotluau_usage.json[/b].\nTo contribute data to improve the AI, see: [color=#4d9de0]github.com/Pimpoli/IALuauAutoCompleted[/color][/color]"
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

# ── Update notification bar ───────────────────────────────────────────────────

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
	_http_version = HTTPRequest.new()
	_http_version.timeout = 10.0
	add_child(_http_version)
	_http_version.request_completed.connect(_on_version_received)
	if _http_version.request(VERSION_URL) != OK:
		_http_version.queue_free()
		_http_version = null

func _on_version_received(result: int, code: int, _headers: PackedStringArray, body: PackedByteArray) -> void:
	if _http_version and is_instance_valid(_http_version):
		_http_version.queue_free()
		_http_version = null
	if result != HTTPRequest.RESULT_SUCCESS or code != 200:
		return
	_remote_version = body.get_string_from_utf8().strip_edges()
	var local := _get_local_version()
	if _remote_version == local:
		return
	_set_bar("New version available: %s  (installed: %s)" % [_remote_version, local],
		"⬇ Update", Color(1.0, 0.9, 0.3))

func _get_local_version() -> String:
	if not FileAccess.file_exists(VERSION_FILE):
		return "unknown"
	var f := FileAccess.open(VERSION_FILE, FileAccess.READ)
	return f.get_as_text().strip_edges() if f else "unknown"

# ── Download ──────────────────────────────────────────────────────────────────

func _on_action_pressed() -> void:
	if _bar_btn.text == "Restart Godot":
		_relaunch_godot(); return
	if _bar_btn.text == "Apply & Close":
		_apply_windows_update(); return
	if not _downloading:
		_start_download()

func _start_download() -> void:
	_downloading = true
	_set_bar("Downloading GodotLuau %s..." % _remote_version, "", Color(0.4, 0.8, 1.0))
	_bar_btn.visible = false
	_http_download = HTTPRequest.new()
	_http_download.use_threads = true
	_http_download.download_file = OS.get_user_data_dir() + "/godotluau_update.zip"
	add_child(_http_download)
	_http_download.request_completed.connect(_on_download_completed)
	if _http_download.request(ZIP_URL) != OK:
		_set_bar("❌ Failed to start download.", "Retry", Color(1.0, 0.4, 0.4))
		_downloading = false
		_http_download.queue_free()
		_http_download = null

func _on_download_completed(result: int, code: int, _headers: PackedStringArray, _body: PackedByteArray) -> void:
	if _http_download and is_instance_valid(_http_download):
		_http_download.queue_free()
		_http_download = null
	_downloading = false
	if result != HTTPRequest.RESULT_SUCCESS or (code != 200 and code != 0):
		_set_bar("❌ Download failed (code %d)." % code, "Retry", Color(1.0, 0.4, 0.4))
		return
	_set_bar("📦 Extracting files...", "", Color(0.4, 0.8, 1.0))
	_bar_btn.visible = false
	_apply_update.call_deferred()

# ── Apply update ──────────────────────────────────────────────────────────────

func _apply_update() -> void:
	var zip_path  := OS.get_user_data_dir() + "/godotluau_update.zip"
	var proj_path := ProjectSettings.globalize_path("res://")
	var zip := ZIPReader.new()
	if zip.open(zip_path) != OK:
		_set_bar("❌ Could not open downloaded ZIP.", "Retry", Color(1.0, 0.4, 0.4))
		return
	var staged_dlls : Array[Dictionary] = []
	var staging_dir := OS.get_user_data_dir() + "/godotluau_staging/"
	DirAccess.make_dir_recursive_absolute(staging_dir)
	var is_windows : bool = OS.get_name() == "Windows"
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
			var stage_path := staging_dir + rel_path.get_file()
			var sf := FileAccess.open(stage_path, FileAccess.WRITE)
			if sf:
				sf.store_buffer(data)
				staged_dlls.append({"src": stage_path, "dst": abs_dest})
		else:
			var f := FileAccess.open(abs_dest, FileAccess.WRITE)
			if f:
				f.store_buffer(data)
	zip.close()
	var vf := FileAccess.open(VERSION_FILE, FileAccess.WRITE)
	if vf:
		vf.store_string(_remote_version + "\n")
	if staged_dlls.is_empty():
		_set_bar("✅ GodotLuau %s installed. Restart editor." % _remote_version,
			"Restart Godot", Color(0.3, 0.9, 0.5))
	else:
		_windows_staged_dlls = staged_dlls
		_set_bar("✅ Ready. Godot will close, apply the DLL, and reopen automatically.",
			"Apply & Close", Color(1.0, 0.7, 0.2))

# ── Windows DLL staging ───────────────────────────────────────────────────────

var _windows_staged_dlls : Array[Dictionary] = []

func _apply_windows_update() -> void:
	var ps_path   := OS.get_user_data_dir() + "/_godotluau_apply.ps1"
	var godot_exe := OS.get_executable_path().replace("/", "\\")
	var proj_path := ProjectSettings.globalize_path("res://").replace("/", "\\")
	var my_pid    := str(OS.get_process_id())
	var ps := PackedStringArray()
	ps.append("$pid_godot = %s" % my_pid)
	ps.append("while (Get-Process -Id $pid_godot -ErrorAction SilentlyContinue) { Start-Sleep -Milliseconds 500 }")
	for item in _windows_staged_dlls:
		var src : String = item["src"].replace("/", "\\")
		var dst : String = item["dst"].replace("/", "\\")
		ps.append("Copy-Item -Force '%s' '%s'" % [src, dst])
	var zip_ps := (OS.get_user_data_dir() + "/godotluau_update.zip").replace("/", "\\")
	ps.append("Remove-Item -Force '%s' -ErrorAction SilentlyContinue" % zip_ps)
	ps.append("Start-Process '%s' -ArgumentList '--path','%s'" % [godot_exe, proj_path])
	var pf := FileAccess.open(ps_path, FileAccess.WRITE)
	if not pf:
		_set_bar("❌ Could not write update script.", "", Color(1.0, 0.4, 0.4))
		return
	pf.store_string("\r\n".join(ps))
	pf.close()
	OS.create_process("powershell.exe", [
		"-WindowStyle", "Hidden", "-NonInteractive",
		"-ExecutionPolicy", "Bypass",
		"-File", ps_path.replace("/", "\\")
	])
	get_tree().quit()

func _relaunch_godot() -> void:
	OS.create_process(OS.get_executable_path(), ["--path", ProjectSettings.globalize_path("res://")])
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
