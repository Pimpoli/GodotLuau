shader_type sky;

uniform vec3 sky_top_color : source_color = vec3(0.38, 0.65, 0.93);
uniform vec3 sky_horizon_color : source_color = vec3(0.75, 0.86, 0.95);
uniform vec3 ground_bottom_color : source_color = vec3(1.0, 1.0, 1.0);
uniform vec3 ground_horizon_color : source_color = vec3(1.0, 1.0, 1.0);

uniform vec3 sun_color : source_color = vec3(1.0, 0.99, 0.95);
uniform float sun_size : hint_range(0.001, 0.1) = 0.005;
uniform float sun_glow : hint_range(0.0, 0.1) = 0.003;

void sky() {
	float v_angle = EYEDIR.y;
	vec3 sky_gradient = mix(sky_horizon_color, sky_top_color, clamp(v_angle, 0.0, 1.0));
	vec3 ground_gradient = mix(ground_horizon_color, ground_bottom_color, clamp(-v_angle, 0.0, 1.0));
	vec3 final_color = mix(ground_gradient, sky_gradient, step(0.0, v_angle));
	float sun_dot = dot(EYEDIR, LIGHT0_DIRECTION);
	float sun_disk = smoothstep(1.0 - sun_size - sun_glow, 1.0 - sun_size, sun_dot);
	final_color = mix(final_color, sun_color, sun_disk * step(0.0, LIGHT0_DIRECTION.y));
	COLOR = final_color;
}