shader_type spatial;
// A thin sheet of standing water over a floor, a few centimetres deep: clear when seen from above,
// a reflecting mirror at grazing angles, with slow ripples and rings where drips land.
// Premultiplied blending keeps reflections at full strength while the water itself stays clear.
// Feet stand in it rather than on it, so it must never be opaque.
render_mode blend_premul_alpha, depth_draw_never, cull_back, shadows_disabled;

uniform vec3 tint : source_color = vec3(0.02, 0.04, 0.05);
uniform float clarity = 0.75;
// How far the water turns opaque at grazing angles; low lets a glossy floor's reflections through.
uniform float murk = 1.0;
uniform float ripple = 0.35;
uniform float drips = 1.0;
uniform float drip_scale = 0.9;
uniform float roughness = 0.05;

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

float waves(vec2 p, float t) {
	return noise(p * 1.3 + vec2(t * 0.11, t * 0.07)) * 0.6 + noise(p * 3.1 - vec2(t * 0.09, -t * 0.13)) * 0.4;
}

// Slope of expanding drip rings on a jittered grid; each cell drips on its own clock.
vec2 rings(vec2 p, float t) {
	vec2 g = floor(p);
	vec2 f = fract(p);
	vec2 slope = vec2(0.0);
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 o = vec2(float(x), float(y));
			vec2 id = g + o;
			float h = hash(id);
			vec2 c = o + vec2(hash(id + 3.1), hash(id + 7.7));
			float ph = fract(t * (0.12 + h * 0.15) + h * 11.0);
			vec2 dv = f - c;
			float d = length(dv);
			float r = ph * 0.9;
			float w = sin((d - r) * 38.0) * (1.0 - smoothstep(0.0, 0.12, abs(d - r))) * (1.0 - ph) * (1.0 - ph);
			slope += normalize(dv + 1e-4) * w * step(0.55, h);
		}
	}
	return slope;
}

void vertex() {
	wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec2 p = wpos.xz;
	float t = TIME;
	float e = 0.05;
	float h0 = waves(p, t);
	vec2 slope = vec2(waves(p + vec2(e, 0.0), t) - h0, waves(p + vec2(0.0, e), t) - h0) / e * ripple * 0.12;
	slope += rings(p * drip_scale, t) * drips * 0.18;
	vec3 n = normalize(vec3(-slope.x, 1.0, -slope.y));
	vec3 nv = normalize((VIEW_MATRIX * vec4(n, 0.0)).xyz);
	float fres = pow(1.0 - clamp(dot(nv, VIEW), 0.0, 1.0), 4.0);
	ALPHA = mix(1.0 - clarity, 1.0, fres * murk);
	ALBEDO = tint * ALPHA;
	NORMAL = nv;
	ROUGHNESS = roughness;
	METALLIC = 0.0;
	SPECULAR = 0.5;
}
