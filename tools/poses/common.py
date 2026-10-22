#!/usr/bin/env python3
"""Source of data/poses_common.json: the shared key poses for a 1.8 m body (scaled to each
fighter). Written as Python so related poses can share pieces; run to regenerate the JSON.

Conventions (see scripts/anim/pose.gd): positions are [forward, up, right] metres from the root
between the feet, angles are degrees about x (pitch, + leans back), y (yaw, + turns left) and z.
The stance is orthodox (left side leading); FighterView mirrors every pose for the right side.
"""
import json
import os

OUT = os.path.join(os.path.dirname(__file__), "..", "..", "data", "poses_common.json")

P = {}

# --- Standing ------------------------------------------------------------------------------------
P["stance"] = {"hips": [0.0, -0.08, 0.0], "hips_rot": [0, -32, 0], "spine": [-5, 10, 0], "chest": [-4, 10, 0], "neck": [0, 4, 0], "head": [6, 8, 0],
               "foot_l": [0.22, 0.0, -0.1], "yaw_l": -12, "foot_r": [-0.24, 0.0, 0.15], "yaw_r": -58,
               "knee_l": [0.9, 0.5, -0.3], "knee_r": [0.6, 0.5, 0.6],
               "hand_l": [0.34, 1.4, -0.08], "elbow_l": [0.1, 0.9, -0.6], "hand_r": [0.2, 1.44, 0.07], "elbow_r": [0.0, 0.9, 0.6]}
P["crouch"] = {"base": "stance", "hips": [0.0, -0.42, 0.0], "hips_rot": [0, -24, 0], "spine": [-16, 8, 0], "chest": [-8, 6, 0], "head": [20, 4, 0],
               "foot_l": [0.26, 0.0, -0.15], "foot_r": [-0.22, 0.0, 0.18], "knee_l": [0.9, 0.4, -0.5], "knee_r": [0.8, 0.3, 0.6],
               "hand_l": [0.36, 1.0, -0.1], "hand_r": [0.26, 1.02, 0.08], "elbow_l": [0.1, 0.5, -0.6], "elbow_r": [0.0, 0.5, 0.6]}
# Block: forearms crossed upright in front of the face, chin tucked.
P["block"] = {"base": "stance", "hips": [-0.02, -0.1, 0.0], "hips_rot": [0, -22, 0], "spine": [-2, 6, 0], "chest": [0, 6, 0], "head": [14, 2, 0],
              "hand_l": [0.26, 1.58, 0.06], "elbow_l": [0.3, 1.25, -0.3], "hand_r": [0.27, 1.55, -0.07], "elbow_r": [0.3, 1.22, 0.32],
              "fingers_l": [0, 1, 0.4], "palm_l": [-1, 0, 0], "fingers_r": [0, 1, -0.4], "palm_r": [-1, 0, 0]}
P["block_hit"] = {"base": "block", "hips": [-0.07, -0.12, 0.0], "spine": [4, 6, 0], "chest": [6, 6, 0], "head": [10, 2, 0]}
P["block_low"] = {"base": "crouch", "head": [24, 2, 0], "hand_l": [0.3, 0.98, 0.05], "elbow_l": [0.3, 0.7, -0.3], "hand_r": [0.3, 0.96, -0.06], "elbow_r": [0.3, 0.68, 0.32],
                  "fingers_l": [0, 1, 0.4], "palm_l": [-1, 0, 0], "fingers_r": [0, 1, -0.4], "palm_r": [-1, 0, 0]}
P["block_low_hit"] = {"base": "block_low", "hips": [-0.07, -0.43, 0.0], "spine": [-8, 6, 0]}

# --- Airborne -----------------------------------------------------------------------------------
P["jump"] = {"base": "stance", "hips": [0.0, -0.1, 0.0], "hips_rot": [-6, -20, 0], "spine": [-6, 6, 0], "chest": [-4, 6, 0], "head": [8, 4, 0],
             "foot_l": [0.12, 0.42, -0.12], "foot_r": [-0.1, 0.32, 0.14], "free_l": 1, "free_r": 1, "toe_l": 35, "toe_r": 35,
             "knee_l": [0.9, 0.6, -0.2], "knee_r": [0.8, 0.5, 0.25],
             "hand_l": [0.3, 1.45, -0.12], "hand_r": [0.18, 1.48, 0.1]}
# The somersault: a tight ball (FighterView turns the whole body over the jump).
P["jump_tuck"] = {"base": "jump", "hips": [0.0, 0.05, 0.0], "hips_rot": [-25, -10, 0], "spine": [-30, 0, 0], "chest": [-20, 0, 0], "neck": [-10, 0, 0], "head": [-5, 0, 0],
                  "foot_l": [0.25, 0.62, -0.1], "foot_r": [0.2, 0.6, 0.12], "knee_l": [0.9, 1.0, -0.2], "knee_r": [0.9, 1.0, 0.2],
                  "hand_l": [0.36, 0.9, -0.14], "hand_r": [0.34, 0.9, 0.14], "elbow_l": [0.3, 1.2, -0.5], "elbow_r": [0.3, 1.2, 0.5]}
P["jk_load"] = {"base": "jump", "foot_r": [0.1, 0.5, 0.1], "knee_r": [0.9, 0.8, 0.2], "hips_rot": [8, -40, 0]}
# Angled jump kick: the lead... the rear leg drives down and forward, the other tucked.
P["jk_hit"] = {"base": "jump", "hips": [0.05, -0.05, 0.0], "hips_rot": [18, -55, 0], "spine": [10, 10, 0], "chest": [6, 10, 0], "head": [-4, 20, 0],
               "foot_r": [0.72, 0.3, 0.0], "knee_r": [0.9, 0.9, 0.1], "toe_r": 10,
               "foot_l": [0.0, 0.5, -0.12], "knee_l": [0.9, 0.6, -0.2],
               "hand_l": [0.1, 1.35, -0.25], "hand_r": [-0.05, 1.25, 0.25]}
P["jkn_load"] = {"base": "jump", "foot_l": [0.15, 0.55, -0.1], "knee_l": [0.9, 0.8, -0.1]}
P["jkn_hit"] = {"base": "jump", "hips_rot": [14, -40, 0], "spine": [8, 8, 0], "foot_l": [0.72, 0.62, -0.02], "knee_l": [0.9, 1.0, 0.0], "toe_l": 5,
                "foot_r": [-0.05, 0.4, 0.14], "hand_l": [0.0, 1.4, -0.25], "hand_r": [-0.05, 1.3, 0.25]}
P["jp_load"] = {"base": "jump", "hand_r": [0.0, 1.55, 0.2], "elbow_r": [-0.2, 1.3, 0.6]}
P["jp_hit"] = {"base": "jump", "hips_rot": [-8, -50, 0], "spine": [-14, 10, 0], "chest": [-10, 14, 0], "hand_r": [0.6, 0.86, 0.0], "elbow_r": [0.2, 1.2, 0.6],
               "hand_l": [0.1, 1.3, -0.2], "fist_r": 1}

# --- Running ------------------------------------------------------------------------------------
P["run_a"] = {"base": "stance", "hips": [0.06, -0.06, 0.0], "hips_rot": [-14, -6, 0], "spine": [-10, 4, 0], "chest": [-6, 4, 0], "head": [14, 0, 0],
              "foot_l": [0.42, 0.04, -0.1], "yaw_l": 0, "foot_r": [-0.38, 0.2, 0.1], "yaw_r": 0, "free_l": 1, "free_r": 1, "toe_r": 40,
              "knee_l": [0.9, 0.5, -0.1], "knee_r": [0.3, 0.3, 0.1],
              "hand_l": [-0.12, 1.08, -0.2], "elbow_l": [-0.4, 1.2, -0.3], "hand_r": [0.3, 1.3, 0.16], "elbow_r": [0.0, 1.0, 0.5]}
P["run_b"] = {"base": "run_a", "foot_l": [-0.38, 0.2, -0.1], "foot_r": [0.42, 0.04, 0.1], "toe_l": 40, "toe_r": 0,
              "knee_l": [0.3, 0.3, -0.1], "knee_r": [0.9, 0.5, 0.1],
              "hand_l": [0.3, 1.3, -0.16], "elbow_l": [0.0, 1.0, -0.5], "hand_r": [-0.12, 1.08, 0.2], "elbow_r": [-0.4, 1.2, 0.3]}

# --- Punches ------------------------------------------------------------------------------------
P["hp_load"] = {"base": "stance", "hips_rot": [0, -36, 0], "hand_l": [0.3, 1.46, -0.06]}
P["hp_hit"] = {"base": "stance", "hips": [0.06, -0.08, 0.0], "hips_rot": [0, -48, 0], "spine": [-4, 14, 0], "chest": [-4, 16, 0], "head": [4, 12, 0],
               "hand_l": [0.68, 1.5, 0.02], "elbow_l": [0.3, 1.3, -0.6], "hand_r": [0.16, 1.44, 0.1]}
P["hp2_load"] = {"base": "stance", "hips_rot": [0, -28, 0], "hand_r": [0.14, 1.46, 0.1]}
P["hp2_hit"] = {"base": "stance", "hips": [0.08, -0.08, 0.0], "hips_rot": [0, 6, 0], "spine": [-4, 14, 0], "chest": [-4, 16, 0], "head": [4, -4, 0],
                "foot_r": [-0.2, 0.0, 0.15], "yaw_r": -30,
                "hand_r": [0.7, 1.5, -0.02], "elbow_r": [0.3, 1.3, 0.6], "hand_l": [0.2, 1.42, -0.12]}
P["lp_load"] = {"base": "stance", "hips_rot": [0, -38, 0], "hand_l": [0.26, 1.2, -0.08]}
P["lp_hit"] = {"base": "stance", "hips": [0.08, -0.12, 0.0], "hips_rot": [0, -50, 0], "spine": [-10, 14, 0], "chest": [-6, 14, 0], "head": [10, 12, 0],
               "hand_l": [0.66, 1.16, 0.0], "elbow_l": [0.3, 0.9, -0.6], "hand_r": [0.18, 1.4, 0.1]}
P["lp2_load"] = {"base": "stance", "hand_r": [0.12, 1.22, 0.12]}
P["lp2_hit"] = {"base": "stance", "hips": [0.1, -0.12, 0.0], "hips_rot": [0, 4, 0], "spine": [-10, 14, 0], "chest": [-6, 14, 0], "head": [10, -2, 0],
                "foot_r": [-0.2, 0.0, 0.15], "yaw_r": -30,
                "hand_r": [0.68, 1.16, 0.0], "elbow_r": [0.3, 0.9, 0.6], "hand_l": [0.22, 1.4, -0.12]}

# --- Kicks --------------------------------------------------------------------------------------
P["hk_chamber"] = {"base": "stance", "hips": [0.0, -0.04, 0.0], "hips_rot": [6, -60, 0], "spine": [6, 10, 0], "chest": [4, 10, 0], "head": [-2, 20, 0],
                   "foot_l": [0.12, 0.0, -0.06], "yaw_l": -60, "foot_r": [0.25, 0.85, 0.05], "free_r": 1, "knee_r": [0.9, 1.0, 0.2], "toe_r": 30}
P["hk_hit"] = {"base": "hk_chamber", "hips": [0.02, -0.02, 0.0], "hips_rot": [16, -75, 0], "spine": [14, 14, 0], "chest": [8, 14, 0], "head": [-10, 28, 0],
               "foot_r": [0.86, 1.4, 0.0], "knee_r": [0.9, 1.4, 0.4], "toe_r": 10,
               "hand_l": [0.2, 1.42, -0.2], "hand_r": [-0.2, 1.15, 0.3]}
P["lk_chamber"] = {"base": "stance", "hips_rot": [0, -45, 0], "foot_r": [0.1, 0.4, 0.06], "free_r": 1, "knee_r": [0.9, 0.7, 0.2], "toe_r": 30}
P["lk_hit"] = {"base": "stance", "hips": [0.04, -0.06, 0.0], "hips_rot": [8, -60, 0], "spine": [6, 10, 0], "head": [-2, 18, 0],
               "foot_r": [0.82, 0.5, 0.02], "knee_r": [0.9, 0.7, 0.3], "free_r": 1, "toe_r": 20,
               "hand_l": [0.3, 1.38, -0.15], "hand_r": [0.0, 1.25, 0.25]}
# Roundhouse: a spinning heel that comes round high and knocks the opponent down.
P["rh_turn"] = {"base": "stance", "hips": [-0.02, -0.06, 0.0], "hips_rot": [4, 60, 0], "spine": [0, 30, 0], "chest": [0, 30, 0], "head": [0, -40, 0],
                "foot_l": [0.18, 0.0, -0.02], "yaw_l": 70, "foot_r": [-0.1, 0.3, 0.2], "free_r": 1, "knee_r": [0.3, 0.6, 0.9],
                "hand_l": [0.1, 1.35, -0.25], "hand_r": [0.0, 1.35, 0.25]}
P["rh_hit"] = {"base": "rh_turn", "hips": [0.0, -0.04, 0.0], "hips_rot": [14, 96, 18], "spine": [10, 10, 0], "chest": [6, 10, 0], "head": [-10, -60, 0],
               "foot_r": [0.86, 1.42, 0.08], "knee_r": [0.9, 1.4, 0.3], "toe_r": 5,
               "hand_l": [-0.1, 1.3, -0.3], "hand_r": [0.05, 1.2, 0.35]}
P["rh_follow"] = {"base": "stance", "hips_rot": [0, 140, 0], "spine": [0, 10, 0], "head": [0, -60, 0],
                  "foot_l": [0.14, 0.0, -0.05], "yaw_l": 120, "foot_r": [0.4, 0.0, 0.1], "yaw_r": 100}
# Sweep: dropping low and spinning the leg round along the floor.
P["sweep_load"] = {"base": "crouch", "hips": [0.0, -0.5, 0.0], "hips_rot": [0, 30, 0], "spine": [-20, 20, 0], "head": [20, -20, 0],
                   "hand_l": [0.3, 0.25, -0.25], "fist_l": 0, "foot_r": [-0.2, 0.05, 0.4], "free_r": 1}
P["sweep_hit"] = {"base": "sweep_load", "hips": [0.0, -0.52, 0.0], "hips_rot": [6, 80, 0], "spine": [-22, 10, 0], "head": [20, -60, 0],
                  "foot_l": [0.08, 0.0, -0.2], "knee_l": [0.6, 0.5, -0.9],
                  "foot_r": [0.9, 0.06, 0.05], "knee_r": [0.9, 0.3, 0.4], "toe_r": 0,
                  "hand_l": [0.1, 0.25, -0.35], "hand_r": [-0.25, 0.3, 0.2], "fist_r": 0}
P["sweep_follow"] = {"base": "crouch", "hips_rot": [0, 140, 0], "head": [20, -60, 0], "foot_r": [0.3, 0.0, 0.25], "yaw_r": 90}

# --- Uppercut -------------------------------------------------------------------------------------
P["upper_load"] = {"base": "crouch", "hips": [0.04, -0.46, 0.0], "hips_rot": [0, -50, 0], "spine": [-24, 12, 0], "head": [24, 10, 0],
                   "hand_r": [-0.04, 0.6, 0.14], "elbow_r": [-0.3, 0.8, 0.5], "hand_l": [0.3, 1.0, -0.1]}
P["upper_hit"] = {"base": "stance", "hips": [0.12, 0.0, 0.0], "hips_rot": [-6, 20, 0], "spine": [10, 26, 0], "chest": [8, 20, 0], "head": [-12, -10, 0],
                  "foot_l": [0.25, 0.0, -0.1], "foot_r": [-0.1, 0.0, 0.15], "yaw_r": -20,
                  "hand_r": [0.42, 1.92, -0.02], "elbow_r": [0.6, 1.4, 0.4], "hand_l": [0.08, 1.3, -0.2]}
P["upper_follow"] = {"base": "upper_hit", "hips": [0.1, -0.05, 0.0], "hand_r": [0.3, 1.8, 0.05]}

# --- Crouching attacks ----------------------------------------------------------------------------
P["c_lp_load"] = {"base": "crouch", "hand_l": [0.3, 0.92, -0.08]}
P["c_lp_hit"] = {"base": "crouch", "hips": [0.06, -0.42, 0.0], "hips_rot": [0, -40, 0], "spine": [-14, 14, 0], "hand_l": [0.62, 0.72, 0.0], "elbow_l": [0.3, 0.5, -0.6]}
P["c_lk_load"] = {"base": "crouch", "foot_l": [0.3, 0.06, -0.12], "free_l": 1}
P["c_lk_hit"] = {"base": "crouch", "hips": [-0.04, -0.46, 0.0], "hips_rot": [4, -30, 0], "spine": [-8, 8, 0], "foot_l": [0.86, 0.06, -0.04], "knee_l": [0.9, 0.3, -0.2],
                 "free_l": 1, "toe_l": 10, "foot_r": [-0.25, 0.0, 0.2]}
P["c_hk_load"] = {"base": "crouch", "foot_r": [0.0, 0.3, 0.15], "free_r": 1, "knee_r": [0.9, 0.6, 0.3]}
P["c_hk_hit"] = {"base": "crouch", "hips": [-0.04, -0.4, 0.0], "hips_rot": [10, -70, 0], "spine": [6, 16, 0], "head": [10, 26, 0],
                 "foot_r": [0.84, 0.74, 0.0], "knee_r": [0.9, 0.9, 0.3], "free_r": 1, "toe_r": 10,
                 "foot_l": [0.1, 0.0, -0.12], "yaw_l": -50}

# --- Throws -------------------------------------------------------------------------------------
P["grab_reach"] = {"base": "stance", "hips": [0.06, -0.1, 0.0], "hips_rot": [0, -14, 0], "spine": [-8, 6, 0],
                   "hand_l": [0.55, 1.35, -0.18], "hand_r": [0.55, 1.35, 0.18], "fist_l": 0.4, "fist_r": 0.4,
                   "elbow_l": [0.2, 1.0, -0.6], "elbow_r": [0.2, 1.0, 0.6]}
P["grab_hold"] = {"base": "grab_reach", "hand_l": [0.45, 1.38, -0.14], "hand_r": [0.45, 1.38, 0.14], "fist_l": 0.8, "fist_r": 0.8}
P["throw_lift"] = {"base": "grab_hold", "hips": [0.0, -0.18, 0.0], "spine": [10, 0, 0], "chest": [10, 0, 0], "hand_l": [0.45, 1.75, -0.14], "hand_r": [0.45, 1.75, 0.14]}
P["throw_toss"] = {"base": "stance", "hips": [0.1, -0.12, 0.0], "spine": [-18, 0, 0], "chest": [-10, 0, 0], "hand_l": [0.7, 1.3, -0.2], "hand_r": [0.7, 1.3, 0.2], "fist_l": 0, "fist_r": 0}
P["throw_over"] = {"base": "grab_hold", "hips": [-0.02, -0.15, 0.0], "spine": [16, 0, 0], "chest": [12, 0, 0], "head": [-14, 0, 0],
                   "hand_l": [0.0, 1.95, -0.14], "hand_r": [0.0, 1.95, 0.14]}
P["throw_slam"] = {"base": "stance", "hips": [-0.05, -0.3, 0.0], "spine": [20, 0, 0], "chest": [14, 0, 0], "head": [-10, 0, 0],
                   "hand_l": [-0.5, 1.0, -0.2], "hand_r": [-0.5, 1.0, 0.2], "fist_l": 0.3, "fist_r": 0.3}
P["thrown_lift"] = {"base": "air_hit", "hips": [0.0, 0.25, 0.0], "hips_rot": [30, 0, 0]}
P["thrown_hold"] = {"base": "stance", "hips": [0.0, -0.06, 0.0], "hips_rot": [8, -10, 0], "spine": [10, 0, 0], "chest": [12, 0, 0], "head": [10, 0, 0],
                    "hand_l": [0.32, 1.45, -0.12], "hand_r": [0.3, 1.45, 0.12], "elbow_l": [0.1, 1.0, -0.5], "elbow_r": [0.1, 1.0, 0.5]}

# --- Reactions ----------------------------------------------------------------------------------
P["hit_head"] = {"base": "stance", "hips": [-0.07, -0.08, 0.0], "hips_rot": [4, -22, 0], "spine": [10, 4, -3], "chest": [12, -6, -4], "neck": [12, -6, 0], "head": [26, -10, 8],
                 "hand_l": [0.18, 1.3, -0.25], "hand_r": [0.05, 1.32, 0.22]}
P["hit_body"] = {"base": "stance", "hips": [-0.08, -0.12, 0.0], "spine": [-14, 4, 0], "chest": [-8, 2, 0], "head": [14, 4, 0],
                 "hand_l": [0.22, 1.15, -0.12], "hand_r": [0.15, 1.2, 0.15]}
P["hit_gut"] = {"base": "stance", "hips": [-0.1, -0.14, 0.0], "hips_rot": [-6, -20, 0], "spine": [-26, 4, 0], "chest": [-16, 4, 0], "head": [10, 0, 0],
                "hand_l": [0.22, 1.0, -0.08], "hand_r": [0.2, 1.03, 0.08], "elbow_l": [0.3, 0.8, -0.5], "elbow_r": [0.3, 0.8, 0.5]}
P["hit_low"] = {"base": "crouch", "hips": [-0.06, -0.44, 0.0], "spine": [-4, 6, 4], "chest": [2, 6, 4], "head": [6, 4, -6],
                "hand_l": [0.25, 1.0, -0.3], "hand_r": [0.12, 1.1, 0.25]}
P["crumple"] = {"base": "stance", "hips": [0.05, -0.52, 0.0], "hips_rot": [-10, -10, 6], "spine": [-22, 4, 4], "chest": [-12, 4, 6], "neck": [-10, 0, 0], "head": [-16, 10, 14],
                "foot_l": [0.15, 0.0, -0.14], "foot_r": [-0.12, 0.0, 0.16], "hand_l": [0.3, 0.55, -0.25], "hand_r": [0.2, 0.5, 0.25],
                "elbow_l": [0.2, 0.8, -0.6], "elbow_r": [0.2, 0.8, 0.6]}
P["dazed"] = {"base": "stance", "hips": [-0.03, -0.1, 0.0], "hips_rot": [4, -10, 0], "spine": [4, 0, 4], "chest": [6, 0, 6], "head": [18, 10, 14],
              "hand_l": [0.1, 1.0, -0.3], "hand_r": [0.0, 0.98, 0.3], "fist_l": 0.3, "fist_r": 0.3, "elbow_l": [0.0, 0.8, -0.6], "elbow_r": [0.0, 0.8, 0.6]}
P["dizzy"] = {"base": "dazed", "hips": [0.0, -0.12, 0.0], "spine": [8, 0, 0], "head": [24, 0, 0], "hand_l": [0.05, 0.9, -0.28], "hand_r": [0.0, 0.88, 0.28],
              "fist_l": 0.1, "fist_r": 0.1}
P["throw_break"] = {"base": "stance", "hips": [-0.08, -0.08, 0.0], "spine": [4, 8, 0], "chest": [6, 9, 0], "hand_l": [0.42, 1.3, -0.3], "hand_r": [0.42, 1.3, 0.22], "fist_l": 0, "fist_r": 0}
P["air_hit"] = {"base": "stance", "hips": [0.0, -0.3, 0.0], "hips_rot": [40, -10, 0], "spine": [16, 0, 0], "chest": [14, 0, 0], "neck": [10, 0, 0], "head": [14, 0, 0],
                "foot_l": [0.3, 0.2, -0.14], "foot_r": [0.16, 0.05, 0.16], "free_l": 1, "free_r": 1, "toe_l": 30, "toe_r": 30,
                "knee_l": [0.6, 0.3, -0.25], "knee_r": [0.5, 0.2, 0.25],
                "hand_l": [-0.3, 1.6, -0.42], "hand_r": [-0.4, 1.5, 0.44], "elbow_l": [-0.5, 1.0, -0.8], "elbow_r": [-0.5, 1.0, 0.8]}
P["air_fall"] = {"base": "air_hit", "hips_rot": [55, -10, 0], "spine": [10, 0, 0], "head": [-14, 0, 0],
                 "hand_l": [-0.45, 1.05, -0.42], "hand_r": [-0.55, 0.92, 0.44], "elbow_l": [-0.5, 0.6, -0.8], "elbow_r": [-0.5, 0.55, 0.8]}
P["swept"] = {"base": "air_fall", "hips_rot": [80, -10, 0], "foot_l": [0.5, 0.35, -0.14], "foot_r": [0.45, 0.4, 0.16]}
P["down"] = {"base": "stance", "hips": [0.0, -0.79, 0.0], "hips_rot": [86, 0, 0], "spine": [4, 0, 0], "chest": [2, 0, 0], "neck": [-6, 0, 0], "head": [-8, 18, 0],
             "foot_l": [0.85, 0.0, -0.17], "foot_r": [0.82, 0.0, 0.2], "yaw_l": 0, "yaw_r": 0, "free_l": 1, "free_r": 1, "toe_l": 60, "toe_r": 55,
             "knee_l": [0.5, 1.2, -0.2], "knee_r": [0.5, 1.2, 0.2],
             "hand_l": [0.05, 0.08, -0.36], "hand_r": [-0.05, 0.08, 0.38], "elbow_l": [-0.2, 0.5, -0.7], "elbow_r": [-0.2, 0.5, 0.7]}
P["rise_1"] = {"base": "down", "hips": [0.0, -0.74, 0.0], "hips_rot": [50, -10, 0], "spine": [-22, 0, 0], "chest": [-10, 0, 0], "head": [10, 8, 0],
               "foot_l": [0.42, 0.0, -0.16], "foot_r": [0.5, 0.0, 0.2], "toe_l": 20, "toe_r": 20, "knee_l": [0.8, 1.0, -0.3], "knee_r": [0.8, 1.0, 0.3],
               "hand_l": [-0.2, 0.06, -0.3], "hand_r": [-0.18, 0.06, 0.32]}
P["rise_2"] = {"base": "crouch", "hips": [-0.05, -0.38, 0.0], "spine": [-22, 4, 0], "chest": [-8, 4, 0], "head": [16, 4, 0], "hand_l": [0.3, 0.9, -0.2], "hand_r": [0.2, 0.92, 0.15]}

# --- Ceremony -----------------------------------------------------------------------------------
P["win"] = {"base": "stance", "hips": [0.0, -0.04, 0.0], "hips_rot": [0, -15, 0], "spine": [2, 6, 0], "chest": [4, 6, 0], "head": [-4, 10, 0],
            "foot_l": [0.12, 0.0, -0.14], "foot_r": [-0.12, 0.0, 0.15], "yaw_r": -30,
            "hand_r": [0.12, 1.98, 0.12], "elbow_r": [0.3, 1.6, 0.6], "hand_l": [0.1, 1.0, -0.25]}
P["intro"] = {"base": "stance", "hips": [0.0, -0.02, 0.0], "hips_rot": [0, -10, 0], "spine": [2, 4, 0], "chest": [2, 4, 0], "head": [2, 4, 0],
              "foot_l": [0.08, 0.0, -0.13], "foot_r": [-0.08, 0.0, 0.14], "yaw_l": -6, "yaw_r": -20,
              "hand_l": [0.0, 0.95, -0.24], "hand_r": [0.0, 0.95, 0.24], "elbow_l": [-0.2, 1.2, -0.4], "elbow_r": [-0.2, 1.2, 0.4], "fist_l": 0.6, "fist_r": 0.6}

if __name__ == "__main__":
    with open(OUT, "w") as f:
        f.write("{\n")
        items = list(P.items())
        for i, (k, v) in enumerate(items):
            f.write('\t"%s": %s%s\n' % (k, json.dumps(v), "," if i < len(items) - 1 else ""))
        f.write("}\n")
    print("wrote %d poses to %s" % (len(P), os.path.relpath(OUT)))
