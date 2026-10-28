"""Author stag Q in the existing attack slot while preserving its previous cast pose."""
import json
import math
import re
import sys
from pathlib import Path

import bpy

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(HERE))
from quadruped_locomotion import Baker


def specification():
    source = (ROOT/'SimpleGame/StagSkill.h').read_text(encoding='utf-8')
    values = {name: float(value) for name, value in
              re.findall(r'constexpr float (\w+) = ([.\d]+)f?;', source)}
    values['Duration'] = values['Charge']+values['Travel']+values.get('BloomRise', 0)+values['Fade']
    return values


def smooth(value):
    value = min(1., max(0., value))
    return value*value*(3-2*value)


def author(rig):
    spec = specification()
    old = bpy.data.actions.get('MOB_STAG_ATTACK')
    if not bpy.data.actions.get('MOB_STAG_CAST') and old:
        cast = old.copy()
        cast.name = 'MOB_STAG_CAST'
        cast.use_fake_user = True
    rig.animation_data_create()
    rig.animation_data.action = None
    if old:
        bpy.data.actions.remove(old)
    baker = Baker(rig, 'stag')
    for frame in range(1, 34):
        age = (frame-1)/32*spec['Animation']
        baker.pose('idle', (frame-1)/32)
        if age < spec['Charge']:
            pitch = -.22*smooth(age/spec['Charge'])
        elif age < spec['Charge']+.24:
            pitch = -.22+.60*smooth((age-spec['Charge'])/.24)
        else:
            pitch = .38*(1-smooth((age-spec['Charge']-.24)/(spec['Animation']-spec['Charge']-.24)))
        rig.pose.bones['head'].rotation_euler.x += pitch
        for name in ('ear.L', 'ear.R', 'charm'):
            if name in rig.pose.bones:
                rig.pose.bones[name].rotation_euler.z += .035*math.sin(age*8)
        for bone in rig.pose.bones:
            bone.keyframe_insert('location', frame=frame, group=bone.name)
            bone.keyframe_insert('rotation_euler', frame=frame, group=bone.name)
    action = rig.animation_data.action
    action.name = 'MOB_STAG_ATTACK'
    action.use_fake_user = True
    action['clip'] = 'attack'
    action['skill'] = '길틔움'
    action['loop'] = False
    rig.animation_data.action = None
    for bone in rig.pose.bones:
        bone.matrix_basis.identity()


def metadata():
    return {'attack': {'name':'MOB_STAG_ATTACK', 'firstFrame':1, 'lastFrame':33, 'loop':False},
            'cast': {'name':'MOB_STAG_CAST', 'firstFrame':1, 'lastFrame':19, 'loop':False}}


def main():
    sys.path.insert(0, str(HERE.parent/'ModularCollection'))
    import surface_forms
    source = HERE/'stag/model.blend'
    bpy.ops.wm.open_mainfile(filepath=str(source))
    bpy.context.preferences.filepaths.save_version = 0
    if hasattr(bpy.context.preferences.filepaths, 'save_preview_images'):
        bpy.context.preferences.filepaths.save_preview_images = False
    rig = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
    author(rig)
    bpy.ops.wm.save_as_mainfile(filepath=str(source))
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    for obj in bpy.context.scene.objects:
        if obj.type == 'MESH':
            obj.select_set(True)
    temporary = HERE/'stag/skill_export.glb'
    with surface_forms.export_materials():
        bpy.ops.export_scene.gltf(filepath=str(temporary), export_format='GLB', use_selection=True,
                                 export_animations=True, export_animation_mode='ACTIONS', export_extras=True)
    temporary.replace(HERE/'stag/model.glb')
    path = HERE/'mob_catalog.json'
    catalog = json.loads(path.read_text(encoding='utf-8'))
    entry = next(e for e in catalog['species'] if e['key'] == 'stag')
    entry['clips'].update(metadata())
    entry['skills'] = {'Q': {'name':'길틔움', 'animation':'MOB_STAG_ATTACK', 'reviewOnly':False,
                             'timing':specification()}}
    path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('STAG_Q_AUTHORED', flush=True)


if __name__ == '__main__':
    main()
