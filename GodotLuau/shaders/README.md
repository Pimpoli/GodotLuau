# GodotLuau — Shaders

Everything that controls how the game LOOKS lives here, so you can open,
tweak or replace it to your taste. The engine loads these files at runtime;
your changes apply to every scene.

| File | What it does |
| --- | --- |
| `environment_roblox.tres` | The full lighting setup (Roblox-style look): ACES tonemap, glow/bloom, SSAO, ambient light, distance fog. Open it in the Inspector and tweak anything — every Workspace uses this file. Delete it to regenerate the default. |
| `character_fade.gdshader` | Dissolve effect when the camera gets close to your character. Discards pixels (dither) instead of alpha-blending, which is cheaper for the GPU. |

Tips:
- `environment_roblox.tres`: try `Glow > Intensity`, `SSAO > Intensity`,
  `Fog > Density`, or switch `Tonemap` for a completely different mood.
- `character_fade.gdshader`: change the noise or the `roughness` uniform.
