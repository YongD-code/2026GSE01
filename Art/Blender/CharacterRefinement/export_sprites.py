"""Render the review candidate using the game's camera and animation layout."""
import ast
import bpy
import math
import sys
from array import array
from pathlib import Path
from mathutils import Vector

HERE = Path(__file__).resolve().parent
OUT = HERE / 'Sprites'
OUT.mkdir(exist_ok=True)
source = (HERE.parent / 'create_protagonist.py').read_text(encoding='utf-8')
bpy.ops.wm.open_mainfile(filepath=str(HERE / 'protagonist_refined.blend'))
scene = bpy.context.scene
rig = bpy.data.objects['주인공_리그']
orb = bpy.data.objects['주술 구체']
CUSTOMIZE = None
ASSET_NAME = 'protagonist'
SIZE = 192
# Reuse only the established pose function; never execute the model generator.
tree = ast.parse(source)
pose_node = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'pose')
exec(compile(ast.Module(body=[pose_node], type_ignores=[]), str(HERE.parent/'create_protagonist.py'), 'exec'))
sys.path.insert(0, str(HERE))
from locomotion import apply_locomotion
base_pose = pose

def pose(kind, t):
    base_pose(kind, t)
    apply_locomotion(rig, kind, t)
    bpy.context.view_layer.update()

rig.animation_data.action = None
# Replace only the two locomotion actions; retain idle and casting actions.
for kind, frames, label in [('walk', 24, '걷기'), ('run', 16, '달리기')]:
    old = bpy.data.actions.get(label)
    if old:
        bpy.data.actions.remove(old)
    rig.animation_data.action = None
    for frame in range(frames + 1):
        pose(kind, frame / frames)
        for bone in rig.pose.bones:
            bone.keyframe_insert('rotation_euler', frame=frame+1, group=bone.name)
            bone.keyframe_insert('location', frame=frame+1, group=bone.name)
    rig.animation_data.action.name = label
    rig.animation_data.action.use_fake_user = True
rig.animation_data.action = None
pose('idle', 0)
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(HERE / 'protagonist_refined.blend'))
cam = scene.camera
cam.location = (0,-6,3.7)
cam.rotation_euler = (Vector((0,0,1.18))-cam.location).to_track_quat('-Z','Y').to_euler()
cam.data.ortho_scale = 3.15
scene.render.film_transparent = True
scene.render.image_settings.color_mode = 'RGBA'
scene.render.resolution_x = scene.render.resolution_y = SIZE
scene.eevee.taa_render_samples = 16
# Keep row order, neutral column, frame count and pixel anchor unchanged.
export_code = source[source.index('exports = '):]
if '--locomotion-only' in sys.argv:
    export_code = export_code.replace("[('walk',9),('run',9),('cast',4)]", "[('walk',9),('run',9)]")
export_code = export_code.replace("('walk',9)", "('walk',17)").replace("('run',9)", "('run',17)")
export_code = export_code.replace('(col-1)/8', '(col-1)/16')
exec(compile(export_code, str(HERE.parent/'create_protagonist.py'), 'exec'))

