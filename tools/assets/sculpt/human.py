"""A human body sculpted from signed distance primitives on the exported bind skeleton.

Measurements are written for a 1.8 m adult and scaled by the fighter's height and build
("shoulders", "chest", "waist", "limbs", "hips" from look.build). The figure ("male" or "female")
and a muscle amount from look.sculpt change proportions and definition. Builders return SDF
nodes so garments can reuse the same shapes inflated outward.
"""
import numpy as np

from sdf import Intersect, Union, ellipsoid, frame_from, half_space, round_cone, sphere

TORSO = ["hips", "spine", "chest", "neck", "clav_l", "clav_r"]


class Body:
    def __init__(self, rig):
        self.rig = rig
        self.h = rig.h
        self.s = rig.h / 1.8
        b = rig.look.get("build", {})
        self.sw = float(b.get("shoulders", 1.0))
        self.cw = float(b.get("chest", 1.0))
        self.ww = float(b.get("waist", 1.0))
        self.lw = float(b.get("limbs", 1.0))
        self.hw = float(b.get("hips", 1.0))
        sc = rig.look.get("sculpt", {})
        self.female = sc.get("figure", "male") == "female"
        self.muscle = float(sc.get("muscle", 0.5))
        self.face = sc.get("face", {})
        self.head_scale = float(sc.get("head", 0.95 if self.female else 1.0))
        # Finger length relative to the palm (long, clawed hands).
        self.fingers = float(sc.get("fingers", 1.0))
        self.feet = float(sc.get("feet", 1.0))

    # --- helpers -------------------------------------------------------------------------------

    def y(self, frac):
        """Height above the floor as a fraction of the fighter's height."""
        return frac * self.h

    def m(self, v):
        """Metres at 1.8 m scaled to this fighter."""
        return v * self.s

    def torso_pt(self, x, y, z):
        """A torso point: x in metres (1.8 m scale, times shoulder or hip build where given by the
        caller), y as a height fraction, z in metres."""
        return np.array([self.m(x), self.y(y), self.m(z)], np.float32)

    # --- torso ---------------------------------------------------------------------------------

    def torso(self, mat="skin", inflate=0.0, smooth=1.0, detail=True):
        """Pelvis to the base of the neck. `inflate` grows every part (garments), `smooth` scales
        the blend radii, `detail` adds muscle and bone landmarks."""
        r = self.rig
        f, mu = self.female, self.muscle
        i = inflate
        hw = self.hw * (1.07 if f else 1.0)
        cw = self.cw
        ww = self.ww
        sw = self.sw
        parts = []
        hips_b = ["hips", "spine"]
        # Pelvis and the hips' widest point (greater trochanters).
        parts.append(ellipsoid(self.torso_pt(0, 0.515, 0.004), [self.m(0.158 * hw) + i, self.m(0.105) + i, self.m(0.1) + i], mat, hips_b))
        parts.append(ellipsoid(self.torso_pt(0, 0.565, 0.0), [self.m(0.15 * ww * (0.92 if f else 1.0)) + i, self.m(0.07) + i, self.m(0.098) + i], mat, ["hips", "spine"]))
        # Waist and abdomen.
        parts.append(round_cone(self.torso_pt(0, 0.56, 0.004), self.torso_pt(0, 0.66, 0.0),
                                self.m(0.142 * ww * (0.86 if f else 1.0)) + i, self.m(0.15 * cw * (0.9 if f else 1.0)) + i, mat, ["spine", "hips", "chest"], squash=(1.0, 0.68)))
        # Ribcage, a little forward-tilted egg.
        rc = self.torso_pt(0, 0.722, 0.006)
        parts.append(ellipsoid(rc, [self.m(0.152 * cw * (0.9 if f else 1.0)) + i, self.m(0.112) + i, self.m(0.112 * (0.93 if f else 1.0)) + i], mat, ["chest", "spine"]))
        # Upper chest and back toward the shoulders.
        parts.append(ellipsoid(self.torso_pt(0, 0.775, 0.012), [self.m(0.158 * sw * cw * (0.88 if f else 1.0)) + i, self.m(0.07) + i, self.m(0.095) + i], mat, ["chest"]))
        # Trapezius: the slope from the neck to each shoulder.
        neck_base = self.torso_pt(0, 0.83, 0.018)
        for s in ("l", "r"):
            sd = r.side(s)
            sh = r.pos("upperarm_" + s)
            top = np.array([sh[0] * 0.8, sh[1] + self.m(0.02), sh[2] + self.m(0.01)], np.float32)
            parts.append(round_cone(neck_base + np.array([sd * self.m(0.02), 0, 0], np.float32), top,
                                    self.m(0.044 + 0.012 * mu * (0 if f else 1)) + i, self.m(0.034) + i, mat, ["chest", "clav_" + s, "neck"]))
            # Shoulder cap (deltoid) over the joint.
            along, front, lat = r.limb_frame("upperarm_" + s, "forearm_" + s, s)
            c = sh + lat * self.m(0.006) + along * self.m(0.03) - [0, 0, self.m(0.004)]
            rot = np.stack([lat, along, front], axis=1)
            cap = 1.0 if detail else 0.74
            parts.append(ellipsoid(c, [self.m(0.05 + 0.014 * mu) * self.lw * cap + i, self.m(0.072) * cap + i, self.m(0.054 + 0.01 * mu) * self.lw * cap + i], mat,
                                   ["upperarm_" + s, "clav_" + s], rot=rot))
        if detail:
            if f:
                # Modest bust under the dress.
                for sd in (-1, 1):
                    parts.append(ellipsoid(self.torso_pt(sd * 0.058 * cw, 0.715, -0.074), [self.m(0.056), self.m(0.05), self.m(0.04)], mat, ["chest"]))
            else:
                for sd in (-1, 1):
                    rot = frame_from([sd * 0.25, 1.0, 0.0], [0, 0, -1])
                    parts.append(ellipsoid(self.torso_pt(sd * 0.062 * cw, 0.738, -0.072), [self.m(0.068 + 0.01 * mu), self.m(0.05), self.m(0.026 + 0.012 * mu)], mat, ["chest"], rot=rot))
            # Shoulder blades and lats.
            for sd in (-1, 1):
                parts.append(ellipsoid(self.torso_pt(sd * 0.075 * sw, 0.765, 0.085), [self.m(0.06), self.m(0.075), self.m(0.03)], mat, ["chest"]))
                parts.append(ellipsoid(self.torso_pt(sd * 0.11 * cw, 0.69, 0.035), [self.m(0.05), self.m(0.1), self.m(0.075)], mat, ["chest", "spine"]))
            # Glutes.
            for sd in (-1, 1):
                parts.append(ellipsoid(self.torso_pt(sd * 0.07 * hw, 0.5, 0.075), [self.m(0.08 * hw), self.m(0.085), self.m(0.07)], mat, ["hips", "thigh_" + ("l" if sd < 0 else "r")]))
            # Collarbones.
            for s in ("l", "r"):
                sd = r.side(s)
                sh = r.pos("upperarm_" + s)
                parts.append(round_cone(self.torso_pt(sd * 0.018, 0.818, -0.05), np.array([sh[0] * 0.92, sh[1] - self.m(0.004), sh[2] - self.m(0.032)], np.float32),
                                        self.m(0.011), self.m(0.012), mat, ["chest", "clav_" + s]))
        return Union(parts, self.m(0.05) * smooth)

    def neck(self, mat="skin", inflate=0.0, top=0.9):
        """Neck from the trapezius into the skull; `top` is the height fraction it reaches."""
        f = self.female
        r0 = self.m(0.056 if f else 0.064) + inflate
        r1 = self.m(0.047 if f else 0.056) + inflate
        a = self.torso_pt(0, 0.82, 0.012)
        b = self.torso_pt(0, top, 0.018)
        parts = [round_cone(a, b, r0, r1, mat, ["neck", "chest", "head"], squash=(1.0, 0.92))]
        return Union(parts, self.m(0.02))

    # --- limbs ---------------------------------------------------------------------------------

    def arm(self, s, mat="skin", inflate=0.0, detail=True, forearm_mat=None, to_wrist=True):
        r = self.rig
        mu = self.muscle * (0.4 if self.female else 1.0)
        lw = self.lw * (0.86 if self.female else 1.0)
        i = inflate
        sh, el, wr = r.pos("upperarm_" + s), r.pos("forearm_" + s), r.pos("hand_" + s)
        ua, uf, ul = r.limb_frame("upperarm_" + s, "forearm_" + s, s)
        fa, ff, fl = r.limb_frame("forearm_" + s, "hand_" + s, s)
        fm = forearm_mat or mat
        ub = ["upperarm_" + s]
        fb = ["forearm_" + s]
        parts = [round_cone(sh + ua * self.m(0.02), el, self.m(0.047) * lw + i, self.m(0.037) * lw + i, mat, ub + ["clav_" + s], squash=(1.0, 1.0), ref=uf),
                 sphere(el, self.m(0.036) * lw + i, mat, ub + fb)]
        if to_wrist:
            parts.append(round_cone(el, wr, self.m(0.039) * lw + i, self.m(0.026) * lw + i, fm, fb, squash=(1.18, 0.86), ref=ff))
        if detail:
            # Biceps in front, triceps behind, the forearm's muscle mass below the elbow.
            parts.append(ellipsoid(sh + ua * (r.lengths["upper"] * 0.56) + uf * self.m(0.016 + 0.008 * mu), [self.m(0.032 + 0.008 * mu) * lw + i, self.m(0.085), self.m(0.03 + 0.008 * mu) * lw + i], mat, ub,
                                   rot=np.stack([ul, ua, uf], axis=1)))
            parts.append(ellipsoid(sh + ua * (r.lengths["upper"] * 0.42) - uf * self.m(0.018), [self.m(0.034 + 0.006 * mu) * lw + i, self.m(0.095), self.m(0.03 + 0.006 * mu) * lw + i], mat, ub,
                                   rot=np.stack([ul, ua, uf], axis=1)))
            if to_wrist:
                parts.append(ellipsoid(el + fa * (r.lengths["fore"] * 0.24) + fl * self.m(0.006), [self.m(0.04 + 0.006 * mu) * lw + i, self.m(0.085), self.m(0.034 + 0.005 * mu) * lw + i], fm, fb,
                                       rot=np.stack([fl, fa, ff], axis=1)))
        return Union(parts, self.m(0.03))

    def leg(self, s, mat="skin", inflate=0.0, detail=True, to_ankle=True, shin_mat=None):
        r = self.rig
        mu = self.muscle * (0.5 if self.female else 1.0)
        lw = self.lw * (0.95 if self.female else 1.0)
        i = inflate
        hp, kn, an = r.pos("thigh_" + s), r.pos("shin_" + s), r.pos("foot_" + s)
        ta, tf, tl = r.limb_frame("thigh_" + s, "shin_" + s, s)
        sa, sf, sl = r.limb_frame("shin_" + s, "foot_" + s, s)
        sm = shin_mat or mat
        tb = ["thigh_" + s]
        sb = ["shin_" + s]
        parts = [round_cone(hp + ta * self.m(0.03), kn, self.m(0.08) * lw + i, self.m(0.05) * lw + i, mat, tb + ["hips"]),
                 ellipsoid(kn, [self.m(0.048) * lw + i, self.m(0.055), self.m(0.05) * lw + i], mat, tb + sb, rot=np.stack([tl, ta, tf], axis=1))]
        if to_ankle:
            parts.append(round_cone(kn, an, self.m(0.048) * lw + i, self.m(0.03) * lw + i, sm, sb))
        if detail:
            parts.append(ellipsoid(hp + ta * (r.lengths["thigh"] * 0.45) + tf * self.m(0.022), [self.m(0.062 + 0.008 * mu) * lw + i, self.m(0.17), self.m(0.055 + 0.008 * mu) * lw + i], mat, tb,
                                   rot=np.stack([tl, ta, tf], axis=1)))
            parts.append(ellipsoid(hp + ta * (r.lengths["thigh"] * 0.42) - tf * self.m(0.02) - tl * self.m(0.012), [self.m(0.055) * lw + i, self.m(0.16), self.m(0.05) * lw + i], mat, tb,
                                   rot=np.stack([tl, ta, tf], axis=1)))
            if to_ankle:
                parts.append(ellipsoid(kn + sa * (r.lengths["shin"] * 0.3) - sf * self.m(0.024), [self.m(0.046 + 0.008 * mu) * lw + i, self.m(0.11), self.m(0.044 + 0.008 * mu) * lw + i], sm, sb,
                                       rot=np.stack([sl, sa, sf], axis=1)))
        return Union(parts, self.m(0.035))

    def foot(self, s, mat="skin", inflate=0.0, shoe=True, toe_cap=1.0):
        """A foot or a shoe on a flat sole, toes toward -z."""
        r = self.rig
        an = r.pos("foot_" + s)
        fb = ["foot_" + s]
        i = inflate
        sole_y = 0.0
        L = self.m(0.26) * (0.92 if self.female else 1.0) * self.feet
        w = self.m(0.05) * (0.9 if self.female else 1.0) * self.feet
        parts = [ellipsoid(an + np.array([0, -self.m(0.012), self.m(0.0)], np.float32), [self.m(0.034) + i, self.m(0.04) + i, self.m(0.04) + i], mat, fb + ["shin_" + s]),
                 ellipsoid(np.array([an[0], sole_y + self.m(0.032), an[2] + self.m(0.035)], np.float32), [self.m(0.034) + i, self.m(0.03) + i, self.m(0.04) + i], mat, fb),
                 round_cone(np.array([an[0], sole_y + self.m(0.045), an[2] - self.m(0.01)], np.float32),
                            np.array([an[0] + r.side(s) * self.m(0.008), sole_y + self.m(0.028), an[2] - L * 0.62], np.float32),
                            self.m(0.036) + i, w * 0.86 * toe_cap + i, mat, fb, squash=(1.35, 0.6), ref=(0, 1, 0))]
        foot = Union(parts, self.m(0.03))
        # Flat sole on the floor.
        return Intersect(foot, half_space([0, sole_y + 0.001, 0], [0, -1, 0]), self.m(0.004))

    # --- head ----------------------------------------------------------------------------------

    def head_model(self):
        from face import Head
        if not hasattr(self, "_head"):
            self._head = Head(self)
        return self._head

    def eye_centres(self):
        return self.head_model().eyes()

    def eye_radius(self):
        return self.head_model().eyes()[1]

    def head(self, mat="skin", lips_mat="lips"):
        return self.head_model().node(mat)

    # --- hands ---------------------------------------------------------------------------------

    def hand(self, s, mat="skin", inflate=0.0):
        """Palm and fingers in the bind pose (fingers straight and slightly spread), with the bone
        layout for the fingers. Returns (node, finger_bones)."""
        hand = HandLayout(self, s, inflate)
        return hand.node(mat), hand.bones


# Finger spread in the bind pose (radians about the palm normal), enough to keep them apart.
SPREAD = {"index": 0.11, "middle": 0.025, "ring": -0.07, "pinky": -0.17}


class HandLayout:
    """Finger joints in the hand's own frame (rig convention: fingers along -y, palm facing +z,
    thumb toward +x on the left hand and -x on the right)."""

    FINGERS = [("index", 0.97, 0.0255, -0.093), ("middle", 1.0, 0.0085, -0.097), ("ring", 0.95, -0.0085, -0.094), ("pinky", 0.78, -0.0245, -0.087)]

    def __init__(self, body, s, inflate):
        self.b = body
        self.s = s
        self.rig = body.rig
        self.i = inflate
        self.k = body.s * (0.92 if body.female else 1.0) * (body.lw ** 0.5)
        self.thumb = 1.0 if s == "l" else -1.0
        self.bones = []
        self._layout()

    def L(self, v):
        return v * self.k

    def to_bind(self, local):
        return self.rig.at("hand_" + self.s, local)

    def _layout(self):
        t = self.thumb
        k = self.L
        # Each finger: knuckle (MCP), PIP, DIP, tip, spread angle (radians about the palm normal).
        self.chains = []
        for name, length, x, y in self.FINGERS:
            spread = SPREAD[name] * t
            seg = np.array([0.046, 0.028, 0.022]) * length * self.b.fingers
            d = np.array([np.sin(spread), -np.cos(spread), 0.0])
            base = np.array([k(x) * t, k(y), 0.0])
            pts = [base]
            for sl in seg:
                pts.append(pts[-1] + d * k(sl))
            radii = [k(0.0092), k(0.0084), k(0.0076), k(0.0068)]
            if name == "pinky":
                radii = [r * 0.86 for r in radii]
            self.chains.append((name, pts, radii, d))
        # Thumb: from the base of the palm out and forward of the index finger.
        tb = np.array([k(0.024) * t, k(-0.022), k(0.012)])
        tdir = np.array([0.55 * t, -0.8, 0.24])
        tdir /= np.linalg.norm(tdir)
        tp = [tb]
        for sl in (0.045, 0.033, 0.028):
            tp.append(tp[-1] + tdir * k(sl) * (1.0 + (self.b.fingers - 1.0) * 0.6))
        self.chains.append(("thumb", tp, [k(0.0125), k(0.011), k(0.0095), k(0.0084)], tdir))
        for name, pts, radii, d in self.chains:
            parent = "hand_" + self.s
            for j in range(3):
                bname = "%s%d_%s" % (name, j + 1, self.s)
                self.bones.append({"name": bname, "parent": parent, "local_pos": pts[j], "dir": d, "finger": name, "seg": j})
                parent = bname

    def node(self, mat):
        k = self.L
        i = self.i
        t = self.thumb
        hb = "hand_" + self.s
        R = self.rig.basis[hb]
        idx_base = self.chains[0][1][0]
        pky_base = self.chains[3][1][0]
        parts = [
            # Palm: a slightly domed back, wider across the knuckles than at the wrist.
            ellipsoid(self.to_bind([0, k(-0.052), k(0.001)]), [k(0.04) + i, k(0.05) + i, k(0.0145) + i], mat, [hb], rot=R),
            # Knuckle bar under the finger bases.
            round_cone(self.to_bind(idx_base + np.array([0, k(0.006), k(0.001)])), self.to_bind(pky_base + np.array([0, k(0.006), k(0.001)])),
                       k(0.0135) + i, k(0.0115) + i, mat, [hb]),
            # Thenar (thumb) and hypothenar pads on the palm side.
            ellipsoid(self.to_bind([k(0.02) * t, k(-0.03), k(0.008)]), [k(0.019) + i, k(0.027) + i, k(0.012) + i], mat, [hb], rot=R),
            ellipsoid(self.to_bind([k(-0.024) * t, k(-0.045), k(0.006)]), [k(0.013) + i, k(0.03) + i, k(0.011) + i], mat, [hb], rot=R),
            # Wrist.
            ellipsoid(self.to_bind([0, k(-0.004), 0]), [k(0.028) + i, k(0.022), k(0.018) + i], mat, [hb, "forearm_" + self.s], rot=R),
        ]
        for name, pts, radii, d in self.chains:
            for j in range(3):
                bname = "%s%d_%s" % (name, j + 1, self.s)
                a = self.to_bind(pts[j])
                b = self.to_bind(pts[j + 1])
                parts.append(round_cone(a, b, radii[j] + i, radii[j + 1] + i, mat, [bname], squash=(1.08, 0.9), ref=R[:, 2]))
        return Union(parts, k(0.007))
