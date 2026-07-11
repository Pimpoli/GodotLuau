@tool
extends EditorPlugin

const VERSION_URL    := "https://raw.githubusercontent.com/Pimpoli/GodotLuau/main/Version"
const ZIP_URL        := "https://github.com/Pimpoli/GodotLuau/raw/main/GodotLuau.zip"
const HASH_URL       := "https://github.com/Pimpoli/GodotLuau/raw/main/GodotLuau.zip.sha256"
const VERSION_FILE   := "res://Version"
const GITHUB_URL     := "https://github.com/Pimpoli/GodotLuau"
const DATA_FILE      := "user://godotluau_usage.json"
const CUSTOM_AC_FILE := "user://godotluau_custom_autocomplete.json"
const DLL_EXTENSIONS := ["dll", "so", "dylib", "framework"]
const WM_META_KEY    := "_godotluau_wm"
const TRASH_DIR      := "res://.luau_trash"
const BACKUP_DIR     := "res://.luau_backup"   # respaldo de la version anterior (rollback)
const SCRIPT_NODE_CLASSES := ["LocalScript", "ServerScript", "ModuleScript"]

# ── Catálogo de modelos de IA para el autocompletado ─────────────────────────
# params: nº de pesos del modelo — más parámetros = mejor predicción,
# pero puede tardar un poco más. Familia LuauIA (cada nivel ≈ x2.5).
const AI_MODELS := [
	{"id": "mini",    "name": "LuauIA-Mini",    "params": "10.000",
	 "url": "https://github.com/Pimpoli/GodotLuau/raw/main/models/LuauIA-Mini.json"},
	{"id": "medium",  "name": "LuauIA-Medium",  "params": "25.000",
	 "url": "https://github.com/Pimpoli/GodotLuau/raw/main/models/LuauIA-Medium.json"},
	{"id": "high",    "name": "LuauIA-High",    "params": "62.500",
	 "url": "https://github.com/Pimpoli/GodotLuau/raw/main/models/LuauIA-High.json"},
	{"id": "highpro", "name": "LuauIA-HighPRO", "params": "~96.000",
	 "url": "https://github.com/Pimpoli/GodotLuau/raw/main/models/LuauIA-HighPRO.json"},
	{"id": "custom",  "name": "Personalizado / Custom", "params": "?", "custom": true},
]

# ── Translation dictionary — EN / ES / PT-BR ─────────────────────────────────
const TR := {
	"en": {
		"panel_title":           "GodotLuau Config",
		"sec_autocomplete":      "🤖  Autocomplete",
		"sec_data":              "📊  Usage Data",
		"sec_debug":             "🔧  Debug",
		"sec_updates":           "🔄  Updates",
		"sec_appearance":        "🎨  Appearance",
		"font_size_label":       "Panel text size",
		"ai_title":              "AI Autocomplete",
		"ai_desc":               "Predicts your next code using the AI model selected below.\nBigger models predict better but may take longer.\nOnly one of AI / Instant Autocomplete can be active.",
		"aim_select_label":      "AI model:",
		"aim_params_suffix":     "parameters",
		"aim_btn_dl":            "Download ⏬",
		"aim_builtin_note":      "Built-in — always ready, instant.",
		"aim_missing":           "Not downloaded yet — press Download ⏬",
		"aim_need_download":     "⚠ Download the selected AI model first",
		"aim_header":            "Custom AI model",
		"aim_note":              "Load your own model (JSON with \"bigrams\") from a file or URL.",
		"aim_status_none":       "Using built-in model \"LuauIA-Mini\".",
		"aim_status_ok":         "✅ Custom model loaded (%d entries).",
		"aim_status_err":        "❌ Download failed — check the URL.",
		"aim_status_bad":        "❌ Invalid model (expected JSON with \"bigrams\").",
		"share_title":           "Share anonymous usage data",
		"share_desc":            "Collects anonymous LOCAL statistics about which APIs you use most\nto improve suggestion ranking. Your code is never stored or shared.\nEnabled by default — you can turn it off anytime.",
		"debug_title":           "Script output (print / warn)",
		"debug_desc":            "Shows print() and warn() messages from YOUR Luau scripts in the Output panel.\nEnabled by default — turn OFF for a clean Output.",
		"dbg_title":             "Debug Mode (system scripts)",
		"anim_title":            "Test character animation",
		"anim_desc":             "TEST ONLY: plays a placeholder code animation (idle/walk bob) on the default character. Off by default; real animations come from rigged models.",
		"dbg_desc":              "Shows internal output from built-in scripts (PlayerModule, Health, Chat...)\nand engine messages. Only the GodotLuau banner is shown when OFF.",
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
		"bar_restart_in":        "✅ Updated — restarting in %d s (Godot will reopen by itself)",
		"bar_apply_close":       "✅ Ready. Godot will close and apply the DLL.",
		"btn_rollback":          "↺ Back to %s",
		"bar_rollback_done":     "✅ Restored %s. Restart to apply.",
		"bar_restart":           "🔄 Restart editor",
		"bar_restarting":        "🔄 Restarting...",
		"bar_data_cleared":      "🗑 Data deleted.",
		"bar_dl_err":            "❌ Could not start download.",
		"bar_retry":             "Retry",
		"bar_script_err":        "❌ Could not write update script.",
		"bar_verifying":         "🔒 Verifying download...",
		"bar_hash_err":          "❌ Integrity check failed — update aborted.",
		"ver_unknown":           "unknown",
		"btn_check_ver":         "🔍",
		"btn_reinstall":         "↺ Reinstall",
		"bar_reinstalling":      "↺ Reinstalling %s...",
		"github_label":          "Official source:",
		"footer_data":           "Data file: user://godotluau_usage.json",
		"warn_watermark":        "⚠ GodotLuau core not detected (DLL missing or failed to load). If you compiled it yourself, ignore this. Otherwise reinstall from: " + GITHUB_URL,
	},
	"es": {
		"panel_title":           "GodotLuau Config",
		"sec_autocomplete":      "🤖  Autocompletado",
		"sec_data":              "📊  Datos de Uso",
		"sec_debug":             "🔧  Debug",
		"sec_updates":           "🔄  Actualizaciones",
		"sec_appearance":        "🎨  Apariencia",
		"font_size_label":       "Tamaño del texto del panel",
		"ai_title":              "Autocompletado con IA",
		"ai_desc":               "Predice tu siguiente código usando el modelo de IA elegido abajo.\nLos modelos más grandes predicen mejor pero pueden tardar más.\nSolo uno de IA / Autocompletado Instantáneo puede estar activo.",
		"aim_select_label":      "Modelo de IA:",
		"aim_params_suffix":     "parámetros",
		"aim_btn_dl":            "Descargar ⏬",
		"aim_builtin_note":      "Integrado — siempre listo, instantáneo.",
		"aim_missing":           "Aún no descargado — pulsa Descargar ⏬",
		"aim_need_download":     "⚠ Descarga primero el modelo de IA seleccionado",
		"aim_header":            "Modelo de IA personalizado",
		"aim_note":              "Carga tu propio modelo (JSON con \"bigrams\") desde archivo o URL.",
		"aim_status_none":       "Usando el modelo integrado \"LuauIA-Mini\".",
		"aim_status_ok":         "✅ Modelo personalizado cargado (%d entradas).",
		"aim_status_err":        "❌ Descarga fallida — verifica la URL.",
		"aim_status_bad":        "❌ Modelo inválido (se esperaba JSON con \"bigrams\").",
		"share_title":           "Compartir datos de uso anónimos",
		"share_desc":            "Recolecta estadísticas anónimas LOCALES de qué APIs usas más\npara mejorar el orden de las sugerencias. Tu código nunca se guarda ni se comparte.\nActivado por defecto — puedes desactivarlo cuando quieras.",
		"debug_title":           "Salida de scripts (print / warn)",
		"debug_desc":            "Muestra los print() y warn() de TUS scripts Luau en el panel Output.\nActivado por defecto — desactívalo para mantener el Output limpio.",
		"dbg_title":             "Modo Debug (scripts del sistema)",
		"anim_title":            "Animación de prueba del personaje",
		"anim_desc":             "SOLO PRUEBA: reproduce una animación por código (rebote al estar quieto/caminar) en el personaje por defecto. Apagado por defecto; las animaciones reales vienen de modelos riggeados.",
		"dbg_desc":              "Muestra la salida interna de los scripts integrados (PlayerModule, Health, Chat...)\ny mensajes del motor. Desactivado solo se ve el banner de GodotLuau.",
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
		"bar_restart_in":        "✅ Actualizado — reiniciando en %d s (Godot se reabrirá solo)",
		"bar_apply_close":       "✅ Listo. Godot se cerrará y aplicará la DLL.",
		"btn_rollback":          "↺ Volver a %s",
		"bar_rollback_done":     "✅ Restaurado %s. Reinicia para aplicar.",
		"bar_restart":           "🔄 Reiniciar editor",
		"bar_restarting":        "🔄 Reiniciando...",
		"bar_data_cleared":      "🗑 Datos eliminados.",
		"bar_dl_err":            "❌ No se pudo iniciar la descarga.",
		"bar_retry":             "Reintentar",
		"bar_script_err":        "❌ No se pudo escribir el script de actualización.",
		"bar_verifying":         "🔒 Verificando descarga...",
		"bar_hash_err":          "❌ Falló la verificación de integridad — actualización cancelada.",
		"ver_unknown":           "desconocida",
		"btn_check_ver":         "🔍",
		"btn_reinstall":         "↺ Reinstalar",
		"bar_reinstalling":      "↺ Reinstalando %s...",
		"github_label":          "Fuente oficial:",
		"footer_data":           "Archivo de datos: user://godotluau_usage.json",
		"warn_watermark":        "⚠ Núcleo de GodotLuau no detectado (DLL ausente o no cargó). Si lo compilaste tú mismo, ignora esto. Si no, reinstala desde: " + GITHUB_URL,
	},
	"pt": {
		"panel_title":           "GodotLuau Config",
		"sec_autocomplete":      "🤖  Autocompletar",
		"sec_data":              "📊  Dados de Uso",
		"sec_debug":             "🔧  Debug",
		"sec_updates":           "🔄  Atualizações",
		"sec_appearance":        "🎨  Aparência",
		"font_size_label":       "Tamanho do texto do painel",
		"ai_title":              "Autocompletar com IA",
		"ai_desc":               "Prevê seu próximo código usando o modelo de IA escolhido abaixo.\nModelos maiores preveem melhor mas podem demorar mais.\nApenas um de IA / Autocompletar Instantâneo pode estar ativo.",
		"aim_select_label":      "Modelo de IA:",
		"aim_params_suffix":     "parâmetros",
		"aim_btn_dl":            "Baixar ⏬",
		"aim_builtin_note":      "Integrado — sempre pronto, instantâneo.",
		"aim_missing":           "Ainda não baixado — clique em Baixar ⏬",
		"aim_need_download":     "⚠ Baixe primeiro o modelo de IA selecionado",
		"aim_header":            "Modelo de IA personalizado",
		"aim_note":              "Carregue seu próprio modelo (JSON com \"bigrams\") de um arquivo ou URL.",
		"aim_status_none":       "Usando o modelo integrado \"LuauIA-Mini\".",
		"aim_status_ok":         "✅ Modelo personalizado carregado (%d entradas).",
		"aim_status_err":        "❌ Download falhou — verifique a URL.",
		"aim_status_bad":        "❌ Modelo inválido (era esperado JSON com \"bigrams\").",
		"share_title":           "Compartilhar dados de uso anônimos",
		"share_desc":            "Coleta estatísticas anônimas LOCAIS de quais APIs você mais usa\npara melhorar a ordem das sugestões. Seu código nunca é salvo ou compartilhado.\nAtivado por padrão — você pode desativar quando quiser.",
		"debug_title":           "Saída de scripts (print / warn)",
		"debug_desc":            "Exibe os print() e warn() dos SEUS scripts Luau no painel Output.\nAtivado por padrão — desative para manter o Output limpo.",
		"dbg_title":             "Modo Debug (scripts do sistema)",
		"anim_title":            "Animação de teste do personagem",
		"anim_desc":             "SÓ TESTE: reproduz uma animação por código (quicar parado/andando) no personagem padrão. Desligado por padrão; as animações reais vêm de modelos com esqueleto.",
		"dbg_desc":              "Exibe a saída interna dos scripts integrados (PlayerModule, Health, Chat...)\ne mensagens do motor. Desativado, apenas o banner do GodotLuau aparece.",
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
		"bar_restart_in":        "✅ Atualizado — reiniciando em %d s (o Godot reabrirá sozinho)",
		"bar_apply_close":       "✅ Pronto. O Godot fechará e aplicará a DLL.",
		"btn_rollback":          "↺ Voltar para %s",
		"bar_rollback_done":     "✅ Restaurado %s. Reinicie para aplicar.",
		"bar_restart":           "🔄 Reiniciar editor",
		"bar_restarting":        "🔄 Reiniciando...",
		"bar_data_cleared":      "🗑 Dados excluídos.",
		"bar_dl_err":            "❌ Não foi possível iniciar o download.",
		"bar_retry":             "Tentar novamente",
		"bar_script_err":        "❌ Não foi possível escrever o script de atualização.",
		"bar_verifying":         "🔒 Verificando download...",
		"bar_hash_err":          "❌ Falha na verificação de integridade — atualização cancelada.",
		"ver_unknown":           "desconhecida",
		"btn_check_ver":         "🔍",
		"btn_reinstall":         "↺ Reinstalar",
		"bar_reinstalling":      "↺ Reinstalando %s...",
		"github_label":          "Fonte oficial:",
		"footer_data":           "Arquivo de dados: user://godotluau_usage.json",
		"warn_watermark":        "⚠ Núcleo do GodotLuau não detectado (DLL ausente ou não carregou). Se você mesmo compilou, ignore isto. Caso contrário, reinstale em: " + GITHUB_URL,
	}
}

# ── State ─────────────────────────────────────────────────────────────────────
var _http_version      : HTTPRequest    = null
var _http_download     : HTTPRequest    = null
var _http_autocomplete : HTTPRequest    = null
var _http_hash         : HTTPRequest    = null
var _http_aimodel      : HTTPRequest    = null
var _bar_action        : String         = ""
var _remote_version    : String         = ""
var _downloading       : bool           = false
var _settings_panel    : Control        = null
var _stats_label       : Label          = null
var _cac_status        : Label          = null
var _cac_url_field     : LineEdit       = null
var _aim_status        : Label          = null
var _aim_url_field     : LineEdit       = null
var _lang              : String         = "en"
var _font_scale        : float          = 1.0
var _font_pct          : int            = 50
var _first_build       : bool           = true
var _ver_label         : Label          = null
var _ver_btn           : Button         = null
var _reinstall_btn     : Button         = null
var _rollback_btn      : Button         = null
var _notif_bar         : PanelContainer = null
var _notif_label       : Label          = null
var _outdated_timer    : Timer          = null
var _suppress_trash_frame : int         = 0

# Colores adaptados al tema del editor (contraste correcto en claro y oscuro)
var _col_text  := Color(0.95, 0.95, 0.95)
var _col_dim   := Color(0.74, 0.74, 0.76)
var _col_faint := Color(0.55, 0.55, 0.58)
var _col_card  := Color(0.12, 0.14, 0.18, 0.9)
var _is_dark_theme := true

# ── Lifecycle ─────────────────────────────────────────────────────────────────

func _enter_tree() -> void:
	_detect_editor_lang()
	_compute_theme_colors()
	_load_font_pref()
	_sync_plugin_cfg()
	_cleanup_old_dlls()
	_register_settings()
	_apply_autocomplete_speed()
	_build_settings_panel()
	_check_watermark()
	_check_for_update()
	_connect_script_lifecycle()
	_purge_old_trash()
	_build_mp_toolbar()
	_setup_luau_highlighting()

# ── Colores de sintaxis Luau (como Roblox Studio, tema oscuro) ────────────────
# El editor abre los .lua con texto plano; aquí se le engancha un CodeHighlighter
# con los colores clásicos de Studio a cada editor de script Luau que se abra.
var _hl_seen: Array = []

func _make_luau_highlighter() -> CodeHighlighter:
	var ch := CodeHighlighter.new()
	var kw := Color("#f86d7c")      # palabras clave (local, function, if...)
	var builtin := Color("#84d6f7") # game, workspace, print, task...
	ch.number_color = Color("#ffc600")
	ch.symbol_color = Color("#cccccc")
	ch.function_color = Color("#fdfbac")
	ch.member_variable_color = Color("#9cdcfe")
	for w in ["and","break","do","else","elseif","end","for","function","if","in",
			  "local","not","or","repeat","return","then","until","while","continue",
			  "type","export","self"]:
		ch.add_keyword_color(w, kw)
	for w in ["true","false","nil"]:
		ch.add_keyword_color(w, Color("#ffc600"))
	for w in ["game","workspace","script","print","warn","wait","task","spawn","delay",
			  "require","pairs","ipairs","next","select","tostring","tonumber","typeof",
			  "pcall","xpcall","error","assert","unpack","math","string","table","os",
			  "coroutine","bit32","utf8","Instance","Vector2","Vector3","CFrame","Color3",
			  "UDim","UDim2","Enum","Ray","Region3","TweenInfo","NumberRange","Random"]:
		ch.add_keyword_color(w, builtin)
	# Regiones: comentario de bloque ANTES que el de línea (comparten prefijo)
	ch.add_color_region("--[[", "]]", Color("#666666"), false)
	ch.add_color_region("--", "", Color("#666666"), true)
	ch.add_color_region("\"", "\"", Color("#adf195"), false)
	ch.add_color_region("'", "'", Color("#adf195"), false)
	return ch

func _setup_luau_highlighting() -> void:
	var se := EditorInterface.get_script_editor()
	if se == null: return
	if not se.editor_script_changed.is_connected(_apply_luau_colors_everywhere):
		se.editor_script_changed.connect(func(_s): _apply_luau_colors_everywhere())
	_apply_luau_colors_everywhere()

func _apply_luau_colors_everywhere() -> void:
	var se := EditorInterface.get_script_editor()
	if se == null: return
	var editors := se.get_open_script_editors()
	var scripts := se.get_open_scripts()
	for i in editors.size():
		var ed = editors[i]
		var base = ed.get_base_editor()
		if base == null or not (base is CodeEdit): continue
		if base in _hl_seen: continue
		# Los arrays de editores y scripts van en el mismo orden de pestañas
		var is_luau := false
		if i < scripts.size() and scripts[i] != null:
			var path := String(scripts[i].resource_path)
			is_luau = path.ends_with(".lua") or scripts[i].get_class() == "LuauScript"
		if is_luau:
			base.syntax_highlighter = _make_luau_highlighter()
			_hl_seen.append(base)

func _exit_tree() -> void:
	_disconnect_script_lifecycle()
	for n in [_http_version, _http_download, _http_autocomplete, _http_hash, _http_aimodel, _outdated_timer]:
		if n and is_instance_valid(n): n.queue_free()
	if _settings_panel and is_instance_valid(_settings_panel):
		remove_control_from_bottom_panel(_settings_panel)
		_settings_panel.queue_free()
	# Cerrar ventanas cliente y quitar nuestros run-args si el editor se cierra o
	# se desactiva el plugin (si quedaran, TODOS los Play futuros serían host).
	_kill_mp_children()
	var base := _strip_mp_args(str(ProjectSettings.get_setting("editor/run/main_run_args", "")))
	if str(ProjectSettings.get_setting("editor/run/main_run_args", "")) != base:
		ProjectSettings.set_setting("editor/run/main_run_args", base)
		ProjectSettings.save()
	_remove_res_file(".gl_mp_session")
	_remove_res_file(".gl_view_cmd")
	if _view_btn and is_instance_valid(_view_btn):
		_view_btn.queue_free()
		_view_btn = null
	_remove_mp_toolbar()

# ── Local multiplayer controls next to the NATIVE Play button ────────────────
# A "Players" SpinBox (1-8) + a PC/Mobile/Console/VR device selector, placed to
# the LEFT of Godot's own Play button (no custom Play button is created). When
# the user presses the native Play with Players >= 2, extra game windows are
# launched and everyone connects into a shared session (Player1..N) — they see,
# collide and move around each other like Roblox Studio's local test.
var _mp_bar: HBoxContainer = null
var _mp_count: SpinBox = null
var _mp_device: OptionButton = null
var _mp_child_pids: Array[int] = []
var _mp_run_bar: Node = null          # EditorRunBar (para desconectar señales)
var _mp_used_container: bool = false  # true si caímos al fallback CONTAINER_TOOLBAR

func _find_node_by_class(root: Node, cls: String) -> Node:
	if root == null: return null
	if root.get_class() == cls: return root
	for c in root.get_children():
		var r := _find_node_by_class(c, cls)
		if r: return r
	return null

func _build_mp_toolbar() -> void:
	_mp_bar = HBoxContainer.new()
	_mp_bar.add_theme_constant_override("separation", 5)

	_mp_bar.add_child(VSeparator.new())

	# Icono de multijugador (del theme del editor, si existe)
	var base := EditorInterface.get_base_control()
	if base.has_theme_icon("MultiplayerSynchronizer", "EditorIcons"):
		var ico := TextureRect.new()
		ico.texture = base.get_theme_icon("MultiplayerSynchronizer", "EditorIcons")
		ico.stretch_mode = TextureRect.STRETCH_KEEP_CENTERED
		ico.tooltip_text = "GodotLuau local multiplayer"
		_mp_bar.add_child(ico)

	var lbl := Label.new()
	lbl.text = "Players"
	lbl.add_theme_color_override("font_color", base.get_theme_color("font_color", "Editor"))
	_mp_bar.add_child(lbl)

	_mp_count = SpinBox.new()
	_mp_count.min_value = 1
	_mp_count.max_value = 8
	_mp_count.step = 1
	_mp_count.value = 1
	# Un solo digito (max 8): angosto para no ocupar barra de mas
	_mp_count.custom_minimum_size = Vector2(44, 0)
	_mp_count.tooltip_text = "How many players to emulate.\n1 = single player.\n2-8 = pressing Play opens one window per player (Player1..N) that connect to each other automatically, PLUS the Server View window (this game view), like Roblox Studio."
	_mp_bar.add_child(_mp_count)

	_mp_device = OptionButton.new()
	_mp_device.add_item("PC")
	_mp_device.add_item("Mobile")
	_mp_device.add_item("Console")
	_mp_device.add_item("VR")
	_mp_device.selected = 0
	_mp_device.flat = true
	# El boton toma el ancho de la opcion elegida (no el de la mas larga)
	_mp_device.fit_to_longest_item = false
	_mp_device.tooltip_text = "Emulated device (PC by default):\n- Mobile: phone-shaped window + on-screen joystick and jump button (mouse acts as your finger).\n- Console: 16:9 TV window, play with a gamepad.\n- VR: split-screen preview with one view per eye.\nScripts can read it with net:GetDevice()."
	_mp_bar.add_child(_mp_device)

	_mp_bar.add_child(VSeparator.new())

	# Keep the host run-args in sync BEFORE Play is pressed (the native instance
	# is launched by Godot itself, so its args must already be configured).
	_mp_count.value_changed.connect(func(_v): _sync_mp_session())
	_mp_device.item_selected.connect(func(_i): _sync_mp_session())

	# Place the controls to the LEFT of Godot's native Play button and hook its
	# signal — we do NOT create our own Play button.
	var run_bar := _find_node_by_class(EditorInterface.get_base_control(), "EditorRunBar")
	if run_bar == null:
		run_bar = _find_node_by_class(get_tree().get_root(), "EditorRunBar")
	if run_bar and run_bar.get_parent():
		_mp_run_bar = run_bar
		var parent := run_bar.get_parent()
		parent.add_child(_mp_bar)
		parent.move_child(_mp_bar, run_bar.get_index())   # justo antes del Play
		if not run_bar.is_connected("play_pressed", _on_native_play):
			run_bar.connect("play_pressed", _on_native_play)
		if not run_bar.is_connected("stop_pressed", _on_native_stop):
			run_bar.connect("stop_pressed", _on_native_stop)
	else:
		# Fallback: standard plugin toolbar (still next to Play)
		_mp_used_container = true
		add_control_to_container(EditorPlugin.CONTAINER_TOOLBAR, _mp_bar)

	# Botón Server View / Client View en la barra del panel Game del editor
	# (funciona con 1-8 jugadores; cambia la perspectiva de la instancia
	# host/única SIN mover al personaje, como Roblox Studio).
	_build_view_button()

	# Clean slate: reparar los run args rotos de 1.9.5/1.9.6, sincronizar la
	# sesion (default 1 + PC → sin archivo) y limpiar comandos de vista viejos.
	_repair_main_run_args()
	_sync_mp_session()
	_remove_res_file(".gl_view_cmd")
	# Apagar el sistema nativo de multiples instancias para que no duplique ventanas
	_neutralize_native_instances()

# ── Botón de perspectiva Server/Client en la barra del panel Game ────────────
var _view_btn: Button = null
var _view_counter: int = 0

func _build_view_button() -> void:
	_view_btn = Button.new()
	_view_btn.toggle_mode = true
	_view_btn.flat = true
	_view_btn.text = "Server"
	var base := EditorInterface.get_base_control()
	if base.has_theme_icon("Camera3D", "EditorIcons"):
		_view_btn.icon = base.get_theme_icon("Camera3D", "EditorIcons")
	_view_btn.tooltip_text = "Switch this game view between perspectives, like Roblox Studio:\n- Server: free camera — WASD + right mouse to fly, wheel = speed, E/Q up-down. Your player does NOT move.\n- Client: back to the player camera.\nAvailable with 1-8 players. In multiplayer tests this window is always the server."
	_view_btn.toggled.connect(_on_view_btn_toggled)
	# Barra del panel Game (la de Entrada / 2D / 3D...): su primer HBoxContainer
	var game_view := _find_node_by_class(EditorInterface.get_base_control(), "GameView")
	var bar: Node = _find_node_by_class(game_view, "HBoxContainer") if game_view else null
	if bar:
		bar.add_child(VSeparator.new())
		bar.add_child(_view_btn)
	elif _mp_bar:
		# Sin panel Game: junto a nuestra barra de Players
		_mp_bar.add_child(_view_btn)

func _update_view_btn_label(pressed: bool) -> void:
	if _view_btn == null: return
	_view_btn.text = "Client" if pressed else "Server"

func _on_view_btn_toggled(pressed: bool) -> void:
	_view_counter += 1
	_update_view_btn_label(pressed)
	var f := FileAccess.open("res://.gl_view_cmd", FileAccess.WRITE)
	if f:
		f.store_string("%s|%d" % ["server" if pressed else "client", _view_counter])
		f.close()

func _reset_view_button() -> void:
	if _view_btn and is_instance_valid(_view_btn):
		_view_btn.set_pressed_no_signal(false)
		_update_view_btn_label(false)
	_remove_res_file(".gl_view_cmd")

func _remove_mp_toolbar() -> void:
	if _mp_run_bar and is_instance_valid(_mp_run_bar):
		if _mp_run_bar.is_connected("play_pressed", _on_native_play):
			_mp_run_bar.disconnect("play_pressed", _on_native_play)
		if _mp_run_bar.is_connected("stop_pressed", _on_native_stop):
			_mp_run_bar.disconnect("stop_pressed", _on_native_stop)
	_mp_run_bar = null
	if _mp_bar and is_instance_valid(_mp_bar):
		if _mp_used_container:
			remove_control_from_container(EditorPlugin.CONTAINER_TOOLBAR, _mp_bar)
		elif _mp_bar.get_parent():
			_mp_bar.get_parent().remove_child(_mp_bar)
		_mp_bar.queue_free()
		_mp_bar = null

func _mp_device_name() -> String:
	var devices := ["PC", "Mobile", "Console", "VR"]
	return devices[_mp_device.selected] if _mp_device else "PC"

# La sesion de prueba viaja por res://.gl_mp_session ("count|device|stamp").
# res:// es la MISMA carpeta para el editor y para el juego lanzado desde el
# editor, asi que la ventana nativa (que sera la VISTA DEL SERVIDOR) siempre la
# encuentra. NUNCA se tocan los run args nativos: inyectar argumentos en
# "editor/run/main_run_args" impedia que el Play nativo lanzara el juego.
const MP_ARGS_MARKER := "-- --glindex"

func _strip_mp_args(s: String) -> String:
	var i := s.find(MP_ARGS_MARKER)
	if i >= 0:
		s = s.substr(0, i)
	return s.strip_edges()

# REPARACION: versiones 1.9.5/1.9.6 dejaron args nuestros en main_run_args, lo
# que rompia el Play nativo. Se limpian una vez al cargar el plugin.
func _repair_main_run_args() -> void:
	var cur := str(ProjectSettings.get_setting("editor/run/main_run_args", ""))
	var base := _strip_mp_args(cur)
	if cur != base:
		ProjectSettings.set_setting("editor/run/main_run_args", base)
		ProjectSettings.save()
		print("[GodotLuau] Repaired editor/run/main_run_args (removed leftover multiplayer args).")

func _sync_mp_session() -> void:
	var count: int = int(_mp_count.value) if _mp_count else 1
	var device := _mp_device_name()
	if count >= 2 or device != "PC":
		var f := FileAccess.open("res://.gl_mp_session", FileAccess.WRITE)
		if f:
			f.store_string("%d|%s|%d" % [count, device, int(Time.get_unix_time_from_system())])
			f.close()
	else:
		_remove_res_file(".gl_mp_session")

func _remove_res_file(fname: String) -> void:
	var d := DirAccess.open("res://")
	if d and d.file_exists(fname):
		d.remove(fname)

# El dialogo nativo "Customize Run Instances" NO debe pelear con nuestra barra:
# si el usuario lo dejo activado con N instancias, el Play nativo lanzaria N
# ventanas nativas ADEMAS de nuestros clientes (cuentas de ventanas absurdas).
# Se apaga en vivo (widgets) y en su metadata persistida.
func _neutralize_native_instances() -> void:
	var es := EditorInterface.get_editor_settings()
	if es:
		es.set_project_metadata("debug_options", "multiple_instances_enabled", false)
	var dlg := _find_node_by_class(EditorInterface.get_base_control().get_tree().get_root(), "RunInstancesDialog")
	if dlg == null: return
	var cb := _find_node_by_class(dlg, "CheckBox")
	if cb and cb is CheckBox and (cb as CheckBox).button_pressed:
		(cb as CheckBox).button_pressed = false
	var sp := _find_node_by_class(dlg, "SpinBox")
	if sp and sp is SpinBox and (sp as SpinBox).value > 1:
		(sp as SpinBox).value = 1

# Native Play pressed → if Players >= 2, launch ONE window per player
# (glindex 2..N+1 → Player1..PlayerN). The native game window (embedded in the
# editor's Game panel) becomes the SERVER VIEW: it hosts, has no character and
# starts with the free camera — exactly like Roblox Studio's local test.
func _on_native_play() -> void:
	# Always close the windows we launched before (so a previous run's client
	# windows never linger and inflate the count on the next Play).
	_kill_mp_children()
	var count: int = int(_mp_count.value) if _mp_count else 1
	_sync_mp_session()   # sello fresco para la ventana nativa (el server)
	if count < 2:
		return
	var device := _mp_device_name()
	# Ensure the extra windows load the CURRENT project state from disk.
	EditorInterface.save_all_scenes()
	# Run the SAME scene the server runs (F5 main / F6 current / F7 custom) so
	# every window shares one world. Only a real scene path is forwarded.
	var scene := str(EditorInterface.get_playing_scene())
	var pass_scene := scene.begins_with("res://") and (scene.ends_with(".tscn") or scene.ends_with(".scn"))
	# One window per player: glindex 2..N+1 (they connect to 127.0.0.1).
	var exe := OS.get_executable_path()
	var proj := ProjectSettings.globalize_path("res://")
	for i in range(2, count + 2):
		var a: Array = ["--path", proj]
		if pass_scene: a.append(scene)
		a.append_array(["--", "--glindex", str(i), "--glcount", str(count), "--gldevice", device])
		var pid := OS.create_process(exe, a)
		if pid > 0: _mp_child_pids.append(pid)
		else: push_warning("[GodotLuau] Could not launch game window for Player%d." % (i - 1))
	# La ventana nativa (panel Game) arranca en Server View: reflejarlo en el boton
	if _view_btn and is_instance_valid(_view_btn):
		_view_btn.set_pressed_no_signal(true)
		_update_view_btn_label(true)

# Native Stop pressed → close the extra windows and reset the view toggle.
func _on_native_stop() -> void:
	_kill_mp_children()
	_reset_view_button()

# Kill every client window we launched (guarded: PIDs can be dead/recycled).
func _kill_mp_children() -> void:
	for pid in _mp_child_pids:
		if OS.is_process_running(pid):
			OS.kill(pid)
	_mp_child_pids.clear()

# ── Ciclo de vida de scripts: papelera + restauración con Ctrl+Z ─────────────
# Al borrar un nodo LocalScript/ServerScript/ModuleScript en el editor, su .lua
# se mueve a res://.luau_trash/<script_id>.lua. Si el nodo vuelve (Ctrl+Z),
# el archivo se restaura automáticamente con todo su contenido.

func _connect_script_lifecycle() -> void:
	var tree := get_tree()
	if not tree.node_removed.is_connected(_on_any_node_removed):
		tree.node_removed.connect(_on_any_node_removed)
	if not tree.node_added.is_connected(_on_any_node_added):
		tree.node_added.connect(_on_any_node_added)

func _disconnect_script_lifecycle() -> void:
	var tree := get_tree()
	if tree.node_removed.is_connected(_on_any_node_removed):
		tree.node_removed.disconnect(_on_any_node_removed)
	if tree.node_added.is_connected(_on_any_node_added):
		tree.node_added.disconnect(_on_any_node_added)

func _node_script_id(n: Node) -> String:
	var v = n.get("script_id")
	return str(v) if v is String else ""

func _node_script_path(n: Node) -> String:
	var res = n.get("codigo_luau")
	if res is Resource: return (res as Resource).resource_path
	return ""

func _on_any_node_removed(n: Node) -> void:
	# Si se quita la raíz de la escena editada es un cierre/cambio de escena:
	# suprimir el trasheo de todo ese lote de nodos
	if n == EditorInterface.get_edited_scene_root():
		_suppress_trash_frame = Engine.get_process_frames() + 5
		return
	if n.get_class() in SCRIPT_NODE_CLASSES:
		# Se pasa el instance_id (int), no el Node: para cuando corre el deferred el
		# nodo puede estar ya liberado y marshalarlo como Node falla
		# ("Cannot convert argument 1 from Object to Object").
		_check_script_removed.call_deferred(n.get_instance_id(), _node_script_id(n), _node_script_path(n))
	elif n.get_child_count() > 0:
		# Al borrar un PADRE, sus scripts DESCENDIENTES también deben ir a la
		# papelera (antes sus .lua quedaban huérfanos en el proyecto).
		_queue_descendant_scripts(n)

func _queue_descendant_scripts(n: Node) -> void:
	for c in n.get_children():
		if c.get_class() in SCRIPT_NODE_CLASSES:
			_check_script_removed.call_deferred(c.get_instance_id(), _node_script_id(c), _node_script_path(c))
		if c.get_child_count() > 0:
			_queue_descendant_scripts(c)

func _check_script_removed(node_id: int, id: String, path: String) -> void:
	await get_tree().process_frame
	if Engine.get_process_frames() <= _suppress_trash_frame: return
	var n := instance_from_id(node_id) as Node
	if is_instance_valid(n) and n.is_inside_tree(): return   # reparent o undo inmediato
	if EditorInterface.get_edited_scene_root() == null: return
	_trash_script(id, path)

func _trash_script(id: String, path: String) -> void:
	if id.is_empty() or path.is_empty() or not path.begins_with("res://"): return
	if not FileAccess.file_exists(path): return
	# Nodo duplicado: si otro nodo de la escena usa el mismo ID, no tocar el archivo
	var root := EditorInterface.get_edited_scene_root()
	if root and _scene_has_script_id(root, id): return
	var g_trash := ProjectSettings.globalize_path(TRASH_DIR)
	DirAccess.make_dir_recursive_absolute(g_trash)
	var ignore_path := g_trash + "/.gdignore"
	if not FileAccess.file_exists(ignore_path):
		var f := FileAccess.open(ignore_path, FileAccess.WRITE)
		if f: f.store_string("")
	if DirAccess.rename_absolute(ProjectSettings.globalize_path(path), g_trash + "/" + id + ".lua") == OK:
		print("[GodotLuau] 🗑 Script en papelera: %s (Ctrl+Z para restaurar)" % path.get_file())
		EditorInterface.get_resource_filesystem().scan()

func _scene_has_script_id(n: Node, id: String) -> bool:
	if n.get_class() in SCRIPT_NODE_CLASSES and _node_script_id(n) == id:
		return true
	for c in n.get_children():
		if _scene_has_script_id(c, id): return true
	return false

func _on_any_node_added(n: Node) -> void:
	if not (n.get_class() in SCRIPT_NODE_CLASSES): return
	_check_script_restored.call_deferred(n)

func _check_script_restored(n: Node) -> void:
	if not is_instance_valid(n) or not n.is_inside_tree(): return
	var id := _node_script_id(n)
	if id.is_empty(): return
	var path := _node_script_path(n)
	if path.is_empty():
		# Ruta nueva (dentro de GodotLuau/); si el proyecto es viejo y esa
		# carpeta no existe pero la vieja si, usar la vieja.
		path = "res://GodotLuau/%ss/%s.lua" % [n.get_class(), id]
		var old_path := "res://%ss/%s.lua" % [n.get_class(), id]
		if not FileAccess.file_exists(path) and FileAccess.file_exists(old_path):
			path = old_path
	if FileAccess.file_exists(path): return
	var trash_file := ProjectSettings.globalize_path(TRASH_DIR + "/" + id + ".lua")
	if not FileAccess.file_exists(trash_file): return
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(path.get_base_dir()))
	if DirAccess.rename_absolute(trash_file, ProjectSettings.globalize_path(path)) == OK:
		print("[GodotLuau] ♻ Script restaurado: " + path.get_file())
		EditorInterface.get_resource_filesystem().scan()

func _purge_old_trash() -> void:
	# Los scripts llevan más de 7 días en la papelera → limpieza definitiva
	var g_trash := ProjectSettings.globalize_path(TRASH_DIR)
	if not DirAccess.dir_exists_absolute(g_trash): return
	var now := Time.get_unix_time_from_system()
	for f in DirAccess.get_files_at(g_trash):
		if not f.ends_with(".lua"): continue
		var p := g_trash + "/" + f
		if now - FileAccess.get_modified_time(p) > 7 * 86400:
			DirAccess.remove_absolute(p)

# ── Language ──────────────────────────────────────────────────────────────────

func _detect_editor_lang() -> void:
	var es := EditorInterface.get_editor_settings()
	# Preferir el idioma elegido por el usuario en el panel (persistente)
	if es.has_setting("godot_luau/lang"):
		var saved := str(es.get_setting("godot_luau/lang"))
		if saved in ["en", "es", "pt"]:
			_lang = saved
			return
	# Por defecto: ingles (internacional). El usuario puede cambiar a es/pt
	# desde el selector del panel y queda guardado en godot_luau/lang.
	_lang = "en"

func _t(key: String) -> String:
	var d : Dictionary = TR.get(_lang, TR["en"])
	return d.get(key, (TR["en"] as Dictionary).get(key, key))

# ── Colores según el tema del editor ─────────────────────────────────────────
# El texto usa el color contrario al fondo del motor para máxima legibilidad.

func _compute_theme_colors() -> void:
	var base : Color = Color(0.21, 0.24, 0.29)
	var es := EditorInterface.get_editor_settings()
	if es.has_setting("interface/theme/base_color"):
		base = es.get_setting("interface/theme/base_color")
	_is_dark_theme = base.get_luminance() < 0.5
	if _is_dark_theme:
		_col_text  = Color(0.96, 0.96, 0.97)
		_col_dim   = Color(0.76, 0.76, 0.79)
		_col_faint = Color(0.56, 0.56, 0.60)
		_col_card  = Color(0.10, 0.12, 0.16, 0.92)
	else:
		_col_text  = Color(0.09, 0.09, 0.11)
		_col_dim   = Color(0.27, 0.27, 0.31)
		_col_faint = Color(0.42, 0.42, 0.46)
		_col_card  = Color(0.93, 0.94, 0.96, 0.92)

# Acentos: en tema claro se oscurecen para que sigan leyéndose bien
func _accent(c: Color) -> Color:
	return c if _is_dark_theme else c.darkened(0.35)

# ── Font preference (per-user, stored in EditorSettings) ─────────────────────
# Escala 0-100 estilo barra de volumen; 50 = tamaño normal.

func _load_font_pref() -> void:
	var es := EditorInterface.get_editor_settings()
	var pct := 50
	if es.has_setting("godot_luau/panel_font_scale_pct"):
		pct = int(es.get_setting("godot_luau/panel_font_scale_pct"))
	elif es.has_setting("godot_luau/panel_font_scale"):
		# Migración desde el viejo selector S/M/L
		pct = [30, 50, 80][clampi(int(es.get_setting("godot_luau/panel_font_scale")), 0, 2)]
	_apply_font_pct(pct)

func _apply_font_pct(pct: int) -> void:
	_font_pct   = clampi(pct, 0, 100)
	_font_scale = 0.85 + (_font_pct / 100.0) * 0.45   # 0.85x – 1.30x

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
	# Quitar BOM UTF-8 si alguna herramienta externa lo metio (rompe el plugin)
	if lines.size() > 0 and lines[0].begins_with(char(0xFEFF)):
		lines[0] = lines[0].trim_prefix(char(0xFEFF))
		changed = true
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
	# Aviso único — sin timer repetitivo (quien compila su propia DLL puede ignorarlo)
	if Engine.has_meta(WM_META_KEY): return
	push_warning("[GodotLuau] " + _t("warn_watermark"))

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
	# v1.11.2: la migracion se DIFIERE. Borrar carpetas y forzar scan() durante
	# el PRIMER escaneo del editor crasheaba el editor entero (signal 11 en el
	# hilo del escaner). Ahora espera a que el editor este tranquilo.
	if is_inside_tree():
		get_tree().create_timer(3.0).timeout.connect(_migrate_old_layout)
	var bin_path := ProjectSettings.globalize_path("res://GodotLuau/bin/")
	if not DirAccess.dir_exists_absolute(bin_path): return
	var dir := DirAccess.open(bin_path)
	if not dir: return
	dir.list_dir_begin()
	var f := dir.get_next()
	while f != "":
		# Restos del intercambio de DLL (.old / .new) y los temporales que deja
		# Windows al renombrar (~...TMP). El '~godotluau...dll' es la copia viva
		# que usa Godot mientras corre; si esta bloqueada el borrado falla solo,
		# y si quedo huerfana de una sesion anterior se limpia.
		if f.ends_with(".old") or f.ends_with(".new") or f.ends_with(".TMP") \
		or (f.begins_with("~") and f.contains("godot_luau")):
			DirAccess.remove_absolute(bin_path + f)
		f = dir.get_next()

# ── Migracion a la estructura nueva (todo dentro de res://GodotLuau/) ─────────
# Las instalaciones viejas tenian bin/, icons/, DefaultScripts/ y assets/ en la
# raiz. El zip nuevo ya extrae en GodotLuau/; aqui se limpian los restos viejos
# (solo carpetas DE LA EXTENSION, nunca contenido del usuario).
func _migrate_old_layout() -> void:
	if not is_inside_tree():
		return
	if not DirAccess.dir_exists_absolute(ProjectSettings.globalize_path("res://GodotLuau/bin")):
		return   # aun no se instalo la estructura nueva
	# ¿Hay algo viejo que borrar? (si no, salir sin tocar el filesystem)
	var pending := false
	for old in ["res://bin", "res://icons", "res://DefaultScripts", "res://assets/avatars/r6"]:
		if DirAccess.dir_exists_absolute(ProjectSettings.globalize_path(old)):
			pending = true
	if not pending:
		for f in ["RobloxAvatarR6.obj", "RobloxAvatarR6.mtl", "RobloxAvatarR6.obj.import",
				  "RobloxAvatarR15.obj", "RobloxAvatarR15.mtl", "RobloxAvatarR15.obj.import",
				  "Rig1_diff.png", "Rig1_diff.png.import"]:
			if FileAccess.file_exists(ProjectSettings.globalize_path("res://assets/avatars/" + f)):
				pending = true
				break
	if not pending:
		return
	# CRITICO: jamas borrar mientras el escaner del editor esta corriendo
	# (crashea el editor). Si esta escaneando, reintentar en unos segundos.
	var fs := EditorInterface.get_resource_filesystem()
	if fs == null:
		return
	if fs.is_scanning():
		get_tree().create_timer(2.0).timeout.connect(_migrate_old_layout)
		return
	var removed := false
	for old in ["res://bin", "res://icons", "res://DefaultScripts", "res://assets/avatars/r6"]:
		var g := ProjectSettings.globalize_path(old)
		if DirAccess.dir_exists_absolute(g):
			_remove_dir_recursive(g)
			removed = true
	# archivos sueltos del avatar viejo en res://assets/avatars/
	for f in ["RobloxAvatarR6.obj", "RobloxAvatarR6.mtl", "RobloxAvatarR6.obj.import",
			  "RobloxAvatarR15.obj", "RobloxAvatarR15.mtl", "RobloxAvatarR15.obj.import",
			  "Rig1_diff.png", "Rig1_diff.png.import"]:
		var p := ProjectSettings.globalize_path("res://assets/avatars/" + f)
		if FileAccess.file_exists(p):
			DirAccess.remove_absolute(p)
			removed = true
	if removed:
		print("[GodotLuau] Migrated to the new res://GodotLuau/ layout (old folders removed).")
		if not fs.is_scanning():
			fs.scan()

func _remove_dir_recursive(g_path: String) -> void:
	var d := DirAccess.open(g_path)
	if d == null: return
	d.list_dir_begin()
	var f := d.get_next()
	while f != "":
		if f != "." and f != "..":
			var full := g_path.path_join(f)
			if d.current_is_dir(): _remove_dir_recursive(full)
			else: DirAccess.remove_absolute(full)
		f = d.get_next()
	d.list_dir_end()
	DirAccess.remove_absolute(g_path)

# ── Settings registration ─────────────────────────────────────────────────────

func _register_settings() -> void:
	_add_bool("godot_luau/ai_autocomplete_enabled",    false,
		"AI Autocomplete — predicts code with the lightweight LuauIA-Mini model.")
	_add_bool("godot_luau/share_data_enabled",         true,
		"Collect anonymous local usage statistics to improve suggestion ranking.")
	_add_bool("godot_luau/script_output",              true,
		"Show print() and warn() output from your Luau scripts in the Output panel.")
	_add_bool("godot_luau/debug_mode",                 false,
		"Debug Mode — show internal output from built-in system scripts and engine messages.")
	_add_bool("godot_luau/debug_test_animation",       false,
		"TEST: play a placeholder code animation on the default character (idle/walk bob). For testing only, until real rigged animations are added.")
	_add_bool("godot_luau/notify_outdated_version",    true,
		"Print a warning in Output when running an outdated GodotLuau version.")
	_add_bool("godot_luau/instant_autocomplete",       true,
		"Trigger autocomplete suggestions instantly on every keystroke.")
	_add_bool("godot_luau/custom_autocomplete_enabled", false,
		"Use a custom JSON autocomplete list.")
	_add_str("godot_luau/custom_autocomplete_url",     "",
		"URL to download a custom autocomplete JSON from.")
	_add_str("godot_luau/ai_model_url",                "",
		"URL to download a custom AI model JSON from.")
	_add_str("godot_luau/ai_model_selected",           "mini",
		"Selected AI model id for AI Autocomplete (mini, plus, custom...).")

func _add_bool(key: String, val: bool, hint: String) -> void:
	if not ProjectSettings.has_setting(key): ProjectSettings.set_setting(key, val)
	ProjectSettings.set_initial_value(key, val)
	ProjectSettings.add_property_info({"name":key,"type":TYPE_BOOL,"hint":PROPERTY_HINT_NONE,"hint_string":hint})

func _add_str(key: String, val: String, hint: String) -> void:
	if not ProjectSettings.has_setting(key): ProjectSettings.set_setting(key, val)
	ProjectSettings.set_initial_value(key, val)
	ProjectSettings.add_property_info({"name":key,"type":TYPE_STRING,"hint":PROPERTY_HINT_NONE,"hint_string":hint})

# ── UI helpers ────────────────────────────────────────────────────────────────

# Zona tipo tarjeta: fondo redondeado + borde de acento a la izquierda.
# Devuelve [tarjeta, contenedor_interno] — añade los controles al segundo.
func _make_zone(title: String, accent: Color) -> Array:
	var card := PanelContainer.new()
	var sb := StyleBoxFlat.new()
	sb.bg_color = _col_card
	sb.set_corner_radius_all(8)
	sb.set_content_margin_all(12)
	sb.border_width_left = 3
	sb.border_color = _accent(accent)
	card.add_theme_stylebox_override("panel", sb)
	var vb := VBoxContainer.new()
	vb.add_theme_constant_override("separation", 6)
	card.add_child(vb)
	var lbl := Label.new()
	lbl.text = title
	lbl.add_theme_font_size_override("font_size", _fs(14))
	lbl.add_theme_color_override("font_color", _accent(accent))
	vb.add_child(lbl)
	return [card, vb]

# Fade-in escalonado de las zonas al construir el panel
func _animate_zones(zones: Array) -> void:
	for i in zones.size():
		var z : Control = zones[i]
		z.modulate.a = 0.0
		var tw := z.create_tween()
		tw.tween_interval(0.05 * i)
		tw.tween_property(z, "modulate:a", 1.0, 0.2)

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
	lbl.add_theme_color_override("font_color", _col_text)
	hb.add_child(lbl)
	vb.add_child(hb)
	if not desc.is_empty():
		var dlbl := Label.new()
		dlbl.text = desc
		dlbl.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		dlbl.add_theme_color_override("font_color", _col_dim)
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
	_settings_panel = scroll
	_build_panel_contents()
	add_control_to_bottom_panel(scroll, "GodotLuau Config")
	if _first_build:
		make_bottom_panel_item_visible(scroll)
		_first_build = false

func _build_panel_contents() -> void:
	var outer := MarginContainer.new()
	for s in ["margin_left","margin_right","margin_top","margin_bottom"]:
		outer.add_theme_constant_override(s, 14)
	_settings_panel.add_child(outer)

	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 10)
	outer.add_child(vbox)

	# ── Header: solo título + idioma ─────────────────────────────────────
	var hdr_row := HBoxContainer.new()
	hdr_row.add_theme_constant_override("separation", 8)
	vbox.add_child(hdr_row)

	var hdr := Label.new()
	hdr.text = _t("panel_title")
	hdr.add_theme_color_override("font_color", _accent(Color(0.4, 0.8, 1.0)))
	hdr.add_theme_font_size_override("font_size", _fs(17))
	hdr.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hdr_row.add_child(hdr)

	var lang_lbl := Label.new()
	lang_lbl.text = "🌐"
	hdr_row.add_child(lang_lbl)
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
		# Persistir la elección: la extensión recordará el idioma siempre
		EditorInterface.get_editor_settings().set_setting("godot_luau/lang", _lang)
		_rebuild_panel()
	)
	hdr_row.add_child(lang_opt)

	# ── Barra de notificaciones (mensajes transitorios) ─────────────────
	_notif_bar = PanelContainer.new()
	_notif_bar.visible = false
	var notif_sb := StyleBoxFlat.new()
	notif_sb.bg_color = Color(0.16, 0.18, 0.24)
	notif_sb.set_corner_radius_all(6)
	notif_sb.set_content_margin_all(8)
	_notif_bar.add_theme_stylebox_override("panel", notif_sb)
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

	var zones : Array = []

	# ══ ZONA 1 · 🔄 ACTUALIZACIONES ══════════════════════════════════════
	var z_upd := _make_zone(_t("sec_updates"), Color(0.3, 0.9, 0.8))
	vbox.add_child(z_upd[0]); zones.append(z_upd[0])
	var upd_vb : VBoxContainer = z_upd[1]

	var ver_row := HBoxContainer.new()
	ver_row.add_theme_constant_override("separation", 6)
	upd_vb.add_child(ver_row)

	_ver_label = Label.new()
	_ver_label.text = _get_local_version()
	_ver_label.add_theme_color_override("font_color", _col_text)
	_ver_label.add_theme_font_size_override("font_size", _fs(13))
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

	# Volver a la version anterior (visible solo si hay un respaldo guardado)
	_rollback_btn = Button.new()
	_rollback_btn.flat = true
	_rollback_btn.add_theme_color_override("font_color", Color(1.0, 0.75, 0.4))
	_rollback_btn.tooltip_text = "Restore the previous version saved on your PC"
	_rollback_btn.pressed.connect(_on_rollback_pressed)
	ver_row.add_child(_rollback_btn)
	_refresh_rollback_btn()

	var gh_row := HBoxContainer.new()
	gh_row.add_theme_constant_override("separation", 6)
	upd_vb.add_child(gh_row)
	var gh_lbl := Label.new()
	gh_lbl.text = _t("github_label")
	gh_lbl.add_theme_font_size_override("font_size", _fs(11))
	gh_lbl.add_theme_color_override("font_color", _col_faint)
	gh_row.add_child(gh_lbl)
	var gh_link := Label.new()
	gh_link.text = "github.com/Pimpoli/GodotLuau"
	gh_link.add_theme_font_size_override("font_size", _fs(11))
	gh_link.add_theme_color_override("font_color", _accent(Color(0.35, 0.65, 1.0)))
	gh_link.mouse_filter = Control.MOUSE_FILTER_STOP
	gh_link.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
	gh_link.gui_input.connect(func(e: InputEvent) -> void:
		if e is InputEventMouseButton and e.button_index == MOUSE_BUTTON_LEFT and e.pressed:
			OS.shell_open(GITHUB_URL)
	)
	gh_row.add_child(gh_link)

	# ══ ZONA 2 · 🤖 IA Y AUTOCOMPLETADO ══════════════════════════════════
	var z_ac := _make_zone(_t("sec_autocomplete"), Color(0.4, 0.8, 1.0))
	vbox.add_child(z_ac[0]); zones.append(z_ac[0])
	var ac_vb : VBoxContainer = z_ac[1]

	# ── 1º: Autocompletado Instantáneo (arriba) ──────────────────────
	ac_vb.add_child(_make_row(_t("speed_title"), _t("speed_desc"),
		"godot_luau/instant_autocomplete", true,
		func(on: bool) -> void:
			if on:
				ProjectSettings.set_setting("godot_luau/ai_autocomplete_enabled", false)
				ProjectSettings.save()
				_apply_autocomplete_speed()
				_rebuild_panel()
	))

	# ── 2º: Autocompletado con IA (abajo, junto al modelo) ───────────
	# IA e Instantáneo son mutuamente excluyentes; la IA requiere
	# que el modelo seleccionado esté disponible (o descargado).
	ac_vb.add_child(_make_row(_t("ai_title"), _t("ai_desc"),
		"godot_luau/ai_autocomplete_enabled", false,
		func(on: bool) -> void:
			if on:
				if not _ai_model_available(_selected_model_id()):
					ProjectSettings.set_setting("godot_luau/ai_autocomplete_enabled", false)
					ProjectSettings.save()
					_set_notif(_t("aim_need_download"), Color(1.0, 0.75, 0.3))
					_rebuild_panel()
					return
				ProjectSettings.set_setting("godot_luau/instant_autocomplete", false)
				ProjectSettings.save()
				_rebuild_panel()
	))

	# Selector de modelo (como el selector de idioma)
	var sel_row := HBoxContainer.new()
	sel_row.add_theme_constant_override("separation", 8)
	ac_vb.add_child(sel_row)

	var sel_lbl := Label.new()
	sel_lbl.text = _t("aim_select_label")
	sel_lbl.add_theme_font_size_override("font_size", _fs(12))
	sel_lbl.add_theme_color_override("font_color", _col_text)
	sel_row.add_child(sel_lbl)

	var model_opt := OptionButton.new()
	model_opt.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var cur_id := _selected_model_id()
	for mi in AI_MODELS.size():
		var m : Dictionary = AI_MODELS[mi]
		model_opt.add_item("%s  —  %s %s" % [m["name"], m["params"], _t("aim_params_suffix")], mi)
		if m["id"] == cur_id:
			model_opt.select(mi)
	model_opt.item_selected.connect(func(idx: int) -> void:
		var m : Dictionary = AI_MODELS[idx]
		ProjectSettings.set_setting("godot_luau/ai_model_selected", m["id"])
		# Si la IA estaba activa pero el modelo nuevo no está disponible, apagarla
		if bool(ProjectSettings.get_setting("godot_luau/ai_autocomplete_enabled", false)) \
				and not _ai_model_available(str(m["id"])):
			ProjectSettings.set_setting("godot_luau/ai_autocomplete_enabled", false)
			_set_notif(_t("aim_need_download"), Color(1.0, 0.75, 0.3))
		ProjectSettings.save()
		_rebuild_panel()
	)
	sel_row.add_child(model_opt)

	# Botón "Descargar ⏬" solo si el modelo elegido es descargable y falta
	var sel_entry := _model_entry(cur_id)
	if sel_entry.has("url") and not _ai_model_available(cur_id):
		var dl_btn := Button.new()
		dl_btn.text = _t("aim_btn_dl")
		dl_btn.add_theme_color_override("font_color", _accent(Color(0.4, 0.9, 1.0)))
		dl_btn.pressed.connect(_download_selected_model)
		sel_row.add_child(dl_btn)

	_aim_status = Label.new()
	_aim_status.add_theme_font_size_override("font_size", _fs(11))
	ac_vb.add_child(_aim_status)
	_refresh_aim_status()

	# ── Modelo de IA personalizado (misma distribución que el
	#    autocompletado personalizado: URL+Descargar, debajo Importar/Limpiar) ──
	var aim_hdr := Label.new()
	aim_hdr.text = _t("aim_header")
	aim_hdr.add_theme_font_size_override("font_size", _fs(12))
	aim_hdr.add_theme_color_override("font_color", _accent(Color(0.95, 0.8, 0.4)))
	ac_vb.add_child(aim_hdr)

	var aim_note := Label.new()
	aim_note.text = _t("aim_note")
	aim_note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	aim_note.add_theme_font_size_override("font_size", _fs(11))
	aim_note.add_theme_color_override("font_color", _col_dim)
	ac_vb.add_child(aim_note)

	var aim_row := HBoxContainer.new()
	aim_row.add_theme_constant_override("separation", 6)
	ac_vb.add_child(aim_row)
	_aim_url_field = LineEdit.new()
	_aim_url_field.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_aim_url_field.placeholder_text = _t("cac_url_hint")
	_aim_url_field.text = str(ProjectSettings.get_setting("godot_luau/ai_model_url", ""))
	_aim_url_field.text_changed.connect(func(txt: String) -> void:
		ProjectSettings.set_setting("godot_luau/ai_model_url", txt)
		ProjectSettings.save()
	)
	aim_row.add_child(_aim_url_field)
	var aim_dl := Button.new()
	aim_dl.text = _t("cac_btn_download")
	aim_dl.pressed.connect(_download_ai_model)
	aim_row.add_child(aim_dl)

	var aim_btns := HBoxContainer.new()
	aim_btns.add_theme_constant_override("separation", 8)
	ac_vb.add_child(aim_btns)
	var aim_imp := Button.new()
	aim_imp.text = _t("cac_btn_import")
	aim_imp.pressed.connect(_import_ai_model_file)
	aim_btns.add_child(aim_imp)
	var aim_clr := Button.new()
	aim_clr.text = _t("cac_btn_clear")
	aim_clr.add_theme_color_override("font_color", Color(1.0, 0.6, 0.6))
	aim_clr.pressed.connect(_clear_ai_model)
	aim_btns.add_child(aim_clr)

	var cac_hdr := Label.new()
	cac_hdr.text = _t("cac_header")
	cac_hdr.add_theme_font_size_override("font_size", _fs(12))
	cac_hdr.add_theme_color_override("font_color", _accent(Color(0.6, 0.9, 0.6)))
	ac_vb.add_child(cac_hdr)

	ac_vb.add_child(_make_row(_t("cac_toggle_title"), _t("cac_toggle_desc"),
		"godot_luau/custom_autocomplete_enabled", false))

	var url_lbl := Label.new()
	url_lbl.text = _t("cac_url_label")
	url_lbl.add_theme_font_size_override("font_size", _fs(11))
	url_lbl.add_theme_color_override("font_color", _col_dim)
	ac_vb.add_child(url_lbl)

	var url_row := HBoxContainer.new()
	url_row.add_theme_constant_override("separation", 6)
	ac_vb.add_child(url_row)
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
	ac_vb.add_child(cac_btns)
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
	ac_vb.add_child(_cac_status)
	_refresh_cac_status()

	# ══ ZONA 3 · 📊 DATOS DE USO ═════════════════════════════════════════
	var z_data := _make_zone(_t("sec_data"), Color(0.5, 0.9, 0.5))
	vbox.add_child(z_data[0]); zones.append(z_data[0])
	var data_vb : VBoxContainer = z_data[1]

	data_vb.add_child(_make_row(_t("share_title"), _t("share_desc"),
		"godot_luau/share_data_enabled", false))

	var stats_sub := Label.new()
	stats_sub.text = _t("stats_header")
	stats_sub.add_theme_font_size_override("font_size", _fs(11))
	stats_sub.add_theme_color_override("font_color", _col_dim)
	data_vb.add_child(stats_sub)

	_stats_label = Label.new()
	_stats_label.add_theme_color_override("font_color", _col_dim)
	_stats_label.add_theme_font_size_override("font_size", _fs(11))
	data_vb.add_child(_stats_label)
	_refresh_stats()

	var stat_btns := HBoxContainer.new()
	stat_btns.add_theme_constant_override("separation", 8)
	data_vb.add_child(stat_btns)
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

	# ══ ZONA 4 · 🔧 DEBUG ════════════════════════════════════════════════
	var z_dbg := _make_zone(_t("sec_debug"), Color(1.0, 0.7, 0.3))
	vbox.add_child(z_dbg[0]); zones.append(z_dbg[0])
	var dbg_vb : VBoxContainer = z_dbg[1]

	dbg_vb.add_child(_make_row(_t("debug_title"), _t("debug_desc"),
		"godot_luau/script_output", true))

	dbg_vb.add_child(_make_row(_t("dbg_title"), _t("dbg_desc"),
		"godot_luau/debug_mode", false))

	dbg_vb.add_child(_make_row(_t("anim_title"), _t("anim_desc"),
		"godot_luau/debug_test_animation", false))

	dbg_vb.add_child(_make_row(_t("notif_outdated_title"), _t("notif_outdated_desc"),
		"godot_luau/notify_outdated_version", true))

	# ══ ZONA 5 · 🎨 APARIENCIA ═══════════════════════════════════════════
	var z_app := _make_zone(_t("sec_appearance"), Color(0.85, 0.6, 1.0))
	vbox.add_child(z_app[0]); zones.append(z_app[0])
	var app_vb : VBoxContainer = z_app[1]

	var size_lbl := Label.new()
	size_lbl.text = _t("font_size_label")
	size_lbl.add_theme_font_size_override("font_size", _fs(12))
	size_lbl.add_theme_color_override("font_color", _col_text)
	app_vb.add_child(size_lbl)

	var size_row := HBoxContainer.new()
	size_row.add_theme_constant_override("separation", 10)
	app_vb.add_child(size_row)

	var slider := HSlider.new()
	slider.min_value = 0
	slider.max_value = 100
	slider.step = 1
	slider.value = _font_pct
	slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	slider.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	slider.custom_minimum_size = Vector2(140, 0)
	size_row.add_child(slider)

	var pct_lbl := Label.new()
	pct_lbl.text = "%d%%" % _font_pct
	pct_lbl.custom_minimum_size = Vector2(46, 0)
	pct_lbl.add_theme_font_size_override("font_size", _fs(12))
	pct_lbl.add_theme_color_override("font_color", _accent(Color(0.85, 0.6, 1.0)))
	size_row.add_child(pct_lbl)

	slider.value_changed.connect(func(v: float) -> void:
		pct_lbl.text = "%d%%" % int(v)
	)
	slider.drag_ended.connect(func(changed: bool) -> void:
		if not changed: return
		EditorInterface.get_editor_settings().set_setting("godot_luau/panel_font_scale_pct", int(slider.value))
		_apply_font_pct(int(slider.value))
		_rebuild_panel()
	)

	# ── Footer ──────────────────────────────────────────────────────────
	var data_lbl := Label.new()
	data_lbl.text = _t("footer_data")
	data_lbl.add_theme_color_override("font_color", _col_faint)
	data_lbl.add_theme_font_size_override("font_size", _fs(10))
	vbox.add_child(data_lbl)

	_animate_zones(zones)

func _rebuild_panel() -> void:
	# Reconstruye solo el contenido, sin quitar el panel inferior (evita el parpadeo)
	if not (_settings_panel and is_instance_valid(_settings_panel)):
		return
	_compute_theme_colors()
	for c in _settings_panel.get_children():
		_settings_panel.remove_child(c)
		c.queue_free()
	_stats_label   = null
	_cac_status    = null
	_cac_url_field = null
	_aim_status    = null
	_aim_url_field = null
	_ver_label     = null
	_ver_btn       = null
	_reinstall_btn = null
	_notif_bar     = null
	_notif_label   = null
	_build_panel_contents()

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

# ── Modelos de IA: selección, descarga y estado ──────────────────────────────

func _ai_model_path() -> String:
	return OS.get_user_data_dir() + "/godotluau_ai_model.json"

func _selected_model_id() -> String:
	return str(ProjectSettings.get_setting("godot_luau/ai_model_selected", "mini"))

func _model_entry(id: String) -> Dictionary:
	for m in AI_MODELS:
		if m["id"] == id: return m
	return AI_MODELS[0]

func _model_file(id: String) -> String:
	if id == "custom": return _ai_model_path()
	return OS.get_user_data_dir() + "/godotluau_models/" + id + ".json"

func _ai_model_available(id: String) -> bool:
	var e := _model_entry(id)
	if e.get("builtin", false): return true
	return FileAccess.file_exists(_model_file(id))

# Descarga el modelo seleccionado del catálogo (solo cuando el usuario pulsa ⏬)
func _download_selected_model() -> void:
	if _http_aimodel and is_instance_valid(_http_aimodel): return
	var id := _selected_model_id()
	var e := _model_entry(id)
	var url := str(e.get("url", ""))
	if url.is_empty(): return
	if _aim_status and is_instance_valid(_aim_status):
		_aim_status.text = _t("cac_status_dl")
		_aim_status.add_theme_color_override("font_color", Color(0.9, 0.85, 0.3))
	_http_aimodel = HTTPRequest.new()
	_http_aimodel.timeout = 60.0
	add_child(_http_aimodel)
	_http_aimodel.request_completed.connect(func(result: int, code: int, _h: PackedStringArray, body: PackedByteArray) -> void:
		if _http_aimodel and is_instance_valid(_http_aimodel):
			_http_aimodel.queue_free(); _http_aimodel = null
		if result != HTTPRequest.RESULT_SUCCESS or code != 200:
			if _aim_status and is_instance_valid(_aim_status):
				_aim_status.text = _t("aim_status_err")
				_aim_status.add_theme_color_override("font_color", Color(1.0, 0.5, 0.5))
			return
		var text := body.get_string_from_utf8()
		var parsed = JSON.parse_string(text)
		if not (parsed is Dictionary and (parsed as Dictionary).has("bigrams")):
			if _aim_status and is_instance_valid(_aim_status):
				_aim_status.text = _t("aim_status_bad")
				_aim_status.add_theme_color_override("font_color", Color(1.0, 0.5, 0.5))
			return
		DirAccess.make_dir_recursive_absolute(OS.get_user_data_dir() + "/godotluau_models")
		var f := FileAccess.open(_model_file(id), FileAccess.WRITE)
		if f: f.store_string(text)
		_rebuild_panel()
	)
	if _http_aimodel.request(url) != OK:
		_http_aimodel.queue_free(); _http_aimodel = null
		if _aim_status and is_instance_valid(_aim_status):
			_aim_status.text = _t("aim_status_err")
			_aim_status.add_theme_color_override("font_color", Color(1.0, 0.5, 0.5))

func _refresh_aim_status() -> void:
	if not (_aim_status and is_instance_valid(_aim_status)): return
	var id := _selected_model_id()
	var e := _model_entry(id)
	if e.get("builtin", false):
		_aim_status.text = _t("aim_builtin_note")
		_aim_status.add_theme_color_override("font_color", Color(0.4, 0.9, 0.4))
		return
	var path := _model_file(id)
	if not FileAccess.file_exists(path):
		_aim_status.text = _t("aim_missing")
		_aim_status.add_theme_color_override("font_color", Color(0.9, 0.75, 0.3))
		return
	var parsed = JSON.parse_string(FileAccess.get_file_as_string(path))
	if parsed is Dictionary and (parsed as Dictionary).has("bigrams"):
		var n : int = int((parsed as Dictionary).get("params", 0))
		if n == 0:
			n = ((parsed as Dictionary)["bigrams"] as Dictionary).size()
			if (parsed as Dictionary).has("trigrams"):
				n += ((parsed as Dictionary)["trigrams"] as Dictionary).size()
		_aim_status.text = _t("aim_status_ok") % n
		_aim_status.add_theme_color_override("font_color", Color(0.4, 0.9, 0.4))
	else:
		_aim_status.text = _t("aim_status_bad")
		_aim_status.add_theme_color_override("font_color", Color(1.0, 0.5, 0.5))

func _save_ai_model(json_text: String) -> void:
	var parsed = JSON.parse_string(json_text)
	if not (parsed is Dictionary and (parsed as Dictionary).has("bigrams")):
		if _aim_status and is_instance_valid(_aim_status):
			_aim_status.text = _t("aim_status_bad")
			_aim_status.add_theme_color_override("font_color", Color(1.0, 0.5, 0.5))
		return
	var f := FileAccess.open(_ai_model_path(), FileAccess.WRITE)
	if f: f.store_string(json_text)
	_refresh_aim_status()

func _download_ai_model() -> void:
	if _http_aimodel and is_instance_valid(_http_aimodel): return
	var url := str(ProjectSettings.get_setting("godot_luau/ai_model_url", ""))
	if url.is_empty(): return
	if _aim_status and is_instance_valid(_aim_status):
		_aim_status.text = _t("cac_status_dl")
		_aim_status.add_theme_color_override("font_color", Color(0.9, 0.85, 0.3))
	_http_aimodel = HTTPRequest.new()
	_http_aimodel.timeout = 30.0
	add_child(_http_aimodel)
	_http_aimodel.request_completed.connect(func(result: int, code: int, _h: PackedStringArray, body: PackedByteArray) -> void:
		if _http_aimodel and is_instance_valid(_http_aimodel):
			_http_aimodel.queue_free(); _http_aimodel = null
		if result != HTTPRequest.RESULT_SUCCESS or code != 200:
			if _aim_status and is_instance_valid(_aim_status):
				_aim_status.text = _t("aim_status_err")
				_aim_status.add_theme_color_override("font_color", Color(1.0, 0.5, 0.5))
			return
		_save_ai_model(body.get_string_from_utf8())
	)
	if _http_aimodel.request(url) != OK:
		_http_aimodel.queue_free(); _http_aimodel = null
		if _aim_status and is_instance_valid(_aim_status):
			_aim_status.text = _t("aim_status_err")
			_aim_status.add_theme_color_override("font_color", Color(1.0, 0.5, 0.5))

func _import_ai_model_file() -> void:
	var dlg := FileDialog.new()
	dlg.title     = _t("cac_btn_import")
	dlg.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	dlg.access    = FileDialog.ACCESS_FILESYSTEM
	dlg.filters   = ["*.json ; JSON files"]
	dlg.file_selected.connect(func(p: String) -> void:
		var text := FileAccess.get_file_as_string(p)
		if not text.is_empty(): _save_ai_model(text)
		dlg.queue_free()
	)
	dlg.canceled.connect(func(): dlg.queue_free())
	get_editor_interface().get_base_control().add_child(dlg)
	dlg.popup_centered_ratio(0.6)

func _clear_ai_model() -> void:
	if FileAccess.file_exists(_ai_model_path()):
		DirAccess.remove_absolute(_ai_model_path())
	_refresh_aim_status()

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
	_notif_bar.modulate.a = 0.0
	var tw := _notif_bar.create_tween()
	tw.tween_property(_notif_bar, "modulate:a", 1.0, 0.2)
	get_tree().create_timer(4.0).timeout.connect(func():
		if not (_notif_bar and is_instance_valid(_notif_bar)): return
		var tw_out := _notif_bar.create_tween()
		tw_out.tween_property(_notif_bar, "modulate:a", 0.0, 0.3)
		tw_out.tween_callback(func():
			if _notif_bar and is_instance_valid(_notif_bar): _notif_bar.visible = false
		)
	)

func _reset_ver_idle() -> void:
	_set_ver_status(_get_local_version(), _t("btn_check_ver"), _col_dim)
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
	if _version_to_num(_remote_version) <= _version_to_num(local):
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

func _version_to_num(v: String) -> int:
	# "v1.4.4" → 1004004; solo avisa si la remota es realmente más nueva
	var n := 0
	for p in v.trim_prefix("v").split("."):
		n = n * 1000 + p.to_int()
	return n

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
	_start_hash_verify()

# ── Verificación de integridad (SHA-256) ─────────────────────────────────────

func _start_hash_verify() -> void:
	if _http_hash and is_instance_valid(_http_hash): return
	_set_ver_status(_t("bar_verifying"), "", Color(0.4, 0.8, 1.0))
	_http_hash = HTTPRequest.new()
	_http_hash.timeout = 15.0
	add_child(_http_hash)
	_http_hash.request_completed.connect(_on_hash_received)
	if _http_hash.request(HASH_URL) != OK:
		# Sin verificación de integridad NO instalamos (evita aplicar un ZIP
		# manipulado o corrupto). El repo oficial siempre publica el .sha256.
		_http_hash.queue_free(); _http_hash = null
		DirAccess.remove_absolute(OS.get_user_data_dir() + "/godotluau_update.zip")
		_set_ver_status(_t("bar_hash_err"), _t("bar_retry"), Color(1.0, 0.4, 0.4), "download")
		if _reinstall_btn and is_instance_valid(_reinstall_btn): _reinstall_btn.disabled = false

func _on_hash_received(result: int, code: int, _hdrs: PackedStringArray, body: PackedByteArray) -> void:
	if _http_hash and is_instance_valid(_http_hash):
		_http_hash.queue_free(); _http_hash = null
	var zip_path := OS.get_user_data_dir() + "/godotluau_update.zip"
	if result != HTTPRequest.RESULT_SUCCESS or code != 200:
		# No se pudo obtener el checksum → abortar (no instalar sin verificar).
		DirAccess.remove_absolute(zip_path)
		_set_ver_status(_t("bar_hash_err"), _t("bar_retry"), Color(1.0, 0.4, 0.4), "download")
		if _reinstall_btn and is_instance_valid(_reinstall_btn): _reinstall_btn.disabled = false
		return
	var expected := body.get_string_from_utf8().strip_edges().to_lower()
	if expected.contains(" "): expected = expected.split(" ")[0]
	var actual := FileAccess.get_sha256(zip_path).to_lower()
	if expected != actual:
		DirAccess.remove_absolute(zip_path)
		_set_ver_status(_t("bar_hash_err"), _t("bar_retry"), Color(1.0, 0.4, 0.4), "download")
		if _reinstall_btn and is_instance_valid(_reinstall_btn): _reinstall_btn.disabled = false
		return
	_set_ver_status(_t("bar_extracting"), "", Color(0.4, 0.8, 1.0))
	_apply_update.call_deferred()

# ── Apply update ──────────────────────────────────────────────────────────────

var _windows_staged_dlls : Array[Dictionary] = []

func _apply_update() -> void:
	var zip_path  := OS.get_user_data_dir() + "/godotluau_update.zip"
	var proj_path := ProjectSettings.globalize_path("res://")

	# ── 1. Validar el ZIP COMPLETO antes de tocar el proyecto ──────────────────
	var zip := ZIPReader.new()
	if zip.open(zip_path) != OK:
		_fail_apply(); return
	var zfiles := zip.get_files()
	var has_ext := false
	var has_dll := false
	for rel in zfiles:
		if rel.ends_with("godot_luau.gdextension"): has_ext = true
		if rel.get_extension().to_lower() in DLL_EXTENSIONS: has_dll = true
	if not (has_ext and has_dll):
		# ZIP incompleto o corrupto → abortar SIN haber tocado nada.
		zip.close(); _fail_apply(); return

	# ── 2. Respaldar la version actual (solo lo que se va a sobrescribir) ───────
	var old_ver := _get_local_version()
	_backup_current(zfiles, old_ver)

	# ── 3. Extraer: archivos normales directo, librerias a staging. Si algo no
	#       se puede escribir, restauramos del respaldo y abortamos. ────────────
	var staged  : Array[Dictionary] = []
	var staging := OS.get_user_data_dir() + "/godotluau_staging/"
	DirAccess.make_dir_recursive_absolute(staging)
	var is_win  : bool = (OS.get_name() == "Windows")
	var write_ok := true

	for rel in zfiles:
		if rel.ends_with("/"): continue
		# Anti zip-slip: rechazar rutas absolutas o con '..'
		var rel_clean : String = rel.replace("\\", "/")
		if rel_clean.begins_with("/") or rel_clean.contains("..") or rel_clean.contains(":"):
			push_warning("[GodotLuau] Skipped suspicious ZIP entry: " + rel)
			continue
		var data    : PackedByteArray = zip.read_file(rel)
		var dst     : String          = proj_path + rel_clean
		var dst_dir : String          = dst.get_base_dir()
		if not DirAccess.dir_exists_absolute(dst_dir):
			DirAccess.make_dir_recursive_absolute(dst_dir)
		# Las librerias nativas estan CARGADAS mientras el editor corre: no se
		# sobrescriben en el sitio, se preparan aparte y se cambian con rename.
		if rel.get_extension().to_lower() in DLL_EXTENSIONS:
			var stage := staging + rel.get_file()
			var sf := FileAccess.open(stage, FileAccess.WRITE)
			if sf: sf.store_buffer(data); staged.append({"src": stage, "dst": dst})
			else: write_ok = false
		else:
			var f := FileAccess.open(dst, FileAccess.WRITE)
			if f: f.store_buffer(data)
			else: write_ok = false
	zip.close()

	if not write_ok:
		# No dejar el proyecto a medias: restaurar lo respaldado.
		_restore_from_backup()
		_fail_apply(); return

	DirAccess.remove_absolute(zip_path)

	# ── 4. Cambiar las DLL (la anterior ya quedo guardada en el respaldo) ──────
	var swapped := true
	if not staged.is_empty():
		_windows_staged_dlls = staged
		swapped = _try_rename_and_replace()

	# ── 5. Escribir la version SOLO ahora que todo salio bien ──────────────────
	var vf := FileAccess.open(VERSION_FILE, FileAccess.WRITE)
	if vf: vf.store_string(_remote_version + "\n")
	if _reinstall_btn and is_instance_valid(_reinstall_btn): _reinstall_btn.disabled = false
	_refresh_rollback_btn()

	if staged.is_empty():
		_set_ver_status(_t("bar_ok_no_restart") % _remote_version, "", Color(0.3, 0.9, 0.5))
		_refresh_stats(); return
	if swapped:
		_set_ver_status(_t("bar_ok_restart"), _t("bar_restart"), Color(0.4, 0.9, 1.0), "restart")
		_auto_restart_countdown()
	elif is_win:
		# La DLL estaba bloqueada: aplicar tras cerrar mediante script externo.
		_set_ver_status(_t("bar_apply_close"), "Apply & Close", Color(1.0, 0.7, 0.2), "apply")
	else:
		_set_ver_status(_t("bar_ok_restart"), _t("bar_restart"), Color(0.4, 0.9, 1.0), "restart")

func _fail_apply() -> void:
	DirAccess.remove_absolute(OS.get_user_data_dir() + "/godotluau_update.zip")
	_set_ver_status(_t("bar_dl_failed") % 0, _t("bar_retry"), Color(1.0, 0.4, 0.4), "download")
	if _reinstall_btn and is_instance_valid(_reinstall_btn): _reinstall_btn.disabled = false

# ── Respaldo y vuelta atras (rollback) ────────────────────────────────────────

# Guarda en res://.luau_backup/files/ una copia de cada archivo del proyecto que
# la actualizacion va a sobrescribir, mas la version anterior. Permite deshacer.
func _backup_current(zfiles: PackedStringArray, old_ver: String) -> void:
	var proj := ProjectSettings.globalize_path("res://")
	var bdir := ProjectSettings.globalize_path(BACKUP_DIR) + "/"
	_rmdir_recursive(bdir)
	DirAccess.make_dir_recursive_absolute(bdir + "files")
	var gi := FileAccess.open(bdir + ".gdignore", FileAccess.WRITE)  # que Godot no lo importe
	if gi: gi.close()
	for rel in zfiles:
		if rel.ends_with("/"): continue
		var rel_clean : String = rel.replace("\\", "/")
		if rel_clean.begins_with("/") or rel_clean.contains("..") or rel_clean.contains(":"): continue
		var cur := proj + rel_clean
		if not FileAccess.file_exists(cur): continue
		var bpath := bdir + "files/" + rel_clean
		DirAccess.make_dir_recursive_absolute(bpath.get_base_dir())
		var bf := FileAccess.open(bpath, FileAccess.WRITE)
		if bf: bf.store_buffer(FileAccess.get_file_as_bytes(cur))
	var inf := FileAccess.open(bdir + "version.txt", FileAccess.WRITE)
	if inf: inf.store_string(old_ver + "\n")

func _can_rollback() -> bool:
	return FileAccess.file_exists(ProjectSettings.globalize_path(BACKUP_DIR) + "/version.txt")

func _backup_version() -> String:
	var p := ProjectSettings.globalize_path(BACKUP_DIR) + "/version.txt"
	if not FileAccess.file_exists(p): return ""
	var f := FileAccess.open(p, FileAccess.READ)
	return f.get_as_text().strip_edges() if f else ""

# Restaura los archivos respaldados a su sitio (lo usa tanto el rollback manual
# como el aborto de una extraccion fallida). Las DLL van por staging+rename.
func _restore_from_backup() -> bool:
	var proj := ProjectSettings.globalize_path("res://")
	var base := ProjectSettings.globalize_path(BACKUP_DIR) + "/files"
	if not DirAccess.dir_exists_absolute(base): return false
	var staged  : Array[Dictionary] = []
	var staging := OS.get_user_data_dir() + "/godotluau_rollback/"
	DirAccess.make_dir_recursive_absolute(staging)
	_collect_backup(base, "", proj, staging, staged)
	if not staged.is_empty():
		_windows_staged_dlls = staged
		_try_rename_and_replace()
	return true

func _collect_backup(base: String, rel: String, proj: String, staging: String, staged: Array) -> void:
	var dir_path := base + ("/" + rel if rel != "" else "")
	var d := DirAccess.open(dir_path)
	if not d: return
	d.list_dir_begin()
	var entry := d.get_next()
	while entry != "":
		var sub := (rel + "/" + entry) if rel != "" else entry
		if d.current_is_dir():
			_collect_backup(base, sub, proj, staging, staged)
		else:
			var src := base + "/" + sub
			var dst := proj + sub
			if src.get_extension().to_lower() in DLL_EXTENSIONS:
				var stage := staging + entry
				var sf := FileAccess.open(stage, FileAccess.WRITE)
				if sf: sf.store_buffer(FileAccess.get_file_as_bytes(src)); staged.append({"src": stage, "dst": dst})
			else:
				DirAccess.make_dir_recursive_absolute(dst.get_base_dir())
				var f := FileAccess.open(dst, FileAccess.WRITE)
				if f: f.store_buffer(FileAccess.get_file_as_bytes(src))
		entry = d.get_next()
	d.list_dir_end()

func _rmdir_recursive(path: String) -> void:
	var clean := path.rstrip("/")
	if not DirAccess.dir_exists_absolute(clean): return
	var d := DirAccess.open(clean)
	if not d: return
	d.list_dir_begin()
	var entry := d.get_next()
	while entry != "":
		var full := clean + "/" + entry
		if d.current_is_dir(): _rmdir_recursive(full)
		else: DirAccess.remove_absolute(full)
		entry = d.get_next()
	d.list_dir_end()
	DirAccess.remove_absolute(clean)

func _on_rollback_pressed() -> void:
	if not _can_rollback(): return
	var ver := _backup_version()
	_restore_from_backup()
	var vf := FileAccess.open(VERSION_FILE, FileAccess.WRITE)
	if vf: vf.store_string(ver + "\n")
	_set_ver_status(_t("bar_rollback_done") % ver, _t("bar_restart"), Color(0.4, 0.9, 1.0), "restart")
	_auto_restart_countdown()

func _refresh_rollback_btn() -> void:
	if not (_rollback_btn and is_instance_valid(_rollback_btn)): return
	if _can_rollback():
		_rollback_btn.visible = true
		_rollback_btn.text = _t("btn_rollback") % _backup_version()
	else:
		_rollback_btn.visible = false

func _auto_restart_countdown() -> void:
	for i in range(5, 0, -1):
		if _bar_action != "restart": return  # el usuario hizo otra cosa
		_set_ver_status(_t("bar_restart_in") % i, _t("bar_restart"), Color(0.4, 0.9, 1.0), "restart")
		await get_tree().create_timer(1.0).timeout
	if _bar_action == "restart":
		_do_editor_restart()

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
	# -e: reabrir el EDITOR con el proyecto (sin -e se ejecutaría el juego)
	lines.append("& \"%s\" -e --path \"%s\"" % [godot, proj_dir])

	var pf := FileAccess.open(ps_path, FileAccess.WRITE)
	if not pf: _set_ver_status(_t("bar_script_err"), "", Color(1.0, 0.4, 0.4)); return
	pf.store_string("\r\n".join(lines)); pf.close()

	OS.create_process("powershell.exe", [
		"-WindowStyle", "Hidden", "-NonInteractive",
		"-ExecutionPolicy", "Bypass", "-File", ps_path.replace("/", "\\")
	])
	await get_tree().create_timer(0.3).timeout
	get_tree().quit()
