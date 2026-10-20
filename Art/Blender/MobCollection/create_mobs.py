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
    'eater_skin': (.16, .17, .20, 1), 'eater_cloth': (.12, .14, .20, 1),
    'tear_blue': (.08, .55, .67, 1), 'eater_bone': (.62, .64, .60, 1),
    'stag_fur': (.35, .42, .34, 1), 'stag_light': (.59, .65, .47, 1),
    'stag_horn': (.55, .43, .25, 1), 'stag_glow': (.44, .79, .69, 1),
    'hound_hide': (.14, .13, .14, 1), 'hound_ash': (.30, .28, .28, 1),
    'hound_red': (.62, .11, .08, 1), 'hound_teeth': (.76, .68, .54, 1),
}
geom.COLORS.update(PALETTE)


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
    geom.ico((0, 0, 1.62), (.38, .27, .53), 'eater_skin', 2)
    geom.cone((0, .03, 1.19), .48, .33, .82, 'eater_cloth', 9)
    for x in (-.26, .26):
        geom.beam((x, -.27, 1.92), (0, -.29, 1.29), .025, 'tear_blue')
    bind('TORSO', 'body')
    geom.ico((0, -.09, 2.22), (.34, .32, .39), 'eater_bone', 2)
    geom.ico((0, -.38, 2.18), (.23, .045, .22), 'shadow')
    for x in (-.13, .13):
        geom.ico((x, -.405, 2.26), (.053, .025, .068), 'tear_blue')
        geom.beam((x, -.410, 2.22), (x*.85, -.417, 2.00), .016, 'tear_blue')
    for s in (-1, 1):
        geom.cone((s*.29, -.02, 2.56), .09, 0, .32, 'eater_bone', 5)
    bind('MASK_AND_TEARS', 'head')
    for side, s in (('L', -1), ('R', 1)):
        geom.ico((s*.49, -.02, 1.64), (.17, .17, .40), 'eater_cloth')
        bind('SLEEVE_'+side, 'arm.'+side)
        geom.beam((s*.60, -.10, 1.38), (s*.75, -.25, 1.00), .095, 'eater_skin')
        for k in range(3):
            geom.cone((s*(.68+k*.06), -.28, .95), .031, 0, .18, 'eater_bone', 5,
                      (math.pi/2, 0, 0))
        bind('CLAW_'+side, 'hand.'+side)
        geom.ico((s*.20, 0, .82), (.18, .18, .39), 'eater_cloth')
        bind('THIGH_'+side, 'leg.'+side)
        geom.ico((s*.22, -.05, .35), (.14, .16, .34), 'eater_skin')
        geom.ico((s*.22, -.19, .12), (.18, .25, .12), 'eater_bone')
        bind('SHIN_'+side, 'foot.'+side)
    for s in (-1, 1):
        geom.mesh('torn_cloth', [(s*.11, .20, 1.84), (s*.41, .20, 1.80),
                                 (s*.50, .27, .40), (s*.20, .29, .79)],
                  [(0, 1, 2, 3)], 'eater_cloth')
    bind('TORN_CLOAK', 'cloak')


def body_quad():
    gentle = species == 'stag'
    hide = 'stag_fur' if gentle else 'hound_hide'
    light = 'stag_light' if gentle else 'hound_ash'
    glow = 'stag_glow' if gentle else 'hound_red'
    geom.ico((0, 0, 1.18), (.57, .90, .47), hide, 2)
    geom.ico((0, -.55, 1.24), (.48, .38, .42), light)
    if not gentle:
        for x, y, z in ((-.28, .18, 1.56), (.25, -.38, 1.57),
                        (-.25, -.57, 1.52), (.20, .60, 1.50)):
            geom.cone((x, y, z), .16, 0, .35, light, 5)
    else:
        for x in (-.27, .27):
            geom.beam((x, -.35, 1.46), (x*.7, -.72, 1.20), .022, glow)
    bind('BODY', 'body')
    geom.ico((0, -1.00, 1.52), (.36, .39, .35), hide, 2)
    geom.ico((0, -1.30, 1.37), (.24, .30, .18), light)
    for x in (-.17, .17):
        geom.ico((x, -1.32, 1.60), (.045, .027, .048), glow)
    if gentle:
        geom.ico((0, -1.18, 1.45), (.085, .05, .07), 'stag_glow')
        for s in (-1, 1):
            geom.beam((s*.17, -.99, 1.78), (s*.36, -.95, 2.38), .045, 'stag_horn')
            geom.beam((s*.31, -.96, 2.12), (s*.58, -.96, 2.28), .030, 'stag_horn')
            geom.ico((s*.35, -.96, 2.37), (.08, .08, .08), 'stag_glow')
        geom.cube((0, -1.39, 1.15), (.13, .05, .25), 'paper')
    else:
        geom.ico((0, -1.50, 1.36), (.27, .15, .12), 'shadow')
        for x in (-.17, -.08, 0, .08, .17):
            geom.cone((x, -1.55, 1.35), .035, 0, .12, 'hound_teeth', 5,
                      (math.pi/2, 0, 0))
        geom.beam((-.20, -1.31, 1.49), (.20, -1.31, 1.49), .019, 'hound_red')
    bind('HEAD', 'head')
    for side, s in (('L', -1), ('R', 1)):
        geom.ico((s*.30, -1.01, 1.87), (.13, .16, .25), hide)
        bind('EAR_'+side, 'ear.'+side)
        for which, y in (('front', -.62), ('back', .58)):
            geom.ico((s*.39, y, .75), (.17, .19, .39), hide)
            bind(which.upper()+'_LEG_'+side, which+'.'+side)
            geom.ico((s*.39, y-.08, .31), (.13, .14, .29), light)
            geom.ico((s*.39, y-.19, .13), (.17, .23, .12),
                     'stag_horn' if gentle else 'hound_hide')
            bind(which.upper()+'_PAW_'+side, 'paw.'+which+'.'+side)
    if gentle:
        geom.cone((0, 1.10, 1.45), .16, 0, .72, light, 7,
                  (math.pi/3, 0, 0))
    else:
        for x in (-.17, .0, .17):
            geom.cone((x, 1.14, 1.38), .14, 0, .65, hide, 5,
                      (math.pi/3, 0, 0))
    bind('TAIL', 'tail')


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
for kind, end in clips.items():
    rig.animation_data_create()
    rig.animation_data.action = None
    for frame in range(1, end+2):
        pose(kind, (frame-1)/end)
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

bpy.ops.wm.save_as_mainfile(filepath=str(destination/'model.blend'))
bpy.ops.object.select_all(action='DESELECT')
rig.select_set(True)
for obj in bpy.context.scene.objects:
    if obj.type == 'MESH':
        obj.select_set(True)
bpy.ops.export_scene.gltf(filepath=str(destination/'model.glb'),
                          export_format='GLB', use_selection=True,
                          export_animations=True, export_animation_mode='ACTIONS',
                          export_extras=True)
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
         'turnStepDegrees': 45}
catalog['species'] = [e for e in catalog['species'] if e['id'] != species_id] + [entry]
catalog['species'].sort(key=lambda e: e['id'])
catalog_path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print('MOB_RIG_READY', species, entry['meshCount'], entry['boneCount'], len(clips), flush=True)
