"""Review-render helpers shared by build_fighter.py (dev stages) and render_fighter.py:
neutral grey studio with a warm key, a cool rim and a soft camera-side fill, plus the pose
library used for deformation checks (bones driven by world-space hinge rotations)."""
import math

import bpy
import numpy as np
from mathutils import Matrix, Vector

X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))


def setup_studio(res=1024, samples=48):
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    sc.eevee.taa_render_samples = samples
    sc.render.resolution_x = sc.render.resolution_y = res
    sc.render.resolution_percentage = 100
    sc.render.image_settings.file_format = "PNG"
    sc.render.film_transparent = False
    sc.view_settings.view_transform = "AgX"
    sc.view_settings.look = "AgX - Base Contrast"
    world = sc.world or bpy.data.worlds.new("World")
    sc.world = world
    bg = world.node_tree.nodes.get("Background")
    bg.inputs["Color"].default_value = (0.32, 0.32, 0.33, 1.0)
    bg.inputs["Strength"].default_value = 0.55

    def sun(name, energy, color, rot, angle=6):
        ob = bpy.data.objects.new(name, bpy.data.lights.new(name, "SUN"))
        ob.data.energy = energy
        ob.data.color = color
        ob.data.angle = math.radians(angle)
        ob.rotation_euler = [math.radians(r) for r in rot]
        sc.collection.objects.link(ob)
        return ob

    sun("KeyWarm", 4.0, (1.0, 0.82, 0.62), (52, 0, -38))       # warm key, front-left high
    sun("RimCool", 5.0, (0.55, 0.72, 1.0), (-62, 0, 20), 3)    # cool rim from behind
    sun("FillCam", 0.7, (0.95, 0.95, 1.0), (82, 0, 8), 25)     # soft fill from the camera side
    me = bpy.data.meshes.new("Floor")
    me.from_pydata([(-60, -60, 0), (60, -60, 0), (60, 60, 0), (-60, 60, 0)], [], [(0, 1, 2, 3)])
    floor = bpy.data.objects.new("Floor", me)
    mat = bpy.data.materials.new("FloorGrey")
    p = mat.node_tree.nodes["Principled BSDF"]
    p.inputs["Base Color"].default_value = (0.16, 0.16, 0.165, 1)
    p.inputs["Roughness"].default_value = 0.85
    me.materials.append(mat)
    sc.collection.objects.link(floor)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    cam.data.clip_start, cam.data.clip_end = 0.02, 200
    sc.collection.objects.link(cam)
    sc.camera = cam
    return cam


def look_at(cam, eye, target):
    cam.location = Vector(eye)
    cam.rotation_euler = (Vector(target) - Vector(eye)).to_track_quat("-Z", "Y").to_euler()


def render(path):
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def bbox(objs):
    dg = bpy.context.evaluated_depsgraph_get()
    pts = []
    for ob in objs:
        ev = ob.evaluated_get(dg)
        me = ev.to_mesh()
        co = np.empty(len(me.vertices) * 3)
        me.vertices.foreach_get("co", co)
        ev.to_mesh_clear()
        M = np.array(ob.matrix_world)
        pts.append(co.reshape(-1, 3) @ M[:3, :3].T + M[:3, 3])
    pts = np.concatenate(pts)
    return pts.min(0), pts.max(0)


def frame_ortho(cam, objs, view, margin=1.08):
    """Orthographic full-figure framing from a world direction (camera looks along -view)."""
    lo, hi = bbox(objs)
    c = (lo + hi) / 2
    size = max(hi[2] - lo[2], abs(hi[0] - lo[0]), abs(hi[1] - lo[1]))
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = size * margin
    v = Vector(view).normalized()
    look_at(cam, Vector(c) + v * 20, Vector(c))


def frame_persp(cam, objs, view, lens=50, margin=1.15):
    lo, hi = bbox(objs)
    c = Vector((lo + hi) / 2)
    r = float(np.linalg.norm(hi - lo)) / 2
    cam.data.type = "PERSP"
    cam.data.lens = lens
    fov = 2 * math.atan(18 / lens)
    dist = r * margin / math.sin(fov / 2)
    look_at(cam, c + Vector(view).normalized() * dist, c)


def composite_lr(left, right, out):
    """Two renders of the same size side by side (each cropped to its middle half)."""
    tiles = []
    for p in (left, right):
        img = bpy.data.images.load(p)
        w, h = img.size
        a = np.empty(w * h * 4, np.float32)
        img.pixels.foreach_get(a)
        a = a.reshape(h, w, 4)[:, w // 4: w // 4 + w // 2]
        tiles.append(a)
        bpy.data.images.remove(img)
    full = np.concatenate(tiles, axis=1)
    img = bpy.data.images.new("lr", full.shape[1], full.shape[0], alpha=True)
    img.pixels.foreach_set(full.ravel())
    img.filepath_raw = out
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)


# ----------------------------------------------------------------------------- posing

def rotate_bone(arm, name, axis_world, angle_deg, want_world=None):
    """Rotate pose bone about a world axis through its head. With want_world, the sign is
    chosen so the bone direction moves towards it; otherwise the angle is taken as given."""
    bpy.context.view_layer.update()
    pb = arm.pose.bones[name]
    to_arm = arm.matrix_world.to_3x3().normalized().inverted()
    axis = (to_arm @ Vector(axis_world)).normalized()
    d = (pb.tail - pb.head).normalized()
    cands = [1] if want_world is None else [1, -1]
    best = None
    for sgn in cands:
        R = Matrix.Rotation(math.radians(sgn * angle_deg), 4, axis)
        score = 0 if want_world is None else (R.to_3x3() @ d).dot((to_arm @ Vector(want_world)).normalized())
        if best is None or score > best[0]:
            best = (score, R)
    head = pb.head.copy()
    pb.matrix = Matrix.Translation(head) @ best[1] @ Matrix.Translation(-head) @ pb.matrix
    bpy.context.view_layer.update()
    return arm.matrix_world.to_3x3().normalized() @ best[1].to_3x3() @ to_arm


def bone_dir(arm, name):
    pb = arm.pose.bones[name]
    return (arm.matrix_world.to_3x3() @ (pb.tail - pb.head)).normalized()


def bone_head(arm, name):
    return arm.matrix_world @ arm.pose.bones[name].head


def reset_pose(arm):
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    arm.location.z = 0.0
    bpy.context.view_layer.update()


def leg(arm, s, hip_flex, knee_flex, abduct=0.0, foot_flat=True, foot_extra=0.0):
    """hip_flex > 0 swings the thigh forward; knee_flex > 0 bends the knee (shin back);
    abduct > 0 moves the leg outwards; the foot is counter-rotated to stay level."""
    out = -X if s == "r" else X
    R = Matrix.Identity(3)
    if abduct:
        R = rotate_bone(arm, "thigh_" + s, Y, abduct, out)
    hinge = (R @ X).normalized()
    if hip_flex:
        rotate_bone(arm, "thigh_" + s, hinge, hip_flex, -Y if hip_flex > 0 else Y)
    if knee_flex:
        rotate_bone(arm, "calf_" + s, hinge, knee_flex, Y if hip_flex < 60 else -Z)
    if foot_flat:
        # a forward-swung shin tips the toes up, a backward one tips them down: level the sole
        net = hip_flex - knee_flex
        if abs(net) > 0.5:
            rotate_bone(arm, "foot_" + s, hinge, abs(net), -Z if net > 0 else Z)


def spine(arm, bend=0.0, twist=0.0):
    """bend > 0 leans forward (spread over three spine bones); twist > 0 turns the chest left."""
    for b in ("spine_01", "spine_02", "spine_03"):
        if bend:
            rotate_bone(arm, b, X, abs(bend) / 3, -Y if bend > 0 else Y)
        if twist:
            rotate_bone(arm, b, Z, twist / 3)


def fist(arm, s, amount=1.0):
    """Curl fingers (01/02/03 segments) towards the palm; thumb tucks less."""
    hand = "hand_" + s
    for f, ang in (("index", 80), ("middle", 85), ("ring", 88), ("pinky", 90), ("thumb", 40)):
        for seg in ("01", "02", "03"):
            b = "%s_%s_%s" % (f, seg, s)
            if b not in arm.pose.bones:
                continue
            ih, ph = bone_head(arm, "index_01_" + s), bone_head(arm, "pinky_01_" + s)
            across = (ih - ph).normalized()
            hd = bone_dir(arm, hand)
            palm = across.cross(hd).normalized()
            th = bone_head(arm, "thumb_01_" + s)
            mid = (ih + ph) / 2
            if palm.dot(th - mid) < 0:
                palm = -palm
            a = ang * amount * (0.6 if (f == "thumb" and seg == "01") else 1.0)
            rotate_bone(arm, b, across, a, palm)


def arm_guard(arm, s, raise_fwd=40, elbow=125, inward=12, high=False):
    """Upper arm swings forward (and in), elbow bends so the fist comes up to the face."""
    side = X if s == "l" else -X
    hinge = bone_dir(arm, "upperarm_" + s).cross(Y).normalized()
    R = rotate_bone(arm, "upperarm_" + s, hinge, raise_fwd, -Y)
    if inward:
        rotate_bone(arm, "upperarm_" + s, Z, inward, -side)
    if high:
        rotate_bone(arm, "upperarm_" + s, Y, high, Z)
    h2 = bone_dir(arm, "upperarm_" + s).cross(bone_dir(arm, "lowerarm_" + s))
    if h2.length < 1e-3:
        h2 = R @ hinge
    rotate_bone(arm, "lowerarm_" + s, h2.normalized(), elbow, Z)


def ground(arm, meshes):
    lo, _ = bbox(meshes)
    arm.location.z -= lo[2]
    bpy.context.view_layer.update()


POSES = ("stance", "kick", "crouch", "punch", "guard_up")


def apply_pose(arm, name):
    reset_pose(arm)
    if name == "stance":            # Muay Thai stance, left foot forward
        leg(arm, "l", 20, 24, abduct=6)
        leg(arm, "r", -16, 8, abduct=6)
        spine(arm, bend=6, twist=-12)
        arm_guard(arm, "l", 45, 120, 14)
        arm_guard(arm, "r", 38, 135, 18)
        fist(arm, "l"); fist(arm, "r")
    elif name == "kick":            # rear-leg high front kick, thigh ~100 deg up
        leg(arm, "l", 8, 18, abduct=4)
        leg(arm, "r", 100, 12, abduct=4, foot_flat=False)
        spine(arm, bend=-12)
        arm_guard(arm, "l", 45, 120, 14)
        arm_guard(arm, "r", 20, 100, 0)
        fist(arm, "l"); fist(arm, "r")
    elif name == "crouch":          # deep crouch, knees out
        leg(arm, "l", 112, 135, abduct=22)
        leg(arm, "r", 112, 135, abduct=22)
        spine(arm, bend=22)
        arm_guard(arm, "l", 55, 115, 10)
        arm_guard(arm, "r", 55, 115, 10)
        fist(arm, "l"); fist(arm, "r")
    elif name == "punch":           # right cross fully extended at shoulder height
        leg(arm, "l", 24, 20, abduct=6)
        leg(arm, "r", -20, 14, abduct=6)
        spine(arm, bend=8, twist=24)
        hinge = bone_dir(arm, "upperarm_r").cross(Y).normalized()
        rotate_bone(arm, "upperarm_r", hinge, 82, -Y)
        rotate_bone(arm, "upperarm_r", Z, 12, X)
        h2 = bone_dir(arm, "upperarm_r").cross(bone_dir(arm, "lowerarm_r"))
        if h2.length > 1e-3:   # straighten the elbow
            d1, d2 = bone_dir(arm, "upperarm_r"), bone_dir(arm, "lowerarm_r")
            ang = math.degrees(d1.angle(d2))
            rotate_bone(arm, "lowerarm_r", h2.normalized(), ang, d1)
        arm_guard(arm, "l", 45, 125, 14)
        fist(arm, "l"); fist(arm, "r")
    elif name == "guard_up":        # both arms up high, forearms over the head
        leg(arm, "l", 16, 16, abduct=6)
        leg(arm, "r", -10, 18, abduct=6)
        for s in ("l", "r"):
            side = X if s == "l" else -X
            hinge = bone_dir(arm, "upperarm_" + s).cross(Y).normalized()
            rotate_bone(arm, "upperarm_" + s, Y, 115, Z)
            rotate_bone(arm, "upperarm_" + s, Z, 25, -Y + 0 * side)
            h2 = bone_dir(arm, "upperarm_" + s).cross(bone_dir(arm, "lowerarm_" + s))
            if h2.length < 1e-3:
                h2 = hinge
            rotate_bone(arm, "lowerarm_" + s, h2.normalized(), 120, -side)
            fist(arm, s)
    bpy.context.view_layer.update()
