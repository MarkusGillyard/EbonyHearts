shader_type spatial;
// An eyeball looking down its local -z: sclera, iris with radial fibres, pupil, a dark limbal
// ring and a wet cornea. UV.x is the angle from the front of the eye (0 at the pupil, 1 behind).
// `glow` lights the iris for Qhi techniques; `alt_mix` blends toward a second iris colour.

uniform vec3 iris_color : source_color = vec3(0.3, 0.5, 0.8);
uniform vec3 alt_color : source_color = vec3(0.6, 0.3, 1.0);
uniform float alt_mix = 0.0;
uniform float glow = 0.0;
uniform float iris_size = 0.2;
uniform float pupil_size = 0.07;
uniform vec3 sclera_color : source_color = vec3(0.72, 0.7, 0.68);
uniform float tint : hint_range(0.0, 1.0) = 0.0;
uniform vec3 tint_color : source_color = vec3(0.2, 0.25, 0.15);

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void fragment() {
	float t = UV.x;
	float ang = UV.y * 6.2831853;
	vec3 iris = mix(iris_color, alt_color, alt_mix);
	float fib = 0.75 + 0.25 * sin(ang * 70.0 + hash(vec2(floor(ang * 11.0), 1.0)) * 6.0) * smoothstep(pupil_size, iris_size, t);
	float r = t / iris_size;
	vec3 iris_col = iris * fib * mix(1.25, 0.7, r);
	float in_iris = 1.0 - smoothstep(iris_size - 0.012, iris_size + 0.004, t);
	float limbal = smoothstep(iris_size - 0.04, iris_size, t) * in_iris;
	iris_col = mix(iris_col, iris * 0.25, limbal * 0.8);
	float pupil = 1.0 - smoothstep(pupil_size - 0.006, pupil_size + 0.004, t);
	vec3 sclera = sclera_color * mix(1.0, 0.82, smoothstep(0.25, 0.55, t));
	// Faint vessels toward the corners.
	sclera = mix(sclera, vec3(0.75, 0.45, 0.42), smoothstep(0.3, 0.6, t) * 0.25 * hash(vec2(floor(ang * 40.0), floor(t * 30.0))));
	vec3 col = mix(sclera, iris_col, in_iris);
	col = mix(col, vec3(0.01), pupil);
	ALBEDO = mix(col, tint_color, tint * 0.8);
	ROUGHNESS = 0.03;
	SPECULAR = 0.7;
	CLEARCOAT = 1.0;
	CLEARCOAT_ROUGHNESS = 0.02;
	EMISSION = iris * in_iris * (1.0 - pupil) * glow;
}
