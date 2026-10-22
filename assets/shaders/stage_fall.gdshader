shader_type spatial;
// Falling liquid on a vertical strip (QuadMesh, UV.x across it): tar falls in the roost, slop
// spilling over a vat's lip. Streaks race downward in world space, the edges fray, and an optional
// glow rides in the streaks.
render_mode cull_disabled;

uniform vec3 albedo : source_color = vec3(0.01, 0.012, 0.01);
uniform vec3 glow : source_color = vec3(0.4, 0.9, 0.1);
uniform float glow_energy = 0.0;
// Share of the glow that runs through the whole stream rather than only its streaks.
uniform float glow_floor = 0.0;
uniform float speed = 1.6;
uniform float roughness = 0.12;
uniform float fray = 0.3;

varying vec3 wpos;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 x) {
	vec2 i = floor(x);
	vec2 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}

float streaks(vec2 p) {
	return noise(vec2(p.x * 9.0, p.y * 0.8)) * 0.65 + noise(vec2(p.x * 23.0, p.y * 2.1)) * 0.35;
}

void vertex() {
	wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec2 p = vec2(UV.x, wpos.y + TIME * speed);
	float s = streaks(p);
	float edge = abs(UV.x - 0.5) * 2.0;
	ALPHA = 1.0 - smoothstep(1.0 - fray, 1.0, edge + (noise(vec2(UV.x * 5.0, p.y * 1.7)) - 0.5) * fray * 1.6);
	ALPHA_SCISSOR_THRESHOLD = 0.5;
	float ds = streaks(p + vec2(0.01, 0.0)) - s;
	NORMAL = normalize(NORMAL + TANGENT * ds * 12.0);
	ALBEDO = albedo * (0.6 + s * 0.8);
	ROUGHNESS = roughness + (1.0 - s) * 0.15;
	SPECULAR = 0.6;
	EMISSION = glow * glow_energy * mix(smoothstep(0.55, 0.85, s), 0.4 + s * 0.8, glow_floor);
}
