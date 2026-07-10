#ifndef ROBLOX_EXTRA_H
#define ROBLOX_EXTRA_H

// ════════════════════════════════════════════════════════════════════
//  Clases "estilo Roblox" que faltaban, en versión FUNCIONAL BÁSICA.
//  Cada una es un nodo real, creable con Instance.new(...), que guarda sus
//  propiedades por el puente genérico "_gl_bridge" (ver luau_api.h). El
//  comportamiento visual/físico avanzado queda como "por mejorar".
//
//  - Modificadores de UI  → base Node   (no renderizan solos)
//  - Efectos / mundo 3D   → base Node3D (posicionables)
//  - Constraints modernos → base Node3D
//  - Efectos de sonido    → base Node
// ════════════════════════════════════════════════════════════════════

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

// Macro: define un nodo puente mínimo (guarda propiedades vía _gl_bridge).
#define GL_BRIDGE_NODE(NAME, BASE)                       \
    class NAME : public BASE {                           \
        GDCLASS(NAME, BASE);                             \
    protected:                                           \
        static void _bind_methods() {}                   \
    public:                                              \
        NAME() { set_meta("_gl_bridge", true); }         \
    };

// ── Modificadores y contenedores de UI ────────────────────────────────
// (UICorner, UIListLayout, UIStroke, UIScale, UIAspectRatioConstraint y
//  UIGridLayout tienen comportamiento real en roblox_behavior.h)
GL_BRIDGE_NODE(UITableLayout, Node)
GL_BRIDGE_NODE(UIGradient, Node)
GL_BRIDGE_NODE(UITextSizeConstraint, Node)
GL_BRIDGE_NODE(ViewportFrame, Node)
GL_BRIDGE_NODE(VideoFrame, Node)
// RobloxCanvasGroup ahora es funcional en roblox_behavior.h

// ── Efectos visuales / objetos de mundo ───────────────────────────────
// (ParticleEmitter tiene comportamiento real en roblox_behavior.h)
GL_BRIDGE_NODE(Beam, Node3D)
GL_BRIDGE_NODE(Trail, Node3D)
GL_BRIDGE_NODE(Highlight, Node3D)
// Smoke, Fire, Sparkles, Explosion → funcionales en roblox_effects.h
GL_BRIDGE_NODE(SelectionBox, Node3D)
GL_BRIDGE_NODE(SelectionSphere, Node3D)
GL_BRIDGE_NODE(Clouds, Node3D)
GL_BRIDGE_NODE(RobloxDecal, Node3D)       // "Decal" colisiona con Godot
GL_BRIDGE_NODE(RobloxTexture, Node3D)     // "Texture" colisiona con Godot
GL_BRIDGE_NODE(MaterialVariant, Node)

// ── Constraints modernos (reemplazan a los BodyMovers) ────────────────
// (VectorForce, Torque, LinearVelocity, AngularVelocity, AlignPosition,
//  AlignOrientation y LineForce tienen física real en roblox_behavior.h)
// RopeConstraint → funcional en roblox_effects.h
GL_BRIDGE_NODE(PrismaticConstraint, Node3D)
GL_BRIDGE_NODE(CylindricalConstraint, Node3D)
GL_BRIDGE_NODE(PlaneConstraint, Node3D)
GL_BRIDGE_NODE(TorsionSpringConstraint, Node3D)
GL_BRIDGE_NODE(RigidConstraint, Node3D)
GL_BRIDGE_NODE(NoCollisionConstraint, Node3D)
GL_BRIDGE_NODE(UniversalConstraint, Node3D)

// ── Efectos de sonido y nueva API de audio ────────────────────────────
// (Reverb/Echo/Equalizer/Distortion/Compressor/PitchShift/Chorus SoundEffect
//  tienen efecto de audio real en roblox_behavior.h)
// FlangeSoundEffect ahora es funcional en roblox_behavior.h
GL_BRIDGE_NODE(TremoloSoundEffect, Node)
GL_BRIDGE_NODE(AudioPlayer, Node)
GL_BRIDGE_NODE(AudioEmitter, Node)
// AudioListener ahora es funcional en roblox_behavior.h
GL_BRIDGE_NODE(AudioFader, Node)
GL_BRIDGE_NODE(AudioDeviceInput, Node)
GL_BRIDGE_NODE(AudioDeviceOutput, Node)
GL_BRIDGE_NODE(Wire, Node)

// ── Asientos ──────────────────────────────────────────────────────────
GL_BRIDGE_NODE(Seat, Node3D)
GL_BRIDGE_NODE(VehicleSeat, Node3D)

#undef GL_BRIDGE_NODE

// Registro de todas las clases de este header (llamado desde register_types)
inline void gl_register_extra_classes() {
    ClassDB::register_class<UITableLayout>();
    ClassDB::register_class<UIGradient>();
    ClassDB::register_class<UITextSizeConstraint>();
    ClassDB::register_class<ViewportFrame>();
    ClassDB::register_class<VideoFrame>();

    ClassDB::register_class<Beam>();
    ClassDB::register_class<Trail>();
    ClassDB::register_class<Highlight>();
    ClassDB::register_class<SelectionBox>();
    ClassDB::register_class<SelectionSphere>();
    ClassDB::register_class<Clouds>();
    ClassDB::register_class<RobloxDecal>();
    ClassDB::register_class<RobloxTexture>();
    ClassDB::register_class<MaterialVariant>();

    ClassDB::register_class<PrismaticConstraint>();
    ClassDB::register_class<CylindricalConstraint>();
    ClassDB::register_class<PlaneConstraint>();
    ClassDB::register_class<TorsionSpringConstraint>();
    ClassDB::register_class<RigidConstraint>();
    ClassDB::register_class<NoCollisionConstraint>();
    ClassDB::register_class<UniversalConstraint>();

    ClassDB::register_class<TremoloSoundEffect>();
    ClassDB::register_class<AudioPlayer>();
    ClassDB::register_class<AudioEmitter>();
    ClassDB::register_class<AudioFader>();
    ClassDB::register_class<AudioDeviceInput>();
    ClassDB::register_class<AudioDeviceOutput>();
    ClassDB::register_class<Wire>();

    ClassDB::register_class<Seat>();
    ClassDB::register_class<VehicleSeat>();
}

#endif // ROBLOX_EXTRA_H
