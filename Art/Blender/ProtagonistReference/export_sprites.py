"""Render the approved model with the existing game's camera and animation layout."""
import ast
import bpy
import math
from array import array
from pathlib import Path
from mathutils import Vector

HERE = Path(__file__).resolve().parent
OUT = HERE / 'Sprites'
OUT.mkdir(exist_ok=True)
source = (HERE.parent / 'create_protagonist.py').read_text(encoding='utf-8')
bpy.ops.wm.open_mainfile(filepath=str(HERE / 'protagonist_candidate.blend'))
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
rig.animation_data.action = None
cam = scene.camera
cam.location = (0,-6,3.7)
cam.rotation_euler = (Vector((0,0,1.18))-cam.location).to_track_quat('-Z','Y').to_euler()
cam.data.ortho_scale = 3.15
scene.render.film_transparent = True
scene.render.image_settings.color_mode = 'RGBA'
scene.render.resolution_x = scene.render.resolution_y = SIZE
scene.display.render_aa = '8'
# Keep row order, neutral column, frame count and pixel anchor unchanged.
export_code = source[source.index('exports = '):]
exec(compile(export_code, str(HERE.parent/'create_protagonist.py'), 'exec'))
