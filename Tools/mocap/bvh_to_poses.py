#!/usr/bin/env python3
"""Turns motion capture (BVH) into Slopbat key poses, retimed onto a move's frame data.

    python3 -I Tools/mocap/bvh_to_poses.py                 writes the files below from CLIPS
    python3 -I Tools/mocap/bvh_to_poses.py --inspect FILE  prints a take's contact frames

Each converted take becomes one pose per game frame (`mc_<move>_<frame>`, in
Content/Data/poses_mocap.json, authored for the 1.8 m base body like the shared poses) and an
animation key list for the move (Content/Data/basics_style.json for the shared basics, or the
fighter's own data for a fighter's own move). Intro and win performances go to
Content/Data/fighters_mocap.json, laid over each fighter at load. Strikes keep their startup, active and recovery
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
# A foot is free of the floor over this rise (m), and points (degrees from standing) within these.
FREE_FROM = 0.015
FREE_RANGE = 0.09
FOOT_REST = 18.0
FIRST_SHARE = 0.35
STRIKE_RISE = 1.5
TOE_MIN = -15.0
TOE_MAX = 70.0

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
    # The Grandmaster's step aside (Daxion's evade, carried over): a lean-away dodge, deepest at
    # frame 8 of its 20 (the simulation moves him).
    "step_aside": ("Male2_E19_DodgeLeft", "Head", (8, 0, 12), {}, "grandmaster"),
    # Wren's kickboxing and taekwondo: the lead-leg head kick, the backfist, the spinning back
    # kick (Turning Light) and the reverse spinning crescent that lifts jumpers (Lifting Light).
    "wren_head_kick": ("Male2_G8_RoundhouseLeft", "LeftFoot", (9, 3, 20), {"fist_l": 1, "fist_r": 1}, "wren"),
    "wren_backfist": ("Male2_E12_BackfistRight", "RightHand", (10, 3, 18), {"fist_l": 1, "fist_r": 1}, "wren"),
    "turning_light": ("Male2_G4_SpinningBackKick", "LeftFoot", (12, 4, 22), {"fist_l": 1, "fist_r": 1}, "wren"),
    "lifting_light": ("Male2_G20_ReverseSpinCrescentRight", "RightFoot", (6, 5, 24), {"fist_l": 1, "fist_r": 1}, "wren"),
    # The Grandmaster's weave: a slip under the blow, deepest at frame 6 of its 22.
    "weave": ("Male2_E21_DuckLeft", "Head", (6, 0, 16), {}, "grandmaster"),
    # His boxing with no Qhi left: a hook to the head and one to the body (both takes start in a
    # right lead, so the mirror makes them the right hook and the left hook to the body).
    "box_hook": ("Male2_E5_HookLeft", "LeftHand", (7, 3, 14), {"fist_l": 1, "fist_r": 1}, "grandmaster_spent"),
    "box_body_hook": ("Male2_E10_BodyHookRight", "RightHand", (7, 3, 15), {"fist_l": 1, "fist_r": 1}, "grandmaster_spent"),
}

# pose -> (take, take frame, the fields taken). Laid over the shared pose of that name; fields not
# taken stay the original's (the block's hips and stance come from the fighter).
POSES = {
    "block": ("Male2_E17_BlockMiddleHigh", 29, ("spine", "chest", "neck", "head", "hand_l", "hand_r", "elbow_l", "elbow_r")),
}

# cycle -> (take, the foot whose lift-off starts a cycle, travel: +1 forward, -1 backward, the
# pose fields taken: "all" or "legs" (hips, legs and feet only: the fighter's own guard and upper
# body stay)). One cycle is cut between two lift-offs of that foot in the take's steadiest stretch
# and sampled at CYCLE_POSES points; the game plays it by distance covered, so feet do not slide.
CYCLES = {
    "run": ("Male2_C3_Run", "LeftFoot", 1, "all"),
    "walk_f": ("Male2_E4_QuickAdvance", "LeftFoot", 1, "legs"),
    "walk_b": ("Male2_E6_QuickRetreat", "RightFoot", -1, "legs"),
}
CYCLE_POSES = 16
LEGS = ("hips", "foot_l", "foot_r", "knee_l", "knee_r", "yaw_l", "yaw_r", "free_l", "free_r", "toe_l", "toe_r")

# fighter -> state -> (take, first and last take frames). Played at the take's own speed (two game
# frames a take frame), turned 45 degrees toward the camera as fighting games show a win.
STATES = {
    "ashka": {"intro": ("Male2_D1_StandToReady", 0, 80), "win": ("Male2_D8_BowToReady", 0, 128)},
    "bram": {"intro": ("Male2_D1_StandToReady", 0, 80), "win": ("Male2_D11_ReadyToTaunt1ToReady", 0, 130)},
    "gorm": {"intro": ("Male2_D1_StandToReady", 0, 80), "win": ("Male2_D10_Victory2", 0, 130)},
    "mirela": {"intro": ("Male2_D1_StandToReady", 0, 80), "win": ("Male2_D8_BowToReady", 0, 128)},
    "nix": {"intro": ("Male2_D1_StandToReady", 0, 80), "win": ("Male2_D12_ReadyToTaunt1ToReady", 0, 130)},
    "wren": {"intro": ("Male2_D9_WarmUpToReadyToWarmUp", 0, 80), "win": ("Male2_D9_Victory1", 0, 130)},
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

    def set_mirror(self):
        """The performer reflected (left for right): sides swapped and positions mirrored."""
        if self.mirror:
            return
        out = []
        for w in self.frames:
            m = {k: (-v[0], v[1], v[2]) for k, v in w.items()}
            for l, r in L_R:
                m[l], m[r] = m[r], m[l]
            out.append(m)
        self.frames = out
        self.mirror = True

    def at(self, t):
        a = int(math.floor(max(0, min(len(self.frames) - 1, t))))
        b = min(a + 1, len(self.frames) - 1)
        u = max(0.0, min(1.0, t - a))
        pa, pb = self.frames[a], self.frames[b]
        return {k: tuple(pa[k][i] * (1 - u) + pb[k][i] * u for i in range(3)) for k in pa}


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
    if joint == "Head":
        # A dodge or duck: the head at its lowest, the take's start to its recovery.
        h = [take.frames[i]["Head"][1] for i in range(n)]
        contact = min(range(n), key=lambda i: h[i])
        drop = h[0] - h[contact]
        start = contact
        while start > 0 and h[start] < h[0] - 0.1 * drop:
            start -= 1
        back = contact
        while back < n - 1 and h[back] < h[0] - 0.25 * drop:
            back += 1
        w = take.frames[start]
        toes = add(norm(sub(w["LeftToeBase_end"], w["LeftFoot"])), norm(sub(w["RightToeBase_end"], w["RightFoot"])))
        return start, contact, back, norm((toes[0], 0.0, toes[2]))
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
        # How far the foot has left the floor, eased so it does not snap from flat to free in a frame.
        lift = max(0.0, min(1.0, (a[1] - (ANKLE_H + FREE_FROM)) / FREE_RANGE))
        out["free_" + side] = round(lift * lift * (3.0 - 2.0 * lift), 3)
        # The ankle's own angle, as the rig reads it: the foot against the shin, not against the floor
        # (a pointed foot at the end of a level shin lies flat, which measured as no point at all).
        shin = norm(sub(w[foot], w[knee]))
        point = math.degrees(math.asin(max(-1.0, min(1.0, dot(norm(tv), shin))))) - FOOT_REST
        out["toe_" + side] = round(max(TOE_MIN, min(TOE_MAX, point)), 1) if lift > 0.0 else 0
    out.update(fists)
    return out


def path_warp(take, joint, start, contact, steps=48):
    """Time in the take for a share of the startup. In a capture nearly all of a limb's travel comes
    in the last instant, so compressed by the clock it crosses a metre in a frame; taken by distance
    along its path, gathering speed into contact (STRIKE_RISE), it arrives fast but stays a limb."""
    ts = [start + (contact - start) * i / steps for i in range(steps + 1)]
    ps = [take.at(t)[joint] for t in ts]
    run = [0.0]
    for a, b in zip(ps, ps[1:]):
        d = sub(b, a)
        run.append(run[-1] + math.sqrt(dot(d, d)))
    total = run[-1]
    if total < 1e-6:
        return lambda u: start + (contact - start) * u

    def at(u):
        want = total * u ** STRIKE_RISE
        for i in range(steps):
            if run[i + 1] >= want:
                span = run[i + 1] - run[i]
                return ts[i] + (ts[i + 1] - ts[i]) * ((want - run[i]) / span if span > 0 else 0.0)
        return contact
    return at


def convert(move, spec):
    name, joint, (startup, active, recovery), fists, dest = spec
    take = Take(name)
    f0 = take.frames[0]
    start, contact, back, fwd = phases(take, joint)
    # The poses are authored with the left foot leading; a right-lead start is mirrored.
    lead = dot(sub(f0["LeftFoot"], f0["RightFoot"]), fwd)
    if lead < 0:
        take.set_mirror()
        joint = joint.replace("Left", "@").replace("Right", "Left").replace("@", "Right")
        start, contact, back, fwd = phases(take, joint)
    origin = (take.at(start)["Hips"][0], 0.0, take.at(start)["Hips"][2])
    total = startup + active + recovery
    hold = 0.5  # take frames per game frame across the active frames (the blow drives on)
    # The take fills most of the startup: squeezed into its last frames, the limb waits and then
    # teleports (a roundhouse's foot reached 52 m/s). The first frames crossfade out of the stance.
    first = max(2, int(round(startup * FIRST_SHARE)))
    poses, keys = {}, []
    arrive = path_warp(take, joint, start, contact)
    for g in range(first, total - 3):
        if g <= startup:
            t = arrive((g - first + 1) / (startup - first + 1))
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
    return poses, keys, dest, joint


def convert_state(fighter, state, spec):
    name, a, b = spec
    take = Take(name)
    w = take.at(a)
    toes = add(norm(sub(w["LeftToeBase_end"], w["LeftFoot"])), norm(sub(w["RightToeBase_end"], w["RightFoot"])))
    f0 = norm((toes[0], 0.0, toes[2]))
    r0 = cross(f0, (0.0, 1.0, 0.0))
    c = math.cos(math.radians(45.0))
    fwd = norm(sub(scale(f0, c), scale(r0, c)))
    origin = (w["Hips"][0], 0.0, w["Hips"][2])
    poses, keys = {}, []
    for i, t in enumerate(range(a, b + 1)):
        n = "mc_%s_%s_%03d" % (fighter, state, i)
        poses[n] = pose(take, t, origin, fwd, {})
        keys.append([6 + 2 * i, n, "linear" if i else "inout"])
    print("%-11s %-30s frames %d-%d (%.1f s)" % (fighter + " " + state, name, a, b, (b - a) * take.clip.dt))
    return poses, keys


def lift_offs(take, joint):
    n = len(take.frames)
    floor = min(take.frames[i][joint][1] for i in range(n))
    up = [take.frames[i][joint][1] > floor + 4.0 for i in range(n)]
    return [i for i in range(1, n) if up[i] and not up[i - 1]]


def convert_cycle(name, spec, mirrored=False):
    take_name, foot, travel, fields = spec
    take = Take(take_name)
    if mirrored:
        take.set_mirror()
        foot = foot.replace("Left", "@").replace("Right", "Left").replace("@", "Right")
    lifts = lift_offs(take, foot)
    # The steadiest cycle: consecutive lift-offs whose hips speed is closest to the take's median.
    best, best_err = None, 1e9
    spans = [(a, b) for a, b in zip(lifts, lifts[1:])]
    speeds = []
    for a, b in spans:
        d = math.dist((take.frames[a]["Hips"][0], take.frames[a]["Hips"][2]), (take.frames[b]["Hips"][0], take.frames[b]["Hips"][2]))
        speeds.append(d / ((b - a) * take.clip.dt))
    med = sorted(speeds)[len(speeds) // 2]
    for (a, b), v in zip(spans, speeds):
        if abs(v - med) < best_err and b - a > 6:
            best, best_err = (a, b), abs(v - med)
    a, b = best
    ha, hb = take.frames[a]["Hips"], take.frames[b]["Hips"]
    move = norm((hb[0] - ha[0], 0.0, hb[2] - ha[2]))
    fwd = move if travel > 0 else scale(move, -1.0)
    stride = math.dist((ha[0], ha[2]), (hb[0], hb[2])) * take.s
    poses = []
    for k in range(CYCLE_POSES):
        t = a + (b - a) * k / CYCLE_POSES
        h = take.at(t)["Hips"]
        p = pose(take, t, (h[0], 0.0, h[2]), fwd, {})
        if fields == "legs":
            p = {f: p[f] for f in LEGS}
            p["base"] = "stance"
        n = "mc_cycle_%s_%02d" % (name, k)
        poses.append((n, p))
    # The poses lead with the left foot; a cycle whose right foot leads is taken again mirrored.
    lead = sum(p["foot_l"][0] - p["foot_r"][0] for _, p in poses)
    if lead < 0 and not mirrored and fields == "legs":
        return convert_cycle(name, spec, True)
    print("%-11s %-30s frames %d-%d, stride %.2f m, %.2f m/s as captured%s" % (name, take_name, a, b, stride, stride / ((b - a) * take.clip.dt),
                                                                           " mirrored" if mirrored else ""))
    return dict(poses), {"stride": round(stride, 4), "poses": [n for n, _ in poses]}


def convert_pose(name, spec):
    take_name, t, fields = spec
    take = Take(take_name)
    w = take.at(t)
    mid = scale(add(w["LeftHand"], w["RightHand"]), 0.5)
    fwd = norm((mid[0] - w["Hips"][0], 0.0, mid[2] - w["Hips"][2]))
    p = pose(take, t, (w["Hips"][0], 0.0, w["Hips"][2]), fwd, {})
    if dot(sub(w["LeftFoot"], w["RightFoot"]), fwd) < 0:
        take.set_mirror()
        w = take.at(t)
        mid = scale(add(w["LeftHand"], w["RightHand"]), 0.5)
        fwd = norm((mid[0] - w["Hips"][0], 0.0, mid[2] - w["Hips"][2]))
        p = pose(take, t, (w["Hips"][0], 0.0, w["Hips"][2]), fwd, {})
    print("%-11s %-30s frame %d%s" % (name, take_name, t, " mirrored" if take.mirror else ""))
    return {f: p[f] for f in fields}


def write_poses(path, poses):
    with open(path, "w") as f:
        f.write("{\n" + ",\n".join('\t"%s": %s' % (k, json.dumps(v)) for k, v in poses.items()) + "\n}\n")


def replace_keys(text, move, keys):
    """Swaps one move's animation keys in a fighter's JSON text, leaving the rest as it is written."""
    at = text.index('"%s": {' % move)
    at = text.index('"keys":', text.index('"anim":', at))
    start = text.index("[", at)
    depth, i = 0, start
    while True:
        depth += {"[": 1, "]": -1}.get(text[i], 0)
        i += 1
        if depth == 0:
            break
    return text[:start] + json.dumps(keys) + text[i:]


def main():
    if len(sys.argv) > 2 and sys.argv[1] == "--inspect":
        take = Take(sys.argv[2])
        for j in ("LeftHand", "RightHand", "LeftFoot", "RightFoot"):
            print(j, phases(take, j)[:3])
        return
    all_poses, basics = {}, {}
    fighter_keys = {}
    for move, spec in CLIPS.items():
        poses, keys, dest, joint = convert(move, spec)
        all_poses.update(poses)
        if dest == "basics":
            # The limb that lands the blow as the poses show it (a mirrored take swaps sides).
            basics[move] = {"anim": {"tip": {"LeftHand": "hand_l", "RightHand": "hand_r", "LeftFoot": "foot_l", "RightFoot": "foot_r"}[joint], "keys": keys}}
        else:
            fighter_keys.setdefault(dest, {})[move] = keys
    for name, spec in POSES.items():
        all_poses[name] = convert_pose(name, spec)
    cycles = {}
    for name, spec in CYCLES.items():
        poses, cycle = convert_cycle(name, spec)
        all_poses.update(poses)
        cycles[name] = cycle
    with open(os.path.join(DATA, "cycles_mocap.json"), "w") as f:
        f.write("{\n" + ",\n".join('\t"%s": %s' % (k, json.dumps(v)) for k, v in cycles.items()) + "\n}\n")
    states = {}
    for fighter, by_state in STATES.items():
        for state, spec in by_state.items():
            poses, keys = convert_state(fighter, state, spec)
            all_poses.update(poses)
            states.setdefault(fighter, {})[state] = keys
    write_poses(os.path.join(DATA, "poses_mocap.json"), all_poses)
    with open(os.path.join(DATA, "basics_style.json"), "w") as f:
        json.dump(basics, f, indent="\t")
        f.write("\n")
    for fighter, moves in fighter_keys.items():
        p = os.path.join(DATA, "fighters", fighter + ".json")
        text = open(p).read()
        for move, keys in moves.items():
            text = replace_keys(text, move, keys)
        json.loads(text)
        open(p, "w").write(text)
    # The intro and win performances, laid over each fighter's data at load (one line a fighter).
    with open(os.path.join(DATA, "fighters_mocap.json"), "w") as f:
        f.write("{\n" + ",\n".join('\t"%s": %s' % (k, json.dumps({"state_anim": v})) for k, v in sorted(states.items())) + "\n}\n")
    print("%d poses" % len(all_poses))


if __name__ == "__main__":
    main()
