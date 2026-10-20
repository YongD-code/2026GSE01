"""Render transparent review PNGs for the seven spirit models and NPC combinations."""

import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


HERE = Path(__file__).resolve().parent
NPC = HERE.parent / 'ModularCollection'
mob_catalog = json.loads((HERE / 'mob_catalog.json').read_text(encoding='utf-8'))
npc_catalog = json.loads((NPC / 'catalog.json').read_text(encoding='utf-8'))


def set_stage(target_z, scale):
    scene = bpy.context.scene
    for obj in list(scene.objects):
        if obj.type in ('LIGHT', 'CAMERA'):
            bpy.data.objects.remove(obj, do_unlink=True)
    scene.render.engine = 'BLENDER_EEVEE'
    scene.render.resolution_x = 440
    scene.render.resolution_y = 520
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    scene.render.film_transparent = True
    scene.render.image_settings.color_depth = '8'
    scene.render.fps = 24
    scene.view_settings.view_transform = 'Standard'
    scene.world.color = (.18, .20, .23)
    camera_data = bpy.data.cameras.new('REVIEW_CAMERA')
    camera = bpy.data.objects.new('REVIEW_CAMERA', camera_data)
    scene.collection.objects.link(camera)
    target = Vector((0, 0, target_z))
    camera.location = target + Vector((4.9, -7.5, 3.7))
    camera.rotation_euler = (target - camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera_data.type = 'ORTHO'
    camera_data.ortho_scale = scale
    scene.camera = camera
    for name, loc, energy, color in (
        ('KEY', (-3, -4, 6), 480, (1.0, .89, .76)),
        ('FILL', (3, -2, 4), 250, (.62, .77, 1.0)),
        ('RIM', (1, 3, 5), 530, (.78, .90, 1.0)),
    ):
        data = bpy.data.lights.new(name, 'AREA')
        data.energy = energy
        data.shape = 'DISK'
        data.size = 4
        data.color = color
        light = bpy.data.objects.new(name, data)
        scene.collection.objects.link(light)
        light.location = loc
        light.rotation_euler = (target - light.location).to_track_quat('-Z', 'Y').to_euler()


def render(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.context.scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    print('PREVIEW_READY', path, flush=True)


if '--npc-only' not in sys.argv and '--environment-only' not in sys.argv:
    for entry in mob_catalog['species']:
        bpy.ops.wm.open_mainfile(filepath=str(HERE / entry['model']))
        rig = bpy.data.objects.get(entry['rig'])
        if rig and rig.animation_data:
            rig.animation_data.action = bpy.data.actions.get('MOB_'+entry['key'].upper()+'_IDLE')
        bpy.context.scene.frame_set(1)
        set_stage(1.45 if entry['gait'] == 'quadruped' else 1.65,
                  4.35 if entry['gait'] == 'quadruped' else 4.6)
        render(HERE / 'Previews' / (entry['key'] + '.png'))

if '--environment-only' not in sys.argv:
    bpy.ops.wm.open_mainfile(filepath=str(NPC / 'npc_parts.blend'))
    set_stage(1.24, 3.20)
    parts = {obj.name: obj for obj in bpy.context.scene.objects if obj.type == 'MESH'}
    for entry in npc_catalog['npcs']:
        selected = set()
        for slot, index in entry['partIndices'].items():
            if index >= 0:
                selected.add(next(p['mesh'] for p in npc_catalog['parts'][slot] if p['index'] == index))
        for name, obj in parts.items():
            obj.hide_render = name not in selected
            obj.scale = (entry['scale'],)*3 if name in selected else (1, 1, 1)
        render(NPC / 'Previews' / (entry['id'] + '.png'))

if '--npc-only' not in sys.argv:
    bpy.ops.wm.open_mainfile(filepath=str(NPC / 'world_props.blend'))
    props = {obj.name: obj for obj in bpy.context.scene.objects if obj.type == 'MESH'}
    for entry in npc_catalog['scenery']:
        building = entry['slot'] == 'building'
        tree = 'TREE_' in entry['mesh']
        set_stage(1.55 if building or tree else .47,
                  5.8 if building else 4.8 if tree else 2.5)
        for name, obj in props.items():
            obj.hide_render = name != entry['mesh']
        render(NPC / 'Previews' / (entry['mesh'].lower() + '.png'))
