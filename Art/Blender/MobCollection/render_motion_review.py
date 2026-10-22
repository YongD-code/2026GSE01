"""Render actual keyed locomotion into temporary review frames, without game integration."""

import json
import sys
from pathlib import Path

import bpy

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import render_previews as review

ROOT = HERE.parents[2]
NPC = HERE.parent/'ModularCollection'
output = ROOT/'tmp/model_motion_review'
only = set(sys.argv[sys.argv.index('--select')+1].split(',')) if '--select' in sys.argv else set()


def frames(key, clip, end, npc=False):
    if only and key not in only:
        return
    if npc:
        catalog = json.loads((NPC/'catalog.json').read_text(encoding='utf-8'))
        entry = next(e for e in catalog['npcs'] if e['id'] == key)
        bpy.ops.wm.open_mainfile(filepath=str(NPC/'npc_rigged.blend'))
        names = {next(p['mesh'] for p in catalog['parts'][slot] if p['index'] == index)
                 for slot, index in entry['partIndices'].items() if index >= 0}
        for obj in bpy.context.scene.objects:
            if obj.type == 'MESH':
                obj.hide_render = obj.name not in names
        rig = bpy.data.objects['NPC_SHARED_RIG']
        action = bpy.data.actions['NPC_'+clip.upper()]
    else:
        catalog = json.loads((HERE/'mob_catalog.json').read_text(encoding='utf-8'))
        entry = next(e for e in catalog['species'] if e['key'] == key)
        bpy.ops.wm.open_mainfile(filepath=str(HERE/entry['model']))
        rig = bpy.data.objects[entry['rig']]
        action = bpy.data.actions['MOB_'+key.upper()+'_'+clip.upper()]
    rig.animation_data.action = action
    bpy.context.scene.frame_set(1)
    review.set_stage(1.25, 3.2)
    if not npc:
        review.fit_creature()
        bpy.context.scene.camera.data.ortho_scale *= 1.10
    scene = bpy.context.scene
    scene.render.resolution_x = 352
    scene.render.resolution_y = 416
    scene.eevee.taa_render_samples = 24
    folder = output/(key+'_'+clip)
    folder.mkdir(parents=True, exist_ok=True)
    for frame in range(1, end+1, 2):
        scene.frame_set(frame)
        review.render(folder/f'{frame:03d}.png')


if '--with-stills' in sys.argv:
    review.selection_filter = {'eater', 'stag', 'hound'}
    review.main()

frames('hunter', 'walk', 24, True)
frames('eater', 'run', 16)
frames('stag', 'walk', 24)
frames('hound', 'run', 16)
