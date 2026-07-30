#ifndef GL_MENU_TEMPLATES_H
#define GL_MENU_TEMPLATES_H

// ══════════════════════════════════════════════════════════════════════
//  Plantillas Luau del Menú (Escape) modular — 1.15
//
//  Estructura (StarterPlayer/StarterPlayerScripts/Modules/):
//    Menu (ModuleScript)          principal: arma el shell y las pestañas
//      MenuUi   (ModuleScript)    toolkit visual + shell (tabs, barra inferior)
//      Settings (ModuleScript)    pestaña "Config." (audio, gráficos, controles)
//      Players  (ModuleScript)    pestaña "Personas" (lista de jugadores)
//
//  TODO es EDITABLE por el usuario: colores, filas, pestañas, textos. El
//  PlayerModule hace require(Menu) y Menu.Init(player). El menú se abre con
//  Escape (UserInputService) y suprime el menú nativo del motor
//  (game:SetNativeMenuEnabled(false)).
//
//  Las cuatro plantillas viven ahora como .luau de verdad en
//  GodotLuau/DefaultScripts/StarterPlayer/StarterPlayerScripts/Modules/
//  (Menu.luau + Menu/MenuUi.luau, Menu/Settings.luau, Menu/Players.luau).
//  Las constantes LUAU_TEMPLATE_MENU* salen del header generado, asi que
//  este archivo solo documenta la estructura. Los .luau siguen compilando
//  en RUNTIME: validalos con scratchpad/checkluau.exe antes de compilar.
// ══════════════════════════════════════════════════════════════════════

#include "luau_templates_gen.h"   // LUAU_TEMPLATE_MENU / _UI / _SETTINGS / _PLAYERS

#endif // GL_MENU_TEMPLATES_H
