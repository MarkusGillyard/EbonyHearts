shader_type canvas_item;
// The menus' moving backdrop: near-black teal with slow drifting fog, a sickly glow rising from
// the bottom of the screen like light off a slop pool, and a vignette.

uniform vec3 base : source_color = vec3(0.012, 0.025, 0.022);
uniform vec3 fog : source_color = vec3(0.12, 0.17, 0.13);
uniform vec3 glow : source_color = vec3(0.45, 0.75, 0.15);
uniform float glow_strength = 0.35;
uniform float speed = 0.02;

// Integer-free hash without sin(), which loses precision on some GPUs and shows as blocks.
float hash(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
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
	for (int i = 0; i < 5; i++) {
		v += a * noise(p);
		p = p * 2.03 + vec2(3.1, 1.7);
		a *= 0.5;
	}
	return v;
}

void fragment() {
	vec2 uv = UV;
	float aspect = 1.78;
	vec2 p = vec2(uv.x * aspect, uv.y) * 3.0;
	float t = TIME * speed;
	float f = fbm(p + vec2(t * 3.0, -t)) * fbm(p * 0.6 - vec2(t, t * 2.0) + 4.0);
	vec3 col = base + fog * f * 1.6;
	float rise = smoothstep(0.35, 1.0, uv.y);
	float flicker = 0.85 + 0.15 * noise(vec2(TIME * 0.7, 0.0));
	col += glow * glow_strength * rise * rise * (0.6 + f) * flicker;
	vec2 c = uv - 0.5;
	col *= 1.0 - dot(c, c) * 1.4;
	COLOR = vec4(col, 1.0);
}
