"""Render transparent review PNGs for the seven spirit models and NPC combinations."""

import json
import math
import os
import sys
import time
from pathlib import Path

import bpy
from mathutils import Vector


HERE = Path(__file__).resolve().parent
NPC = HERE.parent / 'ModularCollection'
mob_catalog = json.loads((HERE / 'mob_catalog.json').read_text(encoding='utf-8'))
npc_catalog = json.loads((NPC / 'catalog.json').read_text(encoding='utf-8'))
selection_filter = set(sys.argv[sys.argv.index('--select')+1].split(',')) if '--select' in sys.argv else set()


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
    scene.eevee.taa_render_samples = 48
    scene.view_settings.view_transform = 'Standard'
    scene.world.color = (.06, .07, .09)
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
        ('KEY', (-3, -4, 6), 420, (1.0, .93, .83)),
        ('FILL', (3, -2, 4), 100, (.68, .79, 1.0)),
        ('RIM', (1, 3, 5), 440, (.78, .90, 1.0)),
    ):
        data = bpy.data.lights.new(name, 'AREA')
        data.energy = energy
        data.shape = 'DISK'
        data.size = 2.8
        data.color = color
        light = bpy.data.objects.new(name, data)
        scene.collection.objects.link(light)
        light.location = loc
        light.rotation_euler = (target - light.location).to_track_quat('-Z', 'Y').to_euler()


def render(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.stem+'.render.png')
    bpy.context.scene.render.filepath = str(temporary)
    bpy.ops.render.render(write_still=True)
    for attempt in range(8):
        try:
            os.replace(temporary, path)
            break
        except PermissionError:
            if attempt == 7:
                raise
            time.sleep(.5)
    print('PREVIEW_READY', path, flush=True)


def fit_creature():
    """Use identical screen margins, regardless of each spirit's original modelling units."""
    scene = bpy.context.scene
    depsgraph = bpy.context.evaluated_depsgraph_get()
    points = [obj.matrix_world @ Vector(corner)
              for original in scene.objects if original.type == 'MESH' and not original.hide_render
              for obj in [original.evaluated_get(depsgraph)] for corner in obj.bound_box]
    rotation = scene.camera.rotation_euler.to_quaternion()
    right, up = rotation @ Vector((1, 0, 0)), rotation @ Vector((0, 1, 0))
    center = sum(points, Vector())/len(points)
    lo_x, hi_x = min(p.dot(right) for p in points), max(p.dot(right) for p in points)
    lo_y, hi_y = min(p.dot(up) for p in points), max(p.dot(up) for p in points)
    center += right*((lo_x+hi_x)/2-center.dot(right))
    center += up*((lo_y+hi_y)/2-center.dot(up))
    direction = Vector((4.9, -7.5, 3.7))
    scene.camera.location = center+direction
    scene.camera.data.ortho_scale = max(hi_y-lo_y, (hi_x-lo_x)*520/440)*1.17


def main():
    if '--npc-only' not in sys.argv and '--environment-only' not in sys.argv:
        for entry in mob_catalog['species']:
            if selection_filter and entry['key'] not in selection_filter:
                continue
            bpy.ops.wm.open_mainfile(filepath=str(HERE / entry['model']))
            rig = bpy.data.objects.get(entry['rig'])
            if rig and rig.animation_data:
                rig.animation_data.action = bpy.data.actions.get('MOB_'+entry['key'].upper()+'_IDLE')
            bpy.context.scene.frame_set(1)
            set_stage(1.45 if entry['gait'] == 'quadruped' else 1.65,
                      4.35 if entry['gait'] == 'quadruped' else 4.6)
            fit_creature()
            render(HERE / 'Previews' / (entry['key'] + '.png'))

    if '--environment-only' not in sys.argv and '--mobs-only' not in sys.argv:
        bpy.ops.wm.open_mainfile(filepath=str(NPC / 'npc_parts.blend'))
        set_stage(1.24, 3.20)
        parts = {obj.name: obj for obj in bpy.context.scene.objects if obj.type == 'MESH'}
        for entry in npc_catalog['npcs']:
            if selection_filter and entry['id'] not in selection_filter:
                continue
            selected = set()
            for slot, index in entry['partIndices'].items():
                if index >= 0:
                    selected.add(next(p['mesh'] for p in npc_catalog['parts'][slot] if p['index'] == index))
            for name, obj in parts.items():
                obj.hide_render = name not in selected
                obj.scale = (entry['scale'],)*3 if name in selected else (1, 1, 1)
            render(NPC / 'Previews' / (entry['id'] + '.png'))

    if '--npc-only' not in sys.argv and '--mobs-only' not in sys.argv:
        bpy.ops.wm.open_mainfile(filepath=str(NPC / 'world_props.blend'))
        props = {obj.name: obj for obj in bpy.context.scene.objects if obj.type == 'MESH'}
        for entry in npc_catalog['scenery']:
            if selection_filter and entry['mesh'] not in selection_filter:
                continue
            building = entry['slot'] == 'building'
            tree = 'TREE_' in entry['mesh']
            set_stage(1.55 if building or tree else .47,
                      5.8 if building else 4.8 if tree else 2.5)
            for name, obj in props.items():
                obj.hide_render = name != entry['mesh']
            render(NPC / 'Previews' / (entry['mesh'].lower() + '.png'))


if __name__ == '__main__':
    main()
