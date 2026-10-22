shader_type spatial;
// Procedural surfaces for the arenas, patterned in world space so meshes need no UVs:
//   pattern 0 weathered plaster or rock, 1 stone flags, 2 wooden planks, 3 iron plate with
//   rivets, 4 brick.
// `wet` darkens the albedo and lowers roughness in the hollows, where water would collect.

uniform int pattern = 0;
uniform vec3 color_a : source_color = vec3(0.32, 0.31, 0.29);
uniform vec3 color_b : source_color = vec3(0.2, 0.2, 0.19);
uniform float scale = 1.0;
uniform float roughness = 0.85;
uniform float metallic = 0.0;
uniform float wet = 0.0;
uniform float grime = 0.4;
// How dark the joints between flags, planks or bricks are (lower under standing water).
uniform float mortar_dark = 0.65;
uniform vec3 emission_color : source_color = vec3(0.0);
uniform float emission_energy = 0.0;

varying vec3 wpos;
varying vec3 wnorm;

float hash(vec3 p) {
	p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float hash2(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec3 x) {
	vec3 i = floor(x);
	vec3 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash(i), hash(i + vec3(1, 0, 0)), f.x), mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
		mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x), mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

float fbm(vec3 p) {
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < 4; i++) {
		v += a * noise(p);
		p *= 2.03;
		a *= 0.5;
	}
	return v;
}

void vertex() {
	wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	wnorm = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

// Coordinates on the dominant plane of the surface.
vec2 plane_uv(vec3 p, vec3 n) {
	vec3 a = abs(n);
	if (a.y > a.x && a.y > a.z) {
		return p.xz;
	} else if (a.x > a.z) {
		return p.zy;
	}
	return p.xy;
}

void fragment() {
	vec3 p = wpos / scale;
	vec2 uv = plane_uv(p, wnorm);
	float n = fbm(p * 3.0);
	float fine = noise(p * 40.0);
	float mortar = 0.0;
	float cell = 0.0;
	if (pattern == 1) {
		vec2 g = uv * vec2(1.6, 1.6);
		vec2 id = floor(g + vec2(floor(g.y) * 0.5, 0.0));
		vec2 f = fract(g + vec2(floor(g.y) * 0.5, 0.0));
		cell = hash2(id);
		float e = min(min(f.x, 1.0 - f.x), min(f.y, 1.0 - f.y));
		mortar = 1.0 - smoothstep(0.015, 0.05, e + (n - 0.5) * 0.03);
	} else if (pattern == 2) {
		vec2 g = vec2(uv.x * 0.35, uv.y * 5.0);
		vec2 id = vec2(floor(g.x + floor(g.y) * 0.37), floor(g.y));
		vec2 f = vec2(fract(g.x + floor(g.y) * 0.37), fract(g.y));
		cell = hash2(id);
		mortar = 1.0 - smoothstep(0.02, 0.07, min(f.y, 1.0 - f.y));
		mortar = max(mortar, 1.0 - smoothstep(0.004, 0.02, min(f.x, 1.0 - f.x)));
		n = mix(n, sin((uv.x + cell * 7.0) * 30.0 + n * 6.0) * 0.5 + 0.5, 0.35);
	} else if (pattern == 3) {
		vec2 g = uv * 0.9;
		vec2 f = fract(g);
		cell = hash2(floor(g));
		float e = min(min(f.x, 1.0 - f.x), min(f.y, 1.0 - f.y));
		mortar = 1.0 - smoothstep(0.004, 0.012, e);
		vec2 r = fract(g * 6.0) - 0.5;
		float rivet = 1.0 - smoothstep(0.08, 0.12, length(r));
		rivet *= step(e, 0.09);
		cell += rivet * 0.6;
	} else if (pattern == 4) {
		vec2 g = uv * vec2(4.0, 8.0);
		vec2 id = floor(g + vec2(floor(g.y) * 0.5, 0.0));
		vec2 f = fract(g + vec2(floor(g.y) * 0.5, 0.0));
		cell = hash2(id);
		float e = min(min(f.x, 1.0 - f.x) * 0.5, min(f.y, 1.0 - f.y));
		mortar = 1.0 - smoothstep(0.03, 0.08, e);
	}
	vec3 col = mix(color_a, color_b, clamp(n * 0.9 + cell * 0.35 - 0.2, 0.0, 1.0));
	col *= 0.9 + fine * 0.2;
	col = mix(col, col * (1.0 - mortar_dark), mortar);
	// Grime gathers low down and in cracks.
	float low = 1.0 - smoothstep(0.0, 1.2, wpos.y);
	col *= 1.0 - grime * (0.35 * low * n + 0.25 * mortar);
	float puddle = wet * smoothstep(0.45, 0.62, fbm(p * 0.9 + vec3(3.1)) + mortar * 0.3) * step(0.7, wnorm.y);
	float damp = wet * (0.5 + 0.5 * mortar);
	ALBEDO = col * (1.0 - damp * 0.35 - puddle * 0.25);
	ROUGHNESS = clamp(roughness - damp * 0.35 - puddle * 0.7 + fine * 0.1, 0.04, 1.0);
	METALLIC = metallic * (1.0 - mortar);
	SPECULAR = 0.5 + puddle * 0.3;
	EMISSION = emission_color * emission_energy;
}
