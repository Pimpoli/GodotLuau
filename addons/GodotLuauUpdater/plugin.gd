@tool
extends EditorPlugin

# ═══════════════════════════════════════════════════════════════════════════
#  GodotLuau Updater — Plugin de actualización automática
#  Comprueba el repositorio de GitHub, descarga el ZIP y aplica la update.
#
#  Cómo usar (para el desarrollador):
#    1. Sube el nuevo ZIP como "GodotLuau.zip" en el repo de GitHub
#    2. Actualiza el archivo "Version" en el repo con la nueva versión (ej: v1.0.9)
#    El updater detectará el cambio automáticamente la próxima vez que el usuario
#    abra Godot y ofrecerá instalar la actualización.
# ═══════════════════════════════════════════════════════════════════════════

const VERSION_URL  := "https://raw.githubusercontent.com/Pimpoli/GodotLuau/main/Version"
const ZIP_URL      := "https://github.com/Pimpoli/GodotLuau/raw/main/GodotLuau.zip"
const VERSION_FILE := "res://addons/GodotLuauUpdater/installed_version.txt"

# Archivos de la extensión que se actualizan (rutas relativas al proyecto)
# Los DLLs necesitan tratamiento especial en Windows (no se pueden reemplazar en caliente)
const DLL_EXTENSIONS := ["dll", "so", "dylib", "framework"]

var _http_version   : HTTPRequest  = null
var _http_download  : HTTPRequest  = null
var _bar            : PanelContainer = null
var _bar_label      : Label        = null
var _bar_btn        : Button       = null
var _remote_version : String       = ""
var _downloading    : bool         = false

# ─── Ciclo de vida ─────────────────────────────────────────────────────────

func _enter_tree() -> void:
	_build_notification_bar()
	_check_for_update()

func _exit_tree() -> void:
	if _http_version  and is_instance_valid(_http_version):  _http_version.queue_free()
	if _http_download and is_instance_valid(_http_download): _http_download.queue_free()
	if _bar           and is_instance_valid(_bar):
		remove_control_from_bottom_panel(_bar)
		_bar.queue_free()

# ─── UI de notificación (barra inferior del editor) ────────────────────────

func _build_notification_bar() -> void:
	_bar = PanelContainer.new()
	_bar.visible = false

	var hbox := HBoxContainer.new()
	hbox.add_theme_constant_override("separation", 8)
	_bar.add_child(hbox)

	var icon_label := Label.new()
	icon_label.text = "GodotLuau"
	icon_label.add_theme_color_override("font_color", Color(0.4, 0.8, 1.0))
	hbox.add_child(icon_label)

	_bar_label = Label.new()
	_bar_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hbox.add_child(_bar_label)

	_bar_btn = Button.new()
	_bar_btn.flat = true
	_bar_btn.pressed.connect(_on_action_button_pressed)
	hbox.add_child(_bar_btn)

	var btn_cerrar := Button.new()
	btn_cerrar.flat = true
	btn_cerrar.text = "✕"
	btn_cerrar.tooltip_text = "Cerrar notificación"
	btn_cerrar.pressed.connect(func(): _bar.visible = false)
	hbox.add_child(btn_cerrar)

	add_control_to_bottom_panel(_bar, "GodotLuau")

# ─── Verificación de versión ────────────────────────────────────────────────

func _check_for_update() -> void:
	_http_version = HTTPRequest.new()
	_http_version.timeout = 10.0
	add_child(_http_version)
	_http_version.request_completed.connect(_on_version_received)
	var err := _http_version.request(VERSION_URL)
	if err != OK:
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
		return  # Ya está actualizado

	# Hay actualización disponible
	_set_bar_state(
		"Nueva versión disponible: %s  (instalada: %s)" % [_remote_version, local],
		"⬇ Actualizar",
		Color(1.0, 0.9, 0.3)
	)

func _get_local_version() -> String:
	if not FileAccess.file_exists(VERSION_FILE):
		return "desconocida"
	var f := FileAccess.open(VERSION_FILE, FileAccess.READ)
	if not f:
		return "desconocida"
	return f.get_as_text().strip_edges()

# ─── Descarga del ZIP ───────────────────────────────────────────────────────

func _on_action_button_pressed() -> void:
	if _bar_btn.text == "Reiniciar Godot":
		_relaunch_godot()
		return
	if _bar_btn.text == "Aplicar y Cerrar":
		_apply_windows_update()
		return
	if not _downloading:
		_start_download()

func _start_download() -> void:
	_downloading = true
	_set_bar_state("Descargando GodotLuau %s..." % _remote_version, "", Color(0.4, 0.8, 1.0))
	_bar_btn.visible = false

	_http_download = HTTPRequest.new()
	_http_download.use_threads = true
	_http_download.download_file = OS.get_user_data_dir() + "/godotluau_update.zip"
	add_child(_http_download)
	_http_download.request_completed.connect(_on_download_completed)

	var err := _http_download.request(ZIP_URL)
	if err != OK:
		_set_bar_state("❌ Error al iniciar la descarga.", "Reintentar", Color(1.0, 0.4, 0.4))
		_downloading = false
		_http_download.queue_free()
		_http_download = null

func _on_download_completed(result: int, code: int, _headers: PackedStringArray, _body: PackedByteArray) -> void:
	if _http_download and is_instance_valid(_http_download):
		_http_download.queue_free()
		_http_download = null
	_downloading = false

	if result != HTTPRequest.RESULT_SUCCESS or (code != 200 and code != 0):
		_set_bar_state("❌ Error al descargar (código %d)." % code, "Reintentar", Color(1.0, 0.4, 0.4))
		return

	_set_bar_state("📦 Extrayendo archivos...", "", Color(0.4, 0.8, 1.0))
	_bar_btn.visible = false
	_apply_update.call_deferred()

# ─── Aplicar actualización ──────────────────────────────────────────────────

func _apply_update() -> void:
	var zip_path  := OS.get_user_data_dir() + "/godotluau_update.zip"
	var proj_path := ProjectSettings.globalize_path("res://")

	var zip := ZIPReader.new()
	if zip.open(zip_path) != OK:
		_set_bar_state("❌ No se pudo abrir el ZIP descargado.", "Reintentar", Color(1.0, 0.4, 0.4))
		return

	var staged_dlls : Array[Dictionary] = []   # {src, dst} — solo Windows
	var staging_dir := OS.get_user_data_dir() + "/godotluau_staging/"
	DirAccess.make_dir_recursive_absolute(staging_dir)

	var is_windows : bool = OS.get_name() == "Windows"

	for rel_path in zip.get_files():
		# Ignorar entradas de directorio
		if rel_path.ends_with("/"):
			continue

		var data     : PackedByteArray = zip.read_file(rel_path)
		var abs_dest : String          = proj_path + rel_path
		var dest_dir : String          = abs_dest.get_base_dir()

		if not DirAccess.dir_exists_absolute(dest_dir):
			DirAccess.make_dir_recursive_absolute(dest_dir)

		var ext := rel_path.get_extension().to_lower()
		if is_windows and ext in DLL_EXTENSIONS:
			# Windows no puede sobreescribir DLLs en uso → staging
			var stage_path := staging_dir + rel_path.get_file()
			var sf := FileAccess.open(stage_path, FileAccess.WRITE)
			if sf:
				sf.store_buffer(data)
				staged_dlls.append({"src": stage_path, "dst": abs_dest})
		else:
			# Todos los demás archivos: extraer directamente
			var f := FileAccess.open(abs_dest, FileAccess.WRITE)
			if f:
				f.store_buffer(data)

	zip.close()

	# Guardar nueva versión instalada
	var vf := FileAccess.open(VERSION_FILE, FileAccess.WRITE)
	if vf:
		vf.store_string(_remote_version + "\n")

	if staged_dlls.is_empty():
		# Linux / macOS: DLLs ya reemplazadas, solo reiniciar
		_set_bar_state(
			"✅ GodotLuau %s instalado. Reinicia el editor." % _remote_version,
			"Reiniciar Godot",
			Color(0.3, 0.9, 0.5)
		)
	else:
		# Windows: hay DLLs pendientes
		_windows_staged_dlls = staged_dlls
		_set_bar_state(
			"✅ Listo. Godot se cerrará para reemplazar la DLL y se reabrirá solo.",
			"Aplicar y Cerrar",
			Color(1.0, 0.7, 0.2)
		)

# ─── Windows: batch file que reemplaza DLLs tras cerrar Godot ──────────────

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
		_set_bar_state("❌ No se pudo escribir el archivo de actualización.", "", Color(1.0, 0.4, 0.4))
		return
	pf.store_string("\r\n".join(ps))
	pf.close()

	# PowerShell oculto — sin ventana visible
	OS.create_process("powershell.exe", [
		"-WindowStyle", "Hidden",
		"-NonInteractive",
		"-ExecutionPolicy", "Bypass",
		"-File", ps_path.replace("/", "\\")
	])
	get_tree().quit()

# ─── Reinicio del editor (Linux / macOS) ────────────────────────────────────

func _relaunch_godot() -> void:
	var proj_path := ProjectSettings.globalize_path("res://")
	OS.create_process(OS.get_executable_path(), ["--path", proj_path])
	get_tree().quit()

# ─── Helper ─────────────────────────────────────────────────────────────────

func _set_bar_state(text: String, btn_text: String, color: Color) -> void:
	if not (_bar and is_instance_valid(_bar)):
		return
	_bar_label.text = text
	_bar_label.add_theme_color_override("font_color", color)
	if btn_text.is_empty():
		_bar_btn.visible = false
	else:
		_bar_btn.text = btn_text
		_bar_btn.visible = true
	_bar.visible = true
	make_bottom_panel_item_visible(_bar)
