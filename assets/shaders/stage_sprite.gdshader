shader_type spatial;
// Glowing camera-facing sprites, meant for MultiMesh: candle and lantern flames (mode 0, upright,
// flickering), star glints that twinkle now and then (1), soft pulsing glow dots (2). Each
// instance takes its own phase from its position. Additive and unshaded, so hundreds cost almost
// nothing; fog is replaced by a distance fade because additive fog would light up the quads.
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled, fog_disabled, skip_vertex_transform;

uniform vec3 color : source_color = vec3(1.0, 0.55, 0.2);
uniform float energy = 4.0;
uniform int mode = 0;
uniform float flicker = 1.0;
uniform float fade = 0.02;

varying float seed;
varying float dist;

void vertex() {
	vec3 origin = MODEL_MATRIX[3].xyz;
	seed = fract(sin(dot(origin, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
	float s = length(MODEL_MATRIX[1].xyz);
	vec3 v = VERTEX * s;
	if (mode == 0) {
		float t = TIME * (6.0 + seed * 5.0) + seed * 40.0;
		v.y *= 1.0 + flicker * (0.14 * sin(t) + 0.08 * sin(t * 2.3 + 1.0) + 0.05 * sin(t * 5.1));
		v.x += sin(t * 0.37) * 0.25 * v.y * flicker;
	}
	vec3 to_cam = normalize(CAMERA_POSITION_WORLD - origin);
	vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), to_cam));
	vec3 up = mode == 0 ? vec3(0.0, 1.0, 0.0) : normalize(cross(to_cam, right));
	vec3 world = origin + right * v.x + up * v.y;
	dist = length(CAMERA_POSITION_WORLD - world);
	VERTEX = (VIEW_MATRIX * vec4(world, 1.0)).xyz;
	NORMAL = vec3(0.0, 0.0, 1.0);
}

void fragment() {
	vec2 p = vec2(UV.x * 2.0 - 1.0, 1.0 - UV.y * 2.0);
	vec3 c = vec3(0.0);
	if (mode == 0) {
		// A teardrop: round below the centre, drawn to a point above it, with a white-hot core.
		vec2 q = vec2(p.x, p.y + 0.35);
		float up = clamp(q.y / 1.3, 0.0, 1.0);
		q.x /= mix(0.55, 0.12, up);
		q.y /= q.y > 0.0 ? 1.3 : 0.6;
		float r = length(q);
		float body = 1.0 - smoothstep(0.35, 1.0, r);
		float hot = 1.0 - smoothstep(0.0, 0.55, r);
		float blue = (1.0 - smoothstep(0.0, 0.5, length(vec2(p.x * 2.0, p.y + 0.75)))) * 0.25;
		c = color * body + vec3(1.0, 0.92, 0.75) * hot * 0.9 + vec3(0.2, 0.3, 1.0) * blue;
	} else if (mode == 1) {
		float tw = pow(max(sin(TIME * (0.6 + seed * 0.9) + seed * 60.0), 0.0), 24.0);
		float star = max(1.0 - abs(p.x) * 14.0, 0.0) * (1.0 - abs(p.y)) + max(1.0 - abs(p.y) * 14.0, 0.0) * (1.0 - abs(p.x));
		float core = 1.0 - smoothstep(0.0, 0.25, length(p));
		c = color * (star * 0.8 + core) * tw;
	} else {
		float pulse = 0.75 + 0.25 * sin(TIME * (0.8 + seed) + seed * 30.0);
		float r = length(p);
		c = color * pow(max(1.0 - r, 0.0), 2.0) * pulse;
	}
	ALBEDO = c * energy * exp(-dist * fade);
}
