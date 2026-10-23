shader_type canvas_item;
render_mode blend_mul, unshaded;
// Over the fight: a soft vignette that keeps the eye on the fighters and a faint, moving grain,
// so the picture reads as a dark print rather than a clean render. Multiplied over the frame:
// no screen read, one quad, a fraction of a millisecond.

uniform float vignette : hint_range(0.0, 1.0) = 0.42;
uniform float grain : hint_range(0.0, 0.3) = 0.07;

float hash(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

void fragment() {
	vec2 c = UV - 0.5;
	c.x *= 1.3;
	float d = dot(c, c) * 2.2;
	float v = 1.0 - vignette * smoothstep(0.22, 1.25, d);
	vec2 cell = floor(FRAGCOORD.xy * 0.5) + floor(fract(TIME * 7.3) * 97.0) * vec2(31.0, 17.0);
	float g = 1.0 + (hash(cell) - 0.5) * grain;
	COLOR = vec4(vec3(v * g), 1.0);
}
