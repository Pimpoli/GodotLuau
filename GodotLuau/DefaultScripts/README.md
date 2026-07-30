# DefaultScripts — plantillas .luau por defecto

**ES.** Aqui vive el codigo Luau que GodotLuau escribe cuando crea un juego
(RobloxTemplate / RobloxGame3D / RobloxGame2D). Cada archivo `.luau` es la
version por defecto de un script del arbol de Roblox, y la carpeta espeja su
ruta real, asi que sabes de un vistazo a que script pertenece:

| Archivo | Script en el arbol de Roblox |
| --- | --- |
| `StarterPlayer/StarterCharacterScripts/Health.luau` | ServerScript `Health` |
| `StarterPlayer/StarterCharacterScripts/Animate.luau` | LocalScript `Animate` |
| `StarterPlayer/StarterPlayerScripts/PlayerModule.luau` | LocalScript `PlayerModule` |
| `StarterPlayer/StarterPlayerScripts/Modules/*.luau` | ModuleScripts `ControlModule`, `CameraModule`, `ChatModule`, `Menu`... |
| `StarterPlayer/StarterPlayerScripts/Modules/ControlModule/*.luau` | Sub-modulos `PCModule`, `MobileModule`, `ConsoleModule` |
| `StarterPlayer/StarterPlayerScripts/Modules/Menu/*.luau` | `MenuUi`, `Settings`, `Players` |
| `ServerScriptService/GameManager.luau` | ServerScript `GameManager` |

Editalos como cualquier script: son texto de verdad, con autocompletado y
resaltado. Al arrancar, el motor **lee el `.luau` del disco**, asi que puedes
cambiar el movimiento del jugador o el menu y probarlo **sin recompilar** la
extension. Si quieres mejorar algo, bajate solo ese `.luau`, modificalo y
mandalo: se reemplaza el archivo y listo.

**Importante:** la DLL lleva una copia embebida (para juegos exportados). Si
cambias un `.luau`, ejecuta antes de compilar:

```
python tools/generar_plantillas_luau.py
```

`PlayerController2D.lua` y `PlayerController3D.lua` NO son plantillas de este
sistema: los usa `tools/nuevo_proyecto.py`. No los muevas.

**EN.** These `.luau` files are the default version of each Roblox script
GodotLuau creates; the folders mirror the real Roblox path (see the table
above). They are plain text, so you get autocompletion, and the engine reads
them from disk at startup — edit one, hit play, no rebuild needed. Grab just
the file you want to improve, change it, and send it over. The DLL keeps an
embedded copy for exported games, so after editing a `.luau` run
`python tools/generar_plantillas_luau.py` before compiling.
