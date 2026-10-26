"""Bake contact-based animal locomotion into the existing quadruped models.

Blender --background --factory-startup --python-exit-code 1 --python this_file
        -- --species all
Preserves meshes, materials and non-locomotion actions. No game build or render.
"""
import argparse
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
PROFILE = {
    'hands': dict(walk=.78, run=1.20, duty=.72, fast_duty=.52, drop=.11,
                  fast_drop=.14, lift=.10, fast_lift=.17, sway=.012, fast='trot', threshold=3.0),
    'stag': dict(walk=1.00, run=1.50, duty=.70, fast_duty=.50, drop=.12,
                 fast_drop=.16, lift=.12, fast_lift=.20, sway=.008, fast='trot', threshold=2.1),
    'hound': dict(walk=1.05, run=2.30, duty=.69, fast_duty=.30, drop=.12,
                  fast_drop=.16, lift=.11, fast_lift=.23, sway=.014, fast='gallop', threshold=2.6),
}
LEGS = ('front.L', 'front.R', 'back.L', 'back.R')


def smooth(t):
    t = max(0., min(1., t))
    return t*t*(3-2*t)


def about(point, rotation):
    return Matrix.Translation(point) @ rotation @ Matrix.Translation(-point)


def add_structure(rig):
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode='EDIT')
    bones = rig.data.edit_bones
    for name, y in (('pelvis', .35), ('chest', -.34)):
        if name not in bones:
            b = bones.new(name)
            b.head, b.tail = (0, y, 1.08), (0, y-.20, 1.08)
            b.parent = bones['body']
    for leg in LEGS:
        bones[leg].parent = bones['chest' if leg.startswith('front') else 'pelvis']
        name = 'toe.'+leg
        if name not in bones:
            b = bones.new(name)
            b.head = bones['paw.'+leg].tail
            b.tail = b.head + Vector((0, -.20, 0))
            b.parent = bones['paw.'+leg]
    bones['head'].parent = bones['chest']
    for name in ('tail', 'tail.0', 'tail.1', 'tail.2'):
        if name in bones:
            bones[name].parent = bones['pelvis']
    bpy.ops.object.mode_set(mode='OBJECT')
    if len(rig.data.bones) > 32:
        raise ValueError('Fallback renderer supports at most 32 bones')


def bind_weights(rig):
    """Redistribute existing rigid weights without changing any vertex or face."""
    known = {'body', *LEGS, *('paw.'+leg for leg in LEGS)}
    inv = rig.matrix_world.inverted()
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH':
            continue
        original = obj.get('locomotion_bind')
        if original is None:
            names = {g.name for g in obj.vertex_groups}
            if len(names) != 1 or not names.issubset(known):
                continue
            original = next(iter(names))
            obj['locomotion_bind'] = original
        if original not in known:
            continue
        affected = {original, 'body', 'pelvis', 'chest'}
        if original.startswith('paw.'):
            affected.add('toe.'+original[4:])
        elif original in LEGS:
            affected.add('paw.'+original)
        for name in affected:
            group = obj.vertex_groups.get(name)
            if group:
                obj.vertex_groups.remove(group)
        groups = {}

        def put(index, name, weight):
            if weight <= .00001:
                return
            if name not in groups:
                groups[name] = obj.vertex_groups.get(name) or obj.vertex_groups.new(name=name)
            groups[name].add([index], weight, 'REPLACE')

        transform = inv @ obj.matrix_world
        for vertex in obj.data.vertices:
            p = transform @ vertex.co
            if original == 'body':
                chest = .80*smooth((-p.y-.02)/.65)
                pelvis = .80*smooth((p.y-.02)/.65)
                put(vertex.index, 'chest', chest)
                put(vertex.index, 'pelvis', pelvis)
                put(vertex.index, 'body', 1-chest-pelvis)
            elif original.startswith('paw.'):
                ankle = rig.data.bones[original].tail_local.z
                foot = smooth((ankle+.17-p.z)/.18)
                put(vertex.index, 'toe.'+original[4:], foot)
                put(vertex.index, original, 1-foot)
            else:
                hip = rig.data.bones[original].head_local.z
                knee = rig.data.bones[original].tail_local.z
                shoulder = smooth((p.z-(hip-.14))/.20)*.75
                lower = smooth((knee+.075-p.z)/.15)*.30
                put(vertex.index, 'chest' if original.startswith('front') else 'pelvis', shoulder)
                put(vertex.index, 'paw.'+original, lower*(1-shoulder))
                put(vertex.index, original, (1-shoulder)*(1-lower))


class Baker:
    def __init__(self, rig, key):
        self.rig, self.key = rig, key
        self.profile = PROFILE[key]
        self.rest = {b.name:b.matrix_local.copy() for b in rig.data.bones}
        self.bones = []

        def visit(b):
            self.bones.append(b)
            for child in b.children:
                visit(child)

        for bone in rig.data.bones:
            if bone.parent is None:
                visit(bone)
        self.previous = {}

    def rotation(self, name, delta, axis, desired):
        bone = self.rig.data.bones[name]
        parent = bone.parent
        inherited = (desired[parent.name] @ self.rest[parent.name].inverted()
                     if parent else Matrix.Identity(4))
        desired[name] = inherited @ about(bone.head_local, Matrix.Rotation(delta, 4, axis)) @ self.rest[name]

    def leg(self, name, phase, duty, distance, lift, desired, turn=0):
        upper = self.rig.data.bones[name]
        lower = self.rig.data.bones['paw.'+name]
        hip = desired[upper.parent.name] @ self.rest[upper.parent.name].inverted() @ upper.head_local
        front = name.startswith('front')
        side = -1 if name.endswith('L') else 1
        u = phase % 1.
        if u < duty:
            y = distance*(u/duty-.5)
            height, roll = 0., 0.
        else:
            swing = (u-duty)/(1-duty)
            y = distance*(.5-smooth(swing))
            height = lift*math.sin(math.pi*swing)**2
            roll = -.24*math.sin(math.pi*swing)**2
        target = lower.tail_local + Vector((turn*side*.035, y, height))
        delta = target-hip
        l1, l2 = upper.length, lower.length
        reach = delta.length
        axis = delta.normalized()
        # A slight knee bend is retained even at the furthest reach.
        reach = max(abs(l1-l2)+.001, min(l1+l2-.002, reach))
        end = hip+axis*reach
        along = (l1*l1-l2*l2+reach*reach)/(2*reach)
        pole = Vector((0, 1 if front else -1, 0))
        pole -= axis*pole.dot(axis)
        pole.normalize()
        knee = hip+axis*along+pole*math.sqrt(max(0., l1*l1-along*along))
        for bone, start, finish in ((upper, hip, knee), (lower, knee, end)):
            rotation = (bone.tail_local-bone.head_local).rotation_difference(finish-start).to_matrix().to_4x4()
            desired[bone.name] = Matrix.Translation(start) @ rotation @ self.rest[bone.name].to_3x3().to_4x4()
        toe = 'toe.'+name
        desired[toe] = (Matrix.Translation(end) @ Matrix.Rotation(roll, 4, 'X') @
                        self.rest[toe].to_3x3().to_4x4())

    def pose(self, kind, t):
        p = self.profile
        a = math.tau*t
        fast = kind == 'run'
        moving = kind in ('walk', 'run')
        turning = kind.startswith('turn_')
        direction = (1 if kind == 'turn_left' else -1) if turning else 0
        turn = direction*math.sin(math.pi*t)**2
        gallop = fast and p['fast'] == 'gallop'
        drop = p['fast_drop'] if fast else p['drop']
        bob = (.035 if fast else .009)*(1-math.cos(2*a))*.5 if moving else .004*math.sin(a)
        sway = p['sway']*math.sin(a) if moving else -.018*turn
        root = Matrix.Translation((sway, 0, -drop+bob))
        desired = {'root':root @ self.rest['root']}
        body_pitch = (.065 if gallop else .018)*math.sin(a) if moving else -.025*turn
        body = self.rig.data.bones['body']
        desired['body'] = root @ about(body.head_local, Matrix.Rotation(body_pitch, 4, 'X')) @ self.rest['body']
        self.rotation('pelvis', -.085*math.sin(a) if gallop else .012*math.sin(a), 'X', desired)
        self.rotation('chest', .070*math.sin(a) if gallop else -.012*math.sin(a), 'X', desired)
        self.rotation('head', -body_pitch*.65, 'X', desired)
        if turning:
            head = self.rig.data.bones['head']
            desired['head'] = about(head.head_local, Matrix.Rotation(turn*.10, 4, 'Z')) @ desired['head']
        if gallop:
            contacts = {'back.L':.04, 'back.R':.16, 'front.L':.52, 'front.R':.64}
        elif fast:
            contacts = {'front.L':0., 'back.R':0., 'front.R':.5, 'back.L':.5}
        else:
            contacts = {'back.L':0., 'front.L':.25, 'back.R':.5, 'front.R':.75}
        for name in LEGS:
            phase = t-contacts[name] if moving else 0.
            duty = p['fast_duty'] if fast else p['duty']
            stride = p['run' if fast else 'walk']*duty if moving else 0.
            lift = p['fast_lift' if fast else 'lift'] if moving else 0.
            # Turning steps unload one paw at a time, without baking a second root yaw.
            if turning:
                index = LEGS.index(name)
                step = max(0., 1-abs(t-(.18+index*.20))/.15)
                phase, duty, stride, lift = .72+step*.14, .70, .045*turn, .065*step
            self.leg(name, phase, duty, stride, lift, desired, turn)
        for i, name in enumerate(('tail', 'tail.0', 'tail.1', 'tail.2', 'tip.0', 'tip.1', 'tip.2')):
            if name in self.rest:
                bone = self.rig.data.bones[name]
                if bone.parent.name not in desired:
                    parent = bone.parent
                    desired[parent.name] = desired[parent.parent.name] @ self.rest[parent.parent.name].inverted() @ self.rest[parent.name]
                self.rotation(name, (.055 if fast else .025)*math.sin(a-i*.42)-turn*.12, 'Z', desired)
        for name in ('ear.L', 'ear.R', 'charm'):
            if name in self.rest:
                self.rotation(name, (.025 if fast else .012)*math.sin(a-.5), 'X', desired)
        for bone in self.bones:
            name = bone.name
            parent = bone.parent
            if name not in desired:
                desired[name] = (desired[parent.name] @ self.rest[parent.name].inverted() @ self.rest[name]
                                 if parent else self.rest[name])
            local = self.rest[parent.name].inverted() @ self.rest[name] if parent else self.rest[name]
            basis = local.inverted() @ (desired[parent.name].inverted() if parent else Matrix.Identity(4)) @ desired[name]
            pb = self.rig.pose.bones[name]
            pb.matrix_basis = basis
            if pb.rotation_mode == 'XYZ' and name in self.previous:
                compatible = pb.rotation_euler.copy()
                compatible.make_compatible(self.previous[name])
                pb.rotation_euler = compatible
            self.previous[name] = pb.rotation_euler.copy()

    def bake(self):
        prefix = 'MOB_'+self.key.upper()+'_'
        for kind, frames in (('idle', 48), ('walk', 24), ('run', 16), ('turn_left', 12), ('turn_right', 12)):
            old = bpy.data.actions.get(prefix+kind.upper())
            if old:
                bpy.data.actions.remove(old)
            self.rig.animation_data_create()
            self.rig.animation_data.action = None
            self.previous.clear()
            samples = 48 if kind in ('walk', 'run') else frames
            for sample in range(samples+1):
                frame = 1+frames*sample/samples
                self.pose(kind, sample/samples)
                for bone in self.rig.pose.bones:
                    bone.keyframe_insert('location', frame=frame, group=bone.name)
                    bone.keyframe_insert('rotation_euler', frame=frame, group=bone.name)
            action = self.rig.animation_data.action
            action.name = prefix+kind.upper()
            action.use_fake_user = True
            action['clip'], action['loop'], action['fps'] = kind, not kind.startswith('turn_'), 24
            for layer in action.layers:
                for strip in layer.strips:
                    for bag in strip.channelbags:
                        for curve in bag.fcurves:
                            for key in curve.keyframe_points:
                                key.interpolation = 'LINEAR'
        self.rig.animation_data.action = None
        for bone in self.rig.pose.bones:
            bone.matrix_basis.identity()


def apply_rig(rig, key):
    rig.animation_data_create()
    rig.animation_data.action = None
    for bone in rig.pose.bones:
        bone.matrix_basis.identity()
    add_structure(rig)
    for bone in rig.pose.bones:
        bone.rotation_mode = 'XYZ'
    bind_weights(rig)
    Baker(rig, key).bake()
    bpy.context.view_layer.update()
    points = [rig.matrix_world.inverted() @ obj.matrix_world @ v.co
              for obj in bpy.context.scene.objects if obj.type == 'MESH' for v in obj.data.vertices]
    height = max(p.z for p in points)-min(p.z for p in points)
    profile = PROFILE[key]
    return {'version':1, 'walkGait':'four_beat_walk', 'runGait':profile['fast'],
            'walkStrideRatio':profile['walk']/height, 'runStrideRatio':profile['run']/height,
            'runThresholdRatio':profile['threshold']/height, 'sourceHeight':height,
            'stanceFraction':profile['duty'], 'runStanceFraction':profile['fast_duty'],
            'method':'planted stance, two-bone IK bake, articulated toes, chest/pelvis weighting'}


def update_entry(entry, rig, metadata):
    entry['boneCount'] = len(rig.data.bones)
    entry['locomotion'] = metadata
    for clip, frames in (('idle', 48), ('walk', 24), ('run', 16), ('turn_left', 12), ('turn_right', 12)):
        entry['clips'][clip] = {'firstFrame':1, 'lastFrame':frames+1, 'loop':not clip.startswith('turn_')}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--species', choices=(*PROFILE, 'all'), default='all')
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    sys.path.insert(0, str(ROOT/'Art/Blender/ModularCollection'))
    import surface_forms
    catalog_path = HERE/'mob_catalog.json'
    catalog = json.loads(catalog_path.read_text(encoding='utf-8'))
    for key in PROFILE if args.species == 'all' else (args.species,):
        entry = next(e for e in catalog['species'] if e['key'] == key)
        source = HERE/entry['model']
        bpy.ops.wm.open_mainfile(filepath=str(source))
        bpy.context.preferences.filepaths.save_version = 0
        if hasattr(bpy.context.preferences.filepaths, 'save_preview_images'):
            bpy.context.preferences.filepaths.save_preview_images = False
        rig = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
        metadata = apply_rig(rig, key)
        update_entry(entry, rig, metadata)
        bpy.ops.wm.save_as_mainfile(filepath=str(source))
        bpy.ops.object.select_all(action='DESELECT')
        rig.select_set(True)
        for obj in bpy.context.scene.objects:
            if obj.type == 'MESH':
                obj.select_set(True)
        output = HERE/entry['export']
        temporary = output.with_name('locomotion_export.glb')
        with surface_forms.export_materials():
            bpy.ops.export_scene.gltf(filepath=str(temporary), export_format='GLB',
                                      use_selection=True, export_animations=True,
                                      export_animation_mode='ACTIONS', export_extras=True)
        # Avoid truncating the active GLB while a sync provider is inspecting it.
        temporary.replace(output)
        catalog_path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
        print('QUADRUPED_READY', key, len(rig.data.bones), metadata, flush=True)


if __name__ == '__main__':
    main()
