#include "luau_script.h"
#include "luau_language.h"

using namespace godot;

ScriptLanguage *LuauScript::_get_language() const {
    return LuauLanguage::get_singleton();
}