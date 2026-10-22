shader_type spatial;
// Swaying things for MultiMesh, each on its own clock taken from its position: lanterns hang from
// the instance origin and swing below it; reeds stand on it and bend more the higher they reach.
// Paper lanterns glow through their ribbed paper (emission with `ribs` per metre). Vertex and
// instance colours tint both albedo and emission (reed heads, lantern papers).
render_mode cull_disabled;

uniform vec3 albedo : source_color = vec3(0.1, 0.1, 0.08);
uniform vec3 emission : source_color = vec3(0.0);
uniform float emission_energy = 0.0;
uniform float ribs = 0.0;
uniform float sway = 0.06;
uniform float speed = 1.0;
uniform bool hanging = true;
uniform float roughness = 0.8;

varying vec3 lpos;

mat2 rot(float a) {
	return mat2(vec2(cos(a), sin(a)), vec2(-sin(a), cos(a)));
}

void vertex() {
	vec3 origin = MODEL_MATRIX[3].xyz;
	float seed = fract(sin(dot(origin, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
	float t = TIME * speed * (0.8 + seed * 0.4) + seed * 20.0;
	lpos = VERTEX;
	if (hanging) {
		VERTEX.xy = rot(sin(t) * sway) * VERTEX.xy;
		VERTEX.zy = rot(sin(t * 0.73 + 1.3) * sway * 0.6) * VERTEX.zy;
	} else {
		float h = max(VERTEX.y, 0.0);
		VERTEX.x += (sin(t + origin.x * 0.4) * 0.7 + sin(t * 2.3) * 0.3) * sway * h * h;
		VERTEX.z += cos(t * 0.8) * sway * 0.4 * h * h;
	}
}

void fragment() {
	ALBEDO = albedo * COLOR.rgb;
	ROUGHNESS = roughness;
	if (emission_energy > 0.0) {
		float rib = ribs > 0.0 ? 1.0 - 0.45 * smoothstep(0.7, 0.95, abs(fract(lpos.y * ribs) - 0.5) * 2.0) : 1.0;
		EMISSION = emission * emission_energy * rib * COLOR.rgb;
	}
}
