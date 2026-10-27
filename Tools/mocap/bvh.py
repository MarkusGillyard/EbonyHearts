"""BVH reading and forward kinematics, plain Python (no numpy).

    clip = load("take.bvh"); clip.frames (count), clip.dt (seconds a frame)
    pos = clip.world(i)  -> {joint name: (x, y, z)} in the file's units for frame i (fractional i
                            interpolates between frames)
"""
import math


def _rot(axis, deg):
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    if axis == "X":
        return ((1, 0, 0), (0, c, -s), (0, s, c))
    if axis == "Y":
        return ((c, 0, s), (0, 1, 0), (-s, 0, c))
    return ((c, -s, 0), (s, c, 0), (0, 0, 1))


def mul(a, b):
    return tuple(tuple(sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)) for i in range(3))


def apply(m, v):
    return tuple(m[i][0] * v[0] + m[i][1] * v[1] + m[i][2] * v[2] for i in range(3))


IDENT = ((1, 0, 0), (0, 1, 0), (0, 0, 1))


class Joint:
    def __init__(self, name, parent):
        self.name, self.parent = name, parent
        self.offset = (0.0, 0.0, 0.0)
        self.channels = []
        self.children = []
        self.end = None  # End Site offset


class Clip:
    def __init__(self, path):
        toks = open(path).read().split()
        self.joints = []
        i = 0
        stack = []
        cur = None
        while toks[i] != "MOTION":
            t = toks[i]
            if t in ("ROOT", "JOINT"):
                j = Joint(toks[i + 1], stack[-1] if stack else None)
                if j.parent:
                    j.parent.children.append(j)
                self.joints.append(j)
                cur = j
                i += 2
            elif t == "End":
                cur = ("end", stack[-1])
                i += 2
            elif t == "{":
                stack.append(cur if isinstance(cur, Joint) else stack[-1])
                if not isinstance(cur, Joint):
                    stack[-1] = ("end", cur[1])
                i += 1
            elif t == "}":
                stack.pop()
                i += 1
            elif t == "OFFSET":
                v = tuple(float(x) for x in toks[i + 1:i + 4])
                if isinstance(stack[-1], tuple):
                    stack[-1][1].end = v
                else:
                    stack[-1].offset = v
                i += 4
            elif t == "CHANNELS":
                n = int(toks[i + 1])
                stack[-1].channels = toks[i + 2:i + 2 + n]
                i += 2 + n
            else:
                i += 1
        i += 1
        assert toks[i] == "Frames:"
        self.frames = int(toks[i + 1])
        assert toks[i + 2] == "Frame" and toks[i + 3] == "Time:"
        self.dt = float(toks[i + 4])
        vals = [float(x) for x in toks[i + 5:]]
        self.nch = sum(len(j.channels) for j in self.joints)
        self.data = [vals[f * self.nch:(f + 1) * self.nch] for f in range(self.frames)]
        self.byname = {j.name: j for j in self.joints}

    def _world_frame(self, f):
        row = self.data[f]
        k = 0
        out = {}
        mats = {}
        for j in self.joints:
            pos = list(j.offset)
            m = IDENT
            for c in j.channels:
                v = row[k]
                k += 1
                if c.endswith("position"):
                    pos["XYZ".index(c[0])] = v
                else:
                    m = mul(m, _rot(c[0], v))
            if j.parent is None:
                wp, wm = tuple(pos), m
            else:
                pm, pp = mats[j.parent.name], out[j.parent.name]
                d = apply(pm, tuple(pos) if not any(c.endswith("position") for c in j.channels) else tuple(pos))
                wp = tuple(pp[a] + d[a] for a in range(3))
                wm = mul(pm, m)
            out[j.name], mats[j.name] = wp, wm
            if j.end:
                d = apply(wm, j.end)
                out[j.name + "_end"] = tuple(wp[a] + d[a] for a in range(3))
        return out

    def world(self, t):
        t = max(0.0, min(self.frames - 1.0, t))
        a = int(math.floor(t))
        b = min(a + 1, self.frames - 1)
        u = t - a
        pa = self._world_frame(a)
        if u < 1e-6 or a == b:
            return pa
        pb = self._world_frame(b)
        return {k: tuple(pa[k][i] * (1 - u) + pb[k][i] * u for i in range(3)) for k in pa}


def load(path):
    return Clip(path)
