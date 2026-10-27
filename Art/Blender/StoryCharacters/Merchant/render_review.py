"""Render the real merchant source in body, face and workstation review views."""

import os
import sys
from pathlib import Path

import bpy
from mathutils import Vector

HERE = Path(__file__).resolve().parent
bpy.ops.wm.open_mainfile(filepath=str(HERE/'model.blend'))
scene = bpy.context.scene
scene.render.engine = 'BLENDER_EEVEE'
scene.eevee.taa_render_samples = 48
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'
scene.render.film_transparent = True
scene.view_settings.view_transform = 'Standard'
scene.world.color = (.07, .075, .09)
for name, position, power, color in [
    ('SHOP_KEY', (-3, -4, 5), 520, (1, .89, .75)),
    ('SHOP_FILL', (4, -3, 3), 190, (.70, .81, 1)),
    ('SHOP_RIM', (2, 3, 4), 620, (1, .75, .47)),
]:
    data = bpy.data.lights.new(name, 'AREA')
    data.energy, data.color, data.size = power, color, 3
    obj = bpy.data.objects.new(name, data)
    scene.collection.objects.link(obj)
    obj.location = position
    obj.rotation_euler = (Vector((0, 0, 1.2))-obj.location).to_track_quat('-Z', 'Y').to_euler()
camera = bpy.data.objects.new('SHOP_CAMERA', bpy.data.cameras.new('SHOP_CAMERA'))
scene.collection.objects.link(camera)
scene.camera = camera
camera.data.type = 'ORTHO'
props = [obj for obj in scene.objects if obj.type == 'MESH' and any(
    obj.name.endswith(suffix) for suffix in ('COUNTER', 'PARCEL', 'COUNTER_GOODS'))]
views = [
    ('quarter', (4.5, -7, 3.6), (0, -.02, 1.14), 2.9, 620, 740, False),
    ('front', (0, -7, 2.7), (0, -.02, 1.14), 2.9, 620, 740, False),
    ('profile', (7, -.35, 2.7), (0, -.02, 1.14), 2.9, 620, 740, False),
    ('back', (4, 7, 3.6), (0, -.02, 1.14), 2.9, 620, 740, False),
    ('portrait', (1.4, -7, 2.8), (0, -.11, 1.94), 1.08, 800, 800, False),
    ('counter', (4.5, -7, 3.6), (0, -.13, 1.14), 3.0, 620, 740, True),
]
only = set(sys.argv[sys.argv.index('--select')+1].split(',')) if '--select' in sys.argv else set()
if '--motion' in sys.argv:
    import json
    meta = json.loads((HERE/'character.json').read_text())
    rig = bpy.data.objects['NPC_MERCHANT_RIG']
    folder = HERE/'Previews'/'Motion'
    folder.mkdir(parents=True, exist_ok=True)
    scene.eevee.taa_render_samples = 24
    scene.render.resolution_x, scene.render.resolution_y = 440, 540
    camera.location = (4.5, -7, 3.6)
    camera.rotation_euler = (Vector((0, -.13, 1.14))-camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.data.ortho_scale = 3.0
    metrics = {}
    for clip, step in [('idle', 8), ('sort', 3), ('pack', 3), ('greet', 3)]:
        if only and clip not in only:
            continue
        action = bpy.data.actions[meta['clips'][clip]['action']]
        rig.animation_data.action = action
        if action.slots:
            rig.animation_data.action_slot = action.slots[0]
        frames = [25, 46, 73] if '--contact' in sys.argv else range(1, meta['clips'][clip]['lastFrame']+1, step)
        samples = []
        for frame in frames:
            scene.frame_set(frame)
            bpy.context.view_layer.update()
            graph = bpy.context.evaluated_depsgraph_get()
            boots = [bpy.data.objects['NPC_MERCHANT_SHOE_'+side] for side in ('L', 'R')]
            floor = min(v.co.z for obj in boots for v in obj.evaluated_get(graph).data.vertices)
            assert abs(floor-.012) < .0001, (clip, frame, floor)
            samples.append({'frame': frame, 'shoeFloor': round(floor, 6)})
            scene.render.filepath = str(folder/f'{clip}_{frame:03}.png')
            bpy.ops.render.render(write_still=True)
        metrics[clip] = samples
    (folder/('contact_metrics.json' if '--contact' in sys.argv else 'motion_metrics.json')).write_text(
        json.dumps(metrics, indent=2)+'\n', encoding='utf-8')
    print('MERCHANT_MOTION_READY', {k: len(v) for k, v in metrics.items()}, flush=True)
    views = []
for key, position, target, scale, width, height, show_props in views:
    if only and key not in only:
        continue
    for obj in props:
        obj.hide_render = not show_props
    camera.location = position
    camera.rotation_euler = (Vector(target)-camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.data.ortho_scale = scale
    scene.render.resolution_x, scene.render.resolution_y = width, height
    folder = HERE/'Previews'
    folder.mkdir(parents=True, exist_ok=True)
    temporary, output = folder/(key+'.render.png'), folder/(key+'.png')
    scene.render.filepath = str(temporary)
    bpy.ops.render.render(write_still=True)
    os.replace(temporary, output)
    print('MERCHANT_PREVIEW', key, flush=True)
