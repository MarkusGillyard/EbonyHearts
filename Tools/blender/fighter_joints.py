"""Half joints: a helper bone on each limb joint that turns half way between the bones either side.

Skinning blends a vertex between two bones along a straight line, so a bent or twisted joint loses
its width: at 135 degrees the middle of a knee keeps 38% of its radius. A bone at half the turn takes
the middle of the joint, which leaves two blends of half the angle each (83% at their worst, and the
middle keeps all of it). It also shares a twist across the joint instead of wringing one ring of
vertices. The weights are split from what each mesh already has, so garments follow the body.

The game turns these bones in FMeshRig::Pose; drive_half_joints does the same for review renders.
"""
import math

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

import fighter_geo as geo

# Joint, the bone above it (the helper's parent), the bone below it (whose head is the pivot) and the
# bone whose length a twist at this joint runs along.
JOINTS = (("shoulder", "clavicle", "upperarm", "upperarm"), ("elbow", "upperarm", "lowerarm", "lowerarm"),
          ("wrist", "lowerarm", "hand", "lowerarm"), ("hip", "pelvis", "thigh", "thigh"),
          ("knee", "thigh", "calf", "calf"), ("ankle", "calf", "foot", "calf"))
NEAR, FAR = 0.45, 0.8


def half_joints():
    """(helper, upper bone, lower bone, twist bone) for both sides."""
    for joint, upper, lower, along in JOINTS:
        for s in "lr":
            yield ("%s_half_%s" % (joint, s), upper if upper == "pelvis" else "%s_%s" % (upper, s),
                   "%s_%s" % (lower, s), "%s_%s" % (along, s))


def add_bones(rig):
    with bpy.context.temp_override(active_object=rig, object=rig, selected_objects=[rig]):
        bpy.context.view_layer.objects.active = rig
        bpy.ops.object.mode_set(mode="EDIT")
        eb = rig.data.edit_bones
        for name, upper, lower, _ in half_joints():
            b = eb.new(name)
            b.head = eb[lower].head.copy()
            b.tail = eb[lower].head + eb[lower].vector.normalized() * 0.04
            b.roll = eb[lower].roll
            b.parent = eb[upper]
            b.use_connect = False
            b.use_deform = True
        bpy.ops.object.mode_set(mode="OBJECT")


def split_weights(ob, rig):
    """Moves the middle of each joint's blend onto its helper. Returns the vertices moved. The share
    fades with distance from the joint (fractions of the limb bone's length): a skirt's hem blends
    between hip and thigh too, but turned rigidly about a joint a metre away it would swing far wide."""
    W = geo.weights(ob)
    V = geo.co(ob)
    moved = 0
    for name, upper, lower, along in half_joints():
        if upper not in W or lower not in W:
            continue
        s = W[upper] + W[lower]
        both = (W[upper] > 0) & (W[lower] > 0)
        if not both.any():
            continue
        t = np.where(both, W[lower] / np.maximum(s, 1e-9), 0.0)
        near = np.array(rig.data.bones[lower].head_local)
        reach = (np.linalg.norm(V - near, axis=1) / rig.data.bones[along].length - NEAR) / (FAR - NEAR)
        k = np.clip(reach, 0.0, 1.0)
        k = 1 - k * k * (3 - 2 * k)
        W[name] = np.where(both, s * k * (1 - np.abs(2 * t - 1)), 0.0)
        W[upper] = np.where(both, s * ((1 - k) * (1 - t) + k * np.maximum(1 - 2 * t, 0)), W[upper])
        W[lower] = np.where(both, s * ((1 - k) * t + k * np.maximum(2 * t - 1, 0)), W[lower])
        moved += int(both.sum())
    if moved:
        geo.set_weights(ob, W)
        geo.clean_weights(ob, limit=8)
    return moved


def add_half_joints(rig, objects):
    add_bones(rig)
    return {ob.name: split_weights(ob, rig) for ob in objects if ob.type == "MESH" and ob.vertex_groups}


def half_turn(turn, axis):
    """FMeshRig::HalfTurn: half the bend and half the twist about axis, the twist's share easing to
    nothing before 180 degrees, where its half would flip sides."""
    along = axis * Vector(turn[1:4]).dot(axis)
    twist = Quaternion((turn.w, along.x, along.y, along.z))
    twist = twist.normalized() if twist.magnitude > 1e-9 else Quaternion()
    if twist.w < 0:
        twist.negate()
    swing = turn @ twist.inverted()
    angle = 2 * math.atan2(Vector(twist[1:4]).dot(axis), twist.w)
    t = min(max((abs(angle) - math.radians(110)) / (math.pi - math.radians(110)), 0.0), 1.0)
    share = 0.5 * (1 - t * t * (3 - 2 * t))
    return Quaternion().slerp(swing, 0.5) @ Quaternion(axis, angle * share)


def drive_half_joints(arm):
    """Poses the helpers as the game does, on the lower bone's head."""
    bpy.context.view_layer.update()
    for name, upper, lower, along in half_joints():
        if name not in arm.pose.bones:
            continue
        pb, up, low, bone = arm.pose.bones[name], arm.pose.bones[upper], arm.pose.bones[lower], arm.data.bones[along]
        turn_up = (up.matrix @ up.bone.matrix_local.inverted()).to_quaternion()
        turn_low = (low.matrix @ low.bone.matrix_local.inverted()).to_quaternion()
        axis = (bone.tail_local - bone.head_local).normalized()
        half = (turn_up @ half_turn(turn_up.inverted() @ turn_low, axis)).to_matrix().to_4x4()
        rest = pb.bone.matrix_local.to_3x3().to_4x4()
        pb.matrix = Matrix.Translation(low.head) @ half @ rest
    bpy.context.view_layer.update()
