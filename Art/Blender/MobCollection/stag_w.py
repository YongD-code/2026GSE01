"""Author W in the stag cast slot and retain Q in the attack slot."""
import json
import sys
from pathlib import Path
import bpy
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
from quadruped_locomotion import Baker, smooth

def author(rig):
    rig.animation_data_create()
    rig.animation_data.action=None
    old=bpy.data.actions.get('MOB_STAG_CAST')
    if old:
        bpy.data.actions.remove(old)
    baker=Baker(rig,'stag')
    for frame in range(1,37):
        age=(frame-1)/24
        baker.pose('idle',age/4)
        bow=smooth(age/.6)*(1-smooth((age-.8)/.7))
        rig.pose.bones['head'].rotation_euler.x += .19*bow
        for bone in rig.pose.bones:
            bone.keyframe_insert('location',frame=frame,group=bone.name)
            bone.keyframe_insert('rotation_euler',frame=frame,group=bone.name)
    action=rig.animation_data.action
    action.name='MOB_STAG_CAST'
    action.use_fake_user=True
    action['skill']='숲의 숨결'
    action['loop']=False
    rig.animation_data.action=None
    for bone in rig.pose.bones: bone.matrix_basis.identity()

def metadata():
    return {'name':'MOB_STAG_CAST','firstFrame':1,'lastFrame':36,'loop':False}

def skill():
    return {'name':'숲의 숨결','animation':'MOB_STAG_CAST','charge':.6,'active':4.,
            'fade':.65,'radius':100.,'pulse':.5,'healPerPulse':3.,'cooldown':12.}

if __name__=='__main__':
    sys.path.insert(0,str(HERE.parent/'ModularCollection'))
    import surface_forms as f
    bpy.ops.wm.open_mainfile(filepath=str(HERE/'stag/model.blend'))
    rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
    author(rig)
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'stag/model.blend'))
    bpy.ops.object.select_all(action='DESELECT')
    for obj in bpy.context.scene.objects:
        if obj.type in ('MESH','ARMATURE'): obj.select_set(True)
    with f.export_materials():
        bpy.ops.export_scene.gltf(filepath=str(HERE/'stag/model.pending.glb'),export_format='GLB',
            use_selection=True,export_animations=True,export_animation_mode='ACTIONS',export_extras=True)
    (HERE/'stag/model.pending.glb').replace(HERE/'stag/model.glb')
    path=HERE/'mob_catalog.json'
    catalog=json.loads(path.read_text(encoding='utf-8'))
    entry=next(e for e in catalog['species'] if e['key']=='stag')
    entry['clips']['cast']=metadata()
    entry.setdefault('skills',{})['W']=skill()
    path.write_text(json.dumps(catalog,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('STAG_W_AUTHORED')
