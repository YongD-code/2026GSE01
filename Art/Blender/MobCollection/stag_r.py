"""Bake the Waykeeper's Sanctuary as a named stag clip."""
import json
import sys
from pathlib import Path

import bpy

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from quadruped_locomotion import Baker, smooth


def author(rig):
    rig.animation_data_create()
    rig.animation_data.action = None
    old = bpy.data.actions.get('MOB_STAG_R_SANCTUARY')
    if old:
        bpy.data.actions.remove(old)
    baker = Baker(rig, 'stag')
    for frame in range(1, 34):
        age = (frame - 1) / 24
        baker.pose('idle', age * .35)
        bow = smooth(age / .8) * (1 - smooth((age - .88) / .48))
        rig.pose.bones['head'].rotation_euler.x += .20 * bow
        for bone in rig.pose.bones:
            bone.keyframe_insert('location', frame=frame, group=bone.name)
            bone.keyframe_insert('rotation_euler', frame=frame, group=bone.name)
    action = rig.animation_data.action
    action.name = 'MOB_STAG_R_SANCTUARY'
    action.use_fake_user = True
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
    entry.setdefault('skills', {})['R'] = {
        'name': '길지기의 성역', 'animation': 'MOB_STAG_R_SANCTUARY',
        'opening': .8, 'active': 6, 'fade': .65, 'radius': 180,
        'damageMultiplier': .75, 'enemyMovementMultiplier': .75, 'cooldown': 40,
    }
    path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('STAG_R_AUTHORED')


if __name__ == '__main__':
    main()
