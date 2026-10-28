"""Extend four existing spirit rigs and make three new keyed spirit models.

Blender 5.2: --background --factory-startup --python-exit-code 1
             --python Art/Blender/MobCollection/create_mobs.py -- --species forgotten
Species: forgotten, pilgrim, hands, saint, eater, stag, hound.
"""

import hashlib
import json
import math
import sys
from pathlib import Path

import bpy


HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / 'Art/Blender/ModularCollection'))
import create_collection as geom

args = sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
species = args[args.index('--species')+1] if '--species' in args else 'forgotten'
INFO = {
    'forgotten': (0, '남겨진 자', 'gentle', 'hover', ROOT/'Art/Blender/SpiritStudy/forgotten_candidate.blend'),
    'pilgrim': (1, '목 없는 순례자', 'ambivalent', 'hover', ROOT/'Art/Blender/SpiritCollection/pilgrim/model.blend'),
    'hands': (2, '기도하는 손', 'gentle', 'quadruped', ROOT/'Art/Blender/SpiritCollection/hands/model.blend'),
    'saint': (3, '기도하지 않는 성자', 'dangerous', 'hover', ROOT/'Art/Blender/SpiritCollection/saint/model.blend'),
    'eater': (4, '울음 먹는 자', 'dangerous', 'biped', None),
    'stag': (5, '길지기 사슴', 'gentle', 'quadruped', None),
    'hound': (6, '재 먹는 들개', 'dangerous', 'quadruped', None),
}
if species not in INFO:
    raise ValueError('Unknown species: ' + species)
species_id, label, disposition, gait, source = INFO[species]
destination = HERE / species
destination.mkdir(parents=True, exist_ok=True)
bpy.context.preferences.filepaths.save_version = 0
source_hash = hashlib.sha256(source.read_bytes()).hexdigest() if source else None

PALETTE = {
    'eater_skin': (.075, .09, .11, 1), 'eater_cloth': (.025, .037, .065, 1),
    'eater_edge': (.12, .15, .18, 1),
    'tear_blue': (.015, .36, .47, 1), 'eater_bone': (.59, .53, .42, 1),
    'stag_fur': (.15, .205, .18, 1), 'stag_light': (.46, .45, .34, 1),
    'stag_horn': (.19, .13, .075, 1), 'stag_glow': (.12, .49, .38, 1),
    'stag_cloth': (.035, .11, .10, 1),
    'hound_hide': (.027, .025, .030, 1), 'hound_ash': (.12, .105, .115, 1),
    'hound_red': (.50, .055, .018, 1), 'hound_teeth': (.48, .40, .28, 1),
}
geom.COLORS.update(PALETTE)
import spirit_shapes
import surface_forms


def add_bone(arm, name, head, tail, parent=None):
    bone = arm.edit_bones.new(name)
    bone.head, bone.tail = head, tail
    if parent:
        bone.parent = arm.edit_bones[parent]


def make_rig():
    arm = bpy.data.armatures.new('MOB_' + species.upper() + '_SKELETON')
    obj = bpy.data.objects.new('MOB_' + species.upper() + '_RIG', arm)
    bpy.context.scene.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    add_bone(arm, 'root', (0, 0, 0), (0, 0, .20))
    if gait == 'biped':
        add_bone(arm, 'body', (0, 0, 1.0), (0, 0, 1.98), 'root')
        add_bone(arm, 'head', (0, -.08, 2.08), (0, -.1, 2.55), 'body')
        for side, s in (('L', -1), ('R', 1)):
            add_bone(arm, 'arm.'+side, (s*.38, 0, 1.90), (s*.57, -.08, 1.39), 'body')
            add_bone(arm, 'hand.'+side, (s*.57, -.08, 1.39), (s*.72, -.20, .98), 'arm.'+side)
            add_bone(arm, 'leg.'+side, (s*.19, 0, 1.05), (s*.22, -.02, .56), 'body')
            add_bone(arm, 'foot.'+side, (s*.22, -.02, .56), (s*.22, -.14, .12), 'leg.'+side)
        add_bone(arm, 'cloak', (0, .25, 1.74), (0, .30, .65), 'body')
    else:
        add_bone(arm, 'body', (0, 0, 1.13), (0, -.40, 1.18), 'root')
        add_bone(arm, 'head', (0, -.78, 1.44), (0, -1.20, 1.48), 'body')
        for side, s in (('L', -1), ('R', 1)):
            for which, y in (('front', -.62), ('back', .58)):
                add_bone(arm, which+'.'+side, (s*.38, y, 1.05), (s*.39, y, .55), 'body')
                add_bone(arm, 'paw.'+which+'.'+side,
                         (s*.39, y, .55), (s*.39, y-.09, .12), which+'.'+side)
        add_bone(arm, 'tail', (0, .80, 1.20), (0, 1.40, 1.48), 'body')
        add_bone(arm, 'ear.L', (-.19, -1.06, 1.66), (-.34, -1.0, 2.02), 'head')
        add_bone(arm, 'ear.R', (.19, -1.06, 1.66), (.34, -1.0, 2.02), 'head')
    bpy.ops.object.mode_set(mode='OBJECT')
    obj.show_in_front = True
    for bone in obj.pose.bones:
        bone.rotation_mode = 'XYZ'
    return obj


def bind(name, bone):
    obj = geom.finish('MOB_'+species.upper()+'_'+name, 'mob', len(parts), parts)
    group = obj.vertex_groups.new(name=bone)
    group.add([v.index for v in obj.data.vertices], 1, 'REPLACE')
    modifier = obj.modifiers.new('MOB_SKIN', 'ARMATURE')
    modifier.object = rig
    obj.parent = rig
    return obj


def body_biped():
    spirit_shapes.eater(bind)


def body_quad():
    spirit_shapes.quadruped(species, bind)


parts = []
if source:
    bpy.ops.wm.open_mainfile(filepath=str(source))
    rigs = [o for o in bpy.data.objects if o.type == 'ARMATURE']
    if len(rigs) != 1:
        raise RuntimeError('Expected one source rig')
    rig = rigs[0]
    for bone in rig.pose.bones:
        bone.rotation_mode = 'XYZ'
else:
    geom.start_collection('MOB_'+species.upper())
    rig = make_rig()
    if gait == 'biped':
        body_biped()
    else:
        body_quad()


def clear_pose():
    for bone in rig.pose.bones:
        bone.location = (0, 0, 0)
        bone.rotation_euler = (0, 0, 0)
        bone.scale = (1, 1, 1)


def set_bone(name, axis, value):
    bone = rig.pose.bones.get(name)
    if bone:
        bone.rotation_euler[axis] = value


def animate_hover(kind, t):
    a = math.tau*t
    root = rig.pose.bones['root']
    body = rig.pose.bones['body']
    amplitude = {'idle': .045, 'walk': .07, 'run': .12}.get(kind, 0)
    if kind in ('idle', 'walk', 'run'):
        root.location.y = amplitude * math.sin(a)
        body.rotation_euler.x = (.035 if kind == 'run' else .015) * math.sin(a)
        body.rotation_euler.z = .025 * math.sin(a)
    if kind == 'jump':
        root.location.y = .72*math.sin(math.pi*t)**2
        body.rotation_euler.x = -.18*math.sin(math.pi*t)
    for side, offset in (('L', 0), ('R', math.pi)):
        swing = math.sin(a+offset)
        set_bone('arm.'+side, 0, (.17 if kind == 'run' else .08)*swing)
        set_bone('hand.'+side, 0, .07*swing)
    for i in range(4):
        set_bone('cloth.'+str(i), 0, .06*math.sin(a+i*.7))
        set_bone('tip.'+str(i), 0, .11*math.sin(a+i*.7-.55))
    set_bone('mask', 2, -.035*math.sin(a))
    set_bone('flame', 0, .06*math.sin(a))
    set_bone('head', 2, -.035*math.sin(a))
    if kind == 'attack':
        strike = math.sin(math.pi*t)**2
        for side in ('L', 'R'):
            set_bone('arm.'+side, 0, -.45*strike)
            set_bone('hand.'+side, 0, -.30*strike)


def animate_quad(kind, t):
    a = math.tau*t
    root = rig.pose.bones['root']
    moving = kind in ('walk', 'run')
    fast = kind == 'run'
    if moving:
        root.location.y = (.10 if fast else .035) * (1-math.cos(2*a))/2
        set_bone('body', 0, (.075 if fast else .025)*math.sin(a))
        set_bone('head', 0, -.04*math.sin(a))
        for which in ('front', 'back'):
            for side in ('L', 'R'):
                # Walk uses diagonal pairs; run uses a bound with paired fore/hind contacts.
                offset = (0 if (which == 'front') == (side == 'L') else math.pi)
                if fast:
                    offset = 0 if which == 'front' else math.pi
                phase = a+offset
                lift = max(0, math.sin(phase))
                set_bone(which+'.'+side, 0, (.40 if fast else .25)*math.cos(phase))
                set_bone('paw.'+which+'.'+side, 0, (.56 if fast else .31)*lift)
    elif kind == 'idle':
        root.location.y = .015*math.sin(a)
        set_bone('head', 0, .035*math.sin(a))
    elif kind == 'jump':
        lift = math.sin(math.pi*t)**2
        root.location.y = .70*lift
        for which in ('front', 'back'):
            for side in ('L', 'R'):
                set_bone(which+'.'+side, 0, (-.36 if which == 'front' else .26)*lift)
                set_bone('paw.'+which+'.'+side, 0, .46*lift)
    elif kind == 'attack':
        set_bone('head', 0, -.34*math.sin(math.pi*t)**2)
        root.location.z = -.12*math.sin(math.pi*t)**2
    set_bone('tail', 2, .18*math.sin(a-.35))
    for i in range(3):
        set_bone('tail.'+str(i), 2, .12*math.sin(a+i*.55))
        set_bone('tip.'+str(i), 2, .18*math.sin(a+i*.55-.45))
    for side in ('L', 'R'):
        set_bone('ear.'+side, 0, .08*math.sin(a))


def animate_biped(kind, t):
    a = math.tau*t
    root = rig.pose.bones['root']
    if kind in ('walk', 'run'):
        fast = kind == 'run'
        root.location.y = (.08 if fast else .025)*(1-math.cos(2*a))/2
        set_bone('body', 0, (.12 if fast else .04)*math.sin(a))
        for side, offset in (('L', 0), ('R', math.pi)):
            phase = a+offset
            set_bone('leg.'+side, 0, (.48 if fast else .28)*math.cos(phase))
            set_bone('foot.'+side, 0, (.62 if fast else .34)*max(0, math.sin(phase)))
            set_bone('arm.'+side, 0, -(.40 if fast else .24)*math.cos(phase))
            set_bone('hand.'+side, 0, .12*math.sin(phase))
    elif kind == 'idle':
        root.location.y = .012*math.sin(a)
        set_bone('head', 0, .025*math.sin(a))
    elif kind == 'jump':
        lift = math.sin(math.pi*t)**2
        root.location.y = .74*lift
        for side in ('L', 'R'):
            set_bone('leg.'+side, 0, .42*lift)
            set_bone('foot.'+side, 0, .53*lift)
            set_bone('arm.'+side, 0, -.32*lift)
    elif kind == 'attack':
        strike = math.sin(math.pi*t)**2
        set_bone('body', 0, -.28*strike)
        for side in ('L', 'R'):
            set_bone('arm.'+side, 0, -.55*strike)
            set_bone('hand.'+side, 0, -.36*strike)
    set_bone('cloak', 0, .07*math.sin(a-.5))


def pose(kind, t):
    clear_pose()
    if kind.startswith('turn_'):
        direction = 1 if kind == 'turn_left' else -1
        s = t*t*(3-2*t)
        rig.pose.bones['root'].rotation_euler.y = direction*math.pi/4*s
        set_bone('head', 1, direction*.12*math.sin(math.pi*t))
        set_bone('mask', 1, direction*.12*math.sin(math.pi*t))
    elif gait == 'hover':
        animate_hover(kind, t)
    elif gait == 'quadruped':
        animate_quad(kind, t)
    else:
        animate_biped(kind, t)


clips = {'idle': 32, 'walk': 24, 'run': 16, 'jump': 24,
         'turn_left': 12, 'turn_right': 12}
if source is None:
    clips['attack'] = 18
feet = [o for o in bpy.context.scene.objects if o.type == 'MESH'
        and ('_SHIN_' in o.name or '_PAW_' in o.name)] if source is None else []


def keep_feet_above_floor():
    if not feet:
        return
    bpy.context.view_layer.update()
    graph = bpy.context.evaluated_depsgraph_get()
    lowest = min((evaluated.matrix_world @ v.co).z for obj in feet
                 for evaluated in [obj.evaluated_get(graph)] for v in evaluated.data.vertices)
    if lowest < .01:
        rig.pose.bones['root'].location.y += .01-lowest


for kind, end in clips.items():
    rig.animation_data_create()
    rig.animation_data.action = None
    for frame in range(1, end+2):
        pose(kind, (frame-1)/end)
        keep_feet_above_floor()
        for bone in rig.pose.bones:
            bone.keyframe_insert('location', frame=frame, group=bone.name)
            bone.keyframe_insert('rotation_euler', frame=frame, group=bone.name)
    action = rig.animation_data.action
    action.name = 'MOB_'+species.upper()+'_'+kind.upper()
    action.use_fake_user = True
    action['clip'] = kind
    action['loop'] = kind in ('idle', 'walk', 'run')
    action['fps'] = 24
rig.animation_data.action = None
clear_pose()

locomotion = None
if gait == 'quadruped':
    import quadruped_locomotion
    locomotion = quadruped_locomotion.apply_rig(rig, species)

if species == 'stag':
    import stag_q
    stag_q.author(rig)

surface_forms.stylize_scene()
bpy.ops.wm.save_as_mainfile(filepath=str(destination/'model.blend'))
bpy.ops.object.select_all(action='DESELECT')
rig.select_set(True)
for obj in bpy.context.scene.objects:
    if obj.type == 'MESH':
        obj.select_set(True)
export_temporary = destination/'model_export.glb'
with surface_forms.export_materials():
    bpy.ops.export_scene.gltf(filepath=str(export_temporary),
                              export_format='GLB', use_selection=True,
                              export_animations=True, export_animation_mode='ACTIONS',
                              export_extras=True)
export_temporary.replace(destination/'model.glb')
if source and hashlib.sha256(source.read_bytes()).hexdigest() != source_hash:
    raise RuntimeError('Protected source model changed')
catalog_path = HERE/'mob_catalog.json'
catalog = json.loads(catalog_path.read_text(encoding='utf-8')) if catalog_path.exists() else {
    'schemaVersion': 1, 'animationFps': 24, 'species': []}
entry = {'id': species_id, 'key': species, 'name': label, 'disposition': disposition,
         'gait': gait, 'model': species+'/model.blend', 'export': species+'/model.glb',
         'rig': rig.name, 'source': str(source.relative_to(ROOT)).replace('\\','/') if source else None,
         'sourceSha256': source_hash, 'meshCount': len([o for o in bpy.context.scene.objects if o.type=='MESH']),
         'boneCount': len(rig.data.bones),
         'clips': {name: {'firstFrame': 1, 'lastFrame': end+1,
                          'loop': name in ('idle', 'walk', 'run')}
                   for name, end in clips.items()},
         'turnStepDegrees': 45,
         'materialExport': 'PBR base colors; four-band lighting is authored in the blend source',
         'triangles': sum(len(p.vertices)-2 for o in bpy.context.scene.objects if o.type == 'MESH' for p in o.data.polygons)}
if locomotion:
    quadruped_locomotion.update_entry(entry, rig, locomotion)
if species == 'stag':
    entry['clips'].update(stag_q.metadata())
    entry['skills'] = {'Q': {'name': '길틔움', 'animation': 'MOB_STAG_ATTACK',
                              'timing': stag_q.specification()}}
catalog['species'] = [e for e in catalog['species'] if e['id'] != species_id] + [entry]
catalog['species'].sort(key=lambda e: e['id'])
catalog_path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print('MOB_RIG_READY', species, entry['meshCount'], entry['boneCount'], len(clips), flush=True)
