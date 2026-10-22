"""The fighter's skeleton in the bind pose, as exported by tools/assets/export_rigs.gd.

Coordinates are rig space: x to the fighter's right, y up, -z toward the opponent. Each joint
has the Rig's own frame (its basis columns) so primitives can be placed relative to a joint and
skin binds match what the game computes.
"""
import json

import numpy as np


class Rig:
    def __init__(self, path):
        d = json.load(open(path))
        self.data = d
        self.id = d["id"]
        self.h = float(d["height"])
        self.look = d["look"]
        self.look_hash = d["look_hash"]
        self.lengths = d["lengths"]
        self.cloth = d["cloth"]
        self.names = [j["name"] for j in d["joints"]]
        self.parent = {j["name"]: j["parent"] for j in d["joints"]}
        self.basis = {}
        self.origin = {}
        for j in d["joints"]:
            b = np.array(j["bind"], np.float32)
            self.basis[j["name"]] = np.stack([b[0:3], b[3:6], b[6:9]], axis=1)
            self.origin[j["name"]] = b[9:12]

    def pos(self, joint):
        return self.origin[joint].copy()

    def at(self, joint, local):
        """A point given in a joint's frame."""
        return self.origin[joint] + self.basis[joint] @ np.asarray(local, np.float32)

    def xform(self, joint):
        """4 x 4 bind transform of a joint."""
        m = np.eye(4, dtype=np.float32)
        m[:3, :3] = self.basis[joint]
        m[:3, 3] = self.origin[joint]
        return m

    def side(self, s):
        return -1.0 if s == "l" else 1.0

    def limb_frame(self, a, b, s, front=(0, 0, -1)):
        """Anatomical frame for the bone from joint a to joint b: along, front and lateral
        (pointing away from the body's midline) unit vectors."""
        along = self.pos(b) - self.pos(a)
        along /= np.linalg.norm(along)
        fr = np.asarray(front, np.float32)
        fr = fr - along * np.dot(fr, along)
        fr /= np.linalg.norm(fr)
        lat = np.cross(along, fr)
        if np.dot(lat, [self.side(s), 0, 0]) < 0:
            lat = -lat
        return along, fr, lat
