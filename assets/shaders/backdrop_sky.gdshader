shader_type sky;
// A night sky for the Sump: a gradient from the horizon haze to black, a smeared glow where a
// distant light source sits (furnaces, a drowned moon), and slow cloud bands.

uniform vec3 top : source_color = vec3(0.01, 0.015, 0.02);
uniform vec3 horizon : source_color = vec3(0.09, 0.11, 0.09);
uniform vec3 glow_color : source_color = vec3(0.9, 0.45, 0.15);
uniform vec3 glow_dir = vec3(-0.3, 0.12, -1.0);
uniform float glow_size = 0.25;
uniform float glow_energy = 1.0;
uniform float clouds = 0.5;

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
	vec2 cp = d.xz / max(d.y + 0.25, 0.05);
	float c = noise(cp * 1.5 + vec2(TIME * 0.01, 0.0)) * noise(cp * 4.0 - vec2(TIME * 0.015, 0.0));
	col = mix(col, horizon * 1.4 + glow_color * 0.05, c * clouds * smoothstep(0.0, 0.3, h) * (1.0 - smoothstep(0.3, 0.8, h)));
	COLOR = col;
}
