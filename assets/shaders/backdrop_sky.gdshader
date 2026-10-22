shader_type sky;
// A night sky for the Sump: a gradient from the horizon haze to black, a smeared glow where a
// distant light source sits (furnaces, a drowned moon), slow cloud bands, and optionally a moon
// disc (moon_size > 0, angular radius in radians) and faint stars, both veiled by the clouds.

uniform vec3 top : source_color = vec3(0.01, 0.015, 0.02);
uniform vec3 horizon : source_color = vec3(0.09, 0.11, 0.09);
uniform vec3 glow_color : source_color = vec3(0.9, 0.45, 0.15);
uniform vec3 glow_dir = vec3(-0.3, 0.12, -1.0);
uniform float glow_size = 0.25;
uniform float glow_energy = 1.0;
uniform float clouds = 0.5;
uniform vec3 moon_dir = vec3(0.4, 0.3, -1.0);
uniform float moon_size = 0.0;
uniform vec3 moon_color : source_color = vec3(0.85, 0.9, 0.82);
uniform float stars = 0.0;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 x) {
	vec2 i = floor(x);
	vec2 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}

void sky() {
	vec3 d = EYEDIR;
	float h = clamp(d.y, -0.2, 1.0);
	vec3 col = mix(horizon, top, smoothstep(-0.05, 0.55, h));
	float g = max(dot(normalize(d), normalize(glow_dir)), 0.0);
	col += glow_color * glow_energy * (pow(g, 1.0 / max(glow_size, 0.01) * 6.0) * 0.8 + pow(g, 3.0) * 0.12);
	if (stars > 0.0) {
		vec2 sp = d.xy / max(abs(d.z) + 0.3, 0.1) * 160.0;
		float s = step(0.9965, hash(floor(sp))) * (1.0 - smoothstep(0.1, 0.5, length(fract(sp) - 0.5)));
		col += vec3(0.75, 0.8, 0.9) * s * stars * smoothstep(0.05, 0.3, h);
	}
	if (moon_size > 0.0) {
		float ang = acos(clamp(dot(d, normalize(moon_dir)), -1.0, 1.0));
		float disc = 1.0 - smoothstep(moon_size * 0.92, moon_size, ang);
		vec2 mp = (d.xy - normalize(moon_dir).xy) / moon_size;
		float maria = noise(mp * 2.5 + 3.0) * 0.6 + noise(mp * 6.0) * 0.4;
		col = mix(col, moon_color * (1.15 - maria * 0.45), disc);
		col += moon_color * (exp(-ang / moon_size * 0.9) * 0.18 + exp(-ang * 4.0) * 0.05);
	}
	vec2 cp = d.xz / max(d.y + 0.25, 0.05);
	float c = noise(cp * 1.5 + vec2(TIME * 0.01, 0.0)) * noise(cp * 4.0 - vec2(TIME * 0.015, 0.0));
	col = mix(col, horizon * 1.4 + glow_color * 0.05, c * clouds * smoothstep(0.0, 0.3, h) * (1.0 - smoothstep(0.3, 0.8, h)));
	COLOR = col;
}
