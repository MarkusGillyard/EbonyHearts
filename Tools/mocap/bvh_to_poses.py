#!/usr/bin/env python3
"""Turns motion capture (BVH) into Slopbat key poses, retimed onto a move's frame data.

    python3 -I Tools/mocap/bvh_to_poses.py                 writes the files below from CLIPS
    python3 -I Tools/mocap/bvh_to_poses.py --inspect FILE  prints a take's contact frames

Each converted take becomes one pose per game frame (`mc_<move>_<frame>`, in
Content/Data/poses_mocap.json, authored for the 1.8 m base body like the shared poses) and an
animation key list for the move (Content/Data/basics_style.json for the shared basics, or the
fighter's style file for a fighter's own move). Strikes keep their startup, active and recovery
frames: the take is cut at the strike's start, contact and return, then each phase is stretched or
compressed onto the move's frames (startup compressed, as every fighting game does).

How a body becomes a pose. Forward kinematics gives every joint's position; the facing is the
direction the striking hand or foot travels. Hands, elbows, ankles and knees are placed in the
fighter's root space (the hips at the take's start), and the pelvis, lower spine, chest, neck and
head rotations are built from the body's own geometry (each segment's length and the hip or
shoulder line), so nothing depends on how the capture rig's joints were oriented. A take that starts
in a right-lead stance is mirrored to the left lead the poses are authored in.

Source: ACCAD Open Motion Project, Male 2 (CC BY 3.0); see SourceArt/ThirdParty/ledger.json.
"""
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bvh  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
TAKES = os.path.join(ROOT, "SourceArt", "ThirdParty", "ACCAD", "extracted")
DATA = os.path.join(ROOT, "Content", "Data")
H = 1.8
REST_HIPS = 0.53 * H
ANKLE_H = 0.042 * H

# move -> (take, striking joint, frames: startup, active, recovery, fist values, where the keys go)
CLIPS = {
    "hp": ("Male2_E1_JabLeft", "LeftHand", (5, 3, 10), {"fist_l": 1, "fist_r": 1}, "basics"),
    "lp": ("Male2_E16_BodyJabLeft", "LeftHand", (5, 3, 10), {"fist_l": 1, "fist_r": 1}, "basics"),
    "hp2": ("Male2_E4_CrossRight", "RightHand", (5, 3, 12), {"fist_l": 1, "fist_r": 1}, "basics"),
    "lp2": ("Male2_E14_BodyCrossRight", "RightHand", (5, 3, 12), {"fist_l": 1, "fist_r": 1}, "basics"),
    "lk": ("Male2_G15_RoundhouseBodyRight", "RightFoot", (7, 3, 12), {}, "basics"),
    "roundhouse": ("Male2_G9_RoundhouseRight", "RightFoot", (13, 4, 22), {}, "basics"),
    "hk": ("Male2_G2_FrontKick", "RightFoot", (9, 4, 16), {}, "basics"),
    "ashka_teep": ("Male2_G17_PushKickLeft", "LeftFoot", (8, 3, 14), {}, "ashka"),
}

# Left out: the uppercut (the take's furthest reach is its overhead follow-through, not the blow,
# and its mostly upward path gives no reliable facing); its hand-keyed poses stay.

L_R = [("LeftHand", "RightHand"), ("LeftForeArm", "RightForeArm"), ("LeftFoot", "RightFoot"), ("LeftLeg", "RightLeg"),
       ("LeftUpLeg", "RightUpLeg"), ("LeftArm", "RightArm"), ("LeftToeBase", "RightToeBase"), ("LeftToeBase_end", "RightToeBase_end"),
       ("LeftShoulder", "RightShoulder")]


def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def scale(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def norm(a):
    n = math.sqrt(dot(a, a))
    return scale(a, 1.0 / n) if n > 1e-9 else (0.0, 0.0, 0.0)


def frame(up, right):
    """Rotation (columns x right, y up, z back) with y along `up` and x as close to `right`."""
    y = norm(up)
    x = norm(sub(right, scale(y, dot(right, y))))
    z = cross(x, y)
    return ((x[0], y[0], z[0]), (x[1], y[1], z[1]), (x[2], y[2], z[2]))


def transpose(m): return tuple(tuple(m[j][i] for j in range(3)) for i in range(3))
def mmul(a, b): return tuple(tuple(sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)) for i in range(3))


def euler_yxz(m):
    """Degrees [x, y, z] with m = Y(y) * X(x) * Z(z), the order FPose reads."""
    x = math.degrees(math.asin(max(-1.0, min(1.0, -m[1][2]))))
    y = math.degrees(math.atan2(m[0][2], m[2][2]))
    z = math.degrees(math.atan2(m[1][0], m[1][1]))
    return [round(x, 1), round(y, 1), round(z, 1)]


class Take:
    def __init__(self, name):
        self.name = name
        self.clip = bvh.load(os.path.join(TAKES, name + ".bvh"))
        self.frames = [self.clip.world(i) for i in range(self.clip.frames)]
        stand = bvh.load(os.path.join(TAKES, "Male2_A1_Stand.bvh")).world(0)
        self.ground = min(stand["LeftToeBase_end"][1], stand["RightToeBase_end"][1])
        self.s = H / (stand["Head_end"][1] - self.ground)
        self.mirror = False

    def at(self, t):
        a = int(math.floor(max(0, min(len(self.frames) - 1, t))))
        b = min(a + 1, len(self.frames) - 1)
        u = max(0.0, min(1.0, t - a))
        pa, pb = self.frames[a], self.frames[b]
        w = {k: tuple(pa[k][i] * (1 - u) + pb[k][i] * u for i in range(3)) for k in pa}
        if self.mirror:
            m = dict(w)
            for l, r in L_R:
                m[l], m[r] = w[r], w[l]
            w = m
        return w


def speed(take, joint, i):
    a = take.frames[max(0, i - 1)][joint]
    b = take.frames[min(len(take.frames) - 1, i + 1)][joint]
    return math.sqrt(dot(sub(b, a), sub(b, a))) / (2 * take.clip.dt)


def phases(take, joint):
    """Start, contact and return frames of a take's strike, and the direction it travels. A fist
    lands where it is furthest from where it started, measured from the hips (so a step does not
    count); a foot lands at its highest. The strike starts where the limb last moved slowly before
    contact and returns when it is back near its start (a foot: back on the floor)."""
    n = len(take.frames)
    rel = [sub(take.frames[i][joint], take.frames[i]["Hips"]) for i in range(n)]
    foot = joint.endswith("Foot")
    if foot:
        contact = max(range(n), key=lambda i: take.frames[i][joint][1])
    else:
        contact = max(range(n), key=lambda i: math.dist(rel[i], rel[0]))
    peak = max(speed(take, joint, i) for i in range(n))
    start = contact
    back = contact
    if foot:
        floor = min(take.frames[i][joint][1] for i in range(n))
        while start > 0 and take.frames[start][joint][1] > floor + 3.0:
            start -= 1
        while back < n - 1 and take.frames[back][joint][1] > floor + 3.0:
            back += 1
    else:
        while start > 0 and (speed(take, joint, start) > 0.12 * peak or start > contact - 2):
            start -= 1
        reach = math.dist(rel[contact], rel[start])
        while back < n - 1 and math.dist(rel[back], rel[start]) > 0.25 * reach:
            back += 1
    d = sub(take.frames[contact][joint], take.frames[start]["Hips"])
    return start, contact, back, norm((d[0], 0.0, d[2]))


def segments(w, rig):
    hip_line = rig(sub(w["RightUpLeg"], w["LeftUpLeg"]))
    shoulder_line = rig(sub(w["RightArm"], w["LeftArm"]))
    return {"P": frame(rig(sub(w["Spine"], w["Hips"])), hip_line),
            "S": frame(rig(sub(w["Spine1"], w["Spine"])), add(norm(hip_line), norm(shoulder_line))),
            "C": frame(rig(sub(w["Neck"], w["Spine1"])), shoulder_line),
            "N": frame(rig(sub(w["Head"], w["Neck"])), shoulder_line),
            "H": frame(rig(sub(w["Head_end"], w["Head"])), shoulder_line)}


_REST = {}


def rest_segments():
    """The performer standing, facing where the toes point: every segment's frame there counts as
    no rotation (it carries the capture skeleton's own spine and neck curves, which are not pose)."""
    if not _REST:
        w = bvh.load(os.path.join(TAKES, "Male2_A1_Stand.bvh")).world(0)
        toes = add(norm(sub(w["LeftToeBase_end"], w["LeftFoot"])), norm(sub(w["RightToeBase_end"], w["RightFoot"])))
        fwd = norm((toes[0], 0.0, toes[2]))
        right = cross(fwd, (0.0, 1.0, 0.0))
        rig = lambda v: (dot(v, right), v[1], -dot(v, fwd))
        _REST.update(segments(w, rig))
        for side, foot, toe in (("l", "LeftFoot", "LeftToeBase_end"), ("r", "RightFoot", "RightToeBase_end")):
            tv = sub(w[toe], w[foot])
            _REST["yaw_" + side] = math.degrees(math.atan2(-dot(tv, right), dot(tv, fwd)))
    return _REST


def pose(take, t, origin, fwd, fists):
    w = take.at(t)
    up = (0.0, 1.0, 0.0)
    right = cross(fwd, up)
    s = take.s

    def data(p):
        v = sub(p, origin)
        return (dot(v, fwd) * s, (p[1] - take.ground) * s, dot(v, right) * s)

    def rig(v):  # a world direction in rig axes (x right, y up, z back)
        return (dot(v, right), v[1], -dot(v, fwd))

    def r3(v):
        return [round(v[0], 4), round(v[1], 4), round(v[2], 4)]

    rest = rest_segments()
    seg = segments(w, rig)
    # Each segment's turn away from standing, then each relative to its parent as the rig chains them.
    g = {k: mmul(seg[k], transpose(rest[k])) for k in ("P", "S", "C", "N", "H")}
    local = lambda parent, child: euler_yxz(mmul(transpose(g[parent]), g[child]))
    hips = data(w["Hips"])
    out = {"hips": r3((hips[0], hips[1] - REST_HIPS, hips[2])), "hips_rot": euler_yxz(g["P"]),
           "spine": local("P", "S"), "chest": local("S", "C"), "neck": local("C", "N"), "head": local("N", "H")}
    for side, hand, elbow, foot, knee, toe in (("l", "LeftHand", "LeftForeArm", "LeftFoot", "LeftLeg", "LeftToeBase_end"),
                                              ("r", "RightHand", "RightForeArm", "RightFoot", "RightLeg", "RightToeBase_end")):
        out["hand_" + side] = r3(data(w[hand]))
        out["elbow_" + side] = r3(data(w[elbow]))
        a = data(w[foot])
        out["foot_" + side] = r3((a[0], a[1] - ANKLE_H, a[2]))
        out["knee_" + side] = r3(data(w[knee]))
        tv = sub(w[toe], w[foot])
        out["yaw_" + side] = round(math.degrees(math.atan2(-dot(tv, right), dot(tv, fwd))) - rest["yaw_" + side], 1)
        lifted = a[1] > ANKLE_H + 0.05
        out["free_" + side] = 1 if lifted else 0
        flat = math.sqrt(dot(tv, fwd) ** 2 + dot(tv, right) ** 2)
        out["toe_" + side] = round(max(0.0, math.degrees(math.atan2(-tv[1], flat)) - 18.0), 1) if lifted else 0
    out.update(fists)
    return out


def convert(move, spec):
    name, joint, (startup, active, recovery), fists, dest = spec
    take = Take(name)
    f0 = take.frames[0]
    start, contact, back, fwd = phases(take, joint)
    # The poses are authored with the left foot leading; a right-lead start is mirrored.
    lead = dot(sub(f0["LeftFoot"], f0["RightFoot"]), fwd)
    if lead < 0:
        take.mirror = True
        joint = joint.replace("Left", "@").replace("Right", "Left").replace("@", "Right")
    origin = (take.at(start)["Hips"][0], 0.0, take.at(start)["Hips"][2])
    total = startup + active + recovery
    hold = 0.5  # take frames per game frame across the active frames (the blow drives on)
    first = max(2, startup - 3)
    poses, keys = {}, []
    for g in range(first, total - 3):
        if g <= startup:
            t = start + (contact - start) * (g - first + 1) / (startup - first + 1)
        elif g <= startup + active:
            t = contact + (g - startup) * hold
        else:
            c2 = contact + active * hold
            t = c2 + (back - c2) * (g - startup - active) / (recovery - 3)
        n = "mc_%s_%02d" % (move, g)
        poses[n] = pose(take, t, origin, fwd, fists)
        keys.append([g, n, "linear"])
    keys.append([total, "stance", "inout"])
    print("%-11s %-30s start %3d contact %3d back %3d (%d take frames to %d game frames)%s" %
          (move, name, start, contact, back, back - start, total, " mirrored" if take.mirror else ""))
    return poses, keys, dest


def write_poses(path, poses):
    with open(path, "w") as f:
        f.write("{\n" + ",\n".join('\t"%s": %s' % (k, json.dumps(v)) for k, v in poses.items()) + "\n}\n")


def main():
    if len(sys.argv) > 2 and sys.argv[1] == "--inspect":
        take = Take(sys.argv[2])
        for j in ("LeftHand", "RightHand", "LeftFoot", "RightFoot"):
            print(j, phases(take, j)[:3])
        return
    all_poses, basics = {}, {}
    fighter_keys = {}
    for move, spec in CLIPS.items():
        poses, keys, dest = convert(move, spec)
        all_poses.update(poses)
        if dest == "basics":
            basics[move] = {"anim": {"tip": {"LeftHand": "hand_l", "RightHand": "hand_r", "LeftFoot": "foot_l", "RightFoot": "foot_r"}[spec[1]], "keys": keys}}
        else:
            fighter_keys.setdefault(dest, {})[move] = keys
    write_poses(os.path.join(DATA, "poses_mocap.json"), all_poses)
    with open(os.path.join(DATA, "basics_style.json"), "w") as f:
        json.dump(basics, f, indent="\t")
        f.write("\n")
    for fighter, moves in fighter_keys.items():
        p = os.path.join(DATA, "fighters", fighter + ".json")
        d = json.load(open(p))
        for move, keys in moves.items():
            d["moves"][move]["anim"]["keys"] = keys
        open(p, "w").write(json.dumps(d, indent="\t", ensure_ascii=False) + "\n")
    print("%d poses" % len(all_poses))


if __name__ == "__main__":
    main()
