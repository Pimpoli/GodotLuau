#ifndef ROBLOX_SOUND_H
#define ROBLOX_SOUND_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_mp3.hpp>
#include <godot_cpp/classes/audio_stream_ogg_vorbis.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/audio_server.hpp>
#include <vector>
#include <cstring>
#include <cmath>

#include "lua.h"
#include "lualib.h"

using namespace godot;

// ────────────────────────────────────────────────────────────────────
//  Sound — equivalent to Roblox Sound (can be 2D or 3D)
//  Usage from Luau:
//    local snd = Instance.new("Sound")
//    snd.SoundId = "res://sfx/jump.ogg"
//    snd.Volume = 0.8
//    snd:Play()
////
//  Sound — equivalente a Roblox Sound (puede ser 2D o 3D)
//  Uso desde Luau:
//    local snd = Instance.new("Sound")
//    snd.SoundId = "res://sfx/jump.ogg"
//    snd.Volume = 0.8
//    snd:Play()
// ────────────────────────────────────────────────────────────────────
class RobloxSound : public Node {
    GDCLASS(RobloxSound, Node);

    String  sound_id       = "";
    float   volume         = 0.5f;
    float   pitch          = 1.0f;
    bool    looped         = false;
    bool    rolloff_3d     = false;
    float   rolloff_max    = 40.0f;
    float   rolloff_min    = 10.0f;
    int     rolloff_mode   = 0;      // Enum.RollOffMode
    float   emitter_size   = 1.0f;
    bool    playing_state  = false;
    String  sound_group    = "";

    AudioStreamPlayer*   player2d = nullptr;
    AudioStreamPlayer3D* player3d = nullptr;

    void _load_stream() {
        if (sound_id.is_empty()) return;
        Ref<AudioStream> stream = ResourceLoader::get_singleton()->load(sound_id, "AudioStream");
        if (!stream.is_valid()) {
            UtilityFunctions::print("[Sound] No se pudo cargar: ", sound_id);
            return;
        }
        bool is3d = rolloff_3d && Object::cast_to<Node3D>(get_parent());

        if (is3d) {
            if (!player3d) {
                player3d = memnew(AudioStreamPlayer3D);
                player3d->set_name("__player3d__");
                add_child(player3d);
            }
            player3d->set_stream(stream);
            player3d->set_volume_db(20.0f * log10f(Math::clamp(volume, 1e-6f, 10.0f)));
            player3d->set_pitch_scale(pitch);
            _apply_spatial();
        } else {
            if (!player2d) {
                player2d = memnew(AudioStreamPlayer);
                player2d->set_name("__player2d__");
                add_child(player2d);
            }
            player2d->set_stream(stream);
            player2d->set_volume_db(20.0f * log10f(Math::clamp(volume, 1e-6f, 10.0f)));
            player2d->set_pitch_scale(pitch);
        }
        if (!sound_group.is_empty()) {
            if (player2d) player2d->set_bus(sound_group);
            if (player3d) player3d->set_bus(sound_group);
        }
        _apply_loop();
    }

    // Aplica Looped al stream cargado (Ogg/MP3/WAV) — antes no hacía nada
    void _apply_loop() {
        Ref<AudioStream> st;
        if (player2d) st = player2d->get_stream();
        else if (player3d) st = player3d->get_stream();
        if (st.is_null()) return;
        Ref<AudioStreamOggVorbis> ogg = st;
        if (ogg.is_valid()) { ogg->set_loop(looped); return; }
        Ref<AudioStreamMP3> mp3 = st;
        if (mp3.is_valid()) { mp3->set_loop(looped); return; }
        Ref<AudioStreamWAV> wav = st;
        if (wav.is_valid()) { wav->set_loop_mode(looped ? AudioStreamWAV::LOOP_FORWARD : AudioStreamWAV::LOOP_DISABLED); }
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_sound_id", "id"),    &RobloxSound::set_sound_id);
        ClassDB::bind_method(D_METHOD("get_sound_id"),          &RobloxSound::get_sound_id);
        ClassDB::bind_method(D_METHOD("set_volume", "v"),       &RobloxSound::set_volume);
        ClassDB::bind_method(D_METHOD("get_volume"),            &RobloxSound::get_volume);
        ClassDB::bind_method(D_METHOD("set_pitch", "v"),        &RobloxSound::set_pitch);
        ClassDB::bind_method(D_METHOD("get_pitch"),             &RobloxSound::get_pitch);
        ClassDB::bind_method(D_METHOD("set_looped", "v"),       &RobloxSound::set_looped);
        ClassDB::bind_method(D_METHOD("get_looped"),            &RobloxSound::get_looped);
        ClassDB::bind_method(D_METHOD("set_rolloff_3d", "v"),   &RobloxSound::set_rolloff_3d);
        ClassDB::bind_method(D_METHOD("get_rolloff_3d"),        &RobloxSound::get_rolloff_3d);
        ClassDB::bind_method(D_METHOD("set_sound_group", "v"),  &RobloxSound::set_sound_group);
        ClassDB::bind_method(D_METHOD("get_sound_group"),       &RobloxSound::get_sound_group);
        ClassDB::bind_method(D_METHOD("play"),  &RobloxSound::play);
        ClassDB::bind_method(D_METHOD("stop"),  &RobloxSound::stop);
        ClassDB::bind_method(D_METHOD("pause"), &RobloxSound::pause);
        ClassDB::bind_method(D_METHOD("resume"), &RobloxSound::resume);
        ClassDB::bind_method(D_METHOD("is_playing"), &RobloxSound::is_playing);
        ClassDB::bind_method(D_METHOD("is_paused"),  &RobloxSound::is_paused);
        ClassDB::bind_method(D_METHOD("is_loaded"),  &RobloxSound::is_loaded);
        ClassDB::bind_method(D_METHOD("get_time_position"),      &RobloxSound::get_time_position);
        ClassDB::bind_method(D_METHOD("set_time_position", "t"), &RobloxSound::set_time_position);

        ADD_PROPERTY(PropertyInfo(Variant::STRING, "SoundId"), "set_sound_id", "get_sound_id");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Volume", PROPERTY_HINT_RANGE, "0,10,0.01"), "set_volume", "get_volume");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "Pitch", PROPERTY_HINT_RANGE, "0.01,4,0.01"), "set_pitch", "get_pitch");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "Looped"), "set_looped", "get_looped");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "RollOffMode"), "set_rolloff_3d", "get_rolloff_3d");
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "SoundGroup"), "set_sound_group", "get_sound_group");
    }

public:
    void set_sound_id(const String& v) { sound_id = v; if (is_inside_tree()) _load_stream(); }
    String get_sound_id() const { return sound_id; }

    void set_volume(float v) {
        volume = Math::clamp(v, 0.0f, 10.0f);
        if (player2d) player2d->set_volume_db(20.0f * log10f(Math::clamp(volume, 1e-6f, 10.0f)));
        if (player3d) player3d->set_volume_db(20.0f * log10f(Math::clamp(volume, 1e-6f, 10.0f)));
    }
    float get_volume() const { return volume; }

    void set_pitch(float v) {
        pitch = Math::clamp(v, 0.01f, 10.0f);
        if (player2d) player2d->set_pitch_scale(pitch);
        if (player3d) player3d->set_pitch_scale(pitch);
    }
    float get_pitch() const { return pitch; }

    void set_looped(bool v) { looped = v; _apply_loop(); }
    bool get_looped() const { return looped; }

    void set_rolloff_3d(bool v) { rolloff_3d = v; }
    bool get_rolloff_3d() const { return rolloff_3d; }

    void set_sound_group(const String& v) {
        sound_group = v;
        if (player2d) player2d->set_bus(v);
        if (player3d) player3d->set_bus(v);
    }
    String get_sound_group() const { return sound_group; }

    void _ready() override {
        if (Engine::get_singleton()->is_editor_hint()) return;
        if (!sound_id.is_empty()) _load_stream();
    }

    void play() {
        if (sound_id.is_empty()) return;
        if (!player2d && !player3d) _load_stream();
        if (player2d) { player2d->stop(); player2d->play(); }
        if (player3d) { player3d->stop(); player3d->play(); }
        playing_state = true;
    }

    void stop() {
        if (player2d) player2d->stop();
        if (player3d) player3d->stop();
        playing_state = false;
    }

    void pause() {
        if (player2d) player2d->set_stream_paused(!player2d->get_stream_paused());
        if (player3d) player3d->set_stream_paused(!player3d->get_stream_paused());
    }

    bool is_playing() const {
        if (player2d) return player2d->is_playing();
        if (player3d) return player3d->is_playing();
        return false;
    }

    float get_time_position() const {
        if (player2d) return (float)player2d->get_playback_position();
        if (player3d) return (float)player3d->get_playback_position();
        return 0.0f;
    }

    // Seek (antes TimePosition era solo lectura)
    void set_time_position(float t) {
        if (player2d) player2d->seek(t);
        if (player3d) player3d->seek(t);
    }
    // Resume real (antes solo existía pause() con toggle)
    void resume() {
        if (player2d) player2d->set_stream_paused(false);
        if (player3d) player3d->set_stream_paused(false);
        playing_state = true;
    }
    bool is_paused() const {
        if (player2d) return player2d->get_stream_paused();
        if (player3d) return player3d->get_stream_paused();
        return false;
    }
    bool is_loaded() const {
        if (player2d) return player2d->get_stream().is_valid();
        if (player3d) return player3d->get_stream().is_valid();
        return false;
    }

    // ── Audio 3D espacial (rolloff) ──────────────────────────────────
    int _atten_model() const {
        switch (rolloff_mode) {
            case 2:  return 1;  // LinearSquare → InverseSquare
            case 3:  return 2;  // InverseTapered → Logarithmic
            default: return 0;  // Linear/Inverse → InverseDistance
        }
    }
    void _apply_spatial() {
        if (!player3d) return;
        player3d->set_max_distance(rolloff_max);
        player3d->set_unit_size(Math::max(rolloff_min, 0.1f));
        player3d->set_attenuation_model((AudioStreamPlayer3D::AttenuationModel)_atten_model());
    }
    // Setear RollOffMode activa el modo 3D (antes era inalcanzable desde Luau)
    void  set_rolloff_mode(int m) { rolloff_mode = m; rolloff_3d = true; if (player3d) _apply_spatial(); else if (is_inside_tree()) _load_stream(); }
    int   get_rolloff_mode() const { return rolloff_mode; }
    void  set_rolloff_min(float v) { rolloff_min = v; _apply_spatial(); }
    float get_rolloff_min() const { return rolloff_min; }
    void  set_rolloff_max(float v) { rolloff_max = v; _apply_spatial(); }
    float get_rolloff_max() const { return rolloff_max; }
    void  set_emitter_size(float v) { emitter_size = v; }
    float get_emitter_size() const { return emitter_size; }
    float get_playback_loudness() const {
        AudioServer* as = AudioServer::get_singleton();
        int bi = as->get_bus_index(sound_group.is_empty() ? "Master" : sound_group);
        if (bi < 0) return 0.0f;
        float db = as->get_bus_peak_volume_left_db(bi, 0);
        float lin = Math::pow(10.0f, db / 20.0f);
        return Math::clamp(lin * 1000.0f, 0.0f, 1000.0f);
    }
};

// ────────────────────────────────────────────────────────────────────
//  SoundGroup — equivalent to Roblox SoundGroup (audio bus)
//  Controls the volume of a group of sounds
////
//  SoundGroup — equivalente a Roblox SoundGroup (bus de audio)
//  Controla volumen de un grupo de sonidos
// ────────────────────────────────────────────────────────────────────
class RobloxSoundGroup : public Node {
    GDCLASS(RobloxSoundGroup, Node);

    String bus_name  = "Master";
    float  volume    = 1.0f;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("set_group_name", "n"), &RobloxSoundGroup::set_group_name);
        ClassDB::bind_method(D_METHOD("get_group_name"),      &RobloxSoundGroup::get_group_name);
        ClassDB::bind_method(D_METHOD("set_volume", "v"),     &RobloxSoundGroup::set_volume);
        ClassDB::bind_method(D_METHOD("get_volume"),          &RobloxSoundGroup::get_volume);
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "GroupName"), "set_group_name", "get_group_name");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "Volume", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_volume", "get_volume");
    }

public:
    void set_group_name(const String& n) {
        bus_name = n;
        _apply_volume();
    }
    String get_group_name() const { return bus_name; }

    void set_volume(float v) {
        volume = Math::clamp(v, 0.0f, 10.0f);   // Roblox: 0-10 (antes 0-1)
        _apply_volume();
    }
    float get_volume() const { return volume; }

    void _apply_volume() {
        AudioServer* as = AudioServer::get_singleton();
        int idx = as->get_bus_index(bus_name);
        if (idx < 0 && !bus_name.is_empty() && bus_name != "Master") {
            // Crear el bus si no existe (antes no hacía nada)
            idx = as->get_bus_count();
            as->add_bus();
            as->set_bus_name(idx, bus_name);
        }
        if (idx >= 0)
            as->set_bus_volume_db(idx, 20.0f * log10f(Math::clamp(volume, 1e-6f, 10.0f)));
    }

    void _ready() override {
        if (!Engine::get_singleton()->is_editor_hint()) _apply_volume();
    }
};

#endif // ROBLOX_SOUND_H
