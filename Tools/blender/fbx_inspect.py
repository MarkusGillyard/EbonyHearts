"""Dump the parts of a binary FBX that matter for Unreal: axis/unit GlobalSettings, the node
tree with types and Lcl transforms, and counts of geometry, skin clusters, materials, textures.

    python3 -I Tools/blender/fbx_inspect.py SourceArt/Bodies/athletic_woman.fbx
"""
import struct
import sys
import zlib


def read_props(f, n):
    out = []
    for _ in range(n):
        t = f.read(1)
        if t == b"Y": out.append(struct.unpack("<h", f.read(2))[0])
        elif t == b"C": out.append(bool(f.read(1)[0]))
        elif t == b"I": out.append(struct.unpack("<i", f.read(4))[0])
        elif t == b"F": out.append(struct.unpack("<f", f.read(4))[0])
        elif t == b"D": out.append(struct.unpack("<d", f.read(8))[0])
        elif t == b"L": out.append(struct.unpack("<q", f.read(8))[0])
        elif t in b"fdlibc" and len(t) == 1:
            length, enc, clen = struct.unpack("<III", f.read(12))
            data = f.read(clen)
            if enc == 1:
                data = zlib.decompress(data)
            fmt = {"f": "f", "d": "d", "l": "q", "i": "i", "b": "b", "c": "b"}[t.decode()]
            out.append(("array", t.decode(), length, struct.unpack("<%d%s" % (length, fmt), data)
                        if length < 64 else None))
        elif t in (b"S", b"R"):
            ln = struct.unpack("<I", f.read(4))[0]
            raw = f.read(ln)
            out.append(raw.decode("utf-8", "replace") if t == b"S" else ("raw", ln))
        else:
            raise ValueError("unknown prop type %r" % t)
    return out


def read_node(f, wide):
    if wide:
        end, nprops, plen = struct.unpack("<QQQ", f.read(24))
    else:
        end, nprops, plen = struct.unpack("<III", f.read(12))
    nlen = f.read(1)[0]
    name = f.read(nlen).decode()
    if end == 0:
        return None
    props = read_props(f, nprops)
    children = []
    while f.tell() < end:
        c = read_node(f, wide)
        if c is None:
            break
        children.append(c)
    f.seek(end)
    return (name, props, children)


def load(path):
    with open(path, "rb") as f:
        head = f.read(27)
        assert head[:20] == b"Kaydara FBX Binary  ", "not a binary FBX"
        version = struct.unpack("<I", head[23:27])[0]
        wide = version >= 7500
        nodes = []
        while True:
            n = read_node(f, wide)
            if n is None:
                break
            nodes.append(n)
    return version, nodes


def child(node, name):
    return [c for c in node[2] if c[0] == name]


def p70(node):
    out = {}
    for pp in child(node, "Properties70"):
        for p in pp[2]:
            out[p[1][0]] = p[1][4:]
    return out


def main(path):
    version, nodes = load(path)
    top = {n[0]: n for n in nodes}
    print("FBX version", version)
    gs = p70(top["GlobalSettings"])
    for k in ("UpAxis", "UpAxisSign", "FrontAxis", "FrontAxisSign", "CoordAxis", "CoordAxisSign",
              "UnitScaleFactor", "OriginalUnitScaleFactor"):
        print("  %-24s %s" % (k, gs.get(k)))
    objs = top["Objects"][2]
    models = {}
    counts = {}
    for o in objs:
        counts[o[0]] = counts.get(o[0], 0) + 1
        if o[0] == "Model":
            uid, nm, typ = o[1][0], o[1][1].split("\x00")[0], o[1][2]
            props = p70(o)
            models[uid] = (nm, typ, props)
        if o[0] == "Deformer" and o[1][2] == "Cluster":
            counts["SkinCluster"] = counts.get("SkinCluster", 0) + 1
        if o[0] == "Geometry":
            verts = child(o, "Vertices")[0][1][0][2] // 3
            idx = child(o, "PolygonVertexIndex")[0][1][0][2]
            print("  Geometry %-28s verts=%d polygon-vertex-indices=%d layers=%s" % (
                o[1][1].split("\x00")[0], verts, idx,
                sorted({c[0] for c in o[2] if c[0].startswith("Layer")})))
    print("  Object counts", counts)
    parent = {}
    for c in child(top["Connections"], "C"):
        # bones are also connected (OO) to their skin clusters; keep only model->model / root
        if c[1][0] == "OO" and c[1][1] in models and (c[1][2] in models or c[1][2] == 0):
            parent[c[1][1]] = c[1][2]

    def depth(u):
        d = 0
        while u in parent and parent[u] in models:
            u = parent[u]
            d += 1
        return d

    roots = [u for u in models if parent.get(u) not in models]
    print("  Model roots:", [(models[u][0], models[u][1]) for u in roots])
    for u, (nm, typ, props) in models.items():
        if typ in ("Null", "Mesh") or depth(u) <= 1:
            print("  %-28s %-9s depth=%d T=%s R=%s S=%s" % (
                nm, typ, depth(u), props.get("Lcl Translation"), props.get("Lcl Rotation"),
                props.get("Lcl Scaling")))
    limbs = [m for m in models.values() if m[1] == "LimbNode"]
    print("  LimbNodes:", len(limbs))


if __name__ == "__main__":
    main(sys.argv[1])
