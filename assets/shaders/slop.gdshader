shader_type spatial;
// Living slop: a thick, slowly churning liquid with a sickly glow in its folds and bubbles that
// swell and pop. Used for vats, pools and the roost's tar. World-space, so any flat mesh works.

uniform vec3 deep : source_color = vec3(0.04, 0.07, 0.02);
uniform vec3 glow : source_color = vec3(0.45, 0.85, 0.12);
uniform float glow_energy = 1.4;
uniform float speed = 0.25;
uniform float bubble_scale = 3.0;
uniform float roughness = 0.12;

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

float fbm(vec2 p) {
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < 4; i++) {
		v += a * noise(p);
		p = p * 2.1 + vec2(1.7, 9.2);
		a *= 0.5;
	}
	return v;
}

// Bubbles on a jittered grid, each with its own phase: a ring that swells, then vanishes.
float bubbles(vec2 p, float t) {
	vec2 g = floor(p);
	vec2 f = fract(p);
	float best = 0.0;
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 o = vec2(float(x), float(y));
			vec2 id = g + o;
			float h = hash(id);
			vec2 c = o + vec2(hash(id + 3.1), hash(id + 7.7)) * 0.8 + 0.1;
			float phase = fract(t * (0.25 + h * 0.3) + h * 13.0);
			float r = phase * 0.35 * (0.5 + h);
			float d = length(f - c);
			float ring = smoothstep(r - 0.06, r, d) * (1.0 - smoothstep(r, r + 0.02, d));
			best = max(best, ring * (1.0 - phase) * step(0.4, h));
		}
	}
	return best;
}

void vertex() {
	wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	float t = TIME * speed;
	vec2 p = wpos.xz;
	vec2 warp = vec2(fbm(p * 0.7 + t), fbm(p * 0.7 - t + 4.0));
	float folds = fbm(p * 1.4 + warp * 2.0 + vec2(t * 0.5, 0.0));
	float b = bubbles(p * bubble_scale, TIME);
	float lit = smoothstep(0.45, 0.8, folds) * 0.8 + b;
	ALBEDO = mix(deep, deep * 2.0, folds);
	ROUGHNESS = roughness + folds * 0.2;
	SPECULAR = 0.6;
	EMISSION = glow * glow_energy * lit;
	// Folds as a normal perturbation so highlights crawl across the surface.
	vec3 n = normalize(vec3((fbm(p * 1.4 + warp * 2.0 + vec2(0.02, 0.0)) - folds) * 25.0, 1.0, (fbm(p * 1.4 + warp * 2.0 + vec2(0.0, 0.02)) - folds) * 25.0));
	NORMAL = normalize((VIEW_MATRIX * vec4(n, 0.0)).xyz);
}
