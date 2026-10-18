"""Render a short asset animation from the independent candidate, never the game."""
import ast
import math
import sys
from pathlib import Path

import bpy

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[2]
bpy.ops.wm.open_mainfile(filepath=str(OUT / 'gatherer_candidate.blend'))
scene = bpy.context.scene
rig = bpy.data.objects['채집꾼_정교화_리그']
rig.animation_data_clear()
sys.path.insert(0, str(ROOT / 'Art/Blender/CharacterRefinement'))
from locomotion import apply_locomotion

pose_node = next(n for n in ast.parse((OUT / 'create_candidate.py').read_text(encoding='utf-8')).body
                 if isinstance(n, ast.FunctionDef) and n.name == 'pose')
exec(compile(ast.Module(body=[pose_node], type_ignores=[]), '<candidate pose>', 'exec'), globals())
scene.render.resolution_x, scene.render.resolution_y = 360, 450
scene.eevee.taa_render_samples = 16
frames = OUT / 'Motion'
frames.mkdir(exist_ok=True)
for frame in range(16):
    pose('walk', frame / 16)
    scene.render.filepath = str(frames / f'walk_{frame:02d}.png')
    bpy.ops.render.render(write_still=True)
print('CANDIDATE_MOTION_READY', flush=True)
