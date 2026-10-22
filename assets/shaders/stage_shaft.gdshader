shader_type spatial;
// A soft beam of light drawn on a flat card running from the light down to where it lands (built
// by Arena.shaft to face the camera side): brightest at the source, fading along its length and
// at its edges, with faint streaks and slow drifting dust. Depth-faded where it meets geometry so
// it never shows a hard edge on floors or walls. With `ripple` the same card lying on water is a
// shimmering reflection streak broken into moving bands. Vertex colour tints each card.
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled, fog_disabled;

uniform vec3 color : source_color = vec3(0.6, 0.75, 1.0);
uniform float energy = 0.5;
uniform float softness = 1.5;
uniform float fade = 0.01;
uniform float ripple = 0.0;
uniform sampler2D depth_tex : hint_depth_texture, filter_nearest;

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

void vertex() {
	wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	float across = abs(UV.x - 0.5) * 2.0;
	float edge = pow(max(1.0 - across * across, 0.0), 2.0);
	float fall = smoothstep(0.0, 0.08, UV.y) * (1.0 - smoothstep(0.35, 1.0, UV.y));
	float streak = 0.65 + 0.35 * noise(vec2(UV.x * 14.0, UV.y * 0.6 - TIME * 0.015));
	float dust = 0.75 + 0.25 * noise(wpos.xy * 1.7 + vec2(TIME * 0.05, -TIME * 0.08));
	streak *= mix(1.0, smoothstep(0.3, 0.75, noise(vec2(UV.x * 3.0 + wpos.x, UV.y * 70.0 - TIME * 0.9))), ripple);
	float depth = texture(depth_tex, SCREEN_UV).r;
	vec4 v = INV_PROJECTION_MATRIX * vec4(SCREEN_UV * 2.0 - 1.0, depth, 1.0);
	float soft = clamp((-v.z / v.w + VERTEX.z) / softness, 0.0, 1.0);
	ALBEDO = color * COLOR.rgb * energy * edge * fall * streak * dust * soft * exp(-length(VERTEX) * fade);
}
