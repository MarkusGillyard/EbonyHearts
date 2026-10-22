"""Writes .wfm character models: a JSON header followed by one deflate-compressed block of
little-endian arrays. Read by scripts/anim/character_model.gd."""
import json
import struct
import zlib

import numpy as np

MAGIC = b"WFM1"
VERSION = 1


class Writer:
    def __init__(self):
        self.blob = bytearray()
        self.meshes = []
        self.extra = {}

    def _put(self, arr, dtype):
        a = np.ascontiguousarray(arr, dtype=dtype)
        off = len(self.blob)
        self.blob += a.tobytes()
        return {"offset": off, "bytes": a.nbytes}

    def add_mesh(self, name, skeleton, surfaces, arrays, extra=None):
        """`arrays`: vertex (N,3) f32, normal (N,3) f32, params (N,4) f32 (roughness, sss, sheen,
        metallic), albedo (N,4) f32 (sRGB + ambient occlusion), bind (N,4) f32 (bind position,
        detail kind), bones (N,k) i32, weights (N,k) f32, index (M,) i32 wound counter-clockwise
        seen from outside. `surfaces` is a list of {"material", "start", "count"} ranges of the
        index array."""
        types = {"vertex": np.float32, "normal": np.float32, "params": np.float32, "albedo": np.float32,
                 "bind": np.float32, "bones": np.int32, "weights": np.float32, "index": np.int32,
                 "uv": np.float32, "uv2": np.float32, "tangent": np.float32}
        attrs = {}
        arrays = dict(arrays)
        # The sculptor winds triangles counter-clockwise seen from outside; Godot treats
        # clockwise as the front face.
        arrays["index"] = np.asarray(arrays["index"]).reshape(-1, 3)[:, ::-1].reshape(-1)
        for k, a in arrays.items():
            attrs[k] = self._put(a, types[k])
            attrs[k]["width"] = 1 if a.ndim == 1 else a.shape[1]
        m = {"name": name, "skeleton": skeleton, "vertices": len(arrays["vertex"]), "surfaces": surfaces, "arrays": attrs}
        if extra:
            m.update(extra)
        self.meshes.append(m)

    def save(self, path, header):
        header = dict(header)
        header["version"] = VERSION
        header["meshes"] = self.meshes
        header.update(self.extra)
        raw = bytes(self.blob)
        header["payload_bytes"] = len(raw)
        comp = zlib.compress(raw, 9)
        hj = json.dumps(header, separators=(",", ":")).encode("utf-8")
        with open(path, "wb") as fh:
            fh.write(MAGIC)
            fh.write(struct.pack("<II", len(hj), len(comp)))
            fh.write(hj)
            fh.write(comp)
        return len(hj) + len(comp) + 12
