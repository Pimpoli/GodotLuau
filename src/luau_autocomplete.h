#ifndef LUAU_AUTOCOMPLETE_H
#define LUAU_AUTOCOMPLETE_H

#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/classes/placeholder_texture2d.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

using namespace godot;

struct LuauAPIItem {
    const char* prefix;      
    const char* name;        
    const char* insert_text; 
    const char* signature;   
    const char* desc;        
    int kind; 
};

struct LocalVar {
    String name;
    String inferred_type;
};

// --- ESTRUCTURA MEJORADA DE IA ---
struct TempSuggestion {
    int priority_layer; // 1 = Exacto, 2 = Fuzzy
    int usage_count;    // La IA guarda cuántas veces has usado esto
    int fuzzy_score;
    String display;
    String insert_text;
    String desc;
    String signature;
    int kind;
    Color color;

    // Regla de ordenamiento: 1º Capa, 2º Uso (El aprendizaje de la IA), 3º Puntuación Fuzzy
    bool operator<(const TempSuggestion& other) const {
        if (priority_layer != other.priority_layer) {
            return priority_layer < other.priority_layer;
        }
        if (usage_count != other.usage_count) {
            return usage_count > other.usage_count; // Lo que más usas va primero
        }
        return fuzzy_score > other.fuzzy_score; 
    }
};

class LuauAutocomplete {
private:
    static Ref<PlaceholderTexture2D> get_dummy_icon() {
        static Ref<PlaceholderTexture2D> dummy_icon;
        if (dummy_icon.is_null()) {
            dummy_icon.instantiate();
            dummy_icon->set_size(Vector2(16, 16));
        }
        return dummy_icon;
    }

    static bool is_lua_keyword(const String& w) {
        static const std::unordered_set<std::string> keywords = {
            "and", "break", "do", "else", "elseif", "end", "false", "for", 
            "function", "if", "in", "local", "nil", "not", "or", "repeat", 
            "return", "then", "true", "until", "while", "require", "continue", "export", "type"
        };
        return keywords.count(w.utf8().get_data()) > 0;
    }

    static int levenshtein_distance(const std::string& s1, const std::string& s2) {
        int len1 = s1.size(), len2 = s2.size();
        std::vector<std::vector<int>> dp(len1 + 1, std::vector<int>(len2 + 1));
        for (int i = 0; i <= len1; i++) dp[i][0] = i;
        for (int j = 0; j <= len2; j++) dp[0][j] = j;
        for (int i = 1; i <= len1; i++) {
            for (int j = 1; j <= len2; j++) {
                if (s1[i - 1] == s2[j - 1]) dp[i][j] = dp[i - 1][j - 1];
                else dp[i][j] = 1 + std::min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
            }
        }
        return dp[len1][len2];
    }

    static int fuzzy_match_score(const String& filter, const String& target) {
        if (filter.is_empty()) return 100;
        std::string f = filter.utf8().get_data();
        std::string t = target.utf8().get_data();
        std::string f_lower = filter.to_lower().utf8().get_data();
        std::string t_lower = target.to_lower().utf8().get_data();

        int score = 0;
        int f_idx = 0;
        for (int i = 0; i < t.length() && f_idx < f.length(); i++) {
            if (t_lower[i] == f_lower[f_idx]) {
                if (std::isupper(t[i]) && std::islower(f[f_idx])) score += 15; 
                else score += 5;
                f_idx++;
            }
        }
        if (f_idx == f.length()) return score + 20;

        if (f.length() > 2 && t.length() > 2) {
            int dist = levenshtein_distance(f_lower, t_lower);
            if (dist <= 2) return 10 - dist; 
        }
        return 0; 
    }

    // --- CEREBRO DE IA: Analiza la frecuencia de palabras en el código actual ---
    static std::unordered_map<std::string, int> analyze_usage_frequency(const String& p_code) {
        std::unordered_map<std::string, int> freq;
        std::string raw_code = p_code.utf8().get_data();
        std::string current_word = "";
        
        for (char c : raw_code) {
            if (std::isalnum(c) || c == '_') {
                current_word += c;
            } else {
                if (!current_word.empty()) {
                    freq[current_word]++;
                    current_word = "";
                }
            }
        }
        if (!current_word.empty()) freq[current_word]++;
        return freq;
    }

    // --- LECTURA DINÁMICA DEL MOTOR GODOT ---
    static void add_dynamic_class_suggestions(const String& class_name, const String& filter, std::vector<TempSuggestion>& sorter, const std::unordered_map<std::string, int>& memory) {
        if (!ClassDB::class_exists(class_name)) return;

        // Propiedades Dinámicas
        TypedArray<Dictionary> properties = ClassDB::class_get_property_list(class_name, true);
        for (int i = 0; i < properties.size(); i++) {
            Dictionary prop = properties[i];
            String p_name = prop["name"];
            int score = fuzzy_match_score(filter, p_name);
            if (score > 0) {
                int layer = p_name.to_lower().begins_with(filter.to_lower()) ? 1 : 2;
                std::string p_name_std = p_name.utf8().get_data();
                int usage = memory.count(p_name_std) ? memory.at(p_name_std) : 0;
                
                sorter.push_back({layer, usage, score, p_name, p_name, "[Propiedad Dinámica] de " + class_name, "", 4, Color(0.6f, 0.8f, 1.0f, 1.0f)});
            }
        }

        // Métodos Dinámicos
        TypedArray<Dictionary> methods = ClassDB::class_get_method_list(class_name, true);
        for (int i = 0; i < methods.size(); i++) {
            Dictionary meth = methods[i];
            String m_name = meth["name"];
            if (m_name.begins_with("_")) continue;

            int score = fuzzy_match_score(filter, m_name);
            if (score > 0) {
                int layer = m_name.to_lower().begins_with(filter.to_lower()) ? 1 : 2;
                String insert = m_name + "()";
                std::string m_name_std = m_name.utf8().get_data();
                int usage = memory.count(m_name_std) ? memory.at(m_name_std) : 0;

                sorter.push_back({layer, usage, score, m_name + "()", insert, "[Método Dinámico] de " + class_name, "", 1, Color(0.87f, 0.87f, 0.67f, 1.0f)});
            }
        }
    }

public:
    static Dictionary get_suggestions(const String& p_code) {
        Dictionary res;
        Array final_suggestions_array;

        if (p_code.is_empty()) {
            res["result"] = final_suggestions_array; res["options"] = final_suggestions_array; res["force"] = true; res["call_hint"] = ""; return res;
        }

        // 1. LA IA APRENDE EL CONTEXTO ACTUAL
        std::unordered_map<std::string, int> usage_memory = analyze_usage_frequency(p_code);

        int cursor_pos = p_code.find(String::chr(0xFFFF));
        if (cursor_pos == -1) cursor_pos = p_code.length();
        String code_to_cursor = p_code.substr(0, cursor_pos);

        // Regla de silenciamiento inteligente
        bool in_str_d = false, in_str_s = false, in_comm = false, in_block_comm = false;
        bool is_expecting_instance_name = false; 
        
        for (int i = 0; i < code_to_cursor.length(); i++) {
            char32_t c = code_to_cursor[i];
            char32_t next_c = (i + 1 < code_to_cursor.length()) ? code_to_cursor[i+1] : 0;
            if (!in_str_d && !in_str_s && !in_comm && !in_block_comm) {
                if (c == '-' && next_c == '-') { 
                    if (i + 3 < code_to_cursor.length() && code_to_cursor[i+2] == '[' && code_to_cursor[i+3] == '[') {
                        in_block_comm = true; i += 3;
                    } else { in_comm = true; i++; }
                }
                else if (c == '"' || c == '\'') {
                    if (c == '"') in_str_d = true;
                    if (c == '\'') in_str_s = true;

                    String code_before_quote = code_to_cursor.substr(0, i).strip_edges();
                    if (code_before_quote.ends_with("FindFirstChild(") ||
                        code_before_quote.ends_with("WaitForChild(")   ||
                        code_before_quote.ends_with("GetService(")     ||
                        code_before_quote.ends_with("Instance.new(")) {
                        is_expecting_instance_name = true;
                    }
                }
            } else {
                if (in_comm && c == '\n') in_comm = false;
                else if (in_block_comm && c == ']' && next_c == ']') { in_block_comm = false; i++; }
                else if (in_str_d && c == '"' && (i == 0 || code_to_cursor[i-1] != '\\')) in_str_d = false;
                else if (in_str_s && c == '\'' && (i == 0 || code_to_cursor[i-1] != '\\')) in_str_s = false;
            }
        }

        if ((in_str_d || in_str_s) && !is_expecting_instance_name) {
            res["result"] = final_suggestions_array; res["options"] = final_suggestions_array; res["force"] = true; res["call_hint"] = ""; return res;
        }
        if (in_comm || in_block_comm) {
            res["result"] = final_suggestions_array; res["options"] = final_suggestions_array; res["force"] = true; res["call_hint"] = ""; return res;
        }

        // Extracción de contexto de la escritura actual
        int last_separator = -1;
        bool is_after_equal = false, is_after_colon = false;

        for (int i = code_to_cursor.length() - 1; i >= 0; i--) {
            char32_t c = code_to_cursor[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == ':')) {
                if (last_separator == -1) last_separator = i;
                if (c != ' ' && c != '\t' && c != '\n') {
                    if (c == '=') is_after_equal = true;
                    break;
                }
            }
        }
        
        String current_word = code_to_cursor.substr(last_separator + 1).strip_edges();
        
        if (is_expecting_instance_name && (current_word.begins_with("\"") || current_word.begins_with("'"))) {
             current_word = current_word.substr(1);
        }

        String target_prefix = "", filter = current_word;
        
        int dot_idx = current_word.rfind(".");
        int colon_idx = current_word.rfind(":");
        if (colon_idx > dot_idx) { target_prefix = current_word.substr(0, colon_idx); filter = current_word.substr(colon_idx + 1); is_after_colon = true; } 
        else if (dot_idx != -1) { target_prefix = current_word.substr(0, dot_idx); filter = current_word.substr(dot_idx + 1); }

        std::vector<LocalVar> local_variables;
        std::unordered_set<std::string> known_vars;

        PackedStringArray lines = code_to_cursor.split("\n"); 
        for (int i = 0; i < lines.size(); i++) {
            String line = lines[i].strip_edges();
            if (line.begins_with("local ")) {
                int eq_idx = line.find("=");
                String var_decl = (eq_idx != -1) ? line.substr(6, eq_idx - 6).strip_edges() : line.substr(6).strip_edges();
                String var_val = (eq_idx != -1) ? line.substr(eq_idx + 1).strip_edges() : "";
                
                std::string var_str = var_decl.utf8().get_data();
                if (known_vars.count(var_str) == 0 && !is_lua_keyword(var_decl) && !var_decl.is_empty()) {
                    LocalVar lvar; lvar.name = var_decl; lvar.inferred_type = "any";

                    if (var_val.begins_with("Instance.new(")) lvar.inferred_type = "Instance";
                    else if (var_val.begins_with("Vector3.new")) lvar.inferred_type = "Vector3";
                    else if (var_val.begins_with("CFrame.new") || var_val.begins_with("CFrame.Angles")) lvar.inferred_type = "CFrame";
                    else if (var_val.begins_with("Color3.new") || var_val.begins_with("Color3.fromRGB")) lvar.inferred_type = "Color3";
                    else if (var_val.begins_with("workspace")) lvar.inferred_type = "Workspace";
                    else if (var_val.begins_with("game:GetService(\"Players\")"))               lvar.inferred_type = "Players";
                    else if (var_val.begins_with("game:GetService(\"Lighting\")"))              lvar.inferred_type = "Lighting";
                    else if (var_val.begins_with("game:GetService(\"RunService\")"))            lvar.inferred_type = "RunService";
                    else if (var_val.begins_with("game:GetService(\"TweenService\")"))          lvar.inferred_type = "TweenService";
                    else if (var_val.begins_with("game:GetService(\"SoundService\")"))          lvar.inferred_type = "SoundService";
                    else if (var_val.begins_with("game:GetService(\"TextChatService\")"))       lvar.inferred_type = "TextChatService";
                    else if (var_val.begins_with("game:GetService(\"StarterGui\")"))            lvar.inferred_type = "StarterGui";
                    else if (var_val.begins_with("game:GetService(\"ReplicatedStorage\")"))     lvar.inferred_type = "ReplicatedStorage";
                    else if (var_val.begins_with("game:GetService(\"ServerScriptService\")"))   lvar.inferred_type = "ServerScriptService";
                    else if (var_val.begins_with("game:GetService(\"UserInputService\")"))      lvar.inferred_type = "UserInputService";
                    else if (var_val.begins_with("game:GetService(\"HttpService\")"))           lvar.inferred_type = "HttpService";
                    else if (var_val.begins_with("game:GetService(\"Debris\")"))                lvar.inferred_type = "Debris";
                    else if (var_val.begins_with("game:GetService(\"CollectionService\")"))     lvar.inferred_type = "CollectionService";
                    else if (var_val.begins_with("game:GetService(\"DataStoreService\")"))      lvar.inferred_type = "DataStoreService";
                    else if (var_val.begins_with("game:GetService(\"ContextActionService\")"))  lvar.inferred_type = "ContextActionService";
                    else if (var_val.begins_with("game:GetService(\"PathfindingService\")"))    lvar.inferred_type = "PathfindingService";
                    else if (var_val.begins_with("game:GetService(\"PhysicsService\")"))        lvar.inferred_type = "PhysicsService";
                    else if (var_val.begins_with("game:GetService(\"TeleportService\")"))       lvar.inferred_type = "TeleportService";
                    else if (var_val.begins_with("game:GetService(\"BadgeService\")"))          lvar.inferred_type = "BadgeService";
                    else if (var_val.begins_with("game:GetService(\"MarketplaceService\")"))    lvar.inferred_type = "MarketplaceService";
                    else if (var_val.begins_with("game:GetService(\"GuiService\")"))            lvar.inferred_type = "GuiService";
                    else if (var_val.begins_with("game:GetService(\"InsertService\")"))         lvar.inferred_type = "InsertService";
                    else if (var_val.begins_with("game:GetService(\"ScriptContext\")"))         lvar.inferred_type = "ScriptContext";
                    else if (var_val.begins_with("script")) lvar.inferred_type = "Node";
                    
                    local_variables.push_back(lvar);
                    known_vars.insert(var_str);
                }
            }
        }

        String resolved_prefix_type = target_prefix;
        for (const auto& v : local_variables) { if (v.name == target_prefix) { resolved_prefix_type = v.inferred_type; break; } }

        // Mapeos Inteligentes para conectar con Godot directamente
        if (target_prefix == "script")                         resolved_prefix_type = "Node";
        if (target_prefix == "workspace" || target_prefix == "Workspace") resolved_prefix_type = "Workspace";
        if (target_prefix == "game")                         resolved_prefix_type = "game";
        if (target_prefix == "RunService")                   resolved_prefix_type = "RunService";
        if (target_prefix == "Players")                      resolved_prefix_type = "Players";
        if (target_prefix == "Lighting")                     resolved_prefix_type = "Lighting";
        if (target_prefix == "TweenService")                 resolved_prefix_type = "TweenService";
        if (target_prefix == "UserInputService")             resolved_prefix_type = "UserInputService";
        if (target_prefix == "HttpService")                  resolved_prefix_type = "HttpService";
        if (target_prefix == "Debris")                       resolved_prefix_type = "Debris";
        if (target_prefix == "CollectionService")            resolved_prefix_type = "CollectionService";
        if (target_prefix == "DataStoreService")             resolved_prefix_type = "DataStoreService";
        if (target_prefix == "ContextActionService")         resolved_prefix_type = "ContextActionService";
        if (target_prefix == "PathfindingService")           resolved_prefix_type = "PathfindingService";
        if (target_prefix == "PhysicsService")               resolved_prefix_type = "PhysicsService";
        if (target_prefix == "TeleportService")              resolved_prefix_type = "TeleportService";
        if (target_prefix == "BadgeService")                 resolved_prefix_type = "BadgeService";
        if (target_prefix == "MarketplaceService")           resolved_prefix_type = "MarketplaceService";
        if (target_prefix == "GuiService")                   resolved_prefix_type = "GuiService";
        if (target_prefix == "InsertService")                resolved_prefix_type = "InsertService";
        if (target_prefix == "ScriptContext")                resolved_prefix_type = "ScriptContext";
        if (target_prefix == "SoundService")                 resolved_prefix_type = "SoundService";
        if (target_prefix == "TextChatService")              resolved_prefix_type = "TextChatService";

        std::vector<TempSuggestion> sorter;

        // BÚSQUEDA DINÁMICA EN EL MOTOR
        if (!target_prefix.is_empty() && !is_expecting_instance_name) {
            String class_to_search = resolved_prefix_type.is_empty() ? target_prefix : resolved_prefix_type;
            add_dynamic_class_suggestions(class_to_search, filter, sorter, usage_memory);
        }

        // ==========================================
        // LA BASE DE DATOS TITÁNICA (EL ALMA DEL MOTOR - INTACTA)
        // ==========================================
        static const LuauAPIItem api[] = {
            // GLOBALES Y PALABRAS CLAVE
            {"", "print", "print()", "(...: any) -> ()", "Imprime valores en la consola de salida.", 1},
            {"", "warn", "warn()", "(...: any) -> ()", "Imprime texto de advertencia en color amarillo.", 1},
            {"", "error", "error()", "(message: string, level: number?) -> ()", "Arroja un error rojo y detiene la ejecucion.", 1},
            {"", "assert", "assert()", "(condition: any, message: string?) -> any", "Arroja error si la condicion es falsa.", 1},
            {"", "require", "require()", "(module: ModuleScript) -> any", "Ejecuta y devuelve el contenido de un ModuleScript.", 1},
            {"", "tick", "tick()", "() -> number", "Devuelve los segundos desde la epoca UNIX local.", 1},
            {"", "time", "time()", "() -> number", "Devuelve el tiempo de ejecucion de la experiencia.", 1},
            {"", "type", "type()", "(value: any) -> string", "Devuelve el tipo base de un valor.", 1},
            {"", "typeof", "typeof()", "(value: any) -> string", "Devuelve el tipo exacto (incluyendo tipos de Roblox).", 1},
            {"", "wait", "wait()", "(seconds: number?) -> (number, number)", "Pausa la ejecucion (Obsoleto, usa task.wait).", 1},
            {"", "spawn", "spawn()", "(callback: function) -> ()", "Ejecuta en un hilo paralelo (Obsoleto, usa task.spawn).", 1},
            {"", "delay", "delay()", "(seconds: number, callback: function) -> ()", "Retrasa ejecucion (Obsoleto, usa task.delay).", 1},
            {"", "game", "game", "DataModel", "La raiz principal que contiene todos los servicios.", 3},
            {"", "workspace", "workspace", "Workspace", "Contiene todas las partes fisicas del mundo.", 3},
            {"", "script", "script", "LuaSourceContainer", "Referencia al script actual en ejecucion.", 3},
            {"", "math", "math", "Library", "Libreria de funciones matematicas estandar.", 0},
            {"", "string", "string", "Library", "Libreria de manipulacion de cadenas de texto.", 0},
            {"", "table", "table", "Library", "Libreria de manipulacion de arreglos y diccionarios.", 0},
            {"", "task", "task", "Library", "Libreria moderna para manejo de tiempo y corrutinas.", 0},
            {"", "coroutine", "coroutine", "Library", "Libreria nativa de manejo de hilos.", 0},
            {"", "utf8", "utf8", "Library", "Libreria para manejo de caracteres Unicode.", 0},
            {"", "bit32", "bit32", "Library", "Libreria para operaciones a nivel de bits.", 0},
            {"", "Enum", "Enum", "Library", "Coleccion de todos los valores enumerados.", 0},
            {"", "Vector2", "Vector2", "Library", "Operaciones de vectores 2D (UI y pantallas).", 0},
            {"", "Vector3", "Vector3", "Library", "Operaciones de vectores 3D (Mundo fisico).", 0},
            {"", "CFrame", "CFrame", "Library", "Matrices de posicion y rotacion 3D.", 0},
            {"", "Color3", "Color3", "Library", "Valores de color RGB.", 0},
            {"", "UDim", "UDim", "Library", "Dimension unidimensional para UI.", 0},
            {"", "UDim2", "UDim2", "Library", "Coordenadas completas para la Interfaz de Usuario.", 0},
            {"", "Instance", "Instance", "Library", "Creador maestro de objetos del motor.", 0},
            {"", "TweenInfo", "TweenInfo", "Library", "Configuracion de interpolaciones y animaciones.", 0},

            // LIBRERÍA: MATH
            {"math", "pi", "pi", "number", "Aproximacion matematica de Pi (3.14159...).", 4},
            {"math", "huge", "huge", "number", "Representa infinito positivo.", 4},
            {"math", "abs", "abs()", "(x: number) -> number", "Devuelve el valor absoluto de un numero.", 1},
            {"math", "acos", "acos()", "(x: number) -> number", "Arco coseno.", 1},
            {"math", "asin", "asin()", "(x: number) -> number", "Arco seno.", 1},
            {"math", "atan", "atan()", "(x: number) -> number", "Arco tangente.", 1},
            {"math", "atan2", "atan2()", "(y: number, x: number) -> number", "Arco tangente de y/x.", 1},
            {"math", "ceil", "ceil()", "(x: number) -> number", "Redondea hacia el entero mayor.", 1},
            {"math", "clamp", "clamp()", "(val: number, min: number, max: number) -> number", "Limita el valor entre un minimo y maximo.", 1},
            {"math", "cos", "cos()", "(x: number) -> number", "Coseno de un angulo en radianes.", 1},
            {"math", "cosh", "cosh()", "(x: number) -> number", "Coseno hiperbolico.", 1},
            {"math", "deg", "deg()", "(x: number) -> number", "Convierte radianes a grados.", 1},
            {"math", "exp", "exp()", "(x: number) -> number", "Devuelve e^x.", 1},
            {"math", "floor", "floor()", "(x: number) -> number", "Redondea hacia el entero menor.", 1},
            {"math", "fmod", "fmod()", "(x: number, y: number) -> number", "Resto de la division entera.", 1},
            {"math", "frexp", "frexp()", "(x: number) -> (number, number)", "Divide un numero en mantisa y exponente.", 1},
            {"math", "ldexp", "ldexp()", "(m: number, e: number) -> number", "Operacion inversa a frexp.", 1},
            {"math", "log", "log()", "(x: number, base: number?) -> number", "Logaritmo natural o en base especifica.", 1},
            {"math", "log10", "log10()", "(x: number) -> number", "Logaritmo en base 10.", 1},
            {"math", "max", "max()", "(...: number) -> number", "Devuelve el valor mas grande de los argumentos.", 1},
            {"math", "min", "min()", "(...: number) -> number", "Devuelve el valor mas pequeno de los argumentos.", 1},
            {"math", "modf", "modf()", "(x: number) -> (number, number)", "Separa en parte entera y fraccional.", 1},
            {"math", "noise", "noise()", "(x: number, y: number?, z: number?) -> number", "Genera ruido Perlin 1D, 2D o 3D.", 1},
            {"math", "pow", "pow()", "(x: number, y: number) -> number", "Eleva x a la potencia y.", 1},
            {"math", "rad", "rad()", "(x: number) -> number", "Convierte grados a radianes.", 1},
            {"math", "random", "random()", "(m: number?, n: number?) -> number", "Genera numero pseudoaleatorio.", 1},
            {"math", "randomseed", "randomseed()", "(seed: number) -> ()", "Establece la semilla para math.random.", 1},
            {"math", "round", "round()", "(x: number) -> number", "Redondea al entero mas cercano.", 1},
            {"math", "sign", "sign()", "(x: number) -> number", "Devuelve 1 si es positivo, -1 si negativo, 0 si cero.", 1},
            {"math", "sin", "sin()", "(x: number) -> number", "Seno de un angulo en radianes.", 1},
            {"math", "sinh", "sinh()", "(x: number) -> number", "Seno hiperbolico.", 1},
            {"math", "sqrt", "sqrt()", "(x: number) -> number", "Raiz cuadrada.", 1},
            {"math", "tan", "tan()", "(x: number) -> number", "Tangente de un angulo en radianes.", 1},
            {"math", "tanh", "tanh()", "(x: number) -> number", "Tangente hiperbolica.", 1},

            // LIBRERÍA: STRING
            {"string", "byte", "byte()", "(s: string, i: number?, j: number?) -> ...number", "Codigo numerico interno de los caracteres.", 1},
            {"string", "char", "char()", "(...: number) -> string", "Genera cadena a partir de codigos numericos.", 1},
            {"string", "find", "find()", "(s: string, pattern: string, init: number?, plain: boolean?) -> (number, number)", "Busca un patron en el string.", 1},
            {"string", "format", "format()", "(format: string, ...: any) -> string", "Formatea una cadena estilo C.", 1},
            {"string", "gmatch", "gmatch()", "(s: string, pattern: string) -> function", "Iterador de coincidencias de patrones.", 1},
            {"string", "gsub", "gsub()", "(s: string, pattern: string, repl: any, n: number?) -> (string, number)", "Reemplazo global de patrones.", 1},
            {"string", "len", "len()", "(s: string) -> number", "Longitud de la cadena.", 1},
            {"string", "lower", "lower()", "(s: string) -> string", "Convierte a minusculas.", 1},
            {"string", "match", "match()", "(s: string, pattern: string, init: number?) -> string?", "Devuelve la primera coincidencia del patron.", 1},
            {"string", "pack", "pack()", "(fmt: string, ...: any) -> string", "Empaqueta valores binarios.", 1},
            {"string", "packsize", "packsize()", "(fmt: string) -> number", "Tamano en bytes de un string empaquetado.", 1},
            {"string", "rep", "rep()", "(s: string, n: number) -> string", "Repite la cadena n veces.", 1},
            {"string", "reverse", "reverse()", "(s: string) -> string", "Invierte la cadena.", 1},
            {"string", "split", "split()", "(s: string, separator: string?) -> {string}", "Divide la cadena en un array.", 1},
            {"string", "sub", "sub()", "(s: string, i: number, j: number?) -> string", "Extrae una subcadena.", 1},
            {"string", "unpack", "unpack()", "(fmt: string, s: string, pos: number?) -> ...any", "Desempaqueta valores binarios.", 1},
            {"string", "upper", "upper()", "(s: string) -> string", "Convierte a mayusculas.", 1},

            // LIBRERÍA: TABLE
            {"table", "clear", "clear()", "(t: table) -> ()", "Vacia la tabla rapido sin reasignar memoria.", 1},
            {"table", "clone", "clone()", "(t: table) -> table", "Devuelve una copia superficial de la tabla.", 1},
            {"table", "concat", "concat()", "(t: table, sep: string?, i: number?, j: number?) -> string", "Concatena elementos en un string.", 1},
            {"table", "create", "create()", "(count: number, value: any?) -> table", "Crea una tabla pre-asignada.", 1},
            {"table", "find", "find()", "(haystack: table, needle: any, init: number?) -> number?", "Busca linealmente un valor en un array.", 1},
            {"table", "freeze", "freeze()", "(t: table) -> table", "Hace la tabla de solo lectura (inmutable).", 1},
            {"table", "insert", "insert()", "(t: table, pos: number?, value: any) -> ()", "Inserta un valor en el array.", 1},
            {"table", "isfrozen", "isfrozen()", "(t: table) -> boolean", "Comprueba si la tabla esta congelada.", 1},
            {"table", "maxn", "maxn()", "(t: table) -> number", "Devuelve la clave numerica positiva mayor.", 1},
            {"table", "move", "move()", "(a1: table, f: number, e: number, t: number, a2: table?) -> table", "Mueve elementos entre tablas.", 1},
            {"table", "pack", "pack()", "(...: any) -> table", "Empaqueta argumentos en una tabla con propiedad n.", 1},
            {"table", "remove", "remove()", "(t: table, pos: number?) -> any", "Elimina un elemento del array.", 1},
            {"table", "sort", "sort()", "(t: table, comp: function?) -> ()", "Ordena el array in-place usando QuickSort.", 1},
            {"table", "unpack", "unpack()", "(list: table, i: number?, j: number?) -> ...any", "Desempaqueta elementos del array.", 1},

            // LIBRERÍA: TASK
            {"task", "cancel", "cancel()", "(thread: thread) -> ()", "Cancela una corrutina pendiente en el administrador.", 1},
            {"task", "defer", "defer()", "(f: any, ...: any) -> thread", "Programa para ejecutar al final del ciclo de frame.", 1},
            {"task", "delay", "delay()", "(sec: number, f: any, ...: any) -> thread", "Ejecuta despues de cierto tiempo optimizado.", 1},
            {"task", "desynchronize", "desynchronize()", "() -> ()", "Cambia la corrutina a ejecucion paralela.", 1},
            {"task", "spawn", "spawn()", "(f: any, ...: any) -> thread", "Ejecuta inmediatamente en un nuevo hilo.", 1},
            {"task", "synchronize", "synchronize()", "() -> ()", "Cambia la corrutina a ejecucion serial (hilo principal).", 1},
            {"task", "wait", "wait()", "(sec: number?) -> number", "Pausa el hilo sincronizado con el motor fisico.", 1},

            // LIBRERÍA: COROUTINE
            {"coroutine", "create", "create()", "(f: function) -> thread", "Crea una nueva corrutina.", 1},
            {"coroutine", "isyieldable", "isyieldable()", "() -> boolean", "Verifica si la corrutina actual puede pausarse.", 1},
            {"coroutine", "resume", "resume()", "(co: thread, ...: any) -> (boolean, ...any)", "Inicia o continua una corrutina.", 1},
            {"coroutine", "running", "running()", "() -> thread", "Devuelve el hilo en ejecucion actual.", 1},
            {"coroutine", "status", "status()", "(co: thread) -> string", "Devuelve el estado (dead, suspended, running).", 1},
            {"coroutine", "wrap", "wrap()", "(f: function) -> function", "Crea corrutina y devuelve funcion para ejecutarla.", 1},
            {"coroutine", "yield", "yield()", "(...: any) -> ...any", "Pausa la ejecucion de la corrutina actual.", 1},

            // CLASES: VECTOR3
            {"Vector3", "new", "new()", "(x: number?, y: number?, z: number?) -> Vector3", "Crea un nuevo vector tridimensional.", 1},
            {"Vector3", "zero", "zero", "Vector3", "Vector3(0, 0, 0)", 4},
            {"Vector3", "one", "one", "Vector3", "Vector3(1, 1, 1)", 4},
            {"Vector3", "xAxis", "xAxis", "Vector3", "Vector3(1, 0, 0)", 4},
            {"Vector3", "yAxis", "yAxis", "Vector3", "Vector3(0, 1, 0)", 4},
            {"Vector3", "zAxis", "zAxis", "Vector3", "Vector3(0, 0, 1)", 4},
            {"Vector3", "Cross", "Cross()", "(other: Vector3) -> Vector3", "Producto cruzado de vectores.", 1},
            {"Vector3", "Dot", "Dot()", "(other: Vector3) -> number", "Producto punto escalar.", 1},
            {"Vector3", "Lerp", "Lerp()", "(goal: Vector3, alpha: number) -> Vector3", "Interpolacion lineal.", 1},
            {"Vector3", "Magnitude", "Magnitude", "number", "Longitud total del vector espacial.", 4},
            {"Vector3", "Unit", "Unit", "Vector3", "Vector normalizado (longitud exacta de 1).", 4},
            {"Vector3", "X", "X", "number", "Componente X del vector.", 4},
            {"Vector3", "Y", "Y", "number", "Componente Y del vector.", 4},
            {"Vector3", "Z", "Z", "number", "Componente Z del vector.", 4},

            // CLASES: VECTOR2
            {"Vector2", "new", "new()", "(x: number?, y: number?) -> Vector2", "Crea un nuevo vector bidimensional.", 1},
            {"Vector2", "zero", "zero", "Vector2", "Vector2(0, 0)", 4},
            {"Vector2", "one", "one", "Vector2", "Vector2(1, 1)", 4},
            {"Vector2", "xAxis", "xAxis", "Vector2", "Vector2(1, 0)", 4},
            {"Vector2", "yAxis", "yAxis", "Vector2", "Vector2(0, 1)", 4},
            {"Vector2", "Cross", "Cross()", "(other: Vector2) -> number", "Producto cruzado 2D.", 1},
            {"Vector2", "Dot", "Dot()", "(other: Vector2) -> number", "Producto punto 2D.", 1},
            {"Vector2", "Lerp", "Lerp()", "(goal: Vector2, alpha: number) -> Vector2", "Interpolacion lineal 2D.", 1},
            {"Vector2", "Magnitude", "Magnitude", "number", "Longitud del vector 2D.", 4},
            {"Vector2", "Unit", "Unit", "Vector2", "Vector 2D normalizado.", 4},
            {"Vector2", "X", "X", "number", "Componente X del vector 2D.", 4},
            {"Vector2", "Y", "Y", "number", "Componente Y del vector 2D.", 4},

            // CLASES: CFRAME
            {"CFrame", "new", "new()", "(x: number?, y: number?, z: number?) -> CFrame", "Crea coordenada espacial de matriz 4x4.", 1},
            {"CFrame", "Angles", "Angles()", "(rx: number, ry: number, rz: number) -> CFrame", "Rotacion usando Radianes XYZ.", 1},
            {"CFrame", "fromAxisAngle", "fromAxisAngle()", "(v: Vector3, r: number) -> CFrame", "Rotacion alrededor de un eje vectorial.", 1},
            {"CFrame", "fromEulerAnglesXYZ", "fromEulerAnglesXYZ()", "(rx: number, ry: number, rz: number) -> CFrame", "Rotacion en orden X, Y, Z.", 1},
            {"CFrame", "fromEulerAnglesYXZ", "fromEulerAnglesYXZ()", "(rx: number, ry: number, rz: number) -> CFrame", "Rotacion en orden Y, X, Z.", 1},
            {"CFrame", "fromMatrix", "fromMatrix()", "(pos: Vector3, vX: Vector3, vY: Vector3, vZ: Vector3?) -> CFrame", "Construye a partir de matriz direccional.", 1},
            {"CFrame", "lookAt", "lookAt()", "(at: Vector3, lookAt: Vector3, up: Vector3?) -> CFrame", "Apunta hacia una posicion especifica.", 1},
            {"CFrame", "identity", "identity", "CFrame", "CFrame nulo sin traslacion ni rotacion.", 4},
            {"CFrame", "Position", "Position", "Vector3", "Extrae la posicion vectorial absoluta.", 4},
            {"CFrame", "Rotation", "Rotation", "CFrame", "Extrae solo la matriz de rotacion limpia.", 4},
            {"CFrame", "LookVector", "LookVector", "Vector3", "Eje direccional Z (Hacia adelante).", 4},
            {"CFrame", "UpVector", "UpVector", "Vector3", "Eje direccional Y (Arriba).", 4},
            {"CFrame", "RightVector", "RightVector", "Vector3", "Eje direccional X (Derecha).", 4},
            {"CFrame", "XVector", "XVector", "Vector3", "Alias para RightVector.", 4},
            {"CFrame", "YVector", "YVector", "Vector3", "Alias para UpVector.", 4},
            {"CFrame", "ZVector", "ZVector", "Vector3", "Alias para LookVector negado.", 4},
            {"CFrame", "Inverse", "Inverse()", "() -> CFrame", "Matriz inversa util para relatividad.", 1},
            {"CFrame", "Lerp", "Lerp()", "(goal: CFrame, alpha: number) -> CFrame", "Interpolacion esferica lineal suave.", 1},
            {"CFrame", "ToWorldSpace", "ToWorldSpace()", "(cf: CFrame) -> CFrame", "Transforma un CFrame relativo a global.", 1},
            {"CFrame", "ToObjectSpace", "ToObjectSpace()", "(cf: CFrame) -> CFrame", "Transforma un CFrame global a relativo.", 1},

            // CLASES: COLOR3
            {"Color3", "new", "new()", "(r: number?, g: number?, b: number?) -> Color3", "Crea color usando escala flotante 0 a 1.", 1},
            {"Color3", "fromRGB", "fromRGB()", "(r: number, g: number, b: number) -> Color3", "Crea color usando bytes de 0 a 255.", 1},
            {"Color3", "fromHSV", "fromHSV()", "(h: number, s: number, v: number) -> Color3", "Crea color usando Hue, Saturation, Value.", 1},
            {"Color3", "fromHex", "fromHex()", "(hex: string) -> Color3", "Crea color desde codigo Hexadecimal web.", 1},
            {"Color3", "R", "R", "number", "Componente Rojo flotante.", 4},
            {"Color3", "G", "G", "number", "Componente Verde flotante.", 4},
            {"Color3", "B", "B", "number", "Componente Azul flotante.", 4},
            {"Color3", "Lerp", "Lerp()", "(goal: Color3, alpha: number) -> Color3", "Fusiona linealmente dos colores.", 1},
            {"Color3", "ToHSV", "ToHSV()", "() -> (number, number, number)", "Extrae el Hue, Saturation y Value.", 1},
            {"Color3", "ToHex", "ToHex()", "() -> string", "Convierte el color a Hexadecimal.", 1},

            // CLASES: UDIM Y UDIM2 (INTERFAZ DE USUARIO)
            {"UDim", "new", "new()", "(scale: number, offset: number) -> UDim", "Crea dimension de UI unidimensional.", 1},
            {"UDim", "Scale", "Scale", "number", "Proporcion relativa a la pantalla (0-1).", 4},
            {"UDim", "Offset", "Offset", "number", "Proporcion absoluta en pixeles.", 4},
            {"UDim2", "new", "new()", "(xScale: number, xOffset: number, yScale: number, yOffset: number) -> UDim2", "Dimension completa 2D para Gui.", 1},
            {"UDim2", "fromScale", "fromScale()", "(x: number, y: number) -> UDim2", "Crea UDim2 solo con valores de escala.", 1},
            {"UDim2", "fromOffset", "fromOffset()", "(x: number, y: number) -> UDim2", "Crea UDim2 solo con valores de pixeles.", 1},
            {"UDim2", "X", "X", "UDim", "Componente X del UDim2.", 4},
            {"UDim2", "Y", "Y", "UDim", "Componente Y del UDim2.", 4},
            {"UDim2", "Lerp", "Lerp()", "(goal: UDim2, alpha: number) -> UDim2", "Anima la posicion de un elemento UI.", 1},

            // CLASES: TWEENINFO (ANIMACIONES)
            {"TweenInfo", "new", "new()", "(time: number, easingStyle: Enum, easingDirection: Enum, repeatCount: number, reverses: boolean, delayTime: number) -> TweenInfo", "Configura una animacion interpolada.", 1},
            
            // SERVICIOS DEL MOTOR Y DATA MODEL (game:GetService("...") o game.)
            {"game", "Workspace", "Workspace", "Workspace", "[Service] El entorno fisico del juego.", 4},
            {"game", "Players", "Players", "Players", "[Service] Maneja jugadores conectados.", 4},
            {"game", "Lighting", "Lighting", "Lighting", "[Service] Efectos de iluminacion y cielo.", 4},
            {"game", "ReplicatedStorage", "ReplicatedStorage", "ReplicatedStorage", "[Service] Contenido visible por Server y Client.", 4},
            {"game", "ReplicatedFirst", "ReplicatedFirst", "ReplicatedFirst", "[Service] Se carga antes que el resto de la escena.", 4},
            {"game", "ServerScriptService", "ServerScriptService", "ServerScriptService", "[Service] Scripts seguros de backend.", 4},
            {"game", "ServerStorage", "ServerStorage", "ServerStorage", "[Service] Objetos seguros inaccesibles al cliente.", 4},
            {"game", "StarterGui", "StarterGui", "StarterGui", "[Service] Interfaces predeterminadas de jugadores.", 4},
            {"game", "StarterPlayer", "StarterPlayer", "StarterPlayer", "[Service] Configuracion inicial de avatares.", 4},
            {"game", "StarterPack", "StarterPack", "StarterPack", "[Service] Items de inventario del jugador.", 4},
            {"game", "RunService", "RunService", "RunService", "[Service] Eventos por frame: Heartbeat, RenderStepped, Stepped.", 4},
            {"game", "TweenService", "TweenService", "TweenService", "[Service] Motor principal de animaciones suaves.", 4},
            {"game", "HttpService", "HttpService", "HttpService", "[Service] Peticiones Web y JSON.", 4},
            {"game", "Debris", "Debris", "Debris", "[Service] Limpieza automatica de objetos temporales.", 4},
            {"game", "CollectionService", "CollectionService", "CollectionService", "[Service] Sistema de tags en instancias.", 4},
            {"game", "DataStoreService", "DataStoreService", "DataStoreService", "[Service] Persistencia de datos de jugadores.", 4},
            {"game", "ContextActionService", "ContextActionService", "ContextActionService", "[Service] Binding de acciones a inputs.", 4},
            {"game", "PathfindingService", "PathfindingService", "PathfindingService", "[Service] Calculo de rutas de navegacion (pathfinding).", 4},
            {"game", "PhysicsService", "PhysicsService", "PhysicsService", "[Service] Grupos y filtros de colision fisica.", 4},
            {"game", "TeleportService", "TeleportService", "TeleportService", "[Service] Teletransporte entre Places y servidores.", 4},
            {"game", "BadgeService", "BadgeService", "BadgeService", "[Service] Otorgar y verificar medallas de jugadores.", 4},
            {"game", "MarketplaceService", "MarketplaceService", "MarketplaceService", "[Service] Compras en juego y GamePasses.", 4},
            {"game", "GuiService", "GuiService", "GuiService", "[Service] Control global de interfaces de usuario.", 4},
            {"game", "InsertService", "InsertService", "InsertService", "[Service] Carga de assets externos en tiempo real.", 4},
            {"game", "ScriptContext", "ScriptContext", "ScriptContext", "[Service] Contexto y errores de scripts en ejecucion.", 4},
            {"game", "SoundService", "SoundService", "SoundService", "[Service] Control del audio global del juego.", 4},
            {"game", "TextChatService", "TextChatService", "TextChatService", "[Service] Sistema de chat de texto en juego.", 4},
            {"game", "Teams", "Teams", "Teams", "[Service] Gestion de equipos y colores.", 4},
            {"game", "UserInputService", "UserInputService", "UserInputService", "[Service] Captura de input de teclado y mouse.", 4},
            {"game", "GetService", "GetService(\"\")", "(className: string) -> Instance", "[Method] Obtiene un servicio seguro del motor.", 1},
            {"game", "IsLoaded", "IsLoaded()", "() -> boolean", "[Method] Verifica si todos los assets han cargado.", 1},
            {"game", "GetJobsInfo", "GetJobsInfo()", "() -> table", "[Method] Estadisticas de rendimiento del motor.", 1},

            // RUNSERVICE (game:GetService("RunService").)
            {"RunService", "Heartbeat", "Heartbeat", "RBXScriptSignal", "[Event] Cada frame despues de la fisica. Argumentos: (deltaTime: number)", 2},
            {"RunService", "RenderStepped", "RenderStepped", "RBXScriptSignal", "[Event] Cada frame antes de renderizar. Argumentos: (deltaTime: number)", 2},
            {"RunService", "Stepped", "Stepped", "RBXScriptSignal", "[Event] Cada paso de simulacion fisica. Argumentos: (time: number, deltaTime: number)", 2},
            {"RunService", "IsRunning", "IsRunning()", "() -> boolean", "[Method] true si el juego esta en ejecucion (no en editor).", 1},
            {"RunService", "IsClient", "IsClient()", "() -> boolean", "[Method] true si se ejecuta en el cliente.", 1},
            {"RunService", "IsServer", "IsServer()", "() -> boolean", "[Method] true si se ejecuta en el servidor.", 1},
            {"RunService", "IsStudio", "IsStudio()", "() -> boolean", "[Method] true si se ejecuta en el editor de Godot.", 1},
            {"RunService", "BindToRenderStep", "BindToRenderStep()", "(name: string, priority: number, fn: function) -> ()", "[Method] Vincula funcion al paso de renderizado con prioridad.", 1},
            {"RunService", "UnbindFromRenderStep", "UnbindFromRenderStep()", "(name: string) -> ()", "[Method] Desvincula funcion del paso de renderizado.", 1},

            // USERINPUTSERVICE (game:GetService("UserInputService").)
            {"UserInputService", "IsKeyDown", "IsKeyDown()", "(keyCode: string) -> boolean", "[Method] Verifica si una tecla esta presionada.", 1},
            {"UserInputService", "IsMouseButtonPressed", "IsMouseButtonPressed()", "(button: number) -> boolean", "[Method] Verifica si un boton del mouse esta presionado.", 1},
            {"UserInputService", "GetMouseDelta", "GetMouseDelta()", "() -> {X: number, Y: number}", "[Method] Devuelve el movimiento del mouse en el frame.", 1},
            {"UserInputService", "InputBegan", "InputBegan", "RBXScriptSignal", "[Event] Se dispara cuando se presiona cualquier input.", 2},
            {"UserInputService", "InputEnded", "InputEnded", "RBXScriptSignal", "[Event] Se dispara cuando se suelta cualquier input.", 2},
            {"UserInputService", "InputChanged", "InputChanged", "RBXScriptSignal", "[Event] Se dispara cuando cambia un input (mouse move).", 2},
            {"UserInputService", "MouseBehavior", "MouseBehavior", "Enum.MouseBehavior", "[Property] Controla si el mouse esta bloqueado o libre.", 4},
            {"UserInputService", "MouseEnabled", "MouseEnabled", "boolean", "[Property] Si el mouse fisico esta disponible.", 4},
            {"UserInputService", "KeyboardEnabled", "KeyboardEnabled", "boolean", "[Property] Si el teclado fisico esta disponible.", 4},

            // SOUNDSERVICE (game:GetService("SoundService").)
            {"SoundService", "SetListener", "SetListener()", "(listenerType: Enum, ...: any) -> ()", "[Method] Establece la posicion del oyente de audio.", 1},
            {"SoundService", "PlayLocalSound", "PlayLocalSound()", "(sound: Sound) -> ()", "[Method] Reproduce un sonido en el cliente local.", 1},

            // TWEENSERVICE (game:GetService("TweenService").)
            {"TweenService", "Create", "Create()", "(instance: Instance, tweenInfo: TweenInfo, goals: table) -> Tween", "[Method] Crea una animacion interpolada de propiedades.", 1},

            // DEBRIS (game:GetService("Debris").)
            {"Debris", "AddItem", "AddItem()", "(instance: Instance, lifetime: number?) -> ()", "[Method] Elimina automaticamente la instancia tras N segundos.", 1},

            // COLLECTIONSERVICE (game:GetService("CollectionService").)
            {"CollectionService", "AddTag", "AddTag()", "(instance: Instance, tag: string) -> ()", "[Method] Asigna un tag a la instancia.", 1},
            {"CollectionService", "RemoveTag", "RemoveTag()", "(instance: Instance, tag: string) -> ()", "[Method] Elimina un tag de la instancia.", 1},
            {"CollectionService", "HasTag", "HasTag()", "(instance: Instance, tag: string) -> boolean", "[Method] Verifica si la instancia tiene el tag.", 1},
            {"CollectionService", "GetTagged", "GetTagged()", "(tag: string) -> {Instance}", "[Method] Devuelve todas las instancias con ese tag.", 1},
            {"CollectionService", "GetInstanceAddedSignal", "GetInstanceAddedSignal()", "(tag: string) -> RBXScriptSignal", "[Method] Se dispara cuando se agrega el tag a una instancia.", 1},
            {"CollectionService", "GetInstanceRemovedSignal", "GetInstanceRemovedSignal()", "(tag: string) -> RBXScriptSignal", "[Method] Se dispara cuando se elimina el tag de una instancia.", 1},

            // DATASTORESERVICE (game:GetService("DataStoreService").)
            {"DataStoreService", "GetDataStore", "GetDataStore()", "(name: string, scope: string?) -> DataStore", "[Method] Obtiene o crea un DataStore con el nombre dado.", 1},
            {"DataStoreService", "GetOrderedDataStore", "GetOrderedDataStore()", "(name: string, scope: string?) -> OrderedDataStore", "[Method] DataStore ordenado numericamente.", 1},
            {"DataStoreService", "GetGlobalDataStore", "GetGlobalDataStore()", "() -> GlobalDataStore", "[Method] DataStore global del juego.", 1},

            // HTTPSERVICE (game:GetService("HttpService").)
            {"HttpService", "JSONEncode", "JSONEncode()", "(value: any) -> string", "[Method] Serializa una tabla Lua a JSON.", 1},
            {"HttpService", "JSONDecode", "JSONDecode()", "(json: string) -> any", "[Method] Deserializa JSON a tabla Lua.", 1},
            {"HttpService", "GetAsync", "GetAsync()", "(url: string, noCache: boolean?, headers: table?) -> string", "[Method] Peticion HTTP GET.", 1},
            {"HttpService", "PostAsync", "PostAsync()", "(url: string, data: string, contentType: string?, compress: boolean?, headers: table?) -> string", "[Method] Peticion HTTP POST.", 1},
            {"HttpService", "RequestAsync", "RequestAsync()", "(requestOptions: table) -> table", "[Method] Peticion HTTP avanzada con control total.", 1},
            {"HttpService", "GenerateGUID", "GenerateGUID()", "(wrapInCurlyBraces: boolean?) -> string", "[Method] Genera un GUID unico.", 1},
            {"HttpService", "UrlEncode", "UrlEncode()", "(input: string) -> string", "[Method] Codifica una cadena para URLs.", 1},

            // CONTEXTACTIONSERVICE (game:GetService("ContextActionService").)
            {"ContextActionService", "BindAction", "BindAction()", "(name: string, func: function, createButton: boolean, ...: any) -> ()", "[Method] Vincula funcion a teclas de input.", 1},
            {"ContextActionService", "UnbindAction", "UnbindAction()", "(name: string) -> ()", "[Method] Desvincula una accion registrada.", 1},
            {"ContextActionService", "BindActionAtPriority", "BindActionAtPriority()", "(name: string, func: function, createButton: boolean, priority: number, ...: any) -> ()", "[Method] Vincula con prioridad explicita.", 1},
            {"ContextActionService", "GetBoundActionInfo", "GetBoundActionInfo()", "(name: string) -> table", "[Method] Info de una accion registrada.", 1},
            {"ContextActionService", "SetTitle", "SetTitle()", "(name: string, title: string) -> ()", "[Method] Cambia el titulo del boton de accion.", 1},

            // PATHFINDINGSERVICE (game:GetService("PathfindingService").)
            {"PathfindingService", "CreatePath", "CreatePath()", "(agentParameters: table?) -> Path", "[Method] Crea una ruta de navegacion.", 1},

            // PHYSICSSERVICE (game:GetService("PhysicsService").)
            {"PhysicsService", "RegisterCollisionGroup", "RegisterCollisionGroup()", "(name: string) -> ()", "[Method] Crea un nuevo grupo de colision.", 1},
            {"PhysicsService", "CollisionGroupSetCollidable", "CollisionGroupSetCollidable()", "(name1: string, name2: string, collidable: boolean) -> ()", "[Method] Controla si dos grupos colisionan.", 1},
            {"PhysicsService", "SetPartCollisionGroup", "SetPartCollisionGroup()", "(part: BasePart, groupName: string) -> ()", "[Method] Asigna un grupo de colision a una parte.", 1},
            {"PhysicsService", "CollisionGroupsAreCollidable", "CollisionGroupsAreCollidable()", "(name1: string, name2: string) -> boolean", "[Method] Verifica si dos grupos colisionan.", 1},

            // TELEPORTSERVICE (game:GetService("TeleportService").)
            {"TeleportService", "Teleport", "Teleport()", "(placeId: number, player: Player?, teleportData: table?, customLoadingScreen: ScreenGui?) -> ()", "[Method] Teletransporta a otro Place.", 1},
            {"TeleportService", "TeleportAsync", "TeleportAsync()", "(placeId: number, players: {Player}, teleportOptions: TeleportOptions?) -> TeleportAsyncResult", "[Method] Teletransporta grupo de jugadores.", 1},
            {"TeleportService", "GetLocalPlayerTeleportData", "GetLocalPlayerTeleportData()", "() -> any", "[Method] Datos enviados al teletransportar.", 1},

            // BADGESERVICE (game:GetService("BadgeService").)
            {"BadgeService", "AwardBadge", "AwardBadge()", "(userId: number, badgeId: number) -> boolean", "[Method] Otorga una medalla al jugador.", 1},
            {"BadgeService", "UserHasBadgeAsync", "UserHasBadgeAsync()", "(userId: number, badgeId: number) -> boolean", "[Method] Verifica si el jugador tiene la medalla.", 1},
            {"BadgeService", "GetBadgeInfoAsync", "GetBadgeInfoAsync()", "(badgeId: number) -> table", "[Method] Obtiene info de la medalla.", 1},

            // MARKETPLACESERVICE (game:GetService("MarketplaceService").)
            {"MarketplaceService", "PromptProductPurchase", "PromptProductPurchase()", "(player: Player, productId: number) -> ()", "[Method] Abre dialogo de compra.", 1},
            {"MarketplaceService", "PromptGamePassPurchase", "PromptGamePassPurchase()", "(player: Player, gamePassId: number) -> ()", "[Method] Abre dialogo de compra de gamepass.", 1},
            {"MarketplaceService", "UserOwnsGamePassAsync", "UserOwnsGamePassAsync()", "(userId: number, gamePassId: number) -> boolean", "[Method] Verifica si el jugador tiene el gamepass.", 1},
            {"MarketplaceService", "GetProductInfo", "GetProductInfo()", "(assetId: number, infoType: Enum.InfoType?) -> table", "[Method] Info del producto o asset.", 1},
            {"MarketplaceService", "ProcessReceipt", "ProcessReceipt", "function", "[Property] Callback de procesamiento de compras.", 4},

            // GUISERVICE (game:GetService("GuiService").)
            {"GuiService", "GetScreenResolution", "GetScreenResolution()", "() -> Vector2", "[Method] Resolucion de pantalla del cliente.", 1},
            {"GuiService", "IsTenFootInterface", "IsTenFootInterface()", "() -> boolean", "[Method] True si es interfaz de consola (TV).", 1},
            {"GuiService", "MenuOpened", "MenuOpened", "RBXScriptSignal", "[Event] Menu de Roblox abierto.", 2},
            {"GuiService", "MenuClosed", "MenuClosed", "RBXScriptSignal", "[Event] Menu de Roblox cerrado.", 2},

            // INSERTSERVICE (game:GetService("InsertService").)
            {"InsertService", "LoadAsset", "LoadAsset()", "(assetId: number) -> Instance", "[Method] Carga un asset de Roblox por ID y lo devuelve.", 1},
            {"InsertService", "LoadAssetVersion", "LoadAssetVersion()", "(assetVersionId: number) -> Instance", "[Method] Carga una version especifica de un asset.", 1},
            {"InsertService", "GetFreeDecals", "GetFreeDecals()", "(searchText: string, pageNum: number?) -> table", "[Method] Busca texturas libres en la biblioteca.", 1},
            {"InsertService", "GetFreeModels", "GetFreeModels()", "(searchText: string, pageNum: number?) -> table", "[Method] Busca modelos libres en la biblioteca.", 1},

            // SCRIPTCONTEXT (game:GetService("ScriptContext").)
            {"ScriptContext", "Error", "Error", "RBXScriptSignal", "[Event] Se dispara cuando ocurre un error en un script. Args: (message, stackTrace, script)", 2},
            {"ScriptContext", "AddCoreScriptLocal", "AddCoreScriptLocal()", "(url: string, parent: Instance) -> ()", "[Method] Agrega un script de nucleo local.", 1},

            // WORKSPACE (workspace.)
            {"Workspace", "CurrentCamera", "CurrentCamera", "Camera", "[Property] Camara activa del cliente local.", 4},
            {"Workspace", "Gravity", "Gravity", "number", "[Property] Fuerza de atraccion gravitacional (default 196.2).", 4},
            {"Workspace", "FallenPartsDestroyHeight", "FallenPartsDestroyHeight", "number", "[Property] Limite de caida al vacio.", 4},
            {"Workspace", "Terrain", "Terrain", "Terrain", "[Property] Objeto de terreno por voxeles.", 4},
            {"Workspace", "GetServerTimeNow", "GetServerTimeNow()", "() -> number", "[Method] Tiempo sincronizado del servidor.", 1},
            {"Workspace", "Raycast", "Raycast()", "(origin: Vector3, direction: Vector3, params: RaycastParams?) -> RaycastResult?", "[Method] Lanza un rayo fisico.", 1},
            {"Workspace", "GetPartsInPart", "GetPartsInPart()", "(part: BasePart, params: OverlapParams?) -> {BasePart}", "[Method] Detecta colisiones espaciales.", 1},

            // PLAYERS (Players.)
            {"Players", "LocalPlayer", "LocalPlayer", "Player", "[Property] El jugador en el cliente actual.", 4},
            {"Players", "MaxPlayers", "MaxPlayers", "number", "[Property] Limite de jugadores en el servidor.", 4},
            {"Players", "PlayerAdded", "PlayerAdded", "RBXScriptSignal", "[Event] Se dispara cuando entra un jugador.", 2},
            {"Players", "PlayerRemoving", "PlayerRemoving", "RBXScriptSignal", "[Event] Se dispara cuando sale un jugador.", 2},
            {"Players", "GetPlayers", "GetPlayers()", "() -> {Player}", "[Method] Devuelve array con los jugadores activos.", 1},
            {"Players", "GetPlayerFromCharacter", "GetPlayerFromCharacter()", "(character: Model) -> Player?", "[Method] Extrae jugador desde su modelo 3D.", 1},

            // INSTANCE SUPERCLASE (Aplica a todas las variables deducidas como Instance)
            {"Instance", "new", "new()", "(className: string, parent: Instance?) -> Instance", "Instancia dinamicamente una clase nativa.", 1},
            {"Instance", "Name", "Name", "string", "[Property] Identificador del nodo en el Explorador.", 4},
            {"Instance", "Parent", "Parent", "Instance", "[Property] Nodo contenedor superior (Padre).", 4},
            {"Instance", "ClassName", "ClassName", "string", "[Property] El tipo nativo exacto de la instancia.", 4},
            {"Instance", "Destroy", "Destroy()", "() -> ()", "[Method] Borra permanentemente de la memoria.", 1},
            {"Instance", "Clone", "Clone()", "() -> Instance", "[Method] Copia profunda exacta en memoria.", 1},
            {"Instance", "GetChildren", "GetChildren()", "() -> {Instance}", "[Method] Retorna un array con subnodos directos.", 1},
            {"Instance", "GetDescendants", "GetDescendants()", "() -> {Instance}", "[Method] Retorna el arbol completo de hijos.", 1},
            {"Instance", "FindFirstChild", "FindFirstChild()", "(name: string, recursive: boolean?) -> Instance?", "[Method] Busca un nodo hijo directo.", 1},
            {"Instance", "FindFirstChildOfClass", "FindFirstChildOfClass()", "(className: string) -> Instance?", "[Method] Busca por tipo de clase.", 1},
            {"Instance", "FindFirstAncestor", "FindFirstAncestor()", "(name: string) -> Instance?", "[Method] Sube el arbol buscando un padre.", 1},
            {"Instance", "WaitForChild", "WaitForChild()", "(name: string, timeOut: number?) -> Instance?", "[Method] Espera sincronamente a que cargue el nodo.", 1},
            {"Instance", "IsA", "IsA()", "(className: string) -> boolean", "[Method] Verifica herencia de clase fuerte.", 1},
            {"Instance", "GetAttribute", "GetAttribute()", "(attribute: string) -> any", "[Method] Obtiene valor personalizado.", 1},
            {"Instance", "SetAttribute", "SetAttribute()", "(attribute: string, value: any) -> ()", "[Method] Define un valor personalizado.", 1},
            {"Instance", "ChildAdded", "ChildAdded", "RBXScriptSignal", "[Event] Disparado al insertar un subnodo.", 2},
            {"Instance", "ChildRemoved", "ChildRemoved", "RBXScriptSignal", "[Event] Disparado al borrar un subnodo.", 2},
            {"Instance", "AncestryChanged", "AncestryChanged", "RBXScriptSignal", "[Event] Disparado al cambiar de Padre.", 2},
            
            // BASEPART (Extiende de Instance, aplica si se asume fisica)
            {"Instance", "Position", "Position", "Vector3", "[Property] Posicion central de la parte en 3D.", 4},
            {"Instance", "Size", "Size", "Vector3", "[Property] Tamano volumetrico de la caja colisionadora.", 4},
            {"Instance", "CFrame", "CFrame", "CFrame", "[Property] Matriz de transformacion absoluta.", 4},
            {"Instance", "Color", "Color", "Color3", "[Property] Tinte visual del material de la parte.", 4},
            {"Instance", "Transparency", "Transparency", "number", "[Property] Nivel de invisibilidad de 0.0 a 1.0.", 4},
            {"Instance", "Anchored", "Anchored", "boolean", "[Property] Inmoviliza el objeto ignorando gravedad.", 4},
            {"Instance", "CanCollide", "CanCollide", "boolean", "[Property] Habilita colision solida con el mundo.", 4},
            {"Instance", "Massless", "Massless", "boolean", "[Property] Elimina la contribucion de masa al modelo.", 4},
            {"Instance", "Material", "Material", "Enum.Material", "[Property] Textura fisica y visual del bloque.", 4},
            {"Instance", "Velocity", "Velocity", "Vector3", "[Property] Vector de movimiento inercial actual.", 4},
            {"Instance", "AssemblyLinearVelocity", "AssemblyLinearVelocity", "Vector3", "[Property] Velocidad lineal del ensamblaje.", 4},
            {"Instance", "Touched", "Touched", "RBXScriptSignal", "[Event] Disparado al intersecar con un colisionador.", 2},
            {"Instance", "TouchEnded", "TouchEnded", "RBXScriptSignal", "[Event] Disparado al separarse de un colisionador.", 2},
            {"Instance", "ApplyImpulse", "ApplyImpulse()", "(impulse: Vector3) -> ()", "[Method] Aplica fuerza instantanea al centro de masa.", 1},
            {"Instance", "GetMass", "GetMass()", "() -> number", "[Method] Devuelve la masa total de la parte.", 1},

            // MODEL (Extiende de Instance, util para Characters)
            {"Instance", "PrimaryPart", "PrimaryPart", "BasePart", "[Property] Parte raiz que guia todo el modelo.", 4},
            {"Instance", "WorldPivot", "WorldPivot", "CFrame", "[Property] Punto de pivote global del modelo.", 4},
            {"Instance", "MoveTo", "MoveTo()", "(position: Vector3) -> ()", "[Method] Teletransporta el modelo a un Vector3.", 1},
            {"Instance", "PivotTo", "PivotTo()", "(cframe: CFrame) -> ()", "[Method] Gira y transporta usando CFrame y el pivote.", 1},
            {"Instance", "GetPivot", "GetPivot()", "() -> CFrame", "[Method] Obtiene el CFrame del pivote actual.", 1},
            {"Instance", "GetBoundingBox", "GetBoundingBox()", "() -> (CFrame, Vector3)", "[Method] Devuelve la caja que encierra al modelo.", 1},

            // HUMANOID (Extiende de Instance, util para personajes)
            {"Instance", "Health", "Health", "number", "[Property] Puntos de vida actuales del personaje.", 4},
            {"Instance", "MaxHealth", "MaxHealth", "number", "[Property] Puntos de vida maximos permitidos.", 4},
            {"Instance", "WalkSpeed", "WalkSpeed", "number", "[Property] Velocidad de movimiento del personaje.", 4},
            {"Instance", "JumpPower", "JumpPower", "number", "[Property] Fuerza bruta del salto (si no usa JumpHeight).", 4},
            {"Instance", "Sit", "Sit", "boolean", "[Property] Determina si el humanoide esta sentado.", 4},
            {"Instance", "PlatformStand", "PlatformStand", "boolean", "[Property] Deshabilita colisiones del torso inferior.", 4},
            {"Instance", "TakeDamage", "TakeDamage()", "(amount: number) -> ()", "[Method] Resta salud si no tiene un escudo (ForceField).", 1},
            {"Instance", "Move", "Move()", "(direction: Vector3, relative: boolean?) -> ()", "[Method] Fuerza movimiento en una direccion 3D.", 1},
            {"Instance", "LoadAnimation", "LoadAnimation()", "(animation: Animation) -> AnimationTrack", "[Method] Prepara una animacion para ser reproducida.", 1},
            {"Instance", "Died", "Died", "RBXScriptSignal", "[Event] Se dispara cuando la Health llega a cero.", 2},
            {"Instance", "HealthChanged", "HealthChanged", "RBXScriptSignal", "[Event] Se dispara cuando cambia la salud. Args: (newHealth, oldHealth)", 2},

            // GUI ELEMENTS (Extienden de Instance)
            {"Instance", "Text", "Text", "string", "[Property] Contenido visible en Labels o Buttons.", 4},
            {"Instance", "TextColor3", "TextColor3", "Color3", "[Property] Color de la fuente del texto.", 4},
            {"Instance", "TextScaled", "TextScaled", "boolean", "[Property] Ajusta el texto al tamano de la caja.", 4},
            {"Instance", "BackgroundColor3", "BackgroundColor3", "Color3", "[Property] Color de fondo del frame.", 4},
            {"Instance", "BackgroundTransparency", "BackgroundTransparency", "number", "[Property] Invisibilidad del fondo de UI.", 4},
            {"Instance", "Visible", "Visible", "boolean", "[Property] Si la interfaz renderiza o no.", 4},
            {"Instance", "ZIndex", "ZIndex", "number", "[Property] Orden de renderizado en profundidad UI.", 4},
            {"Instance", "MouseButton1Click", "MouseButton1Click", "RBXScriptSignal", "[Event] Al hacer clic izquierdo en el boton.", 2},
            {"Instance", "MouseEnter", "MouseEnter", "RBXScriptSignal", "[Event] Cuando el mouse entra al frame.", 2},
            {"Instance", "MouseLeave", "MouseLeave", "RBXScriptSignal", "[Event] Cuando el mouse sale del frame.", 2}
        };

        String search_prefix = target_prefix;
        static const std::unordered_set<std::string> known_prefixes = {
            "math", "string", "table", "task", "coroutine", "utf8", "bit32",
            "Vector3", "Vector2", "CFrame", "Color3", "UDim", "UDim2",
            "Enum", "TweenInfo", "Instance",
            "game", "Workspace", "workspace", "Players", "Lighting",
            "RunService", "TweenService", "UserInputService", "HttpService",
            "Debris", "CollectionService", "DataStoreService", "ContextActionService",
            "PathfindingService", "PhysicsService", "TeleportService",
            "BadgeService", "MarketplaceService", "GuiService", "InsertService",
            "SoundService", "TextChatService", "StarterGui"
        };
        if (!target_prefix.is_empty() && !known_prefixes.count(target_prefix.utf8().get_data()) && !is_expecting_instance_name) {
            search_prefix = "Instance";
        }

        // Cuando estamos dentro de GetService("..."), mostrar nombres de servicios
        if (is_expecting_instance_name) {
            static const char* service_names[] = {
                "Players", "RunService", "Lighting", "Workspace",
                "ReplicatedStorage", "ReplicatedFirst",
                "ServerScriptService", "ServerStorage",
                "StarterGui", "StarterPlayer", "StarterPack",
                "TextChatService", "SoundService", "Teams",
                "TweenService", "HttpService", "UserInputService",
                "Debris", "CollectionService", "DataStoreService",
                "ContextActionService", "PathfindingService", "PhysicsService",
                "TeleportService", "BadgeService", "MarketplaceService",
                "GuiService", "InsertService", "ScriptContext",
                "MaterialService", "NetworkClient", nullptr
            };
            for (int si = 0; service_names[si] != nullptr; si++) {
                String svc = String(service_names[si]);
                int score = fuzzy_match_score(filter, svc);
                if (score > 0) {
                    int layer = svc.to_lower().begins_with(filter.to_lower()) ? 1 : 2;
                    std::string sn_std = svc.utf8().get_data();
                    int usage = usage_memory.count(sn_std) ? usage_memory.at(sn_std) : 0;
                    sorter.push_back({layer, usage, score, svc, svc, "Servicio de Roblox", "", 5, Color(0.9f, 0.7f, 0.3f, 1.0f)});
                }
            }
        }

        // AGREGANDO LA BASE DE DATOS Y APLICANDO LA IA DE FRECUENCIA
        for (const LuauAPIItem& item : api) {
            if (is_expecting_instance_name) {
                // Mostrar tambien clases de Instance.new()
                if (String(item.prefix).is_empty() && String(item.name).length() > 0 &&
                    String(item.name).substr(0,1) == String(item.name).substr(0,1).to_upper()) {
                    int score = fuzzy_match_score(filter, item.name);
                    if (score > 0) {
                        int layer = String(item.name).to_lower().begins_with(filter.to_lower()) ? 1 : 2;
                        std::string i_name_std = String(item.name).utf8().get_data();
                        int usage = usage_memory.count(i_name_std) ? usage_memory.at(i_name_std) : 0;
                        sorter.push_back({layer, usage, score, item.name, item.name, "Clase de Roblox", "", 5, Color(0.9f, 0.8f, 0.5f, 1.0f)});
                    }
                }
                continue;
            }

            bool matches_context = (item.prefix == search_prefix) || (resolved_prefix_type == item.prefix);
            if (target_prefix == "game" && String(item.prefix) == "game") matches_context = true;
            if (target_prefix == "workspace" && String(item.prefix) == "Workspace") matches_context = true;
            if (search_prefix == "Instance" && String(item.prefix) == "game" && target_prefix == "game") matches_context = true;
            if (search_prefix == "Instance" && String(item.prefix) == "Workspace" && target_prefix == "workspace") matches_context = true;

            if (matches_context) {
                int score = fuzzy_match_score(filter, item.name);
                if (score > 0) {
                    Color c = Color(0.6f, 0.8f, 1.0f, 1.0f);
                    if (item.kind == 1) c = Color(0.87f, 0.87f, 0.67f, 1.0f); 
                    else if (item.kind == 2) c = Color(0.9f, 0.5f, 0.5f, 1.0f); 
                    else if (item.kind == 5) c = Color(0.8f, 0.6f, 0.4f, 1.0f); 
                    
                    int layer = String(item.name).to_lower().begins_with(filter.to_lower()) ? 1 : 2;
                    std::string i_name_std = String(item.name).utf8().get_data();
                    int usage = usage_memory.count(i_name_std) ? usage_memory.at(i_name_std) : 0;
                    
                    String display_str = String(item.name) + "   " + String(item.signature);
                    String desc_str = String(item.desc) + "\n\nUsado " + String::num(usage) + " veces en este script.";
                    
                    sorter.push_back({layer, usage, score, display_str, item.insert_text, desc_str, item.signature, item.kind, c});
                }
            }
        }

        if (target_prefix.is_empty()) {
            if (is_after_equal) {
                struct EqSnippet { String trigger; String display; String code; String desc; };
                EqSnippet eq_snippets[] = {
                    {"Instance", "[Class] Instance.new", "Instance.new(\"Part\")", "Inicializa nueva entidad."},
                    {"Vector3", "[Class] Vector3.new", "Vector3.new(0, 0, 0)", "Inicializa vector espacial."},
                    {"Color3", "[Class] Color3.fromRGB", "Color3.fromRGB(255, 255, 255)", "Inicializa color."},
                    {"CFrame", "[Class] CFrame.new", "CFrame.new()", "Inicializa matriz de rotacion."},
                    {"UDim2", "[Class] UDim2.new", "UDim2.new(0, 0, 0, 0)", "Inicializa dimension de UI."},
                    {"workspace", "[Global] workspace", "workspace", "Referencia al mundo."},
                    {"game", "[Global] game", "game", "Referencia a DataModel."}
                };
                for (const EqSnippet& snip : eq_snippets) {
                    int score = fuzzy_match_score(filter, snip.trigger);
                    if (score > 0) {
                        int layer = snip.trigger.to_lower().begins_with(filter.to_lower()) ? 1 : 2;
                        std::string i_name_std = snip.trigger.utf8().get_data();
                        int usage = usage_memory.count(i_name_std) ? usage_memory.at(i_name_std) : 0;

                        sorter.push_back({layer, usage, score, snip.display, snip.code, snip.desc, "", 0, Color(0.4f, 0.9f, 1.0f, 1.0f)});
                    }
                }
            } else {
                struct Snippet { String trigger; String display; String code; String desc; };
                Snippet ai_snippets[] = {
                    {"local", "local", "local ", "Declaracion rapida local."},
                    {"function", "function", "function()", "Bloque de funcion global."},
                    {"for", "for i, v pairs", "for i, v in pairs() do", "Iteracion segura sobre diccionario."},
                    {"while", "while task.wait", "while task.wait() do", "Bucle de ciclo infinito de motor."},
                    {"if", "if then", "if  then", "Bloque de condicion logica."},
                    {"require", "require", "require()", "Llamada modular completa."}
                };
                for (const Snippet& snip : ai_snippets) {
                    int score = fuzzy_match_score(filter, snip.trigger);
                    if (score > 0) {
                        int layer = snip.trigger.to_lower().begins_with(filter.to_lower()) ? 1 : 2;
                        std::string i_name_std = snip.trigger.utf8().get_data();
                        int usage = usage_memory.count(i_name_std) ? usage_memory.at(i_name_std) : 0;

                        sorter.push_back({layer, usage, score, snip.display, snip.code, snip.desc, "", 6, Color(0.85f, 0.65f, 0.95f, 1.0f)});
                    }
                }
            }

            for (int i = 0; i < local_variables.size(); i++) {
                int score = fuzzy_match_score(filter, local_variables[i].name);
                if (score > 0 && !is_after_equal) {
                    int layer = local_variables[i].name.to_lower().begins_with(filter.to_lower()) ? 1 : 2;
                    std::string i_name_std = local_variables[i].name.utf8().get_data();
                    int usage = usage_memory.count(i_name_std) ? usage_memory.at(i_name_std) : 0;

                    sorter.push_back({layer, usage, score, local_variables[i].name, local_variables[i].name, "Variable local inferida como: " + local_variables[i].inferred_type, "", 3, Color(0.6f, 0.95f, 0.6f, 1.0f)});
                }
            }
        }

        if (is_after_colon && fuzzy_match_score(filter, "Connect") > 0) {
            std::string i_name_std = "Connect";
            int usage = usage_memory.count(i_name_std) ? usage_memory.at(i_name_std) : 0;
            sorter.push_back({1, usage, 200, "Connect(function)", "Connect(function(hit)\n\t\nend)", "Conecta un evento nativo a una funcion anonima inline.", "", 2, Color(0.85f, 0.65f, 0.95f, 1.0f)});
        }

        std::sort(sorter.begin(), sorter.end());

        // LÍMITE ESTRICTO DE 30 ELEMENTOS PARA NO CONGELAR GODOT
        for (int i = 0; i < sorter.size() && i < 30; i++) {
            Dictionary s;
            s["display"] = sorter[i].display; 
            s["insert_text"] = sorter[i].insert_text; 
            s["kind"] = sorter[i].kind; 
            s["type"] = 1; 
            s["font_color"] = sorter[i].color; 
            s["icon"] = get_dummy_icon(); 
            s["description"] = sorter[i].desc; 
            s["location"] = 0; 
            s["default_value"] = Variant(); 
            s["matches"] = Array();
            
            final_suggestions_array.push_back(s);
        }

        res["result"] = final_suggestions_array; res["options"] = final_suggestions_array; res["force"] = true; res["call_hint"] = ""; return res;
    }
};

#endif