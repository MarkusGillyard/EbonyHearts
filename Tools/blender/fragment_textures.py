"""The two tiling textures of the Fragment's skin: stars and a nebula (SourceArt/Fragment).

    Tools/blender/run_blender.sh Tools/blender/fragment_textures.py
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fighter_tex as tex  # noqa: E402

HERE = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(HERE, "SourceArt", "Fragment")
N = 1024
RNG = np.random.default_rng(61)


def fbm(octaves=6, base=4, falloff=0.55):
    """Tiling fractal noise: band-limited random phases, in [0, 1]."""
    out = np.zeros((N, N))
    amp = 1.0
    for o in range(octaves):
        f = base * 2 ** o
        grid = RNG.random((f, f))
        spec = np.fft.fft2(grid)
        pad = np.zeros((N, N), complex)
        k = f // 2
        pad[:k, :k], pad[:k, -k:], pad[-k:, :k], pad[-k:, -k:] = spec[:k, :k], spec[:k, -k:], spec[-k:, :k], spec[-k:, -k:]
        out += amp * np.real(np.fft.ifft2(pad)) * (N / f) ** 2
        amp *= falloff
    out -= out.min()
    return out / out.max()


def main():
    os.makedirs(OUT, exist_ok=True)
    a, b = fbm(), fbm(base=6)
    cloud = np.clip((a * 0.7 + b * 0.3 - 0.3) * 2.6, 0, 1) ** 1.3
    wisps = np.clip(1 - np.abs(b - 0.5) * 7, 0, 1) ** 2 * cloud
    tex.save_png(os.path.join(OUT, "fragment_nebula.png"), np.stack([cloud, wisps, a, np.ones_like(a)], 2))
    stars = np.zeros((N, N))
    for _ in range(2600):
        x, y = RNG.integers(0, N, 2)
        s = RNG.random() ** 6
        r = 1 + int(s * 3)
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                d = (dx * dx + dy * dy) / (r * r + 0.5)
                if d <= 1:
                    stars[(y + dy) % N, (x + dx) % N] = max(stars[(y + dy) % N, (x + dx) % N], (0.25 + 0.75 * s) * (1 - d))
    stars = np.clip(stars, 0, 1)
    tex.save_png(os.path.join(OUT, "fragment_stars.png"), np.stack([stars, stars, stars, np.ones_like(stars)], 2))


main()
