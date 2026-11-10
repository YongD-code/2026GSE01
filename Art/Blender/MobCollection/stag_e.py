"""Bake the flower-path leap into the stag's existing Jump slot."""
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from quadruped_locomotion import Baker, smooth


class LeapBaker(Baker):
    def leg(self, name, phase, duty, distance, lift, desired, turn=0):
        front = name.startswith('front')
        lift = self.fore if front else self.hind
        if lift > 0:
            phase, duty = .85, .7
        super().leg(name, phase, duty, distance, lift, desired, turn)


def author(rig):
    rig.animation_data_create()
    rig.animation_data.action = None
    old = bpy.data.actions.get('MOB_STAG_JUMP')
    if old:
        bpy.data.actions.remove(old)
    baker = LeapBaker(rig, 'stag')
    baker.profile = baker.profile.copy()
    for frame in range(1, 21):
        age = .76 * (frame - 1) / 19
        t = max(0., min(1., (age - .16) / .44))
        arc = 4 * t * (1 - t)
        crouch = math.sin(math.pi * min(1., age / .24)) ** 2
        landing = math.sin(math.pi * max(0., min(1., (age - .54) / .22))) ** 2
        baker.profile['drop'] = .12 + .125 * crouch + .095 * landing
        baker.fore = .28 * arc * (1 - smooth((t - .70) / .30))
        baker.hind = .31 * arc + .15 * landing * (1 - smooth((age - .62) / .14))
        baker.pose('idle', 0.)
        root = rig.pose.bones['root']
        root.location += root.bone.matrix_local.to_3x3().inverted() @ Vector((0, 0, .55 * arc))
        rig.pose.bones['head'].rotation_euler.x += .06 * arc - .11 * (crouch + landing)
        for bone in rig.pose.bones:
            bone.keyframe_insert('location', frame=frame, group=bone.name)
            bone.keyframe_insert('rotation_euler', frame=frame, group=bone.name)
    action = rig.animation_data.action
    action.name = 'MOB_STAG_JUMP'
    action.use_fake_user = True
    action['skill'] = '꽃길 도약'
    action['loop'] = False
    rig.animation_data.action = None
    for bone in rig.pose.bones:
        bone.matrix_basis.identity()


def main():
    sys.path.insert(0, str(HERE.parent/'ModularCollection'))
    import surface_forms as forms

    folder = HERE/'stag'
    bpy.ops.wm.open_mainfile(filepath=str(folder/'model.blend'))
    rig = next(obj for obj in bpy.context.scene.objects if obj.type == 'ARMATURE')
    author(rig)
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(folder/'model.blend'))
    bpy.ops.object.select_all(action='DESELECT')
    for obj in bpy.context.scene.objects:
        if obj.type in ('MESH', 'ARMATURE'):
            obj.select_set(True)
    with forms.export_materials():
        bpy.ops.export_scene.gltf(filepath=str(folder/'model.pending.glb'), export_format='GLB',
                                  use_selection=True, export_animations=True,
                                  export_animation_mode='ACTIONS', export_extras=True)
    (folder/'model.pending.glb').replace(folder/'model.glb')
    path = HERE/'mob_catalog.json'
    catalog = json.loads(path.read_text(encoding='utf-8'))
    entry = next(e for e in catalog['species'] if e['key'] == 'stag')
    entry['clips']['jump'] = {'name': 'MOB_STAG_JUMP', 'firstFrame': 1, 'lastFrame': 20, 'loop': False}
    entry.setdefault('skills', {})['E'] = {
        'name': '꽃길 도약', 'animation': 'MOB_STAG_JUMP', 'reach': 160,
        'charge': .16, 'land': .60, 'animationDuration': .76,
        'radius': 72, 'slow': 1, 'cooldown': 8,
    }
    path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('STAG_E_AUTHORED')


if __name__ == '__main__':
    main()
