shader_type spatial;
render_mode cull_disabled;
// Hair cards: each ribbon is drawn as several strands of slightly different thickness and length,
// alpha-tested with coverage so edges stay smooth under MSAA. Highlights stretch along the strand
// (TANGENT runs root to tip) and thin hair lets some light through.
// UV.x runs across the card, UV.y from root (0) to tip (1); UV2.x is a random number per card.

uniform vec3 color : source_color = vec3(0.05, 0.04, 0.05);
uniform vec3 tip_color : source_color = vec3(0.12, 0.1, 0.11);
uniform float strands = 5.0;
uniform float root_fade = 0.04;
uniform float roughness = 0.42;
uniform float tint : hint_range(0.0, 1.0) = 0.0;
uniform vec3 tint_color : source_color = vec3(0.2, 0.25, 0.15);
uniform float dissolve : hint_range(0.0, 1.0) = 0.0;
uniform vec3 dissolve_color : source_color = vec3(1.0, 0.45, 0.1);
uniform float flash : hint_range(0.0, 1.0) = 0.0;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void fragment() {
	float seed = UV2.x;
	float x = UV.x * strands;
	float id = floor(x);
	float h = hash(vec2(id, seed * 91.0));
	float c = fract(x) - 0.5 + (h - 0.5) * 0.35;
	float thick = mix(0.2, 0.45, hash(vec2(id + 7.0, seed * 13.0)));
	float len = mix(0.72, 1.0, hash(vec2(id + 3.0, seed * 29.0)));
	float body = 1.0 - smoothstep(len - 0.15, len, UV.y);
	float a = (1.0 - smoothstep(thick * 0.55, thick, abs(c))) * body * smoothstep(0.0, root_fade, UV.y);
	float glow = 0.0;
	if (dissolve > 0.0) {
		// Hair goes first: tips toward roots, card by card.
		float cut = (1.0 - UV.y) * 0.7 + hash(vec2(seed * 17.0, id)) * 0.3;
		float t = dissolve * 1.6;
		if (cut < t - 0.05) {
			discard;
		}
		glow = 1.0 - smoothstep(t - 0.05, t + 0.05, cut);
	}
	ALPHA = a;
	ALPHA_SCISSOR_THRESHOLD = 0.5;
	ALPHA_ANTIALIASING_EDGE = 0.35;

	vec3 col = mix(color * 0.55, color, smoothstep(0.0, 0.3, UV.y));
	col = mix(col, tip_color, smoothstep(0.4, 1.0, UV.y) * 0.5);
	col *= 0.82 + 0.36 * h;
	ALBEDO = mix(col, tint_color, tint * 0.85);
	ROUGHNESS = roughness;
	SPECULAR = 0.3;
	ANISOTROPY = 0.7;
	BACKLIGHT = col * 0.3;
	EMISSION = dissolve_color * glow * 3.0 + vec3(flash * 0.6);
}
