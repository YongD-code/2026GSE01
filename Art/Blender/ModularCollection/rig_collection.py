"""Bind every interchangeable NPC mesh to one shared rig and author keyed clips."""

import json
import math
import sys
from pathlib import Path

import bpy


OUT = Path(__file__).resolve().parent
sys.path.insert(0, str(OUT))
import surface_forms
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.open_mainfile(filepath=str(OUT / "npc_parts.blend"))
scene = bpy.context.scene


def add_bone(arm, name, head, tail, parent=None):
    bone = arm.edit_bones.new(name)
    bone.head, bone.tail = head, tail
    if parent:
        bone.parent = arm.edit_bones[parent]


arm = bpy.data.armatures.new("NPC_SHARED_SKELETON")
rig = bpy.data.objects.new("NPC_SHARED_RIG", arm)
scene.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig
rig.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
add_bone(arm, 'root', (0, 0, 0), (0, 0, .25))
add_bone(arm, 'hips', (0, 0, .94), (0, 0, 1.15), 'root')
add_bone(arm, 'spine', (0, 0, 1.15), (0, 0, 1.62), 'hips')
add_bone(arm, 'head', (0, 0, 1.70), (0, 0, 2.35), 'spine')
for side, x in (('L', -.43), ('R', .43)):
    add_bone(arm, f'arm.{side}', (x*.65, 0, 1.54), (x, 0, 1.13), 'spine')
    add_bone(arm, f'hand.{side}', (x, 0, 1.13), (x, 0, .94), f'arm.{side}')
    add_bone(arm, f'thigh.{side}', (x*.35, 0, .93), (x*.35, 0, .55), 'hips')
    add_bone(arm, f'shin.{side}', (x*.35, 0, .55), (x*.35, 0, .10), f'thigh.{side}')
bpy.ops.object.mode_set(mode='OBJECT')
rig.show_in_front = True
for bone in rig.pose.bones:
    bone.rotation_mode = 'XYZ'


def influences(obj, co):
    x, y, z = co
    name = obj.name
    if 'HAIR_' in name or 'RIBBON_' in name:
        return {'head': 1.0}
    if 'ACCESSORY_' in name:
        if 'BOW_' in name:
            return {'spine': 1.0} if x < .1 else {'hand.R': 1.0}
        if any(t in name for t in ('HAMMER_', 'STAFF_')):
            return {'hand.R': 1.0}
        if 'LEDGER_' in name:
            return {'hand.L': 1.0}
        if 'SCARF_' in name or 'SEAL_' in name:
            return {'spine': 1.0}
        if 'SATCHEL_' in name and z > 1.15:
            return {'spine': 1.0}
        return {'hips': 1.0}
    if z > 1.73:
        return {'head': 1.0}
    if 1.61 < z <= 1.73 and abs(x) < .19:
        w = (z-1.61)/.12
        return {'head': w, 'spine': 1-w}
    if abs(x) > .32 and .91 < z < 1.63:
        side = 'L' if x < 0 else 'R'
        w = max(0, min(1, (1.18-z)/.14))
        return {f'hand.{side}': w, f'arm.{side}': 1-w}
    if z < .93 and abs(x) > .02:
        side = 'L' if x < 0 else 'R'
        w = max(0, min(1, (.64-z)/.18))
        return {f'shin.{side}': w, f'thigh.{side}': 1-w}
    if z < 1.12:
        return {'hips': 1.0}
    return {'spine': 1.0}


meshes = [obj for obj in scene.objects if obj.type == 'MESH']
for obj in meshes:
    groups = {name: obj.vertex_groups.new(name=name) for name in arm.bones.keys()}
    for vertex in obj.data.vertices:
        for name, weight in influences(obj, vertex.co).items():
            groups[name].add([vertex.index], weight, 'REPLACE')
    modifier = obj.modifiers.new('NPC_SKIN', 'ARMATURE')
    modifier.object = rig
    obj.parent = rig
    obj['sharedRig'] = rig.name


def reset_pose():
    for bone in rig.pose.bones:
        bone.location = (0, 0, 0)
        bone.rotation_euler = (0, 0, 0)
        bone.scale = (1, 1, 1)


def pose(kind, t):
    reset_pose()
    root = rig.pose.bones['root']
    hips = rig.pose.bones['hips']
    spine = rig.pose.bones['spine']
    head = rig.pose.bones['head']
    if kind == 'idle':
        spine.rotation_euler.x = .018 * math.sin(2*math.pi*t)
        head.rotation_euler.x = -.012 * math.sin(2*math.pi*t)
        root.location.y = .006 * math.sin(2*math.pi*t)
    elif kind in ('walk', 'run'):
        run = kind == 'run'
        phase = 2*math.pi*t
        root.location.y = (.06 if run else .02) * (1-math.cos(2*phase)) / 2
        spine.rotation_euler.x = (.09 if run else .035) * math.sin(phase)
        hips.rotation_euler.y = (.045 if run else .025) * math.sin(phase)
        for side, offset in (('L', 0), ('R', math.pi)):
            a = phase + offset
            leg = rig.pose.bones[f'thigh.{side}']
            shin = rig.pose.bones[f'shin.{side}']
            arm_bone = rig.pose.bones[f'arm.{side}']
            hand = rig.pose.bones[f'hand.{side}']
            leg.rotation_euler.x = (.49 if run else .30) * math.cos(a)
            shin.rotation_euler.x = (.55 if run else .30) * max(0, math.sin(a))
            arm_bone.rotation_euler.x = -(.43 if run else .25) * math.cos(a)
            hand.rotation_euler.x = .10 * math.sin(a)
        head.rotation_euler.x = -.025 * math.sin(phase)
    elif kind == 'jump':
        lift = math.sin(math.pi*t)**2
        root.location.y = .64 * lift
        hips.rotation_euler.x = .12 * math.sin(2*math.pi*t)
        spine.rotation_euler.x = -.10 * lift
        for side in ('L', 'R'):
            rig.pose.bones[f'thigh.{side}'].rotation_euler.x = .45*lift
            rig.pose.bones[f'shin.{side}'].rotation_euler.x = .58*lift
            rig.pose.bones[f'arm.{side}'].rotation_euler.x = -.35*lift
    elif kind in ('turn_left', 'turn_right'):
        direction = 1 if kind == 'turn_left' else -1
        s = t*t*(3-2*t)
        root.rotation_euler.y = direction * math.pi/4 * s
        head.rotation_euler.y = direction * .14 * math.sin(math.pi*t)
        spine.rotation_euler.y = -direction * .06 * math.sin(math.pi*t)


clips = {'idle': 32, 'walk': 24, 'run': 16, 'jump': 24,
         'turn_left': 12, 'turn_right': 12}
foot_mesh = next(o for o in meshes if 'OUTFIT_TUNIC_' in o.name)
foot_indices = [v.index for v in foot_mesh.data.vertices if v.co.z < .45]


def keep_boots_above_floor():
    # Check actual toe geometry, not only the ankle bone. Long boot toes sweep lower
    # than the bone endpoint during a stride.
    bpy.context.view_layer.update()
    evaluated = foot_mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
    lowest = min((evaluated.matrix_world @ evaluated.data.vertices[i].co).z for i in foot_indices)
    if lowest < .012:
        rig.pose.bones['root'].location.y += .012-lowest


for kind, end in clips.items():
    rig.animation_data_create()
    rig.animation_data.action = None
    for frame in range(1, end+2):
        t = (frame-1)/end
        pose(kind, t)
        keep_boots_above_floor()
        for bone in rig.pose.bones:
            bone.keyframe_insert('location', frame=frame, group=bone.name)
            bone.keyframe_insert('rotation_euler', frame=frame, group=bone.name)
    action = rig.animation_data.action
    action.name = 'NPC_' + kind.upper()
    action.use_fake_user = True
    action['clip'] = kind
    action['loop'] = kind in ('idle', 'walk', 'run')
    action['fps'] = 24
rig.animation_data.action = None
reset_pose()

bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'npc_rigged.blend'))
bpy.ops.object.select_all(action='SELECT')
with surface_forms.export_materials():
    bpy.ops.export_scene.gltf(filepath=str(OUT / 'npc_rigged.glb'), export_format='GLB',
                              use_selection=True, export_animations=True,
                              export_animation_mode='ACTIONS', export_extras=True)
catalog_path = OUT / 'catalog.json'
catalog = json.loads(catalog_path.read_text(encoding='utf-8'))
catalog['riggedPartFile'] = 'npc_rigged.glb'
catalog['rigFile'] = 'npc_rigged.blend'
catalog['rigName'] = rig.name
catalog['clips'] = {name: {'firstFrame': 1, 'lastFrame': end+1,
                           'loop': name in ('idle', 'walk', 'run')}
                    for name, end in clips.items()}
catalog_path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print('NPC_RIG_READY', len(meshes), len(arm.bones), len(clips), flush=True)
