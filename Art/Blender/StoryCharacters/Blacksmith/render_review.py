"""Render modelling review angles only; never changes the saved character source."""

import os
import sys
import time
from pathlib import Path

import bpy
from mathutils import Vector

HERE = Path(__file__).resolve().parent
ONLY = set(sys.argv[sys.argv.index('--select')+1].split(',')) if '--select' in sys.argv else set()
bpy.ops.wm.open_mainfile(filepath=str(HERE/'model.blend'))
scene = bpy.context.scene
scene.render.engine = 'BLENDER_EEVEE'
scene.eevee.taa_render_samples = 64
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'
scene.render.film_transparent = True
scene.view_settings.view_transform = 'Standard'
scene.world.color = (.07, .075, .09)
for name, position, power, color, size in [
        ('SMITH_REVIEW_KEY', (-3, -4, 5), 520, (1, .89, .75), 3),
        ('SMITH_REVIEW_FILL', (4, -3, 3), 190, (.70, .81, 1), 3),
        ('SMITH_REVIEW_RIM', (2, 3, 4), 620, (1, .75, .47), 2.5)]:
    data = bpy.data.lights.new(name, 'AREA')
    data.energy, data.color, data.size = power, color, size
    obj = bpy.data.objects.new(name, data)
    scene.collection.objects.link(obj)
    obj.location = position
    obj.rotation_euler = (Vector((0, 0, 1))-obj.location).to_track_quat('-Z', 'Y').to_euler()
camera_data = bpy.data.cameras.new('SMITH_REVIEW_CAMERA')
camera = bpy.data.objects.new('SMITH_REVIEW_CAMERA', camera_data)
scene.collection.objects.link(camera)
scene.camera = camera
camera_data.type = 'ORTHO'
views = [
    ('quarter', (4.5, -7, 3.5), (0, -.03, .99), 2.65, 620, 740),
    ('front', (0, -7, 2.25), (0, -.03, .99), 2.65, 620, 740),
    ('profile', (7, -.35, 2.35), (0, -.03, .99), 2.65, 620, 740),
    ('back', (4, 7, 3.35), (0, -.03, .99), 2.65, 620, 740),
    ('portrait', (1.8, -7, 2.5), (0, -.16, 1.47), 1.23, 800, 800),
]
for key, position, target, scale, width, height in views:
    if ONLY and key not in ONLY:
        continue
    camera.location = position
    camera.rotation_euler = (Vector(target)-camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera_data.ortho_scale = scale
    scene.render.resolution_x, scene.render.resolution_y = width, height
    folder = HERE/'Previews'
    folder.mkdir(parents=True, exist_ok=True)
    temporary, output = folder/(key+'.render.png'), folder/(key+'.png')
    scene.render.filepath = str(temporary)
    bpy.ops.render.render(write_still=True)
    for attempt in range(8):
        try:
            os.replace(temporary, output)
            break
        except PermissionError:
            if attempt == 7:
                raise
            time.sleep(.5)
    print('SMITH_PREVIEW_READY', key, flush=True)
