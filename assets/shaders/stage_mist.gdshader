shader_type spatial;
// Drifting mist on flat cards (lying on water or standing in the distance): noisy, slow, faded at
// the card's edges, near the camera and where it meets geometry, so no card edge ever shows.
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled, shadows_disabled, fog_disabled;

uniform vec3 color : source_color = vec3(0.4, 0.45, 0.5);
uniform float density = 0.5;
uniform float scale = 0.35;
uniform float speed = 0.04;
uniform float softness = 1.0;
uniform float near_fade = 4.0;
uniform sampler2D depth_tex : hint_depth_texture, filter_nearest;

varying vec3 wpos;

float hash(vec3 p) {
	p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(vec3 x) {
	vec3 i = floor(x);
	vec3 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash(i), hash(i + vec3(1, 0, 0)), f.x), mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
		mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x), mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

void vertex() {
	wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec3 p = wpos * scale;
	float t = TIME * speed;
	float n = noise(p + vec3(t, 0.0, t * 0.6)) * 0.6 + noise(p * 2.3 - vec3(t * 1.4, 0.0, 0.0)) * 0.4;
	n = smoothstep(0.3, 0.8, n);
	vec2 e = min(UV, 1.0 - UV);
	float edge = smoothstep(0.0, 0.25, e.x) * smoothstep(0.0, 0.25, e.y);
	float depth = texture(depth_tex, SCREEN_UV).r;
	vec4 v = INV_PROJECTION_MATRIX * vec4(SCREEN_UV * 2.0 - 1.0, depth, 1.0);
	float soft = clamp((-v.z / v.w + VERTEX.z) / softness, 0.0, 1.0);
	float near = smoothstep(near_fade * 0.5, near_fade, -VERTEX.z);
	ALBEDO = color;
	ALPHA = clamp(density * n * edge * soft * near, 0.0, 1.0);
}
