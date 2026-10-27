"""Bake shop work and a customer wave, with planted feet and a rigid counter."""

import math

import bpy
from mathutils import Quaternion, Vector

CLIPS = {'idle': (96, True), 'sort': (144, False),
         'pack': (120, False), 'greet': (72, False)}
PALM_DOWN = Quaternion((0, 0, 1), math.pi) @ Quaternion((1, 0, 0), math.pi/2)
REST_L, REST_R = Vector((-.44, -.10, 1.01)), Vector((.44, -.10, 1.01))


def global_rotation(rig, name, rotation):
    bone = rig.pose.bones[name]
    base = bone.bone.matrix_local.copy()
    if bone.parent:
        base = bone.parent.matrix @ bone.parent.bone.matrix_local.inverted() @ base
    bone.rotation_quaternion = base.to_quaternion().inverted() @ rotation
    bpy.context.view_layer.update()


def solve_arm(rig, side, wrist, delta):
    arm, forearm = rig.pose.bones['arm.'+side], rig.pose.bones['hand.'+side]
    shoulder = arm.head.copy()
    reach = Vector(wrist)-shoulder
    # Authored targets must remain inside the two-link reach, avoiding stretching.
    assert reach.length <= arm.bone.length+forearm.bone.length+.0001, (side, tuple(wrist), reach.length)
    distance = min(reach.length, arm.bone.length+forearm.bone.length-.0001)
    direction = reach.normalized()
    along = (arm.bone.length**2-forearm.bone.length**2+distance**2)/(2*distance)
    height = math.sqrt(max(0., arm.bone.length**2-along**2))
    pole = Vector((-1 if side == 'L' else 1, .32, -.08))
    pole = (pole-direction*pole.dot(direction)).normalized()
    elbow = shoulder+direction*along+pole*height
    for name, aim in [('arm.'+side, elbow-shoulder), ('hand.'+side, Vector(wrist)-elbow)]:
        rest = rig.data.bones[name]
        rotation = (rest.tail_local-rest.head_local).rotation_difference(aim)
        global_rotation(rig, name, rotation @ rest.matrix_local.to_quaternion())
    global_rotation(rig, 'wrist.'+side, delta @ rig.data.bones['wrist.'+side].matrix_local.to_quaternion())


def smooth(t):
    t = max(0., min(1., t))
    return t*t*(3-2*t)


def key_sample(frame, keys):
    for first, last in zip(keys, keys[1:]):
        if first[0] <= frame <= last[0]:
            t = smooth((frame-first[0])/(last[0]-first[0]))
            return Vector(first[1]).lerp(Vector(last[1]), t), first[2]+(last[2]-first[2])*t
    raise ValueError(frame)


def pose(rig, clip, frame, length):
    for bone in rig.pose.bones:
        bone.rotation_mode = 'QUATERNION'
        bone.rotation_quaternion = Quaternion()
        bone.location = (0, 0, 0)
        bone.scale = (1, 1, 1)
    t, phase = frame/length, math.tau*frame/length
    left, right = REST_L.copy(), REST_R.copy()
    ql, qr = Quaternion(), Quaternion()
    bend, nod, glance = .007*math.sin(phase), -.005*math.sin(phase), .018*math.sin(phase)
    if clip == 'idle':
        left.y += .003*math.sin(phase)
        right.y -= .003*math.sin(phase)
    elif clip == 'sort':
        right, engage = key_sample(frame, [
            (0, REST_R, 0), (24, (.40, -.20, 1.30), 1),
            (52, (.23, -.40, 1.36), 1), (68, (.31, -.35, 1.33), 1),
            (90, (.23, -.40, 1.36), 1), (108, (.38, -.26, 1.33), 1),
            (132, REST_R, 0), (144, REST_R, 0)])
        qr = Quaternion().slerp(PALM_DOWN, engage)
        left = REST_L.lerp(Vector((-.36, -.24, 1.20)), engage)
        ql = Quaternion().slerp(PALM_DOWN, engage*.55)
        bend += .035*engage
        nod += .10*engage
        glance -= .07*engage
    elif clip == 'pack':
        left, engage = key_sample(frame, [
            (0, REST_L, 0), (24, (-.15, -.40, 1.38), 1),
            (44, (-.10, -.42, 1.37), 1), (62, (-.19, -.39, 1.37), 1),
            (80, (-.09, -.42, 1.38), 1), (96, (-.18, -.38, 1.40), 1),
            (114, REST_L, 0), (120, REST_L, 0)])
        right, unused = key_sample(frame, [
            (0, REST_R, 0), (24, (.14, -.40, 1.43), 1),
            (44, (.12, -.41, 1.42), 1), (62, (.22, -.38, 1.42), 1),
            (80, (.09, -.42, 1.43), 1), (96, (.21, -.37, 1.44), 1),
            (114, REST_R, 0), (120, REST_R, 0)])
        ql, qr = Quaternion().slerp(PALM_DOWN, engage), Quaternion().slerp(PALM_DOWN, engage)
        bend += .035*engage
        nod += .12*engage
    else:
        attend = smooth(t/.23)*(1-smooth((t-.75)/.25))
        wave = math.sin(phase*2)*.17*attend
        left = REST_L.lerp(Vector((-.40, -.14, 1.57)), attend)
        ql = Quaternion().slerp(Quaternion((0, 0, 1), wave) @ Quaternion((0, 1, 0), -2.75), attend)
        nod += .07*math.sin(phase)*attend
        glance -= .10*attend
    rig.pose.bones['spine'].rotation_quaternion = Quaternion((1, 0, 0), bend)
    rig.pose.bones['head'].rotation_quaternion = Quaternion((0, 1, 0), glance) @ Quaternion((1, 0, 0), nod)
    bpy.context.view_layer.update()
    solve_arm(rig, 'L', left, ql)
    solve_arm(rig, 'R', right, qr)


def author_clips(rig):
    scene = bpy.context.scene
    scene.render.fps = 24
    rig.animation_data_create()
    result = {}
    for clip, (length, loop) in CLIPS.items():
        rig.animation_data.action = None
        for frame in range(length+1):
            pose(rig, clip, frame, length)
            for bone in rig.pose.bones:
                bone.keyframe_insert('rotation_quaternion', frame=frame+1, group=bone.name)
        action = rig.animation_data.action
        action.name = 'NPC_MERCHANT_'+clip.upper()
        action.use_fake_user = True
        action['loop'], action['fps'], action['stationary'] = loop, 24, True
        result[clip] = {'action': action.name, 'firstFrame': 1, 'lastFrame': length+1,
                        'fps': 24, 'loop': loop, 'rootMotion': False}
    rig.animation_data.action = None
    for bone in rig.pose.bones:
        bone.rotation_quaternion = Quaternion()
    scene.frame_start, scene.frame_end = 1, 145
    rig['stationary'] = True
    return result
