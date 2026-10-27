"""Render the real keyed rig with a preview anvil; never save over the model."""

import json
import sys
from pathlib import Path

import bpy
from mathutils import Vector

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1]/'ModularCollection'))
import create_collection as g
import surface_forms as f

bpy.ops.wm.open_mainfile(filepath=str(HERE/'model.blend'))
scene = bpy.context.scene
rig = bpy.data.objects['NPC_SMITH_RIG']
meta = json.loads((HERE/'character.json').read_text())
folder = HERE/'Previews'/'Motion'
folder.mkdir(parents=True, exist_ok=True)
scene.render.engine = 'BLENDER_EEVEE'
scene.eevee.taa_render_samples = 32
scene.render.resolution_x, scene.render.resolution_y = 480, 520
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'
scene.render.film_transparent = True
scene.view_settings.view_transform = 'Standard'
scene.world.color = (.07, .075, .09)
for name, position, energy, color in [
    ('MOTION_KEY', (-3, -4, 5), 520, (1, .89, .75)),
    ('MOTION_FILL', (4, -3, 3), 190, (.70, .81, 1)),
    ('MOTION_RIM', (2, 3, 4), 620, (1, .75, .47)),
]:
    data = bpy.data.lights.new(name, 'AREA')
    data.energy, data.color, data.size = energy, color, 3
    obj = bpy.data.objects.new(name, data)
    scene.collection.objects.link(obj)
    obj.location = position
    obj.rotation_euler = (Vector((0, 0, 1))-obj.location).to_track_quat('-Z', 'Y').to_euler()
camera = bpy.data.objects.new('MOTION_CAMERA', bpy.data.cameras.new('MOTION_CAMERA'))
scene.collection.objects.link(camera)
scene.camera = camera
camera.data.type, camera.data.ortho_scale = 'ORTHO', 3.05
camera.location = (4, -7, 3.4)
camera.rotation_euler = (Vector((.12, -.16, 1.02))-camera.location).to_track_quat('-Z', 'Y').to_euler()

# A modelling contact reference, not part of the exported character's meshes.
g.COLORS.update({'preview_iron': (.12, .15, .16, 1),
                 'preview_iron_edge': (.24, .28, .29, 1),
                 'preview_wood': (.18, .095, .044, 1)})
anvil = []
for location, size, color in [
    ((.72, -.76, .27), (.36, .34, .54), 'preview_wood'),
    ((.72, -.76, .56), (.43, .32, .09), 'preview_iron'),
    ((.72, -.76, .65), (.22, .21, .15), 'preview_iron'),
    ((.72, -.76, .740), (.62, .28, .05), 'preview_iron_edge'),
]:
    obj = g.cube(location, size, color)
    obj.name = 'REVIEW_ANVIL_'+str(len(anvil))
    anvil.append(obj)
f.stylize_scene()

contact_only = '--contact' in sys.argv
metrics = {}
boots = [bpy.data.objects['NPC_SMITH_BOOT_'+side] for side in ('L', 'R')]
hammer = bpy.data.objects['NPC_SMITH_FORGING_HAMMER']
for clip, step in [('idle', 4), ('work', 3), ('greet', 3)]:
    if contact_only and clip != 'work':
        continue
    action = bpy.data.actions[meta['clips'][clip]['action']]
    rig.animation_data.action = action
    if action.slots:
        rig.animation_data.action_slot = action.slots[0]
    frames = [1, 31, 39, 43] if contact_only else range(1, meta['clips'][clip]['lastFrame']+1, step)
    poses = []
    for frame in frames:
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        graph = bpy.context.evaluated_depsgraph_get()
        floor = min((obj.matrix_world @ vertex.co).z
                    for obj in boots for vertex in obj.evaluated_get(graph).data.vertices)
        assert floor >= -.0001, (clip, frame, floor)
        posed_hammer = hammer.evaluated_get(graph)
        bottom = min((hammer.matrix_world @ vertex.co).z for vertex in posed_hammer.data.vertices)
        poses.append({'frame': frame, 'bootFloor': round(floor, 6), 'hammerBottom': round(bottom, 6)})
        for obj in anvil:
            obj.hide_render = clip != 'work'
        scene.render.filepath = str(folder/f'{clip}_{frame:03}.png')
        bpy.ops.render.render(write_still=True)
    metrics[clip] = poses
(folder/('contact_metrics.json' if contact_only else 'motion_metrics.json')).write_text(
    json.dumps(metrics, indent=2)+'\n', encoding='utf-8')
print('STATIONARY_PREVIEWS_READY', {k: len(v) for k, v in metrics.items()}, flush=True)
