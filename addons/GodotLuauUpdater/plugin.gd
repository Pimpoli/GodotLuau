@tool
extends EditorPlugin

const VERSION_URL    := "https://raw.githubusercontent.com/Pimpoli/GodotLuau/main/Version"
const ZIP_URL        := "https://github.com/Pimpoli/GodotLuau/raw/main/GodotLuau.zip"
const VERSION_FILE   := "res://addons/GodotLuauUpdater/installed_version.txt"
const DATA_FILE      := "user://godotluau_usage.json"
const CUSTOM_AC_FILE := "user://godotluau_custom_autocomplete.json"
const DLL_EXTENSIONS := ["dll", "so", "dylib", "framework"]

# ── Translation dictionary — EN / ES / PT-BR ─────────────────────────────────
const TR := {
	"en": {
		"panel_title":          "GodotLuau — AI & Autocomplete",
		"installed":            "Installed: %s",
		"ai_title":             "AI Smart Autocomplete",
		"ai_desc":              "Suggests values based on variable names (e.g. 'speed =' → '16').\nDisabled by default — data is still collected to improve suggestions.",
		"share_title":          "Share anonymous usage data",
		"share_desc":           "Improves future suggestions with anonymous usage patterns.\nCode content is never shared — only which APIs you use most.",
		"debug_title":          "Debug Mode",
		"debug_desc":           "Show debug output from GodotLuau runtime in the Output panel.",
		"speed_title":          "Instant Autocomplete",
		"speed_desc":           "Triggers suggestions on every keystroke without waiting.\nSets the editor completion delay to near-zero.",
		"cac_header":           "Custom Autocomplete",
		"cac_toggle_title":     "Use custom autocomplete",
		"cac_toggle_desc":      "Replace or extend built-in suggestions with your own JSON API list.\nLoad from a local file or a GitHub raw URL.",
		"cac_url_label":        "Source URL (GitHub raw or direct link):",
		"cac_url_hint":         "https://raw.githubusercontent.com/.../autocomplete.json",
		"cac_btn_download":     "⬇ Download",
		"cac_btn_import":       "📂 Import JSON file",
		"cac_btn_clear":        "🗑 Clear custom",
		"cac_status_none":      "No custom autocomplete loaded.",
		"cac_status_ok":        "✅ %d custom items loaded.",
		"cac_status_dl":        "⏳ Downloading...",
		"cac_status_err":       "❌ Download failed — check the URL.",
		"cac_status_bad_json":  "❌ Invalid JSON format (expected an array).",
		"stats_header":         "Collected usage data",
		"stats_none":           "No data collected yet.",
		"stats_size":           "Size: %.1f KB  |  Entries: ~%d",
		"stats_err":            "Could not read data file.",
		"btn_refresh":          "↺ Refresh",
		"btn_open":             "📂 Open folder",
		"btn_clear":            "🗑 Delete data",
		"upd_header":           "Updates",
		"btn_check":            "🔍 Check for updates",
		"footer":               "[color=#666666]Data: [b]user://godotluau_usage.json[/b]\nContribute: [color=#4d9de0]github.com/Pimpoli/IALuauAutoCompleted[/color][/color]",
		"dlg_clear_title":      "Delete usage data",
		"dlg_clear_text":       "Delete all collected usage data?\nThis does not affect plugin functionality.",
		"lang_label":           "Language:",
		"bar_checking":         "🔍 Checking for updates...",
		"bar_uptodate":         "✅ GodotLuau %s — up to date.",
		"bar_newver":           "New version: %s  (installed: %s)",
		"bar_btn_update":       "⬇ Update",
		"bar_downloading":      "⬇ Downloading GodotLuau %s...",
		"bar_dl_failed":        "❌ Download failed (code %d).",
		"bar_extracting":       "📦 Extracting...",
		"bar_ok_no_restart":    "✅ GodotLuau %s installed.",
		"bar_ok_restart":       "✅ Updated — click to restart.",
		"bar_apply_close":      "✅ Ready. Godot will close and apply the DLL.",
		"bar_restart":          "🔄 Restart editor",
		"bar_restarting":       "🔄 Restarting...",
		"bar_data_cleared":     "🗑 Data deleted.",
		"bar_dl_err":           "❌ Could not start download.",
		"bar_retry":            "Retry",
		"bar_script_err":       "❌ Could not write update script.",
		"ver_unknown":          "unknown",
	},
	"es": {
		"panel_title":          "GodotLuau — IA y Autocompletado",
		"installed":            "Instalado: %s",
		"ai_title":             "Autocompletado Inteligente IA",
		"ai_desc":              "Sugiere valores según nombres de variable (ej: 'speed =' → '16').\nDesactivado por defecto — el sistema sigue recolectando datos para mejorar.",
		"share_title":          "Compartir datos de uso anónimos",
		"share_desc":           "Mejora las sugerencias futuras con patrones de uso anónimos.\nNunca se comparte contenido de código — solo qué APIs usas más.",
		"debug_title":          "Modo Debug",
		"debug_desc":           "Muestra la salida de depuración del runtime GodotLuau en el panel Output.",
		"speed_title":          "Autocompletado Instantáneo",
		"speed_desc":           "Activa sugerencias con cada tecla, sin esperar.\nEstablece el retraso del editor a casi cero.",
		"cac_header":           "Autocompletado Personalizado",
		"cac_toggle_title":     "Usar autocompletado personalizado",
		"cac_toggle_desc":      "Reemplaza o extiende las sugerencias incorporadas con tu propia lista JSON.\nCarga desde un archivo local o una URL raw de GitHub.",
		"cac_url_label":        "URL de origen (GitHub raw o enlace directo):",
		"cac_url_hint":         "https://raw.githubusercontent.com/.../autocomplete.json",
		"cac_btn_download":     "⬇ Descargar",
		"cac_btn_import":       "📂 Importar archivo JSON",
		"cac_btn_clear":        "🗑 Limpiar personalizado",
		"cac_status_none":      "Sin autocompletado personalizado cargado.",
		"cac_status_ok":        "✅ %d entradas personalizadas cargadas.",
		"cac_status_dl":        "⏳ Descargando...",
		"cac_status_err":       "❌ Descarga fallida — verifica la URL.",
		"cac_status_bad_json":  "❌ Formato JSON inválido (se esperaba un array).",
		"stats_header":         "Datos recolectados de uso",
		"stats_none":           "Sin datos recolectados todavía.",
		"stats_size":           "Tamaño: %.1f KB  |  Entradas: ~%d",
		"stats_err":            "No se pudo leer el archivo de datos.",
		"btn_refresh":          "↺ Actualizar",
		"btn_open":             "📂 Abrir carpeta",
		"btn_clear":            "🗑 Borrar datos",
		"upd_header":           "Actualizaciones",
		"btn_check":            "🔍 Buscar actualizaciones",
		"footer":               "[color=#666666]Datos en [b]user://godotluau_usage.json[/b]\nContribuye: [color=#4d9de0]github.com/Pimpoli/IALuauAutoCompleted[/color][/color]",
		"dlg_clear_title":      "Borrar datos de uso",
		"dlg_clear_text":       "¿Eliminar todos los datos de uso recolectados?\nEsto no afecta el funcionamiento del plugin.",
		"lang_label":           "Idioma:",
		"bar_checking":         "🔍 Buscando actualizaciones...",
		"bar_uptodate":         "✅ GodotLuau %s — al día.",
		"bar_newver":           "Nueva versión: %s  (instalada: %s)",
		"bar_btn_update":       "⬇ Actualizar",
		"bar_downloading":      "⬇ Descargando GodotLuau %s...",
		"bar_dl_failed":        "❌ Descarga fallida (código %d).",
		"bar_extracting":       "📦 Extrayendo...",
		"bar_ok_no_restart":    "✅ GodotLuau %s instalado.",
		"bar_ok_restart":       "✅ Actualizado — clic para reiniciar.",
		"bar_apply_close":      "✅ Listo. Godot se cerrará y aplicará la DLL.",
		"bar_restart":          "🔄 Reiniciar editor",
		"bar_restarting":       "🔄 Reiniciando...",
		"bar_data_cleared":     "🗑 Datos eliminados.",
		"bar_dl_err":           "❌ No se pudo iniciar la descarga.",
		"bar_retry":            "Reintentar",
		"bar_script_err":       "❌ No se pudo escribir el script de actualización.",
		"ver_unknown":          "desconocida",
	},
	"pt": {
		"panel_title":          "GodotLuau — IA e Autocompletar",
		"installed":            "Instalado: %s",
		"ai_title":             "Autocompletar Inteligente IA",
		"ai_desc":              "Sugere valores baseados em nomes de variáveis (ex: 'speed =' → '16').\nDesativado por padrão — o sistema ainda coleta dados para melhorar.",
		"share_title":          "Compartilhar dados de uso anônimos",
		"share_desc":           "Melhora sugestões futuras com padrões de uso anônimos.\nO conteúdo do código nunca é compartilhado — apenas quais APIs você usa mais.",
		"debug_title":          "Modo Debug",
		"debug_desc":           "Exibe a saída de depuração do runtime GodotLuau no painel Output.",
		"speed_title":          "Autocompletar Instantâneo",
		"speed_desc":           "Ativa sugestões a cada tecla, sem esperar.\nDefine o atraso do editor para quase zero.",
		"cac_header":           "Autocompletar Personalizado",
		"cac_toggle_title":     "Usar autocompletar personalizado",
		"cac_toggle_desc":      "Substitua ou estenda as sugestões embutidas com sua própria lista JSON.\nCarregue de um arquivo local ou de uma URL raw do GitHub.",
		"cac_url_label":        "URL de origem (GitHub raw ou link direto):",
		"cac_url_hint":         "https://raw.githubusercontent.com/.../autocomplete.json",
		"cac_btn_download":     "⬇ Baixar",
		"cac_btn_import":       "📂 Importar arquivo JSON",
		"cac_btn_clear":        "🗑 Limpar personalizado",
		"cac_status_none":      "Nenhum autocompletar personalizado carregado.",
		"cac_status_ok":        "✅ %d itens personalizados carregados.",
		"cac_status_dl":        "⏳ Baixando...",
		"cac_status_err":       "❌ Download falhou — verifique a URL.",
		"cac_status_bad_json":  "❌ Formato JSON inválido (era esperado um array).",
		"stats_header":         "Dados de uso coletados",
		"stats_none":           "Nenhum dado coletado ainda.",
		"stats_size":           "Tamanho: %.1f KB  |  Entradas: ~%d",
		"stats_err":            "Não foi possível ler o arquivo de dados.",
		"btn_refresh":          "↺ Atualizar",
		"btn_open":             "📂 Abrir pasta",
		"btn_clear":            "🗑 Apagar dados",
		"upd_header":           "Atualizações",
		"btn_check":            "🔍 Verificar atualizações",
		"footer":               "[color=#666666]Dados em [b]user://godotluau_usage.json[/b]\nContribua: [color=#4d9de0]github.com/Pimpoli/IALuauAutoCompleted[/color][/color]",
		"dlg_clear_title":      "Apagar dados de uso",
		"dlg_clear_text":       "Excluir todos os dados de uso coletados?\nIsso não afeta a funcionalidade do plugin.",
		"lang_label":           "Idioma:",
		"bar_checking":         "🔍 Verificando atualizações...",
		"bar_uptodate":         "✅ GodotLuau %s — atualizado.",
		"bar_newver":           "Nova versão: %s  (instalada: %s)",
		"bar_btn_update":       "⬇ Atualizar",
		"bar_downloading":      "⬇ Baixando GodotLuau %s...",
		"bar_dl_failed":        "❌ Download falhou (código %d).",
		"bar_extracting":       "📦 Extraindo...",
		"bar_ok_no_restart":    "✅ GodotLuau %s instalado.",
		"bar_ok_restart":       "✅ Atualizado — clique para reiniciar.",
		"bar_apply_close":      "✅ Pronto. O Godot fechará e aplicará a DLL.",
		"bar_restart":          "🔄 Reiniciar editor",
		"bar_restarting":       "🔄 Reiniciando...",
		"bar_data_cleared":     "🗑 Dados excluídos.",
		"bar_dl_err":           "❌ Não foi possível iniciar o download.",
		"bar_retry":            "Tentar novamente",
		"bar_script_err":       "❌ Não foi possível escrever o script de atualização.",
		"ver_unknown":          "desconhecida",
	}
}

# ── State ─────────────────────────────────────────────────────────────────────
var _http_version      : HTTPRequest    = null
var _http_download     : HTTPRequest    = null
var _http_autocomplete : HTTPRequest    = null
var _bar               : PanelContainer = null
var _bar_label         : Label          = null
var _bar_btn           : Button         = null
var _bar_action        : String         = ""
var _remote_version    : String         = ""
var _downloading       : bool           = false
var _settings_panel    : Control        = null
var _stats_label       : Label          = null
var _cac_status        : Label          = null
var _cac_url_field     : LineEdit       = null
var _lang              : String         = "en"

# ── Lifecycle ─────────────────────────────────────────────────────────────────

func _enter_tree() -> void:
	_detect_editor_lang()
	_cleanup_old_dlls()
	_register_settings()
	_apply_autocomplete_speed()
	_build_notification_bar()
	_build_settings_panel()
	_check_for_update()

func _exit_tree() -> void:
	for n in [_http_version, _http_download, _http_autocomplete]:
		if n and is_instance_valid(n): n.queue_free()
	if _bar and is_instance_valid(_bar):
		remove_control_from_bottom_panel(_bar)
		_bar.queue_free()
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

# ── Autocomplete speed ────────────────────────────────────────────────────────

func _apply_autocomplete_speed() -> void:
	if not ProjectSettings.get_setting("godot_luau/instant_autocomplete", true):
		return
	var es := EditorInterface.get_editor_settings()
	# Near-zero delay: suggestions appear on the next idle frame after typing
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
		"Show debug output from the GodotLuau runtime in the Output panel.")
	_add_bool("godot_luau/instant_autocomplete",       true,
		"Trigger autocomplete suggestions instantly on every keystroke (near-zero delay).")
	_add_bool("godot_luau/custom_autocomplete_enabled", false,
		"Use a custom JSON autocomplete list instead of (or in addition to) the built-in one.")
	_add_str("godot_luau/custom_autocomplete_url",     "",
		"URL to download a custom autocomplete JSON from (GitHub raw or direct link).")

func _add_bool(key: String, val: bool, hint: String) -> void:
	if not ProjectSettings.has_setting(key): ProjectSettings.set_setting(key, val)
	ProjectSettings.set_initial_value(key, val)
	ProjectSettings.add_property_info({"name":key,"type":TYPE_BOOL,"hint":PROPERTY_HINT_NONE,"hint_string":hint})

func _add_str(key: String, val: String, hint: String) -> void:
	if not ProjectSettings.has_setting(key): ProjectSettings.set_setting(key, val)
	ProjectSettings.set_initial_value(key, val)
	ProjectSettings.add_property_info({"name":key,"type":TYPE_STRING,"hint":PROPERTY_HINT_NONE,"hint_string":hint})

# ── Settings panel ────────────────────────────────────────────────────────────

func _build_settings_panel() -> void:
	var scroll := ScrollContainer.new()
	scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.size_flags_vertical   = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED

	var outer := MarginContainer.new()
	for s in ["margin_left","margin_right","margin_top","margin_bottom"]:
		outer.add_theme_constant_override(s, 14)
	scroll.add_child(outer)

	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 10)
	outer.add_child(vbox)

	# ── Header row ──
	var hdr_row := HBoxContainer.new()
	hdr_row.add_theme_constant_override("separation", 12)
	vbox.add_child(hdr_row)

	var hdr := Label.new()
	hdr.text = _t("panel_title")
	hdr.add_theme_color_override("font_color", Color(0.4, 0.8, 1.0))
	hdr.add_theme_font_size_override("font_size", 15)
	hdr.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hdr_row.add_child(hdr)

	# Language selector
	var lang_box := HBoxContainer.new()
	lang_box.add_theme_constant_override("separation", 5)
	var lang_lbl := Label.new()
	lang_lbl.text = "🌐"
	lang_box.add_child(lang_lbl)
	var lang_opt := OptionButton.new()
	lang_opt.add_item("English", 0)
	lang_opt.add_item("Español", 1)
	lang_opt.add_item("Português (BR)", 2)
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
	lang_box.add_child(lang_opt)
	hdr_row.add_child(lang_box)

	# Version label
	var ver := Label.new()
	ver.text = _t("installed") % _get_local_version()
	ver.add_theme_color_override("font_color", Color(0.5, 0.5, 0.5))
	ver.add_theme_font_size_override("font_size", 11)
	vbox.add_child(ver)

	vbox.add_child(HSeparator.new())

	# ── AI autocomplete ──
	vbox.add_child(_make_row(_t("ai_title"), _t("ai_desc"),
		"godot_luau/ai_autocomplete_enabled", false))
	vbox.add_child(HSeparator.new())

	# ── Instant autocomplete ──
	vbox.add_child(_make_row(_t("speed_title"), _t("speed_desc"),
		"godot_luau/instant_autocomplete", true,
		func(on: bool) -> void:
			if on: _apply_autocomplete_speed()
	))
	vbox.add_child(HSeparator.new())

	# ── Custom autocomplete ──
	var cac_hdr := Label.new()
	cac_hdr.text = _t("cac_header")
	cac_hdr.add_theme_font_size_override("font_size", 13)
	cac_hdr.add_theme_color_override("font_color", Color(0.75, 0.95, 0.55))
	vbox.add_child(cac_hdr)

	vbox.add_child(_make_row(_t("cac_toggle_title"), _t("cac_toggle_desc"),
		"godot_luau/custom_autocomplete_enabled", false))

	# URL row
	var url_lbl := Label.new()
	url_lbl.text = _t("cac_url_label")
	url_lbl.add_theme_font_size_override("font_size", 11)
	url_lbl.add_theme_color_override("font_color", Color(0.65, 0.65, 0.65))
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

	# Import / clear row
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
	_cac_status.add_theme_font_size_override("font_size", 11)
	vbox.add_child(_cac_status)
	_refresh_cac_status()

	vbox.add_child(HSeparator.new())

	# ── Data sharing ──
	vbox.add_child(_make_row(_t("share_title"), _t("share_desc"),
		"godot_luau/share_data_enabled", true))
	vbox.add_child(HSeparator.new())

	# ── Debug mode ──
	vbox.add_child(_make_row(_t("debug_title"), _t("debug_desc"),
		"godot_luau/debug_mode", false))
	vbox.add_child(HSeparator.new())

	# ── Usage stats ──
	var stats_hdr := Label.new()
	stats_hdr.text = _t("stats_header")
	stats_hdr.add_theme_font_size_override("font_size", 13)
	vbox.add_child(stats_hdr)

	_stats_label = Label.new()
	_stats_label.add_theme_color_override("font_color", Color(0.65, 0.65, 0.65))
	_stats_label.add_theme_font_size_override("font_size", 11)
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

	vbox.add_child(HSeparator.new())

	# ── Updates ──
	var upd_hdr := Label.new()
	upd_hdr.text = _t("upd_header")
	upd_hdr.add_theme_font_size_override("font_size", 13)
	vbox.add_child(upd_hdr)

	var upd_row := HBoxContainer.new()
	upd_row.add_theme_constant_override("separation", 8)
	vbox.add_child(upd_row)

	var btn_chk := Button.new()
	btn_chk.text = _t("btn_check")
	btn_chk.pressed.connect(_check_for_update)
	upd_row.add_child(btn_chk)

	# ── Footer note ──
	var note := RichTextLabel.new()
	note.bbcode_enabled  = true
	note.fit_content     = true
	note.scroll_active   = false
	note.text = _t("footer")
	vbox.add_child(note)

	_settings_panel = scroll
	add_control_to_bottom_panel(scroll, "GodotLuau AI")

func _rebuild_panel() -> void:
	if _settings_panel and is_instance_valid(_settings_panel):
		remove_control_from_bottom_panel(_settings_panel)
		_settings_panel.queue_free()
	_stats_label  = null
	_cac_status   = null
	_cac_url_field = null
	_build_settings_panel()

func _make_row(title: String, desc: String, key: String, default_val: bool,
			   on_change: Callable = Callable()) -> Control:
	var vb := VBoxContainer.new()
	vb.add_theme_constant_override("separation", 4)

	var hb := HBoxContainer.new()
	hb.add_theme_constant_override("separation", 10)

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
	lbl.add_theme_font_size_override("font_size", 13)
	hb.add_child(lbl)
	vb.add_child(hb)

	var dlbl := Label.new()
	dlbl.text = desc
	dlbl.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	dlbl.add_theme_color_override("font_color", Color(0.65, 0.65, 0.65))
	dlbl.add_theme_font_size_override("font_size", 11)
	vb.add_child(dlbl)

	return vb

# ── Custom autocomplete ───────────────────────────────────────────────────────

func _refresh_cac_status() -> void:
	if not _cac_status or not is_instance_valid(_cac_status): return
	var path := OS.get_user_data_dir() + "/godotluau_custom_autocomplete.json"
	if not FileAccess.file_exists(path):
		_cac_status.text = _t("cac_status_none")
		_cac_status.add_theme_color_override("font_color", Color(0.55, 0.55, 0.55))
		return
	var f := FileAccess.open(path, FileAccess.READ)
	if not f:
		_cac_status.text = _t("cac_status_none")
		return
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
		_http_autocomplete.queue_free()
		_http_autocomplete = null
		if _cac_status and is_instance_valid(_cac_status):
			_cac_status.text = _t("cac_status_err")
			_cac_status.add_theme_color_override("font_color", Color(1.0, 0.5, 0.5))

func _on_custom_ac_downloaded(result: int, code: int, _hdrs: PackedStringArray, body: PackedByteArray) -> void:
	if _http_autocomplete and is_instance_valid(_http_autocomplete):
		_http_autocomplete.queue_free()
		_http_autocomplete = null
	if result != HTTPRequest.RESULT_SUCCESS or code != 200:
		if _cac_status and is_instance_valid(_cac_status):
			_cac_status.text = _t("cac_status_err")
			_cac_status.add_theme_color_override("font_color", Color(1.0, 0.5, 0.5))
		return
	_save_custom_ac(body.get_string_from_utf8())

func _import_custom_ac_file() -> void:
	var dlg := FileDialog.new()
	dlg.title  = _t("cac_btn_import")
	dlg.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	dlg.access   = FileDialog.ACCESS_FILESYSTEM
	dlg.filters  = ["*.json ; JSON files"]
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
	if not FileAccess.file_exists(path):
		_stats_label.text = _t("stats_none"); return
	var f := FileAccess.open(path, FileAccess.READ)
	if not f:
		_stats_label.text = _t("stats_err"); return
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
	_set_bar(_t("bar_data_cleared"), "", Color(0.5, 0.9, 0.5))

# ── Notification bar ──────────────────────────────────────────────────────────

func _build_notification_bar() -> void:
	_bar = PanelContainer.new()
	_bar.visible = false

	var hb := HBoxContainer.new()
	hb.add_theme_constant_override("separation", 8)
	_bar.add_child(hb)

	var brand := Label.new()
	brand.text = "GodotLuau"
	brand.add_theme_color_override("font_color", Color(0.4, 0.8, 1.0))
	hb.add_child(brand)

	_bar_label = Label.new()
	_bar_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hb.add_child(_bar_label)

	_bar_btn = Button.new()
	_bar_btn.flat = true
	_bar_btn.pressed.connect(_on_action_pressed)
	hb.add_child(_bar_btn)

	var x_btn := Button.new()
	x_btn.flat = true
	x_btn.text = "✕"
	x_btn.pressed.connect(func(): _bar.visible = false)
	hb.add_child(x_btn)

	add_control_to_bottom_panel(_bar, "GodotLuau")

func _set_bar(msg: String, btn_text: String, color: Color, action: String = "") -> void:
	if not (_bar and is_instance_valid(_bar)): return
	_bar.visible = true
	_bar_label.text = msg
	_bar_label.add_theme_color_override("font_color", color)
	_bar_btn.visible = not btn_text.is_empty()
	_bar_action = action
	if not btn_text.is_empty(): _bar_btn.text = btn_text

func _on_action_pressed() -> void:
	match _bar_action:
		"restart": _do_editor_restart()
		"apply":   _apply_windows_update_legacy()
		_:         if not _downloading: _start_download()

# ── Version check ─────────────────────────────────────────────────────────────

func _check_for_update() -> void:
	if _http_version and is_instance_valid(_http_version): return
	_http_version = HTTPRequest.new()
	_http_version.timeout = 10.0
	add_child(_http_version)
	_http_version.request_completed.connect(_on_version_received)
	_set_bar(_t("bar_checking"), "", Color(0.5, 0.8, 1.0))
	if _http_version.request(VERSION_URL) != OK:
		_http_version.queue_free(); _http_version = null
		_bar.visible = false

func _on_version_received(result: int, code: int, _hdrs: PackedStringArray, body: PackedByteArray) -> void:
	if _http_version and is_instance_valid(_http_version):
		_http_version.queue_free(); _http_version = null
	if result != HTTPRequest.RESULT_SUCCESS or code != 200:
		_bar.visible = false; return
	_remote_version = body.get_string_from_utf8().strip_edges()
	var local := _get_local_version()
	if _remote_version == local:
		_set_bar(_t("bar_uptodate") % local, "", Color(0.4, 0.9, 0.5))
		await get_tree().create_timer(4.0).timeout
		if _bar and is_instance_valid(_bar): _bar.visible = false
		return
	_set_bar(_t("bar_newver") % [_remote_version, local],
		_t("bar_btn_update"), Color(1.0, 0.9, 0.3), "download")

func _get_local_version() -> String:
	if not FileAccess.file_exists(VERSION_FILE): return _t("ver_unknown")
	var f := FileAccess.open(VERSION_FILE, FileAccess.READ)
	return f.get_as_text().strip_edges() if f else _t("ver_unknown")

# ── Download ──────────────────────────────────────────────────────────────────

func _start_download() -> void:
	_downloading = true
	_set_bar(_t("bar_downloading") % _remote_version, "", Color(0.4, 0.8, 1.0))
	_bar_btn.visible = false
	_http_download = HTTPRequest.new()
	_http_download.use_threads   = true
	_http_download.download_file = OS.get_user_data_dir() + "/godotluau_update.zip"
	add_child(_http_download)
	_http_download.request_completed.connect(_on_download_completed)
	if _http_download.request(ZIP_URL) != OK:
		_set_bar(_t("bar_dl_err"), _t("bar_retry"), Color(1.0, 0.4, 0.4), "download")
		_downloading = false
		_http_download.queue_free(); _http_download = null

func _on_download_completed(result: int, code: int, _hdrs: PackedStringArray, _body: PackedByteArray) -> void:
	if _http_download and is_instance_valid(_http_download):
		_http_download.queue_free(); _http_download = null
	_downloading = false
	if result != HTTPRequest.RESULT_SUCCESS or (code != 200 and code != 0):
		_set_bar(_t("bar_dl_failed") % code, _t("bar_retry"), Color(1.0, 0.4, 0.4), "download")
		return
	_set_bar(_t("bar_extracting"), "", Color(0.4, 0.8, 1.0))
	_bar_btn.visible = false
	_apply_update.call_deferred()

# ── Apply update ──────────────────────────────────────────────────────────────

var _windows_staged_dlls : Array[Dictionary] = []

func _apply_update() -> void:
	var zip_path  := OS.get_user_data_dir() + "/godotluau_update.zip"
	var proj_path := ProjectSettings.globalize_path("res://")
	var zip := ZIPReader.new()
	if zip.open(zip_path) != OK:
		_set_bar(_t("bar_dl_failed") % 0, _t("bar_retry"), Color(1.0, 0.4, 0.4), "download")
		return

	var staged   : Array[Dictionary] = []
	var staging  := OS.get_user_data_dir() + "/godotluau_staging/"
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

	if staged.is_empty():
		_set_bar(_t("bar_ok_no_restart") % _remote_version, "", Color(0.3, 0.9, 0.5))
		_refresh_stats(); return

	_windows_staged_dlls = staged
	if is_win and _try_rename_and_replace():
		_set_bar(_t("bar_ok_restart"), _t("bar_restart"), Color(0.4, 0.9, 1.0), "restart")
	else:
		_set_bar(_t("bar_apply_close"), "Apply & Close", Color(1.0, 0.7, 0.2), "apply")

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
	_set_bar(_t("bar_restarting"), "", Color(0.4, 0.8, 1.0))
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
	if not pf:
		_set_bar(_t("bar_script_err"), "", Color(1.0, 0.4, 0.4)); return
	pf.store_string("\r\n".join(lines)); pf.close()

	OS.create_process("powershell.exe", [
		"-WindowStyle", "Hidden", "-NonInteractive",
		"-ExecutionPolicy", "Bypass", "-File", ps_path.replace("/", "\\")
	])
	await get_tree().create_timer(0.3).timeout
	get_tree().quit()
