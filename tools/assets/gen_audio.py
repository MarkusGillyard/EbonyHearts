#!/usr/bin/env python3
"""Synthesises every sound effect and music loop of Mortal Slopbat into assets/audio/.

Usage: .venv/bin/python tools/assets/gen_audio.py [--only name,name] [--check] [--jobs N]

Nothing is sampled or downloaded. Every sound is built here from oscillators, filtered noise,
modal resonators (struck wood, metal, skin), additive voices with formant envelopes (choir,
bowed strings, brass, reeds, plucked strings), and convolution reverb with synthetic impulse
responses. numpy/scipy are dev-time tools only; the game just loads the WAV files.

Output is deterministic: each sound seeds its own RNG from a CRC of its name.

- SFX: 16-bit mono 44.1 kHz, DC-blocked, short fades, peak-normalised to -1 dBFS.
- Music: 16-bit mono 32 kHz loops. Each loop is rendered on a circular timeline: note tails,
  reverb and filters wrap from the end back to the start, so the whole file loops with no
  seam (the end flows into the start exactly as any other bar line). Levels: about -20 dBFS
  RMS, peaks held under -3 dBFS by a look-ahead limiter. Each loop file also carries a RIFF
  'smpl' chunk marking a forward loop over the whole file (Godot's "Detect From WAV" reads it).

--check prints duration, peak, RMS and DC offset of every file (plus the wrap-point step for
music loops) and the folder total. MIXDEBUG=1 prints per-bus levels while rendering music.
"""
import argparse
import os
import struct
import sys
import wave
import zlib
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np
from scipy import signal
from scipy.ndimage import minimum_filter1d, uniform_filter1d

OUT = Path(__file__).resolve().parents[2] / "assets" / "audio"
SR = 44100          # sound effects
MUSIC_SR = 32000    # music loops
TAU = 2.0 * np.pi
CTRL = 64           # control-rate hop (samples) for additive envelopes


# --- Basic signal helpers ------------------------------------------------------------------------

def n_of(dur, sr):
    return max(1, int(round(dur * sr)))


def tvec(n, sr):
    return np.arange(n) / sr


def db(v):
    return 20.0 * np.log10(max(float(v), 1e-12))


def rms(x):
    return float(np.sqrt(np.mean(np.square(x))))


def norm_peak(x, peak=1.0):
    m = np.max(np.abs(x)) if len(x) else 0.0
    return x * (peak / m) if m > 0 else x


def phase_of(freq, n, sr):
    """Running phase in cycles (starting at 0) of a constant or per-sample frequency."""
    f = np.broadcast_to(np.asarray(freq, dtype=float), (n,))
    ph = np.cumsum(f) / sr
    return ph - f[0] / sr


def sine(freq, n, sr, ph0=0.0):
    return np.sin(TAU * phase_of(freq, n, sr) + ph0)


def polyblep_saw(freq, n, sr, ph0=0.0):
    """Band-limited sawtooth (PolyBLEP) for a constant or per-sample frequency."""
    f = np.broadcast_to(np.asarray(freq, dtype=float), (n,))
    dt = f / sr
    p = (phase_of(f, n, sr) + ph0) % 1.0
    y = 2.0 * p - 1.0
    m = p < dt
    x = p[m] / dt[m]
    y[m] -= 2.0 * x - x * x - 1.0
    m = p > 1.0 - dt
    x = (p[m] - 1.0) / dt[m]
    y[m] -= x * x + 2.0 * x + 1.0
    return y


def glide(n, sr, f0, f1, tau):
    """Frequency curve moving exponentially from f0 toward f1 (time constant tau)."""
    return f1 + (f0 - f1) * np.exp(-tvec(n, sr) / tau)


def env_exp(n, sr, attack, decay, hold=0.0):
    """Smooth attack, optional hold, then exponential decay with time constant `decay`."""
    t = tvec(n, sr)
    a = np.ones(n) if attack <= 0 else np.sin(0.5 * np.pi * np.clip(t / attack, 0, 1)) ** 2
    return a * np.exp(-np.maximum(t - attack - hold, 0.0) / decay)


def env_pts(n, sr, pts):
    """Piecewise-linear envelope through (time, value) points."""
    ts, vs = zip(*pts)
    return np.interp(tvec(n, sr), ts, vs)


def env_note(n, sr, dur, att, rel):
    """Sustained-note envelope: smooth attack, hold until `dur`, smooth release."""
    t = tvec(n, sr)
    a = np.sin(0.5 * np.pi * np.clip(t / max(att, 1e-4), 0, 1)) ** 2
    r = np.clip((t - dur) / max(rel, 1e-4), 0, 1)
    return a * np.cos(0.5 * np.pi * r) ** 2


def fade(x, sr, fin=0.0, fout=0.0):
    x = x.copy()
    if fin > 0:
        k = min(len(x), n_of(fin, sr))
        x[:k] *= np.sin(0.5 * np.pi * np.linspace(0, 1, k)) ** 2
    if fout > 0:
        k = min(len(x), n_of(fout, sr))
        x[-k:] *= np.cos(0.5 * np.pi * np.linspace(0, 1, k)) ** 2
    return x


def tail_fade(x, sr, frac=0.25, cap=0.15):
    """Fade the last part of a layer to exactly zero so truncated decays never click."""
    return fade(x, sr, 0.0, min(cap, frac * len(x) / sr))


def place(dst, src, i):
    """Add src into dst starting at sample i (clipped to dst)."""
    if i < 0:
        src, i = src[-i:], 0
    if i < len(dst) and len(src):
        e = min(len(dst), i + len(src))
        dst[i:e] += src[:e - i]
    return dst


def mixdown(dur, sr, *layers):
    """Layers are (signal, start_seconds, gain)."""
    out = np.zeros(n_of(dur, sr))
    for sig, t0, g in layers:
        place(out, g * sig, int(round(t0 * sr)))
    return tail_fade(out, sr, 0.05, 0.02)


def sat(x, drive):
    """tanh saturation normalised so a unit peak stays at unit level."""
    return np.tanh(drive * x) / np.tanh(drive)


# --- Noise ---------------------------------------------------------------------------------------

def white(n, rng):
    return rng.standard_normal(n)


def colored(n, rng, alpha):
    """Circular noise with a 1/f**alpha power spectrum (1 pink, 2 brown), unit RMS."""
    spec = np.fft.rfft(rng.standard_normal(n))
    f = np.arange(len(spec), dtype=float)
    f[0] = 1.0
    spec *= f ** (-alpha / 2.0)
    spec[0] = 0.0
    y = np.fft.irfft(spec, n)
    return y / (np.std(y) + 1e-12)


def slow_noise(n, sr, rng, rate):
    """Smooth random control signal (about unit std) wandering at roughly `rate` Hz."""
    m = int(n / sr * rate) + 3
    x = np.interp(tvec(n, sr) * rate, np.arange(m), rng.standard_normal(m))
    x = uniform_filter1d(x, max(1, int(sr / rate / 2)), mode="nearest")
    return x / (np.std(x) + 1e-9)


# --- Filters -------------------------------------------------------------------------------------

def _butter(kind, fc, sr, order):
    top = 0.47 * sr
    fc = [min(max(f, 5.0), top) for f in fc] if np.ndim(fc) else min(max(fc, 5.0), top)
    return signal.butter(order, fc, kind, fs=sr, output="sos")


def lp(x, fc, sr, order=2):
    return signal.sosfilt(_butter("lowpass", fc, sr, order), x)


def hp(x, fc, sr, order=2):
    return signal.sosfilt(_butter("highpass", fc, sr, order), x)


def bp(x, lo, hi, sr, order=2):
    return signal.sosfilt(_butter("bandpass", [lo, max(hi, lo * 1.05)], sr, order), x)


def reson(x, f, q, sr):
    """Resonant band-pass with unity gain at f."""
    b, a = signal.iirpeak(min(f, 0.45 * sr), q, fs=sr)
    return signal.lfilter(b, a, x)


def comb(x, delay, fb, sr):
    """Feedback comb filter (hollow, metallic colouration)."""
    d = max(1, int(round(delay * sr)))
    a = np.zeros(d + 1)
    a[0], a[d] = 1.0, -fb
    return signal.lfilter([1.0], a, x)


def _rbj(kind, f, q, sr):
    w = TAU * f / sr
    cw, al = np.cos(w), np.sin(w) / (2.0 * q)
    if kind == "lp":
        b = [(1 - cw) / 2, 1 - cw, (1 - cw) / 2]
    elif kind == "hp":
        b = [(1 + cw) / 2, -(1 + cw), (1 + cw) / 2]
    else:
        b = [al, 0.0, -al]
    a0 = 1.0 + al
    return np.array(b) / a0, np.array([1.0, -2.0 * cw / a0, (1.0 - al) / a0])


def tv_filter(x, fc, sr, q=0.707, kind="lp", block=64):
    """Biquad whose cutoff follows the per-sample curve fc (updated every `block` samples)."""
    n = len(x)
    fc = np.broadcast_to(np.asarray(fc, dtype=float), (n,))
    y = np.empty(n)
    zi = np.zeros(2)
    for s in range(0, n, block):
        e = min(n, s + block)
        b, a = _rbj(kind, min(max(fc[(s + e - 1) // 2], 10.0), 0.45 * sr), q, sr)
        y[s:e], zi = signal.lfilter(b, a, x[s:e], zi=zi)
    return y


def spectral_noise(n, sr, gain, rng, nfft=512):
    """White noise shaped by gain(t, f) per STFT frame (t seconds, f Hz); unit RMS."""
    hop = nfft // 4
    x = rng.standard_normal(n + nfft)
    f, t, z = signal.stft(x, fs=sr, nperseg=nfft, noverlap=nfft - hop)
    z = z * gain(t[None, :], f[:, None])
    _, y = signal.istft(z, fs=sr, nperseg=nfft, noverlap=nfft - hop)
    y = y[:n]
    return y / (rms(y) + 1e-12)


def g_band(f, fc, width):
    """Gaussian band in log-frequency: centre fc, width = std dev in octaves."""
    return np.exp(-0.5 * (np.log2(np.maximum(f, 1.0) / fc) / width) ** 2)


def g_lowpass(f, fc, order=2):
    return 1.0 / np.sqrt(1.0 + (f / fc) ** (2 * order))


def g_highpass(f, fc, order=2):
    return 1.0 / np.sqrt(1.0 + (fc / np.maximum(f, 1.0)) ** (2 * order))


# --- Circular (loop) processing ------------------------------------------------------------------

def loop_f(f, length):
    """Nearest frequency with a whole number of cycles in a loop of `length` seconds."""
    return max(1, round(f * length)) / length


def circ(fn, x, pad):
    """Run a causal filter as if x were periodic: prime it with the wrapped end."""
    p = min(pad, len(x))
    return fn(np.concatenate([x[-p:], x]))[p:]


def circ_conv(x, h):
    n = len(x)
    if len(h) > n:
        m = -(-len(h) // n)
        h = np.pad(h, (0, m * n - len(h))).reshape(m, n).sum(axis=0)
    return np.fft.irfft(np.fft.rfft(x) * np.fft.rfft(h, n), n)


def fft_shape(x, sr, gain_f):
    """Circular zero-phase filtering by a magnitude response gain_f(f)."""
    spec = np.fft.rfft(x)
    return np.fft.irfft(spec * gain_f(np.fft.rfftfreq(len(x), 1.0 / sr)), len(x))


def limit(x, ceiling, sr, window=0.008, wrap=True):
    """Look-ahead peak limiter (gain never exceeds what keeps |x| <= ceiling)."""
    mode = "wrap" if wrap else "nearest"
    need = np.minimum(1.0, ceiling / np.maximum(np.abs(x), 1e-12))
    w = n_of(window, sr)
    g = minimum_filter1d(need, 2 * w + 1, mode=mode)
    for _ in range(2):
        g = uniform_filter1d(g, w // 2 + 1, mode=mode)
    return x * g


# --- Reverb --------------------------------------------------------------------------------------

def make_ir(sr, rt60, rng, pre=0.012, damp=0.5, early=0.35, size=1.0, dur=None):
    """Synthetic room impulse response: frequency-dependent exponential decay of noise,
    a diffuse build-up and sparse early reflections. Unit energy."""
    n = n_of(dur if dur else min(rt60 * 1.25, 7.0), sr)
    t = tvec(n, sr)
    nz = rng.standard_normal(n)
    bands = [(lp(nz, 250, sr, 4), 1.15), (bp(nz, 250, 1200, sr, 3), 1.0),
             (bp(nz, 1200, 4500, sr, 3), 1.0 - 0.35 * damp), (hp(nz, 4500, sr, 4), 1.0 - 0.7 * damp)]
    ir = sum(b * 10.0 ** (-3.0 * t / (rt60 * k)) for b, k in bands)
    ir *= 1.0 - np.exp(-t / (0.018 * size))
    er = np.zeros(n)
    for tt in np.sort(rng.uniform(0.003, 0.065 * size, 14)):
        i = int(tt * sr)
        if i < n:
            er[i] += rng.choice([-1.0, 1.0]) * np.exp(-tt / (0.045 * size)) * rng.uniform(0.5, 1.0)
    er = lp(er, 6500, sr)
    ir = ir / np.sqrt(np.sum(ir ** 2)) + early * er / (np.sqrt(np.sum(er ** 2)) + 1e-12)
    ir = np.concatenate([np.zeros(n_of(pre, sr)), ir])
    return ir / np.sqrt(np.sum(ir ** 2))


def add_reverb(x, sr, rng, rt60, mix, **kw):
    """One-shot reverb; `mix` is the wet peak relative to the dry peak. Extends the sound."""
    x = fade(x, sr, 0.0, 0.01)
    wet = signal.fftconvolve(x, make_ir(sr, rt60, rng, **kw))
    out = np.zeros(len(wet))
    out[:len(x)] += x
    return out + mix * wet * (np.max(np.abs(x)) / (np.max(np.abs(wet)) + 1e-12))


# --- Synthesis building blocks -------------------------------------------------------------------

def ctrl_grid(n):
    idx = np.arange(0, n, CTRL)
    if idx[-1] != n - 1:
        idx = np.append(idx, n - 1)
    return idx


def additive(f0, sr, amp_fn, max_f=6000.0, rng=None, stretch=0.0):
    """Sum of partials k*f0(t) (optionally stretched). amp_fn(k, fk, tc) gives each partial's
    amplitude at control points (fk = its frequency there, tc = time in seconds)."""
    n = len(f0)
    ph = TAU * phase_of(f0, n, sr)
    idx = ctrl_grid(n)
    tc, f0c = idx / sr, f0[idx]
    grid = np.arange(n)
    out = np.zeros(n)
    kmax = int(max_f / max(float(np.min(f0)), 1.0))
    for k in range(1, kmax + 1):
        mult = k * np.sqrt(1.0 + stretch * k * k)
        fk = f0c * mult
        a = np.broadcast_to(amp_fn(k, fk, tc), fk.shape) * np.clip((max_f - fk) / (0.15 * max_f), 0, 1)
        if not np.any(a > 1e-5):
            continue
        p0 = rng.uniform(0, TAU) if rng is not None else 0.0
        out += np.interp(grid, idx, a) * np.sin(mult * ph + p0)
    return out


def formant_gain(fk, F, B, A, floor=0.01):
    """Sum of resonance curves (centre F, bandwidth B, gain A; arrays (P, M) or (P, 1))."""
    return np.sum(A / np.sqrt(1.0 + ((fk[None, :] - F) / (0.5 * B)) ** 2), axis=0) + floor


def body_arrays(body):
    b = np.array(body, dtype=float)
    return b[:, 0:1], b[:, 1:2], 10.0 ** (b[:, 2:3] / 20.0)


# Formant tables (Hz, bandwidth Hz, dB) for sung vowels.
VOWELS = {
    ("bass", "a"): [(600, 60, 0), (1040, 70, -7), (2250, 110, -9), (2450, 120, -9), (2750, 130, -20)],
    ("bass", "o"): [(400, 40, 0), (750, 80, -11), (2400, 100, -21), (2600, 120, -20), (2900, 120, -40)],
    ("bass", "u"): [(250, 60, 0), (600, 40, -20), (2400, 100, -32), (2675, 140, -28), (2950, 120, -36)],
    ("tenor", "a"): [(650, 80, 0), (1080, 90, -6), (2650, 120, -7), (2900, 130, -8), (3250, 140, -22)],
    ("tenor", "o"): [(400, 40, 0), (800, 80, -10), (2600, 100, -12), (2800, 120, -12), (3000, 120, -26)],
    ("tenor", "u"): [(350, 40, 0), (600, 60, -20), (2700, 100, -17), (2900, 120, -14), (3300, 120, -26)],
    ("alto", "a"): [(800, 80, 0), (1150, 90, -4), (2800, 120, -20), (3500, 130, -36), (4950, 140, -60)],
    ("alto", "o"): [(450, 70, 0), (800, 80, -9), (2830, 100, -16), (3500, 130, -28), (4950, 135, -55)],
    ("alto", "u"): [(325, 50, 0), (700, 60, -12), (2530, 170, -30), (3500, 180, -40), (4950, 200, -64)],
    ("alto", "i"): [(350, 50, 0), (1700, 100, -20), (2700, 120, -30), (3700, 150, -36), (4950, 200, -60)],
    ("soprano", "a"): [(800, 80, 0), (1150, 90, -6), (2900, 120, -32), (3900, 130, -20), (4950, 140, -50)],
    ("soprano", "o"): [(450, 70, 0), (800, 80, -11), (2830, 100, -22), (3800, 130, -22), (4950, 135, -50)],
    ("soprano", "u"): [(325, 50, 0), (700, 60, -16), (2700, 170, -35), (3800, 180, -40), (4950, 200, -60)],
    ("soprano", "i"): [(270, 60, 0), (2140, 90, -12), (2950, 100, -26), (3900, 120, -26), (4950, 120, -44)],
    ("soprano", "e"): [(350, 60, 0), (2000, 100, -20), (2800, 120, -15), (3600, 150, -40), (4950, 200, -56)],
}


def vowel_curve(part, vowels, m, scale=1.0, bw=1.0):
    """Formant arrays (F, B, A), each (5, len(m)), morphing through `vowels` as m goes 0..V-1."""
    tab = np.array([VOWELS[(part, v)] for v in vowels], dtype=float)
    m = np.clip(np.atleast_1d(np.asarray(m, dtype=float)), 0, len(vowels) - 1)
    i0 = np.floor(m).astype(int)
    i1 = np.minimum(i0 + 1, len(vowels) - 1)
    fr = (m - i0)[:, None, None]
    p = tab[i0] * (1.0 - fr) + tab[i1] * fr
    return p[:, :, 0].T * scale, p[:, :, 1].T * bw, 10.0 ** (p[:, :, 2].T / 20.0)


def vibrato(n, sr, rng, depth, rate, onset=0.25, drift=4.0):
    """Pitch deviation in cents: vibrato fading in after the onset, plus slow drift."""
    t = tvec(n, sr)
    ramp = np.clip((t - 0.4 * onset) / max(onset, 1e-3), 0, 1)
    r = rate * (1.0 + 0.04 * slow_noise(n, sr, rng, 1.5))
    vib = depth * ramp * np.sin(TAU * phase_of(r, n, sr) + rng.uniform(0, TAU))
    return vib + drift * slow_noise(n, sr, rng, 3.0)


def voice(f0, sr, rng, part, vowels, morph=None, tilt=0.9, max_f=5000.0, bw=1.6, scale=1.0, floor=0.01):
    """Additive sung voice: harmonics of f0 (Hz per sample) under morphing vowel formants."""
    vowels = [vowels] if isinstance(vowels, str) else list(vowels)
    idx = ctrl_grid(len(f0))
    if morph is None:
        m = np.linspace(0, len(vowels) - 1, len(idx))
    else:
        m = np.asarray(morph, dtype=float)[idx]
    F, B, A = vowel_curve(part, vowels, m, scale, bw)
    return additive(f0, sr, lambda k, fk, tc: k ** -tilt * formant_gain(fk, F, B, A, floor), max_f=max_f, rng=rng)


def formant_noise(n, sr, rng, part, vowel, scale=1.0, bw=2.0):
    F, B, A = vowel_curve(part, [vowel], [0.0], scale, bw)
    return spectral_noise(n, sr, lambda t, f: formant_gain(f[:, 0], F, B, A)[:, None] * np.ones_like(t), rng, 1024)


def thump(sr, dur, f0, f1, tau, decay, attack=0.0008):
    """Sine with a falling pitch and exponential decay: the body of any impact."""
    n = n_of(dur, sr)
    return tail_fade(sine(glide(n, sr, f0, f1, tau), n, sr) * env_exp(n, sr, attack, decay), sr)


def burst(sr, dur, rng, lo, hi, decay, attack=0.0003, order=2):
    """Filtered noise burst with exponential decay, peak-normalised (lo/hi None = open)."""
    n = n_of(dur, sr)
    x = white(n, rng)
    if lo and hi:
        x = bp(x, lo, hi, sr, order)
    elif hi:
        x = lp(x, hi, sr, order)
    elif lo:
        x = hp(x, lo, sr, order)
    return norm_peak(tail_fade(x * env_exp(n, sr, attack, decay), sr))


def modes(sr, dur, freqs, amps, decays, attack=0.0004, beat=0.0, rng=None):
    """Modal synthesis: exponentially decaying partials (struck wood, metal, skin)."""
    n = n_of(dur, sr)
    t = tvec(n, sr)
    out = np.zeros(n)
    for f, a, d in zip(freqs, amps, decays):
        if f >= 0.45 * sr:
            continue
        if beat and rng is not None:
            df = f * beat * rng.uniform(0.5, 1.5)
            part = 0.5 * (np.sin(TAU * (f - df / 2) * t) + np.sin(TAU * (f + df / 2) * t))
        else:
            part = np.sin(TAU * f * t)
        out += a * part * np.exp(-t / d)
    if attack > 0:
        out *= np.clip(t / attack, 0, 1)
    return tail_fade(out, sr)


def grain_times(rng, count, t0, t1, skew=1.0):
    """Sorted random event times; skew > 1 crowds them toward t0."""
    return t0 + (t1 - t0) * np.sort(rng.random(count)) ** skew


def env_times(rng, count, env, sr):
    """Random event times distributed with density proportional to `env`."""
    cdf = np.cumsum(np.maximum(env, 0.0))
    return np.sort(np.interp(rng.random(count) * cdf[-1], cdf, tvec(len(env), sr)))


def clink(sr, rng, lo=1800.0, hi=5200.0, decay=0.03):
    """One small metal-on-metal tick (a chain link)."""
    f = rng.uniform(lo, hi)
    fr = [f, f * rng.uniform(1.35, 1.65), f * rng.uniform(2.3, 2.9), f * rng.uniform(3.6, 4.4)]
    de = [decay * rng.uniform(0.6, 1.4) * k for k in (1.0, 0.7, 0.5, 0.35)]
    s = norm_peak(modes(sr, decay * 5, fr, [1.0, 0.6, 0.4, 0.2], de, attack=0.0002))
    return place(s, 0.3 * burst(sr, 0.004, rng, 2000, 9000, 0.0008), 0)


def bubble(sr, f0, rise=1.0, dur=None):
    """Bubble resonance (van den Doel): damped sine whose pitch rises as it decays."""
    d = 0.043 * f0 + 0.0014 * f0 ** 1.5
    n = n_of(dur or min(0.6, 5.0 / d), sr)
    t = tvec(n, sr)
    f = np.minimum(f0 * (1.0 + rise * d * t), 0.4 * sr)
    return np.sin(TAU * phase_of(f, n, sr)) * np.exp(-d * t) * np.clip(t / 0.0008, 0, 1)


def crackle(n, sr, rng, rate, lo=1500.0, hi=7000.0):
    """Sparse random clicks (rate in events/s, constant or per sample), band-passed."""
    rate = np.broadcast_to(np.asarray(rate, dtype=float), (n,))
    hits = rng.random(n) < rate / sr
    amp = rng.exponential(1.0, n) ** 1.5 * rng.choice([-1.0, 1.0], n)
    return bp(hits * amp, lo, hi, sr)


def cloth(sr, rng, dur, count, lo=900.0, hi=5000.0, skew=1.6):
    """Fabric rustle built from many tiny noise grains."""
    out = np.zeros(n_of(dur, sr))
    for t0 in grain_times(rng, count, 0.0, dur * 0.85, skew):
        gn = n_of(rng.uniform(0.004, 0.02), sr)
        g = bp(white(gn, rng), lo, hi, sr) * np.hanning(gn)
        place(out, g * rng.uniform(0.3, 1.0), int(t0 * sr))
    return out


def flicker(n, sr, rng, rate=14.0, depth=0.6):
    """Random amplitude flicker for flames."""
    return np.clip(1.0 + depth * slow_noise(n, sr, rng, rate), 0.05, None)


def debris(sr, rng, dur, count, t0, t1, lo=1500.0, hi=6500.0, skew=1.6, fall=0.25):
    out = np.zeros(n_of(dur, sr))
    for t in grain_times(rng, count, t0, t1, skew):
        g = burst(sr, 0.012, rng, rng.uniform(lo, lo * 2), rng.uniform(hi * 0.6, hi), rng.uniform(0.001, 0.003))
        place(out, g * rng.uniform(0.2, 1.0) * np.exp(-(t - t0) / fall), int(t * sr))
    return out


# --- Instruments (shared by SFX stingers and music) ----------------------------------------------

def drum(sr, rng, f_hi, f_lo, tau, decay, skin=0.3, skin_lp=900.0, click=0.15, over=0.2, dur=None, vel=1.0):
    """Membrane drum: pitched body with overtone modes, skin noise and stick click."""
    dur = dur or min(decay * 6, 3.0)
    n = n_of(dur, sr)
    body = thump(sr, dur, f_hi, f_lo, tau, decay)
    ov = modes(sr, dur, [f_lo * 1.59, f_lo * 2.14, f_lo * 2.65], [1, 0.6, 0.35],
               [decay * 0.35, decay * 0.25, decay * 0.18])
    sk = lp(white(n, rng), skin_lp, sr) * env_exp(n, sr, 0.0005, decay * 0.25)
    ck = bp(white(n, rng), 1500, 6000, sr) * env_exp(n, sr, 0.0001, 0.003)
    out = body + over * norm_peak(ov) + skin * norm_peak(sk) + click * norm_peak(ck)
    return vel * norm_peak(out)


def snare(sr, rng, tone=190.0, decay=0.14, vel=1.0, wires=0.9):
    dur = decay * 5
    n = n_of(dur, sr)
    body = thump(sr, dur, tone * 1.5, tone, 0.008, 0.06)
    w = bp(white(n, rng), 1800, 7000, sr) * env_exp(n, sr, 0.0004, decay)
    return vel * norm_peak(0.7 * body + wires * norm_peak(w))


def shaker(sr, rng, vel=1.0, dur=0.08):
    n = n_of(dur, sr)
    x = bp(white(n, rng), 3000, 8000, sr) * env_pts(n, sr, [(0, 0), (0.008, 1), (dur, 0)]) ** 2
    return vel * norm_peak(x)


def metal_tick(sr, rng, vel=1.0, decay=0.022):
    n = n_of(decay * 6, sr)
    t = tvec(n, sr)
    base = 520.0 * rng.uniform(0.97, 1.03)
    sq = sum(np.sign(np.sin(TAU * base * r * t + rng.uniform(0, TAU))) for r in (1, 1.342, 1.523, 1.712, 2.004, 2.31))
    return vel * norm_peak(lp(bp(sq, 2500, 7000, sr), 7500, sr) * env_exp(n, sr, 0.0003, decay))


def anvil(sr, rng, f=780.0, vel=1.0, decay=0.45):
    dur = decay * 3
    s = modes(sr, dur, [f, f * 2.31, f * 3.52, f * 4.87, f * 6.3], [1, 0.5, 0.32, 0.2, 0.1],
              [decay, decay * 0.55, decay * 0.38, decay * 0.25, decay * 0.15], beat=0.0015, rng=rng)
    out = norm_peak(s) + 0.45 * burst(sr, dur, rng, None, 3500, 0.005) + 0.4 * thump(sr, dur, 320, 200, 0.006, 0.02)
    return vel * norm_peak(lp(out, 7000, sr))


def clank(sr, rng, base=None, vel=1.0, decay=0.22):
    """Heavy sheet-metal / iron clank (inharmonic modes plus a noise hit)."""
    base = base or rng.uniform(160, 260)
    rs = np.concatenate([[1.0], np.sort(rng.uniform(1.3, 11.0, 9))])
    s = modes(sr, decay * 4, base * rs, rs ** -0.5 * rng.uniform(0.5, 1.0, 10), decay * rs ** -0.45, beat=0.003, rng=rng)
    return vel * norm_peak(lp(norm_peak(s) + 0.6 * burst(sr, decay * 4, rng, 400, 3500, 0.012), 7000, sr))


def timpani(f, sr, rng, vel=1.0, decay=1.6, dur=None):
    dur = dur or decay * 1.8
    rs = [1.0, 1.504, 1.742, 2.0, 2.245, 2.494]
    s = modes(sr, dur, [f * r for r in rs], [1, 0.55, 0.3, 0.25, 0.14, 0.1],
              [decay * k for k in (1, 0.7, 0.5, 0.45, 0.35, 0.3)], beat=0.002, rng=rng, attack=0.001)
    out = norm_peak(s) + 0.5 * thump(sr, dur, f * 1.6, f, 0.02, 0.09) + 0.35 * burst(sr, dur, rng, None, 1500, 0.015)
    return vel * norm_peak(out)


def cymbal(sr, rng, dur=1.6, decay=0.6, vel=1.0, bright=1.0):
    n = n_of(dur, sr)
    t = tvec(n, sr)
    out = np.zeros(n)
    for f in np.exp(rng.uniform(np.log(380), np.log(8000), 70)):
        d = decay * (f / 2500.0) ** -0.3 * rng.uniform(0.6, 1.4)
        out += rng.uniform(0.3, 1.0) * (f / 1000.0) ** -0.25 * np.sin(TAU * f * t + rng.uniform(0, TAU)) * np.exp(-t / d)
    nz = spectral_noise(n, sr, lambda tt, ff: g_band(ff, 5500, 0.9) * np.exp(-tt / (decay * 0.8)), rng)
    out = lp(norm_peak(out) + 0.5 * norm_peak(nz), 9500 * bright, sr) * np.clip(t / 0.0015, 0, 1)
    out = tail_fade(norm_peak(out), sr, 0.3, 0.4)
    return vel * norm_peak(place(out, 0.35 * burst(sr, 0.05, rng, 2500, 7000, 0.004), 0))


def gong(sr, rng, f0=64.0, dur=4.4, decay=3.2):
    """Tam-tam: many inharmonic modes; upper modes bloom late (the shimmer swell)."""
    n = n_of(dur, sr)
    t = tvec(n, sr)
    rs = np.concatenate([[1.0, 1.52, 1.98, 2.42, 2.94, 3.41, 3.98, 4.62, 5.2],
                         np.sort(np.exp(rng.uniform(np.log(5.5), np.log(64), 46)))])
    out = np.zeros(n)
    for r in rs:
        f = f0 * r
        if f > 0.42 * sr:
            continue
        rise = 0.003 + 0.9 * (np.log(r) / np.log(64)) ** 1.6
        d = decay * r ** -0.35 * rng.uniform(0.75, 1.25)
        fc = f * (1.0 + 0.015 * np.exp(-t / 0.5)) if r < 6 else f
        warble = 1.0 + 0.25 * np.sin(TAU * rng.uniform(0.3, 1.5) * t + rng.uniform(0, TAU))
        out += (r ** -0.5 * rng.uniform(0.5, 1.0) * np.sin(TAU * phase_of(fc, n, sr) + rng.uniform(0, TAU))
                * (1.0 - np.exp(-t / rise)) * np.exp(-t / d) * warble)
    mallet = mixdown(dur, sr, (burst(sr, 0.3, rng, None, 800, 0.05), 0, 0.5), (thump(sr, 0.6, 85, 60, 0.05, 0.16), 0, 1.0))
    return fade(norm_peak(out) + 0.45 * norm_peak(mallet), sr, 0, 0.8)


# Church-bell partials: (ratio to strike note, amplitude, relative decay).
BELL = [(0.5, 0.8, 1.0), (1.0, 0.7, 0.62), (1.19, 0.6, 0.5), (1.5, 0.35, 0.4), (2.0, 0.6, 0.32),
        (2.52, 0.3, 0.24), (2.66, 0.26, 0.21), (3.01, 0.22, 0.17), (4.17, 0.15, 0.12),
        (5.43, 0.09, 0.09), (6.8, 0.05, 0.07)]


def bell(f, sr, rng, decay=4.0, dur=None, bright=1.0, iron=0.0, strike=0.25):
    n = n_of(dur or decay * 1.3, sr)
    t = tvec(n, sr)
    out = np.zeros(n)
    for r, a, d in BELL:
        rr = r if r == 1.0 else r * (1.0 + iron * rng.normal(0, 0.012))
        fr = f * rr
        if fr > 0.42 * sr:
            continue
        aa = a * bright ** max(0.0, np.log2(r))
        dd = decay * d * (1.0 - 0.45 * iron * min(1.0, max(0.0, (r - 1.0) / 2.0)))
        bt = fr * rng.uniform(0.0005, 0.002)
        p1, p2 = rng.uniform(0, TAU, 2)
        out += aa * 0.5 * (np.sin(TAU * (fr - bt / 2) * t + p1) + np.sin(TAU * (fr + bt / 2) * t + p2)) * np.exp(-t / dd)
    if iron:
        for r in rng.uniform(3.3, 7.5, 6):
            if f * r < 0.42 * sr:
                out += 0.12 * iron * np.sin(TAU * f * r * t + rng.uniform(0, TAU)) * np.exp(-t / rng.uniform(0.12, 0.4))
    out = tail_fade(norm_peak(out * np.clip(t / 0.0015, 0, 1)), sr, 0.3, 1.0)
    return place(out, strike * burst(sr, 0.1, rng, 700, 5000, 0.008), 0)


def pad(f, dur, sr, rng, att=1.0, rel=1.8, cutoff=1200.0, voices=3, detune=9.0, vel=1.0):
    """Detuned band-limited saws through a gentle low-pass: string/synth pad voice."""
    n = n_of(dur + rel, sr)
    x = sum(polyblep_saw(f * 2 ** (d / 1200), n, sr, rng.random()) for d in np.linspace(-detune, detune, voices))
    x = lp(lp(x, cutoff, sr), cutoff * 1.5, sr)
    return vel * norm_peak(x) * env_note(n, sr, dur, att, rel)


def bass(f, dur, sr, rng, vel=0.9, kind="round", rel=0.06, cutoff=250.0):
    n = n_of(dur + rel, sr)
    t = tvec(n, sr)
    ph = TAU * phase_of(f, n, sr)
    if kind == "round":
        x = np.sin(ph) + 0.3 * np.sin(2 * ph) + 0.1 * np.sin(3 * ph)
        x = sat(x / 1.4, 1.6) * (0.7 + 0.3 * np.exp(-t / 0.15))
        env = env_note(n, sr, dur, 0.006, rel)
    else:
        x = polyblep_saw(f, n, sr) + 0.6 * np.sin(ph)
        x = sat(norm_peak(tv_filter(x, cutoff + 1800 * vel * np.exp(-t / 0.06), sr, q=1.2)), 2.0)
        env = env_note(n, sr, dur, 0.003, rel)
    return vel * norm_peak(x) * env


VIOLA_BODY = [(290, 140, 0), (540, 220, -4), (1100, 380, -3), (2300, 700, -9), (3400, 900, -16)]
OUD_BODY = [(180, 120, 0), (420, 250, -3), (1100, 600, -8), (2600, 1200, -14)]
REED_BODY = [(500, 400, -4), (1250, 700, 0), (2700, 1100, -7)]


def bowed(f, dur, sr, rng, vel=0.8, att=0.2, rel=0.45, vib=16.0, rate=5.2, body=VIOLA_BODY, bright=1.0, swell=0.3):
    """Bowed string: sawtooth-like harmonics through body resonances, delayed vibrato."""
    n = n_of(dur + rel, sr)
    t = tvec(n, sr)
    cents = -14.0 * np.exp(-t / 0.05) + vibrato(n, sr, rng, vib, rate, onset=min(0.35, dur * 0.5))
    F, B, A = body_arrays(body)
    cut = 1600 + 2600 * vel * bright
    x = additive(f * 2 ** (cents / 1200), sr, lambda k, fk, tc: formant_gain(fk, F, B, A, 0.06) * np.exp(-fk / cut) / k,
                 max_f=7000, rng=rng)
    x = norm_peak(x) + 0.025 * norm_peak(bp(white(n, rng), 1500, 5000, sr))
    u = np.clip(t / max(dur, 1e-3), 0, 1)
    return vel * x * env_note(n, sr, dur, att, rel) * (1.0 - swell + swell * np.sin(np.pi * u) ** 0.7)


def brass(f, dur, sr, rng, vel=0.8, att=0.04, rel=0.2, bright=1.0, vib=0.0, swell=False):
    """Brass: additive harmonics whose brightness follows the breath (the 'blat')."""
    n = n_of(dur + rel, sr)
    t = tvec(n, sr)
    cents = (0.0 if swell else -30.0) * np.exp(-t / 0.03) + 3.0 * slow_noise(n, sr, rng, 4.0)
    if vib:
        cents = cents + vib * np.clip((t - 0.3) / 0.4, 0, 1) * np.sin(TAU * 5.0 * t)
    lo = 1.3 * f + 120.0
    hi = f * (2.5 + 6.0 * vel * bright) + 500.0 * vel * bright
    if swell:
        cut = lo + (hi - lo) * np.clip(t / max(dur, 1e-3), 0, 1) ** 1.6
        env = np.clip(t / max(dur, 1e-3), 0, 1) ** 1.4 * env_note(n, sr, dur, 0.05, rel)
    else:
        a = np.clip(t / att, 0, 1)
        cut = lo + (hi - lo) * a * (0.6 + 0.4 * np.exp(-np.maximum(t - att, 0) / 0.15))
        env = env_note(n, sr, dur, att, rel)
    k_end = min(n - 1, n_of(dur, sr))
    cut = np.where(t > dur, lo + (cut[k_end] - lo) * np.exp(-(t - dur) / max(rel, 1e-3)), cut)
    cut_c = cut[ctrl_grid(n)]
    x = additive(f * 2 ** (cents / 1200), sr, lambda k, fk, tc: k ** -0.55 * np.exp(-fk / cut_c), max_f=6500, rng=rng)
    breath = norm_peak(bp(white(n, rng), 600, 2500, sr) * env_exp(n, sr, att * 0.5, 0.04))
    return vel * (norm_peak(x) * env + 0.06 * breath * (0.0 if swell else 1.0))


def organ(f, dur, sr, rng, att=0.08, rel=0.4, chiff=0.05,
          stops=((0.5, 0.4), (1, 1.0), (2, 0.45), (3, 0.14), (4, 0.18), (6, 0.05), (8, 0.04))):
    n = n_of(dur + rel, sr)
    t = tvec(n, sr)
    x = np.zeros(n)
    for r, a in stops:
        fr = f * r * (1.0 + rng.normal(0, 0.0005))
        if fr < 0.42 * sr:
            x += a * np.sin(TAU * fr * t + rng.uniform(0, TAU))
    x = norm_peak(x) + 0.012 * norm_peak(bp(white(n, rng), f * 2, min(f * 8, 0.4 * sr), sr))
    ch = norm_peak(bp(white(n, rng), f * 3, min(f * 6, 0.4 * sr), sr) * env_exp(n, sr, 0.005, 0.03))
    return x * env_note(n, sr, dur, att, rel) + chiff * ch


def choir(f, dur, sr, rng, part="alto", vowels=("a",), att=0.6, rel=1.2, voices=3, vib=15.0, breath=0.04, max_f=5000.0):
    n = n_of(dur + rel, sr)
    t = tvec(n, sr)
    morph = np.clip(t / max(dur, 1e-3), 0, 1) * (len(vowels) - 1)
    out = np.zeros(n)
    for _ in range(voices):
        cents = rng.normal(0, 6.0) + vibrato(n, sr, rng, vib * rng.uniform(0.8, 1.2), rng.uniform(4.8, 5.8), onset=0.4)
        out += voice(f * 2 ** (cents / 1200), sr, rng, part, vowels, morph, max_f=max_f, bw=1.7)
    out = norm_peak(out)
    if breath:
        out += breath * norm_peak(formant_noise(n, sr, rng, part, vowels[0]))
    return out * env_note(n, sr, dur, att, rel)


def pluck(f, sr, rng, dur=1.0, hold=None, vel=0.8, decay=0.8, bright=0.5, pos=0.15, body=None, stretch=0.0002):
    """Plucked string: additive partials, upper ones dying faster; optional finger damping."""
    n = n_of(dur, sr)
    t = tvec(n, sr)
    cut = 900 + 4500 * bright * vel
    body_g = body_arrays(body) if body else None

    def amp(k, fk, tc):
        tau = decay / (1.0 + fk / 350.0) ** 0.8
        a = abs(np.sin(np.pi * k * pos)) / k * np.exp(-fk / cut) * np.exp(-tc / tau)
        return a * formant_gain(fk, *body_g, 0.3) if body_g else a

    x = norm_peak(additive(f * (1.0 + 0.003 * np.exp(-t / 0.03)), sr, amp, max_f=min(8000, 0.45 * sr), stretch=stretch))
    if hold is not None:
        x *= np.where(t < hold, 1.0, np.exp(-np.maximum(t - hold, 0) / 0.05))
    x = vel * x
    place(x, 0.1 * vel * burst(sr, 0.03, rng, 1200, 6000, 0.004), 0)
    return tail_fade(x, sr, 0.3, 0.5)


def reed(f, dur, sr, rng, vel=0.7, att=0.06, rel=0.15, musette=9.0, swell=0.15, bright=1.0):
    """Free reed (accordion): two slightly detuned reeds beating (musette)."""
    n = n_of(dur + rel, sr)
    t = tvec(n, sr)
    F, B, A = body_arrays(REED_BODY)
    out = np.zeros(n)
    for det in (-0.5 * musette, 0.5 * musette):
        cents = det + 2.5 * slow_noise(n, sr, rng, 2.0)
        out += additive(f * 2 ** (cents / 1200), sr,
                        lambda k, fk, tc: k ** -0.7 * formant_gain(fk, F, B, A, 0.12) * np.exp(-fk / (3500 * bright)),
                        max_f=7000, rng=rng)
    u = np.clip(t / max(dur, 1e-3), 0, 1)
    return vel * norm_peak(out) * env_note(n, sr, dur, att, rel) * (1.0 + swell * np.sin(np.pi * u)) / (1.0 + swell)


# --- Note helpers --------------------------------------------------------------------------------

_NOTE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def midi(name):
    """'C#4', 'Bb2' -> MIDI note number."""
    i, acc = 1, 0
    while name[i] in "#b":
        acc += 1 if name[i] == "#" else -1
        i += 1
    return 12 * (int(name[i:]) + 1) + _NOTE[name[0].upper()] + acc


def hz(name):
    return 440.0 * 2.0 ** ((midi(name) - 69) / 12.0)


def melody(spec, start=0.0):
    """'D4:1.5 F4:.5 r:1 | ...' -> [(start_beat, Hz, beats)]; duration defaults to 1 beat."""
    out, pos = [], start
    for tok in spec.replace("|", " ").split():
        name, _, d = tok.partition(":")
        d = float(d) if d else 1.0
        if name != "r":
            out.append((pos, hz(name), d))
        pos += d
    return out


# --- Sound effects -------------------------------------------------------------------------------

SFX = {}


def sfx(name):
    def deco(fn):
        SFX[name] = fn
        return fn
    return deco


def whoosh(rng, dur, f_lo, f_hi, width, peak=0.45, air=0.0, flutter=0.0, drive=1.0, sr=SR):
    """Band-passed noise whose centre rises and falls as the limb passes (Doppler-like)."""
    n = n_of(dur, sr)

    def bump(u):
        u = np.clip(u, 0, 1)
        return np.where(u < peak, np.sin(0.5 * np.pi * u / peak) ** 2, np.cos(0.5 * np.pi * (u - peak) / (1 - peak)) ** 2)

    x = spectral_noise(n, sr, lambda t, f: g_band(f, f_lo * (f_hi / f_lo) ** bump(t / dur), width) * g_lowpass(f, 9000), rng)
    b = bump(tvec(n, sr) / dur)
    out = norm_peak(x * b ** 1.4)
    if air:
        out += air * norm_peak(lp(white(n, rng), 230, sr) * b ** 2)
    if flutter:
        out *= 1.0 + flutter * np.sin(TAU * phase_of(25 + 25 * b, n, sr))
    return sat(norm_peak(out), drive) if drive > 1.0 else out


@sfx("whoosh_light")
def _(rng):
    return whoosh(rng, 0.17, 1300, 3600, 0.55, peak=0.3, drive=1.5)


@sfx("whoosh_medium")
def _(rng):
    return whoosh(rng, 0.25, 800, 2400, 0.7, peak=0.33, air=0.25, drive=2.2)


@sfx("whoosh_heavy")
def _(rng):
    return whoosh(rng, 0.34, 420, 1500, 0.85, peak=0.38, air=0.3, flutter=0.2, drive=3.0)


def impact(rng, *, dur, f0, f1, tf, td, sub, knock_f, knock_d, slap, slap_d, crack_d, crack_g, tail, drive,
           crunch=0, slap_g=1.6, knock_g=0.8, meat_g=0.9):
    """Layered body hit: saturated low thump (+ sub), mid knock, low-mid 'meat', fleshy slap,
    short high crack. Noise layers get higher gains because their crest factor is higher."""
    n = n_of(dur, SR)
    body = sat(thump(SR, dur, f0, f1, tf, td), 2.5)
    knock = norm_peak(thump(SR, dur, knock_f * 1.7, knock_f, 0.006, knock_d))
    nz = white(n, rng)
    meat = norm_peak(bp(nz, 150, 750, SR) * env_exp(n, SR, 0.0008, slap_d * 1.7))
    nz = white(n, rng)
    skin = 0.7 * bp(nz, slap[0], slap[1], SR) + 0.5 * reson(nz, slap[0] * 1.8, 2.5, SR) + 0.3 * reson(nz, slap[1] * 0.55, 3.0, SR)
    skin = norm_peak(skin * env_exp(n, SR, 0.0006, slap_d))
    crack = norm_peak(lp(hp(white(n, rng), 3200, SR), 9000, SR) * env_exp(n, SR, 0.0001, crack_d))
    out = body + knock_g * knock + meat_g * meat + slap_g * skin + crack_g * crack
    if sub:
        out += sub * thump(SR, dur, f1 * 1.5, f1 * 0.78, 0.05, td * 1.6, attack=0.004)
    if tail:
        out += 0.45 * norm_peak(lp(white(n, rng), 260, SR) * env_exp(n, SR, 0.006, tail))
    if crunch:
        out += 0.5 * norm_peak(debris(SR, rng, dur, crunch, 0.002, 0.05, 1200, 5000, 1.3, 1.0))
    return tail_fade(sat(norm_peak(out), drive), SR, 0.35)


@sfx("hit_light")
def _(rng):
    return impact(rng, dur=0.2, f0=230, f1=90, tf=0.014, td=0.036, sub=0.0, knock_f=480, knock_d=0.02,
                  slap=(900, 4000), slap_d=0.02, crack_d=0.004, crack_g=0.6, tail=0.0, drive=1.8)


@sfx("hit_medium")
def _(rng):
    return impact(rng, dur=0.32, f0=180, f1=64, tf=0.022, td=0.065, sub=0.3, knock_f=370, knock_d=0.03,
                  slap=(700, 3400), slap_d=0.028, crack_d=0.005, crack_g=0.55, tail=0.06, drive=2.3, crunch=3)


@sfx("hit_heavy")
def _(rng):
    x = impact(rng, dur=0.5, f0=150, f1=48, tf=0.032, td=0.1, sub=0.55, knock_f=300, knock_d=0.045,
               slap=(550, 3000), slap_d=0.036, crack_d=0.006, crack_g=0.55, tail=0.16, drive=2.8, crunch=7)
    return add_reverb(x, SR, rng, 0.45, 0.12, damp=0.7)


@sfx("ko_hit")
def _(rng):
    x = impact(rng, dur=0.9, f0=145, f1=42, tf=0.04, td=0.15, sub=0.8, knock_f=270, knock_d=0.055,
               slap=(500, 2800), slap_d=0.042, crack_d=0.007, crack_g=0.55, tail=0.3, drive=3.2, crunch=10)
    x = mixdown(1.5, SR, (x, 0, 1.0), (thump(SR, 1.4, 70, 34, 0.12, 0.45, attack=0.01), 0.0, 0.45))
    return add_reverb(x, SR, rng, 1.9, 0.38, damp=0.6, pre=0.02)


@sfx("block")
def _(rng):
    d = 0.2
    n = n_of(d, SR)
    out = (thump(SR, d, 320, 175, 0.012, 0.042) + 0.45 * thump(SR, d, 120, 85, 0.02, 0.06)
           + 0.6 * norm_peak(lp(white(n, rng), 1100, SR) * env_exp(n, SR, 0.0008, 0.028))
           + 0.45 * burst(SR, d, rng, 2200, 6500, 0.0035, attack=0.0001)
           + 0.25 * norm_peak(modes(SR, d, [1850, 2930, 4100], [1, 0.6, 0.3], [0.014, 0.009, 0.006])))
    return sat(norm_peak(out), 1.8)


@sfx("armor")
def _(rng):
    d = 0.75
    n = n_of(d, SR)
    wood = modes(SR, d, [196, 371, 612, 947, 1418], [0.7, 0.5, 0.38, 0.22, 0.12], [0.2, 0.15, 0.1, 0.07, 0.045], beat=0.004, rng=rng)
    ring = modes(SR, d, [233, 631, 1187], [0.5, 0.22, 0.1], [0.42, 0.26, 0.15], beat=0.006, rng=rng)
    out = (thump(SR, d, 150, 82, 0.03, 0.12) + 0.55 * norm_peak(wood) + 0.4 * norm_peak(ring)
           + 0.4 * norm_peak(lp(white(n, rng), 2400, SR) * env_exp(n, SR, 0.0005, 0.018)))
    return add_reverb(sat(norm_peak(out), 1.6), SR, rng, 0.5, 0.1, damp=0.6)


@sfx("grab")
def _(rng):
    d = 0.34
    n = n_of(d, SR)
    fwip = spectral_noise(n, SR, lambda t, f: g_band(f, 2400, 0.9), rng) * env_exp(n, SR, 0.012, 0.05)
    return (0.9 * norm_peak(cloth(SR, rng, d, 120, 900, 5200, skew=1.8)) + 0.5 * norm_peak(fwip)
            + 0.55 * thump(SR, d, 150, 95, 0.015, 0.035)
            + 0.35 * norm_peak(lp(white(n, rng), 900, SR) * env_exp(n, SR, 0.001, 0.02)))


@sfx("slam")
def _(rng):
    d = 0.85
    n = n_of(d, SR)
    out = (thump(SR, d, 115, 38, 0.05, 0.25) + 0.5 * norm_peak(thump(SR, d, 260, 160, 0.01, 0.05))
           + 0.75 * burst(SR, d, rng, 300, 2600, 0.03) + 0.3 * burst(SR, d, rng, 2500, 8000, 0.005)
           + 0.25 * norm_peak(debris(SR, rng, d, 28, 0.03, 0.55))
           + 0.5 * norm_peak(lp(white(n, rng), 180, SR) * env_exp(n, SR, 0.01, 0.3)))
    return add_reverb(sat(norm_peak(out), 2.8), SR, rng, 0.6, 0.12, damp=0.7)


def clap(rng, scale=1.0, decay=0.011):
    n = n_of(0.08, SR)
    nz = white(n, rng)
    x = 0.6 * bp(nz, 1000, 5200, SR) + 0.5 * reson(nz, 1300 * scale, 3, SR) + 0.4 * reson(nz, 2500 * scale, 4, SR)
    return norm_peak(x * env_exp(n, SR, 0.0004, decay))


@sfx("throw_break")
def _(rng):
    return sat(mixdown(0.32, SR, (clap(rng, 1.0), 0, 1.0), (clap(rng, 1.15), 0.032, 0.7),
                       (whoosh(rng, 0.16, 1800, 4200, 0.6, peak=0.35), 0.025, 0.35),
                       (thump(SR, 0.1, 200, 120, 0.01, 0.025), 0, 0.35)), 2.0)


@sfx("land")
def _(rng):
    d = 0.45
    n = n_of(d, SR)
    dust = spectral_noise(n, SR, lambda t, f: g_highpass(f, 1600) * g_lowpass(f, 7000), rng) * env_exp(n, SR, 0.018, 0.16)
    out = (thump(SR, d, 125, 50, 0.025, 0.09) + 0.5 * burst(SR, d, rng, 250, 1800, 0.028)
           + 0.28 * norm_peak(dust) + 0.15 * norm_peak(debris(SR, rng, d, 12, 0.01, 0.25, 1500, 6000)))
    return sat(norm_peak(out), 1.8)


@sfx("step")
def _(rng):
    d = 0.17
    n = n_of(d, SR)
    scuff = spectral_noise(n, SR, lambda t, f: g_band(f, 1100, 0.9), rng) * env_exp(n, SR, 0.004, 0.035)
    return sat(norm_peak(thump(SR, d, 100, 62, 0.015, 0.032) + 0.45 * norm_peak(scuff)), 1.4)


@sfx("step_soft")
def _(rng):
    d = 0.2
    n = n_of(d, SR)
    scuff = spectral_noise(n, SR, lambda t, f: g_band(f, 800, 0.8), rng) * env_exp(n, SR, 0.008, 0.045)
    swish = spectral_noise(n, SR, lambda t, f: g_band(f, 2600, 0.6), rng) * env_pts(n, SR, [(0, 0), (0.06, 1), (0.16, 0)])
    return 0.6 * thump(SR, d, 90, 60, 0.015, 0.03) + 0.55 * norm_peak(scuff) + 0.18 * norm_peak(swish)


@sfx("burst")
def _(rng):
    d = 0.5
    n = n_of(d, SR)
    t = tvec(n, SR)
    crk = norm_peak(crackle(n, SR, rng, 900 * np.exp(-t / 0.09) + 120 * np.exp(-t / 0.25), 1200, 6500) * np.exp(-t / 0.3))
    sizzle = spectral_noise(n, SR, lambda tt, f: g_highpass(f, 2500) * g_lowpass(f, 8000), rng) * env_exp(n, SR, 0.005, 0.12)
    out = (thump(SR, d, 280, 75, 0.012, 0.055) + 0.8 * burst(SR, d, rng, None, 4500, 0.018) + 0.5 * crk
           + 0.15 * norm_peak(sizzle) + 0.45 * burst(SR, d, rng, None, 300, 0.07, attack=0.002))
    return sat(norm_peak(out), 2.0)


@sfx("cast_ember")
def _(rng):
    d = 0.6
    n = n_of(d, SR)
    t = tvec(n, SR)
    fw = tv_filter(white(n, rng), env_pts(n, SR, [(0, 300), (0.1, 3800), (0.6, 900)]), SR, q=0.8) * env_exp(n, SR, 0.035, 0.22)
    roar = bp(colored(n, rng, 1.5), 150, 900, SR) * flicker(n, SR, rng) * env_exp(n, SR, 0.05, 0.25)
    crk = crackle(n, SR, rng, 90 * np.exp(-t / 0.3), 2000, 6500) * env_exp(n, SR, 0.02, 0.3)
    return (norm_peak(fw) + 0.55 * norm_peak(roar) + 0.3 * norm_peak(crk)
            + 0.35 * thump(SR, d, 95, 55, 0.04, 0.09, attack=0.01))


@sfx("cast_orb")
def _(rng):
    d = 0.5
    n = n_of(d, SR)
    t = tvec(n, SR)
    f = glide(n, SR, 220, 330, 0.12)
    tone = sine(f, n, SR) + 0.5 * sine(f * 1.5 * 1.004, n, SR) + 0.25 * sine(f * 2.01, n, SR)
    tone = sat(norm_peak(tone), 1.5) * (1.0 + 0.35 * np.sin(TAU * 18 * t)) * env_exp(n, SR, 0.03, 0.18)
    return (0.6 * norm_peak(tone) + 0.55 * mixdown(d, SR, (whoosh(rng, 0.4, 600, 2600, 0.6, peak=0.3), 0, 1.0))
            + 0.4 * thump(SR, d, 140, 70, 0.02, 0.07))


@sfx("cast_wave")
def _(rng):
    d = 1.3
    n = n_of(d, SR)
    t = tvec(n, SR)
    sigh = -100.0 * np.clip((t - 0.78) / 0.3, 0, 1) ** 2
    out = np.zeros(n)
    for note, g, part in (("A4", 1.0, "soprano"), ("E4", 0.55, "alto")):
        for _ in range(2):
            cents = sigh + rng.normal(0, 5) + vibrato(n, SR, rng, 22, 5.4, onset=0.25)
            out += g * voice(hz(note) * 2 ** (cents / 1200), SR, rng, part, ("o", "a"), tilt=1.0, max_f=6000, bw=1.5)
    out = norm_peak(out) + 0.06 * norm_peak(formant_noise(n, SR, rng, "soprano", "o"))
    swell = spectral_noise(n, SR, lambda tt, f: g_band(f, 500, 0.7), rng)
    env = env_pts(n, SR, [(0, 0), (0.35, 1.0), (0.85, 0.9), (1.3, 0.0)]) ** 1.5
    out = (out + 0.12 * norm_peak(swell)) * env
    return add_reverb(lp(out, 7000, SR), SR, rng, 1.2, 0.3, damp=0.5, pre=0.02)


@sfx("cast_flare")
def _(rng):
    d = 0.8
    n = n_of(d, SR)
    t = tvec(n, SR)
    fenv = env_pts(n, SR, [(0, 0), (0.03, 1), (0.5, 0.8), (0.78, 0)])
    fizz = spectral_noise(n, SR, lambda tt, f: g_band(f, 4000, 0.8) * g_lowpass(f, 8500), rng) * flicker(n, SR, rng, 60, 0.7) * fenv
    crk = crackle(n, SR, rng, 60.0, 2000, 7000) * fenv
    pop = sat(norm_peak(burst(SR, d, rng, None, 3500, 0.025) + 0.8 * thump(SR, d, 160, 70, 0.015, 0.05)
                        + 0.3 * burst(SR, d, rng, 3000, None, 0.003)), 2.0)
    return pop + 0.6 * norm_peak(fizz) + 0.3 * norm_peak(crk)


@sfx("cast_bubble")
def _(rng):
    d = 0.5
    n = n_of(d, SR)
    t = tvec(n, SR)
    f = 180 * (520 / 180) ** np.clip(t / 0.18, 0, 1) ** 0.7
    blorp = (sine(f, n, SR) + 0.3 * sine(2 * f, n, SR)) * env_exp(n, SR, 0.01, 0.12)
    small = np.zeros(n)
    for t0 in grain_times(rng, 9, 0.02, 0.32, 1.2):
        place(small, bubble(SR, rng.uniform(300, 900), rise=rng.uniform(0.6, 1.4)) * rng.uniform(0.15, 0.4), int(t0 * SR))
    glug = norm_peak(reson(white(n, rng), 250, 4, SR) * env_exp(n, SR, 0.004, 0.05))
    return lp(norm_peak(blorp) + 0.4 * norm_peak(small) + 0.25 * glug, 3500, SR)


@sfx("cast_tar")
def _(rng):
    d = 0.35
    n = n_of(d, SR)
    wob = 1.0 + 0.6 * slow_noise(n, SR, rng, 50)
    spray = spectral_noise(n, SR, lambda t, f: g_band(f, 1800, 0.9), rng) * np.clip(wob, 0.15, None) * env_exp(n, SR, 0.01, 0.09)
    blips = np.zeros(n)
    for t0 in grain_times(rng, 5, 0.01, 0.15):
        place(blips, bubble(SR, rng.uniform(700, 1500), rise=0.8) * rng.uniform(0.4, 1.0), int(t0 * SR))
    gloop = mixdown(d, SR, (bubble(SR, 150, rise=1.2), 0.02, 1.0))
    return (0.6 * thump(SR, d, 200, 90, 0.008, 0.015) + 0.4 * burst(SR, d, rng, None, 2000, 0.01)
            + 0.8 * norm_peak(spray) + 0.25 * norm_peak(blips) + 0.4 * norm_peak(gloop))


@sfx("cast_sickle")
def _(rng):
    d = 0.52
    n = n_of(d, SR)
    t = tvec(n, SR)
    spin = 0.35 + 0.65 * np.abs(np.sin(np.pi * phase_of(14 - 4 * t / d, n, SR)))
    env = env_pts(n, SR, [(0, 0), (0.05, 1), (0.35, 0.8), (0.52, 0)])
    swish = spectral_noise(n, SR, lambda tt, f: g_band(f, 2500, 0.7), rng) * spin * env
    ring = modes(SR, d, [2350, 3410, 5050], [1, 0.6, 0.3], [0.5, 0.35, 0.2], beat=0.004, rng=rng) * spin * env
    chain = np.zeros(n)
    for t0 in grain_times(rng, 25, 0.0, 0.42, 1.4):
        place(chain, clink(SR, rng, 2000, 5000, 0.02) * rng.uniform(0.3, 1.0), int(t0 * SR))
    return 0.7 * norm_peak(swish) + 0.15 * norm_peak(ring) + 0.4 * norm_peak(chain)


@sfx("cast_harpoon")
def _(rng):
    d = 0.65
    n = n_of(d, SR)
    air = tv_filter(white(n, rng), env_pts(n, SR, [(0, 6000), (0.08, 1500), (0.65, 800)]), SR, q=0.7) * env_exp(n, SR, 0.002, 0.06)
    metal = modes(SR, d, [420, 1130, 1870], [1, 0.5, 0.25], [0.06, 0.05, 0.03], beat=0.004, rng=rng)
    chain = np.zeros(n)
    for t0 in grain_times(rng, 26, 0.05, 0.55, 1.5):
        place(chain, clink(SR, rng, 1600, 4200, 0.025) * rng.uniform(0.3, 1.0), int(t0 * SR))
    unspool = spectral_noise(n, SR, lambda t, f: g_band(f, 3000, 0.8), rng) * env_pts(n, SR, [(0, 0), (0.06, 0), (0.12, 1), (0.6, 0)])
    return sat(norm_peak(0.8 * norm_peak(air) + 0.9 * thump(SR, d, 180, 70, 0.012, 0.05) + 0.35 * norm_peak(metal)
                         + 0.3 * burst(SR, d, rng, 2500, None, 0.002) + 0.45 * norm_peak(chain) + 0.12 * norm_peak(unspool)), 2.2)


@sfx("cast_anchor")
def _(rng):
    d = 0.75
    n = n_of(d, SR)
    t = tvec(n, SR)
    env = np.where(t < 0.55, (t / 0.55) ** 2, np.cos(0.5 * np.pi * np.clip((t - 0.55) / 0.2, 0, 1)) ** 2)
    wh = spectral_noise(n, SR, lambda tt, f: g_band(f, np.interp(tt, [0, 0.7], [1200, 250]), 0.8), rng) * env
    groan = np.zeros(n)
    for f in (110, 173, 287):
        groan += sine(f * (1.0 - 0.1 * t / d), n, SR)
    chain = np.zeros(n)
    for t0 in grain_times(rng, 6, 0.0, 0.2):
        place(chain, clink(SR, rng, 1500, 3500, 0.03) * rng.uniform(0.3, 0.8), int(t0 * SR))
    return norm_peak(wh) + 0.3 * norm_peak(groan * env) + 0.2 * norm_peak(chain)


@sfx("cast_storm")
def _(rng):
    d = 0.62
    n = n_of(d, SR)
    t = tvec(n, SR)
    gate = np.zeros(n)
    for t0, ln in zip((0.0, 0.07, 0.16, 0.24, 0.36, 0.45), rng.uniform(0.03, 0.07, 6)):
        gate = np.maximum(gate, env_pts(n, SR, [(0, 0), (t0, 0), (t0 + 0.002, 1), (t0 + ln, 1), (t0 + ln + 0.004, 0), (d, 0)]))
    buzz = lp(hp(polyblep_saw(rng.uniform(70, 140) * (1 + 0.05 * slow_noise(n, SR, rng, 30)), n, SR), 250, SR), 5000, SR) * gate
    crk = crackle(n, SR, rng, 2500 * gate + 300 * np.exp(-t / 0.3), 1500, 7500)
    hiss = spectral_noise(n, SR, lambda tt, f: g_highpass(f, 3000) * g_lowpass(f, 8500), rng) * (0.3 + gate) * env_exp(n, SR, 0.005, 0.3)
    out = (0.5 * norm_peak(buzz) + 0.8 * norm_peak(crk) + 0.25 * norm_peak(hiss)
           + 0.35 * thump(SR, d, 120, 60, 0.02, 0.05))
    return sat(norm_peak(out), 2.2)


@sfx("fire")
def _(rng):
    d = 0.42
    n = n_of(d, SR)
    t = tvec(n, SR)
    b = np.sin(np.pi * np.clip(t / d, 0, 1)) ** 1.5
    roar = bp(colored(n, rng, 1.5), 120, 800, SR) * flicker(n, SR, rng) * b
    crk = crackle(n, SR, rng, 70.0, 2000, 6000) * b
    return (norm_peak(whoosh(rng, d, 500, 1800, 0.8, peak=0.4, air=0.3)) + 0.5 * norm_peak(roar)
            + 0.25 * norm_peak(crk))


@sfx("ignite")
def _(rng):
    d = 0.85
    n = n_of(d, SR)
    t = tvec(n, SR)
    wh = tv_filter(white(n, rng), env_pts(n, SR, [(0, 150), (0.15, 2600), (0.85, 1100)]), SR, q=0.9) * env_exp(n, SR, 0.04, 0.28)
    flame = bp(colored(n, rng, 1.2), 200, 2200, SR) * flicker(n, SR, rng) * env_exp(n, SR, 0.08, 0.4)
    crk = crackle(n, SR, rng, 50 + 60 * np.exp(-t / 0.4), 1800, 6500) * env_exp(n, SR, 0.05, 0.4)
    return (norm_peak(wh) + 0.5 * thump(SR, d, 75, 45, 0.05, 0.16, attack=0.02) + 0.4 * norm_peak(flame)
            + 0.25 * norm_peak(crk))


@sfx("steam")
def _(rng):
    d = 0.8
    n = n_of(d, SR)
    hiss = spectral_noise(n, SR, lambda t, f: g_band(f, 4200, 1.0) * g_lowpass(f, 9000, 3) + 0.15 * g_band(f, 1500, 1.2), rng)
    env = env_pts(n, SR, [(0, 0), (0.006, 1), (0.06, 0.75), (0.4, 0.6), (0.8, 0)]) ** 1.5
    whistle = sine(2900 + 40 * slow_noise(n, SR, rng, 8), n, SR) * env
    return (norm_peak(hiss * env) + 0.5 * burst(SR, d, rng, 900, None, 0.03) + 0.05 * whistle
            + 0.3 * burst(SR, d, rng, None, 300, 0.08, attack=0.002))


@sfx("chain")
def _(rng):
    d = 0.5
    n = n_of(d, SR)
    out = np.zeros(n)
    for c0, cnt, g in ((0.0, 10, 1.0), (0.12, 8, 0.8), (0.22, 6, 0.6), (0.33, 4, 0.45)):
        for t0 in c0 + rng.uniform(0, 0.04, cnt):
            place(out, clink(SR, rng, 1800, 5200, 0.025) * g * rng.uniform(0.4, 1.0), int(t0 * SR))
    return sat(norm_peak(norm_peak(out) + 0.3 * norm_peak(modes(SR, d, [520, 1380], [1, 0.5], [0.08, 0.05]))), 2.5)


@sfx("scream")
def _(rng):
    d = 1.5
    n = n_of(d, SR)
    contour = env_pts(n, SR, [(0, 620), (0.22, 880), (0.95, 860), (1.35, 700), (1.5, 680)])
    contour = uniform_filter1d(contour, n_of(0.05, SR), mode="nearest")
    cents = vibrato(n, SR, rng, 20, 5.6, onset=0.3, drift=7.0) + 6.0 * slow_noise(n, SR, rng, 25)
    f0 = contour * 2 ** (cents / 1200)
    morph = env_pts(n, SR, [(0, 0), (0.35, 0.6), (0.9, 1.3), (1.5, 2.0)])
    v1 = voice(f0, SR, rng, "soprano", ("u", "a", "o"), morph, tilt=1.1, max_f=6500, bw=1.3)
    v2 = voice(f0 * 2 ** (-95 / 1200), SR, rng, "soprano", ("u", "a", "o"), morph, tilt=1.1, max_f=6500, bw=1.3)
    v3 = voice(f0 / 2, SR, rng, "alto", ("u", "o"), tilt=1.0, max_f=4000, bw=1.5)
    br = formant_noise(n, SR, rng, "soprano", "a")
    env = env_pts(n, SR, [(0, 0), (0.14, 0.85), (0.3, 1.0), (1.1, 0.9), (1.5, 0.0)])
    shimmer = 1.0 + 0.08 * slow_noise(n, SR, rng, 12)
    out = (norm_peak(v1) + 0.6 * norm_peak(v2) + 0.25 * norm_peak(v3) + 0.12 * norm_peak(br)) * env * shimmer
    return add_reverb(lp(out, 6500, SR, 3), SR, rng, 1.6, 0.3, damp=0.6, pre=0.03)


@sfx("splash")
def _(rng):
    d = 1.0
    n = n_of(d, SR)
    body = spectral_noise(n, SR, lambda t, f: g_band(f, np.interp(t, [0, 0.5], [1600, 700]), 1.1), rng) * env_exp(n, SR, 0.01, 0.22)
    drops = np.zeros(n)
    for t0 in grain_times(rng, 45, 0.04, 0.8, 1.5):
        f0 = np.exp(rng.uniform(np.log(500), np.log(2600)))
        place(drops, bubble(SR, f0, rise=rng.uniform(0.4, 1.2)) * rng.uniform(0.2, 1.0) * np.exp(-t0 / 0.35), int(t0 * SR))
    spray = spectral_noise(n, SR, lambda t, f: g_highpass(f, 3000) * g_lowpass(f, 9000), rng) * env_exp(n, SR, 0.01, 0.25)
    out = (0.9 * burst(SR, d, rng, None, 3000, 0.05, attack=0.002) + 0.6 * thump(SR, d, 100, 50, 0.03, 0.08)
           + 0.8 * norm_peak(body) + 0.35 * norm_peak(drops) + 0.18 * norm_peak(spray))
    return sat(norm_peak(out), 1.6)


@sfx("sink")
def _(rng):
    d = 1.2
    n = n_of(d, SR)
    glugs = np.zeros(n)
    for t0 in grain_times(rng, 14, 0.0, 0.95, 1.2):
        f0 = np.exp(rng.uniform(np.log(110), np.log(380)))
        place(glugs, bubble(SR, f0, rise=rng.uniform(0.8, 1.6)) * rng.uniform(0.4, 1.0) * (1 - 0.6 * t0 / d), int(t0 * SR))
    fade_env = env_pts(n, SR, [(0, 0), (0.06, 1), (1.2, 0)])
    under = sine(glide(n, SR, 170, 85, 0.5), n, SR) * fade_env
    slosh = spectral_noise(n, SR, lambda t, f: g_band(f, 600, 0.8), rng) * (0.5 + 0.5 * np.abs(slow_noise(n, SR, rng, 6))) * fade_env
    return lp(norm_peak(glugs) + 0.35 * norm_peak(under) + 0.3 * norm_peak(slosh), 2600, SR)


@sfx("stamp")
def _(rng):
    d = 0.95
    n = n_of(d, SR)
    boom = thump(SR, d, 85, 32, 0.06, 0.36) + 0.5 * thump(SR, d, 60, 45, 0.1, 0.5, attack=0.01)
    out = (norm_peak(boom) + 0.8 * burst(SR, d, rng, 220, 2400, 0.028) + 0.45 * norm_peak(thump(SR, d, 300, 180, 0.01, 0.05))
           + 0.35 * norm_peak(debris(SR, rng, d, 6, 0.0, 0.06, 1000, 4000, 1.0, 1.0))
           + 0.5 * norm_peak(lp(colored(n, rng, 2.0), 200, SR) * env_exp(n, SR, 0.01, 0.45))
           + 0.2 * norm_peak(debris(SR, rng, d, 30, 0.1, 0.7)))
    return add_reverb(sat(norm_peak(out), 3.0), SR, rng, 0.7, 0.12, damp=0.7)


@sfx("crumble")
def _(rng):
    d = 1.2
    n = n_of(d, SR)
    dens = env_pts(n, SR, [(0, 0), (0.04, 1), (0.35, 0.6), (1.1, 0)])
    grains = np.zeros(n)
    for t0 in env_times(rng, 380, dens, SR):
        gn = n_of(rng.uniform(0.001, 0.006), SR)
        fc = np.exp(rng.uniform(np.log(700), np.log(6000)))
        g = bp(white(gn + 64, rng), fc / 1.4, fc * 1.4, SR)[:gn] * np.hanning(gn)
        place(grains, g * rng.uniform(0.1, 1.0) ** 2, int(t0 * SR))
    chunks = np.zeros(n)
    for t0 in env_times(rng, 16, dens, SR):
        fr = rng.uniform(250, 900)
        c = modes(SR, 0.12, [fr, fr * 1.7, fr * 2.6], [1, 0.5, 0.3], rng.uniform(0.02, 0.05, 3)) + 0.5 * burst(SR, 0.12, rng, None, 2000, 0.01)
        place(chunks, c * rng.uniform(0.4, 1.0), int(t0 * SR))
    sift = spectral_noise(n, SR, lambda t, f: g_band(f, 3800, 0.8), rng) * uniform_filter1d(dens, n_of(0.1, SR))
    rumble = lp(colored(n, rng, 2.0), 220, SR) * dens
    return norm_peak(grains) + 0.5 * norm_peak(chunks) + 0.35 * norm_peak(rumble) + 0.15 * norm_peak(sift)


@sfx("far_whoosh")
def _(rng):
    d = 1.85
    n = n_of(d, SR)
    t = tvec(n, SR)
    amp = np.where(t < 0.06, np.sin(0.5 * np.pi * t / 0.06) ** 2, np.exp(-(t - 0.06) / 0.3)) * (t < 1.3)
    amp = uniform_filter1d(amp, n_of(0.01, SR))
    wh = spectral_noise(n, SR, lambda tt, f: g_band(f, np.interp(tt, [0, 0.08, 1.25], [900, 2200, 450]), 0.75)
                        * g_lowpass(f, np.interp(tt, [0, 1.25], [9000, 1800])), rng)
    tone = sine(glide(n, SR, 1100, 480, 0.4), n, SR)
    ting = np.zeros(n)
    for t0, f, g in ((1.32, 2217.0, 1.0), (1.4, 2960.0, 0.6)):
        place(ting, g * modes(SR, 0.45, [f, f * 1.5, f * 2.76], [1, 0.3, 0.12], [0.18, 0.1, 0.05], attack=0.001), int(t0 * SR))
    ting *= 1.0 + 0.3 * np.sin(TAU * 28 * t)
    return norm_peak(wh * amp) + 0.1 * tone * amp + 0.3 * norm_peak(ting)


@sfx("gulp")
def _(rng):
    d = 0.95
    n = n_of(d, SR)
    suck = spectral_noise(n, SR, lambda t, f: g_band(f, np.interp(t, [0, 0.32], [2800, 700]), 0.6), rng)
    suck *= env_pts(n, SR, [(0, 0), (0.28, 1), (0.33, 0), (d, 0)]) ** 2
    dry = mixdown(d, SR, (norm_peak(suck), 0, 0.6), (burst(SR, 0.05, rng, None, 1500, 0.004), 0.31, 0.4),
                  (bubble(SR, 140, rise=1.2), 0.32, 0.9), (thump(SR, 0.3, 160, 80, 0.03, 0.08), 0.32, 1.0),
                  (bubble(SR, 110, rise=1.0), 0.58, 0.6), (thump(SR, 0.3, 130, 70, 0.03, 0.07), 0.58, 0.6))
    helmet = comb(dry, 0.0041, 0.6, SR)
    res = reson(dry, 640, 9, SR) + reson(dry, 1370, 10, SR) + 0.7 * reson(dry, 2230, 12, SR)
    out = 0.5 * norm_peak(dry) + 0.6 * norm_peak(helmet) + 0.4 * norm_peak(res)
    return add_reverb(out, SR, rng, 0.25, 0.2, damp=0.2, pre=0.003)


@sfx("bell")
def _(rng):
    x = bell(98.0, SR, rng, decay=3.4, dur=4.2, iron=0.6, bright=0.8, strike=0.35)
    x = mixdown(4.2, SR, (x, 0, 1.0), (thump(SR, 0.2, 400, 250, 0.01, 0.02), 0, 0.3))
    return add_reverb(lp(x, 8000, SR), SR, rng, 2.5, 0.25, damp=0.5, pre=0.02)


@sfx("vat")
def _(rng):
    d = 3.0
    n = n_of(d, SR)
    churn = lp(colored(n, rng, 2.0), 350, SR) * (1.0 + 0.5 * slow_noise(n, SR, rng, 3))
    bub = np.zeros(n)
    for t0 in np.sort(rng.uniform(0, d - 0.2, 130)):
        f0 = np.exp(rng.uniform(np.log(90), np.log(650)))
        place(bub, bubble(SR, f0, rise=rng.uniform(0.6, 1.6)) * (90 / f0) ** 0.5 * rng.uniform(0.4, 1.0), int(t0 * SR))
    slosh = spectral_noise(n, SR, lambda t, f: g_band(f, 700, 0.9), rng) * (0.5 + 0.5 * np.abs(slow_noise(n, SR, rng, 2)))
    out = lp(0.5 * norm_peak(churn) + 0.9 * norm_peak(bub) + 0.25 * norm_peak(slosh), 4500, SR)
    return fade(out, SR, 0.2, 0.5)


@sfx("screech")
def _(rng):
    d = 1.7
    n = n_of(d, SR)
    t = tvec(n, SR)
    contour = uniform_filter1d(env_pts(n, SR, [(0, 300), (0.18, 560), (0.55, 520), (1.0, 470), (1.55, 280), (1.7, 260)]),
                               n_of(0.04, SR), mode="nearest")
    f0 = contour * 2 ** ((vibrato(n, SR, rng, 45, 7.5, onset=0.1) + 30 * slow_noise(n, SR, rng, 30)) / 1200)
    morph = env_pts(n, SR, [(0, 0), (0.5, 1), (1.7, 2)])
    v1 = voice(f0, SR, rng, "alto", ("i", "a", "o"), morph, tilt=0.7, max_f=7000, bw=2.2, scale=0.85)
    v1 *= 1.0 + 0.7 * np.sin(np.pi * phase_of(f0, n, SR))     # period-doubling: rough subharmonic growl
    v2 = voice(f0 * 2 ** (38 / 1200), SR, rng, "tenor", ("a", "o"), tilt=0.8, max_f=6000, bw=2.0)
    low = voice(f0 / 4, SR, rng, "bass", ("a", "o"), tilt=0.8, max_f=3000, bw=1.8)
    hiss = spectral_noise(n, SR, lambda tt, f: formant_gain(f[:, 0], *vowel_curve("alto", ["a"], [0.0], 0.85, 2.5))[:, None]
                          * np.ones_like(tt), rng, 1024)
    chirps = np.zeros(n)
    for t0 in (0.0, 0.045, 0.09):
        m = n_of(0.03, SR)
        place(chirps, sine(np.linspace(3000, 1500, m), m, SR) * np.hanning(m), int(t0 * SR))
    env = env_pts(n, SR, [(0, 0), (0.08, 0.9), (0.25, 1.0), (1.2, 0.85), (1.7, 0.0)])
    out = sat(norm_peak(norm_peak(v1) + 0.5 * norm_peak(v2) + 0.45 * norm_peak(hiss) + 0.4 * norm_peak(low)), 2.0) * env
    out = lp(out + 0.15 * chirps, 7000, SR, 3)
    return add_reverb(out, SR, rng, 1.6, 0.28, damp=0.5, pre=0.03, size=1.6)


@sfx("wing")
def _(rng):
    d = 0.6
    n = n_of(d, SR)
    whump = tv_filter(white(n, rng), env_pts(n, SR, [(0, 250), (0.07, 1300), (0.3, 300)]), SR, q=0.7)
    whump *= env_pts(n, SR, [(0, 0), (0.06, 1), (0.12, 0.8), (0.45, 0), (d, 0)]) ** 1.5
    ripples = np.zeros(n)
    for t0, g in ((0.09, 0.6), (0.125, 0.45), (0.16, 0.3), (0.19, 0.2)):
        m = n_of(0.025, SR)
        place(ripples, g * bp(white(m, rng), 300, 1500, SR) * np.hanning(m), int(t0 * SR))
    return (norm_peak(whump) + 0.45 * mixdown(d, SR, (burst(SR, 0.1, rng, 400, 2200, 0.018), 0.065, 1.0))
            + 0.35 * norm_peak(ripples) + 0.45 * thump(SR, d, 70, 45, 0.04, 0.09, attack=0.02))


@sfx("thunder")
def _(rng):
    d = 4.0
    n = n_of(d, SR)
    t = tvec(n, SR)
    crack = (burst(SR, d, rng, 600, None, 0.05)
             + 1.5 * norm_peak(crackle(n, SR, rng, 3000 * np.exp(-t / 0.06) + 400 * np.exp(-t / 0.2), 700, 7000)))
    boom = norm_peak(thump(SR, d, 70, 32, 0.1, 0.35, attack=0.005)) + 0.7 * burst(SR, d, rng, None, 500, 0.35, attack=0.004)
    rolls = 0.4 + 0.6 * np.abs(slow_noise(n, SR, rng, 5)) ** 1.2
    rumble = lp(colored(n, rng, 2.0), 170, SR) * rolls * env_pts(n, SR, [(0, 0), (0.06, 1), (0.5, 0.6), (0.9, 0.75), (1.5, 0.45), (4.0, 0)])
    mid = bp(colored(n, rng, 1.0), 150, 700, SR) * env_exp(n, SR, 0.02, 0.4)
    out = 0.9 * norm_peak(crack) + 0.8 * norm_peak(boom) + 0.8 * norm_peak(rumble) + 0.3 * norm_peak(mid)
    return fade(sat(norm_peak(out), 2.0), SR, 0, 0.5)


@sfx("charge")
def _(rng):
    d = 0.85
    n = n_of(d, SR)
    t = tvec(n, SR)
    u = np.clip(t / 0.78, 0, 1)
    f = 70 * 4.5 ** (u ** 1.2)
    saw = polyblep_saw(f, n, SR)
    buzz = tv_filter(saw, 6 * f, SR, q=1.5, kind="bp") + 0.3 * lp(saw, 1500, SR)
    whine = sine(f * 8, n, SR)
    crk = crackle(n, SR, rng, 60 + 1400 * u ** 2, 1800, 7000)
    pulse = 0.55 + 0.45 * np.sin(TAU * phase_of(7 + 26 * u ** 1.5, n, SR))
    release = np.cos(0.5 * np.pi * np.clip((t - 0.78) / 0.05, 0, 1)) ** 2
    out = (0.6 * norm_peak(buzz) + 0.22 * whine + 0.45 * norm_peak(crk)) * pulse * (0.15 + 0.85 * u ** 1.6) * release
    return sat(norm_peak(out), 1.3)


@sfx("palm_strike")
def _(rng):
    d = 1.5
    n = n_of(d, SR)
    nz = white(n, rng)
    slap = norm_peak((0.6 * bp(nz, 700, 3500, SR) + 0.4 * reson(nz, 1500, 2, SR)) * env_exp(n, SR, 0.0004, 0.01))
    body = modes(SR, d, [73.4, 146.8 * 1.002, 202.6, 293.7 * 1.01, 396.0], [0.9, 0.4, 0.3, 0.15, 0.08],
                 [1.1, 0.8, 0.55, 0.35, 0.25], beat=0.002, rng=rng, attack=0.002)
    air = spectral_noise(n, SR, lambda t, f: g_lowpass(f, 800), rng) * env_exp(n, SR, 0.005, 0.12)
    out = 0.8 * slap + thump(SR, d, 170, 72, 0.02, 0.07) + 0.75 * norm_peak(body) + 0.2 * norm_peak(air)
    return add_reverb(sat(norm_peak(out), 1.6), SR, rng, 1.3, 0.2, damp=0.5)


@sfx("gong")
def _(rng):
    return gong(SR, rng, f0=64.0, dur=4.4, decay=3.2)


@sfx("fight")
def _(rng):
    d = 1.6
    br = sum(brass(hz(nm), 0.12, SR, rng, vel=1.0, att=0.02, rel=0.35) for nm in ("D2", "A2", "D3"))
    x = mixdown(d + 0.6, SR, (drum(SR, rng, 150, 62, 0.04, 0.32), 0, 1.0), (snare(SR, rng, tone=200, decay=0.12), 0, 0.55),
                (cymbal(SR, rng, dur=1.5, decay=0.6), 0, 0.45), (norm_peak(br), 0, 0.45))
    return add_reverb(x, SR, rng, 1.3, 0.2, damp=0.5)


@sfx("finish")
def _(rng):
    """Low timpani roll crescendo into a hit, under a dissonant (b9) low brass swell."""
    d = 2.6
    roll = np.zeros(n_of(d, SR))
    tt = 0.0
    while tt < 0.85:
        v = 0.15 + 0.7 * (tt / 0.85) ** 1.5
        place(roll, timpani(hz("D2"), SR, rng, vel=v * rng.uniform(0.85, 1.0), decay=0.8), int(tt * SR))
        tt += rng.uniform(0.042, 0.052)
    hit = mixdown(1.7, SR, (timpani(hz("D2"), SR, rng, vel=1.0, decay=1.2), 0, 1.0), (drum(SR, rng, 95, 48, 0.06, 0.5), 0, 0.7))
    br = sum(g * brass(hz(nm), 0.95, SR, rng, vel=0.9, rel=0.55, swell=True)
             for nm, g in (("D2", 1.0), ("A2", 0.7), ("D3", 0.6), ("Eb3", 0.55)))
    x = mixdown(d, SR, (norm_peak(roll), 0, 0.7), (norm_peak(hit), 0.9, 1.0), (norm_peak(br), 0.0, 0.75),
                (cymbal(SR, rng, dur=1.0, decay=0.45, bright=0.7), 0.9, 0.2))
    return fade(add_reverb(x, SR, rng, 1.5, 0.26, damp=0.5), SR, 0, 0.6)


@sfx("sloptality")
def _(rng):
    """Da-da-DUM drums with C minor and A-flat brass stabs, ending on an open C with gong."""
    d = 2.9
    layers = []
    for t0, v in ((0.0, 0.6), (0.18, 0.7), (0.4, 1.0)):
        layers.append((drum(SR, rng, 120, 55, 0.05, 0.4), t0, v))
        layers.append((timpani(hz("C2"), SR, rng, decay=1.0), t0, 0.5 * v))

    def chord(names, dur, vel):
        return norm_peak(sum(brass(hz(nm), dur, SR, rng, vel=vel, att=0.03, rel=0.4) for nm in names.split()))

    layers += [(chord("C2 G2 C3 Eb3 G3", 0.36, 0.9), 0.4, 0.6), (chord("Ab1 Eb2 Ab2 C3 Eb3", 0.36, 0.9), 0.85, 0.6),
               (chord("C2 G2 C3 G3 C4", 0.75, 1.0), 1.3, 0.8), (drum(SR, rng, 100, 46, 0.06, 0.6), 1.3, 1.0),
               (gong(SR, rng, f0=55.0, dur=1.5, decay=1.3), 1.3, 0.35), (cymbal(SR, rng, dur=1.3, decay=0.55), 1.3, 0.3),
               (bell(hz("C3"), SR, rng, decay=1.3, dur=1.5, iron=0.4), 1.3, 0.25)]
    return fade(add_reverb(mixdown(d, SR, *layers), SR, rng, 1.6, 0.28, damp=0.5), SR, 0, 0.7)


@sfx("victory")
def _(rng):
    d = 2.0
    chord = norm_peak(sum(brass(hz(nm), 0.8, SR, rng, vel=0.9, att=0.03, rel=0.6, vib=8) for nm in "D3 F#3 A3 D4".split()))
    x = mixdown(d, SR, (brass(hz("A3"), 0.08, SR, rng, vel=0.8, att=0.015, rel=0.06), 0.0, 0.5),
                (brass(hz("A3"), 0.08, SR, rng, vel=0.85, att=0.015, rel=0.06), 0.11, 0.55),
                (drum(SR, rng, 140, 70, 0.03, 0.15), 0.0, 0.45), (drum(SR, rng, 140, 70, 0.03, 0.15), 0.11, 0.5),
                (chord, 0.22, 0.75), (drum(SR, rng, 130, 58, 0.04, 0.35), 0.22, 1.0),
                (snare(SR, rng), 0.22, 0.5), (cymbal(SR, rng, dur=1.4, decay=0.55), 0.22, 0.3))
    return add_reverb(x, SR, rng, 1.4, 0.22, damp=0.5)


@sfx("ui_move")
def _(rng):
    return (modes(SR, 0.07, [1500, 2650, 4100], [1, 0.4, 0.15], [0.01, 0.006, 0.004])
            + 0.3 * burst(SR, 0.07, rng, None, 6000, 0.002))


@sfx("ui_select")
def _(rng):
    knock = modes(SR, 0.2, [700, 1520, 2350], [1, 0.5, 0.25], [0.03, 0.018, 0.01]) + 0.5 * thump(SR, 0.2, 220, 160, 0.01, 0.03)
    chime = modes(SR, 0.55, [784, 1175, 784 * 2.76], [1, 0.6, 0.2], [0.35, 0.25, 0.08], beat=0.003, rng=rng, attack=0.002)
    return mixdown(0.56, SR, (norm_peak(knock), 0, 1.0), (norm_peak(chime), 0.012, 0.45),
                   (burst(SR, 0.05, rng, 1500, 6000, 0.002), 0, 0.2))


@sfx("ui_back")
def _(rng):
    d = 0.16
    n = n_of(d, SR)
    return (norm_peak(modes(SR, d, [360, 820, 1300], [1, 0.4, 0.15], [0.035, 0.02, 0.01])) + 0.5 * thump(SR, d, 180, 120, 0.01, 0.03)
            + 0.25 * norm_peak(lp(white(n, rng), 1200, SR) * env_exp(n, SR, 0.0005, 0.01)))


@sfx("ui_locked")
def _(rng):
    d = 0.24
    n = n_of(d, SR)
    f = glide(n, SR, 104, 96, 0.2)
    buzz = polyblep_saw(f, n, SR) + polyblep_saw(f * 1.056, n, SR)
    return lp(buzz, 900, SR) * env_pts(n, SR, [(0, 0), (0.01, 1), (0.18, 0.8), (d, 0)])


@sfx("select_fighter")
def _(rng):
    d = 1.0
    n = n_of(d, SR)
    t = tvec(n, SR)
    shimmer = modes(SR, d, [659, 659 * 2.76, 659 * 5.4, 659 * 1.5 * 1.003], [1, 0.4, 0.15, 0.3], [0.7, 0.4, 0.2, 0.5], beat=0.004, rng=rng)
    shimmer *= 1.0 + 0.25 * np.sin(TAU * 7 * t)
    swish = spectral_noise(n, SR, lambda tt, f: g_band(f, 6000, 0.6), rng) * env_exp(n, SR, 0.005, 0.35)
    return mixdown(d, SR, (drum(SR, rng, 120, 62, 0.03, 0.16), 0, 1.0),
                   (norm_peak(modes(SR, d, [540, 1210, 1990], [1, 0.5, 0.2], [0.04, 0.025, 0.012])), 0, 0.5),
                   (norm_peak(shimmer), 0, 0.4), (norm_peak(swish), 0, 0.12))


# --- Music ---------------------------------------------------------------------------------------

MUSIC = {}


def music(name):
    def deco(fn):
        MUSIC[name] = fn
        return fn
    return deco


class Song:
    """A circular timeline of named buses at a fixed tempo; events wrap past the end."""

    def __init__(self, sr, bpm, bars, beats=4):
        self.sr, self.beats = sr, beats
        self.beat_n = int(round(sr * 60.0 / bpm))
        self.spb = self.beat_n / sr
        self.n = self.beat_n * beats * bars
        self.length = self.n / sr
        self.buses = {}

    def t(self, bar, beat=0.0):
        return (bar * self.beats + beat) * self.spb

    def bus(self, name):
        return self.buses.setdefault(name, np.zeros(self.n))

    def add(self, name, sig, t, gain=1.0):
        buf = self.bus(name)
        i = int(round(t * self.sr)) % self.n
        sig = sig * gain
        while len(sig):
            k = min(len(sig), self.n - i)
            buf[i:i + k] += sig[:k]
            sig, i = sig[k:], 0

    def render(self, mix, ir):
        """mix: {bus: (gain, reverb send)}; one shared circular convolution reverb."""
        dry, send = np.zeros(self.n), np.zeros(self.n)
        for name, buf in self.buses.items():
            g, r = mix[name]
            dry += g * buf
            send += g * r * buf
            if os.environ.get("MIXDEBUG"):
                print(f"    {name:8s} rms {db(rms(g * buf)):6.1f}  peak {db(np.max(np.abs(g * buf))):6.1f}", file=sys.stderr)
        return dry + circ_conv(send, ir)


def master(x, drive):
    """Light bus saturation at a fixed operating level (RMS -16 dBFS) so drive means the same
    thing for every track."""
    return sat(x * (0.16 / rms(x)), drive)


def kit(rng, **makers):
    """Pre-render a few variants of each percussion sound; returns a picker."""
    bank = {k: [mk() for _ in range(cnt)] for k, (mk, cnt) in makers.items()}
    return lambda name: bank[name][rng.integers(len(bank[name]))]


def periodic_saws(notes, length, sr, rng, detune=(-6.0, 0.0, 7.0)):
    """Drone of detuned saws whose frequencies complete whole cycles over the loop."""
    n = n_of(length, sr)
    x = np.zeros(n)
    for nm, g in notes:
        for d in detune:
            x += g * polyblep_saw(loop_f(hz(nm) * 2 ** (d / 1200), length), n, sr, rng.random())
    return x


@music("music_menu")
def _(rng):
    """D minor, 60 BPM, 16 bars: drone, slow pad chords, a lonely viola line, a distant bell."""
    sr = MUSIC_SR
    s = Song(sr, 60, 16)
    sp, n, L = s.spb, s.n, s.length
    t = tvec(n, sr)
    dr = periodic_saws([("D2", 1.0), ("D3", 0.3), ("A2", 0.25)], L, sr, rng)
    lfo = 0.5 + 0.5 * np.sin(TAU * loop_f(1 / 16, L) * t)
    dark = circ(lambda v: lp(v, 190, sr, 2), dr, sr)
    bright = circ(lambda v: lp(v, 520, sr, 2), dr, sr)
    s.buses["drone"] = (dark * (1 - lfo) + 0.6 * bright * lfo) * (0.8 + 0.2 * np.sin(TAU * loop_f(1 / 9, L) * t))
    events = []
    for bar, chord in ((0, "D3 A3 D4 F4"), (2, "Bb2 F3 Bb3 D4"), (4, "G2 D3 G3 Bb3"), (6, "A2 E3 A3 C#4"),
                       (8, "D3 A3 D4 F4"), (10, "Bb2 F3 A3 D4"), (12, "G2 D3 Bb3 E4"), (14, "A2 E3 A3")):
        events += [(bar, 2, nm) for nm in chord.split()]
    events += [(14, 1, "D4"), (15, 1, "C#4")]
    for bar, ln, nm in events:
        f = hz(nm)
        s.add("pad", pad(f, ln * 4 * sp + 0.6, sr, rng, att=1.6, rel=2.6, cutoff=600 + 1.5 * f, voices=3, detune=8), s.t(bar) - 0.5, 0.25)
    mel = melody("D4:1.5 F4:.5 Bb4 A4 | F4:3 r | G4:1.5 Bb4:.5 A4 G4 | D4:3 r | E4 F4 G4 E4 | C#4:3 r | "
                 "A4:1.5 D5:.5 C5 A4 | F4:1.5 G4:.5 A4:2 | Bb4:1.5 A4:.5 G4 F4 | D4:2 F4 G4 | "
                 "A4 G4 E4 G4 | F4:1.5 E4:.5 D4:2 | C#4 D4 E4:2 | A3:3 r", start=8)
    for beat, f, d in mel:
        dur = d * sp
        vel = 0.62 + (0.16 if 32 <= beat < 56 else 0.0) + 0.05 * min(d, 2)
        s.add("lead", bowed(f, dur * 1.03, sr, rng, vel=vel, att=min(0.28, 0.45 * dur), rel=0.55, vib=14, swell=0.35),
              beat * sp + rng.normal(0, 0.006))
    for bar, g in ((0, 1.0), (8, 0.7)):
        s.add("bell", lp(bell(hz("D3"), sr, rng, decay=5.0, dur=7.5, bright=0.6), 2500, sr), s.t(bar), g)
    for bar, beat, g in ((0, 0, 0.9), (4, 0, 0.55), (8, 0, 0.8), (12, 0, 0.55), (7, 3.5, 0.3), (15, 3.5, 0.35)):
        s.add("drum", lp(drum(sr, rng, 90, 50, 0.06, 0.6, skin=0.2, click=0.0), 500, sr), s.t(bar, beat), g)
    w = fft_shape(colored(n, rng, 1.0), sr, lambda f: g_band(f, 600, 1.1))
    s.buses["wind"] = w * (0.6 + 0.4 * np.sin(TAU * loop_f(0.07, L) * t + 1.0)) * (0.7 + 0.3 * np.sin(TAU * loop_f(0.023, L) * t))
    ir = make_ir(sr, 3.6, rng, pre=0.03, damp=0.6, size=1.6)
    mix = {"drone": (0.17, 0.25), "pad": (0.85, 0.5), "lead": (0.62, 0.45), "bell": (0.8, 1.4), "drum": (0.85, 0.9), "wind": (0.13, 0.2)}
    return master(s.render(mix, ir), 1.2)


@music("music_select")
def _(rng):
    """E minor, 100 BPM, 24 bars: toms, pulsing bass, pad; a plucked motif, brass in the last third."""
    sr = MUSIC_SR
    s = Song(sr, 100, 24)
    sp = s.spb
    st = sp / 4
    pick = kit(rng,
               kick=(lambda: drum(sr, rng, 150, 50, 0.03, 0.24, skin=0.3, click=0.25), 4),
               tom=(lambda: drum(sr, rng, 200, 120, 0.03, 0.22, skin=0.35), 3),
               tomlo=(lambda: drum(sr, rng, 150, 90, 0.03, 0.28, skin=0.35), 3),
               snare=(lambda: snare(sr, rng, tone=185, decay=0.13), 4),
               rim=(lambda: norm_peak(modes(sr, 0.1, [900, 1650], [1, 0.5], [0.02, 0.012]) + 0.5 * burst(sr, 0.1, rng, 2000, 6000, 0.003)), 3),
               shaker=(lambda: shaker(sr, rng, dur=0.07), 6),
               frame=(lambda: drum(sr, rng, 260, 210, 0.01, 0.06, skin=0.6, skin_lp=3000, click=0.4), 4))
    cyc = [("E2", "E3 G3 B3"), ("E2", "E3 G3 B3"), ("C2", "E3 G3 C4"), ("D2", "F#3 A3 D4"),
           ("E2", "E3 G3 B3"), ("E2", "G3 B3 E4"), ("A1", "E3 A3 C4"), ("B1", "F#3 B3 D#4")]
    for bar in range(24):
        root, chord = cyc[bar % 8]
        sec, tb = bar // 8, s.t(bar)
        for nm in chord.split():
            s.add("pad", pad(hz(nm), 4 * sp, sr, rng, att=0.4, rel=0.9, cutoff=900 + 300 * sec, voices=2, detune=10), tb, 0.12 + 0.03 * sec)
        pat = [0, 0, 12, 0, 0, 7, 12, 7 if root == "B1" else 0]
        for i, iv in enumerate(pat):
            s.add("bass", bass(hz(root) * 2 ** (iv / 12), 0.42 * sp, sr, rng, vel=0.95 if i in (0, 3) else 0.75, kind="saw", cutoff=260),
                  tb + i * sp / 2)
        kicks = [0, 6, 8] + ([11] if bar % 2 else []) + ([3, 14] if sec == 2 else [])
        for k in kicks:
            s.add("kick", pick("kick"), tb + k * st, 1.0 if k in (0, 8) else 0.8)
        for k in (4, 12):
            s.add("snare" if sec else "rim", pick("snare" if sec else "rim"), tb + k * st, 0.9 if k == 12 else 0.8)
        if bar % 4 == 3 and bar != 23:
            s.add("tom", pick("tom"), tb + 14 * st, 0.7)
            s.add("tom", pick("tomlo"), tb + 15 * st, 0.8)
        for k in range(0, 16, 1 if sec == 2 else 2):
            s.add("shaker", pick("shaker"), tb + k * st + rng.normal(0, 0.003), (0.9 if k % 4 == 2 else 0.55) * (0.7 if k % 2 else 1))
        if sec >= 1:
            for k in range(16):
                if k not in kicks and k % 4:
                    s.add("frame", pick("frame"), tb + k * st, 0.25 + 0.15 * (k % 2 == 0))
        if bar == 23:
            for k, nm in zip(range(8, 16), ("tom", "tom", "tom", "tomlo", "tom", "tomlo", "tomlo", "tomlo")):
                s.add("tom", pick(nm), tb + k * st, 0.6 + 0.05 * (k - 8))
    for bar in (0, 16):
        s.add("crash", cymbal(sr, rng, dur=2.0, decay=0.8, bright=0.8), s.t(bar), 1.0)
    motif = ("E4:.5 G4:.5 B4 A4:.5 G4:.5 F#4 | G4:.5 F#4:.5 E4 B3:2 | E4:.5 G4:.5 C5 B4:.5 A4:.5 G4 | F#4:.5 G4:.5 A4 D4:2 | "
             "E4:.5 G4:.5 B4 A4:.5 G4:.5 F#4 | G4:.5 A4:.5 B4 E5:2 | C5:.5 B4:.5 A4 E4:.5 F#4:.5 G4 | F#4 D#4 B3:2")
    for start, inst in ((32, "pluck"), (64, "brass")):
        for beat, f, d in melody(motif, start):
            dur = d * sp
            if inst == "pluck":
                s.add("motif", pluck(f, sr, rng, dur=dur + 0.6, hold=dur * 0.95, vel=0.85, decay=0.9, bright=0.65, pos=0.2), beat * sp)
            else:
                s.add("brass", brass(f, dur * 0.92, sr, rng, vel=0.75, att=0.03, rel=0.12, bright=0.8), beat * sp)
                s.add("motif", pluck(2 * f, sr, rng, dur=dur + 0.5, hold=dur, vel=0.6, decay=0.7, bright=0.5, pos=0.2), beat * sp, 0.5)
    for bar in (16, 18, 20, 22):
        root, chord = cyc[bar % 8]
        for f in [hz(root) * (2 if root.endswith("1") else 1)] + [hz(nm) for nm in chord.split()[:2]]:
            s.add("brass", brass(f, 0.35, sr, rng, vel=0.85, att=0.025, rel=0.25), s.t(bar), 0.45)
    ir = make_ir(sr, 1.6, rng, pre=0.015, damp=0.5)
    mix = {"kick": (0.85, 0.08), "tom": (0.5, 0.2), "snare": (0.8, 0.25), "rim": (0.55, 0.2), "shaker": (0.18, 0.1),
           "frame": (0.4, 0.12), "crash": (0.32, 0.3), "bass": (0.5, 0.05), "pad": (1.0, 0.35), "motif": (0.6, 0.3),
           "brass": (0.45, 0.3)}
    return master(s.render(mix, ir), 1.3)


@music("music_yards")
def _(rng):
    """C Phrygian, 110 BPM, 32 bars: hammer kicks, anvils, clanks, steam, a pumping saw bass."""
    sr = MUSIC_SR
    s = Song(sr, 110, 32)
    sp = s.spb
    st = sp / 4
    L = s.length
    pick = kit(rng,
               kick=(lambda: norm_peak(drum(sr, rng, 135, 44, 0.03, 0.2, skin=0.25, click=0.35)
                                        + 0.22 * place(np.zeros(n_of(1.2, sr)), clank(sr, rng, base=rng.uniform(110, 140), decay=0.07), 0)), 4),
               anvil=(lambda: anvil(sr, rng, rng.choice([700.0, 760.0, 790.0, 820.0]), decay=0.35), 4),
               tick=(lambda: metal_tick(sr, rng), 6),
               clank=(lambda: clank(sr, rng), 8))
    roots = ["C2", "C2", "Db2", "C2", "C2", "C2", "Ab1", "G1"]
    kicks = []
    for bar in range(32):
        sec, tb = bar // 8, s.t(bar)
        root = hz(roots[bar % 8])
        breakdown = sec == 3 and bar < 30
        if not breakdown:
            ks = [0, 8] + ([6] if bar % 2 else []) + ([11, 14] if bar % 4 == 3 else [])
            if bar >= 30:
                ks = [0, 4, 8, 12] + ([10, 14, 15] if bar == 31 else [])
            for k in ks:
                s.add("kick", pick("kick"), tb + k * st, 1.0 if k in (0, 8) else 0.8)
                kicks.append(tb + k * st)
        for k in range(16):
            if breakdown and k % 4:
                continue
            s.add("tick", pick("tick"), tb + k * st + (0.012 * sp if k % 2 else 0.0), (0.9, 0.35, 0.6, 0.35)[k % 4])
        if sec in (1, 2) or bar >= 30:
            for k in (4, 12):
                s.add("anvil", pick("anvil"), tb + k * st, 1.0 if k == 4 else 0.85)
        elif sec == 3:
            s.add("anvil", pick("anvil"), tb + 8 * st, 0.8)
        elif bar % 2:
            s.add("anvil", pick("anvil"), tb + 12 * st, 0.55)
        if sec in (1, 2):
            for k in ((3, 10) if bar % 2 == 0 else (7, 13)):
                s.add("clank", pick("clank"), tb + k * st, rng.uniform(0.5, 0.9))
        if bar == 31:
            for k in range(16):
                s.add("clank", pick("clank"), tb + k * st, 0.2 + 0.04 * k)
        if bar % 4 == 3 or (breakdown and bar % 2):
            m = n_of(1.6, sr)
            hiss = spectral_noise(m, sr, lambda tt, f: g_band(f, 3500, 1.1) * g_lowpass(f, 7500), rng)
            s.add("steam", hiss * env_pts(m, sr, [(0, 0), (0.15, 1), (0.5, 0.6), (1.6, 0)]) ** 1.5, tb + 2 * sp)
        if not breakdown:
            for k in (0, 2, 4, 5, 7, 8, 10, 12, 14, 15):
                iv = 12 if k in (7, 15) else (1 if k == 14 and bar % 2 else 0)
                s.add("bass", bass(root * 2 ** (iv / 12), 0.85 * st, sr, rng, vel=1.0 if k in (0, 8) else 0.75, kind="saw", cutoff=200), tb + k * st)
        else:
            s.add("bass", bass(root, 4 * sp - 0.05, sr, rng, vel=0.8, kind="saw", cutoff=150, rel=0.2), tb)
    duck = np.ones(s.n)
    m = n_of(0.3, sr)
    shape = 1.0 - 0.6 * np.exp(-tvec(m, sr) / 0.07) * np.clip(tvec(m, sr) / 0.004, 0, 1)
    for tk in kicks:
        i = int(round(tk * sr)) % s.n
        idx = (i + np.arange(m)) % s.n
        duck[idx] = np.minimum(duck[idx], shape)
    s.buses["bass"] *= duck
    dr = periodic_saws([("C2", 1.0), ("G2", 0.5), ("C1", 0.6)], L, sr, rng)
    s.buses["drone"] = sat(norm_peak(circ(lambda v: lp(v, 320, sr, 2), dr, sr)), 2.0)
    lead = melody("C4:1.5 Eb4:.5 G4 F4 | Eb4 Db4 C4:2 | Db4:1.5 F4:.5 Ab4 G4 | F4 Eb4 C4:2 | "
                  "C5:1.5 Bb4:.5 G4 Ab4 | G4 F4:.5 Eb4:.5 F4:2 | Eb4 F4 Ab4 G4 | G4:3 r", start=64)
    for beat, f, d in lead:
        s.add("lead", brass(f, d * sp * 0.95, sr, rng, vel=0.7, att=0.05, rel=0.25, bright=0.7, vib=10), beat * sp)
        s.add("lead", pad(f / 2, d * sp * 0.95, sr, rng, att=0.05, rel=0.3, cutoff=700, voices=2), beat * sp, 0.35)
    ir = make_ir(sr, 2.0, rng, pre=0.02, damp=0.35, size=1.3)
    mix = {"kick": (1.0, 0.08), "tick": (0.17, 0.1), "anvil": (0.5, 0.35), "clank": (0.4, 0.3), "steam": (0.1, 0.3),
           "bass": (0.55, 0.05), "drone": (0.22, 0.2), "lead": (0.5, 0.35)}
    return master(s.render(mix, ir), 1.6)


@music("music_chapel")
def _(rng):
    """A minor, 60 BPM, 16 bars: choir and organ chords, a soprano line, sparse timpani."""
    sr = MUSIC_SR
    s = Song(sr, 60, 16)
    sp = s.spb
    chords = ["A2 E3 A3 C4", "F2 F3 A3 C4", "D3 F3 A3 D4", "E2 E3 G#3 B3", "A2 E3 A3 C4", "C3 E3 G3 C4", "G2 D3 G3 B3", "E2 E3 G#3 B3",
              "F2 F3 A3 C4", "D3 F3 A3 D4", "A2 E3 A3 C4", "E2 E3 G#3 B3", "F2 F3 A3 C4", "G2 D3 G3 B3", "E2 E3 A3 B3", "E2 E3 G#3 B3"]
    pedal = ["A1", "F1", "D2", "E1", "A1", "C2", "G1", "E1", "F1", "D2", "A1", "E1", "F1", "G1", "E1", "E1"]
    parts = ["bass", "tenor", "alto", "alto"]
    soft = ((0.5, 0.3), (1, 1.0), (2, 0.35), (4, 0.12))
    for bar, ch in enumerate(chords):
        tb = s.t(bar)
        vow = ("a", "o") if bar % 2 == 0 else ("o", "a")
        for j, nm in enumerate(ch.split()):
            f = hz(nm)
            s.add("choir", choir(f, 4 * sp + 0.3, sr, rng, part=parts[j], vowels=vow, att=0.9, rel=1.4, voices=2, vib=12), tb - 0.15, 0.25)
            s.add("organ", organ(f, 4 * sp, sr, rng, att=0.12, rel=0.6, stops=soft, chiff=0.03), tb, 0.2)
        s.add("pedal", organ(hz(pedal[bar]), 4 * sp, sr, rng, att=0.15, rel=0.6, stops=((1, 1.0), (2, 0.5), (3, 0.12), (4, 0.15)), chiff=0.0), tb)
    mel = melody("r:4 | r:3 E4 | A4:2 F4 A4 | B4:2 G#4:2 | C5:2 B4 A4 | G4:3 E4 | G4:2 B4 D5 | B4:4 | "
                 "C5:2 A4:2 | D5:2 C5 A4 | C5:1.5 B4:.5 A4:2 | G#4:2 B4:2 | A4:2 C5 A4 | B4 A4 G4:2 | A4:4 | G#4:3 r")
    for beat, f, d in mel:
        dur = d * sp
        s.add("sop", choir(f, dur * 1.04, sr, rng, part="soprano", vowels=("a", "o"), att=min(0.35, 0.4 * dur), rel=0.7, voices=3, vib=18), beat * sp)
    for bar, nm, g in ((0, "A2", 0.8), (4, "A2", 0.5), (8, "F2", 0.7), (12, "F2", 0.5)):
        s.add("timp", timpani(hz(nm), sr, rng, decay=1.8), s.t(bar), g)
    tt = s.t(15, 1.0)
    while tt < s.t(15, 4.0) - 0.05:
        u = (tt - s.t(15, 1.0)) / (3 * sp)
        s.add("timp", timpani(hz("E2"), sr, rng, vel=0.15 + 0.5 * u ** 1.5, decay=1.0), tt)
        tt += rng.uniform(0.07, 0.085)
    for bar in (0, 8):
        s.add("bell", bell(hz("E5"), sr, rng, decay=2.5, dur=4.0, bright=0.7), s.t(bar, 2.0), 1.0)
    ir = make_ir(sr, 4.8, rng, pre=0.045, damp=0.55, size=2.2)
    mix = {"choir": (1.0, 0.6), "sop": (0.55, 0.55), "organ": (0.6, 0.5), "pedal": (0.18, 0.4), "timp": (0.7, 0.6), "bell": (0.4, 0.9)}
    return master(s.render(mix, ir), 1.1)


@music("music_market")
def _(rng):
    """A harmonic minor, 96 BPM, 24 bars: oud-like ostinato, frame drum, accordion, folk melody."""
    sr = MUSIC_SR
    s = Song(sr, 96, 24)
    sp = s.spb
    e8 = sp / 2
    pick = kit(rng,
               doum=(lambda: drum(sr, rng, 125, 88, 0.04, 0.32, skin=0.4, click=0.05), 4),
               tek=(lambda: norm_peak(modes(sr, 0.15, [650, 1180, 2300], [0.6, 0.5, 0.3], [0.03, 0.02, 0.012])
                                      + burst(sr, 0.15, rng, 1800, 5000, 0.012)), 4),
               ka=(lambda: 0.5 * burst(sr, 0.1, rng, 1200, 4000, 0.01), 4),
               riq=(lambda: norm_peak(sum(clink(sr, rng, 3000, 6000, 0.03) for _ in range(3))), 5))
    CH = {"Am": ("A2", "A3 E4 A4 E4 C5 E4 A4 E4", "A3 C4 E4"), "Dm": ("D2", "D3 A3 D4 A3 F4 A3 D4 A3", "A3 D4 F4"),
          "E": ("E2", "E3 B3 E4 B3 G#4 B3 E4 B3", "G#3 B3 E4"), "G": ("G2", "G3 D4 G4 D4 B4 D4 G4 D4", "G3 B3 D4"),
          "F": ("F2", "F3 C4 F4 C4 A4 C4 F4 C4", "A3 C4 F4")}
    cyc = ["Am", "Am", "Dm", "E", "Am", "G", "F", "E"]
    for bar in range(24):
        sec, tb = bar // 8, s.t(bar)
        bn, ost, acc = CH[cyc[bar % 8]]
        for i, nm in enumerate(ost.split()):
            v = (0.95, 0.6, 0.8, 0.6, 0.9, 0.6, 0.8, 0.65)[i]
            s.add("oud", pluck(hz(nm), sr, rng, dur=0.9, hold=e8 * 1.6, vel=v, decay=0.7, bright=0.55, pos=0.13, body=OUD_BODY),
                  tb + i * e8 + (0.06 * e8 if i % 2 else 0.0) + rng.normal(0, 0.003))
        for b in (0, 2):
            s.add("bassp", pluck(hz(bn), sr, rng, dur=1.5, hold=sp * 1.8, vel=0.9, decay=1.0, bright=0.25, pos=0.2), tb + b * sp)
        for nm in acc.split():
            s.add("acc", reed(hz(nm), 4 * sp - 0.05, sr, rng, vel=0.6, att=0.12, rel=0.2, swell=0.25), tb, 0.3 if sec == 0 else 0.22)
        if bar == 23:
            for k in range(16):
                s.add("frame", pick("doum" if k % 4 == 0 else "tek"), tb + k * e8 / 2, 0.5 + 0.03 * k)
            continue
        for i in (0, 4):
            s.add("frame", pick("doum"), tb + i * e8, 1.0)
        for i in (1, 3, 6):
            s.add("frame", pick("tek"), tb + i * e8, 0.8)
        if sec >= 1:
            for k in (5, 9, 11, 15):
                s.add("frame", pick("ka"), tb + k * e8 / 2, 0.5)
        if sec == 2 or (sec == 1 and bar % 2):
            for i in (1, 3, 5, 7):
                s.add("riq", pick("riq"), tb + i * e8, 0.8 if i in (3, 7) else 0.55)
    tune_b = ("A4 B4:.5 C5:.5 B4 A4 | G#4:.5 A4:.5 B4 E4:2 | F4 E4:.5 F4:.5 A4 D5 | C5:.5 B4:.5 A4:.5 G#4:.5 B4:2 | "
              "A4 C5 E5 D5:.5 C5:.5 | B4 D5 G4:2 | A4:.5 G4:.5 F4:.5 E4:.5 F4 A4 | G#4 B4:.5 A4:.5 G#4:.5 F4:.5 E4")
    tune_c = ("E5 D5:.5 C5:.5 B4:.5 C5:.5 A4 | B4:.5 C5:.5 D5:.5 B4:.5 G#4 E4 | A4:.5 B4:.5 C5:.5 D5:.5 F5 D5 | E5 D5:.5 C5:.5 B4:2 | "
              "C5:.5 B4:.5 A4:.5 B4:.5 C5 E5 | D5 B4:.5 G4:.5 B4 D5 | C5:.5 A4:.5 F4:.5 A4:.5 C5 B4:.5 A4:.5 | G#4 B4 E4:2")
    for start, tune in ((32, tune_b), (64, tune_c)):
        for beat, f, d in melody(tune, start):
            s.add("tune", reed(f, d * sp * 0.97, sr, rng, vel=0.8, att=0.03, rel=0.1, musette=12, swell=0.1, bright=1.2), beat * sp)
    for i, nm in enumerate("E4 E4 F4 E4 E4 D4 C4 B3".split()):
        s.add("fiddle", bowed(hz(nm), 3.8 * sp, sr, rng, vel=0.6, att=0.4, rel=0.5, vib=18, swell=0.4), s.t(16 + i))
    ir = make_ir(sr, 1.4, rng, pre=0.012, damp=0.6, early=0.5)
    mix = {"oud": (0.42, 0.25), "bassp": (0.5, 0.1), "acc": (1.1, 0.3), "frame": (0.45, 0.2), "riq": (0.5, 0.25),
           "tune": (0.6, 0.3), "fiddle": (0.55, 0.4)}
    return master(s.render(mix, ir), 1.2)


@music("music_roost")
def _(rng):
    """F minor with tritones, 126 BPM, 32 bars: taiko, low brass clusters, ostinato, dissonant choir."""
    sr = MUSIC_SR
    s = Song(sr, 126, 32)
    sp = s.spb
    st = sp / 4
    L = s.length
    pick = kit(rng,
               odaiko=(lambda: drum(sr, rng, 85, 46, 0.07, 0.7, skin=0.35, click=0.2), 3),
               nagado=(lambda: drum(sr, rng, 130, 88, 0.04, 0.32, skin=0.35, click=0.3), 4),
               shime=(lambda: drum(sr, rng, 520, 440, 0.01, 0.06, skin=0.5, skin_lp=3500, click=0.5), 5))
    roots = ["F", "F", "Gb", "F", "B", "C", "Db", "C"]
    pat = [0, 0, 12, 0, 1, 0, 7, 0, 0, 0, 12, 0, 1, 0, 6, 0]
    for bar in range(32):
        sec, tb = bar // 8, s.t(bar)
        r2 = hz(roots[bar % 8] + "2")
        half = sec == 3 and bar < 30
        for k, iv in enumerate(pat):
            vel = 1.0 if k % 4 == 0 else 0.7
            s.add("ost", bass(r2 * 2 ** (iv / 12), 0.7 * st, sr, rng, vel=vel, kind="saw", cutoff=420), tb + k * st, 0.6 if half else 1.0)
        if half:
            for k in (0, 8):
                s.add("taiko", pick("odaiko"), tb + k * st, 1.0)
            s.add("taiko", pick("nagado"), tb + 12 * st, 0.6)
        else:
            for k in (0, 6, 10):
                s.add("taiko", pick("odaiko"), tb + k * st, 1.0 if k == 0 else 0.85)
            for k in (4, 12):
                s.add("taiko", pick("nagado"), tb + k * st, 0.9)
            if bar % 4 == 3:
                for k in (12, 13, 14, 15):
                    s.add("taiko", pick("nagado"), tb + k * st, 0.55 + 0.12 * (k - 12))
            dense = sec == 2 or bar >= 30
            for k in range(0, 16, 1 if dense else 2):
                acc = 1.0 if k % 4 == 2 else 0.55
                if bar >= 30:
                    acc *= 0.5 + 0.5 * ((bar - 30) * 16 + k) / 32
                s.add("shime", pick("shime"), tb + k * st, acc)
        if sec >= 1 and not half:
            hits = ([0, 6] if bar % 2 == 0 else []) + ([10] if bar % 4 == 3 else [])
            for k in hits:
                for nm_mult, g in ((1.0, 1.0), (2.0, 0.7), (2 * 2 ** (1 / 12), 0.6), (2 * 2 ** (6 / 12), 0.5)):
                    s.add("brass", brass(r2 * nm_mult, 0.36 * sp, sr, rng, vel=0.95, att=0.025, rel=0.25, bright=1.1), tb + k * st, 0.3 * g)
        if half and bar % 2 == 0:
            for nm_mult, g in ((1.0, 1.0), (1.5, 0.6), (2 * 2 ** (1 / 12), 0.5)):
                s.add("brass", brass(r2 * nm_mult, 7.5 * sp, sr, rng, vel=0.7, rel=0.8, swell=True), tb, 0.3 * g)
    for b0, cl in ((16, "F4 Gb4 C5"), (20, "B3 C4 F#4")):
        for nm in cl.split():
            s.add("choir", choir(hz(nm), 4 * 4 * sp, sr, rng, part="soprano", vowels=("u", "o"), att=2.0, rel=1.5, voices=2, vib=25), s.t(b0), 0.3)
    horn = melody("F3:1.5 Ab3:.5 C4 Db4 | C4 B3 C4:2 | Db4:1.5 Bb3:.5 Gb3 F3 | F3:4 | "
                  "B3:1.5 D4:.5 F4 E4 | E4:1.5 G4:.5 F4 E4 | F4:1.5 Ab4:.5 Db4:2 | C4:3 r", start=64)
    for beat, f, d in horn:
        s.add("horn", brass(f, d * sp * 0.95, sr, rng, vel=0.8, att=0.05, rel=0.2, bright=0.75, vib=12), beat * sp)
    m = n_of(4 * 4 * sp, sr)
    u = tvec(m, sr) / (m / sr)
    riser = spectral_noise(m, sr, lambda tt, f: g_band(f, 400 * 8 ** (tt / (m / sr)), 0.8), rng) * u ** 2
    riser += 0.3 * sine(110 * 4 ** u, m, sr) * u ** 2
    s.add("riser", fade(riser, sr, 0, 0.05), s.t(28))
    dr = periodic_saws([("F1", 1.0), ("F2", 0.5)], L, sr, rng)
    s.buses["drone"] = circ(lambda v: lp(v, 220, sr, 2), dr, sr)
    ir = make_ir(sr, 2.2, rng, pre=0.02, damp=0.5, size=1.5)
    mix = {"ost": (0.45, 0.1), "taiko": (0.45, 0.25), "shime": (0.3, 0.15), "brass": (1.0, 0.3), "choir": (0.6, 0.6),
           "horn": (0.55, 0.35), "riser": (0.12, 0.3), "drone": (0.08, 0.2)}
    return master(s.render(mix, ir), 1.5)


@music("music_ending")
def _(rng):
    """F major with a borrowed minor iv, 72 BPM, 12 bars: harp, warm pad, the main theme in major."""
    sr = MUSIC_SR
    s = Song(sr, 72, 12)
    sp = s.spb
    e8 = sp / 2
    harp = ["F2 C3 F3 A3 C4 F4", "D2 A2 D3 F3 A3 D4", "Bb2 D3 F3 Bb3 D4 F4", "C2 G2 C3 E3 G3 C4",
            "F2 C3 F3 A3 C4 F4", "A2 E3 A3 C4 E4 A4", "Bb2 D3 F3 Bb3 D4 F4", "Bb2 Db3 F3 Bb3 Db4 F4",
            "F2 C3 F3 A3 C4 F4", "D2 A2 D3 F3 A3 D4", "G2 D3 F3 Bb3 D4 F4", "C3 F3 G3 C4 F4 G4"]
    pads = ["F3 A3 C4", "F3 A3 D4", "F3 Bb3 D4", "E3 G3 C4", "F3 A3 C4", "E3 A3 C4", "F3 Bb3 D4", "F3 Bb3 Db4",
            "F3 A3 C4", "F3 A3 D4", "F3 Bb3 D4", "F3 G3 C4"]
    for bar in range(12):
        tb = s.t(bar)
        tones = harp[bar].split()
        for i, j in enumerate((0, 1, 2, 3, 4, 5, 4, 3)):
            nm = tones[j]
            if bar == 11 and i >= 4:
                nm = {"F3": "E3", "F4": "E4"}.get(nm, nm)
            s.add("harp", pluck(hz(nm), sr, rng, dur=2.5, vel=0.7 if i else 0.85, decay=1.6, bright=0.4, pos=0.3), tb + i * e8 + rng.normal(0, 0.004))
        for nm in pads[bar].split():
            s.add("pad", pad(hz(nm), 4 * sp, sr, rng, att=0.8, rel=1.5, cutoff=750, voices=3, detune=7), tb, 0.2)
        s.add("bass", bass(hz(harp[bar].split()[0]) / (2 if bar in (3, 11) else 1), 4 * sp - 0.1, sr, rng, vel=0.7, kind="round", rel=0.4), tb, 0.6)
    mel = melody("r:4 | A4:1.5 C5:.5 F5 E5 | D5:3 r | C5:1.5 E5:.5 D5 C5 | A4:3 r | C5:1.5 E5:.5 D5 C5 | "
                 "D5:1.5 C5:.5 Bb4 A4 | Db5:2 C5 Bb4 | C5 A4:2 r | F4:1.5 A4:.5 D5 C5 | Bb4:1.5 A4:.5 G4 F4 | G4:2 E4:2")
    for beat, f, d in mel:
        dur = d * sp
        s.add("lead", bowed(f, dur * 1.03, sr, rng, vel=0.65, att=min(0.25, 0.4 * dur), rel=0.5, vib=15, swell=0.3, bright=0.8), beat * sp)
    s.add("bell", bell(hz("F4"), sr, rng, decay=3.0, dur=5.0, bright=0.6), s.t(0), 1.0)
    s.add("bell", bell(hz("C5"), sr, rng, decay=2.5, dur=4.0, bright=0.6), s.t(8), 0.6)
    ir = make_ir(sr, 2.8, rng, pre=0.025, damp=0.6, size=1.4)
    mix = {"harp": (0.4, 0.4), "pad": (1.0, 0.45), "bass": (0.4, 0.15), "lead": (0.55, 0.45), "bell": (0.5, 0.8)}
    return master(s.render(mix, ir), 1.15)


# --- Finishing, output and checks ----------------------------------------------------------------

SFX_PEAK = 10 ** (-1.0 / 20)
MUSIC_RMS = 10 ** (-20.0 / 20)
MUSIC_CEIL = 10 ** (-3.4 / 20)


def finish_sfx(x):
    """DC-block, tame the extreme top, trim the silent tail, fade the ends, peak to -1 dBFS."""
    x = lp(hp(np.asarray(x, dtype=float), 28, SR), 14000, SR)
    a = np.abs(x)
    last = np.nonzero(a > a.max() * 10 ** (-64 / 20))[0][-1]
    x = x[:min(len(x), last + n_of(0.01, SR))]
    x = fade(x, SR, 0.0005, min(0.03, 0.15 * len(x) / SR))
    # Remove any residual mean with a Hann-shaped offset (mean 1 when doubled) so the ends stay at 0.
    return norm_peak(x - np.mean(x) * np.hanning(len(x)) * 2.0, SFX_PEAK)


def finish_music(x):
    """Circular DC-block and top roll-off, then RMS to -20 dBFS under a -3.4 dBFS limiter."""
    x = circ(lambda v: hp(v, 32, MUSIC_SR), x, MUSIC_SR)
    x = circ(lambda v: lp(v, 11000, MUSIC_SR), x, MUSIC_SR // 10)
    x = x - np.mean(x)
    for _ in range(4):
        x = limit(x * (MUSIC_RMS / rms(x)), MUSIC_CEIL, MUSIC_SR)
    return x


def write_wav(path, x, sr, dither_rng=None, loop=False):
    """16-bit mono PCM WAV. Loops also get a RIFF 'smpl' chunk (forward loop over the whole
    file), which Godot's importer picks up with its default "Detect From WAV" loop mode."""
    y = np.clip(x, -1.0, 1.0) * 32767.0
    if dither_rng is not None:
        y += dither_rng.random(len(y)) - dither_rng.random(len(y))
    pcm = np.clip(np.round(y), -32768, 32767).astype("<i2").tobytes()
    chunks = struct.pack("<4sIHHIIHH", b"fmt ", 16, 1, 1, sr, sr * 2, 2, 16)
    chunks += struct.pack("<4sI", b"data", len(pcm)) + pcm
    if loop:
        body = struct.pack("<9I", 0, 0, round(1e9 / sr), 60, 0, 0, 0, 1, 0)
        body += struct.pack("<6I", 0, 0, 0, len(y) - 1, 0, 0)   # cue id, forward, start, inclusive end
        chunks += struct.pack("<4sI", b"smpl", len(body)) + body
    path.write_bytes(struct.pack("<4sI4s", b"RIFF", 4 + len(chunks), b"WAVE") + chunks)


def build(name):
    rng = np.random.default_rng(zlib.crc32(name.encode()))
    if name in MUSIC:
        x, sr = finish_music(MUSIC[name](rng)), MUSIC_SR
        write_wav(OUT / f"{name}.wav", x, sr, np.random.default_rng(zlib.crc32(name.encode()) + 1), loop=True)
    else:
        x, sr = finish_sfx(SFX[name](rng)), SR
        write_wav(OUT / f"{name}.wav", x, sr)
    return name, len(x) / sr


def check():
    """Print level statistics for every expected file."""
    total = 0
    print(f"{'name':16s} {'dur':>6s} {'rate':>6s} {'peak':>6s} {'rms':>6s} {'dc':>9s}  wrap")
    for name in list(SFX) + list(MUSIC):
        path = OUT / f"{name}.wav"
        if not path.exists():
            print(f"{name:16s} MISSING")
            continue
        total += path.stat().st_size
        with wave.open(str(path), "rb") as w:
            sr = w.getframerate()
            x = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2") / 32768.0
        line = f"{name:16s} {len(x) / sr:6.2f} {sr:6d} {db(np.max(np.abs(x))):6.1f} {db(rms(x)):6.1f} {np.mean(x):+9.6f}"
        if name in MUSIC:
            step = np.abs(np.diff(x))
            line += f"  |x0-xN| {abs(x[0] - x[-1]):.4f} (median step {np.median(step):.4f}, p99.9 {np.percentile(step, 99.9):.4f})"
        else:
            line += f"  first {abs(x[0]):.4f} last {abs(x[-1]):.4f}"
        print(line)
    print(f"total {total / 1e6:.1f} MB in {OUT}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--only", help="comma-separated names to (re)generate")
    ap.add_argument("--check", action="store_true", help="report levels of the generated files")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    args = ap.parse_args()
    if args.check:
        check()
        return
    names = list(MUSIC) + list(SFX)
    if args.only:
        names = [s.strip() for s in args.only.split(",") if s.strip()]
        unknown = [s for s in names if s not in SFX and s not in MUSIC]
        if unknown:
            sys.exit(f"unknown sound(s): {', '.join(unknown)}")
    OUT.mkdir(parents=True, exist_ok=True)
    with ProcessPoolExecutor(max_workers=max(1, args.jobs)) as ex:
        for name, dur in ex.map(build, names):
            print(f"  {name:16s} {dur:6.2f} s")


if __name__ == "__main__":
    main()
