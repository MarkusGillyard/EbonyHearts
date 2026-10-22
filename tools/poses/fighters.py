#!/usr/bin/env python3
"""Source of data/poses/<id>.json: each fighter's own key poses (stance, intro and win
overrides, and the poses of its specials, chains and finisher). Authored for a 1.8 m body like
the shared poses and scaled here to each fighter's height, so positions read the same for every
fighter. Run to regenerate; see tools/poses/common.py for the conventions.
"""
import json
import os

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
POS_KEYS = ["hips", "hand_l", "hand_r", "elbow_l", "elbow_r", "foot_l", "foot_r", "knee_l", "knee_r"]
OPEN = {"fist_l": 0.0, "fist_r": 0.0}
PALM_L = {"fist_l": 0.0, "fingers_l": [0, 1, 0], "palm_l": [1, 0, 0]}
PALM_R = {"fist_r": 0.0, "fingers_r": [0, 1, 0], "palm_r": [1, 0, 0]}


def pose(base=None, **kw):
    d = {"base": base} if base else {}
    d.update(kw)
    return d


def merge(*ds):
    out = {}
    for d in ds:
        out.update(d)
    return out


F = {}

# --- Ashka Rook: Muay Thai, upright, high guard ------------------------------------------------
F["ashka"] = {
    "stance": pose("stance", hips=[0, -0.06, 0], hips_rot=[0, -24, 0], spine=[-2, 8, 0], chest=[-2, 8, 0], head=[6, 6, 0],
                   foot_l=[0.2, 0, -0.12], yaw_l=-10, foot_r=[-0.2, 0, 0.15], yaw_r=-45, knee_l=[0.9, 0.5, -0.3], knee_r=[0.6, 0.5, 0.6],
                   hand_l=[0.32, 1.5, -0.1], elbow_l=[0.2, 1.0, -0.5], hand_r=[0.24, 1.52, 0.08], elbow_r=[0.1, 1.0, 0.5]),
    "intro": pose("stance", hips=[0, -0.04, 0], hips_rot=[0, -8, 0], spine=[-6, 4, 0], head=[22, 0, 0],
                  foot_l=[0.08, 0, -0.12], foot_r=[-0.06, 0, 0.13], hand_l=[0.2, 1.36, -0.02], hand_r=[0.2, 1.36, 0.03],
                  elbow_l=[0.1, 1.1, -0.5], elbow_r=[0.1, 1.1, 0.5], **merge(PALM_L, PALM_R)),
    "ashka_elbow_load": pose("stance", hips_rot=[0, -46, 0], spine=[-4, 12, 0], hand_r=[0.0, 1.56, 0.15], elbow_r=[-0.1, 1.5, 0.45]),
    "ashka_elbow_hit": pose("stance", hips=[0.08, -0.08, 0], hips_rot=[0, 18, 0], spine=[-6, 16, 0], chest=[-4, 16, 0], head=[6, -6, 0],
                            foot_r=[-0.18, 0, 0.14], yaw_r=-25, hand_r=[0.16, 1.46, -0.14], elbow_r=[0.9, 1.4, 0.2], hand_l=[0.22, 1.48, -0.14]),
    "ashka_knee_load": pose("stance", hips=[0.04, -0.04, 0], hand_l=[0.42, 1.48, -0.12], hand_r=[0.42, 1.48, 0.12], foot_r=[-0.1, 0.32, 0.1],
                            free_r=1, knee_r=[0.9, 0.9, 0.2], toe_r=30, **OPEN),
    "ashka_knee_hit": pose("stance", hips=[0.1, -0.02, 0], hips_rot=[10, -20, 0], spine=[-10, 6, 0], head=[14, 4, 0],
                           hand_l=[0.45, 1.42, -0.12], hand_r=[0.45, 1.42, 0.12], foot_r=[0.05, 0.62, 0.08], free_r=1, knee_r=[1.0, 1.2, 0.1], toe_r=40,
                           foot_l=[0.1, 0, -0.1], yaw_l=-20, **OPEN),
    "ashka_head_hit": pose("hk_hit", hips_rot=[18, -82, 0], spine=[16, 14, 0], foot_r=[0.88, 1.56, 0.0], knee_r=[0.9, 1.5, 0.4]),
    "ashka_ember_load": pose("stance", hips_rot=[0, -62, 0], spine=[-4, 18, 0], head=[6, 30, 0], hand_l=[-0.04, 1.06, 0.1], hand_r=[-0.1, 1.0, 0.18],
                             elbow_l=[-0.2, 0.9, -0.4], elbow_r=[-0.3, 0.9, 0.5], fist_l=0.3, fist_r=0.3, glow=0.6),
    "ashka_ember_throw": pose("stance", hips=[0.08, -0.08, 0], hips_rot=[0, -12, 0], spine=[-8, 6, 0], head=[6, 6, 0],
                              hand_l=[0.64, 1.36, -0.04], hand_r=[0.6, 1.3, 0.07], elbow_l=[0.3, 1.0, -0.6], elbow_r=[0.3, 1.0, 0.6],
                              fingers_l=[0, 1, 0], palm_l=[1, 0, 0], fingers_r=[0, 1, 0], palm_r=[1, 0, 0], fist_l=0, fist_r=0, glow=1.0),
    "ashka_flying_knee": pose("jump", hips_rot=[-10, -20, 0], spine=[-12, 6, 0], head=[16, 0, 0], foot_r=[0.12, 0.58, 0.08], knee_r=[1.0, 1.1, 0.1], toe_r=40,
                              foot_l=[-0.42, 0.25, -0.1], knee_l=[0.2, 0.4, -0.2], toe_l=50, hand_l=[0.3, 1.55, -0.1], hand_r=[0.3, 1.55, 0.1]),
    "ashka_rising_knee": pose("jump", hips_rot=[-6, -18, 0], spine=[-6, 6, 0], head=[-8, 0, 0], foot_r=[0.06, 0.82, 0.08], knee_r=[0.9, 1.7, 0.1], toe_r=35,
                              foot_l=[-0.05, 0.1, -0.12], hand_r=[0.25, 2.0, 0.08], elbow_r=[0.4, 1.6, 0.5], hand_l=[0.2, 1.3, -0.15], glow=0.8),
}

# --- Gorm Tallow: sumo and catch wrestling, wide and low ----------------------------------------
F["gorm"] = {
    "stance": pose("stance", hips=[0, -0.16, 0], hips_rot=[0, -16, 0], spine=[-14, 6, 0], chest=[-6, 6, 0], head=[14, 4, 0],
                   foot_l=[0.18, 0, -0.2], yaw_l=-25, foot_r=[-0.22, 0, 0.24], yaw_r=-60, knee_l=[0.8, 0.5, -0.7], knee_r=[0.5, 0.5, 0.8],
                   hand_l=[0.42, 1.15, -0.2], hand_r=[0.36, 1.12, 0.22], elbow_l=[0.0, 1.0, -0.8], elbow_r=[0.0, 1.0, 0.8], fist_l=0.5, fist_r=0.5),
    "crouch": pose("crouch", hips=[0, -0.4, 0], foot_l=[0.22, 0, -0.22], foot_r=[-0.22, 0, 0.26], knee_l=[0.7, 0.4, -0.8], knee_r=[0.6, 0.3, 0.8]),
    "intro": pose("stance", hips=[0, -0.2, 0], hips_rot=[0, -6, 0], spine=[-6, 0, 0], foot_l=[0.05, 0, -0.26], foot_r=[0.05, 0.62, 0.48], free_r=1,
                  knee_r=[0.3, 1.0, 1.0], hand_l=[0.12, 0.72, -0.24], hand_r=[0.12, 0.85, 0.3], fist_l=0, fist_r=0),
    "win": pose("stance", hips=[0, -0.1, 0], hips_rot=[0, -10, 0], spine=[6, 0, 0], chest=[8, 0, 0], head=[-8, 0, 0],
                hand_l=[0.05, 1.72, -0.3], hand_r=[0.05, 1.72, 0.3], elbow_l=[0.0, 1.5, -0.9], elbow_r=[0.0, 1.5, 0.9], fist_l=1, fist_r=1),
    "gorm_hammer_load": pose("stance", hips=[0, -0.08, 0], spine=[10, 0, 0], chest=[8, 0, 0], head=[-10, 0, 0],
                             hand_l=[0.05, 2.02, -0.07], hand_r=[0.05, 2.02, 0.07], elbow_l=[-0.2, 1.7, -0.45], elbow_r=[-0.2, 1.7, 0.45]),
    "gorm_hammer_hit": pose("stance", hips=[0.12, -0.28, 0], spine=[-26, 0, 0], chest=[-14, 0, 0], head=[18, 0, 0],
                            hand_l=[0.66, 0.95, -0.06], hand_r=[0.66, 0.95, 0.06], elbow_l=[0.5, 1.5, -0.4], elbow_r=[0.5, 1.5, 0.4]),
    "gorm_belly_load": pose("stance", spine=[14, 0, 0], chest=[10, 0, 0], head=[-6, 0, 0], hand_l=[-0.3, 1.15, -0.45], hand_r=[-0.3, 1.15, 0.45], fist_l=0.3, fist_r=0.3),
    "gorm_belly_hit": pose("stance", hips=[0.24, -0.12, 0], hips_rot=[-10, -6, 0], spine=[16, 0, 0], chest=[10, 0, 0], head=[-4, 0, 0],
                           hand_l=[-0.1, 1.2, -0.6], hand_r=[-0.1, 1.2, 0.6], fist_l=0.3, fist_r=0.3),
    "gorm_lift": pose("stance", hips=[0, -0.18, 0], spine=[8, 0, 0], chest=[6, 0, 0], head=[-14, 0, 0],
                      hand_l=[0.25, 2.12, -0.2], hand_r=[0.25, 2.12, 0.2], elbow_l=[0.1, 1.8, -0.7], elbow_r=[0.1, 1.8, 0.7], fist_l=0.6, fist_r=0.6),
    "gorm_slam": pose("stance", hips=[0.1, -0.36, 0], spine=[-32, 0, 0], chest=[-14, 0, 0], head=[16, 0, 0],
                      hand_l=[0.78, 0.6, -0.2], hand_r=[0.78, 0.6, 0.2], fist_l=0.3, fist_r=0.3),
    "gorm_hide_load": pose("stance", hips=[0, -0.22, 0], spine=[-18, 0, 0], hand_l=[0.25, 1.25, 0.08], hand_r=[0.25, 1.28, -0.08], fist_l=1, fist_r=1),
    "gorm_hide": pose("stance", hips=[0, -0.18, 0], hips_rot=[0, -10, 0], spine=[8, 0, 0], chest=[10, 0, 0], head=[-6, 0, 0],
                      hand_l=[0.22, 0.98, -0.48], hand_r=[0.22, 0.98, 0.48], elbow_l=[0.0, 1.2, -0.9], elbow_r=[0.0, 1.2, 0.9], fist_l=1, fist_r=1),
    "gorm_charge_load": pose("stance", hips=[-0.04, -0.2, 0], hips_rot=[0, -68, 0], spine=[-24, 8, 0], head=[20, 40, 0],
                             hand_l=[0.2, 1.1, -0.1], hand_r=[-0.05, 1.05, 0.2]),
    "gorm_charge": pose("stance", hips=[0.18, -0.2, 0], hips_rot=[-8, -72, 0], spine=[-30, 10, 0], head=[24, 50, 0],
                        foot_l=[0.3, 0, -0.18], foot_r=[-0.4, 0, 0.22], free_r=0, hand_l=[0.28, 1.15, -0.1], hand_r=[-0.1, 1.0, 0.25]),
    "gorm_slap_load": pose("stance", hips=[0, -0.1, 0], hand_r=[0.1, 2.02, 0.3], elbow_r=[-0.2, 1.7, 0.6], fist_r=0),
    "gorm_slap": pose("crouch", hips=[0.08, -0.55, 0], spine=[-30, 10, 0], head=[24, 0, 0], hand_r=[0.78, 0.05, 0.1], fist_r=0,
                      fingers_r=[1, 0, 0], palm_r=[0, -1, 0], hand_l=[0.3, 0.6, -0.25]),
    "gorm_stomp_load": pose("stance", hips=[0, -0.08, 0], spine=[4, 0, 0], foot_r=[0.45, 0.78, 0.12], free_r=1, knee_r=[0.9, 1.4, 0.3],
                            hand_l=[0.1, 1.4, -0.5], hand_r=[-0.05, 1.4, 0.5]),
    "gorm_stomp": pose("stance", hips=[0.1, -0.2, 0], spine=[-10, 0, 0], foot_r=[0.62, 0.0, 0.06], free_r=1, knee_r=[0.9, 0.8, 0.3],
                       hand_l=[0.0, 1.2, -0.55], hand_r=[-0.1, 1.2, 0.55]),
}

# --- Nix Hollow: low ninja stance, sickle in the right hand ------------------------------------
F["nix"] = {
    "stance": pose("stance", hips=[0, -0.2, 0], hips_rot=[0, -42, 0], spine=[-12, 12, 0], chest=[-8, 12, 0], head=[14, 14, 0],
                   foot_l=[0.28, 0, -0.14], yaw_l=-15, foot_r=[-0.3, 0, 0.18], yaw_r=-70, knee_l=[0.9, 0.4, -0.4], knee_r=[0.6, 0.4, 0.7],
                   hand_l=[0.46, 1.26, -0.08], elbow_l=[0.2, 0.9, -0.5], hand_r=[-0.14, 1.02, 0.26], elbow_r=[-0.3, 1.1, 0.5],
                   fist_l=0, fingers_l=[0.3, 1, 0], palm_l=[0, 0, 1]),
    "intro": pose("crouch", hips=[0, -0.5, 0], hand_r=[0.1, 1.5, 0.12], hand_l=[0.15, 0.4, -0.2], fist_l=0, head=[24, 0, 0]),
    "win": pose("stance", hips=[0, -0.1, 0], hips_rot=[0, -20, 0], spine=[-2, 6, 0], head=[4, 10, 0],
                hand_l=[0.15, 1.36, -0.04], fist_l=0.3, hand_r=[0.12, 1.76, 0.12], elbow_r=[0.3, 1.5, 0.5]),
    "nix_slash_load": pose("stance", hand_r=[-0.24, 1.58, 0.3], elbow_r=[-0.3, 1.4, 0.6], hips_rot=[0, -55, 0]),
    "nix_slash_hit": pose("stance", hips=[0.08, -0.22, 0], hips_rot=[0, 8, 0], spine=[-16, 10, 0], hand_r=[0.62, 1.0, 0.0], elbow_r=[0.3, 1.1, 0.6]),
    "nix_low_hit": pose("sweep_hit", hand_r=[-0.2, 0.35, 0.3]),
    "nix_axe_load": pose("stance", hips=[0, -0.04, 0], hips_rot=[8, -30, 0], spine=[10, 6, 0], foot_l=[0.22, 1.72, -0.05], free_l=1, knee_l=[0.9, 1.6, -0.2], toe_l=5),
    "nix_axe_hit": pose("stance", hips=[0.1, -0.12, 0], hips_rot=[-6, -30, 0], spine=[-10, 6, 0], foot_l=[0.65, 0.7, -0.02], free_l=1, knee_l=[0.9, 1.1, -0.2], toe_l=10),
    "nix_throw_load": pose("stance", hand_r=[-0.3, 1.62, 0.35], elbow_r=[-0.4, 1.4, 0.6], hips_rot=[0, -60, 0]),
    "nix_throw": pose("stance", hips=[0.06, -0.18, 0], hips_rot=[0, 6, 0], spine=[-10, 8, 0], hand_r=[0.62, 1.32, 0.05], elbow_r=[0.3, 1.2, 0.6], fist_r=0.2),
    "nix_sink": pose("crouch", hips=[0, -0.62, 0], spine=[-34, 10, 0], head=[30, 0, 0], hand_l=[0.4, 0.05, -0.2], hand_r=[0.35, 0.05, 0.22], fist_l=0),
    "nix_rise_slash": pose("stance", hips=[0.06, 0.0, 0], hips_rot=[-4, -20, 0], spine=[8, 10, 0], head=[-14, 6, 0], foot_l=[0.2, 0, -0.1],
                           hand_r=[0.42, 2.0, 0.05], elbow_r=[0.5, 1.5, 0.5], hand_l=[0.0, 1.2, -0.3]),
    "nix_chain_load": pose("stance", hand_r=[0.0, 1.92, 0.22], elbow_r=[0.0, 1.6, 0.6], hips_rot=[0, -50, 0], spine=[0, 14, 0]),
    "nix_chain_throw": pose("stance", hips=[0.1, -0.28, 0], hips_rot=[0, 4, 0], spine=[-18, 6, 0], hand_r=[0.68, 0.72, 0.05], elbow_r=[0.3, 0.9, 0.6]),
    "nix_pull": pose("stance", hips=[-0.1, -0.22, 0], hips_rot=[0, -50, 0], spine=[10, 14, 0], head=[0, 10, 0], hand_r=[-0.2, 1.0, 0.22], hand_l=[0.2, 1.1, -0.1],
                     foot_l=[0.34, 0, -0.14]),
    "nix_drop": pose("jump", hips_rot=[32, -46, 0], spine=[10, 10, 0], head=[-12, 20, 0], foot_r=[0.55, -0.14, 0.0], knee_r=[0.9, 0.4, 0.2], toe_r=5,
                     foot_l=[-0.05, 0.42, -0.1], hand_l=[0.0, 1.3, -0.3], hand_r=[-0.2, 1.1, 0.3]),
}

# --- Mirela Sorrow: Baguazhang palms, upright and flowing ----------------------------------------
F["mirela"] = {
    "stance": pose("stance", hips=[0, -0.07, 0], hips_rot=[0, -40, 0], spine=[-2, 12, 0], chest=[0, 10, 0], head=[2, 12, 0],
                   foot_l=[0.22, 0, -0.06], yaw_l=12, foot_r=[-0.18, 0, 0.14], yaw_r=-40, knee_l=[0.9, 0.5, -0.1], knee_r=[0.6, 0.5, 0.4],
                   hand_l=[0.44, 1.42, -0.05], elbow_l=[0.2, 1.1, -0.5], hand_r=[0.14, 1.2, 0.04], elbow_r=[0.0, 1.0, 0.5], **merge(PALM_L, PALM_R)),
    "intro": pose("stance", hips=[0, -0.02, 0], hips_rot=[0, -10, 0], spine=[-4, 2, 0], head=[20, 0, 0], foot_l=[0.06, 0, -0.1], foot_r=[-0.04, 0, 0.1],
                  hand_l=[0.18, 1.0, -0.02], hand_r=[0.17, 1.02, 0.03], elbow_l=[0.0, 1.1, -0.4], elbow_r=[0.0, 1.1, 0.4], fist_l=0.2, fist_r=0.2),
    "win": pose("stance", hips=[0, -0.03, 0], hips_rot=[0, -18, 0], spine=[2, 4, 0], head=[-6, 8, 12], foot_l=[0.1, 0, -0.08], foot_r=[-0.08, 0, 0.1],
                hand_l=[0.2, 1.36, -0.02], hand_r=[0.12, 1.78, 0.12], elbow_r=[0.3, 1.5, 0.6], **merge(PALM_L, PALM_R)),
    "hp_hit": pose("hp_hit", **PALM_L),
    "hp2_hit": pose("hp2_hit", **PALM_R),
    "lp_hit": pose("lp_hit", **PALM_L),
    "lp2_hit": pose("lp2_hit", **PALM_R),
    "mirela_double_load": pose("stance", hand_l=[0.18, 1.18, -0.08], hand_r=[0.16, 1.16, 0.08], **merge(PALM_L, PALM_R)),
    "mirela_double_hit": pose("stance", hips=[0.1, -0.1, 0], hips_rot=[0, -20, 0], spine=[-8, 6, 0], hand_l=[0.64, 1.2, -0.1], hand_r=[0.62, 1.12, 0.1],
                              elbow_l=[0.3, 0.9, -0.6], elbow_r=[0.3, 0.9, 0.6], **merge(PALM_L, PALM_R)),
    "mirela_circle_load": pose("stance", hips_rot=[0, -75, 0], spine=[0, 20, 0], head=[0, 30, 0], hand_l=[0.1, 1.35, -0.4], hand_r=[-0.2, 1.2, 0.3],
                               **merge(PALM_L, PALM_R)),
    "mirela_circle_hit": pose("stance", hips=[0.12, -0.12, 0], hips_rot=[0, 10, 0], spine=[-8, -6, 0], head=[4, -10, 0],
                              hand_r=[0.66, 1.18, 0.0], elbow_r=[0.3, 0.9, 0.6], hand_l=[0.2, 1.4, -0.25], **merge(PALM_L, PALM_R)),
    "mirela_crescent_hit": pose("hk_hit", foot_r=[-0.1, 0, 0.14], free_r=0, foot_l=[0.8, 1.48, -0.1], free_l=1, knee_l=[0.9, 1.4, -0.4], toe_l=10,
                                hips_rot=[14, -20, 0], **merge(PALM_L, PALM_R)),
    "mirela_lament_load": pose("stance", hips=[0, -0.08, 0], spine=[-12, 4, 0], head=[24, 0, 0], hand_l=[0.2, 1.36, 0.1], hand_r=[0.2, 1.38, -0.1],
                               elbow_l=[0.3, 1.2, -0.4], elbow_r=[0.3, 1.2, 0.4], fist_l=0.3, fist_r=0.3),
    "mirela_lament": pose("stance", hips=[0.04, -0.04, 0], hips_rot=[0, -20, 0], spine=[8, 4, 0], chest=[10, 4, 0], head=[-22, 0, 0],
                          hand_l=[0.3, 1.55, -0.55], hand_r=[0.15, 1.5, 0.55], elbow_l=[0.1, 1.4, -0.9], elbow_r=[0.0, 1.4, 0.9], glow=1.0,
                          **merge(PALM_L, PALM_R)),
    "mirela_keen_load": pose("stance", hips=[-0.04, -0.06, 0], spine=[10, 4, 0], head=[-8, 0, 0], hand_l=[0.2, 1.62, -0.04], hand_r=[0.18, 1.6, 0.05], fist_l=0.3, fist_r=0.3),
    "mirela_keen": pose("stance", hips=[0.06, -0.1, 0], hips_rot=[0, -18, 0], spine=[-16, 4, 0], chest=[-10, 4, 0], head=[-14, 0, 0],
                        hand_l=[-0.3, 1.0, -0.38], hand_r=[-0.32, 0.98, 0.38], elbow_l=[-0.3, 1.2, -0.7], elbow_r=[-0.3, 1.2, 0.7], glow=1.0, fist_l=0, fist_r=0),
    "mirela_float": pose("jump", hips_rot=[-4, -20, 0], spine=[2, 6, 0], head=[-4, 0, 0], foot_l=[0.05, 0.12, -0.08], foot_r=[-0.06, 0.06, 0.09],
                         toe_l=60, toe_r=60, knee_l=[0.9, 0.5, -0.1], knee_r=[0.9, 0.5, 0.1],
                         hand_l=[0.2, 1.32, -0.6], hand_r=[0.1, 1.3, 0.6], elbow_l=[0.0, 1.4, -0.9], elbow_r=[0.0, 1.4, 0.9], **merge(PALM_L, PALM_R)),
    "mirela_float_dive": pose("mirela_float", hips_rot=[-24, -30, 0], spine=[-20, 6, 0], head=[20, 0, 0], hand_r=[0.56, 0.22, 0.0], elbow_r=[0.4, 0.9, 0.5],
                              hand_l=[-0.1, 1.2, -0.4], fingers_r=[1, -0.5, 0], palm_r=[0, -1, 0]),
    "mirela_veil_step": pose("stance", hips=[-0.08, -0.06, 0], spine=[12, 6, 0], head=[2, 6, 0], hand_l=[0.5, 1.3, -0.2], hand_r=[0.4, 1.2, 0.2],
                             foot_l=[0.3, 0.04, -0.06], free_l=1, toe_l=40, **merge(PALM_L, PALM_R)),
}

# --- Bram Ketch: a boxer with a baton in the right fist ------------------------------------------
F["bram"] = {
    "stance": pose("stance", hips=[0, -0.1, 0], hips_rot=[0, -36, 0], spine=[-8, 12, 0], chest=[-6, 12, 0], head=[16, 10, 0],
                   foot_l=[0.22, 0, -0.1], yaw_l=-14, foot_r=[-0.22, 0, 0.15], yaw_r=-55,
                   hand_l=[0.36, 1.44, -0.08], elbow_l=[0.2, 1.0, -0.5], hand_r=[0.2, 1.38, 0.1], elbow_r=[0.0, 0.95, 0.5]),
    "intro": pose("stance", hips=[0, -0.03, 0], hips_rot=[0, -14, 0], spine=[-2, 4, 0], head=[8, 4, 0], foot_l=[0.1, 0, -0.12], foot_r=[-0.08, 0, 0.13],
                  hand_r=[0.32, 1.12, 0.0], elbow_r=[0.0, 1.0, 0.5], hand_l=[0.3, 1.08, -0.06], elbow_l=[0.0, 1.0, -0.5], fist_l=0),
    "win": pose("stance", hips=[0, -0.03, 0], hips_rot=[0, -18, 0], spine=[2, 4, 0], head=[-6, 6, 0], foot_l=[0.1, 0, -0.12], foot_r=[-0.1, 0, 0.14],
                hand_r=[0.1, 1.95, 0.15], elbow_r=[0.3, 1.6, 0.6], hand_l=[0.0, 1.0, -0.26], elbow_l=[0.0, 1.1, -0.7]),
    "bram_hook_load": pose("stance", hips_rot=[0, -50, 0], hand_l=[0.2, 1.44, -0.3], elbow_l=[0.1, 1.4, -0.8]),
    "bram_hook_hit": pose("stance", hips=[0.06, -0.12, 0], hips_rot=[0, 4, 0], spine=[-8, 6, 0], head=[16, -4, 0], hand_l=[0.5, 1.46, 0.16],
                          elbow_l=[0.3, 1.46, -0.7], hand_r=[0.2, 1.38, 0.12]),
    "bram_body_hit": pose("lp2_hit", hips=[0.1, -0.2, 0], spine=[-16, 14, 0], hand_r=[0.62, 1.05, 0.0]),
    "bram_baton_load": pose("stance", hips=[0, -0.18, 0], hips_rot=[0, -60, 0], spine=[-10, 18, 0], hand_r=[-0.2, 0.8, 0.3], elbow_r=[-0.3, 0.9, 0.6]),
    "bram_baton_hit": pose("stance", hips=[0.08, -0.04, 0], hips_rot=[0, 4, 0], spine=[6, 6, 0], head=[-6, 0, 0], hand_r=[0.46, 1.72, 0.0], elbow_r=[0.4, 1.3, 0.6]),
    "bram_flare_load": pose("stance", hips_rot=[0, -50, 0], hand_r=[0.0, 0.96, 0.2], elbow_r=[-0.2, 1.1, 0.5]),
    "bram_flare_high": pose("stance", hips=[0.06, -0.06, 0], hips_rot=[0, -4, 0], spine=[4, 6, 0], head=[-14, 0, 0], hand_r=[0.42, 1.86, 0.05], elbow_r=[0.3, 1.4, 0.6], fist_r=0.2),
    "bram_flare_low": pose("crouch", hips=[0.08, -0.36, 0], spine=[-18, 6, 0], hand_r=[0.56, 0.35, 0.05], elbow_r=[0.2, 0.6, 0.6], fist_r=0.2),
    "bram_trip_load": pose("crouch", hips_rot=[0, -60, 0], hand_r=[-0.25, 0.6, 0.3], elbow_r=[-0.3, 0.7, 0.6]),
    "bram_trip_hit": pose("crouch", hips=[0.06, -0.48, 0], hips_rot=[0, 10, 0], spine=[-22, 6, 0], hand_r=[0.75, 0.16, 0.0], elbow_r=[0.3, 0.5, 0.6]),
    "bram_rush_load": pose("stance", hips=[0, -0.18, 0], hips_rot=[0, -62, 0], spine=[-20, 10, 0], head=[22, 30, 0], hand_l=[0.3, 1.2, -0.1]),
    "bram_rush": pose("stance", hips=[0.18, -0.18, 0], hips_rot=[-6, -66, 0], spine=[-26, 12, 0], head=[24, 40, 0], foot_l=[0.32, 0, -0.14], foot_r=[-0.42, 0, 0.18],
                      hand_l=[0.34, 1.25, -0.08], hand_r=[0.3, 1.2, 0.12]),
    "bram_stand_down": pose("stance", hips=[-0.02, -0.12, 0], hips_rot=[0, -20, 0], spine=[-4, 6, 0], head=[10, 2, 0],
                            hand_r=[0.36, 1.56, 0.06], hand_l=[0.36, 1.52, -0.16], elbow_r=[0.2, 1.2, 0.6], elbow_l=[0.2, 1.2, -0.6]),
    "bram_swing_load": pose("stance", hips=[-0.04, -0.1, 0], hips_rot=[0, -95, 0], spine=[0, 30, 0], head=[0, 60, 0],
                            hand_r=[-0.3, 1.42, 0.35], hand_l=[-0.24, 1.36, 0.28], elbow_r=[-0.3, 1.6, 0.7], elbow_l=[-0.1, 1.2, -0.3]),
    "bram_swing_hit": pose("stance", hips=[0.08, -0.08, 0], hips_rot=[0, 30, 0], spine=[-4, -14, 0], head=[0, -20, 0],
                           hand_r=[0.42, 1.32, -0.34], hand_l=[0.38, 1.3, -0.3], elbow_r=[0.3, 1.2, 0.6], elbow_l=[0.2, 1.2, -0.6]),
}

# --- Hollis Fathom: a heavy diving suit, harpoon gun in the right hand -------------------------
F["hollis"] = {
    "stance": pose("stance", hips=[0, -0.08, 0], hips_rot=[0, -26, 0], spine=[-10, 8, 0], chest=[-6, 8, 0], head=[10, 6, 0],
                   foot_l=[0.2, 0, -0.15], yaw_l=-14, foot_r=[-0.2, 0, 0.18], yaw_r=-45,
                   hand_l=[0.4, 1.32, -0.12], elbow_l=[0.1, 1.0, -0.6], hand_r=[0.16, 1.06, 0.22], elbow_r=[-0.2, 1.0, 0.6]),
    "intro": pose("stance", hips=[0, -0.03, 0], hips_rot=[0, -12, 0], spine=[-2, 4, 0], head=[-8, 14, 0], foot_l=[0.1, 0, -0.14], foot_r=[-0.08, 0, 0.15],
                  hand_r=[0.0, 1.62, 0.26], elbow_r=[0.0, 1.4, 0.6], hand_l=[0.1, 1.0, -0.3]),
    "win": pose("stance", hips=[0, -0.03, 0], hips_rot=[0, -14, 0], spine=[4, 4, 0], head=[-10, 4, 0], hand_r=[0.15, 2.0, 0.12], elbow_r=[0.3, 1.6, 0.6],
                hand_l=[0.15, 1.1, -0.3]),
    "hollis_butt_load": pose("stance", spine=[14, 4, 0], chest=[10, 4, 0], head=[-14, 0, 0], hand_l=[0.3, 1.3, -0.25], hand_r=[0.1, 1.1, 0.28]),
    "hollis_butt_hit": pose("stance", hips=[0.12, -0.14, 0], spine=[-26, 4, 0], chest=[-16, 4, 0], head=[24, 0, 0], hand_l=[0.2, 1.1, -0.35], hand_r=[0.0, 1.0, 0.35]),
    "hollis_boot_hit": pose("lk_hit", hips_rot=[6, -40, 0], spine=[8, 8, 0], foot_r=[0.8, 0.74, 0.0], knee_r=[0.9, 0.8, 0.2]),
    "hollis_aim": pose("stance", hips_rot=[0, -40, 0], spine=[-4, 14, 0], head=[6, 20, 0], hand_r=[0.46, 1.36, 0.05], elbow_r=[0.1, 1.2, 0.6],
                       hand_l=[0.36, 1.3, -0.05], elbow_l=[0.1, 1.0, -0.6]),
    "hollis_fire": pose("hollis_aim", hips=[-0.06, -0.08, 0], spine=[6, 14, 0], head=[-4, 20, 0], hand_r=[0.38, 1.44, 0.05], hand_l=[0.3, 1.38, -0.05]),
    "hollis_reel": pose("stance", hips=[-0.08, -0.16, 0], hips_rot=[0, -40, 0], spine=[10, 14, 0], hand_r=[0.06, 1.2, 0.2], hand_l=[0.2, 1.2, -0.05]),
    "hollis_gulp": pose("stance", hips=[0.06, -0.14, 0], spine=[-22, 0, 0], chest=[-12, 0, 0], head=[20, 0, 0], hand_l=[0.2, 1.3, -0.6], hand_r=[0.1, 1.3, 0.6],
                        elbow_l=[0.0, 1.4, -0.9], elbow_r=[0.0, 1.4, 0.9]),
    "hollis_signal_load": pose("stance", hand_l=[0.1, 2.02, -0.2], elbow_l=[0.1, 1.7, -0.6], spine=[6, 0, 0], head=[-16, 0, 0], fist_l=0),
    "hollis_signal": pose("stance", hips=[0.04, -0.12, 0], spine=[-14, 0, 0], hand_l=[0.7, 1.15, -0.1], elbow_l=[0.3, 1.1, -0.6], fist_l=0,
                          fingers_l=[1, 0, 0], palm_l=[0, -1, 0]),
    "hollis_bubble_load": pose("stance", hand_l=[0.32, 1.6, -0.08], hand_r=[0.2, 1.4, 0.15], fist_l=0, head=[-6, 0, 0]),
    "hollis_bubble": pose("stance", hips=[0.06, -0.08, 0], hand_l=[0.64, 1.32, -0.04], elbow_l=[0.3, 1.0, -0.6], spine=[-6, 4, 0], **PALM_L),
    "hollis_vent_load": pose("crouch", hand_l=[0.2, 1.1, 0.05], hand_r=[0.2, 1.1, -0.05], spine=[-24, 0, 0], head=[20, 0, 0]),
    "hollis_vent": pose("stance", hips=[0, -0.06, 0], hips_rot=[0, -10, 0], spine=[10, 0, 0], chest=[12, 0, 0], head=[-14, 0, 0],
                        hand_l=[0.15, 1.3, -0.65], hand_r=[0.0, 1.3, 0.65], elbow_l=[0.0, 1.5, -0.9], elbow_r=[0.0, 1.5, 0.9], fist_l=0, fist_r=0),
}

# --- The Slopbat: a hunched predator, claws open, wings spread for its big moves ---------------
F["slopbat"] = {
    "stance": pose("stance", hips=[0, -0.12, 0], hips_rot=[0, -20, 0], spine=[-26, 6, 0], chest=[-12, 6, 0], neck=[10, 0, 0], head=[16, 4, 0],
                   foot_l=[0.26, 0, -0.18], yaw_l=-10, foot_r=[-0.26, 0, 0.22], yaw_r=-40, knee_l=[0.9, 0.4, -0.5], knee_r=[0.6, 0.4, 0.7],
                   hand_l=[0.52, 1.32, -0.22], elbow_l=[0.3, 1.5, -0.7], hand_r=[0.36, 1.2, 0.32], elbow_r=[0.1, 1.4, 0.8], fist_l=0.35, fist_r=0.35),
    "intro": pose("stance", hips=[0, -0.04, 0], spine=[4, 0, 0], chest=[8, 0, 0], head=[-14, 0, 0],
                  hand_l=[0.1, 1.85, -0.7], hand_r=[0.05, 1.85, 0.7], elbow_l=[0.0, 1.6, -1.0], elbow_r=[0.0, 1.6, 1.0], fist_l=0, fist_r=0, prop=1.0),
    "win": pose("intro"),
    "bat_claw_load": pose("stance", hand_r=[-0.1, 1.75, 0.45], elbow_r=[-0.2, 1.7, 0.8], hips_rot=[0, -45, 0], fist_r=0),
    "bat_claw_hit": pose("stance", hips=[0.1, -0.14, 0], hips_rot=[0, 10, 0], spine=[-30, -6, 0], hand_r=[0.78, 1.3, -0.05], elbow_r=[0.4, 1.5, 0.6], fist_r=0.2),
    "bat_claw2_load": pose("stance", hand_l=[-0.05, 1.5, -0.5], elbow_l=[-0.2, 1.6, -0.8], hips_rot=[0, 10, 0], fist_l=0),
    "bat_claw2_hit": pose("stance", hips=[0.1, -0.18, 0], hips_rot=[0, -55, 0], spine=[-32, 14, 0], hand_l=[0.8, 0.95, 0.1], elbow_l=[0.4, 1.3, -0.6], fist_l=0.2),
    "bat_low_hit": pose("crouch", hips=[0.08, -0.5, 0], spine=[-36, 6, 0], hand_r=[0.95, 0.12, 0.0], elbow_r=[0.4, 0.6, 0.6], fist_r=0.2),
    "bat_rise_hit": pose("stance", hips=[0.1, 0.0, 0], hips_rot=[-6, 10, 0], spine=[8, 16, 0], head=[-14, 0, 0], hand_r=[0.4, 2.05, 0.0], elbow_r=[0.5, 1.6, 0.5], fist_r=0),
    "bat_spit_load": pose("stance", spine=[6, 0, 0], chest=[8, 0, 0], neck=[-10, 0, 0], head=[-24, 0, 0], prop=0.4),
    "bat_spit": pose("stance", hips=[0.06, -0.12, 0], spine=[-34, 0, 0], neck=[16, 0, 0], head=[14, 0, 0], prop=0.4),
    "bat_gust_load": pose("intro", hips=[-0.06, -0.08, 0], spine=[10, 0, 0]),
    "bat_gust": pose("stance", hips=[0.08, -0.16, 0], spine=[-30, 0, 0], hand_l=[0.6, 1.2, -0.6], hand_r=[0.55, 1.15, 0.6], fist_l=0, fist_r=0, prop=1.0),
    "bat_rise": pose("crouch", hips=[0, -0.5, 0], hand_l=[0.1, 1.6, -0.7], hand_r=[0.1, 1.6, 0.7], prop=1.0, fist_l=0, fist_r=0),
    "bat_fly": pose("jump", hips_rot=[-10, -10, 0], spine=[-10, 0, 0], head=[10, 0, 0], foot_l=[0.0, 0.1, -0.15], foot_r=[-0.1, 0.05, 0.17], toe_l=50, toe_r=50,
                    hand_l=[0.1, 1.6, -0.75], hand_r=[0.05, 1.6, 0.75], elbow_l=[0.0, 1.7, -1.0], elbow_r=[0.0, 1.7, 1.0], prop=1.0, fist_l=0.3, fist_r=0.3),
    "bat_dive": pose("bat_fly", hips_rot=[-40, -10, 0], spine=[-24, 0, 0], head=[20, 0, 0], hand_l=[0.5, 0.4, -0.25], hand_r=[0.5, 0.4, 0.25],
                     elbow_l=[0.3, 1.0, -0.6], elbow_r=[0.3, 1.0, 0.6], foot_l=[-0.3, 0.6, -0.15], foot_r=[-0.35, 0.55, 0.15], prop=0.6),
    "bat_stuck": pose("crouch", hips=[0, -0.6, 0], spine=[-40, 0, 0], neck=[10, 0, 0], head=[20, 0, 0], hand_l=[0.5, 0.02, -0.3], hand_r=[0.48, 0.02, 0.32],
                      fist_l=0, fist_r=0, prop=0.2),
    "bat_storm_load": pose("intro", hips=[0, -0.14, 0], spine=[12, 0, 0], head=[-24, 0, 0], hand_l=[0.0, 2.1, -0.45], hand_r=[0.0, 2.1, 0.45], glow=1.0, prop=1.0),
    "bat_storm": pose("intro", hips=[0, -0.2, 0], spine=[-8, 0, 0], head=[10, 0, 0], hand_l=[0.1, 1.2, -0.9], hand_r=[0.05, 1.2, 0.9], glow=1.0, prop=1.0),
    "bat_bite_hold": pose("grab_hold", spine=[-20, 0, 0], head=[10, 0, 0]),
    "bat_bite": pose("grab_hold", hips=[0.06, -0.14, 0], spine=[-30, 0, 0], neck=[16, 0, 0], head=[20, 0, 0]),
    "bat_toss": pose("throw_toss", spine=[-26, 0, 0]),
}

# --- The stranger: still, with his hands behind his back -----------------------------------------
F["daxion"] = {
    "stance": pose("stance", hips=[0, -0.01, 0], hips_rot=[0, -12, 0], spine=[2, 4, 0], chest=[2, 4, 0], neck=[0, 0, 0], head=[-2, 10, 0],
                   foot_l=[0.1, 0, -0.12], yaw_l=-8, foot_r=[-0.1, 0, 0.13], yaw_r=-24, knee_l=[0.9, 0.5, -0.2], knee_r=[0.8, 0.5, 0.3],
                   hand_l=[-0.17, 1.0, -0.05], elbow_l=[-0.4, 1.1, -0.5], hand_r=[-0.17, 1.02, 0.05], elbow_r=[-0.4, 1.1, 0.5], fist_l=0.4, fist_r=0.2),
    "intro": pose("stance"),
    "win": pose("stance"),
    "dax_sidestep": pose("stance", hips=[-0.05, -0.04, 0], hips_rot=[0, -30, 0], spine=[4, 6, 0], head=[-4, 26, 0], foot_l=[0.18, 0, -0.1], foot_r=[-0.2, 0, 0.16]),
    "dax_touch": pose("stance", hips=[0.04, -0.02, 0], hips_rot=[0, -30, 0], spine=[0, 10, 0], head=[2, 16, 0], hand_r=[0.62, 1.36, 0.0], elbow_r=[0.3, 1.1, 0.6],
                      fist_r=0.55, fingers_r=[1, 0.1, 0], palm_r=[0, 0, -1]),
    "dax_study": pose("dax_touch", head=[8, 22, 6], spine=[-2, 12, 0]),
    "dax_punch_load": pose("stance", hips=[0.02, -0.06, 0], hips_rot=[0, -38, 0], spine=[-2, 12, 0], hand_r=[0.12, 1.32, 0.12], elbow_r=[-0.1, 1.0, 0.5], fist_r=1,
                           foot_l=[0.24, 0, -0.1], foot_r=[-0.22, 0, 0.16]),
    "dax_punch": pose("stance", hips=[0.12, -0.1, 0], hips_rot=[0, 6, 0], spine=[-4, 2, 0], head=[2, 0, 0], hand_r=[0.74, 1.4, 0.0], elbow_r=[0.3, 1.2, 0.6], fist_r=1,
                      foot_l=[0.3, 0, -0.1], foot_r=[-0.24, 0, 0.16], yaw_r=-20),
    "dax_palm_load": pose("stance", hand_r=[0.2, 1.3, 0.1], fist_r=0),
    "dax_palm": pose("stance", hips=[0.08, -0.06, 0], hips_rot=[0, -6, 0], hand_r=[0.7, 1.32, 0.0], elbow_r=[0.3, 1.1, 0.6], **PALM_R),
}


def heights():
    out = {}
    for fid in F:
        d = json.load(open(os.path.join(ROOT, "data", "fighters", fid + ".json")))
        out[fid] = float(d.get("stats", {}).get("height", 1.8))
    return out


if __name__ == "__main__":
    os.makedirs(os.path.join(ROOT, "data", "poses"), exist_ok=True)
    hs = heights()
    for fid, poses in F.items():
        k = hs[fid] / 1.8
        out = {}
        for name, p in poses.items():
            q = dict(p)
            for key in POS_KEYS:
                if key in q:
                    q[key] = [round(float(v) * k, 4) for v in q[key]]
            out[name] = q
        path = os.path.join(ROOT, "data", "poses", fid + ".json")
        with open(path, "w") as f:
            f.write("{\n")
            items = list(out.items())
            for i, (name, v) in enumerate(items):
                f.write('\t"%s": %s%s\n' % (name, json.dumps(v), "," if i < len(items) - 1 else ""))
            f.write("}\n")
        print("wrote %d poses for %s (x%.2f)" % (len(out), fid, k))
