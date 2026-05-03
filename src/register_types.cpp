#include "register_types.h"
#include "luau_script.h"
#include "luau_language.h"
#include "roblox_workspace.h"
#include "roblox_workspace2d.h"
#include "roblox_player.h"
#include "roblox_player2d.h"
#include "roblox_part.h"
#include "humanoid.h"
#include "humanoid2d.h"
#include "folder.h"
#include "roblox_chat.h"
#include "roblox_services.h"
#include "roblox_datamodel.h"
#include "roblox_game3d.h"
#include "roblox_game2d.h"
#include "roblox_remote.h"
#include "roblox_lighting_fx.h"
#include "roblox_bodymovers.h"
#include "roblox_constraints.h"
#include "roblox_gui.h"
#include "roblox_tween.h"
#include "roblox_sound.h"
#include "roblox_interactive.h"
#include "roblox_billboard.h"
#include "roblox_input.h"
#include "roblox_animation.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

static LuauLanguage* luau_lang   = nullptr;
static Ref<LuauLoader>  luau_loader;
static Ref<LuauSaver>   luau_saver;

void initialize_luau_module(ModuleInitializationLevel p_level) {

    // ── SERVERS level: register the Luau scripting language
    //// ── Nivel SERVERS: registrar el lenguaje Luau
    if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
        ClassDB::register_class<LuauScript>();
        ClassDB::register_class<LuauLanguage>();
        luau_lang = memnew(LuauLanguage);
        Engine::get_singleton()->register_script_language(luau_lang);
    }

    // ── SCENE level: register all nodes and services
    //// ── Nivel SCENE: registrar todos los nodos y servicios
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {

        // Scripts (color in editor: Blue / Naranja / Purple)
        //// Scripts (color en el editor: Azul / Naranja / Morado)
        ClassDB::register_class<ScriptNodeBase>();
        ClassDB::register_class<LocalScript>();   // Blue / Azul
        ClassDB::register_class<ServerScript>();  // Orange / Naranja
        ClassDB::register_class<ModuleScript>();  // Purple / Morado

        // 3D characters and physics
        //// Personajes y físicas 3D
        ClassDB::register_class<Humanoid>();
        ClassDB::register_class<RobloxWorkspace>();
        ClassDB::register_class<RobloxPlayer>();
        ClassDB::register_class<RobloxPart>();

        // 2D characters and physics
        //// Personajes y físicas 2D
        ClassDB::register_class<Humanoid2D>();
        ClassDB::register_class<RobloxWorkspace2D>();
        ClassDB::register_class<RobloxPlayer2D>();

        // Chat
        //// Chat
        ClassDB::register_class<RobloxChat>();

        // Services equivalent to Roblox
        //// Servicios equivalentes a Roblox
        ClassDB::register_class<ReplicatedStorage>();
        ClassDB::register_class<ServerStorage>();
        ClassDB::register_class<ReplicatedFirst>();
        ClassDB::register_class<ServerScriptService>();
        ClassDB::register_class<StarterCharacterScripts>();
        ClassDB::register_class<StarterPlayerScripts>();
        ClassDB::register_class<StarterPlayer>();
        ClassDB::register_class<StarterGui>();
        ClassDB::register_class<StarterPack>();
        ClassDB::register_class<Players>();
        ClassDB::register_class<Lighting>();
        ClassDB::register_class<MaterialService>();
        ClassDB::register_class<NetworkClient>();
        ClassDB::register_class<Teams>();
        ClassDB::register_class<Folder>();
        ClassDB::register_class<RemoteEventNode>();
        ClassDB::register_class<RemoteFunctionNode>();
        ClassDB::register_class<BindableEventNode>();
        ClassDB::register_class<SoundService>();
        ClassDB::register_class<RunService>();
        ClassDB::register_class<TextChatService>();

        // BodyMovers — equivalent to Roblox's BodyMovers
        //// BodyMovers — equivalentes a los BodyMovers de Roblox
        ClassDB::register_class<BodyVelocity>();
        ClassDB::register_class<BodyPosition>();
        ClassDB::register_class<BodyForce>();
        ClassDB::register_class<BodyAngularVelocity>();
        ClassDB::register_class<BodyGyro>();

        // Constraints — joints and physical restrictions
        //// Constraints — juntas y restricciones físicas
        ClassDB::register_class<WeldConstraint>();
        ClassDB::register_class<HingeConstraint>();
        ClassDB::register_class<BallSocketConstraint>();
        ClassDB::register_class<RodConstraint>();
        ClassDB::register_class<SpringConstraint>();

        // Roblox-style GUI — ScreenGui, Frame, TextLabel, etc.
        //// GUI tipo Roblox — ScreenGui, Frame, TextLabel, etc.
        ClassDB::register_class<ScreenGui>();
        ClassDB::register_class<RobloxFrame>();
        ClassDB::register_class<RobloxTextLabel>();
        ClassDB::register_class<RobloxTextButton>();
        ClassDB::register_class<RobloxTextBox>();
        ClassDB::register_class<RobloxImageLabel>();
        ClassDB::register_class<RobloxScrollingFrame>();

        // Lighting FX nodes
        //// Nodos de efectos de iluminación
        ClassDB::register_class<AtmosphereNode>();
        ClassDB::register_class<LightingSkyNode>();
        ClassDB::register_class<SunRaysNode>();
        ClassDB::register_class<BloomEffect>();
        ClassDB::register_class<BlurEffect>();
        ClassDB::register_class<ColorCorrectionEffect>();
        ClassDB::register_class<DepthOfFieldEffect>();

        // DataModel (legacy root node)
        //// DataModel (nodo raíz heredado)
        ClassDB::register_class<RobloxDataModel>();

        // New root nodes: choose 3D or 2D when creating the scene
        //// Nuevos nodos raíz: selecciona 3D o 2D al crear la escena
        ClassDB::register_class<RobloxGame3D>();
        ClassDB::register_class<RobloxGame2D>();

        // TweenService
        //// TweenService
        ClassDB::register_class<RobloxTween>();
        ClassDB::register_class<TweenService>();

        // Sound
        //// Sonido
        ClassDB::register_class<RobloxSound>();
        ClassDB::register_class<RobloxSoundGroup>();

        // Interactive
        //// Elementos interactivos
        ClassDB::register_class<ClickDetector>();
        ClassDB::register_class<ProximityPrompt>();
        ClassDB::register_class<SpawnLocation>();

        // Billboard / Surface / Motor6D / Tool
        //// Billboard / Surface / Motor6D / Herramienta
        ClassDB::register_class<BillboardGui>();
        ClassDB::register_class<SurfaceGui>();
        ClassDB::register_class<Motor6D>();
        ClassDB::register_class<RobloxTool>();
        ClassDB::register_class<Backpack>();

        // Input / Collection
        //// Entrada / Colección
        ClassDB::register_class<UserInputService>();
        ClassDB::register_class<CollectionService>();

        // Animation
        //// Animación
        ClassDB::register_class<AnimationTrack>();
        ClassDB::register_class<AnimationObject>();

        // .lua script loader and saver
        //// Cargador y guardador de scripts .lua
        ClassDB::register_class<LuauLoader>();
        ClassDB::register_class<LuauSaver>();
        luau_loader.instantiate();
        ResourceLoader::get_singleton()->add_resource_format_loader(luau_loader);
        luau_saver.instantiate();
        ResourceSaver::get_singleton()->add_resource_format_saver(luau_saver);

        // Ensure the icons folder exists (required for node colors in the editor)
        //// Asegurar carpeta de iconos (necesaria para colores de nodos en editor)
        if (Engine::get_singleton()->is_editor_hint()) {
            Ref<DirAccess> dir = DirAccess::open("res://");
            if (dir.is_valid() && !dir->dir_exists("icons")) {
                dir->make_dir("icons");
                UtilityFunctions::print("[GodotLuau] Carpeta 'res://icons/' creada.");
            }
        }

        UtilityFunctions::print(
            "[GodotLuau] ══════════════════════════════════════════\n"
            "[GodotLuau]   GodotLuau — by PimpoliDev\n"
            "[GodotLuau]   Sistema Luau para Godot [Activado]\n"
            "[GodotLuau] ══════════════════════════════════════════");
    }
}

void uninitialize_luau_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        if (luau_loader.is_valid())
            ResourceLoader::get_singleton()->remove_resource_format_loader(luau_loader);
        if (luau_saver.is_valid())
            ResourceSaver::get_singleton()->remove_resource_format_saver(luau_saver);
    }
    if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
        if (luau_lang) {
            Engine::get_singleton()->unregister_script_language(luau_lang);
            memdelete(luau_lang);
            luau_lang = nullptr;
        }
    }
}

extern "C" {
GDExtensionBool GDE_EXPORT luau_extension_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    const GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization* r_initialization)
{
    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
    init_obj.register_initializer(initialize_luau_module);
    init_obj.register_terminator(uninitialize_luau_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SERVERS);
    return init_obj.init();
}
}
