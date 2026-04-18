#ifndef LUAU_AUTOCOMPLETE_H
#define LUAU_AUTOCOMPLETE_H

#include "luau_type_database.h"

#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/classes/placeholder_texture2d.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/project_settings.hpp>
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

struct LocalVar { String name; String inferred_type; };

struct TempSuggestion {
    int priority_layer, usage_count, fuzzy_score;
    String display, insert_text, desc, signature;
    int kind;
    Color color;
    bool operator<(const TempSuggestion& o) const {
        if (priority_layer != o.priority_layer) return priority_layer < o.priority_layer;
        if (usage_count != o.usage_count) return usage_count > o.usage_count;
        return fuzzy_score > o.fuzzy_score;
    }
};

enum StringContextType {
    STR_CTX_NONE = 0,
    STR_CTX_GET_SERVICE,
    STR_CTX_FIND_CHILD,
    STR_CTX_WAIT_CHILD,
    STR_CTX_INSTANCE_NEW,
    STR_CTX_IS_A,
    STR_CTX_REQUIRE,
    STR_CTX_SUPPRESS,
};

class LuauAutocomplete {
private:

    static Ref<PlaceholderTexture2D> get_dummy_icon() {
        static Ref<PlaceholderTexture2D> icon;
        if (icon.is_null()) { icon.instantiate(); icon->set_size(Vector2(16,16)); }
        return icon;
    }

    static bool is_lua_keyword(const String& w) {
        static const std::unordered_set<std::string> kw = {
            "and","break","do","else","elseif","end","false","for","function",
            "if","in","local","nil","not","or","repeat","return","then","true",
            "until","while","require","continue","export","type"
        };
        return kw.count(w.utf8().get_data()) > 0;
    }

    static int levenshtein(const std::string& a, const std::string& b) {
        int la=a.size(), lb=b.size();
        std::vector<std::vector<int>> dp(la+1, std::vector<int>(lb+1));
        for(int i=0;i<=la;i++) dp[i][0]=i;
        for(int j=0;j<=lb;j++) dp[0][j]=j;
        for(int i=1;i<=la;i++) for(int j=1;j<=lb;j++)
            dp[i][j] = a[i-1]==b[j-1] ? dp[i-1][j-1] : 1+std::min({dp[i-1][j],dp[i][j-1],dp[i-1][j-1]});
        return dp[la][lb];
    }

    static int fuzzy_match_score(const std::string& f, const std::string& fl, const String& target) {
        if (f.empty()) return 100;
        std::string t = target.utf8().get_data();
        std::string tl = target.to_lower().utf8().get_data();
        int score = 0, fi = 0;
        for(int i=0; i<(int)t.size() && fi<(int)f.size(); i++) {
            if(tl[i] == fl[fi]) { 
                score += std::isupper(t[i]) && std::islower(f[fi]) ? 15 : 5; 
                fi++; 
            }
        }
        if(fi == (int)f.size()) return score+20;
        if(f.size() > 2 && t.size() > 2) { 
            int d = levenshtein(fl, tl); 
            if(d <= 2) return 10-d; 
        }
        return 0;
    }

    static std::unordered_map<std::string,int> analyze_usage(const String& code) {
        std::unordered_map<std::string,int> freq;
        std::string raw=code.utf8().get_data(), word;
        for(char c:raw) {
            if(std::isalnum(c)||c=='_') word+=c;
            else { if(!word.empty()) { freq[word]++; word=""; } }
        }
        if(!word.empty()) freq[word]++;
        return freq;
    }

    static String detect_language(const String& code) {
        std::string cl = code.to_lower().utf8().get_data();
        int es=0, pt=0, en=0;
        static const char* es_w[]={"jugador","velocidad","salud","vida","personaje","funcion","escena","camara","movimiento","ataque","enemigo","suelo",nullptr};
        static const char* pt_w[]={"jogador","velocidade","saude","personagem","inimigo","cena","camera","movimento","ataque","chao","funcao",nullptr};
        static const char* en_w[]={"player","speed","health","character","scene","camera","movement","attack","enemy","ground","weapon","shoot",nullptr};
        for(int i=0;es_w[i];i++) if(cl.find(es_w[i])!=std::string::npos) es++;
        for(int i=0;pt_w[i];i++) if(cl.find(pt_w[i])!=std::string::npos) pt++;
        for(int i=0;en_w[i];i++) if(cl.find(en_w[i])!=std::string::npos) en++;
        if(es>pt&&es>en) return "es";
        if(pt>es&&pt>en) return "pt";
        if(en>es&&en>pt) return "en";
        return "unknown";
    }

    static String pick_lang(const char* raw, const String& lang) {
        String d = String(raw);
        PackedStringArray p = d.split("||");
        if(p.size()==3) {
            if(lang=="en") return p[0];
            if(lang=="es") return p[1];
            if(lang=="pt") return p[2];
            String res = String("English: ");
            res += p[0];
            res += String("\nEspañol: ");
            res += p[1];
            res += String("\nPortuguês: ");
            res += p[2];
            return res;
        }
        return d;
    }

    static String make_desc(const char* raw, const String& lang, const String& prefix, const String& name, int usage) {
        String result = pick_lang(raw, lang);
        if(usage>0) {
            result += String("\n\nUsed ");
            result += String::num(usage);
            result += String(" times in this script.");
        }
        static const std::unordered_set<std::string> libs={"math","string","table","task","coroutine","utf8","bit32"};
        std::string pref=prefix.utf8().get_data();
        String url;
        if(libs.count(pref)) {
            url = String("https://create.roblox.com/docs/reference/engine/libraries/");
            url += prefix;
        }
        else if(!prefix.is_empty()) {
            url = String("https://create.roblox.com/docs/reference/engine/classes/");
            url += prefix;
        }
        else url = "https://create.roblox.com/docs/reference/engine/globals/LuaGlobals";
        result += "\n\n[color=#4d9de0]Learn More...[/color]";
        return result;
    }

    static StringContextType detect_string_ctx(const String& before) {
        String t=before.strip_edges();
        if(t.ends_with("GetService(")||t.ends_with("GetService(\"")||t.ends_with("GetService('")) return STR_CTX_GET_SERVICE;
        if(t.ends_with("FindFirstChild(")||t.ends_with("FindFirstChild(\"")||t.ends_with("FindFirstChild('")) return STR_CTX_FIND_CHILD;
        if(t.ends_with("WaitForChild(")||t.ends_with("WaitForChild(\"")||t.ends_with("WaitForChild('")) return STR_CTX_WAIT_CHILD;
        if(t.ends_with("Instance.new(")||t.ends_with("Instance.new(\"")||t.ends_with("Instance.new('")) return STR_CTX_INSTANCE_NEW;
        if(t.ends_with("IsA(")||t.ends_with("IsA(\"")||t.ends_with("IsA('")) return STR_CTX_IS_A;
        if(t.ends_with("require(")||t.ends_with("require(\"")||t.ends_with("require('")) return STR_CTX_REQUIRE;
        static const char* suppress[]={"print(","warn(","error(","tostring(","tonumber(","string.format(",nullptr};
        for(int i=0;suppress[i];i++) {
            String fn=String(suppress[i]);
            if(t.ends_with(fn)||t.ends_with(fn.substr(0,fn.length()-1)+"\"")) return STR_CTX_SUPPRESS;
        }
        return STR_CTX_NONE;
    }

    static std::vector<std::string> extract_known_names(const String& code) {
        std::vector<std::string> names;
        std::unordered_set<std::string> seen;
        PackedStringArray lines = code.split("\n");
        for(int i=0;i<lines.size();i++) {
            String line=lines[i];
            for(const char* fn:{"FindFirstChild(\"","WaitForChild(\""}) {
                int pos=0;
                while(true) {
                    int idx=line.find(String(fn),pos);
                    if(idx==-1) break;
                    int start=idx+String(fn).length();
                    int end=line.find("\"",start);
                    if(end>start) {
                        std::string n=line.substr(start,end-start).utf8().get_data();
                        if(!n.empty()&&!seen.count(n)){names.push_back(n);seen.insert(n);}
                    }
                    pos=end+1; if(pos>=line.length()) break;
                }
            }
            for(const char* sep:{"script.","script:"}) {
                int sidx=line.find(String(sep));
                while(sidx!=-1) {
                    int start=sidx+String(sep).length();
                    int end=start;
                    while(end<line.length()&&(std::isalnum(line[end])||line[end]=='_')) end++;
                    if(end>start){
                        String child=line.substr(start,end-start);
                        if(!is_lua_keyword(child)){
                            std::string n=child.utf8().get_data();
                            if(!seen.count(n)){names.push_back(n);seen.insert(n);}
                        }
                    }
                    sidx=line.find(String(sep),end);
                }
            }
        }
        return names;
    }

    static std::vector<TempSuggestion> get_var_ai_hints(const String& var_name, const std::unordered_map<std::string,int>& mem) {
        std::vector<TempSuggestion> hints;
        std::string vn=var_name.to_lower().utf8().get_data();
        auto add=[&](const String& disp, const String& code, const String& desc){
            std::string k=code.utf8().get_data();
            int u=mem.count(k)?mem.at(k):0;
            hints.push_back({1,u+10,200,disp,code,desc,"",6,Color(0.4f,0.9f,1.0f,1.0f)});
        };
        auto has=[&](const char* w){return vn.find(w)!=std::string::npos;};
        if(has("speed")||has("velocidad")||has("velocidade")||has("walkspeed"))
            add("16  (default WalkSpeed)","16","Standard Roblox walk speed.");
        if(has("health")||has("salud")||has("saude")||vn=="hp"||vn=="vida")
            add("100  (max health)","100","Standard maximum health value.");
        if(has("jumppow")||has("jumpheight"))
            add("50  (default JumpPower)","50","Standard JumpPower value.");
        if((has("player")||has("jugador")||has("jogador"))&&!has("players"))
            add("game.Players.LocalPlayer","game.Players.LocalPlayer","Local player reference.");
        if(has("character")||has("personaje")||has("personagem"))
            add("player.Character or player.CharacterAdded:Wait()","player.Character or player.CharacterAdded:Wait()","Safe character reference.");
        if(has("humanoid"))
            add("character:FindFirstChildOfClass(\"Humanoid\")","character:FindFirstChildOfClass(\"Humanoid\")","Get Humanoid from character.");
        if(has("rootpart")||vn=="hrp"||has("root"))
            add("character:FindFirstChild(\"HumanoidRootPart\")","character:FindFirstChild(\"HumanoidRootPart\")","Character root part.");
        if(vn=="rs"||has("runservice"))
            add("game:GetService(\"RunService\")","game:GetService(\"RunService\")","RunService shortcut.");
        if(vn=="ts"||has("tweenservice"))
            add("game:GetService(\"TweenService\")","game:GetService(\"TweenService\")","TweenService shortcut.");
        if(vn=="uis"||has("userinput"))
            add("game:GetService(\"UserInputService\")","game:GetService(\"UserInputService\")","UserInputService shortcut.");
        if(vn=="rep"||vn=="rs2"||has("replicated"))
            add("game:GetService(\"ReplicatedStorage\")","game:GetService(\"ReplicatedStorage\")","ReplicatedStorage shortcut.");
        if(has("players")&&!has("player")&&!has("getplayer"))
            add("game:GetService(\"Players\")","game:GetService(\"Players\")","Players service shortcut.");
        if(has("camera")||has("camara"))
            add("workspace.CurrentCamera","workspace.CurrentCamera","Active camera reference.");
        if(has("mouse")||has("raton"))
            add("game.Players.LocalPlayer:GetMouse()","game.Players.LocalPlayer:GetMouse()","Local player mouse.");
        if((has("part")||has("parte"))&&!has("parts")&&!has("rootpart"))
            add("Instance.new(\"Part\")","Instance.new(\"Part\")","Create a new Part.");
        if(has("pos")||has("position")||has("posicion"))
            add("Vector3.new(0, 0, 0)","Vector3.new(0, 0, 0)","Zero position vector.");
        if(has("color")||has("colour")||has("cor"))
            add("Color3.fromRGB(255, 255, 255)","Color3.fromRGB(255, 255, 255)","White color.");
        if(has("tween")&&!has("tweenservice"))
            add("TweenInfo.new(1)","TweenInfo.new(1)","1-second tween configuration.");
        if(has("anim")||has("track"))
            add("humanoid:LoadAnimation(animation)","humanoid:LoadAnimation(animation)","Load animation on humanoid.");
        return hints;
    }

    // ── Detect which property appears on the LHS of an `=` assignment ───────
    static String detect_assigned_property(const String& ctc) {
        for (int i = ctc.length() - 1; i >= 1; i--) {
            char32_t c = ctc[i];
            if (c == '\n') break;
            char32_t pc = ctc[i-1];
            if (c == '=') {
                if (pc=='='||pc=='~'||pc=='<'||pc=='>') continue;
                String lhs = ctc.substr(0, i).strip_edges();
                int dot = lhs.rfind(".");
                if (dot == -1) return "";
                String prop = lhs.substr(dot+1).strip_edges();
                for (int k=0; k<prop.length(); k++) {
                    char32_t ch=prop[k];
                    if (!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_')) return "";
                }
                return prop;
            }
        }
        return "";
    }

    // ── Type-specific suggestions when assigning to a known property ─────────
    static void add_property_assign_hints(const String& prop, std::vector<TempSuggestion>& out) {
        std::string p = prop.to_lower().utf8().get_data();
        static const Color kColor(0.95f,0.65f,0.45f,1.f), kVec(0.5f,0.85f,1.f,1.f),
                           kBool(0.4f,0.9f,0.6f,1.f),     kNum(0.7f,0.95f,0.5f,1.f),
                           kStr(0.9f,0.85f,0.5f,1.f),      kSvc(0.9f,0.75f,0.4f,1.f),
                           kEnum(0.8f,0.7f,0.5f,1.f);
        auto add = [&](const String& disp, const String& code, const String& desc, Color c) {
            out.push_back({0, 100, 250, disp, code, desc, "", 6, c});
        };

        if (p=="color"||p=="backgroundcolor3"||p=="textcolor3"||p=="bordercolor3"||p.find("color3")!=std::string::npos) {
            add("Color3.fromRGB(r,g,b) → white",  "Color3.fromRGB(255, 255, 255)", "White — channels 0-255.", kColor);
            add("Color3.fromRGB(r,g,b) → red",    "Color3.fromRGB(255, 0, 0)",     "Red.", kColor);
            add("Color3.fromRGB(r,g,b) → green",  "Color3.fromRGB(0, 200, 0)",     "Green.", kColor);
            add("Color3.fromRGB(r,g,b) → blue",   "Color3.fromRGB(0, 120, 255)",   "Blue.", kColor);
            add("Color3.fromRGB(r,g,b) → black",  "Color3.fromRGB(0, 0, 0)",       "Black.", kColor);
            add("Color3.new(r,g,b) float 0-1",    "Color3.new(1, 1, 1)",           "Float channels [0–1].", kColor);
            add("Color3.fromHex(hex)",             "Color3.fromHex(\"#FFFFFF\")",   "Web hex color.", kColor);
        } else if (p=="size"||p=="velocity"||p=="rotationvelocity") {
            add("Vector3.new(x, y, z) → unit", "Vector3.new(1, 1, 1)", "Unit cube size.",  kVec);
            add("Vector3.new(x, y, z) → zero","Vector3.new(0, 0, 0)", "Zero vector.",      kVec);
            add("Vector3.zero",                "Vector3.zero",          "(0,0,0) shorthand.",kVec);
            add("Vector3.one",                 "Vector3.one",           "(1,1,1) shorthand.",kVec);
        } else if (p=="position"||p=="rotation") {
            add("Vector3.new(x, y, z)","Vector3.new(0, 0, 0)", "World position / rotation.", kVec);
            add("Vector3.zero",        "Vector3.zero",          "Origin.",                    kVec);
        } else if (p=="cframe") {
            add("CFrame.new(pos)",           "CFrame.new(Vector3.new(0, 0, 0))",      "CFrame at position.",          kVec);
            add("CFrame.lookAt(at,target)",  "CFrame.lookAt(Vector3.new(), Vector3.new(0,0,-1))", "Pointing toward target.", kVec);
            add("CFrame.Angles(rx,ry,rz)",   "CFrame.Angles(0, math.rad(90), 0)",     "Rotation-only CFrame.",        kVec);
            add("CFrame.identity",           "CFrame.identity",                        "No translation or rotation.",  kVec);
        } else if (p=="parent") {
            add("workspace",                          "workspace",                                    "Physical world.",        kSvc);
            add("script.Parent",                      "script.Parent",                                "Same container as this script.", kSvc);
            add("game:GetService(\"ReplicatedStorage\")", "game:GetService(\"ReplicatedStorage\")", "Shared client+server.",   kSvc);
            add("game:GetService(\"ServerStorage\")", "game:GetService(\"ServerStorage\")",         "Server-only storage.",    kSvc);
            add("nil  (removes from tree)",           "nil",                                          "Detach from scene tree.", Color(0.7f,0.5f,0.5f,1.f));
        } else if (p=="walkspeed") {
            add("16   (default Roblox)", "16",  "Standard walk speed.",  kNum);
            add("24   (sprint)",         "24",  "Sprint speed.",          kNum);
            add("0    (freeze)",         "0",   "Immobilize character.",  kNum);
            add("32   (very fast)",      "32",  "Very fast movement.",    kNum);
        } else if (p=="jumppower") {
            add("50   (default)", "50",  "Standard jump power.", kNum);
            add("0    (disabled)","0",   "Disable jumping.",     kNum);
            add("100  (high)",    "100", "Double height.",        kNum);
        } else if (p=="health"||p=="maxhealth") {
            add("100  (full)",   "100",                    "Full HP.",             kNum);
            add("0    (dead)",   "0",                      "Kills the character.", kNum);
            add("humanoid.MaxHealth","humanoid.MaxHealth","Restore to max.",       kNum);
        } else if (p=="transparency"||p=="backgroundtransparency") {
            add("0    (solid)",       "0",   "Fully visible.",       kNum);
            add("0.5  (semi)",        "0.5", "Half transparent.",    kNum);
            add("1    (invisible)",   "1",   "Fully invisible.",     kNum);
        } else if (p=="anchored"||p=="cancollide"||p=="massless"||p=="visible"||
                   p=="textscaled"||p=="globalshadows"||p=="enabled"||p=="castshado") {
            add("true",  "true",  "Enable.",  kBool);
            add("false", "false", "Disable.", Color(0.95f,0.5f,0.5f,1.f));
        } else if (p=="name") {
            add("\"MyObject\"", "\"MyObject\"", "Name shown in the scene explorer.", kStr);
        } else if (p=="text") {
            add("\"Your text here\"", "\"Your text here\"", "Visible label or button text.", kStr);
        } else if (p=="material") {
            add("Enum.Material.SmoothPlastic","Enum.Material.SmoothPlastic","Default smooth plastic.", kEnum);
            add("Enum.Material.Neon",         "Enum.Material.Neon",         "Glowing neon.",           kEnum);
            add("Enum.Material.Metal",        "Enum.Material.Metal",        "Metallic sheen.",         kEnum);
            add("Enum.Material.Wood",         "Enum.Material.Wood",         "Wooden texture.",         kEnum);
            add("Enum.Material.Glass",        "Enum.Material.Glass",        "Transparent glass.",      kEnum);
        } else if (p=="zindex") {
            add("1", "1",  "Default layer.",  kNum);
            add("2", "2",  "Above layer 1.",  kNum);
            add("10","10", "Topmost layer.",  kNum);
        }
    }

    static void add_dynamic_suggestions(const String& class_name, const std::string& f, const std::string& fl, std::vector<TempSuggestion>& out, const std::unordered_map<std::string,int>& mem) {
        if(!ClassDB::class_exists(class_name)) return;
        TypedArray<Dictionary> props=ClassDB::class_get_property_list(class_name,true);
        for(int i=0;i<props.size();i++) {
            Dictionary p=props[i]; String pn=p["name"];
            int sc=fuzzy_match_score(f,fl,pn); if(sc<=0) continue;
            int layer=pn.to_lower().begins_with(String(f.c_str()).to_lower())?1:2;
            std::string k=pn.utf8().get_data(); int u=mem.count(k)?mem.at(k):0;
            
            String desc = String("[Dynamic Property] of ");
            desc += class_name;
            out.push_back({layer,u,sc,pn,pn,desc,"",4,Color(0.6f,0.8f,1.0f,1.0f)});
        }
        TypedArray<Dictionary> meths=ClassDB::class_get_method_list(class_name,true);
        for(int i=0;i<meths.size();i++) {
            Dictionary m=meths[i]; String mn=m["name"];
            if(mn.begins_with("_")) continue;
            int sc=fuzzy_match_score(f,fl,mn); if(sc<=0) continue;
            int layer=mn.to_lower().begins_with(String(f.c_str()).to_lower())?1:2;
            std::string k=mn.utf8().get_data(); int u=mem.count(k)?mem.at(k):0;
            
            String mns = mn;
            mns += String("()");
            String desc = String("[Dynamic Method] of ");
            desc += class_name;
            out.push_back({layer,u,sc,mns,mns,desc,"",1,Color(0.87f,0.87f,0.67f,1.0f)});
        }
    }

    static const LuauAPIItem* get_api() {
        static const LuauAPIItem api[] = {
            {"","print","print()","(...: any) -> ()","Prints values to the output console.||Imprime valores en la consola de salida.||Imprime valores no console de saída.",1},
            {"","warn","warn()","(...: any) -> ()","Prints a yellow warning message.||Imprime advertencia amarilla.||Imprime aviso em amarelo.",1},
            {"","error","error()","(msg: string, level: number?) -> ()","Throws a red error and stops execution.||Lanza error rojo y detiene la ejecución.||Lança erro vermelho e para a execução.",1},
            {"","assert","assert()","(cond: any, msg: string?) -> any","Throws error if condition is false.||Lanza error si condición es falsa.||Lança erro se condição for falsa.",1},
            {"","require","require()","(module: ModuleScript) -> any","Executes and returns a ModuleScript.||Ejecuta y retorna un ModuleScript.||Executa e retorna um ModuleScript.",1},
            {"","tick","tick()","() -> number","Seconds since local UNIX epoch.||Segundos desde la época UNIX local.||Segundos desde a época UNIX local.",1},
            {"","time","time()","() -> number","Returns experience uptime.||Tiempo de ejecución de la experiencia.||Tempo de execução da experiência.",1},
            {"","type","type()","(value: any) -> string","Returns base type of a value.||Tipo base de un valor.||Tipo base de um valor.",1},
            {"","typeof","typeof()","(value: any) -> string","Returns exact Roblox type.||Tipo exacto Roblox de un valor.||Tipo Roblox exato de um valor.",1},
            {"","wait","wait()","(seconds: number?) -> (number, number)","Pauses execution (deprecated, use task.wait).||Pausa ejecución (obsoleto, usa task.wait).||Pausa execução (obsoleto, use task.wait).",1},
            {"","spawn","spawn()","(callback: function) -> ()","Parallel thread (deprecated, use task.spawn).||Hilo paralelo (obsoleto, usa task.spawn).||Thread paralela (obsoleto, use task.spawn).",1},
            {"","delay","delay()","(sec: number, fn: function) -> ()","Delayed execution (deprecated, use task.delay).||Retrasa ejecución (obsoleto, usa task.delay).||Atrasa execução (obsoleto, use task.delay).",1},
            {"","game","game","DataModel","Root DataModel containing all services.||Raíz que contiene todos los servicios.||DataModel raiz com todos os serviços.",3},
            {"","workspace","workspace","Workspace","Contains all physical world parts.||Contiene todas las partes físicas del mundo.||Contém todas as partes físicas do mundo.",3},
            {"","script","script","LuaSourceContainer","Reference to the current running script.||Referencia al script actual en ejecución.||Referência ao script em execução.",3},
            {"","math","math","Library","Standard math functions library.||Librería de funciones matemáticas.||Biblioteca de funções matemáticas.",0},
            {"","string","string","Library","String manipulation library.||Librería de manipulación de cadenas.||Biblioteca de manipulação de strings.",0},
            {"","table","table","Library","Array and dictionary manipulation library.||Librería de arreglos y diccionarios.||Biblioteca de arrays e dicionários.",0},
            {"","task","task","Library","Modern coroutine and timing library.||Librería moderna de tiempo y corrutinas.||Biblioteca moderna de tempo e corrotinas.",0},
            {"","coroutine","coroutine","Library","Native thread management library.||Librería nativa de manejo de hilos.||Biblioteca nativa de gerenciamento de threads.",0},
            {"","utf8","utf8","Library","Unicode character handling library.||Librería para caracteres Unicode.||Biblioteca de caracteres Unicode.",0},
            {"","bit32","bit32","Library","Bitwise operations library.||Librería para operaciones bit.||Biblioteca de operações bit.",0},
            {"","Enum","Enum","Library","Collection of all enumerated values.||Colección de todos los valores enumerados.||Coleção de todos os valores enumerados.",0},
            {"","Vector2","Vector2","Library","2D vector operations for UI and screens.||Vectores 2D para UI y pantallas.||Vetores 2D para UI e telas.",0},
            {"","Vector3","Vector3","Library","3D vector operations for world space.||Vectores 3D para el mundo físico.||Vetores 3D para o mundo físico.",0},
            {"","CFrame","CFrame","Library","3D position and rotation matrix.||Matrices de posición y rotación 3D.||Matriz de posição e rotação 3D.",0},
            {"","Color3","Color3","Library","RGB color values.||Valores de color RGB.||Valores de cor RGB.",0},
            {"","UDim","UDim","Library","1D UI dimension.||Dimensión UI unidimensional.||Dimensão UI unidimensional.",0},
            {"","UDim2","UDim2","Library","Full 2D UI coordinates.||Coordenadas UI 2D completas.||Coordenadas UI 2D completas.",0},
            {"","Instance","Instance","Library","Master creator for all engine objects.||Creador maestro de objetos del motor.||Criador mestre de objetos do motor.",0},
            {"","TweenInfo","TweenInfo","Library","Animation interpolation configuration.||Configuración de interpolaciones.||Configuração de interpolações.",0},

            {"math","pi","pi","number","Pi approximation (3.14159...).||Aproximación de Pi (3.14159...).||Aproximação de Pi (3.14159...).",4},
            {"math","huge","huge","number","Positive infinity.||Infinito positivo.||Infinito positivo.",4},
            {"math","abs","abs()","(x: number) -> number","Absolute value.||Valor absoluto.||Valor absoluto.",1},
            {"math","acos","acos()","(x: number) -> number","Arc cosine.||Arco coseno.||Arco cosseno.",1},
            {"math","asin","asin()","(x: number) -> number","Arc sine.||Arco seno.||Arco seno.",1},
            {"math","atan","atan()","(x: number) -> number","Arc tangent.||Arco tangente.||Arco tangente.",1},
            {"math","atan2","atan2()","(y: number, x: number) -> number","Arc tangent of y/x.||Arco tangente de y/x.||Arco tangente de y/x.",1},
            {"math","ceil","ceil()","(x: number) -> number","Rounds up to integer.||Redondea hacia arriba.||Arredonda para cima.",1},
            {"math","clamp","clamp()","(val: number, min: number, max: number) -> number","Clamps value between min and max.||Limita valor entre mínimo y máximo.||Limita valor entre mínimo e máximo.",1},
            {"math","cos","cos()","(x: number) -> number","Cosine of angle in radians.||Coseno en radianes.||Cosseno em radianos.",1},
            {"math","cosh","cosh()","(x: number) -> number","Hyperbolic cosine.||Coseno hiperbólico.||Cosseno hiperbólico.",1},
            {"math","deg","deg()","(x: number) -> number","Converts radians to degrees.||Radianes a grados.||Radianos em graus.",1},
            {"math","exp","exp()","(x: number) -> number","Returns e^x.||Devuelve e^x.||Retorna e^x.",1},
            {"math","floor","floor()","(x: number) -> number","Rounds down to integer.||Redondea hacia abajo.||Arredonda para baixo.",1},
            {"math","fmod","fmod()","(x: number, y: number) -> number","Integer division remainder.||Resto de la división entera.||Resto da divisão inteira.",1},
            {"math","frexp","frexp()","(x: number) -> (number, number)","Splits into mantissa and exponent.||Divide en mantisa y exponente.||Divide em mantissa e expoente.",1},
            {"math","ldexp","ldexp()","(m: number, e: number) -> number","Inverse of frexp.||Operación inversa a frexp.||Operação inversa ao frexp.",1},
            {"math","log","log()","(x: number, base: number?) -> number","Natural log or specific base.||Logaritmo natural o en base.||Logaritmo natural ou em base.",1},
            {"math","log10","log10()","(x: number) -> number","Base-10 logarithm.||Logaritmo en base 10.||Logaritmo na base 10.",1},
            {"math","max","max()","(...: number) -> number","Returns largest argument.||El mayor de los argumentos.||O maior dos argumentos.",1},
            {"math","min","min()","(...: number) -> number","Returns smallest argument.||El menor de los argumentos.||O menor dos argumentos.",1},
            {"math","modf","modf()","(x: number) -> (number, number)","Splits into integer and fractional parts.||Parte entera y fraccional.||Parte inteira e fracionária.",1},
            {"math","noise","noise()","(x: number, y: number?, z: number?) -> number","Generates 1D/2D/3D Perlin noise.||Ruido Perlin 1D, 2D o 3D.||Ruído Perlin 1D, 2D ou 3D.",1},
            {"math","pow","pow()","(x: number, y: number) -> number","Raises x to power y.||Eleva x a la potencia y.||Eleva x à potência y.",1},
            {"math","rad","rad()","(x: number) -> number","Converts degrees to radians.||Grados a radianes.||Graus em radianos.",1},
            {"math","random","random()","(m: number?, n: number?) -> number","Generates pseudorandom number.||Número pseudoaleatorio.||Número pseudoaleatório.",1},
            {"math","randomseed","randomseed()","(seed: number) -> ()","Sets random seed.||Semilla para math.random.||Semente para math.random.",1},
            {"math","round","round()","(x: number) -> number","Rounds to nearest integer.||Redondea al entero más cercano.||Arredonda ao inteiro mais próximo.",1},
            {"math","sign","sign()","(x: number) -> number","Returns sign: 1, -1, or 0.||Signo del número: 1, -1 o 0.||Sinal do número: 1, -1 ou 0.",1},
            {"math","sin","sin()","(x: number) -> number","Sine of angle in radians.||Seno en radianes.||Seno em radianos.",1},
            {"math","sinh","sinh()","(x: number) -> number","Hyperbolic sine.||Seno hiperbólico.||Seno hiperbólico.",1},
            {"math","sqrt","sqrt()","(x: number) -> number","Square root.||Raíz cuadrada.||Raiz quadrada.",1},
            {"math","tan","tan()","(x: number) -> number","Tangent of angle in radians.||Tangente en radianes.||Tangente em radianos.",1},
            {"math","tanh","tanh()","(x: number) -> number","Hyperbolic tangent.||Tangente hiperbólica.||Tangente hiperbólica.",1},

            {"string","byte","byte()","(s: string, i: number?, j: number?) -> ...number","Numeric codes of characters.||Código numérico de los caracteres.||Código numérico dos caracteres.",1},
            {"string","char","char()","(...: number) -> string","String from numeric codes.||Cadena desde códigos numéricos.||String a partir de códigos numéricos.",1},
            {"string","find","find()","(s: string, pattern: string, init: number?, plain: boolean?) -> (number, number)","Finds pattern in string.||Busca un patrón en el string.||Busca um padrão na string.",1},
            {"string","format","format()","(fmt: string, ...: any) -> string","Formats string like printf.||Formatea cadena estilo C.||Formata string como printf.",1},
            {"string","gmatch","gmatch()","(s: string, pattern: string) -> function","Iterator for all pattern matches.||Iterador de coincidencias.||Iterador de correspondências.",1},
            {"string","gsub","gsub()","(s: string, pattern: string, repl: any, n: number?) -> (string, number)","Global pattern replacement.||Reemplazo global de patrones.||Substituição global de padrões.",1},
            {"string","len","len()","(s: string) -> number","String length.||Longitud de la cadena.||Comprimento da string.",1},
            {"string","lower","lower()","(s: string) -> string","Converts to lowercase.||Convierte a minúsculas.||Converte para minúsculas.",1},
            {"string","match","match()","(s: string, pattern: string, init: number?) -> string?","Returns first pattern match.||Primera coincidencia del patrón.||Primeira correspondência do padrão.",1},
            {"string","pack","pack()","(fmt: string, ...: any) -> string","Packs binary values into string.||Empaqueta valores binarios.||Empacota valores binários.",1},
            {"string","packsize","packsize()","(fmt: string) -> number","Size in bytes of packed string.||Tamaño en bytes empaquetado.||Tamanho em bytes empacotado.",1},
            {"string","rep","rep()","(s: string, n: number) -> string","Repeats string n times.||Repite la cadena n veces.||Repete a string n vezes.",1},
            {"string","reverse","reverse()","(s: string) -> string","Reverses the string.||Invierte la cadena.||Inverte a string.",1},
            {"string","split","split()","(s: string, sep: string?) -> {string}","Splits string into array.||Divide la cadena en array.||Divide a string em array.",1},
            {"string","sub","sub()","(s: string, i: number, j: number?) -> string","Extracts a substring.||Extrae una subcadena.||Extrai uma substring.",1},
            {"string","unpack","unpack()","(fmt: string, s: string, pos: number?) -> ...any","Unpacks binary values.||Desempaqueta valores binarios.||Desempacota valores binários.",1},
            {"string","upper","upper()","(s: string) -> string","Converts to uppercase.||Convierte a mayúsculas.||Converte para maiúsculas.",1},

            {"table","clear","clear()","(t: table) -> ()","Clears table without reallocating memory.||Vacía la tabla sin reasignar memoria.||Limpa a tabela sem realocar memória.",1},
            {"table","clone","clone()","(t: table) -> table","Shallow copy of a table.||Copia superficial de la tabla.||Cópia rasa da tabela.",1},
            {"table","concat","concat()","(t: table, sep: string?, i: number?, j: number?) -> string","Concatenates table elements into string.||Concatena elementos en string.||Concatena elementos em string.",1},
            {"table","create","create()","(count: number, value: any?) -> table","Creates pre-allocated table.||Crea tabla pre-asignada.||Cria tabela pré-alocada.",1},
            {"table","find","find()","(haystack: table, needle: any, init: number?) -> number?","Finds value in array.||Busca un valor en el array.||Busca um valor no array.",1},
            {"table","freeze","freeze()","(t: table) -> table","Makes table read-only (immutable).||Tabla de solo lectura.||Tabela somente leitura.",1},
            {"table","insert","insert()","(t: table, pos: number?, value: any) -> ()","Inserts value into array.||Inserta un valor en el array.||Insere um valor no array.",1},
            {"table","isfrozen","isfrozen()","(t: table) -> boolean","Checks if table is frozen.||Comprueba si la tabla está congelada.||Verifica se a tabela está congelada.",1},
            {"table","maxn","maxn()","(t: table) -> number","Largest positive numeric key.||Mayor clave numérica positiva.||Maior chave numérica positiva.",1},
            {"table","move","move()","(a1: table, f: number, e: number, t: number, a2: table?) -> table","Moves elements between tables.||Mueve elementos entre tablas.||Move elementos entre tabelas.",1},
            {"table","pack","pack()","(...: any) -> table","Packs arguments into table with n field.||Empaqueta argumentos en tabla con n.||Empacota argumentos em tabela com n.",1},
            {"table","remove","remove()","(t: table, pos: number?) -> any","Removes element from array.||Elimina elemento del array.||Remove elemento do array.",1},
            {"table","sort","sort()","(t: table, comp: function?) -> ()","Sorts array in-place (QuickSort).||Ordena el array in-place.||Ordena o array in-place.",1},
            {"table","unpack","unpack()","(list: table, i: number?, j: number?) -> ...any","Unpacks array elements as arguments.||Desempaqueta elementos del array.||Desempacota elementos do array.",1},

            {"task","cancel","cancel()","(thread: thread) -> ()","Cancels a pending coroutine.||Cancela corrutina pendiente.||Cancela corrotina pendente.",1},
            {"task","defer","defer()","(f: any, ...: any) -> thread","Runs at end of current frame cycle.||Ejecuta al final del ciclo de frame.||Executa ao final do ciclo de frame.",1},
            {"task","delay","delay()","(sec: number, f: any, ...: any) -> thread","Executes after given time (optimized).||Ejecuta después de cierto tiempo.||Executa após um tempo determinado.",1},
            {"task","desynchronize","desynchronize()","() -> ()","Switches coroutine to parallel execution.||Cambia a ejecución paralela.||Muda para execução paralela.",1},
            {"task","spawn","spawn()","(f: any, ...: any) -> thread","Runs immediately in a new thread.||Ejecuta en un nuevo hilo.||Executa em uma nova thread.",1},
            {"task","synchronize","synchronize()","() -> ()","Switches coroutine to serial (main thread).||Cambia a ejecución serial.||Muda para execução serial.",1},
            {"task","wait","task.wait()","(sec: number?) -> number","Pauses thread synchronized with physics.||Pausa sincronizado con el motor físico.||Pausa sincronizado com o motor físico.",1},

            {"coroutine","create","create()","(f: function) -> thread","Creates a new coroutine.||Crea una nueva corrutina.||Cria uma nova corrotina.",1},
            {"coroutine","isyieldable","isyieldable()","() -> boolean","Checks if current coroutine can yield.||Verifica si puede pausarse.||Verifica se pode pausar.",1},
            {"coroutine","resume","resume()","(co: thread, ...: any) -> (boolean, ...any)","Starts or continues a coroutine.||Inicia o continúa una corrutina.||Inicia ou continua uma corrotina.",1},
            {"coroutine","running","running()","() -> thread","Returns current running thread.||Hilo en ejecución actual.||Thread em execução atual.",1},
            {"coroutine","status","status()","(co: thread) -> string","Returns coroutine state (dead/suspended/running).||Estado de la corrutina.||Estado da corrotina.",1},
            {"coroutine","wrap","wrap()","(f: function) -> function","Creates coroutine wrapped as a function.||Corrutina envuelta como función.||Corrotina envolvida como função.",1},
            {"coroutine","yield","yield()","(...: any) -> ...any","Pauses the current coroutine.||Pausa la corrutina actual.||Pausa a corrotina atual.",1},

            {"Vector3","new","new()","(x: number?, y: number?, z: number?) -> Vector3","Creates a 3D vector.||Crea un vector tridimensional.||Cria um vetor tridimensional.",1},
            {"Vector3","zero","zero","Vector3","Vector3(0, 0, 0) — zero vector.||Vector3(0,0,0) — vector cero.||Vector3(0,0,0) — vetor zero.",4},
            {"Vector3","one","one","Vector3","Vector3(1, 1, 1) — unit vector.||Vector3(1,1,1) — vector unitario.||Vector3(1,1,1) — vetor unitário.",4},
            {"Vector3","xAxis","xAxis","Vector3","Vector3(1, 0, 0) — X axis.||Vector3(1,0,0) — eje X.||Vector3(1,0,0) — eixo X.",4},
            {"Vector3","yAxis","yAxis","Vector3","Vector3(0, 1, 0) — Y axis.||Vector3(0,1,0) — eje Y.||Vector3(0,1,0) — eixo Y.",4},
            {"Vector3","zAxis","zAxis","Vector3","Vector3(0, 0, 1) — Z axis.||Vector3(0,0,1) — eje Z.||Vector3(0,0,1) — eixo Z.",4},
            {"Vector3","Cross","Cross()","(other: Vector3) -> Vector3","Cross product of two vectors.||Producto cruzado de vectores.||Produto vetorial de dois vetores.",1},
            {"Vector3","Dot","Dot()","(other: Vector3) -> number","Dot product (scalar).||Producto punto escalar.||Produto escalar.",1},
            {"Vector3","Lerp","Lerp()","(goal: Vector3, alpha: number) -> Vector3","Linear interpolation between vectors.||Interpolación lineal entre vectores.||Interpolação linear entre vetores.",1},
            {"Vector3","Magnitude","Magnitude","number","Total length of the vector.||Longitud total del vector.||Comprimento total do vetor.",4},
            {"Vector3","Unit","Unit","Vector3","Normalized vector with length 1.||Vector normalizado (longitud 1).||Vetor normalizado (comprimento 1).",4},
            {"Vector3","X","X","number","X component.||Componente X.||Componente X.",4},
            {"Vector3","Y","Y","number","Y component.||Componente Y.||Componente Y.",4},
            {"Vector3","Z","Z","number","Z component.||Componente Z.||Componente Z.",4},

            {"Vector2","new","new()","(x: number?, y: number?) -> Vector2","Creates a 2D vector.||Crea un vector bidimensional.||Cria um vetor bidimensional.",1},
            {"Vector2","zero","zero","Vector2","Vector2(0, 0).||Vector2(0, 0).||Vector2(0, 0).",4},
            {"Vector2","one","one","Vector2","Vector2(1, 1).||Vector2(1, 1).||Vector2(1, 1).",4},
            {"Vector2","xAxis","xAxis","Vector2","Vector2(1, 0).||Vector2(1, 0).||Vector2(1, 0).",4},
            {"Vector2","yAxis","yAxis","Vector2","Vector2(0, 1).||Vector2(0, 1).||Vector2(0, 1).",4},
            {"Vector2","Cross","Cross()","(other: Vector2) -> number","2D cross product.||Producto cruzado 2D.||Produto vetorial 2D.",1},
            {"Vector2","Dot","Dot()","(other: Vector2) -> number","2D dot product.||Producto punto 2D.||Produto escalar 2D.",1},
            {"Vector2","Lerp","Lerp()","(goal: Vector2, alpha: number) -> Vector2","2D linear interpolation.||Interpolación lineal 2D.||Interpolação linear 2D.",1},
            {"Vector2","Magnitude","Magnitude","number","2D vector length.||Longitud del vector 2D.||Comprimento do vetor 2D.",4},
            {"Vector2","Unit","Unit","Vector2","Normalized 2D vector.||Vector 2D normalizado.||Vetor 2D normalizado.",4},
            {"Vector2","X","X","number","X component (2D).||Componente X del 2D.||Componente X do 2D.",4},
            {"Vector2","Y","Y","number","Y component (2D).||Componente Y del 2D.||Componente Y do 2D.",4},

            {"CFrame","new","new()","(x: number?, y: number?, z: number?) -> CFrame","Creates a 4x4 spatial matrix.||Crea coordenada espacial de matriz 4x4.||Cria coordenada espacial de matriz 4x4.",1},
            {"CFrame","Angles","Angles()","(rx: number, ry: number, rz: number) -> CFrame","Rotation from XYZ radians.||Rotación usando Radianes XYZ.||Rotação usando Radianos XYZ.",1},
            {"CFrame","fromAxisAngle","fromAxisAngle()","(v: Vector3, r: number) -> CFrame","Rotation around a vector axis.||Rotación alrededor de un eje vectorial.||Rotação em torno de um eixo vetorial.",1},
            {"CFrame","fromEulerAnglesXYZ","fromEulerAnglesXYZ()","(rx: number, ry: number, rz: number) -> CFrame","Rotation in X, Y, Z order.||Rotación en orden X, Y, Z.||Rotação na ordem X, Y, Z.",1},
            {"CFrame","fromEulerAnglesYXZ","fromEulerAnglesYXZ()","(rx: number, ry: number, rz: number) -> CFrame","Rotation in Y, X, Z order.||Rotación en orden Y, X, Z.||Rotação na ordem Y, X, Z.",1},
            {"CFrame","fromMatrix","fromMatrix()","(pos: Vector3, vX: Vector3, vY: Vector3, vZ: Vector3?) -> CFrame","Constructs from a direction matrix.||Construye desde matriz direccional.||Constrói a partir de matriz direcional.",1},
            {"CFrame","lookAt","lookAt()","(at: Vector3, lookAt: Vector3, up: Vector3?) -> CFrame","Points toward a specific position.||Apunta hacia una posición específica.||Aponta para uma posição específica.",1},
            {"CFrame","identity","identity","CFrame","Zero CFrame (no translation or rotation).||CFrame nulo sin traslación ni rotación.||CFrame nulo sem translação ou rotação.",4},
            {"CFrame","Position","Position","Vector3","Extracts the absolute position.||Posición vectorial absoluta.||Posição vetorial absoluta.",4},
            {"CFrame","Rotation","Rotation","CFrame","Extracts rotation matrix only.||Solo la matriz de rotación.||Apenas a matriz de rotação.",4},
            {"CFrame","LookVector","LookVector","Vector3","Forward direction (Z axis).||Dirección hacia adelante (eje Z).||Direção para frente (eixo Z).",4},
            {"CFrame","UpVector","UpVector","Vector3","Up direction (Y axis).||Dirección arriba (eje Y).||Direção para cima (eixo Y).",4},
            {"CFrame","RightVector","RightVector","Vector3","Right direction (X axis).||Dirección derecha (eje X).||Direção direita (eixo X).",4},
            {"CFrame","XVector","XVector","Vector3","Alias for RightVector.||Alias de RightVector.||Alias de RightVector.",4},
            {"CFrame","YVector","YVector","Vector3","Alias for UpVector.||Alias de UpVector.||Alias de UpVector.",4},
            {"CFrame","ZVector","ZVector","Vector3","Alias for negated LookVector.||Alias negado de LookVector.||Alias negado de LookVector.",4},
            {"CFrame","Inverse","Inverse()","() -> CFrame","Inverse matrix for relative transforms.||Matriz inversa para relatividad.||Matriz inversa para relatividade.",1},
            {"CFrame","Lerp","Lerp()","(goal: CFrame, alpha: number) -> CFrame","Smooth spherical linear interpolation.||Interpolación esférica lineal suave.||Interpolação esférica linear suave.",1},
            {"CFrame","ToWorldSpace","ToWorldSpace()","(cf: CFrame) -> CFrame","Transforms relative CFrame to global.||CFrame relativo a global.||CFrame relativo para global.",1},
            {"CFrame","ToObjectSpace","ToObjectSpace()","(cf: CFrame) -> CFrame","Transforms global CFrame to relative.||CFrame global a relativo.||CFrame global para relativo.",1},

            {"Color3","new","new()","(r: number?, g: number?, b: number?) -> Color3","Color from 0-1 float values.||Color en escala flotante 0-1.||Cor em escala flutuante 0-1.",1},
            {"Color3","fromRGB","fromRGB()","(r: number, g: number, b: number) -> Color3","Color from 0-255 byte values.||Color desde bytes 0-255.||Cor a partir de bytes 0-255.",1},
            {"Color3","fromHSV","fromHSV()","(h: number, s: number, v: number) -> Color3","Color from Hue, Saturation, Value.||Color en Matiz, Saturación, Valor.||Cor em Matiz, Saturação, Valor.",1},
            {"Color3","fromHex","fromHex()","(hex: string) -> Color3","Color from hex web code.||Color desde código Hexadecimal.||Cor a partir de código Hexadecimal.",1},
            {"Color3","R","R","number","Red float component (0-1).||Componente Rojo (0-1).||Componente Vermelho (0-1).",4},
            {"Color3","G","G","number","Green float component (0-1).||Componente Verde (0-1).||Componente Verde (0-1).",4},
            {"Color3","B","B","number","Blue float component (0-1).||Componente Azul (0-1).||Componente Azul (0-1).",4},
            {"Color3","Lerp","Lerp()","(goal: Color3, alpha: number) -> Color3","Linearly blends two colors.||Fusiona dos colores linealmente.||Funde duas cores linearmente.",1},
            {"Color3","ToHSV","ToHSV()","() -> (number, number, number)","Extracts Hue, Saturation, Value.||Extrae Matiz, Saturación y Valor.||Extrai Matiz, Saturação e Valor.",1},
            {"Color3","ToHex","ToHex()","() -> string","Converts to hex string.||Convierte a cadena Hexadecimal.||Converte para string Hexadecimal.",1},

            {"UDim","new","new()","(scale: number, offset: number) -> UDim","1D UI dimension.||Dimensión UI unidimensional.||Dimensão UI unidimensional.",1},
            {"UDim","Scale","Scale","number","Relative screen proportion (0-1).||Proporción relativa a la pantalla (0-1).||Proporção relativa à tela (0-1).",4},
            {"UDim","Offset","Offset","number","Absolute pixel offset.||Desplazamiento absoluto en píxeles.||Deslocamento absoluto em pixels.",4},
            {"UDim2","new","new()","(xScale: number, xOffset: number, yScale: number, yOffset: number) -> UDim2","Full 2D UI dimension.||Dimensión UI 2D completa.||Dimensão UI 2D completa.",1},
            {"UDim2","fromScale","fromScale()","(x: number, y: number) -> UDim2","UDim2 with scale values only.||UDim2 solo con valores de escala.||UDim2 apenas com valores de escala.",1},
            {"UDim2","fromOffset","fromOffset()","(x: number, y: number) -> UDim2","UDim2 with pixel values only.||UDim2 solo con píxeles.||UDim2 apenas com pixels.",1},
            {"UDim2","X","X","UDim","X component of UDim2.||Componente X del UDim2.||Componente X do UDim2.",4},
            {"UDim2","Y","Y","UDim","Y component of UDim2.||Componente Y del UDim2.||Componente Y do UDim2.",4},
            {"UDim2","Lerp","Lerp()","(goal: UDim2, alpha: number) -> UDim2","Animates UI element position.||Anima la posición de un elemento UI.||Anima a posição de um elemento UI.",1},

            {"TweenInfo","new","new()","(time: number, easingStyle: Enum, easingDir: Enum, repeatCount: number, reverses: boolean, delayTime: number) -> TweenInfo","Configures an interpolated animation.||Configura una animación interpolada.||Configura uma animação interpolada.",1},

            {"game","Workspace","Workspace","Workspace","[Service] Physical game environment.||[Servicio] Entorno físico del juego.||[Serviço] Ambiente físico do jogo.",4},
            {"game","Players","Players","Players","[Service] Manages connected players.||[Servicio] Maneja jugadores conectados.||[Serviço] Gerencia jogadores conectados.",4},
            {"game","Lighting","Lighting","Lighting","[Service] Lighting effects and sky.||[Servicio] Efectos de iluminación y cielo.||[Serviço] Efeitos de iluminação e céu.",4},
            {"game","ReplicatedStorage","ReplicatedStorage","ReplicatedStorage","[Service] Content visible to Server and Client.||[Servicio] Contenido visible por Server y Client.||[Serviço] Conteúdo visível para Server e Client.",4},
            {"game","ReplicatedFirst","ReplicatedFirst","ReplicatedFirst","[Service] Loads before the rest of the scene.||[Servicio] Se carga antes que el resto.||[Serviço] Carrega antes do restante.",4},
            {"game","ServerScriptService","ServerScriptService","ServerScriptService","[Service] Secure backend scripts.||[Servicio] Scripts seguros de backend.||[Serviço] Scripts seguros de backend.",4},
            {"game","ServerStorage","ServerStorage","ServerStorage","[Service] Server-only secure storage.||[Servicio] Almacenamiento seguro del servidor.||[Serviço] Armazenamento seguro do servidor.",4},
            {"game","StarterGui","StarterGui","StarterGui","[Service] Default player UI templates.||[Servicio] Interfaces predeterminadas de jugadores.||[Serviço] Templates de UI padrão dos jogadores.",4},
            {"game","StarterPlayer","StarterPlayer","StarterPlayer","[Service] Initial avatar configuration.||[Servicio] Configuración inicial de avatares.||[Serviço] Configuração inicial de avatares.",4},
            {"game","StarterPack","StarterPack","StarterPack","[Service] Player inventory items.||[Servicio] Items de inventario del jugador.||[Serviço] Itens de inventário do jogador.",4},
            {"game","RunService","RunService","RunService","[Service] Per-frame events: Heartbeat, RenderStepped, Stepped.||[Servicio] Eventos por frame.||[Serviço] Eventos por frame.",4},
            {"game","TweenService","TweenService","TweenService","[Service] Smooth animation engine.||[Servicio] Motor de animaciones suaves.||[Serviço] Motor de animações suaves.",4},
            {"game","HttpService","HttpService","HttpService","[Service] Web requests and JSON.||[Servicio] Peticiones Web y JSON.||[Serviço] Requisições Web e JSON.",4},
            {"game","Debris","Debris","Debris","[Service] Auto-cleanup of temporary objects.||[Servicio] Limpieza automática de objetos temporales.||[Serviço] Limpeza automática de objetos temporários.",4},
            {"game","CollectionService","CollectionService","CollectionService","[Service] Tag system for instances.||[Servicio] Sistema de tags en instancias.||[Serviço] Sistema de tags em instâncias.",4},
            {"game","DataStoreService","DataStoreService","DataStoreService","[Service] Player data persistence.||[Servicio] Persistencia de datos de jugadores.||[Serviço] Persistência de dados de jogadores.",4},
            {"game","ContextActionService","ContextActionService","ContextActionService","[Service] Input action binding.||[Servicio] Binding de acciones a inputs.||[Serviço] Vinculação de ações a inputs.",4},
            {"game","PathfindingService","PathfindingService","PathfindingService","[Service] Navigation pathfinding.||[Servicio] Cálculo de rutas de navegación.||[Serviço] Cálculo de rotas de navegação.",4},
            {"game","PhysicsService","PhysicsService","PhysicsService","[Service] Collision groups and filters.||[Servicio] Grupos y filtros de colisión física.||[Serviço] Grupos e filtros de colisão física.",4},
            {"game","TeleportService","TeleportService","TeleportService","[Service] Teleport between Places.||[Servicio] Teletransporte entre Places.||[Serviço] Teletransporte entre Places.",4},
            {"game","BadgeService","BadgeService","BadgeService","[Service] Award and verify player badges.||[Servicio] Otorgar y verificar medallas.||[Serviço] Conceder e verificar medalhas.",4},
            {"game","MarketplaceService","MarketplaceService","MarketplaceService","[Service] In-game purchases and GamePasses.||[Servicio] Compras en juego y GamePasses.||[Serviço] Compras no jogo e GamePasses.",4},
            {"game","GuiService","GuiService","GuiService","[Service] Global UI control.||[Servicio] Control global de interfaces.||[Serviço] Controle global de interfaces.",4},
            {"game","InsertService","InsertService","InsertService","[Service] Load external assets at runtime.||[Servicio] Carga assets externos en tiempo real.||[Serviço] Carrega assets externos em tempo real.",4},
            {"game","ScriptContext","ScriptContext","ScriptContext","[Service] Script context and error events.||[Servicio] Contexto y errores de scripts.||[Serviço] Contexto e erros de scripts.",4},
            {"game","SoundService","SoundService","SoundService","[Service] Global audio control.||[Servicio] Control del audio global.||[Serviço] Controle de áudio global.",4},
            {"game","TextChatService","TextChatService","TextChatService","[Service] In-game text chat system.||[Servicio] Sistema de chat de texto.||[Serviço] Sistema de chat de texto.",4},
            {"game","Teams","Teams","Teams","[Service] Team and color management.||[Servicio] Gestión de equipos y colores.||[Serviço] Gerenciamento de equipes e cores.",4},
            {"game","UserInputService","UserInputService","UserInputService","[Service] Keyboard and mouse input.||[Servicio] Captura de input de teclado y mouse.||[Serviço] Captura de input de teclado e mouse.",4},
            {"game","GetService","GetService(\"\")","(className: string) -> Instance","[Method] Gets a service safely by name.||[Método] Obtiene un servicio por nombre.||[Método] Obtém um serviço pelo nome.",1},
            {"game","IsLoaded","IsLoaded()","() -> boolean","[Method] True if all assets have loaded.||[Método] Verdadero si todos los assets cargaron.||[Método] Verdadeiro se todos os assets carregaram.",1},

            {"RunService","Heartbeat","Heartbeat","RBXScriptSignal","[Event] Every frame after physics. Args: (deltaTime: number).||[Evento] Cada frame tras la física. Args: (deltaTime: number).||[Evento] A cada frame após a física. Args: (deltaTime: number).",2},
            {"RunService","RenderStepped","RenderStepped","RBXScriptSignal","[Event] Every frame before rendering. Args: (deltaTime: number).||[Evento] Cada frame antes de renderizar.||[Evento] A cada frame antes de renderizar.",2},
            {"RunService","Stepped","Stepped","RBXScriptSignal","[Event] Every physics simulation step. Args: (time, deltaTime).||[Evento] Cada paso de simulación física.||[Evento] A cada passo de simulação física.",2},
            {"RunService","IsRunning","IsRunning()","() -> boolean","True if game is running (not in editor).||True si el juego está en ejecución.||True se o jogo está em execução.",1},
            {"RunService","IsClient","IsClient()","() -> boolean","True if executing on the client.||True si se ejecuta en el cliente.||True se executado no cliente.",1},
            {"RunService","IsServer","IsServer()","() -> boolean","True if executing on the server.||True si se ejecuta en el servidor.||True se executado no servidor.",1},
            {"RunService","IsStudio","IsStudio()","() -> boolean","True if running inside Godot editor.||True si se ejecuta en el editor de Godot.||True se executado no editor Godot.",1},
            {"RunService","BindToRenderStep","BindToRenderStep()","(name: string, priority: number, fn: function) -> ()","Binds function to render step with priority.||Vincula función al paso de renderizado.||Vincula função ao passo de renderização.",1},
            {"RunService","UnbindFromRenderStep","UnbindFromRenderStep()","(name: string) -> ()","Unbinds function from render step.||Desvincula función del paso de renderizado.||Desvincula função do passo de renderização.",1},

            {"UserInputService","IsKeyDown","IsKeyDown()","(keyCode: string) -> boolean","Checks if a key is currently pressed.||Verifica si una tecla está presionada.||Verifica se uma tecla está pressionada.",1},
            {"UserInputService","IsMouseButtonPressed","IsMouseButtonPressed()","(button: number) -> boolean","Checks if a mouse button is pressed.||Verifica si un botón del mouse está presionado.||Verifica se um botão do mouse está pressionado.",1},
            {"UserInputService","GetMouseDelta","GetMouseDelta()","() -> {X: number, Y: number}","Returns mouse movement this frame.||Movimiento del mouse en el frame.||Movimento do mouse neste frame.",1},
            {"UserInputService","InputBegan","InputBegan","RBXScriptSignal","[Event] Fired when any input is pressed.||[Evento] Se dispara cuando se presiona un input.||[Evento] Disparado quando um input é pressionado.",2},
            {"UserInputService","InputEnded","InputEnded","RBXScriptSignal","[Event] Fired when any input is released.||[Evento] Se dispara cuando se suelta un input.||[Evento] Disparado quando um input é solto.",2},
            {"UserInputService","InputChanged","InputChanged","RBXScriptSignal","[Event] Fired when input changes (mouse move).||[Evento] Se dispara cuando cambia un input.||[Evento] Disparado quando um input muda.",2},
            {"UserInputService","MouseBehavior","MouseBehavior","Enum.MouseBehavior","Controls whether mouse is locked or free.||Controla si el mouse está bloqueado.||Controla se o mouse está bloqueado.",4},
            {"UserInputService","MouseEnabled","MouseEnabled","boolean","True if a physical mouse is available.||Si el mouse físico está disponible.||Se o mouse físico está disponível.",4},
            {"UserInputService","KeyboardEnabled","KeyboardEnabled","boolean","True if a physical keyboard is available.||Si el teclado físico está disponible.||Se o teclado físico está disponível.",4},

            {"SoundService","SetListener","SetListener()","(listenerType: Enum, ...: any) -> ()","Sets the audio listener position.||Establece la posición del oyente de audio.||Define a posição do ouvinte de áudio.",1},
            {"SoundService","PlayLocalSound","PlayLocalSound()","(sound: Sound) -> ()","Plays a sound on the local client.||Reproduce un sonido en el cliente local.||Reproduz um som no cliente local.",1},

            {"TweenService","Create","Create()","(instance: Instance, tweenInfo: TweenInfo, goals: table) -> Tween","Creates an interpolated property animation.||Crea una animación interpolada de propiedades.||Cria uma animação interpolada de propriedades.",1},

            {"Debris","AddItem","AddItem()","(instance: Instance, lifetime: number?) -> ()","Auto-destroys instance after N seconds.||Elimina automáticamente tras N segundos.||Destrói automaticamente após N segundos.",1},

            {"CollectionService","AddTag","AddTag()","(instance: Instance, tag: string) -> ()","Assigns a tag to an instance.||Asigna un tag a la instancia.||Atribui uma tag à instância.",1},
            {"CollectionService","RemoveTag","RemoveTag()","(instance: Instance, tag: string) -> ()","Removes a tag from an instance.||Elimina un tag de la instancia.||Remove uma tag da instância.",1},
            {"CollectionService","HasTag","HasTag()","(instance: Instance, tag: string) -> boolean","Checks if instance has the tag.||Verifica si la instancia tiene el tag.||Verifica se a instância tem a tag.",1},
            {"CollectionService","GetTagged","GetTagged()","(tag: string) -> {Instance}","Returns all instances with the tag.||Devuelve todas las instancias con ese tag.||Retorna todas as instâncias com essa tag.",1},
            {"CollectionService","GetInstanceAddedSignal","GetInstanceAddedSignal()","(tag: string) -> RBXScriptSignal","Fires when tag is added to any instance.||Se dispara cuando se agrega el tag.||Disparado quando a tag é adicionada.",1},
            {"CollectionService","GetInstanceRemovedSignal","GetInstanceRemovedSignal()","(tag: string) -> RBXScriptSignal","Fires when tag is removed from any instance.||Se dispara cuando se elimina el tag.||Disparado quando a tag é removida.",1},

            {"DataStoreService","GetDataStore","GetDataStore()","(name: string, scope: string?) -> DataStore","Gets or creates a named DataStore.||Obtiene o crea un DataStore.||Obtém ou cria um DataStore.",1},
            {"DataStoreService","GetOrderedDataStore","GetOrderedDataStore()","(name: string, scope: string?) -> OrderedDataStore","Numerically ordered DataStore.||DataStore ordenado numéricamente.||DataStore ordenado numericamente.",1},
            {"DataStoreService","GetGlobalDataStore","GetGlobalDataStore()","() -> GlobalDataStore","Global game DataStore.||DataStore global del juego.||DataStore global do jogo.",1},

            {"HttpService","JSONEncode","JSONEncode()","(value: any) -> string","Serializes Lua table to JSON.||Serializa tabla Lua a JSON.||Serializa tabela Lua para JSON.",1},
            {"HttpService","JSONDecode","JSONDecode()","(json: string) -> any","Deserializes JSON to Lua table.||Deserializa JSON a tabla Lua.||Desserializa JSON para tabela Lua.",1},
            {"HttpService","GetAsync","GetAsync()","(url: string, noCache: boolean?, headers: table?) -> string","HTTP GET request.||Petición HTTP GET.||Requisição HTTP GET.",1},
            {"HttpService","PostAsync","PostAsync()","(url: string, data: string, contentType: string?) -> string","HTTP POST request.||Petición HTTP POST.||Requisição HTTP POST.",1},
            {"HttpService","RequestAsync","RequestAsync()","(requestOptions: table) -> table","Advanced HTTP request with full control.||Petición HTTP avanzada.||Requisição HTTP avançada.",1},
            {"HttpService","GenerateGUID","GenerateGUID()","(wrapInCurlyBraces: boolean?) -> string","Generates a unique GUID.||Genera un GUID único.||Gera um GUID único.",1},
            {"HttpService","UrlEncode","UrlEncode()","(input: string) -> string","URL-encodes a string.||Codifica una cadena para URLs.||Codifica uma string para URLs.",1},

            {"ContextActionService","BindAction","BindAction()","(name: string, func: function, createButton: boolean, ...: any) -> ()","Binds function to input keys.||Vincula función a teclas de input.||Vincula função a teclas de input.",1},
            {"ContextActionService","UnbindAction","UnbindAction()","(name: string) -> ()","Unbinds a registered action.||Desvincula una acción registrada.||Desvincula uma ação registrada.",1},
            {"ContextActionService","BindActionAtPriority","BindActionAtPriority()","(name: string, func: function, createButton: boolean, priority: number, ...: any) -> ()","Binds action with explicit priority.||Vincula con prioridad explícita.||Vincula com prioridade explícita.",1},

            {"PathfindingService","CreatePath","CreatePath()","(agentParameters: table?) -> Path","Creates a navigation path.||Crea una ruta de navegación.||Cria uma rota de navegação.",1},

            {"PhysicsService","RegisterCollisionGroup","RegisterCollisionGroup()","(name: string) -> ()","Creates a new collision group.||Crea un nuevo grupo de colisión.||Cria um novo grupo de colisão.",1},
            {"PhysicsService","CollisionGroupSetCollidable","CollisionGroupSetCollidable()","(name1: string, name2: string, collidable: boolean) -> ()","Sets whether two groups collide.||Controla si dos grupos colisionan.||Controla se dois grupos colidem.",1},
            {"PhysicsService","SetPartCollisionGroup","SetPartCollisionGroup()","(part: BasePart, groupName: string) -> ()","Assigns collision group to a part.||Asigna grupo de colisión a una parte.||Atribui grupo de colisão a uma parte.",1},

            {"TeleportService","Teleport","Teleport()","(placeId: number, player: Player?, data: table?) -> ()","Teleports player to another Place.||Teletransporta a otro Place.||Teletransporta para outro Place.",1},
            {"TeleportService","TeleportAsync","TeleportAsync()","(placeId: number, players: {Player}, opts: TeleportOptions?) -> TeleportAsyncResult","Teleports group of players.||Teletransporta grupo de jugadores.||Teletransporta grupo de jogadores.",1},

            {"BadgeService","AwardBadge","AwardBadge()","(userId: number, badgeId: number) -> boolean","Awards a badge to the player.||Otorga una medalla al jugador.||Concede uma medalha ao jogador.",1},
            {"BadgeService","UserHasBadgeAsync","UserHasBadgeAsync()","(userId: number, badgeId: number) -> boolean","Checks if player has the badge.||Verifica si el jugador tiene la medalla.||Verifica se o jogador tem a medalha.",1},

            {"MarketplaceService","PromptProductPurchase","PromptProductPurchase()","(player: Player, productId: number) -> ()","Opens purchase dialog.||Abre diálogo de compra.||Abre diálogo de compra.",1},
            {"MarketplaceService","PromptGamePassPurchase","PromptGamePassPurchase()","(player: Player, gamePassId: number) -> ()","Opens GamePass purchase dialog.||Abre diálogo de compra de GamePass.||Abre diálogo de compra de GamePass.",1},
            {"MarketplaceService","UserOwnsGamePassAsync","UserOwnsGamePassAsync()","(userId: number, gamePassId: number) -> boolean","Checks if player owns the GamePass.||Verifica si el jugador tiene el GamePass.||Verifica se o jogador tem o GamePass.",1},
            {"MarketplaceService","ProcessReceipt","ProcessReceipt","function","Purchase processing callback.||Callback de procesamiento de compras.||Callback de processamento de compras.",4},

            {"Workspace","CurrentCamera","CurrentCamera","Camera","Active camera on local client.||Cámara activa del cliente local.||Câmera ativa no cliente local.",4},
            {"Workspace","Gravity","Gravity","number","Gravitational force (default 196.2).||Fuerza gravitacional (defecto 196.2).||Força gravitacional (padrão 196.2).",4},
            {"Workspace","Terrain","Terrain","Terrain","Voxel terrain object.||Objeto de terreno por vóxeles.||Objeto de terreno por voxels.",4},
            {"Workspace","Raycast","Raycast()","(origin: Vector3, direction: Vector3, params: RaycastParams?) -> RaycastResult?","Casts a physics ray.||Lanza un rayo físico.||Lança um raio físico.",1},
            {"Workspace","GetServerTimeNow","GetServerTimeNow()","() -> number","Synchronized server time.||Tiempo sincronizado del servidor.||Tempo sincronizado do servidor.",1},
            {"Workspace","GetPartsInPart","GetPartsInPart()","(part: BasePart, params: OverlapParams?) -> {BasePart}","Detects spatial collisions.||Detecta colisiones espaciales.||Detecta colisões espaciais.",1},

            {"Players","LocalPlayer","LocalPlayer","Player","The player on the current client.||El jugador en el cliente actual.||O jogador no cliente atual.",4},
            {"Players","MaxPlayers","MaxPlayers","number","Maximum player limit on server.||Límite de jugadores en el servidor.||Limite de jogadores no servidor.",4},
            {"Players","PlayerAdded","PlayerAdded","RBXScriptSignal","[Event] Fires when a player joins.||[Evento] Se dispara cuando entra un jugador.||[Evento] Disparado quando um jogador entra.",2},
            {"Players","PlayerRemoving","PlayerRemoving","RBXScriptSignal","[Event] Fires when a player leaves.||[Evento] Se dispara cuando sale un jugador.||[Evento] Disparado quando um jogador sai.",2},
            {"Players","GetPlayers","GetPlayers()","() -> {Player}","Returns array of active players.||Array con los jugadores activos.||Array com os jogadores ativos.",1},
            {"Players","GetPlayerFromCharacter","GetPlayerFromCharacter()","(character: Model) -> Player?","Gets player from their 3D model.||Extrae jugador desde su modelo 3D.||Obtém jogador a partir do modelo 3D.",1},

            {"Instance","new","new()","(className: string, parent: Instance?) -> Instance","Dynamically instantiates a native class.||Instancia dinámicamente una clase nativa.||Instancia dinamicamente uma classe nativa.",1},
            {"Instance","Name","Name","string","Node identifier in the explorer.||Nombre del nodo en el Explorador.||Nome do nó no Explorador.",4},
            {"Instance","Parent","Parent","Instance","Container/parent node.||Nodo contenedor padre.||Nó contêiner pai.",4},
            {"Instance","ClassName","ClassName","string","Exact native type of the instance.||Tipo nativo exacto de la instancia.||Tipo nativo exato da instância.",4},
            {"Instance","Destroy","Destroy()","() -> ()","Permanently removes from memory.||Borra permanentemente de la memoria.||Remove permanentemente da memória.",1},
            {"Instance","Clone","Clone()","() -> Instance","Deep copy of the instance.||Copia profunda en memoria.||Cópia profunda na memória.",1},
            {"Instance","GetChildren","GetChildren()","() -> {Instance}","Array of direct child nodes.||Array de hijos directos.||Array de filhos diretos.",1},
            {"Instance","GetDescendants","GetDescendants()","() -> {Instance}","Full descendant tree.||Árbol completo de hijos.||Árvore completa de filhos.",1},
            {"Instance","FindFirstChild","FindFirstChild()","(name: string, recursive: boolean?) -> Instance?","Finds a direct child by name.||Busca un hijo directo por nombre.||Busca um filho direto por nome.",1},
            {"Instance","FindFirstChildOfClass","FindFirstChildOfClass()","(className: string) -> Instance?","Finds first child of a class type.||Busca por tipo de clase.||Busca por tipo de classe.",1},
            {"Instance","FindFirstAncestor","FindFirstAncestor()","(name: string) -> Instance?","Searches up the tree for a parent.||Sube el árbol buscando un padre.||Sobe a árvore buscando um pai.",1},
            {"Instance","WaitForChild","WaitForChild()","(name: string, timeOut: number?) -> Instance?","Waits synchronously for a child to load.||Espera a que cargue el nodo.||Aguarda o nó carregar.",1},
            {"Instance","IsA","IsA()","(className: string) -> boolean","Checks class inheritance strongly.||Verifica herencia de clase.||Verifica herança de classe.",1},
            {"Instance","GetAttribute","GetAttribute()","(attribute: string) -> any","Gets a custom attribute value.||Obtiene valor personalizado.||Obtém valor personalizado.",1},
            {"Instance","SetAttribute","SetAttribute()","(attribute: string, value: any) -> ()","Sets a custom attribute value.||Define valor personalizado.||Define valor personalizado.",1},
            {"Instance","ChildAdded","ChildAdded","RBXScriptSignal","[Event] Fired when a child is inserted.||[Evento] Al insertar un hijo.||[Evento] Ao inserir um filho.",2},
            {"Instance","ChildRemoved","ChildRemoved","RBXScriptSignal","[Event] Fired when a child is removed.||[Evento] Al borrar un hijo.||[Evento] Ao remover um filho.",2},
            {"Instance","AncestryChanged","AncestryChanged","RBXScriptSignal","[Event] Fired when Parent changes.||[Evento] Al cambiar de Padre.||[Evento] Ao mudar de pai.",2},
            {"Instance","Position","Position","Vector3","3D center position of the part.||Posición central de la parte en 3D.||Posição central da parte em 3D.",4},
            {"Instance","Size","Size","Vector3","Volumetric size of the collision box.||Tamaño volumétrico del colisionador.||Tamanho volumétrico do colisor.",4},
            {"Instance","CFrame","CFrame","CFrame","Absolute transformation matrix.||Matriz de transformación absoluta.||Matriz de transformação absoluta.",4},
            {"Instance","Color","Color","Color3","Visual tint of the part material.||Tinte visual del material de la parte.||Tinte visual do material da parte.",4},
            {"Instance","Transparency","Transparency","number","Invisibility level from 0.0 to 1.0.||Nivel de invisibilidad de 0.0 a 1.0.||Nível de invisibilidade de 0.0 a 1.0.",4},
            {"Instance","Anchored","Anchored","boolean","Locks object ignoring gravity.||Inmoviliza el objeto ignorando gravedad.||Imobiliza o objeto ignorando gravidade.",4},
            {"Instance","CanCollide","CanCollide","boolean","Enables solid collision with the world.||Habilita colisión sólida con el mundo.||Habilita colisão sólida com o mundo.",4},
            {"Instance","Massless","Massless","boolean","Removes mass contribution from model.||Elimina la contribución de masa.||Remove a contribuição de massa.",4},
            {"Instance","Material","Material","Enum.Material","Physical and visual texture of the block.||Textura física y visual del bloque.||Textura física e visual do bloco.",4},
            {"Instance","Velocity","Velocity","Vector3","Current inertial movement vector.||Vector de movimiento inercial actual.||Vetor de movimento inercial atual.",4},
            {"Instance","Touched","Touched","RBXScriptSignal","[Event] Fired when intersecting a collider.||[Evento] Al intersectar con un colisionador.||[Evento] Ao intersetar com um colisor.",2},
            {"Instance","TouchEnded","TouchEnded","RBXScriptSignal","[Event] Fired when separating from collider.||[Evento] Al separarse de un colisionador.||[Evento] Ao se separar de um colisor.",2},
            {"Instance","ApplyImpulse","ApplyImpulse()","(impulse: Vector3) -> ()","Applies instant force to center of mass.||Aplica fuerza instantánea al centro de masa.||Aplica força instantânea ao centro de massa.",1},
            {"Instance","GetMass","GetMass()","() -> number","Returns total mass of the part.||Devuelve la masa total de la parte.||Retorna a massa total da parte.",1},
            {"Instance","PrimaryPart","PrimaryPart","BasePart","Root part guiding the whole model.||Parte raíz que guía todo el modelo.||Parte raiz que guia todo o modelo.",4},
            {"Instance","MoveTo","MoveTo()","(position: Vector3) -> ()","Teleports model to a Vector3.||Teletransporta el modelo a un Vector3.||Teletransporta o modelo para um Vector3.",1},
            {"Instance","PivotTo","PivotTo()","(cframe: CFrame) -> ()","Rotates and moves using CFrame pivot.||Gira y transporta usando el pivote CFrame.||Gira e move usando o pivô CFrame.",1},
            {"Instance","GetPivot","GetPivot()","() -> CFrame","Gets current pivot CFrame.||Obtiene el CFrame del pivote.||Obtém o CFrame do pivô.",1},
            {"Instance","Health","Health","number","Current character health points.||Puntos de vida actuales del personaje.||Pontos de vida atuais do personagem.",4},
            {"Instance","MaxHealth","MaxHealth","number","Maximum allowed health points.||Puntos de vida máximos.||Pontos de vida máximos.",4},
            {"Instance","WalkSpeed","WalkSpeed","number","Character movement speed.||Velocidad de movimiento del personaje.||Velocidade de movimento do personagem.",4},
            {"Instance","JumpPower","JumpPower","number","Jump force strength.||Fuerza bruta del salto.||Força bruta do salto.",4},
            {"Instance","Sit","Sit","boolean","Whether the humanoid is sitting.||Si el humanoide está sentado.||Se o humanoide está sentado.",4},
            {"Instance","TakeDamage","TakeDamage()","(amount: number) -> ()","Reduces health (respects ForceField).||Resta salud si no tiene escudo.||Reduz vida se não houver escudo.",1},
            {"Instance","Move","Move()","(direction: Vector3, relative: boolean?) -> ()","Forces movement in 3D direction.||Fuerza movimiento en dirección 3D.||Força movimento em direção 3D.",1},
            {"Instance","LoadAnimation","LoadAnimation()","(animation: Animation) -> AnimationTrack","Prepares an animation for playback.||Prepara una animación para reproducir.||Prepara uma animação para reprodução.",1},
            {"Instance","Died","Died","RBXScriptSignal","[Event] Fires when Health reaches zero.||[Evento] Cuando la Health llega a cero.||[Evento] Quando a Health chega a zero.",2},
            {"Instance","HealthChanged","HealthChanged","RBXScriptSignal","[Event] Fires when health changes.||[Evento] Cuando cambia la salud.||[Evento] Quando a vida muda.",2},
            {"Instance","Text","Text","string","Visible content on Labels or Buttons.||Contenido visible en Labels o Buttons.||Conteúdo visível em Labels ou Buttons.",4},
            {"Instance","TextColor3","TextColor3","Color3","Font color.||Color de la fuente del texto.||Cor da fonte do texto.",4},
            {"Instance","TextScaled","TextScaled","boolean","Adjusts text to fit its box.||Ajusta el texto al tamaño de la caja.||Ajusta o texto ao tamanho da caixa.",4},
            {"Instance","BackgroundColor3","BackgroundColor3","Color3","Frame background color.||Color de fondo del frame.||Cor de fundo do frame.",4},
            {"Instance","BackgroundTransparency","BackgroundTransparency","number","UI background invisibility.||Invisibilidad del fondo de UI.||Invisibilidade do fundo da UI.",4},
            {"Instance","Visible","Visible","boolean","Whether the UI element renders.||Si la interfaz renderiza.||Se a interface renderiza.",4},
            {"Instance","ZIndex","ZIndex","number","Render depth order in UI.||Orden de renderizado en UI.||Ordem de renderização na UI.",4},
            {"Instance","MouseButton1Click","MouseButton1Click","RBXScriptSignal","[Event] Left click on the button.||[Evento] Clic izquierdo en el botón.||[Evento] Clique esquerdo no botão.",2},
            {"Instance","MouseEnter","MouseEnter","RBXScriptSignal","[Event] Mouse enters the frame.||[Evento] El mouse entra al frame.||[Evento] O mouse entra no frame.",2},
            {"Instance","MouseLeave","MouseLeave","RBXScriptSignal","[Event] Mouse leaves the frame.||[Evento] El mouse sale del frame.||[Evento] O mouse sai do frame.",2},

            {"RemoteEvent","FireServer","FireServer()","(...: any) -> ()","Sends data from client to server.||Envía datos del cliente al servidor.||Envia dados do cliente para o servidor.",1},
            {"RemoteEvent","FireClient","FireClient()","(player: Player, ...: any) -> ()","Sends data from server to one client.||Envía datos del servidor a un cliente.||Envia dados do servidor para um cliente.",1},
            {"RemoteEvent","FireAllClients","FireAllClients()","(...: any) -> ()","Broadcasts from server to all clients.||Transmite del servidor a todos.||Transmite do servidor para todos.",1},
            {"RemoteEvent","OnServerEvent","OnServerEvent","RBXScriptSignal","[Event] Server receives client :FireServer call. Args: (player, ...).||[Evento] Servidor recibe :FireServer.||[Evento] Servidor recebe :FireServer.",2},
            {"RemoteEvent","OnClientEvent","OnClientEvent","RBXScriptSignal","[Event] Client receives :FireClient call. Args: (...).||[Evento] Cliente recibe :FireClient.||[Evento] Cliente recebe :FireClient.",2},

            {"RemoteFunction","InvokeServer","InvokeServer()","(...: any) -> ...any","Calls server handler and waits for return value.||Llama al handler del servidor y espera retorno.||Chama o handler do servidor e aguarda retorno.",1},
            {"RemoteFunction","InvokeClient","InvokeClient()","(player: Player, ...: any) -> ...any","Calls client handler and waits for return value.||Llama al handler del cliente y espera.||Chama o handler do cliente e aguarda.",1},
            {"RemoteFunction","OnServerInvoke","OnServerInvoke","function","Assign function; called when client invokes. Args: (player, ...).||Asignar función; llamado al invocar desde cliente.||Atribuir função; chamado ao invocar do cliente.",4},
            {"RemoteFunction","OnClientInvoke","OnClientInvoke","function","Assign function; called when server invokes. Args: (...).||Asignar función; llamado al invocar desde servidor.||Atribuir função; chamado ao invocar do servidor.",4},

            {"BindableEvent","Fire","Fire()","(...: any) -> ()","Fires event to all same-side listeners.||Dispara evento a los oyentes del mismo lado.||Dispara evento para ouvintes do mesmo lado.",1},
            {"BindableEvent","Event","Event","RBXScriptSignal","[Event] Connect to listen for :Fire(). Same side only.||[Evento] Escuchar :Fire() del mismo lado.||[Evento] Ouvir :Fire() do mesmo lado.",2},

            {"BindableFunction","Invoke","Invoke()","(...: any) -> ...any","Synchronously calls OnInvoke handler.||Llama síncronamente al handler OnInvoke.||Chama sincronamente o handler OnInvoke.",1},
            {"BindableFunction","OnInvoke","OnInvoke","function","Assign function; called by :Invoke(). Args: (...).||Asignar función; llamada por :Invoke().||Atribuir função; chamada por :Invoke().",4},

            {nullptr,nullptr,nullptr,nullptr,nullptr,0}
        };
        return api;
    }

public:

    static Dictionary get_suggestions(const String& p_code) {
        Dictionary res;
        Array out;

        if (p_code.is_empty()) {
            res["result"]=out; res["options"]=out; res["force"]=true; res["call_hint"]=""; return res;
        }

        bool ai_enabled = false;
        if(ProjectSettings::get_singleton())
            ai_enabled = (bool)ProjectSettings::get_singleton()->get_setting("godot_luau/ai_autocomplete_enabled", false);

        auto usage_mem = analyze_usage(p_code);
        String lang = detect_language(p_code);

        int cursor_pos = p_code.find(String::chr(0xFFFF));
        if(cursor_pos==-1) cursor_pos=p_code.length();
        String code_to_cursor = p_code.substr(0, cursor_pos);

        bool in_str_d=false, in_str_s=false, in_comm=false, in_block_comm=false;
        StringContextType str_ctx = STR_CTX_NONE;

        for(int i=0;i<code_to_cursor.length();i++) {
            char32_t c=code_to_cursor[i];
            char32_t nc=(i+1<code_to_cursor.length())?code_to_cursor[i+1]:0;

            if(!in_str_d&&!in_str_s&&!in_comm&&!in_block_comm) {
                if(c=='-'&&nc=='-') {
                    if(i+3<code_to_cursor.length()&&code_to_cursor[i+2]=='['&&code_to_cursor[i+3]=='[') {
                        in_block_comm=true; i+=3;
                    } else { in_comm=true; i++; }
                } else if(c=='"'||c=='\'') {
                    if(c=='"') in_str_d=true; else in_str_s=true;
                    str_ctx = detect_string_ctx(code_to_cursor.substr(0,i));
                }
            } else {
                if(in_comm&&c=='\n') in_comm=false;
                else if(in_block_comm&&c==']'&&nc==']') { in_block_comm=false; i++; }
                else if(in_str_d&&c=='"'&&(i==0||code_to_cursor[i-1]!='\\')) { in_str_d=false; str_ctx=STR_CTX_NONE; }
                else if(in_str_s&&c=='\''&&(i==0||code_to_cursor[i-1]!='\\')) { in_str_s=false; str_ctx=STR_CTX_NONE; }
            }
        }

        bool in_string = in_str_d || in_str_s;
        if(in_comm||in_block_comm) {
            res["result"]=out; res["options"]=out; res["force"]=true; res["call_hint"]=""; return res;
        }
        if(in_string && (str_ctx==STR_CTX_NONE||str_ctx==STR_CTX_SUPPRESS)) {
            res["result"]=out; res["options"]=out; res["force"]=true; res["call_hint"]=""; return res;
        }

        int last_sep=-1;
        bool is_after_equal=false, is_after_colon=false;
        for(int i=code_to_cursor.length()-1;i>=0;i--) {
            char32_t c=code_to_cursor[i];
            bool is_word_char=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='.'||c==':';
            if(!is_word_char) {
                if(last_sep==-1) last_sep=i;
                if(c!=' '&&c!='\t'&&c!='\n') { if(c=='=') is_after_equal=true; break; }
            }
        }
        String current_word = code_to_cursor.substr(last_sep+1).strip_edges();
        if(in_string && (current_word.begins_with("\"")||current_word.begins_with("'")))
            current_word = current_word.substr(1);

        String target_prefix="", filter=current_word;
        int dot_idx=current_word.rfind(".");
        int colon_idx=current_word.rfind(":");
        if(colon_idx>dot_idx) {
            target_prefix=current_word.substr(0,colon_idx); filter=current_word.substr(colon_idx+1); is_after_colon=true;
        } else if(dot_idx!=-1) {
            target_prefix=current_word.substr(0,dot_idx); filter=current_word.substr(dot_idx+1);
        }

        std::string filter_str = filter.utf8().get_data();
        std::string filter_lower = filter.to_lower().utf8().get_data();

        std::vector<LocalVar> locals;
        std::unordered_set<std::string> known_vars;
        PackedStringArray lines = code_to_cursor.split("\n");
        for(int i=0;i<lines.size();i++) {
            String line=lines[i].strip_edges();
            if(!line.begins_with("local ")) continue;
            int eq=line.find("=");
            String var_decl=(eq!=-1)?line.substr(6,eq-6).strip_edges():line.substr(6).strip_edges();
            String var_val=(eq!=-1)?line.substr(eq+1).strip_edges():"";
            std::string vs=var_decl.utf8().get_data();
            if(known_vars.count(vs)||is_lua_keyword(var_decl)||var_decl.is_empty()) continue;
            LocalVar lv; lv.name=var_decl; lv.inferred_type="any";
            // Instance.new("ClassName") → map to Roblox type
            if(var_val.find("Instance.new(")!=-1) {
                static const char* inst_map[][2]={
                    {"\"Part\"","BasePart"},{"\"MeshPart\"","BasePart"},{"\"WedgePart\"","BasePart"},
                    {"\"CornerWedgePart\"","BasePart"},{"\"TrussPart\"","BasePart"},
                    {"\"VehicleSeat\"","BasePart"},{"\"Seat\"","BasePart"},
                    {"\"Humanoid\"","Humanoid"},{"\"Sound\"","Sound"},{"\"Model\"","Model"},
                    {"\"Tween\"","Tween"},{"\"Animation\"","AnimationTrack"},
                    {"\"RemoteEvent\"","RemoteEvent"},{"\"RemoteFunction\"","RemoteFunction"},
                    {"\"BindableEvent\"","BindableEvent"},{"\"BindableFunction\"","BindableFunction"},
                    {"\"ProximityPrompt\"","ProximityPrompt"},
                    {nullptr,nullptr}
                };
                bool matched=false;
                for(int ii=0;inst_map[ii][0];ii++) {
                    if(var_val.find(String(inst_map[ii][0]))!=-1) {
                        lv.inferred_type=String(inst_map[ii][1]); matched=true; break;
                    }
                }
                if(!matched) lv.inferred_type="Instance";
            }
            else if(var_val.begins_with("Vector3.new")) lv.inferred_type="Vector3";
            else if(var_val.begins_with("CFrame")) lv.inferred_type="CFrame";
            else if(var_val.begins_with("Color3")) lv.inferred_type="Color3";
            else if(var_val.begins_with("workspace")) lv.inferred_type="Workspace";
            else if(var_val.begins_with("script")) lv.inferred_type="Script";
            // Player / Character chains
            else if(var_val.find("LocalPlayer.Character")!=-1||var_val.find("CharacterAdded:Wait()")!=-1)
                lv.inferred_type="Model";
            else if(var_val.find(".LocalPlayer")!=-1)      lv.inferred_type="Player";
            else if(var_val.find("CurrentCamera")!=-1)     lv.inferred_type="Camera";
            else if(var_val.find(":FindFirstChildOfClass(\"Humanoid\")")!=-1||
                    var_val.find(":FindFirstChild(\"Humanoid\")")!=-1||
                    var_val.find("WaitForChild(\"Humanoid\")")!=-1)
                lv.inferred_type="Humanoid";
            else if(var_val.find("GetPlayers()")!=-1)      lv.inferred_type="Players";
            else {
                static const char* svc_pairs[][2]={
                    {"Players","Players"},{"Lighting","Lighting"},{"RunService","RunService"},
                    {"TweenService","TweenService"},{"SoundService","SoundService"},
                    {"TextChatService","TextChatService"},{"StarterGui","StarterGui"},
                    {"ReplicatedStorage","ReplicatedStorage"},{"ServerScriptService","ServerScriptService"},
                    {"UserInputService","UserInputService"},{"HttpService","HttpService"},
                    {"Debris","Debris"},{"CollectionService","CollectionService"},
                    {"DataStoreService","DataStoreService"},{"ContextActionService","ContextActionService"},
                    {"PathfindingService","PathfindingService"},{"PhysicsService","PhysicsService"},
                    {"TeleportService","TeleportService"},{"BadgeService","BadgeService"},
                    {"MarketplaceService","MarketplaceService"},{"GuiService","GuiService"},
                    {"InsertService","InsertService"},{"ScriptContext","ScriptContext"},
                    {nullptr,nullptr}
                };
                for(int si=0;svc_pairs[si][0];si++) {
                    if(var_val.find(String("GetService(\"") + String(svc_pairs[si][0]) + "\")")!=-1) {
                        lv.inferred_type=String(svc_pairs[si][1]); break;
                    }
                }
            }
            locals.push_back(lv); known_vars.insert(vs);
        }

        String resolved_type=target_prefix;
        for(const auto& v:locals) if(v.name==target_prefix) { resolved_type=v.inferred_type; break; }
        static const char* direct_maps[][2]={
            {"script","Script"},{"workspace","Workspace"},{"Workspace","Workspace"},
            {"game","DataModel"},{"game.Workspace","Workspace"},
            {"game.Players","Players"},{"game.Lighting","Lighting"},
            {"game.RunService","RunService"},{"game.TweenService","TweenService"},
            {"game.UserInputService","UserInputService"},{"game.SoundService","SoundService"},
            {"game.ReplicatedStorage","ReplicatedStorage"},{"game.ServerScriptService","ServerScriptService"},
            {"game.StarterPlayer","StarterPlayer"},{"game.StarterGui","StarterGui"},
            {"game.Players.LocalPlayer","Player"},
            {"game.Players.LocalPlayer.Character","Model"},
            {"workspace.CurrentCamera","Camera"},{"workspace.Terrain","Terrain"},
            {"RemoteEvent","RemoteEvent"},{"RemoteFunction","RemoteFunction"},
            {"BindableEvent","BindableEvent"},{"BindableFunction","BindableFunction"},
            {"RunService","RunService"},{"Players","Players"},{"Lighting","Lighting"},
            {"TweenService","TweenService"},{"UserInputService","UserInputService"},
            {"HttpService","HttpService"},{"Debris","Debris"},{"CollectionService","CollectionService"},
            {"DataStoreService","DataStoreService"},{"ContextActionService","ContextActionService"},
            {"PathfindingService","PathfindingService"},{"PhysicsService","PhysicsService"},
            {"TeleportService","TeleportService"},{"BadgeService","BadgeService"},
            {"MarketplaceService","MarketplaceService"},{"GuiService","GuiService"},
            {"InsertService","InsertService"},{"ScriptContext","ScriptContext"},
            {"SoundService","SoundService"},{"TextChatService","TextChatService"},
            {"StarterPlayer","StarterPlayer"},{"Camera","Camera"},
            {"Humanoid","Humanoid"},{"Player","Player"},{"Model","Model"},{"Sound","Sound"},
            {nullptr,nullptr}
        };
        for(int mi=0;direct_maps[mi][0];mi++)
            if(target_prefix==String(direct_maps[mi][0])) { resolved_type=String(direct_maps[mi][1]); break; }

        std::vector<TempSuggestion> sorter;

        if(in_string) {
            if(str_ctx==STR_CTX_GET_SERVICE) {
                static const char* svcs[]={"Players","RunService","Lighting","Workspace",
                    "ReplicatedStorage","ReplicatedFirst","ServerScriptService","ServerStorage",
                    "StarterGui","StarterPlayer","StarterPack","TextChatService","SoundService","Teams",
                    "TweenService","HttpService","UserInputService","Debris","CollectionService",
                    "DataStoreService","ContextActionService","PathfindingService","PhysicsService",
                    "TeleportService","BadgeService","MarketplaceService","GuiService","InsertService",
                    "ScriptContext","MaterialService",nullptr};
                for(int si=0;svcs[si];si++) {
                    String svc=String(svcs[si]);
                    int sc=fuzzy_match_score(filter_str,filter_lower,svc); if(sc<=0) continue;
                    int layer=svc.to_lower().begins_with(String(filter_str.c_str()).to_lower())?1:2;
                    std::string k=svc.utf8().get_data(); int u=usage_mem.count(k)?usage_mem.at(k):0;
                    sorter.push_back({layer,u,sc,svc,svc,String("[Service] ") + svc,"",5,Color(0.9f,0.7f,0.3f,1.0f)});
                }
            } else if(str_ctx==STR_CTX_FIND_CHILD||str_ctx==STR_CTX_WAIT_CHILD) {
                auto names=extract_known_names(p_code);
                for(const auto& n:names) {
                    String sn=String(n.c_str());
                    int sc=fuzzy_match_score(filter_str,filter_lower,sn); if(sc<=0) continue;
                    int layer=sn.to_lower().begins_with(String(filter_str.c_str()).to_lower())?1:2;
                    int u=usage_mem.count(n)?usage_mem.at(n):0;
                    sorter.push_back({layer,u+5,sc,sn + String(" [known child]"),sn,"Child name found in this script.","",5,Color(0.6f,0.95f,0.6f,1.0f)});
                }
            } else if(str_ctx==STR_CTX_IS_A||str_ctx==STR_CTX_INSTANCE_NEW) {
                static const char* classes[]={"Part","Model","Folder","Script","LocalScript","ModuleScript",
                    "Humanoid","Animation","Sound","ScreenGui","Frame","TextLabel","TextButton","TextBox",
                    "ImageLabel","ImageButton","ScrollingFrame","SurfaceGui","BillboardGui",
                    "BasePart","MeshPart","UnionOperation","SpecialMesh","Decal","Texture",
                    "PointLight","SpotLight","SurfaceLight","ParticleEmitter","Trail","Beam",
                    "BodyVelocity","BodyPosition","BodyAngularVelocity","BodyGyro","VectorForce",
                    "Attachment","WeldConstraint","Motor6D","HingeConstraint","BallSocketConstraint",
                    "RemoteEvent","RemoteFunction","BindableEvent","BindableFunction",
                    "NumberValue","StringValue","BoolValue","IntValue","ObjectValue","CFrameValue",
                    "Vector3Value","Color3Value",nullptr};
                for(int ci=0;classes[ci];ci++) {
                    String cl=String(classes[ci]);
                    int sc=fuzzy_match_score(filter_str,filter_lower,cl); if(sc<=0) continue;
                    int layer=cl.to_lower().begins_with(String(filter_str.c_str()).to_lower())?1:2;
                    std::string k=cl.utf8().get_data(); int u=usage_mem.count(k)?usage_mem.at(k):0;
                    sorter.push_back({layer,u,sc,cl,cl,String("[Class] ") + cl,"",5,Color(0.9f,0.8f,0.5f,1.0f)});
                }
            }
            std::sort(sorter.begin(),sorter.end());
            for(int i=0;i<(int)sorter.size()&&i<30;i++) {
                Dictionary s;
                s["display"]=sorter[i].display; s["insert_text"]=sorter[i].insert_text;
                s["kind"]=sorter[i].kind; s["type"]=1; s["font_color"]=sorter[i].color;
                s["icon"]=get_dummy_icon(); s["description"]=sorter[i].desc;
                s["location"]=0; s["default_value"]=Variant(); s["matches"]=Array();
                out.push_back(s);
            }
            res["result"]=out; res["options"]=out; res["force"]=true; res["call_hint"]=""; return res;
        }

        if(!target_prefix.is_empty()) {
            String cls=resolved_type.is_empty()?target_prefix:resolved_type;
            add_dynamic_suggestions(cls,filter_str,filter_lower,sorter,usage_mem);

            // Roblox type database: full property/method/signal list for known classes
            std::string roblox_cls = cls.utf8().get_data();
            const auto& roblox_mems = roblox_get_members(roblox_cls);
            for(const auto& m : roblox_mems) {
                String mname(m.name);
                int sc=fuzzy_match_score(filter_str,filter_lower,mname); if(sc<=0) continue;
                int layer=mname.to_lower().begins_with(String(filter_str.c_str()).to_lower())?1:2;
                std::string k=mname.utf8().get_data(); int u=usage_mem.count(k)?usage_mem.at(k):0;
                Color c(0.6f,0.8f,1.0f,1.0f);
                if(m.kind==1) c=Color(0.87f,0.87f,0.67f,1.0f);
                else if(m.kind==2) c=Color(0.9f,0.5f,0.8f,1.0f);
                String insert=mname;
                if(m.kind==1) { insert+=String("("); insert+=String(")"); }
                String display=mname; display+=String("  "); display+=String(m.sig);
                String desc(m.desc);
                desc+=String("\n\n[color=#888888]"); desc+=String(m.sig); desc+=String("[/color]");
                sorter.push_back({layer,u,sc,display,insert,desc,String(m.sig),m.kind,c});
            }
        }

        static const std::unordered_set<std::string> known_pfx={
            "math","string","table","task","coroutine","utf8","bit32",
            "Vector3","Vector2","CFrame","Color3","UDim","UDim2","Enum","TweenInfo","Instance",
            "game","Workspace","workspace","Players","Lighting","RunService","TweenService",
            "UserInputService","HttpService","Debris","CollectionService","DataStoreService",
            "ContextActionService","PathfindingService","PhysicsService","TeleportService",
            "BadgeService","MarketplaceService","GuiService","InsertService",
            "SoundService","TextChatService","StarterGui","ScriptContext",
            "RemoteEvent","RemoteFunction","BindableEvent","BindableFunction"
        };
        String search_pfx = target_prefix;
        if(!target_prefix.is_empty()&&!known_pfx.count(target_prefix.utf8().get_data()))
            search_pfx="Instance";

        const LuauAPIItem* api=get_api();
        for(int i=0;api[i].name!=nullptr;i++) {
            String ipfx=String(api[i].prefix), iname=String(api[i].name);
            bool matches = (ipfx==search_pfx)||(resolved_type==ipfx);
            if(target_prefix=="game"&&ipfx=="game") matches=true;
            if(target_prefix=="workspace"&&ipfx=="Workspace") matches=true;
            if(search_pfx=="Instance"&&ipfx=="game"&&target_prefix=="game") matches=true;
            if(search_pfx=="Instance"&&ipfx=="Workspace"&&target_prefix=="workspace") matches=true;
            if(!matches) continue;

            int sc=fuzzy_match_score(filter_str,filter_lower,iname); if(sc<=0) continue;
            int layer=iname.to_lower().begins_with(String(filter_str.c_str()).to_lower())?1:2;
            std::string k=iname.utf8().get_data(); int u=usage_mem.count(k)?usage_mem.at(k):0;
            Color c(0.6f,0.8f,1.0f,1.0f);
            if(api[i].kind==1) c=Color(0.87f,0.87f,0.67f,1.0f);
            else if(api[i].kind==2) c=Color(0.9f,0.5f,0.5f,1.0f);
            else if(api[i].kind==5) c=Color(0.8f,0.6f,0.4f,1.0f);
            String display = iname;
            display += String("   ");
            display += String(api[i].signature);
            String desc=make_desc(api[i].desc, lang, ipfx, iname, u);
            sorter.push_back({layer,u,sc,display,api[i].insert_text,desc,api[i].signature,api[i].kind,c});
        }

        if(target_prefix.is_empty()) {
            if(is_after_equal) {
                // Property-aware hints (highest priority when a property is detected)
                String assigned_prop = detect_assigned_property(code_to_cursor);
                if (!assigned_prop.is_empty())
                    add_property_assign_hints(assigned_prop, sorter);

                // AI variable-name hints
                if(!filter.is_empty()) {
                    auto hints=get_var_ai_hints(filter,usage_mem);
                    for(auto& h:hints) sorter.push_back(h);
                }
                struct Snip{String tr,disp,code,desc;};
                Snip eq_snips[]={
                    {"Instance","Instance.new(\"Part\")","Instance.new(\"Part\")","Create a new Part instance."},
                    {"Instance","Instance.new(\"RemoteEvent\")","Instance.new(\"RemoteEvent\")","Create a RemoteEvent."},
                    {"Instance","Instance.new(\"BindableEvent\")","Instance.new(\"BindableEvent\")","Create a BindableEvent."},
                    {"Vector3","Vector3.new(x, y, z)","Vector3.new(0, 0, 0)","3D zero vector."},
                    {"Color3","Color3.fromRGB(r,g,b)","Color3.fromRGB(255, 255, 255)","White color."},
                    {"CFrame","CFrame.new()","CFrame.new()","Identity CFrame."},
                    {"UDim2","UDim2.new(xs,xo,ys,yo)","UDim2.new(0, 0, 0, 0)","UI dimension."},
                    {"workspace","workspace","workspace","Physical world reference."},
                    {"game","game","game","DataModel root reference."},
                    {"true","true","true","Boolean true."},
                    {"false","false","false","Boolean false."},
                    {"nil","nil","nil","Empty / null value."},
                    {"0","0","0","Zero number."},
                    {"math.huge","math.huge","math.huge","Positive infinity."},
                };
                for(auto& s:eq_snips) {
                    int sc=fuzzy_match_score(filter_str,filter_lower,s.tr); if(sc<=0) continue;
                    int layer=s.tr.to_lower().begins_with(String(filter_str.c_str()).to_lower())?1:2;
                    std::string k=s.tr.utf8().get_data(); int u=usage_mem.count(k)?usage_mem.at(k):0;
                    sorter.push_back({layer,u,sc,s.disp,s.code,s.desc,"",0,Color(0.4f,0.9f,1.0f,1.0f)});
                }
            } else {
                struct Snip{String tr,disp,code,desc;};
                Snip snips[]={
                    {"local","local var","local ","Declare a local variable."},
                    {"function","function() end","function()\n\t\nend","Function block."},
                    {"for pairs","for i,v in pairs","for i, v in pairs(t) do\n\t\nend","Iterate dictionary."},
                    {"for ipairs","for i,v in ipairs","for i, v in ipairs(t) do\n\t\nend","Iterate ordered array."},
                    {"for numeric","for i = 1","for i = 1, 10 do\n\t\nend","Numeric for loop."},
                    {"while","while task.wait","while task.wait() do\n\t\nend","Infinite engine loop."},
                    {"repeat","repeat until","repeat\n\t\nuntil false","Repeat-until block."},
                    {"if","if then","if  then\n\t\nend","Condition block."},
                    {"if else","if then else","if  then\n\t\nelse\n\t\nend","If/else block."},
                    {"pcall","pcall(fn)","local ok, err = pcall(function()\n\t\nend)","Protected call."},
                    {"do","do end","do\n\t\nend","Scoped block."},
                    {"require","require(module)","require()","Load a ModuleScript."},
                    {"task.spawn","task.spawn","task.spawn(function()\n\t\nend)","Spawn async task."},
                    {"task.wait","task.wait()","task.wait()","Pause current thread."},
                    {"task.delay","task.delay(sec,fn)","task.delay(1, function()\n\t\nend)","Delayed execution."},
                    {"RemoteEvent","OnServerEvent setup","event.OnServerEvent:Connect(function(player)\n\t\nend)","Server receives client event."},
                    {"RemoteEvent","OnClientEvent setup","event.OnClientEvent:Connect(function()\n\t\nend)","Client receives server event."},
                    {"setmetatable","class OOP","local Class = {}\nClass.__index = Class\nfunction Class.new()\n\treturn setmetatable({}, Class)\nend\nreturn Class","OOP class skeleton."},
                };
                for(auto& s:snips) {
                    int sc=fuzzy_match_score(filter_str,filter_lower,s.tr); if(sc<=0) continue;
                    int layer=s.tr.to_lower().begins_with(String(filter_str.c_str()).to_lower())?1:2;
                    std::string k=s.tr.utf8().get_data(); int u=usage_mem.count(k)?usage_mem.at(k):0;
                    sorter.push_back({layer,u,sc,s.disp,s.code,s.desc,"",6,Color(0.85f,0.65f,0.95f,1.0f)});
                }
            }
            for(auto& lv:locals) {
                int sc=fuzzy_match_score(filter_str,filter_lower,lv.name); if(sc<=0||is_after_equal) continue;
                int layer=lv.name.to_lower().begins_with(String(filter_str.c_str()).to_lower())?1:2;
                std::string k=lv.name.utf8().get_data(); int u=usage_mem.count(k)?usage_mem.at(k):0;
                
                String desc = String("Local variable — type: ");
                desc += lv.inferred_type;
                sorter.push_back({layer,u,sc,lv.name,lv.name,desc,"",3,Color(0.6f,0.95f,0.6f,1.0f)});
            }
        }

        if(is_after_colon) {
            auto try_colon = [&](const String& name, const String& insert, const String& desc, int kind, Color c) {
                int sc=fuzzy_match_score(filter_str,filter_lower,name); if(sc<=0) return;
                std::string k=name.utf8().get_data(); int u=usage_mem.count(k)?usage_mem.at(k):0;
                sorter.push_back({1,u,sc,name,insert,desc,"",kind,c});
            };

            // Type-specific method suggestions from database
            if(!target_prefix.is_empty()) {
                String cls2=resolved_type.is_empty()?target_prefix:resolved_type;
                std::string rblx2=cls2.utf8().get_data();
                const auto& mems2=roblox_get_members(rblx2);
                for(const auto& m:mems2) {
                    if(m.kind!=1) continue; // methods only for colon completion
                    String mname(m.name);
                    int sc=fuzzy_match_score(filter_str,filter_lower,mname); if(sc<=0) continue;
                    std::string k=mname.utf8().get_data(); int u=usage_mem.count(k)?usage_mem.at(k):0;
                    String insert=mname; insert+=String("()");
                    String display=mname; display+=String("  "); display+=String(m.sig);
                    sorter.push_back({1,u,sc,display,insert,String(m.desc),String(m.sig),1,Color(0.87f,0.87f,0.67f,1.f)});
                }
            }

            static const Color kEvt(0.85f,0.65f,0.95f,1.f), kRem(0.9f,0.7f,0.4f,1.f),
                               kObj(0.6f,0.95f,0.6f,1.f),   kDel(0.9f,0.5f,0.5f,1.f);
            try_colon("Connect(function)","Connect(function()\n\t\nend)","Connect event handler.",2,kEvt);
            try_colon("Once(function)",   "Once(function()\n\t\nend)",   "Connect handler that fires once.",2,kEvt);
            try_colon("Wait()",           "Wait()",                       "Yield until this signal fires.",1,kEvt);
            try_colon("FireServer(...)",  "FireServer()",                  "Send event from client to server.",1,kRem);
            try_colon("FireClient(player,...)", "FireClient(player)",      "Send event to one client.",1,kRem);
            try_colon("FireAllClients(...)","FireAllClients()",            "Broadcast to all clients.",1,kRem);
            try_colon("InvokeServer(...)","InvokeServer()",                "Synchronous call to server (returns value).",1,kRem);
            try_colon("Fire(...)",        "Fire()",                        "Fire a BindableEvent.",1,kRem);
            try_colon("FindFirstChild(name)","FindFirstChild(\"\")",       "Find direct child by name.",1,kObj);
            try_colon("WaitForChild(name)","WaitForChild(\"\")",           "Wait for child to load.",1,kObj);
            try_colon("FindFirstChildOfClass(cls)","FindFirstChildOfClass(\"\")","Find child by class.",1,kObj);
            try_colon("GetService(name)", "GetService(\"\")",              "Get a service by class name.",1,kObj);
            try_colon("GetChildren()",    "GetChildren()",                 "Array of direct children.",1,kObj);
            try_colon("GetDescendants()", "GetDescendants()",              "All descendant nodes.",1,kObj);
            try_colon("IsA(className)",   "IsA(\"\")",                     "Check class type.",1,kObj);
            try_colon("Clone()",          "Clone()",                       "Deep copy this instance.",1,kObj);
            try_colon("Destroy()",        "Destroy()",                     "Remove from scene and memory.",1,kDel);
            try_colon("SetAttribute(k,v)","SetAttribute(\"\", )",          "Store custom attribute.",1,kObj);
            try_colon("GetAttribute(k)",  "GetAttribute(\"\")",            "Read custom attribute.",1,kObj);
            try_colon("TakeDamage(n)",    "TakeDamage()",                  "Reduce Humanoid health.",1,kObj);
            try_colon("LoadAnimation(a)", "LoadAnimation(animation)",      "Load animation onto Humanoid.",1,kObj);
        }

        std::sort(sorter.begin(),sorter.end());
        for(int i=0;i<(int)sorter.size()&&i<30;i++) {
            Dictionary s;
            s["display"]=sorter[i].display; s["insert_text"]=sorter[i].insert_text;
            s["kind"]=sorter[i].kind; s["type"]=1; s["font_color"]=sorter[i].color;
            s["icon"]=get_dummy_icon(); s["description"]=sorter[i].desc;
            s["location"]=0; s["default_value"]=Variant(); s["matches"]=Array();
            out.push_back(s);
        }
        res["result"]=out; res["options"]=out; res["force"]=true; res["call_hint"]=""; return res;
    }

    static String lookup_symbol(const String& symbol, const String& code) {
        String lang = detect_language(code);
        const LuauAPIItem* api = get_api();
        for(int i=0;api[i].name!=nullptr;i++) {
            String check_sym = symbol;
            check_sym += String("()");
            
            if(String(api[i].name) == symbol || String(api[i].name) == check_sym) {
                String prefix=String(api[i].prefix);
                String desc=pick_lang(api[i].desc,lang);
                
                String result = String("[b]");
                result += symbol;
                result += String("[/b]  [i]");
                result += String(api[i].signature);
                result += String("[/i]\n\n");
                
                result+=desc;
                
                static const std::unordered_set<std::string> libs={"math","string","table","task","coroutine","utf8","bit32"};
                std::string p=prefix.utf8().get_data();
                String url;
                if(libs.count(p)) {
                    url = String("https://create.roblox.com/docs/reference/engine/libraries/");
                    url += prefix;
                }
                else if(!prefix.is_empty()) {
                    url = String("https://create.roblox.com/docs/reference/engine/classes/");
                    url += prefix;
                }
                else url = "https://create.roblox.com/docs/reference/engine/globals/LuaGlobals";
                
                result += String("\n\n[color=#4d9de0][url=");
                result += url;
                result += String("]Learn More...[/url][/color]");
                
                return result;
            }
        }
        return "";
    }
};

#endif