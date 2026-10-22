"""Surface parameters per material key, from the fighter's look.materials with realistic
defaults for each kind of surface. Values are written per vertex, so one shader draws skin,
cloth, leather and metal with soft transitions at seams."""
import numpy as np

# kind -> roughness, subsurface (skin), sheen (cloth), metallic, detail kind for the shader
KINDS = {
    "skin": (0.52, 1.0, 0.0, 0.0, 1),
    "lips": (0.4, 0.9, 0.0, 0.0, 1),
    "fabric": (0.82, 0.0, 0.55, 0.0, 2),
    "silk": (0.5, 0.0, 0.7, 0.0, 3),
    "knit": (0.92, 0.0, 0.45, 0.0, 4),
    "leather": (0.38, 0.0, 0.05, 0.0, 5),
    "fur": (0.9, 0.0, 0.8, 0.0, 6),
    "metal": (0.3, 0.0, 0.0, 1.0, 0),
    "hair": (0.45, 0.0, 0.0, 0.0, 0),
    # Lit from within (the Slopbat's veins): smooth, and glowing in its own colour in the shader.
    "glow": (0.35, 0.0, 0.0, 0.0, 7),
}

DEFAULT_KIND = {"skin": "skin", "lips": "lips", "dress": "silk", "robe": "fabric", "coat": "fur", "fur": "fur",
                "turtleneck": "knit", "slacks": "fabric", "boots": "leather", "gloves": "leather",
                "slippers": "silk", "gold": "metal", "ring": "metal", "staff": "metal", "hair": "hair",
                "sash": "fabric", "trousers": "fabric", "lining": "fabric"}


def srgb(hexstr):
    h = hexstr.lstrip("#")
    return np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], np.float32) / 255.0


class Palette:
    def __init__(self, look):
        self.look = look
        self.mats = dict(look.get("materials", {}))
        if "lips" not in self.mats and "skin" in self.mats:
            sk = srgb(self.mats["skin"]["color"])
            lip = np.clip(sk * np.array([0.92, 0.66, 0.66]) + np.array([0.03, 0.0, 0.0]), 0, 1)
            self.mats["lips"] = {"color": "#%02x%02x%02x" % tuple(int(c * 255) for c in lip), "kind": "lips"}

    def params(self, key):
        spec = self.mats.get(key, {})
        kind = spec.get("kind", DEFAULT_KIND.get(key, "fabric"))
        rough, sss, sheen, metal, detail = KINDS[kind]
        if "roughness" in spec and kind not in ("skin", "lips"):
            rough = float(spec["roughness"])
        if "metallic" in spec:
            metal = float(spec["metallic"])
        color = srgb(spec.get("color", "#808080"))
        return color, np.array([rough, sss, sheen, metal], np.float32), detail

    def kind(self, key):
        return self.mats.get(key, {}).get("kind", DEFAULT_KIND.get(key, "fabric"))

    def assign(self, keys, ao):
        """Per-vertex albedo (RGBA 0..1, alpha = occlusion), params (RGBA 0..1) and detail kinds.
        Values are quantised to 8 bits, the precision they were authored at."""
        n = len(keys)
        albedo = np.zeros((n, 4), np.float32)
        params = np.zeros((n, 4), np.float32)
        detail = np.zeros(n, np.float32)
        for key in set(keys):
            idx = np.array([i for i, k in enumerate(keys) if k == key])
            c, p, d = self.params(key)
            albedo[idx, :3] = c
            params[idx] = p
            detail[idx] = d
        albedo[:, 3] = ao
        return _quantise(albedo), _quantise(params), detail


def _quantise(a):
    return (np.round(np.clip(a, 0, 1) * 255) / 255.0).astype(np.float32)
