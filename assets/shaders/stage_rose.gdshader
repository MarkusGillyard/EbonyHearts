shader_type spatial;
// Stained glass on a quad: a rose window (shape 0: an oculus, sixteen lancets with pointed heads,
// a ring of thirty-two lobes) or a lancet window (shape 1: two lights under a pointed arch with a
// quatrefoil). Stone tracery is lit like the wall; the glass glows with cold moonlight, pieced by
// lead cames into small panes of blues and moon-white with a few violet and ruby ones. The glass
// skips the scene fog (it is the light source seen through the haze); the stone is fogged here.
render_mode fog_disabled;

uniform int shape = 0;
uniform vec3 stone : source_color = vec3(0.22, 0.23, 0.25);
uniform float energy = 3.0;
uniform vec2 moon_from = vec2(-0.5, 0.7);
uniform vec3 fog_color : source_color = vec3(0.06, 0.075, 0.1);
uniform float fog_density = 0.03;

varying float dist;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 x) {
	vec2 i = floor(x);
	vec2 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}

vec3 glass_color(float h) {
	if (h < 0.48) return vec3(0.07, 0.17, 1.0);
	if (h < 0.68) return vec3(0.12, 0.45, 0.85);
	if (h < 0.86) return vec3(0.7, 0.82, 1.0);
	if (h < 0.95) return vec3(0.35, 0.2, 0.85);
	return vec3(0.85, 0.1, 0.12);
}

// Voronoi panes: returns the nearest cell id and the gap to the second nearest (for the lead).
vec3 panes(vec2 p) {
	vec2 g = floor(p);
	vec2 f = fract(p);
	float d1 = 8.0;
	float d2 = 8.0;
	vec2 id = vec2(0.0);
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 o = vec2(float(x), float(y));
			vec2 c = o + vec2(hash(g + o), hash(g + o + 5.3)) * 0.8 + 0.1;
			float d = length(f - c);
			if (d < d1) {
				d2 = d1;
				d1 = d;
				id = g + o;
			} else if (d < d2) {
				d2 = d;
			}
		}
	}
	return vec3(id, d2 - d1);
}

float line(float d, float w) {
	return 1.0 - smoothstep(w * 0.6, w, abs(d));
}

void vertex() {
	dist = length((MODELVIEW_MATRIX * vec4(VERTEX, 1.0)).xyz);
}

void fragment() {
	vec2 p = (UV - 0.5) * 2.0;
	p.y = -p.y;
	float st = 0.0;
	float zone = 0.0;
	if (shape == 0) {
		float r = length(p);
		if (r > 1.0) {
			discard;
		}
		float a = atan(p.y, p.x);
		st = max(st, step(0.955, r));
		st = max(st, line(r - 0.64, 0.022));
		st = max(st, line(r - 0.15, 0.02));
		if (r < 0.15) {
			st = max(st, line(r - 0.07 - 0.03 * abs(sin(a * 3.0)), 0.012));
		} else if (r < 0.64) {
			float s = a / 6.2831853 * 16.0;
			float x = (fract(s) - 0.5) * 2.0;
			st = max(st, line((1.0 - abs(x)) * r * 0.196, 0.016));
			float head = 0.56 - 0.12 * pow(abs(x), 1.2);
			st = max(st, line(r - head, 0.014));
			st = max(st, line(length(vec2(x * r * 0.196, r - 0.6)) - 0.022, 0.008) * step(head, r));
			zone = 1.0 + floor(s) * 0.1 + step(head, r);
		} else {
			float s = a / 6.2831853 * 32.0;
			float x = (fract(s) - 0.5) * 2.0;
			st = max(st, line((1.0 - abs(x)) * r * 0.098, 0.012));
			float lobe = 0.9 - 0.16 * x * x;
			st = max(st, line(r - lobe, 0.012));
			zone = 3.0 + step(lobe, r);
		}
	} else {
		// A lancet in a 1:3 quad: p.x across, p.y up.
		vec2 q = vec2(p.x, p.y * 3.0);
		float arch = q.y - 1.2;
		float w = arch > 0.0 ? 1.0 - pow(arch / 1.8, 1.3) : 1.0;
		if (abs(q.x) > w || q.y > 3.0) {
			discard;
		}
		st = max(st, step(w - 0.12, abs(q.x)));
		st = max(st, line(q.x, 0.06) * step(q.y, 1.25));
		st = max(st, line(q.y + 2.92, 0.08));
		float qf = length(vec2(q.x, q.y - 1.85)) - 0.28 - 0.08 * abs(sin(atan(q.y - 1.85, q.x) * 2.0));
		st = max(st, line(qf, 0.05));
		st = max(st, line(arch - 0.05 + abs(q.x) * 0.3, 0.05) * step(0.0, 1.0 - abs(q.x) * 1.4));
		zone = step(0.0, q.x) + step(1.2, q.y) * 2.0;
	}
	vec3 pc = panes(shape == 0 ? p * 11.0 : vec2(p.x, p.y * 3.0) * 2.6);
	float h = hash(pc.xy + zone * 7.31);
	vec3 g = glass_color(h);
	float lead = 1.0 - smoothstep(0.04, 0.09, pc.z);
	float density = 0.4 + 0.75 * hash(pc.xy + 1.7) * hash(pc.xy + 4.2);
	float lit = 0.8 + 0.35 * dot(normalize(p + 0.0001), normalize(moon_from)) * min(length(p), 1.0);
	lit *= 0.9 + 0.1 * sin(TIME * 0.3 + p.x * 2.0);
	float glass = (1.0 - st) * (1.0 - lead * 0.9);
	float fog = 1.0 - exp(-dist * fog_density);
	vec3 st_col = stone * (0.7 + noise(p * 24.0) * 0.5);
	ALBEDO = mix(vec3(0.015), st_col, st) * (1.0 - fog);
	ROUGHNESS = 0.85;
	EMISSION = g * energy * glass * lit * density + fog_color * fog * 0.6;
}
