"""Bind the skeleton spirit's rigid bones and flexible cloth to a game-size FK rig."""
import math

import bpy
from mathutils import Quaternion, Vector

RIG = 'MOB_SKELETON_RIG'
PREFIX = 'MOB_SKELETON_'
SIDES = (('R', -1), ('L', 1))


def build_rig(collection, parts):
    joints = bpy.data.armatures.new(RIG)
    rig = bpy.data.objects.new(RIG, joints)
    collection.objects.link(rig)
    rig.show_in_front = True
    joints.display_type = 'STICK'
    bpy.context.view_layer.objects.active = rig
    rig.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')

    def add(name, head, tail, parent=None, deform=True):
        bone = joints.edit_bones.new(name)
        bone.head, bone.tail = head, tail
        bone.use_deform = deform
        if parent:
            bone.parent = joints.edit_bones[parent]
            bone.use_connect = (bone.head - bone.parent.tail).length < .00001

    add('root', (0, 0, 0), (0, 0, .28), deform=False)
    add('hips', (0, .065, 1.42), (0, .11, 1.66), 'root')
    add('spine', (0, .11, 1.66), (0, .172, 2.04), 'hips')
    add('chest', (0, .172, 2.04), (0, .138, 2.47), 'spine')
    add('neck', (0, .138, 2.47), (0, -.086, 2.777), 'chest')
    add('head', (0, -.086, 2.777), (0, -.15, 3.19), 'neck')
    add('jaw', (0, -.202, 2.785), (0, -.406, 2.579), 'head')
    add('cloth.front', (0, -.125, 1.49), (0, -.165, 1.12), 'hips')
    add('cloth.back', (0, .190, 1.49), (0, .222, 1.12), 'hips')
    add('talisman', (.077, -.17, 1.47), (.082, -.229, 1.21), 'cloth.front')
    for side, sign in SIDES:
        def pt(x, y, z):
            return (sign * x, y, z)

        asym = .021 * sign
        shoulder = pt(.358, .005, 2.509)
        elbow = pt(.488, -.037 + asym, 1.881)
        wrist = pt(.529, -.126 - asym, 1.253 + asym)
        hip = pt(.183, .034, 1.43)
        knee = pt(.212, -.110 + asym, .789)
        ankle = pt(.251, .086 + asym, .165)
        toe = pt(.245, -.145, .055)
        add(f'clavicle.{side}', pt(.035, -.095, 2.49), shoulder, 'chest')
        add(f'upper_arm.{side}', shoulder, elbow, f'clavicle.{side}')
        add(f'forearm.{side}', elbow, wrist, f'upper_arm.{side}')
        add(f'hand.{side}', wrist, pt(.54, wrist[1] - .065, wrist[2] - .27), f'forearm.{side}')
        add(f'thigh.{side}', hip, knee, 'hips')
        add(f'shin.{side}', knee, ankle, f'thigh.{side}')
        add(f'foot.{side}', ankle, toe, f'shin.{side}')
        add(f'toe.{side}', toe, pt(.232, -.29, .022), f'foot.{side}')
    bpy.ops.object.mode_set(mode='OBJECT')

    bone_sets = {
        '몸통': ('hips', 'spine', 'chest', 'neck', 'head', 'jaw'),
        '팔': tuple(f'{name}.{side}' for side, _ in SIDES
                   for name in ('clavicle', 'upper_arm', 'forearm', 'hand')),
        '다리': tuple(f'{name}.{side}' for side, _ in SIDES
                    for name in ('thigh', 'shin', 'foot', 'toe')),
        '의상': ('cloth.front', 'cloth.back', 'talisman'),
        '이동': ('root',),
    }
    for (label, names), palette in zip(bone_sets.items(),
                                       ('THEME04', 'THEME03', 'THEME09', 'THEME06', 'THEME02')):
        group = joints.collections.new(label)
        for name in names:
            group.assign(joints.bones[name])
            joints.bones[name].color.palette = palette
    for bone in rig.pose.bones:
        bone.rotation_mode = 'QUATERNION'

    rigid = {'SKULL': 'head', 'SOCKET_LIGHTS': 'head', 'JAW': 'jaw', 'RIBS': 'chest',
             'WAIST_TALISMAN': 'talisman', 'PELVIS': 'hips'}
    for side, _ in SIDES:
        for suffix, bone in (('SHOULDER', 'clavicle'), ('UPPER_ARM', 'upper_arm'),
                             ('FOREARM', 'forearm'), ('HAND', 'hand'),
                             ('THIGH', 'thigh'), ('SHIN', 'shin')):
            rigid[f'{suffix}_{side}'] = f'{bone}.{side}'

    for part in parts:
        obj = collection.objects[part['name']]
        role = obj.name[len(PREFIX):]
        groups = {}

        def assign(index, weights):
            for name, weight in weights.items():
                if weight <= 0:
                    continue
                if name not in groups:
                    groups[name] = obj.vertex_groups.new(name=name)
                groups[name].add([index], weight, 'REPLACE')

        if role in rigid:
            assign_group = obj.vertex_groups.new(name=rigid[role])
            assign_group.add(list(range(len(obj.data.vertices))), 1., 'REPLACE')
        else:
            for vertex in obj.data.vertices:
                x, y, z = vertex.co
                if role == 'SPINE_AND_NECK':
                    weights = spine_weights(z)
                elif role == 'WAIST_CLOTH':
                    loose = smooth((1.49 - z) / .17)
                    front = smooth((.105 - y) / .14)
                    weights = {'hips': 1 - loose, 'cloth.front': loose * front,
                               'cloth.back': loose * (1 - front)}
                elif role.startswith('FOOT_'):
                    side = role[-1]
                    toe = smooth((-.105 - y) / .09)
                    weights = {f'foot.{side}': 1 - toe, f'toe.{side}': toe}
                else:
                    raise ValueError(f'Undefined skin weights: {role}')
                assign(vertex.index, weights)
        obj.parent = rig
        modifier = obj.modifiers.new('해골형 주령 스킨', 'ARMATURE')
        modifier.object = rig
        part['skinJoints'] = [group.name for group in obj.vertex_groups]
    return rig


def smooth(value):
    t = max(0., min(1., value))
    return t * t * (3 - 2 * t)


def spine_weights(z):
    for edge, width, below, above in ((1.66, .13, 'hips', 'spine'),
                                     (2.04, .16, 'spine', 'chest'),
                                     (2.47, .14, 'chest', 'neck')):
        if z < edge + width / 2:
            upper = smooth((z - edge) / width + .5)
            return {below: 1 - upper, above: upper}
    return {'neck': 1.}


def rotate(rig, name, axis, degrees):
    bone = rig.pose.bones[name]
    local_axis = bone.bone.matrix_local.to_quaternion().inverted() @ Vector(axis)
    bone.rotation_quaternion = Quaternion(local_axis, math.radians(degrees))


def set_pose(rig, name):
    for bone in rig.pose.bones:
        bone.matrix_basis.identity()
    if name == 'rest':
        return
    if name == 'reach':
        rotate(rig, 'chest', (0, 0, 1), -15)
        rotate(rig, 'neck', (1, 0, 0), 9)
        rotate(rig, 'head', (0, 0, 1), 10)
        rotate(rig, 'jaw', (1, 0, 0), 24)
        rotate(rig, 'upper_arm.L', (1, 0, 0), -49)
        rotate(rig, 'forearm.L', (1, 0, 0), -22)
        rotate(rig, 'hand.L', (1, 0, 0), 12)
        rotate(rig, 'upper_arm.R', (0, 1, 0), 19)
        rotate(rig, 'forearm.R', (1, 0, 0), -24)
        rotate(rig, 'cloth.front', (1, 0, 0), -8)
        rotate(rig, 'talisman', (0, 1, 0), 11)
    elif name == 'crouch':
        rotate(rig, 'spine', (1, 0, 0), 12)
        rotate(rig, 'chest', (1, 0, 0), 10)
        rotate(rig, 'head', (1, 0, 0), -14)
        rotate(rig, 'jaw', (1, 0, 0), 11)
        for side, sign in SIDES:
            rotate(rig, f'upper_arm.{side}', (0, 1, 0), -sign * 12)
            rotate(rig, f'forearm.{side}', (1, 0, 0), -20)
            rotate(rig, f'thigh.{side}', (1, 0, 0), -27)
            rotate(rig, f'shin.{side}', (1, 0, 0), 53)
            rotate(rig, f'foot.{side}', (1, 0, 0), -26)
        rotate(rig, 'cloth.front', (1, 0, 0), -34)
        rotate(rig, 'cloth.back', (1, 0, 0), 16)
    else:
        raise ValueError(name)
    bpy.context.view_layer.update()
    # Ground the foot meshes after posing. The render keeps the author's +Z-up axes.
    graph = bpy.context.evaluated_depsgraph_get()
    feet = [bpy.data.objects[PREFIX + 'FOOT_' + side].evaluated_get(graph) for side, _ in SIDES]
    floor = min((obj.matrix_world @ v.co).z for obj in feet for v in obj.data.vertices)
    root = rig.pose.bones['root']
    root.location = root.bone.matrix_local.to_3x3().inverted() @ Vector((0, 0, -floor))
    bpy.context.view_layer.update()


def save_review_poses(rig):
    scene = bpy.context.scene
    for frame, name in ((1, 'rest'), (16, 'reach'), (32, 'crouch')):
        set_pose(rig, name)
        scene.timeline_markers.new(name, frame=frame)
        for bone in rig.pose.bones:
            bone.keyframe_insert('rotation_quaternion', frame=frame, group=bone.name)
            bone.keyframe_insert('location', frame=frame, group=bone.name)
    action = rig.animation_data.action
    action.name = 'MOB_SKELETON_RIG_REVIEW'
    action.use_fake_user = True
    rig.animation_data.action = None
    set_pose(rig, 'rest')
    scene.frame_start, scene.frame_end = 1, 32
    scene.frame_set(1)
