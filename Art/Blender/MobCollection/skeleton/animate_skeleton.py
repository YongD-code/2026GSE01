"""Bake the skeletal spirit's movement and combat poses at 24 FPS."""
import math

import bpy
from mathutils import Matrix, Quaternion, Vector

from rig_skeleton import PREFIX, SIDES, rotate, set_pose, smooth

FPS = 24
CLIPS = {
    'idle': (48, True),
    'walk': (32, True),
    'run': (20, True),
    'cast': (36, False),
    'jump': (26, False),
    'turn_left': (14, False),
    'turn_right': (14, False),
    'attack': (30, False),
    'q_shards': (24, False),
}
STRIDE = {'walk': 1.20, 'run': 1.92}


def curve(t, *keys):
    for (a, start), (b, end) in zip(keys, keys[1:]):
        if t <= b:
            return start + (end - start) * smooth((t - a) / (b - a))
    return keys[-1][1]


def transform(rig, name, shift):
    bone = rig.pose.bones[name]
    bone.location = bone.bone.matrix_local.to_quaternion().inverted() @ Vector(shift)


def bend(rig, name, x=0., y=0., z=0.):
    bone = rig.pose.bones[name]
    rest = bone.bone.matrix_local.to_quaternion()
    rotation = (Quaternion((0, 0, 1), math.radians(z)) @
                Quaternion((0, 1, 0), math.radians(y)) @
                Quaternion((1, 0, 0), math.radians(x)))
    bone.rotation_quaternion = rest.inverted() @ rotation @ rest


def leg(rig, side, offset):
    """Solve the ankle placement in armature space, then write ordinary FK transforms."""
    thigh = rig.pose.bones['thigh.' + side]
    shin = rig.pose.bones['shin.' + side]
    foot = rig.pose.bones['foot.' + side]
    hip = thigh.head.copy()
    ankle = foot.bone.head_local + Vector(offset)
    delta = ankle - hip
    axis = delta.normalized()
    a, b = thigh.bone.length, shin.bone.length
    reach = max(abs(a - b) + .001, min(a + b - .002, delta.length))
    end = hip + axis * reach
    along = (a * a - b * b + reach * reach) / (2 * reach)
    pole = Vector((0, -1, 0))
    pole = (pole - axis * pole.dot(axis)).normalized()
    knee = hip + axis * along + pole * math.sqrt(max(0., a * a - along * along))

    desired = []
    for bone, start, stop in ((thigh, hip, knee), (shin, knee, end)):
        rest = bone.bone
        rotation = (rest.tail_local - rest.head_local).rotation_difference(stop - start)
        desired.append(Matrix.Translation(start) @ rotation.to_matrix().to_4x4() @
                       rest.matrix_local.to_quaternion().to_matrix().to_4x4())
    desired.append(Matrix.Translation(end) @ foot.bone.matrix_local.to_quaternion().to_matrix().to_4x4())

    parent = thigh.parent.matrix.copy()
    for bone, matrix in zip((thigh, shin, foot), desired):
        rest_local = bone.parent.bone.matrix_local.inverted() @ bone.bone.matrix_local
        bone.matrix_basis = rest_local.inverted() @ parent.inverted() @ matrix
        parent = matrix
    return (end - ankle).length


def stride(phase, distance, duty, lift):
    if phase < duty:
        return distance * (phase / duty - .5), 0.
    step = (phase - duty) / (1 - duty)
    return distance * (.5 - smooth(step)), lift * math.sin(math.pi * step)**2


def shard_arm(rig, t):
    """Bake a chest gather and palm-first release, with the elbow outside the rib cage."""
    upper = rig.pose.bones['upper_arm.L']
    forearm = rig.pose.bones['forearm.L']
    hand = rig.pose.bones['hand.L']
    rest = hand.bone.head_local.copy()
    gather = Vector((.08, -.39, 2.15))
    release = Vector((.19, -1.02, 2.20))
    if t <= .30:
        target = rest.lerp(gather, smooth(t / .30))
    elif t <= .40:
        target = gather.lerp(release, smooth((t - .30) / .10))
    else:
        target = release.lerp(rest, smooth((t - .49) / .51))
    hip = upper.head.copy()
    delta = target - hip
    axis = delta.normalized()
    a, b = upper.bone.length, forearm.bone.length
    reach = max(abs(a - b) + .001, min(a + b - .002, delta.length))
    along = (a*a - b*b + reach*reach) / (2*reach)
    pole = Vector((1, -.25, -.35))
    pole = (pole - axis * pole.dot(axis)).normalized()
    elbow = hip + axis * along + pole * math.sqrt(max(0., a*a - along*along))
    end = hip + axis * reach
    desired = []
    for bone, start, stop in ((upper, hip, elbow), (forearm, elbow, end)):
        source = bone.bone
        rotation = (source.tail_local - source.head_local).rotation_difference(stop - start)
        desired.append(Matrix.Translation(start) @ rotation.to_matrix().to_4x4() @
                       source.matrix_local.to_quaternion().to_matrix().to_4x4())
    amount = curve(t, (0, 0), (.30, .65), (.40, 1), (.54, 1), (1, 0))
    rotation = Quaternion((1, 0, 0), math.radians(-85 * amount))
    desired.append(Matrix.Translation(end) @ rotation.to_matrix().to_4x4() @
                   hand.bone.matrix_local.to_quaternion().to_matrix().to_4x4())
    parent = upper.parent.matrix.copy()
    for bone, matrix in zip((upper, forearm, hand), desired):
        rest_local = bone.parent.bone.matrix_local.inverted() @ bone.bone.matrix_local
        bone.matrix_basis = rest_local.inverted() @ parent.inverted() @ matrix
        parent = matrix
    blend = curve(t, (0,0), (.18,1), (.75,1), (1,0))
    for bone in (upper, forearm, hand):
        bone.rotation_quaternion = Quaternion((1, 0, 0, 0)).slerp(bone.rotation_quaternion, blend)
        bone.location *= blend


def pose(rig, clip, t, foot_correction):
    set_pose(rig, 'rest')
    angle = math.tau * t
    feet = {side: (0., 0., 0.) for side, _ in SIDES}
    hip_y, hip_z = 0., 0.
    spine, chest, twist, neck, jaw = 0., 0., 0., 0., 3.
    clothes, charm = 0., 0.
    arms = {side: (0., 0., 0., 0.) for side, _ in SIDES}

    if clip in STRIDE:
        running = clip == 'run'
        duty = .43 if running else .64
        distance = STRIDE[clip] * duty
        flight = []
        for side, sign in SIDES:
            phase = (t + (0. if sign == 1 else .5)) % 1.
            travel, lift = stride(phase, distance, duty, .25 if running else .12)
            feet[side] = (0., travel, lift)
            flight.append(lift)
            swing = math.cos(math.tau * phase)
            arms[side] = ((34 if running else 15) * swing - (13 if running else 3),
                          -sign * 4, 0., -11 - (27 if running else 7) * (swing + 1) / 2)
        hip_z = (-.14 if running else -.085) + min(flight) * .75 - .02 * math.cos(2 * angle)
        spine = 12 if running else 3
        chest = 5 if running else 0
        neck = -spine * .60
        twist = (11 if running else 7) * math.sin(angle)
        clothes = (15 if running else 7) * math.sin(angle - .42)
        charm = 10 * math.sin(2 * angle - .7)
        jaw = 7 + 3 * math.sin(2 * angle) if running else 3 + 2 * math.sin(angle)
    elif clip == 'idle':
        hip_z = -.040 + .014 * math.sin(angle)
        spine = 2 + 2.5 * math.sin(angle)
        twist = 3 * math.sin(angle - .3)
        neck = -1.7 * math.sin(angle)
        jaw = 4 + 3 * math.sin(angle - .7)
        clothes = 3 * math.sin(angle - .5)
        charm = 4 * math.sin(angle - .8)
    elif clip == 'attack':
        hip_z = curve(t, (0,-.04), (.33,-.17), (.49,-.105), (.72,-.16), (1,-.04))
        hip_y = curve(t, (0,0), (.33,.10), (.50,-.14), (.69,-.07), (1,0))
        twist = curve(t, (0,0), (.35,25), (.48,-30), (.65,-38), (1,0))
        chest = curve(t, (0,0), (.34,-14), (.49,21), (.69,11), (1,0))
        spine, neck = chest * .48, -chest * .38
        jaw = curve(t, (0,3), (.30,10), (.48,29), (.73,13), (1,3))
        arms['L'] = (curve(t, (0,0), (.34,-62), (.49,-86), (.67,-68), (1,0)),
                     curve(t, (0,0), (.34,-48), (.49,-12), (.67,1), (1,0)),
                     curve(t, (0,0), (.34,53), (.49,-28), (.67,-37), (1,0)),
                     curve(t, (0,0), (.35,-50), (.49,-7), (.69,-32), (1,0)))
        arms['R'] = (curve(t, (0,0), (.34,-17), (.56,23), (1,0)),
                     0., 0., curve(t, (0,0), (.45,-19), (1,0)))
        clothes = -chest * .7
        charm = curve(t, (0,0), (.36,6), (.54,-20), (.72,8), (1,0))
    elif clip == 'q_shards':
        gather = curve(t, (0,0), (.28,1), (.40,.15), (.55,.35), (1,0))
        hip_z = -.04 - .10 * gather
        spine = 11 * gather
        chest = 7 * gather
        twist = curve(t, (0,0), (.30,12), (.40,-8), (.57,-5), (1,0))
        neck, jaw = -spine * .5, 3 + 18 * gather
        arms['R'] = (-14 * gather, 8 * gather, -8 * gather, -22 * gather)
        clothes, charm = -10 * gather, 12 * gather
    elif clip == 'cast':
        strength = curve(t, (0,0), (.33,1), (.55,.82), (.69,1), (.82,.55), (1,0))
        hip_z = -.04 - .09 * strength
        spine, chest, neck, jaw = -5 * strength, -8 * strength, 12 * strength, 3 + 24 * strength
        for side, sign in SIDES:
            arms[side] = (-61 * strength, -sign * 19 * strength,
                          sign * 18 * strength, -37 * strength)
        clothes, charm = -13 * strength, 11 * strength
    elif clip == 'jump':
        air = max(0., min(1., (t - .23) / .53))
        arc = 4 * air * (1 - air)
        tuck = math.sin(math.pi * air)**2
        hip_z = curve(t, (0,-.04), (.18,-.24), (.24,-.10), (.75,-.10),
                      (.83,-.25), (1,-.04)) + .76 * arc
        spine = curve(t, (0,0), (.18,17), (.34,-11), (.60,6), (.83,19), (1,0))
        neck = -spine * .6
        for side, sign in SIDES:
            feet[side] = (0., sign * .10 * tuck, .76 * arc + .16 * tuck)
            arms[side] = (-54 * arc, -sign * 15 * arc, 0., -24 * tuck)
        clothes, charm = -26 * arc, 18 * math.sin(air * math.tau) * arc
    elif clip.startswith('turn_'):
        direction = -1 if clip == 'turn_right' else 1
        for side, sign in SIDES:
            begin = .04 if sign == direction else .51
            step = max(0., min(1., (t - begin) / .43))
            lift = math.sin(math.pi * step)**2
            feet[side] = (sign * .11 * lift, sign * direction * -.10 * lift, .12 * lift)
        envelope = math.sin(math.pi * t)**2
        hip_z = -.04 - .033 * envelope
        twist, neck = direction * 16 * envelope, -3 * envelope
        clothes, charm = direction * 6 * envelope, -direction * 10 * envelope
        for side, sign in SIDES:
            arms[side] = (sign * 10 * envelope, 0., direction * 5 * envelope, -6 * envelope)
    else:
        raise ValueError(clip)

    transform(rig, 'hips', (0, hip_y, hip_z))
    bend(rig, 'spine', spine, z=twist * .38)
    bend(rig, 'chest', chest, z=twist * .62)
    bend(rig, 'neck', neck)
    bend(rig, 'head', x=-neck * .35, z=twist * .30)
    rotate(rig, 'jaw', (1, 0, 0), jaw)
    bend(rig, 'cloth.front', clothes)
    bend(rig, 'cloth.back', -clothes * .55)
    bend(rig, 'talisman', charm, z=clothes * .45)
    for side, (raise_arm, spread, sweep, elbow) in arms.items():
        bend(rig, 'upper_arm.' + side, raise_arm, spread, sweep)
        bend(rig, 'forearm.' + side, elbow)
    bpy.context.view_layer.update()
    if clip == 'q_shards':
        shard_arm(rig, t)
        bpy.context.view_layer.update()
    errors = []
    for side, _ in SIDES:
        x, y, z = feet[side]
        errors.append(leg(rig, side, (x, y, z + foot_correction[side])))
    error = max(errors)
    if error > .004:
        raise ValueError(f'{clip} at {t:.3f}: ankle target missed by {error:.4f}')


def bake(rig):
    scene = bpy.context.scene
    scene.render.fps = FPS
    scene.render.fps_base = 1.
    # The source feet are slightly asymmetric. Match their ground plane in both stances.
    sole_heights = {
        side: min(v.co.z for v in bpy.data.objects[PREFIX+'FOOT_'+side].data.vertices)
        for side, _ in SIDES
    }
    floor = min(sole_heights.values())
    foot_correction = {side: floor - z for side, z in sole_heights.items()}
    metadata = {}
    for clip, (frames, loop) in CLIPS.items():
        rig.animation_data.action = None
        previous = {}
        for i in range(frames + 1):
            pose(rig, clip, i / frames, foot_correction)
            for bone in rig.pose.bones:
                rotation = bone.rotation_quaternion.copy()
                if bone.name in previous and rotation.dot(previous[bone.name]) < 0:
                    rotation.negate()
                    bone.rotation_quaternion = rotation
                previous[bone.name] = rotation
                bone.keyframe_insert('location', frame=i+1, group=bone.name)
                bone.keyframe_insert('rotation_quaternion', frame=i+1, group=bone.name)
        action = rig.animation_data.action
        action.name = PREFIX + clip.upper()
        action.use_fake_user = True
        action['loop'] = loop
        action['fps'] = FPS
        if action.layers:
            for strip in action.layers[0].strips:
                for bag in strip.channelbags:
                    for channel in bag.fcurves:
                        for key in channel.keyframe_points:
                            key.interpolation = 'LINEAR'
        metadata[clip] = {'action': action.name, 'firstFrame': 1, 'lastFrame': frames+1,
                          'seconds': frames / FPS, 'loop': loop, 'rootMotion': False}
        if clip in STRIDE:
            metadata[clip]['strideToHeight'] = round(STRIDE[clip] / 3.31, 4)
    rig.animation_data.action = None
    set_pose(rig, 'rest')
    scene.frame_start, scene.frame_end = 1, CLIPS['idle'][0] + 1
    scene.frame_set(1)
    return metadata
