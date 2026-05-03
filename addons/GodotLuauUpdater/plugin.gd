@tool
extends EditorPlugin

const VERSION_URL    := "https://raw.githubusercontent.com/Pimpoli/GodotLuau/main/Version"
const ZIP_URL        := "https://github.com/Pimpoli/GodotLuau/raw/main/GodotLuau.zip"
const VERSION_FILE   := "res://Version"
const GITHUB_URL     := "https://github.com/Pimpoli/GodotLuau"
const DATA_FILE      := "user://godotluau_usage.json"
const CUSTOM_AC_FILE := "user://godotluau_custom_autocomplete.json"
const DLL_EXTENSIONS := ["dll", "so", "dylib", "framework"]
const WM_META_KEY    := "_godotluau_wm"

# ── Translation dictionary — EN / ES / PT-BR ─────────────────────────────────
const TR := {
	"en": {
		"panel_title":           "GodotLuau Config",
		"sec_autocomplete":      "🤖  Autocomplete",
		"sec_data":              "📊  Usage Data",
		"sec_debug":             "🔧  Debug",
		"sec_updates":           "🔄  Updates",
		"font_small":            "S",
		"font_normal":           "M",
		"font_large":            "L",
		"ai_title":              "AI Smart Autocomplete",
		"ai_desc":               "Suggests values based on variable names (e.g. 'speed =' → '16').\nDisabled by default — data is still collected to improve suggestions.",
		"share_title":           "Share anonymous usage data",
		"share_desc":            "Helps improve autocomplete suggestions.\nTracks which APIs you use most (e.g. FindFirstChild vs WaitForChild).\nCode content is never shared.",
		"debug_title":           "Debug Mode",
		"debug_desc":            "Shows GodotLuau runtime messages in Output.\nWhen OFF, Luau script print() and warn() calls are suppressed to keep your Output clean.",
		"notif_outdated_title":  "Notify outdated version in Output",
		"notif_outdated_desc":   "Prints a warning in the Output panel when running an outdated GodotLuau version.",
		"speed_title":           "Instant Autocomplete",
		"speed_desc":            "Triggers suggestions on every keystroke without waiting.\nSets the editor completion delay to near-zero.",
		"cac_header":            "Custom Autocomplete",
		"cac_toggle_title":      "Use custom autocomplete",
		"cac_toggle_desc":       "Replace or extend built-in suggestions with your own JSON API list.\nLoad from a local file or a GitHub raw URL.",
		"cac_url_label":         "Source URL (GitHub raw or direct link):",
		"cac_url_hint":          "https://raw.githubusercontent.com/.../autocomplete.json",
		"cac_btn_download":      "⬇ Download",
		"cac_btn_import":        "📂 Import JSON",
		"cac_btn_clear":         "🗑 Clear",
		"cac_status_none":       "No custom autocomplete loaded.",
		"cac_status_ok":         "✅ %d custom items loaded.",
		"cac_status_dl":         "⏳ Downloading...",
		"cac_status_err":        "❌ Download failed — check the URL.",
		"cac_status_bad_json":   "❌ Invalid JSON format (expected an array).",
		"stats_header":          "Collected data",
		"stats_none":            "No data collected yet.",
		"stats_size":            "Size: %.1f KB  |  Entries: ~%d",
		"stats_err":             "Could not read data file.",
		"btn_refresh":           "↺ Refresh",
		"btn_open":              "📂 Open folder",
		"btn_clear":             "🗑 Delete data",
		"dlg_clear_title":       "Delete usage data",
		"dlg_clear_text":        "Delete all collected usage data?\nThis does not affect plugin functionality.",
		"bar_checking":          "🔍 Checking...",
		"bar_uptodate":          "✅ %s — up to date.",
		"bar_newver":            "%s  →  %s",
		"bar_btn_update":        "⬇ Update",
		"bar_downloading":       "⬇ Downloading %s...",
		"bar_dl_failed":         "❌ Download failed (code %d).",
		"bar_extracting":        "📦 Extracting...",
		"bar_ok_no_restart":     "✅ %s installed.",
		"bar_ok_restart":        "✅ Updated — click to restart.",
		"bar_apply_close":       "✅ Ready. Godot will close and apply the DLL.",
		"bar_restart":           "🔄 Restart editor",
		"bar_restarting":        "🔄 Restarting...",
		"bar_data_cleared":      "🗑 Data deleted.",
		"bar_dl_err":            "❌ Could not start download.",
		"bar_retry":             "Retry",
		"bar_script_err":        "❌ Could not write update script.",
		"ver_unknown":           "unknown",
		"btn_check_ver":         "🔍",
		"btn_reinstall":         "↺ Reinstall",
		"bar_reinstalling":      "↺ Reinstalling %s...",
		"github_label":          "Official source:",
		"footer_data":           "Data file: user://godotluau_usage.json",
		"warn_watermark":        "⚠ GodotLuau watermark missing — this may be a modified version. Please reinstall from: " + GITHUB_URL,
	},
	"es": {
		"panel_title":           "GodotLuau Config",
		"sec_autocomplete":      "🤖  Autocompletado",
		"sec_data":              "📊  Datos de Uso",
		"sec_debug":             "🔧  Debug",
		"sec_updates":           "🔄  Actualizaciones",
		"font_small":            "S",
		"font_normal":           "M",
		"font_large":            "L",
		"ai_title":              "Autocompletado Inteligente IA",
		"ai_desc":               "Sugiere valores según nombres de variable (ej: 'speed =' → '16').\nDesactivado por defecto — el sistema sigue recolectando datos para mejorar.",
		"share_title":           "Compartir datos de uso anónimos",
		"share_desc":            "Ayuda a mejorar las sugerencias de autocompletado.\nRegistra qué APIs usas más (ej: FindFirstChild vs WaitForChild).\nNunca se comparte el contenido del código.",
		"debug_title":           "Modo Debug",
		"debug_desc":            "Muestra mensajes del runtime GodotLuau en el Output.\nCuando está DESACTIVADO, suprime los print() y warn() de scripts Luau para mantener el Output limpio.",
		"notif_outdated_title":  "Notificar versión desactualizada en Output",
		"notif_outdated_desc":   "Imprime una advertencia en el panel Output cuando se ejecuta una versión desactualizada de GodotLuau.",
		"speed_title":           "Autocompletado Instantáneo",
		"speed_desc":            "Activa sugerencias con cada tecla, sin esperar.\nEstablece el retraso del editor a casi cero.",
		"cac_header":            "Autocompletado Personalizado",
		"cac_toggle_title":      "Usar autocompletado personalizado",
		"cac_toggle_desc":       "Reemplaza o extiende las sugerencias incorporadas con tu propia lista JSON.\nCarga desde un archivo local o una URL raw de GitHub.",
		"cac_url_label":         "URL de origen (GitHub raw o enlace directo):",
		"cac_url_hint":          "https://raw.githubusercontent.com/.../autocomplete.json",
		"cac_btn_download":      "⬇ Descargar",
		"cac_btn_import":        "📂 Importar JSON",
		"cac_btn_clear":         "🗑 Limpiar",
		"cac_status_none":       "Sin autocompletado personalizado cargado.",
		"cac_status_ok":         "✅ %d entradas personalizadas cargadas.",
		"cac_status_dl":         "⏳ Descargando...",
		"cac_status_err":        "❌ Descarga fallida — verifica la URL.",
		"cac_status_bad_json":   "❌ Formato JSON inválido (se esperaba un array).",
		"stats_header":          "Datos recolectados",
		"stats_none":            "Sin datos recolectados todavía.",
		"stats_size":            "Tamaño: %.1f KB  |  Entradas: ~%d",
		"stats_err":             "No se pudo leer el archivo de datos.",
		"btn_refresh":           "↺ Actualizar",
		"btn_open":              "📂 Abrir carpeta",
		"btn_clear":             "🗑 Borrar datos",
		"dlg_clear_title":       "Borrar datos de uso",
		"dlg_clear_text":        "¿Eliminar todos los datos de uso recolectados?\nEsto no afecta el funcionamiento del plugin.",
		"bar_checking":          "🔍 Verificando...",
		"bar_uptodate":          "✅ %s — al día.",
		"bar_newver":            "%s  →  %s",
		"bar_btn_update":        "⬇ Actualizar",
		"bar_downloading":       "⬇ Descargando %s...",
		"bar_dl_failed":         "❌ Descarga fallida (código %d).",
		"bar_extracting":        "📦 Extrayendo...",
		"bar_ok_no_restart":     "✅ %s instalado.",
		"bar_ok_restart":        "✅ Actualizado — clic para reiniciar.",
		"bar_apply_close":       "✅ Listo. Godot se cerrará y aplicará la DLL.",
		"bar_restart":           "🔄 Reiniciar editor",
		"bar_restarting":        "🔄 Reiniciando...",
		"bar_data_cleared":      "🗑 Datos eliminados.",
		"bar_dl_err":            "❌ No se pudo iniciar la descarga.",
		"bar_retry":             "Reintentar",
		"bar_script_err":        "❌ No se pudo escribir el script de actualización.",
		"ver_unknown":           "desconocida",
		"btn_check_ver":         "🔍",
		"btn_reinstall":         "↺ Reinstalar",
		"bar_reinstalling":      "↺ Reinstalando %s...",
		"github_label":          "Fuente oficial:",
		"footer_data":           "Archivo de datos: user://godotluau_usage.json",
		"warn_watermark":        "⚠ Marca de agua de GodotLuau no encontrada — puede ser una versión modificada. Reinstala desde: " + GITHUB_URL,
	},
	"pt": {
		"panel_title":           "GodotLuau Config",
		"sec_autocomplete":      "🤖  Autocompletar",
		"sec_data":              "📊  Dados de Uso",
		"sec_debug":             "🔧  Debug",
		"sec_updates":           "🔄  Atualizações",
		"font_small":            "S",
		"font_normal":           "M",
		"font_large":            "L",
		"ai_title":              "Autocompletar Inteligente IA",
		"ai_desc":               "Sugere valores baseados em nomes de variáveis (ex: 'speed =' → '16').\nDesativado por padrão — o sistema ainda coleta dados para melhorar.",
		"share_title":           "Compartilhar dados de uso anônimos",
		"share_desc":            "Ajuda a melhorar as sugestões de autocompletar.\nRegistra quais APIs você usa mais (ex: FindFirstChild vs WaitForChild).\nO conteúdo do código nunca é compartilhado.",
		"debug_title":           "Modo Debug",
		"debug_desc":            "Exibe mensagens do runtime GodotLuau no Output.\nQuando DESATIVADO, suprime os print() e warn() dos scripts Luau para manter o Output limpo.",
		"notif_outdated_title":  "Notificar versão desatualizada no Output",
		"notif_outdated_desc":   "Exibe um aviso no painel Output quando uma versão desatualizada do GodotLuau está em uso.",
		"speed_title":           "Autocompletar Instantâneo",
		"speed_desc":            "Ativa sugestões a cada tecla, sem esperar.\nDefine o atraso do editor para quase zero.",
		"cac_header":            "Autocompletar Personalizado",
		"cac_toggle_title":      "Usar autocompletar personalizado",
		"cac_toggle_desc":       "Substitua ou estenda as sugestões embutidas com sua própria lista JSON.\nCarregue de um arquivo local ou de uma URL raw do GitHub.",
		"cac_url_label":         "URL de origem (GitHub raw ou link direto):",
		"cac_url_hint":          "https://raw.githubusercontent.com/.../autocomplete.json",
		"cac_btn_download":      "⬇ Baixar",
		"cac_btn_import":        "📂 Importar JSON",
		"cac_btn_clear":         "🗑 Limpar",
		"cac_status_none":       "Nenhum autocompletar personalizado carregado.",
		"cac_status_ok":         "✅ %d itens personalizados carregados.",
		"cac_status_dl":         "⏳ Baixando...",
		"cac_status_err":        "❌ Download falhou — verifique a URL.",
		"cac_status_bad_json":   "❌ Formato JSON inválido (era esperado um array).",
		"stats_header":          "Dados coletados",
		"stats_none":            "Nenhum dado coletado ainda.",
		"stats_size":            "Tamanho: %.1f KB  |  Entradas: ~%d",
		"stats_err":             "Não foi possível ler o arquivo de dados.",
		"btn_refresh":           "↺ Atualizar",
		"btn_open":              "📂 Abrir pasta",
		"btn_clear":             "🗑 Apagar dados",
		"dlg_clear_title":       "Apagar dados de uso",
		"dlg_clear_text":        "Excluir todos os dados de uso coletados?\nIsso não afeta a funcionalidade do plugin.",
		"bar_checking":          "🔍 Verificando...",
		"bar_uptodate":          "✅ %s — atualizado.",
		"bar_newver":            "%s  →  %s",
		"bar_btn_update":        "⬇ Atualizar",
		"bar_downloading":       "⬇ Baixando %s...",
		"bar_dl_failed":         "❌ Download falhou (código %d).",
		"bar_extracting":        "📦 Extraindo...",
		"bar_ok_no_restart":     "✅ %s instalado.",
		"bar_ok_restart":        "✅ Atualizado — clique para reiniciar.",
		"bar_apply_close":       "✅ Pronto. O Godot fechará e aplicará a DLL.",
		"bar_restart":           "🔄 Reiniciar editor",
		"bar_restarting":        "🔄 Reiniciando...",
		"bar_data_cleared":      "🗑 Dados excluídos.",
		"bar_dl_err":            "❌ Não foi possível iniciar o download.",
		"bar_retry":             "Tentar novamente",
		"bar_script_err":        "❌ Não foi possível escrever o script de atualização.",
		"ver_unknown":           "desconhecida",
		"btn_check_ver":         "🔍",
		"btn_reinstall":         "↺ Reinstalar",
		"bar_reinstalling":      "↺ Reinstalando %s...",
		"github_label":          "Fonte oficial:",
		"footer_data":           "Arquivo de dados: user://godotluau_usage.json",
		"warn_watermark":        "⚠ Marca d'água do GodotLuau não encontrada — pode ser versão modificada. Reinstale em: " + GITHUB_URL,
	}
}

# ── State ─────────────────────────────────────────────────────────────────────
var _http_version      : HTTPRequest    = null
var _http_download     : HTTPRequest    = null
var _http_autocomplete : HTTPRequest    = null
var _bar_action        : String         = ""
var _remote_version    : String         = ""
var _downloading       : bool           = false
var _settings_panel    : Control        = null
var _stats_label       : Label          = null
var _cac_status        : Label          = null
var _cac_url_field     : LineEdit       = null
var _lang              : String         = "en"
var _font_scale        : float          = 1.0
var _first_build       : bool           = true
var _ver_label         : Label          = null
var _ver_btn           : Button         = null
var _reinstall_btn     : Button         = null
var _notif_bar         : PanelContainer = null
var _notif_label       : Label          = null
var _outdated_timer    : Timer          = null
var _wm_timer          : Timer          = null

# ── Lifecycle ─────────────────────────────────────────────────────────────────

func _enter_tree() -> void:
	_detect_editor_lang()
	_load_font_pref()
	_sync_plugin_cfg()
	_cleanup_old_dlls()
	_register_settings()
	_apply_autocomplete_speed()
	_build_settings_panel()
	_check_watermark()
	_check_for_update()

func _exit_tree() -> void:
	for n in [_http_version, _http_download, _http_autocomplete, _outdated_timer, _wm_timer]:
		if n and is_instance_valid(n): n.queue_free()
	if _settings_panel and is_instance_valid(_settings_panel):
		remove_control_from_bottom_panel(_settings_panel)
		_settings_panel.queue_free()

# ── Language ──────────────────────────────────────────────────────────────────

func _detect_editor_lang() -> void:
	var es := EditorInterface.get_editor_settings()
	var locale : String = "en"
	if es.has_setting("interface/editor/editor_language"):
		locale = str(es.get_setting("interface/editor/editor_language"))
	if   locale.begins_with("es"): _lang = "es"
	elif locale.begins_with("pt"): _lang = "pt"
	else:                          _lang = "en"

func _t(key: String) -> String:
	var d : Dictionary = TR.get(_lang, TR["en"])
	return d.get(key, (TR["en"] as Dictionary).get(key, key))

# ── Font preference (per-user, stored in EditorSettings) ─────────────────────

func _load_font_pref() -> void:
	var es := EditorInterface.get_editor_settings()
	var idx := 1
	if es.has_setting("godot_luau/panel_font_scale"):
		idx = int(es.get_setting("godot_luau/panel_font_scale"))
	_apply_font_idx(idx)

func _apply_font_idx(idx: int) -> void:
	match idx:
		0: _font_scale = 0.85
		2: _font_scale = 1.2
		_: _font_scale = 1.0

func _fs(base: int) -> int:
	return max(9, int(base * _font_scale))

# ── Plugin.cfg auto-sync from res://Version ───────────────────────────────────

func _sync_plugin_cfg() -> void:
	for old in ["user://godotluau_version.txt",
				"res://addons/GodotLuauUpdater/installed_version.txt"]:
		if FileAccess.file_exists(old):
			DirAccess.remove_absolute(ProjectSettings.globalize_path(old) \
				if old.begins_with("res://") \
				else OS.get_user_data_dir().path_join(old.get_file()))

	var ver := _get_local_version().trim_prefix("v")
	if ver == _t("ver_unknown"): return
	var cfg_path := "res://addons/GodotLuauUpdater/plugin.cfg"
	if not FileAccess.file_exists(cfg_path): return
	var f := FileAccess.open(cfg_path, FileAccess.READ)
	if not f: return
	var lines := f.get_as_text().split("\n")
	var changed := false
	for i in lines.size():
		if lines[i].begins_with("version="):
			var expected := 'version="%s"' % ver
			if lines[i] != expected:
				lines[i] = expected
				changed = true
			break
	if not changed: return
	var wf := FileAccess.open(cfg_path, FileAccess.WRITE)
	if wf: wf.store_string("\n".join(lines))

# ── Watermark integrity check ─────────────────────────────────────────────────

func _check_watermark() -> void:
	if Engine.has_meta(WM_META_KEY): return
	push_warning("[GodotLuau] " + _t("warn_watermark"))
	if _wm_timer and is_instance_valid(_wm_timer): return
	_wm_timer = Timer.new()
	_wm_timer.wait_time = 10.0
	_wm_timer.autostart = true
	add_child(_wm_timer)
	_wm_timer.timeout.connect(func():
		if not Engine.has_meta(WM_META_KEY):
			push_warning("[GodotLuau] ⚠ Watermark still missing! Reinstall from: " + GITHUB_URL)
	)

# ── Outdated version console warning ─────────────────────────────────────────

func _setup_outdated_warning(local: String, remote: String) -> void:
	if not ProjectSettings.get_setting("godot_luau/notify_outdated_version", true): return
	push_warning("[GodotLuau] ⚠ Outdated version: %s — latest: %s | %s" % [local, remote, GITHUB_URL])
	if _outdated_timer and is_instance_valid(_outdated_timer): return
	_outdated_timer = Timer.new()
	_outdated_timer.wait_time = 30.0
	add_child(_outdated_timer)
	_outdated_timer.timeout.connect(func():
		if ProjectSettings.get_setting("godot_luau/notify_outdated_version", true):
			push_warning("[GodotLuau] ⚠ Still on outdated version: %s — latest: %s" % [local, remote])
	)
	_outdated_timer.start()

func _stop_outdated_warning() -> void:
	if _outdated_timer and is_instance_valid(_outdated_timer):
		_outdated_timer.stop()
		_outdated_timer.queue_free()
		_outdated_timer = null

# ── Autocomplete speed ────────────────────────────────────────────────────────

func _apply_autocomplete_speed() -> void:
	if not ProjectSettings.get_setting("godot_luau/instant_autocomplete", true): return
	var es := EditorInterface.get_editor_settings()
	es.set("text_editor/completion/code_complete_delay", 0.05)
	es.set("text_editor/completion/idle_parse_delay",    0.05)

# ── Cleanup old DLL backups ───────────────────────────────────────────────────

func _cleanup_old_dlls() -> void:
	var bin_path := ProjectSettings.globalize_path("res://bin/")
	if not DirAccess.dir_exists_absolute(bin_path): return
	var dir := DirAccess.open(bin_path)
	if not dir: return
	dir.list_dir_begin()
	var f := dir.get_next()
	while f != "":
		if f.ends_with(".dll.old") or f.ends_with(".so.old") or f.ends_with(".dylib.old"):
			DirAccess.remove_absolute(bin_path + f)
		f = dir.get_next()

# ── Settings registration ─────────────────────────────────────────────────────

func _register_settings() -> void:
	_add_bool("godot_luau/ai_autocomplete_enabled",    false,
		"Enable AI Smart Autocomplete — suggests values based on variable names.")
	_add_bool("godot_luau/share_data_enabled",         true,
		"Share anonymous usage data to improve the AI model.")
	_add_bool("godot_luau/debug_mode",                 false,
		"Show GodotLuau runtime messages in Output. When OFF, Luau print() is suppressed.")
	_add_bool("godot_luau/notify_outdated_version",    true,
		"Print a warning in Output when running an outdated GodotLuau version.")
	_add_bool("godot_luau/instant_autocomplete",       true,
		"Trigger autocomplete suggestions instantly on every keystroke.")
	_add_bool("godot_luau/custom_autocomplete_enabled", false,
		"Use a custom JSON autocomplete list.")
	_add_str("godot_luau/custom_autocomplete_url",     "",
		"URL to download a custom autocomplete JSON from.")

func _add_bool(key: String, val: bool, hint: String) -> void:
	if not ProjectSettings.has_setting(key): ProjectSettings.set_setting(key, val)
	ProjectSettings.set_initial_value(key, val)
	ProjectSettings.add_property_info({"name":key,"type":TYPE_BOOL,"hint":PROPERTY_HINT_NONE,"hint_string":hint})

func _add_str(key: String, val: String, hint: String) -> void:
	if not ProjectSettings.has_setting(key): ProjectSettings.set_setting(key, val)
	ProjectSettings.set_initial_value(key, val)
	ProjectSettings.add_property_info({"name":key,"type":TYPE_STRING,"hint":PROPERTY_HINT_NONE,"hint_string":hint})

# ── UI helpers ────────────────────────────────────────────────────────────────

func _make_section(text: String, color: Color) -> Control:
	var vb := VBoxContainer.new()
	vb.add_theme_constant_override("separation", 4)
	vb.add_child(HSeparator.new())
	var lbl := Label.new()
	lbl.text = text
	lbl.add_theme_font_size_override("font_size", _fs(14))
	lbl.add_theme_color_override("font_color", color)
	vb.add_child(lbl)
	return vb

func _make_row(title: String, desc: String, key: String, default_val: bool,
			   on_change: Callable = Callable()) -> Control:
	var vb := VBoxContainer.new()
	vb.add_theme_constant_override("separation", 3)
	var hb := HBoxContainer.new()
	hb.add_theme_constant_override("separation", 8)
	var tog := CheckButton.new()
	tog.button_pressed = ProjectSettings.get_setting(key, default_val)
	tog.toggled.connect(func(pressed: bool) -> void:
		ProjectSettings.set_setting(key, pressed)
		ProjectSettings.save()
		if on_change.is_valid(): on_change.call(pressed)
	)
	hb.add_child(tog)
	var lbl := Label.new()
	lbl.text = title
	lbl.add_theme_font_size_override("font_size", _fs(13))
	lbl.add_theme_color_override("font_color", Color(0.92, 0.92, 0.92))
	hb.add_child(lbl)
	vb.add_child(hb)
	if not desc.is_empty():
		var dlbl := Label.new()
		dlbl.text = desc
		dlbl.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		dlbl.add_theme_color_override("font_color", Color(0.72, 0.72, 0.72))
		dlbl.add_theme_font_size_override("font_size", _fs(11))
		vb.add_child(dlbl)
	return vb

# ── Settings panel ────────────────────────────────────────────────────────────

func _build_settings_panel() -> void:
	var scroll := ScrollContainer.new()
	scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.size_flags_vertical   = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	scroll.custom_minimum_size    = Vector2(0, 300)

	var outer := MarginContainer.new()
	for s in ["margin_left","margin_right","margin_top","margin_bottom"]:
		outer.add_theme_constant_override(s, 14)
	scroll.add_child(outer)

	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 8)
	outer.add_child(vbox)

	# ── Header ──────────────────────────────────────────────────────────
	var hdr_row := HBoxContainer.new()
	hdr_row.add_theme_constant_override("separation", 8)
	vbox.add_child(hdr_row)

	var hdr := Label.new()
	hdr.text = _t("panel_title")
	hdr.add_theme_color_override("font_color", Color(0.4, 0.8, 1.0))
	hdr.add_theme_font_size_override("font_size", _fs(16))
	hdr.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hdr_row.add_child(hdr)

	# Font size selector
	var font_opt := OptionButton.new()
	font_opt.add_item(_t("font_small"),  0)
	font_opt.add_item(_t("font_normal"), 1)
	font_opt.add_item(_t("font_large"),  2)
	var es_fs := EditorInterface.get_editor_settings()
	var cur_fs_idx := 1
	if es_fs.has_setting("godot_luau/panel_font_scale"):
		cur_fs_idx = int(es_fs.get_setting("godot_luau/panel_font_scale"))
	font_opt.select(cur_fs_idx)
	font_opt.item_selected.connect(func(idx: int) -> void:
		EditorInterface.get_editor_settings().set_setting("godot_luau/panel_font_scale", idx)
		_apply_font_idx(idx)
		_rebuild_panel()
	)
	hdr_row.add_child(font_opt)

	# Language selector
	var lang_lbl := Label.new()
	lang_lbl.text = "🌐"
	hdr_row.add_child(lang_lbl)
	var lang_opt := OptionButton.new()
	lang_opt.add_item("EN", 0)
	lang_opt.add_item("ES", 1)
	lang_opt.add_item("PT", 2)
	match _lang:
		"es": lang_opt.select(1)
		"pt": lang_opt.select(2)
		_:    lang_opt.select(0)
	lang_opt.item_selected.connect(func(idx: int) -> void:
		match idx:
			0: _lang = "en"
			1: _lang = "es"
			2: _lang = "pt"
		_rebuild_panel()
	)
	hdr_row.add_child(lang_opt)

	# ── Notification bar (transient messages) ───────────────────────────
	_notif_bar = PanelContainer.new()
	_notif_bar.visible = false
	var notif_hb := HBoxContainer.new()
	notif_hb.add_theme_constant_override("separation", 8)
	_notif_bar.add_child(notif_hb)
	_notif_label = Label.new()
	_notif_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_notif_label.add_theme_font_size_override("font_size", _fs(11))
	notif_hb.add_child(_notif_label)
	var notif_x := Button.new()
	notif_x.flat = true
	notif_x.text = "✕"
	notif_x.pressed.connect(func(): _notif_bar.visible = false)
	notif_hb.add_child(notif_x)
	vbox.add_child(_notif_bar)

	# ══ 🤖 AUTOCOMPLETE ════════════════════════════════════════════════
	vbox.add_child(_make_section(_t("sec_autocomplete"), Color(0.4, 0.8, 1.0)))

	vbox.add_child(_make_row(_t("ai_title"), _t("ai_desc"),
		"godot_luau/ai_autocomplete_enabled", false))

	vbox.add_child(_make_row(_t("speed_title"), _t("speed_desc"),
		"godot_luau/instant_autocomplete", true,
		func(on: bool) -> void:
			if on: _apply_autocomplete_speed()
	))

	var cac_hdr := Label.new()
	cac_hdr.text = "  " + _t("cac_header")
	cac_hdr.add_theme_font_size_override("font_size", _fs(12))
	cac_hdr.add_theme_color_override("font_color", Color(0.6, 0.9, 0.6))
	vbox.add_child(cac_hdr)

	vbox.add_child(_make_row(_t("cac_toggle_title"), _t("cac_toggle_desc"),
		"godot_luau/custom_autocomplete_enabled", false))

	var url_lbl := Label.new()
	url_lbl.text = _t("cac_url_label")
	url_lbl.add_theme_font_size_override("font_size", _fs(11))
	url_lbl.add_theme_color_override("font_color", Color(0.72, 0.72, 0.72))
	vbox.add_child(url_lbl)

	var url_row := HBoxContainer.new()
	url_row.add_theme_constant_override("separation", 6)
	vbox.add_child(url_row)
	_cac_url_field = LineEdit.new()
	_cac_url_field.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_cac_url_field.placeholder_text = _t("cac_url_hint")
	_cac_url_field.text = str(ProjectSettings.get_setting("godot_luau/custom_autocomplete_url", ""))
	_cac_url_field.text_changed.connect(func(txt: String) -> void:
		ProjectSettings.set_setting("godot_luau/custom_autocomplete_url", txt)
		ProjectSettings.save()
	)
	url_row.add_child(_cac_url_field)
	var btn_dl := Button.new()
	btn_dl.text = _t("cac_btn_download")
	btn_dl.pressed.connect(_download_custom_ac)
	url_row.add_child(btn_dl)

	var cac_btns := HBoxContainer.new()
	cac_btns.add_theme_constant_override("separation", 8)
	vbox.add_child(cac_btns)
	var btn_import := Button.new()
	btn_import.text = _t("cac_btn_import")
	btn_import.pressed.connect(_import_custom_ac_file)
	cac_btns.add_child(btn_import)
	var btn_clr_ac := Button.new()
	btn_clr_ac.text = _t("cac_btn_clear")
	btn_clr_ac.add_theme_color_override("font_color", Color(1.0, 0.6, 0.6))
	btn_clr_ac.pressed.connect(_clear_custom_ac)
	cac_btns.add_child(btn_clr_ac)

	_cac_status = Label.new()
	_cac_status.add_theme_font_size_override("font_size", _fs(11))
	vbox.add_child(_cac_status)
	_refresh_cac_status()

	# ══ 📊 USAGE DATA ══════════════════════════════════════════════════
	vbox.add_child(_make_section(_t("sec_data"), Color(0.5, 0.9, 0.5)))

	vbox.add_child(_make_row(_t("share_title"), _t("share_desc"),
		"godot_luau/share_data_enabled", true))

	var stats_sub := Label.new()
	stats_sub.text = "  " + _t("stats_header")
	stats_sub.add_theme_font_size_override("font_size", _fs(11))
	stats_sub.add_theme_color_override("font_color", Color(0.72, 0.72, 0.72))
	vbox.add_child(stats_sub)

	_stats_label = Label.new()
	_stats_label.add_theme_color_override("font_color", Color(0.72, 0.72, 0.72))
	_stats_label.add_theme_font_size_override("font_size", _fs(11))
	vbox.add_child(_stats_label)
	_refresh_stats()

	var stat_btns := HBoxContainer.new()
	stat_btns.add_theme_constant_override("separation", 8)
	vbox.add_child(stat_btns)
	var btn_ref := Button.new()
	btn_ref.text = _t("btn_refresh")
	btn_ref.pressed.connect(_refresh_stats)
	stat_btns.add_child(btn_ref)
	var btn_open := Button.new()
	btn_open.text = _t("btn_open")
	btn_open.pressed.connect(func(): OS.shell_open(OS.get_user_data_dir()))
	stat_btns.add_child(btn_open)
	var btn_del := Button.new()
	btn_del.text = _t("btn_clear")
	btn_del.add_theme_color_override("font_color", Color(1.0, 0.5, 0.5))
	btn_del.pressed.connect(_confirm_clear_data)
	stat_btns.add_child(btn_del)

	# ══ 🔧 DEBUG ═══════════════════════════════════════════════════════
	vbox.add_child(_make_section(_t("sec_debug"), Color(1.0, 0.7, 0.3)))

	vbox.add_child(_make_row(_t("debug_title"), _t("debug_desc"),
		"godot_luau/debug_mode", false))

	vbox.add_child(_make_row(_t("notif_outdated_title"), _t("notif_outdated_desc"),
		"godot_luau/notify_outdated_version", true))

	# ══ 🔄 UPDATES ═════════════════════════════════════════════════════
	vbox.add_child(_make_section(_t("sec_updates"), Color(0.3, 0.9, 0.8)))

	var ver_row := HBoxContainer.new()
	ver_row.add_theme_constant_override("separation", 6)
	vbox.add_child(ver_row)

	_ver_label = Label.new()
	_ver_label.text = _get_local_version()
	_ver_label.add_theme_color_override("font_color", Color(0.72, 0.72, 0.72))
	_ver_label.add_theme_font_size_override("font_size", _fs(12))
	_ver_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	ver_row.add_child(_ver_label)

	_ver_btn = Button.new()
	_ver_btn.text = _t("btn_check_ver")
	_ver_btn.flat = true
	_ver_btn.tooltip_text = "Check for updates"
	_ver_btn.pressed.connect(_on_action_pressed)
	_bar_action = ""
	ver_row.add_child(_ver_btn)

	_reinstall_btn = Button.new()
	_reinstall_btn.text = _t("btn_reinstall")
	_reinstall_btn.flat = true
	_reinstall_btn.add_theme_color_override("font_color", Color(0.6, 0.75, 1.0))
	_reinstall_btn.tooltip_text = "Re-download and reinstall the current version"
	_reinstall_btn.pressed.connect(_start_reinstall)
	ver_row.add_child(_reinstall_btn)

	# ── Footer ──────────────────────────────────────────────────────────
	vbox.add_child(HSeparator.new())

	var footer := VBoxContainer.new()
	footer.add_theme_constant_override("separation", 3)
	vbox.add_child(footer)

	var gh_row := HBoxContainer.new()
	gh_row.add_theme_constant_override("separation", 6)
	footer.add_child(gh_row)
	var gh_lbl := Label.new()
	gh_lbl.text = _t("github_label")
	gh_lbl.add_theme_font_size_override("font_size", _fs(11))
	gh_lbl.add_theme_color_override("font_color", Color(0.55, 0.55, 0.55))
	gh_row.add_child(gh_lbl)
	var gh_link := Label.new()
	gh_link.text = "github.com/Pimpoli/GodotLuau"
	gh_link.add_theme_font_size_override("font_size", _fs(11))
	gh_link.add_theme_color_override("font_color", Color(0.35, 0.65, 1.0))
	gh_link.mouse_filter = Control.MOUSE_FILTER_STOP
	gh_link.gui_input.connect(func(e: InputEvent) -> void:
		if e is InputEventMouseButton and e.button_index == MOUSE_BUTTON_LEFT and e.pressed:
			OS.shell_open(GITHUB_URL)
	)
	gh_row.add_child(gh_link)

	var data_lbl := Label.new()
	data_lbl.text = _t("footer_data")
	data_lbl.add_theme_color_override("font_color", Color(0.4, 0.4, 0.4))
	data_lbl.add_theme_font_size_override("font_size", _fs(10))
	footer.add_child(data_lbl)

	_settings_panel = scroll
	add_control_to_bottom_panel(scroll, "GodotLuau Config")
	if _first_build:
		make_bottom_panel_item_visible(scroll)
		_first_build = false

func _rebuild_panel() -> void:
	if _settings_panel and is_instance_valid(_settings_panel):
		remove_control_from_bottom_panel(_settings_panel)
		_settings_panel.queue_free()
	_stats_label   = null
	_cac_status    = null
	_cac_url_field = null
	_ver_label     = null
	_ver_btn       = null
	_reinstall_btn = null
	_notif_bar     = null
	_notif_label   = null
	_build_settings_panel()

# ── Custom autocomplete ───────────────────────────────────────────────────────

func _refresh_cac_status() -> void:
	if not _cac_status or not is_instance_valid(_cac_status): return
	var path := OS.get_user_data_dir() + "/godotluau_custom_autocomplete.json"
	if not FileAccess.file_exists(path):
		_cac_status.text = _t("cac_status_none")
		_cac_status.add_theme_color_override("font_color", Color(0.55, 0.55, 0.55))
		return
	var f := FileAccess.open(path, FileAccess.READ)
	if not f: _cac_status.text = _t("cac_status_none"); return
	var parsed = JSON.parse_string(f.get_as_text())
	if parsed is Array:
		_cac_status.text = _t("cac_status_ok") % (parsed as Array).size()
		_cac_status.add_theme_color_override("font_color", Color(0.4, 0.9, 0.4))
	else:
		_cac_status.text = _t("cac_status_bad_json")
		_cac_status.add_theme_color_override("font_color", Color(1.0, 0.5, 0.5))

func _download_custom_ac() -> void:
	if _http_autocomplete and is_instance_valid(_http_autocomplete): return
	var url := str(ProjectSettings.get_setting("godot_luau/custom_autocomplete_url", ""))
	if url.is_empty(): return
	if _cac_status and is_instance_valid(_cac_status):
		_cac_status.text = _t("cac_status_dl")
		_cac_status.add_theme_color_override("font_color", Color(0.9, 0.85, 0.3))
	_http_autocomplete = HTTPRequest.new()
	_http_autocomplete.timeout = 20.0
	add_child(_http_autocomplete)
	_http_autocomplete.request_completed.connect(_on_custom_ac_downloaded)
	if _http_autocomplete.request(url) != OK:
		_http_autocomplete.queue_free(); _http_autocomplete = null
		if _cac_status and is_instance_valid(_cac_status):
			_cac_status.text = _t("cac_status_err")
			_cac_status.add_theme_color_override("font_color", Color(1.0, 0.5, 0.5))

func _on_custom_ac_downloaded(result: int, code: int, _hdrs: PackedStringArray, body: PackedByteArray) -> void:
	if _http_autocomplete and is_instance_valid(_http_autocomplete):
		_http_autocomplete.queue_free(); _http_autocomplete = null
	if result != HTTPRequest.RESULT_SUCCESS or code != 200:
		if _cac_status and is_instance_valid(_cac_status):
			_cac_status.text = _t("cac_status_err")
			_cac_status.add_theme_color_override("font_color", Color(1.0, 0.5, 0.5))
		return
	_save_custom_ac(body.get_string_from_utf8())

func _import_custom_ac_file() -> void:
	var dlg := FileDialog.new()
	dlg.title     = _t("cac_btn_import")
	dlg.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	dlg.access    = FileDialog.ACCESS_FILESYSTEM
	dlg.filters   = ["*.json ; JSON files"]
	dlg.file_selected.connect(func(p: String) -> void:
		var text := FileAccess.get_file_as_string(p)
		if not text.is_empty(): _save_custom_ac(text)
		dlg.queue_free()
	)
	dlg.canceled.connect(func(): dlg.queue_free())
	get_editor_interface().get_base_control().add_child(dlg)
	dlg.popup_centered_ratio(0.6)

func _save_custom_ac(json_text: String) -> void:
	var parsed = JSON.parse_string(json_text)
	if not _cac_status or not is_instance_valid(_cac_status): return
	if not parsed is Array:
		_cac_status.text = _t("cac_status_bad_json")
		_cac_status.add_theme_color_override("font_color", Color(1.0, 0.5, 0.5))
		return
	var path := OS.get_user_data_dir() + "/godotluau_custom_autocomplete.json"
	var f := FileAccess.open(path, FileAccess.WRITE)
	if f: f.store_string(json_text)
	_cac_status.text = _t("cac_status_ok") % (parsed as Array).size()
	_cac_status.add_theme_color_override("font_color", Color(0.4, 0.9, 0.4))

func _clear_custom_ac() -> void:
	var path := OS.get_user_data_dir() + "/godotluau_custom_autocomplete.json"
	if FileAccess.file_exists(path): DirAccess.remove_absolute(path)
	_refresh_cac_status()

# ── Usage stats ───────────────────────────────────────────────────────────────

func _refresh_stats() -> void:
	if not _stats_label or not is_instance_valid(_stats_label): return
	var path := OS.get_user_data_dir() + "/godotluau_usage.json"
	if not FileAccess.file_exists(path): _stats_label.text = _t("stats_none"); return
	var f := FileAccess.open(path, FileAccess.READ)
	if not f: _stats_label.text = _t("stats_err"); return
	_stats_label.text = _t("stats_size") % [f.get_length() / 1024.0, f.get_as_text().count("\n")]

func _confirm_clear_data() -> void:
	var dlg := ConfirmationDialog.new()
	dlg.title       = _t("dlg_clear_title")
	dlg.dialog_text = _t("dlg_clear_text")
	dlg.confirmed.connect(_clear_data)
	get_editor_interface().get_base_control().add_child(dlg)
	dlg.popup_centered()

func _clear_data() -> void:
	var path := OS.get_user_data_dir() + "/godotluau_usage.json"
	if FileAccess.file_exists(path): DirAccess.remove_absolute(path)
	_refresh_stats()
	_set_notif(_t("bar_data_cleared"), Color(0.5, 0.9, 0.5))

# ── Version status (inline) ───────────────────────────────────────────────────

func _set_ver_status(msg: String, btn_text: String, color: Color, action: String = "") -> void:
	if not (_ver_label and is_instance_valid(_ver_label)): return
	_ver_label.text = msg
	_ver_label.add_theme_color_override("font_color", color)
	_bar_action = action
	if not (_ver_btn and is_instance_valid(_ver_btn)): return
	if btn_text.is_empty():
		_ver_btn.visible = false
	else:
		_ver_btn.text    = btn_text
		_ver_btn.visible = true

func _set_notif(msg: String, color: Color) -> void:
	if not (_notif_bar and is_instance_valid(_notif_bar)): return
	_notif_label.text = msg
	_notif_label.add_theme_color_override("font_color", color)
	_notif_bar.visible = true
	get_tree().create_timer(4.0).timeout.connect(func():
		if _notif_bar and is_instance_valid(_notif_bar): _notif_bar.visible = false
	)

func _reset_ver_idle() -> void:
	_set_ver_status(_get_local_version(), _t("btn_check_ver"), Color(0.72, 0.72, 0.72))
	if _reinstall_btn and is_instance_valid(_reinstall_btn):
		_reinstall_btn.disabled = false

func _on_action_pressed() -> void:
	match _bar_action:
		"restart":  _do_editor_restart()
		"apply":    _apply_windows_update_legacy()
		"download": if not _downloading: _start_download()
		_:          _check_for_update()

# ── Version check ─────────────────────────────────────────────────────────────

func _check_for_update() -> void:
	if _http_version and is_instance_valid(_http_version): return
	_http_version = HTTPRequest.new()
	_http_version.timeout = 10.0
	add_child(_http_version)
	_http_version.request_completed.connect(_on_version_received)
	_set_ver_status(_t("bar_checking"), "", Color(0.5, 0.8, 1.0))
	if _reinstall_btn and is_instance_valid(_reinstall_btn): _reinstall_btn.disabled = true
	if _http_version.request(VERSION_URL) != OK:
		_http_version.queue_free(); _http_version = null
		_reset_ver_idle()

func _on_version_received(result: int, code: int, _hdrs: PackedStringArray, body: PackedByteArray) -> void:
	if _http_version and is_instance_valid(_http_version):
		_http_version.queue_free(); _http_version = null
	if result != HTTPRequest.RESULT_SUCCESS or code != 200:
		_reset_ver_idle(); return
	_remote_version = body.get_string_from_utf8().strip_edges()
	var local := _get_local_version()
	if _remote_version == local:
		_stop_outdated_warning()
		_set_ver_status(_t("bar_uptodate") % local, "", Color(0.4, 0.9, 0.5))
		if _reinstall_btn and is_instance_valid(_reinstall_btn): _reinstall_btn.disabled = false
		await get_tree().create_timer(4.0).timeout
		if _ver_label and is_instance_valid(_ver_label): _reset_ver_idle()
		return
	_setup_outdated_warning(local, _remote_version)
	if _reinstall_btn and is_instance_valid(_reinstall_btn): _reinstall_btn.disabled = false
	_set_ver_status(
		_t("bar_newver") % [local, _remote_version],
		_t("bar_btn_update"),
		Color(1.0, 0.9, 0.3),
		"download"
	)

func _get_local_version() -> String:
	if not FileAccess.file_exists(VERSION_FILE): return _t("ver_unknown")
	var f := FileAccess.open(VERSION_FILE, FileAccess.READ)
	return f.get_as_text().strip_edges() if f else _t("ver_unknown")

# ── Download & reinstall ──────────────────────────────────────────────────────

func _start_reinstall() -> void:
	if _downloading: return
	if _remote_version.is_empty(): _remote_version = _get_local_version()
	_set_ver_status(_t("bar_reinstalling") % _get_local_version(), "", Color(0.4, 0.8, 1.0))
	if _reinstall_btn and is_instance_valid(_reinstall_btn): _reinstall_btn.disabled = true
	_start_download()

func _start_download() -> void:
	_downloading = true
	if not (_ver_label and is_instance_valid(_ver_label) and _ver_label.text.begins_with("↺")):
		_set_ver_status(_t("bar_downloading") % _remote_version, "", Color(0.4, 0.8, 1.0))
	_http_download = HTTPRequest.new()
	_http_download.use_threads   = true
	_http_download.download_file = OS.get_user_data_dir() + "/godotluau_update.zip"
	add_child(_http_download)
	_http_download.request_completed.connect(_on_download_completed)
	if _http_download.request(ZIP_URL) != OK:
		_set_ver_status(_t("bar_dl_err"), _t("bar_retry"), Color(1.0, 0.4, 0.4), "download")
		_downloading = false
		if _reinstall_btn and is_instance_valid(_reinstall_btn): _reinstall_btn.disabled = false
		_http_download.queue_free(); _http_download = null

func _on_download_completed(result: int, code: int, _hdrs: PackedStringArray, _body: PackedByteArray) -> void:
	if _http_download and is_instance_valid(_http_download):
		_http_download.queue_free(); _http_download = null
	_downloading = false
	if result != HTTPRequest.RESULT_SUCCESS or (code != 200 and code != 0):
		_set_ver_status(_t("bar_dl_failed") % code, _t("bar_retry"), Color(1.0, 0.4, 0.4), "download")
		if _reinstall_btn and is_instance_valid(_reinstall_btn): _reinstall_btn.disabled = false
		return
	_set_ver_status(_t("bar_extracting"), "", Color(0.4, 0.8, 1.0))
	_apply_update.call_deferred()

# ── Apply update ──────────────────────────────────────────────────────────────

var _windows_staged_dlls : Array[Dictionary] = []

func _apply_update() -> void:
	var zip_path  := OS.get_user_data_dir() + "/godotluau_update.zip"
	var proj_path := ProjectSettings.globalize_path("res://")
	var zip := ZIPReader.new()
	if zip.open(zip_path) != OK:
		_set_ver_status(_t("bar_dl_failed") % 0, _t("bar_retry"), Color(1.0, 0.4, 0.4), "download")
		if _reinstall_btn and is_instance_valid(_reinstall_btn): _reinstall_btn.disabled = false
		return

	var staged  : Array[Dictionary] = []
	var staging := OS.get_user_data_dir() + "/godotluau_staging/"
	DirAccess.make_dir_recursive_absolute(staging)
	var is_win : bool = (OS.get_name() == "Windows")

	for rel in zip.get_files():
		if rel.ends_with("/"): continue
		var data    : PackedByteArray = zip.read_file(rel)
		var dst     : String          = proj_path + rel
		var dst_dir : String          = dst.get_base_dir()
		if not DirAccess.dir_exists_absolute(dst_dir):
			DirAccess.make_dir_recursive_absolute(dst_dir)
		if is_win and rel.get_extension().to_lower() in DLL_EXTENSIONS:
			var stage := staging + rel.get_file()
			var sf := FileAccess.open(stage, FileAccess.WRITE)
			if sf: sf.store_buffer(data); staged.append({"src": stage, "dst": dst})
		else:
			var f := FileAccess.open(dst, FileAccess.WRITE)
			if f: f.store_buffer(data)

	zip.close()
	var vf := FileAccess.open(VERSION_FILE, FileAccess.WRITE)
	if vf: vf.store_string(_remote_version + "\n")
	DirAccess.remove_absolute(zip_path)
	if _reinstall_btn and is_instance_valid(_reinstall_btn): _reinstall_btn.disabled = false

	if staged.is_empty():
		_set_ver_status(_t("bar_ok_no_restart") % _remote_version, "", Color(0.3, 0.9, 0.5))
		_refresh_stats(); return

	_windows_staged_dlls = staged
	if is_win and _try_rename_and_replace():
		_set_ver_status(_t("bar_ok_restart"), _t("bar_restart"), Color(0.4, 0.9, 1.0), "restart")
	else:
		_set_ver_status(_t("bar_apply_close"), "Apply & Close", Color(1.0, 0.7, 0.2), "apply")

func _try_rename_and_replace() -> bool:
	for item in _windows_staged_dlls:
		var dst : String = item["dst"]
		var src : String = item["src"]
		if not FileAccess.file_exists(dst):
			var data := FileAccess.get_file_as_bytes(src)
			if data.is_empty(): return false
			var f := FileAccess.open(dst, FileAccess.WRITE)
			if not f: return false
			f.store_buffer(data); continue
		var old := dst + ".old"
		if DirAccess.rename_absolute(dst, old) != OK: return false
		var data := FileAccess.get_file_as_bytes(src)
		if data.is_empty(): DirAccess.rename_absolute(old, dst); return false
		var f := FileAccess.open(dst, FileAccess.WRITE)
		if not f: DirAccess.rename_absolute(old, dst); return false
		f.store_buffer(data)
	return true

func _do_editor_restart() -> void:
	_set_ver_status(_t("bar_restarting"), "", Color(0.4, 0.8, 1.0))
	await get_tree().create_timer(0.5).timeout
	get_editor_interface().restart_editor()

func _apply_windows_update_legacy() -> void:
	var ps_path  := OS.get_user_data_dir() + "/_godotluau_apply.ps1"
	var godot    := OS.get_executable_path().replace("/", "\\")
	var proj_dir := ProjectSettings.globalize_path("res://").rstrip("/\\").replace("/", "\\")
	var pid      := str(OS.get_process_id())

	var lines := PackedStringArray()
	lines.append("# GodotLuau auto-updater")
	lines.append("$ErrorActionPreference = 'Continue'")
	lines.append("$godotPid = %s" % pid)
	lines.append("$maxWait = 30; $waited = 0")
	lines.append("while ($waited -lt $maxWait) {")
	lines.append("    if (-not (Get-Process -Id $godotPid -ErrorAction SilentlyContinue)) { break }")
	lines.append("    Start-Sleep -Milliseconds 400; $waited++")
	lines.append("}")
	lines.append("Start-Sleep -Milliseconds 600")
	for item in _windows_staged_dlls:
		var s : String = item["src"].replace("/", "\\")
		var d : String = item["dst"].replace("/", "\\")
		lines.append("try { Copy-Item -Force -Path \"%s\" -Destination \"%s\" } catch { Write-Host $_.Exception.Message }" % [s, d])
	lines.append("Start-Sleep -Milliseconds 300")
	lines.append("& \"%s\" --path \"%s\"" % [godot, proj_dir])

	var pf := FileAccess.open(ps_path, FileAccess.WRITE)
	if not pf: _set_ver_status(_t("bar_script_err"), "", Color(1.0, 0.4, 0.4)); return
	pf.store_string("\r\n".join(lines)); pf.close()

	OS.create_process("powershell.exe", [
		"-WindowStyle", "Hidden", "-NonInteractive",
		"-ExecutionPolicy", "Bypass", "-File", ps_path.replace("/", "\\")
	])
	await get_tree().create_timer(0.3).timeout
	get_tree().quit()
