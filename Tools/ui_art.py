"""Builds the interface textures in SourceArt/UI from the owner's title card and the design tokens.

    python3 -I Tools/ui_art.py            (needs numpy, scipy and Pillow)

Everything but EbonyHearts_TitleCard.png is derived here and can be rebuilt at any time. The title
card itself is never written to. The result is imported by the `ui` step of Tools/import_assets.py.
Sizes are twice the 1920x1080 design units, so frames stay crisp at 3440x1440.
"""
import os

import numpy as np
from PIL import Image
from scipy.ndimage import distance_transform_edt, gaussian_filter

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "SourceArt", "UI")
RNG = np.random.default_rng(1986)

# Sampled from the title card (k-means over its opaque pixels), as linear-free sRGB bytes.
INK = (10, 7, 16)
OBSIDIAN = (17, 8, 26)
AUBERGINE = (40, 24, 52)
ROYAL = (57, 24, 94)
PEWTER = (99, 87, 104)
SILVER = (174, 153, 186)
HIGHLIGHT = (219, 202, 226)
ELECTRIC = (210, 125, 242)


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def fbm(h, w, base, octaves, gain=0.5):
    """Tileless fractal noise in 0..1; `base` is the largest feature size in pixels."""
    out = np.zeros((h, w))
    amp = 1.0
    size = base
    for _ in range(octaves):
        n = gaussian_filter(RNG.standard_normal((h, w)), size, mode="reflect")
        out += amp * n / max(n.std(), 1e-6)
        amp *= gain
        size = max(size * 0.5, 0.6)
    out -= out.min()
    return out / out.max()


def ramp(t, stops):
    """Colour ramp: stops are (position, (r, g, b))."""
    xs = [s[0] for s in stops]
    return np.stack([np.interp(t, xs, [s[1][c] for s in stops]) for c in range(3)], axis=-1)


def dither(rgb):
    """Dark gradients band in 8 bits; a triangular dither of one level hides it."""
    n = RNG.random(rgb.shape) - RNG.random(rgb.shape)
    return np.clip(rgb + n, 0, 255)


def save(name, rgb, alpha=None):
    arr = dither(rgb.astype(np.float64))
    if alpha is not None:
        a = np.clip(dither(alpha[..., None] * 255.0), 0, 255)
        arr = np.concatenate([arr, a], axis=-1)
    Image.fromarray(arr.round().astype(np.uint8), "RGBA" if alpha is not None else "RGB").save(os.path.join(ROOT, name), optimize=True)
    print(name, arr.shape[1], "x", arr.shape[0])


def tinted(name, alpha):
    """An alpha shape on white, tinted by the widget that draws it."""
    save(name, np.full(alpha.shape + (3,), 255.0), alpha)


def hammered(h, w):
    """Beaten dark silver: the title card's metal language, drawn fresh from its palette."""
    dents = fbm(h, w, 7, 4, 0.55)
    gy, gx = np.gradient(gaussian_filter(dents, 1.0))
    light = np.clip(0.5 + (-gx - gy) * 14.0, 0, 1)
    tone = 0.25 * fbm(h, w, 24, 3) + 0.75 * light
    return ramp(tone, [(0.0, (74, 62, 80)), (0.35, PEWTER), (0.6, (133, 122, 138)), (0.82, SILVER), (1.0, HIGHLIGHT)])


def logo_energy(card):
    """The violet veins of the E and S, blurred into a halo that breathes behind the card."""
    px = card.astype(np.float64) / 255.0
    r, g, b, a = px[..., 0], px[..., 1], px[..., 2], px[..., 3]
    sat = np.clip(b - g, 0, 1)
    mask = smooth(0.18, 0.5, sat) * smooth(0.25, 0.7, b) * a
    glow = gaussian_filter(mask, 7) * 2.2 + gaussian_filter(mask, 22) * 2.6
    glow = np.clip(glow, 0, 1)
    h, w = glow.shape
    small = np.asarray(Image.fromarray((glow * 65535).astype(np.uint16)).resize((w // 2, h // 2), Image.BILINEAR)).astype(np.float64) / 65535.0
    save("logo_energy.png", np.full(small.shape + (3,), 0.0) + np.array(ELECTRIC), small * 0.85)


def hall():
    """The far sky of the menus' hall: the card's violet veins opened out into a night."""
    h, w = 1024, 2048
    n = fbm(h, w, 190, 7, 0.55)
    wisp = fbm(h, w, 60, 6, 0.62)
    cloud = smooth(0.4, 0.92, n)
    veins = 1 - np.abs(fbm(h, w, 70, 5, 0.6) - 0.5) * 2
    veins = smooth(0.965, 1.0, veins) * smooth(0.45, 0.8, n)
    y, x = np.mgrid[0:h, 0:w]
    u = (x - w * 0.5) / (w * 0.5)
    v = (y - h * 0.62) / (h * 0.62)
    fall = 1.0 - smooth(0.2, 1.2, np.sqrt(u * u * 0.8 + v * v))
    rgb = ramp(gaussian_filter(cloud * (0.6 + 0.4 * wisp), 2.5) * fall, [(0.0, INK), (0.4, (24, 12, 42)), (0.75, (52, 25, 92)), (1.0, (96, 52, 150))])
    rgb = rgb + (gaussian_filter(veins, 2.2) * 2.0)[..., None] * np.array(ELECTRIC) * 0.16 * fall[..., None]
    stars = gaussian_filter((RNG.random((h, w)) > 0.9992).astype(np.float64), 0.9) * 5.0
    rgb = rgb + stars[..., None] * np.array(HIGHLIGHT) * 0.7
    save("hall.png", np.clip(rgb, 0, 255))


def fade_panel():
    """Side panels: opaque ink on the left falling away, so the world stays in sight on the right."""
    h, w = 64, 1024
    t = np.linspace(0, 1, w)
    a = np.tile((1 - smooth(0.42, 1.0, t)) * 0.94, (h, 1))
    save("fade_side.png", np.full((h, w, 3), 0.0) + np.array(INK), a)
    h, w = 512, 64
    t = np.linspace(0, 1, h)[:, None]
    save("fade_foot.png", np.full((h, w, 3), 0.0) + np.array(INK), np.tile(smooth(0.0, 1.0, t) ** 1.4 * 0.95, (1, w)))


def rule():
    """A long hairline, tapered at both ends and fractured once, as if scored into dark metal."""
    h, w = 16, 2048
    x = np.linspace(-1, 1, w)
    env = (1 - smooth(0.55, 1.0, np.abs(x)))
    line = np.zeros((h, w))
    cy = h / 2 + 2.2 * gaussian_filter(RNG.standard_normal(w), 90) / 0.02 * 0.012
    yy = np.arange(h)[:, None]
    dist = np.abs(yy - cy[None, :])
    line = np.clip(1.5 - dist, 0, 1)
    chips = gaussian_filter((RNG.random(w) > 0.012).astype(np.float64), 1.0)
    a = line * env * (0.55 + 0.45 * chips)
    tinted("rule.png", a)


def spike_shape(n, power):
    """A four-pointed lozenge, concave-sided like the serifs of the card's great E."""
    y, x = np.mgrid[-1:1:complex(n), -1:1:complex(n)]
    r = np.abs(x) ** power + np.abs(y) ** power
    return smooth(1.0 + 0.09, 1.0 - 0.09, r)


def spikes():
    solid = spike_shape(128, 0.62)
    tinted("spike.png", solid)
    inner = spike_shape(128, 0.62)
    y, x = np.mgrid[-1:1:128j, -1:1:128j]
    ring = solid * (1 - smooth(1.0 + 0.08, 1.0 - 0.08, (np.abs(x) ** 0.62 + np.abs(y) ** 0.62) / 0.42))
    tinted("spike_hollow.png", ring)


def corner_frame():
    """The corner language: a hairline box whose corners are cut and spurred. Drawn nine-sliced."""
    n = 96
    a = np.zeros((n, n))
    t = 2.0
    y, x = np.mgrid[0:n, 0:n].astype(np.float64)
    edge = np.minimum.reduce([x, y, n - 1 - x, n - 1 - y])
    # Cut corners: distance to the 45 degree chamfer.
    cx = np.minimum(x, n - 1 - x)
    cy = np.minimum(y, n - 1 - y)
    chamfer = (cx + cy - 14) / np.sqrt(2)
    outline = np.where(chamfer < 0, np.abs(chamfer), np.minimum(edge, 1e9))
    a = smooth(t + 0.8, t - 0.8, outline)
    a[chamfer < -t - 1] = 0
    tinted("frame.png", a)


def bar_frame():
    """The health bar's mount: two rails of beaten metal running out to a spur like the E's serif."""
    h, w = 148, 1760
    y, x = np.mgrid[0:h, 0:w].astype(np.float64)
    # The bar's slot in the frame, in pixels at 2x: 68 to 1720 across, 42 to 94 down.
    x0, x1, y0, y1 = 68, 1724, 44, 92
    rail = 7.0
    inside = (x > x0) & (x < x1) & (y > y0) & (y < y1)
    outer = (x > x0 - rail) & (x < x1 + rail) & (y > y0 - rail) & (y < y1 + rail)
    shape = (outer & ~inside).astype(np.float64)
    # The spur: a curved blade from the outer end of the frame.
    t = np.clip((x0 + 4 - x) / (x0 + 4), 0, 1)
    mid = (y0 + y1) / 2 - 8 * t * t
    half = (y1 - y0) / 2 * (1 - t) ** 1.7 + 0.8
    blade = (x <= x0 + 4) & (np.abs(y - mid) < half)
    shape = np.maximum(shape, blade.astype(np.float64))
    # A thorn off the top rail at the inner third, and a stop at the inner end.
    thorn_x = 1180
    tt = np.clip(np.abs(x - thorn_x) / 60, 0, 1)
    thorn = (y < y0) & (y > y0 - 18 * (1 - tt) ** 2 - rail)
    shape = np.maximum(shape, thorn.astype(np.float64))
    soft = gaussian_filter(shape, 0.7)
    metal = hammered(h, w)
    dist = distance_transform_edt(shape > 0.5)
    bevel = np.clip(dist / 3.0, 0, 1)
    metal = metal * (0.55 + 0.45 * bevel[..., None])
    save("bar_frame.png", metal, smooth(0.2, 0.8, soft))


def timer_plate():
    """The plaque under the clock: an obsidian shield with a spur at its foot."""
    h, w = 232, 300
    y, x = np.mgrid[0:h, 0:w].astype(np.float64)
    u = np.abs(x - w / 2)
    top = 4.0
    width = 142 - 34 * smooth(70, 170, y)
    foot = 228 - 0.62 * u
    shape = (y > top) & (u < width) & (y < foot)
    d = distance_transform_edt(shape)
    metal = hammered(h, w)
    fill = ramp(smooth(0, 200, y), [(0.0, (20, 12, 30)), (1.0, INK)])
    rim = smooth(4.5, 2.5, d)
    rgb = fill * (1 - rim[..., None]) + metal * rim[..., None]
    save("timer_plate.png", rgb, smooth(0.0, 1.5, d) * 0.96)


def bar_fill():
    """A bar's body: lit along its top, darker beneath, with a grain that reads as worn plate."""
    h, w = 48, 1024
    grain = gaussian_filter(RNG.standard_normal((h, w)), (0.6, 14))
    grain = 1 + grain / grain.std() * 0.05
    v = np.linspace(0, 1, h)[:, None]
    shade = 0.74 + 0.26 * (1 - smooth(0.0, 0.62, v)) - 0.22 * smooth(0.6, 1.0, v)
    c = np.clip(shade * grain, 0, 1)
    save("bar_fill.png", np.repeat((c * 255.0)[..., None], 3, axis=-1))


def band():
    """The band behind round calls: dark through the middle, gone at the screen's edges."""
    h, w = 256, 1024
    x = np.linspace(-1, 1, w)
    y = np.linspace(-1, 1, h)[:, None]
    a = (1 - smooth(0.1, 1.0, np.abs(x))) * (1 - smooth(0.2, 1.0, np.abs(y)))
    save("band.png", np.full((h, w, 3), 0.0) + np.array(INK), a * 0.7)


def vignette():
    h, w = 288, 512
    y, x = np.mgrid[-1:1:complex(h), -1:1:complex(w)]
    a = smooth(0.45, 1.5, np.sqrt(x * x + y * y)) * 0.9
    save("vignette.png", np.full((h, w, 3), 0.0) + np.array(INK), a)


def glow():
    n = 256
    y, x = np.mgrid[-1:1:complex(n), -1:1:complex(n)]
    tinted("glow.png", np.clip(1 - np.sqrt(x * x + y * y), 0, 1) ** 2.2)
    h, w = 64, 512
    t = np.linspace(0, 1, w)
    tinted("sweep.png", np.tile((1 - t) ** 1.6, (h, 1)))


def main():
    os.makedirs(ROOT, exist_ok=True)
    card = np.asarray(Image.open(os.path.join(ROOT, "EbonyHearts_TitleCard.png")).convert("RGBA"))
    assert card.shape[:2] == (724, 2172), "the title card is 2172 x 724"
    logo_energy(card)
    hall()
    fade_panel()
    rule()
    spikes()
    corner_frame()
    bar_frame()
    timer_plate()
    bar_fill()
    band()
    vignette()
    glow()


if __name__ == "__main__":
    main()
