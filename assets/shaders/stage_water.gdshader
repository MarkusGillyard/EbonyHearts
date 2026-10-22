shader_type spatial;
// Black bog water: still and glossy, broken by slow swells, with drifting patches of duckweed and
// scum that dull the reflection. Opaque, so it takes screen-space reflections on high settings.

uniform vec3 deep : source_color = vec3(0.01, 0.018, 0.02);
uniform vec3 scum : source_color = vec3(0.07, 0.08, 0.04);
uniform float scum_amount = 0.35;
uniform float swell = 0.5;
uniform float roughness = 0.06;

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

float height(vec2 p, float t) {
	return noise(p * 0.35 + vec2(t * 0.03, t * 0.02)) * 0.6 + noise(p * 1.1 - vec2(t * 0.05, -t * 0.04)) * 0.3
		+ noise(p * 3.7 + vec2(t * 0.12, 0.0)) * 0.1;
}

void vertex() {
	wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec2 p = wpos.xz;
	float t = TIME;
	float e = 0.08;
	float h0 = height(p, t);
	vec2 slope = vec2(height(p + vec2(e, 0.0), t) - h0, height(p + vec2(0.0, e), t) - h0) / e * swell * 0.3;
	float patch = smoothstep(0.58, 0.72, noise(p * 0.22 + vec2(t * 0.004, 0.0)) * 0.7 + noise(p * 1.9) * 0.3) * scum_amount;
	vec3 n = normalize(vec3(-slope.x * (1.0 - patch), 1.0, -slope.y * (1.0 - patch)));
	NORMAL = normalize((VIEW_MATRIX * vec4(n, 0.0)).xyz);
	ALBEDO = mix(deep, scum * (0.7 + noise(p * 9.0) * 0.6), patch);
	ROUGHNESS = mix(roughness, 0.85, patch);
	SPECULAR = 0.5;
}
