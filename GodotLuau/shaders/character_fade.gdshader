// GodotLuau — Character fade (dissolve) shader
// =============================================
// Used on the player's character when the camera gets close: instead of
// classic alpha transparency (which still draws every pixel and blends),
// this shader DISCARDS pixels with a screen-space dither pattern, so the
// GPU simply skips them. dissolve = 0 draws the full character, 1 = gone.
//
// Feel free to tweak or replace it — the game loads it from this file.

shader_type spatial;
render_mode cull_back;

uniform sampler2D albedo_tex : source_color, filter_linear_mipmap;
uniform float dissolve : hint_range(0.0, 1.0) = 0.0;
uniform float roughness : hint_range(0.0, 1.0) = 0.95;

void fragment() {
	// Interleaved gradient noise: stable per-pixel threshold, no texture needed
	float threshold = fract(52.9829189 * fract(dot(FRAGCOORD.xy, vec2(0.06711056, 0.00583715))));
	if (dissolve >= threshold) {
		discard;
	}
	ALBEDO = texture(albedo_tex, UV).rgb;
	ROUGHNESS = roughness;
}
