"""Render approved candidate A into compact idle/glide/cast clips for the game.

Blender --background --factory-startup --python Art/Blender/SpiritStudy/export_selected.py
Then run pack_selected.py with Pillow. The approved model and old sprite stay untouched.
"""
import ast
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector

OUT=Path(__file__).resolve().parent
ROOT=OUT.parents[2]
DEST=OUT/'SelectedA'
DEST.mkdir(exist_ok=True)
TAU=math.tau
CELL=128
CLIPS=[('idle',0,12),('glide',12,12),('cast',24,8)]
sys.path.insert(0,str(OUT))
from fire_volume import animate_fire
PROTECTED=[OUT/'forgotten_candidate.blend',ROOT/'SimpleGame/Assets/Spirits/ash_lantern_4dir.png',
           OUT/'Variants/round_lantern/candidate.blend',OUT/'Variants/split_mask/candidate.blend']
before={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in PROTECTED}
tree=ast.parse((OUT/'create_forgotten.py').read_text(encoding='utf-8'))
functions=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in {'pose','camera_view','render'}]
exec(compile(ast.Module(body=functions,type_ignores=[]),'<approved lantern poses>','exec'),globals())
base_pose=pose
bpy.ops.wm.open_mainfile(filepath=str(OUT/'forgotten_candidate.blend'))
scene=bpy.context.scene
camera=scene.camera
rig=bpy.data.objects['남겨진 자 · 부유 리그']
rig.animation_data_clear()
camera_view(0)
scene.eevee.taa_render_samples=32


def pose(kind,phase):
    base_pose('glide' if kind=='glide' else 'idle',phase if kind!='cast' else 0)
    if kind=='cast':
        power=math.sin(math.pi*phase)
        for side,sign in [('L',-1),('R',1)]:
            rig.pose.bones['arm.'+side].rotation_euler.x-=.62*power
            rig.pose.bones['arm.'+side].rotation_euler.y+=sign*.15*power
            rig.pose.bones['hand.'+side].rotation_euler.z+=sign*.34*power
        rig.pose.bones['mask'].rotation_euler.x-=.10*power
        for i in range(4):
            rig.pose.bones[f'tip.{i}'].rotation_euler.x-=.14*power
        rig.pose.bones['flame'].scale=(1+.12*power,1+.26*power,1+.12*power)
        animate_fire(phase)
    bpy.context.view_layer.update()


yaws=[]
folder=DEST/'Frames';folder.mkdir(exist_ok=True)
for row in range(8):
    phi=row*TAU/8
    yaw=math.atan2(-math.sin(phi),math.cos(phi)/math.sin(math.atan2(4.05,8)))
    yaws.append(yaw)
    rig.rotation_euler.z=yaw
    for kind,first,count in CLIPS:
        for frame in range(count):
            phase=frame/(count-1) if kind=='cast' else frame/count
            pose(kind,phase)
            render(folder/f'd{row}_f{first+frame:02d}.png',CELL,CELL)
    print('SELECTED_DIRECTION',row+1,'/8',flush=True)
after={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in PROTECTED}
if before!=after:raise RuntimeError('A protected source changed')
report={'selected':'A','status':'rendered_for_integration','cellSize':CELL,'columns':32,'rows':8,
        'clips':{kind:{'first':first,'frames':count} for kind,first,count in CLIPS},
        'loopSeconds':4/3,'directionRows':list(range(8)),'directionYaws':yaws,
        'worldOriginAnchorY':.5+math.cos(math.atan2(4.05,8))*1.92/4.8,
        'protectedBefore':before,'protectedAfter':after,'gameBuildRunTested':False}
(DEST/'render_manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('SELECTED_A_RENDER_READY',flush=True)
