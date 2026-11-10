"""Render the five in-game tree sprites and the stag's realm cut-in."""
import sys
from pathlib import Path
import bpy
from mathutils import Vector

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1]))
import render_previews as review

OUT = HERE.parents[4]/'SimpleGame/Assets/Effects'
bpy.ops.wm.open_mainfile(filepath=str(HERE/'sanctuary.blend'))
scene = bpy.context.scene
camera = scene.camera
trees = sorted((ob for ob in scene.objects if ob.name.startswith('R_SANCTUARY_TREE_')),
               key=lambda ob: ob.name)
effects = [ob for ob in scene.objects if ob.type in ('CURVE','MESH')]
assert len(trees) == 5

def aim(target, direction, scale):
    camera.location = target + direction
    camera.rotation_euler = (target - camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.ortho_scale = scale

scene.frame_set(61)
scene.render.film_transparent = True
scene.render.resolution_x, scene.render.resolution_y = 320, 395
for i, tree in enumerate(trees):
    for ob in effects:
        ob.hide_render = ob.parent != tree
    aim(tree.location + Vector((0,0,1.50)), Vector((5,-8,4.6)), 4.0)
    review.render(OUT/'StagSanctuary'/f'tree_{i+1}.png')

scene.frame_set(16)
for ob in effects:
    ob.hide_render = ob.parent in trees
scene.render.film_transparent = False
scene.render.resolution_x, scene.render.resolution_y = 640, 359
aim(Vector((0,-.60,1.64)), Vector((4,-8,1.65)), 3.4)
review.render(OUT/'stag_ultimate.png')
