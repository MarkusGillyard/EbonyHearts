shader_type spatial;
render_mode sss_mode_skin;
// Sculpted character surfaces. Material parameters come per vertex from the sculptor (see
// tools/assets/sculpt/materials.py), so one draw covers skin, cloth, leather and metal with soft
// transitions at seams:
//   COLOR    sRGB albedo and baked occlusion
//   CUSTOM0  roughness, subsurface, sheen, metallic
//   CUSTOM1  bind-pose position (xyz) and detail kind (w)
// Fine detail (pores, weave) is a bump from noise in bind space, so it stays on the surface while
// the body moves.
//
// Presentation effects, all driven from FighterView: `tint` washes the whole body toward a colour
// (the Reflection's slop-dark, a finisher's ash or stone), `flash` brightens it for a frame on a
// heavy hit, `dissolve` eats it away from noise with a glowing edge, and `cloth_hue` turns the
// colours of everything but skin for the alternate costume.

uniform float tint : hint_range(0.0, 1.0) = 0.0;
uniform vec3 tint_color : source_color = vec3(0.2, 0.25, 0.15);
uniform float tint_gloss = 0.0;
uniform float flash : hint_range(0.0, 1.0) = 0.0;
uniform float dissolve : hint_range(0.0, 1.0) = 0.0;
uniform vec3 dissolve_color : source_color = vec3(1.0, 0.45, 0.1);
uniform float cloth_hue = 0.0;
uniform float cloth_sat = 1.0;
uniform float detail_strength = 1.0;
// Brightness of surfaces lit from within (detail kind 7, "glow" in the sculpt materials).
uniform float glow_energy = 1.6;
// The fighter's own colour, caught along the edges of the body like a back light, so each
// silhouette reads against every stage.
uniform vec3 accent_color : source_color = vec3(1.0, 0.6, 0.3);
uniform float accent_strength = 0.3;
// Dirt and wear: it gathers low down (boots, hems, knees) and in the creases.
uniform float grime = 0.4;

varying vec4 v_params;
varying vec4 v_bind;
varying vec4 v_albedo;

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

vec3 hue_turn(vec3 c, float turns, float sat) {
	// Rotation about the grey axis (YIQ), which keeps brightness.
	float y = dot(c, vec3(0.299, 0.587, 0.114));
	float i = dot(c, vec3(0.596, -0.274, -0.322));
	float q = dot(c, vec3(0.211, -0.523, 0.312));
	float a = turns * 6.2831853;
	float ci = (i * cos(a) - q * sin(a)) * sat;
	float cq = (i * sin(a) + q * cos(a)) * sat;
	return max(vec3(y + 0.956 * ci + 0.621 * cq, y - 0.272 * ci - 0.647 * cq, y - 1.106 * ci + 1.703 * cq), vec3(0.0));
}

vec3 to_linear(vec3 c) {
	return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}

// Height of the fine surface detail for each kind: 1 skin, 2 cloth, 3 silk, 4 knit, 5 leather, 6 fur.
float detail_height(vec3 p, float kind) {
	if (kind < 0.5) {
		return 0.0;
	} else if (kind < 1.5) {
		return noise(p * 900.0) * 0.6 + noise(p * 260.0) * 0.4;
	} else if (kind < 2.5) {
		float wx = sin(p.x * 2600.0 + noise(p * 120.0) * 3.0);
		float wy = sin(p.y * 2600.0 + noise(p * 120.0) * 3.0);
		return 0.5 + 0.25 * (wx * wy) + noise(p * 300.0) * 0.3;
	} else if (kind < 3.5) {
		return noise(p * 80.0) * 0.5;
	} else if (kind < 4.5) {
		return abs(sin(p.y * 1400.0 + noise(p * 200.0) * 2.0)) * 0.7 + noise(p * 500.0) * 0.3;
	} else if (kind < 5.5) {
		return noise(p * 700.0) * 0.5 + noise(p * 90.0) * 0.5;
	} else if (kind < 6.5) {
		return noise(p * vec3(500.0, 160.0, 500.0));
	}
	return 0.0;
}

void vertex() {
	v_params = CUSTOM0;
	v_bind = CUSTOM1;
	v_albedo = COLOR;
}

void fragment() {
	float kind = v_bind.w;
	float edge_glow = 0.0;
	if (dissolve > 0.0) {
		// Eaten away from the head down, broken up by noise, with a glowing rim.
		float n = noise(v_bind.xyz * 18.0) * 0.6 + noise(v_bind.xyz * 55.0) * 0.4;
		float height = clamp(v_bind.y / 2.0, 0.0, 1.0);
		float cut = n * 0.55 + height * 0.45;
		float t = 1.0 - dissolve * 1.15;
		if (cut > t + 0.04) {
			discard;
		}
		edge_glow = smoothstep(t - 0.06, t + 0.04, cut);
	}
	vec3 srgb = v_albedo.rgb;
	if ((kind < 0.5 || kind > 1.5) && (abs(cloth_hue) > 0.001 || abs(cloth_sat - 1.0) > 0.001)) {
		srgb = hue_turn(srgb, cloth_hue, cloth_sat);
	}
	vec3 base = to_linear(srgb);
	float h = detail_height(v_bind.xyz, kind);
	// Bump mapping from the screen-space derivatives of the detail height (Mikkelsen's surface
	// gradient), which needs no tangents.
	// Detail smaller than a pixel only flickers, so it fades out with distance.
	float footprint = max(length(dFdx(v_bind.xyz)), length(dFdy(v_bind.xyz)));
	float fade = 1.0 - smoothstep(0.0012, 0.004, footprint);
	float amp = fade * (kind < 1.5 ? 0.00007 : (kind < 2.5 ? 0.00025 : (kind < 3.5 ? 0.0001 : (kind < 4.5 ? 0.0004 : (kind < 5.5 ? 0.0001 : 0.00015))))) * detail_strength;
	vec3 sx = dFdx(VERTEX);
	vec3 sy = dFdy(VERTEX);
	vec3 r1 = cross(sy, NORMAL);
	vec3 r2 = cross(NORMAL, sx);
	float det = dot(sx, r1);
	vec3 surf_grad = sign(det) * (dFdx(h * amp) * r1 + dFdy(h * amp) * r2);
	NORMAL = normalize(abs(det) * NORMAL - surf_grad);
	// Broader detail that survives at fighting distance: creases and folds in cloth, scuffs on
	// leather, a faint mottling of the skin. A gentle bump, far larger than the pores and weave.
	vec3 bp = v_bind.xyz;
	float fold = 0.0;
	float fold_amp = 0.0;
	if (kind > 1.5 && kind < 4.5) {
		// Folds run mostly across the limb: stretched noise, two scales.
		fold = noise(bp * vec3(16.0, 40.0, 16.0)) * 0.6 + noise(bp * vec3(34.0, 90.0, 34.0)) * 0.4;
		fold_amp = kind < 2.5 ? 0.0035 : (kind < 3.5 ? 0.0025 : 0.003);
	} else if (kind > 4.5 && kind < 5.5) {
		fold = noise(bp * 28.0) * 0.5 + noise(bp * 70.0) * 0.5;
		fold_amp = 0.0018;
	} else if (kind > 0.5 && kind < 1.5) {
		fold = noise(bp * 24.0);
		fold_amp = 0.0006;
	}
	float far_fade = 1.0 - smoothstep(0.01, 0.03, footprint);
	fold_amp *= far_fade * detail_strength;
	if (fold_amp > 0.0) {
		vec3 fold_grad = sign(det) * (dFdx(fold * fold_amp) * r1 + dFdy(fold * fold_amp) * r2);
		NORMAL = normalize(abs(det) * NORMAL - fold_grad);
	}
	float var_ = noise(bp * 40.0) * 0.08 - 0.04;
	vec3 col = base * (1.0 + var_);
	float ao = v_albedo.a;
	if (kind > 0.5 && kind < 1.5) {
		// Skin: mottled, warmer and redder where it is thin and in the creases, never flat.
		col *= 1.0 + (noise(bp * 11.0) - 0.5) * 0.09 + (noise(bp * 3.5) - 0.5) * 0.06;
		col *= mix(vec3(1.0, 0.84, 0.8), vec3(1.0), ao);
	} else if (kind > 4.5 && kind < 5.5) {
		// Leather: pale scuffs where it has rubbed.
		col = mix(col, col * 1.4 + vec3(0.02), smoothstep(0.58, 0.8, noise(bp * 19.0)) * 0.45);
	} else if (kind > 1.5 && kind < 4.5) {
		// Cloth: the folds catch dirt and shadow.
		col *= 1.0 - (0.5 - fold) * 0.18;
	}
	// Grime low down and in the creases; the colour muddies and the surface dulls.
	float lowness = 1.0 - smoothstep(0.02, 0.38, bp.y);
	float dirt = grime * (lowness * (0.3 + 0.7 * noise(bp * 9.0)) * (kind < 6.5 ? 1.0 : 0.3) + (1.0 - ao) * 0.4);
	dirt = clamp(dirt, 0.0, 1.0);
	col = mix(col, col * vec3(0.55, 0.5, 0.44), dirt * 0.5);
	float grain = noise(v_bind.xyz * 70.0);
	vec3 tinted = tint_color * (0.75 + grain * 0.5);
	ALBEDO = mix(col, tinted, tint);
	float rough = v_params.x + (h - 0.5) * 0.08 + (noise(bp * 27.0) - 0.5) * 0.14 + dirt * 0.3;
	if (kind > 0.5 && kind < 1.5) {
		// Skin is never uniformly glossy: duller in the hollows, a little sheen on the high points.
		rough += (0.5 - ao) * 0.25;
	}
	ROUGHNESS = mix(clamp(rough, 0.05, 1.0), mix(0.95, 0.2, tint_gloss), tint);
	METALLIC = v_params.w * (1.0 - tint);
	SPECULAR = v_params.y > 0.5 ? 0.35 : 0.5;
	AO = v_albedo.a;
	AO_LIGHT_AFFECT = 0.45;
	SSS_STRENGTH = v_params.y * 0.32 * (1.0 - tint);
	// Sheen: Godot's rim widens with roughness, so keep it faint on rough cloth.
	RIM = v_params.z * 0.12;
	RIM_TINT = 0.8;
	EMISSION = vec3(flash * 0.9) + dissolve_color * edge_glow * 4.0;
	if (kind > 6.5) {
		EMISSION += col * glow_energy * (1.0 - tint);
	}
	// The accent: strongest where the surface turns away from the camera, like a rim light in
	// the fighter's colour; metal and glowing parts keep their own light.
	float fres = pow(1.0 - clamp(dot(normalize(NORMAL), normalize(VIEW)), 0.0, 1.0), 3.5);
	EMISSION += accent_color * fres * accent_strength * (1.0 - tint) * (0.4 + 0.6 * ao) * (1.0 - v_params.w) * (kind > 6.5 ? 0.0 : 1.0);
}
