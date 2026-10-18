"""Editable gatherer rig and 8-direction PNG locomotion for the existing NPC Actor.

Run: blender --background --factory-startup --python this_file
Use -- --source-only to save the rig without exporting images.
Use -- --preview-only to assemble four existing idle views without rendering again.
"""
import ast
import json
import math
import sys
from array import array
from pathlib import Path

import bpy
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
OUT = ROOT / 'SimpleGame/Assets/Characters/Townsfolk/Gatherer'
SIZE, STEPS = 128, 32


def write_preview():
    source_image = bpy.data.images.load(str(OUT / 'walk.png'), check_existing=False)
    width, height = source_image.size
    pixels = array('f', [0]) * (width * height * 4)
    source_image.pixels.foreach_get(pixels)
    result = array('f', [0]) * (SIZE * SIZE * 4 * 4)
    for column, row in enumerate([0, 2, 4, 6]):
        for y in range(SIZE):
            start = (((7 - row) * SIZE + y) * width) * 4
            dest = (y * SIZE * 4 + column * SIZE) * 4
            result[dest:dest + SIZE * 4] = pixels[start:start + SIZE * 4]
    image = bpy.data.images.new('채집꾼 앞·옆·뒤', width=SIZE * 4, height=SIZE, alpha=True)
    image.pixels.foreach_set(result)
    image.filepath_raw = str(HERE / 'gatherer_preview.png')
    image.file_format = 'PNG'
    image.save()
    bpy.data.images.remove(image)
    bpy.data.images.remove(source_image)


if '--preview-only' in sys.argv:
    write_preview()
    sys.exit(0)

scene = bpy.context.scene
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.context.preferences.filepaths.save_version = 0

# Reuse only mesh-authoring functions, never the original script's render/main code.
source = (ROOT / 'Art/Blender/create_protagonist.py').read_text(encoding='utf-8')
helpers = {'material', 'bone', 'bind', 'ellipsoid', 'surface', 'loft', 'blend_height', 'seam'}
nodes = [node for node in ast.parse(source).body
         if isinstance(node, ast.FunctionDef) and node.name in helpers]
rig_data = bpy.data.armatures.new('채집꾼 뼈대')
rig = bpy.data.objects.new('채집꾼_리그', rig_data)
scene.collection.objects.link(rig)
exec(compile(ast.Module(body=nodes, type_ignores=[]), '<mesh helpers>', 'exec'), globals())
bpy.context.view_layer.objects.active = rig
rig.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
bone('root', (0, 0, 0), (0, 0, .25))
bone('hips', (0, 0, 1.02), (0, 0, 1.28), 'root')
bone('chest', (0, 0, 1.28), (0, 0, 1.68), 'hips')
bone('head', (0, 0, 1.68), (0, 0, 2.1), 'chest')
for side, x in [('L', -.17), ('R', .17)]:
    bone('thigh.' + side, (x, 0, 1.05), (x, 0, .59), 'hips')
    bone('shin.' + side, (x, 0, .59), (x, 0, .18), 'thigh.' + side)
    bone('foot.' + side, (x, 0, .18), (x, -.23, .12), 'shin.' + side)
    bone('arm.' + side, (x * 2.1, 0, 1.59), (x * 2.1, 0, 1.22), 'chest')
    bone('forearm.' + side, (x * 2.1, 0, 1.22), (x * 2.1, 0, .91), 'arm.' + side)
bpy.ops.object.mode_set(mode='OBJECT')
rig.select_set(False)
rig.show_in_front = True
for joint in rig.pose.bones:
    joint.rotation_mode = 'XYZ'

cloth = material('苔色の作務衣', (.14, .22, .16))
edge = material('擦れた布端', (.28, .33, .21))
trousers = material('墨藍の股引', (.10, .13, .17))
linen = material('生成りの襟と手甲', (.64, .57, .41))
skin = material('日焼けした肌', (.60, .37, .23))
skin_light = material('頬の明るい面', (.70, .46, .30))
hair = material('焦げ茶の髪', (.055, .037, .027))
hair_light = material('髪の流れ', (.12, .075, .043))
leather = material('革と草履の底', (.12, .075, .044))
straw = material('背負い籠', (.38, .25, .105))
straw_light = material('籠の編み目', (.55, .39, .19))
leaf = material('採れた薬草', (.13, .26, .065))
leaf_light = material('若葉', (.27, .40, .10))
eye = material('目と眉', (.022, .018, .015))

# Continuous cross-wrapped jacket with thickness in profile, not a flat front card.
body = loft('作務衣の身頃', [(0, .015, 1.04, .245, .17),
            (0, .015, 1.15, .26, .185), (0, .01, 1.31, .29, .20),
            (0, .01, 1.47, .31, .205), (0, 0, 1.57, .325, .19),
            (0, 0, 1.64, .25, .15), (0, 0, 1.68, .115, .10)],
            cloth, 'chest', count=24, folds=.022)
blend_height(body, 'hips', 'chest', 1.27, .24)
loft('腰帯', [(0, 0, 1.10, .27, .195), (0, 0, 1.17, .267, .19)],
     linen, 'hips', count=24)
for points in [[(-.10, -.10, 1.69), (.015, -.207, 1.49), (.21, -.158, 1.22)],
               [(.11, -.10, 1.69), (-.018, -.21, 1.47), (-.12, -.19, 1.33)]]:
    seam('重ねた半襟', points, .023, linen, 'chest')
seam('帯の結び目', [(-.055, -.204, 1.15), (0, -.235, 1.12), (.065, -.207, 1.14)],
     .022, straw_light, 'hips')
for sign in [-1, 1]:
    seam('帯の垂れ', [(sign * .025, -.218, 1.13), (sign * .07, -.222, 1.02),
                     (sign * .05, -.20, .95)], .014, linen, 'hips')
for i in range(6):
    x = -.21 + i * .084
    seam('裾の柔らかな縫い目', [(x, -.16, 1.08), (x * 1.08, -.17, 1.21),
                              (x * 1.12, -.18, 1.32)], .0035, edge, 'hips')
loft('首', [(0, 0, 1.63, .09, .09), (0, 0, 1.82, .10, .10)], skin, 'head', count=20)
loft('顎から頬へ続く顔', [(0, -.058, 1.765, .07, .07),
     (0, -.045, 1.80, .13, .12), (0, -.01, 1.88, .195, .169),
     (0, .012, 1.97, .218, .188), (0, .019, 2.065, .215, .18),
     (0, .03, 2.14, .16, .14), (0, .03, 2.17, .055, .055)],
     skin, 'head', count=32)
loft('鼻筋', [(0, -.17, 1.87, .025, .025), (0, -.218, 1.90, .032, .037),
     (0, -.19, 1.98, .018, .02)], skin_light, 'head', count=16)
seam('口元', [(-.052, -.181, 1.84), (0, -.196, 1.833), (.048, -.18, 1.842)],
     .0055, leather, 'head')
for sign in [-1, 1]:
    ellipsoid('耳', (sign * .215, .002, 1.955), (.041, .033, .068), skin, 'head', 16)
    ellipsoid('耳の内側', (sign * .237, -.013, 1.955), (.015, .018, .036), leather, 'head', 12)
    x = sign * .083
    seam('落ち着いた目', [(x - .038, -.178, 1.972), (x, -.193, 1.98),
                         (x + .034, -.18, 1.974)], .008, eye, 'head')
    seam('眉', [(x - .039, -.165, 2.017), (x, -.186, 2.025),
               (x + .035, -.165, 2.017)], .009, hair, 'head')
    ellipsoid('頬', (sign * .137, -.139, 1.91), (.055, .025, .032), skin_light, 'head', 16)
ellipsoid('髪の丸み', (0, .04, 2.084), (.224, .194, .115), hair, 'head', 24)
ellipsoid('後ろで束ねた髪', (0, .207, 2.09), (.085, .077, .075), hair, 'head', 20)
for i in range(9):
    x = -.17 + i * .041
    seam('髪の筋', [(x * .65, .10, 2.174), (x, -.015, 2.186),
                   (x * 1.10, -.145, 2.103)], .012, hair_light if i % 3 == 0 else hair, 'head')
loft('鉢巻', [(0, .015, 2.048, .226, .194), (0, .02, 2.085, .223, .19)],
     linen, 'head', count=32)
for sign in [-1, 1]:
    seam('鉢巻の結び', [(0, .217, 2.07), (sign * .05, .245, 2.04),
                      (sign * .06, .25, 1.95)], .016, linen, 'head')

for side, x in [('L', -.17), ('R', .17)]:
    leg = loft('膝まで続く脚 ' + side, [(x, 0, .21, .077, .09),
               (x, 0, .37, .10, .10), (x, .008, .55, .12, .13),
               (x, 0, .68, .145, .14), (x, 0, .88, .15, .155),
               (x * .86, 0, 1.10, .145, .145)], trousers,
               'thigh.' + side, count=20, folds=.06)
    blend_height(leg, 'shin.' + side, 'thigh.' + side, .59, .18)
    loft('脚絆 ' + side, [(x, 0, .18, .083, .096), (x, 0, .27, .086, .10),
         (x, 0, .38, .095, .105)], linen, 'shin.' + side, count=20)
    for z in [.23, .28, .33]:
        seam('脚絆の紐', [(x - .07, -.06, z), (x, -.106, z + .012),
                        (x + .07, -.06, z + .018)], .006, straw, 'shin.' + side)
    loft('足袋 ' + side, [(x, -.09, .025, .09, .19), (x, -.09, .065, .10, .20),
         (x, -.075, .115, .09, .175), (x, 0, .205, .075, .085)],
         linen, 'foot.' + side, count=24)
    loft('草履の底 ' + side, [(x, -.095, .018, .102, .207),
         (x, -.095, .045, .105, .21)], leather, 'foot.' + side, count=24)
    for sign in [-1, 1]:
        seam('草履の鼻緒', [(x + sign * .087, -.04, .075),
                          (x, -.15, .127), (x, -.225, .105)], .013, straw_light, 'foot.' + side)
    ax = x * 2.1
    sleeve = loft('ゆったりした袖 ' + side, [(ax, 0, 1.02, .085, .10),
                  (ax, .005, 1.13, .105, .118), (ax, .008, 1.25, .135, .14),
                  (ax, 0, 1.43, .15, .145), (ax * .92, 0, 1.58, .135, .13)],
                  cloth, 'arm.' + side, count=20, folds=.035)
    blend_height(sleeve, 'forearm.' + side, 'arm.' + side, 1.22, .20)
    loft('折り返した袖口 ' + side, [(ax, 0, 1.014, .092, .108),
         (ax, 0, 1.065, .098, .112)], edge, 'forearm.' + side, count=20)
    loft('手甲 ' + side, [(ax, 0, .93, .064, .059), (ax, 0, 1.015, .075, .078)],
         linen, 'forearm.' + side, count=16)
    ellipsoid('手 ' + side, (ax, -.016, .897), (.059, .048, .077), skin,
              'forearm.' + side, 20)
    ellipsoid('親指 ' + side, (ax + (.053 if x < 0 else -.053), -.025, .91),
              (.026, .03, .044), skin_light, 'forearm.' + side, 12)

# Back basket has its own silhouette, woven rim, shoulder straps and gathered plants.
loft('背負い籠の本体', [(0, .29, 1.04, .20, .11), (0, .31, 1.12, .235, .135),
     (0, .32, 1.37, .25, .15), (0, .32, 1.65, .27, .17)], straw, 'chest', count=24)
for j in range(8):
    z = 1.08 + j * .079
    radius = .215 + (z - 1.08) * .095
    seam('籠の横編み', [(math.sin(a) * radius, .315 - math.cos(a) * (radius * .60), z)
                       for a in [i * math.tau / 24 for i in range(25)]],
         .008, straw_light, 'chest')
for i in range(20):
    a = math.tau * i / 20
    seam('籠の縦編み', [(math.sin(a) * r, .315 - math.cos(a) * r * .60, z)
                       for r, z in [(.20, 1.04), (.24, 1.34), (.273, 1.66)]],
         .009, straw_light, 'chest')
for sign in [-1, 1]:
    seam('肩の縄', [(sign * .22, .29, 1.63), (sign * .23, -.008, 1.64),
                   (sign * .24, -.19, 1.39), (sign * .21, .12, 1.13)],
         .026, straw_light, 'chest')
for i in range(11):
    a = i * 2.399963
    cx, cy = math.sin(a) * .16, .32 + math.cos(a) * .08
    top = 1.78 + .10 * (i % 3)
    seam('薬草の茎', [(cx, cy, 1.60), (cx * 1.2, cy, top)], .006, leaf, 'chest')
    for sign in [-1, 1]:
        ob = ellipsoid('籠からのぞく葉', (cx + sign * .045, cy, top - .04),
                       (.027, .022, .105), leaf_light if i % 3 == 0 else leaf, 'chest', 12)
        # Geometry is already bound; rotate the leaf in edit geometry around its own center.
        center = Vector((cx + sign * .045, cy, top - .04))
        from mathutils import Matrix
        matrix = Matrix.Rotation(sign * .55, 4, 'Y')
        for vertex in ob.data.vertices:
            vertex.co = center + matrix @ (vertex.co - center)

sys.path.insert(0, str(ROOT / 'Art/Blender/CharacterRefinement'))
from locomotion import apply_locomotion


def pose(kind, t):
    for joint in rig.pose.bones:
        joint.location = (0, 0, 0)
        joint.rotation_euler = (0, 0, 0)
    if kind == 'walk':
        # 0.447 source units match a 44-world-unit cycle at the 67-unit display height.
        apply_locomotion(rig, kind, t, walk_stride=.447)
        # Carrying a basket: a small lean and restrained arm swing.
        rig.pose.bones['chest'].rotation_euler.x = .075
        for side, offset in [('L', 0), ('R', .5)]:
            rig.pose.bones['arm.' + side].rotation_euler.x *= .65
            rig.pose.bones['forearm.' + side].rotation_euler.x = -.22
    else:
        rig.pose.bones['hips'].location.y = -.025
        rig.pose.bones['chest'].rotation_euler.x = .025 + .006 * math.sin(t * math.tau)
        rig.pose.bones['forearm.L'].rotation_euler.x = -.16
        rig.pose.bones['forearm.R'].rotation_euler.x = -.16
    bpy.context.view_layer.update()


# Store editable, looping animation actions; source remains useful beyond this atlas.
for kind in ['idle', 'walk']:
    rig.animation_data_create()
    rig.animation_data.action = None
    for frame in range(STEPS + 1):
        pose(kind, frame / STEPS)
        for joint in rig.pose.bones:
            joint.keyframe_insert('location', frame=frame + 1, group=joint.name)
            joint.keyframe_insert('rotation_euler', frame=frame + 1, group=joint.name)
    rig.animation_data.action.name = '채집꾼 대기' if kind == 'idle' else '채집꾼 걷기'
    rig.animation_data.action.use_fake_user = True
rig.animation_data.action = None
pose('idle', 0)

scene.render.engine = 'BLENDER_WORKBENCH'
scene.render.resolution_x = scene.render.resolution_y = SIZE
scene.render.resolution_percentage = 100
scene.render.film_transparent = True
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'
scene.display.render_aa = '16'
scene.display.shading.light = 'STUDIO'
scene.display.shading.studiolight_rotate_z = .5
scene.display.shading.color_type = 'MATERIAL'
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.display.shading.cavity_type = 'BOTH'
scene.display.shading.curvature_ridge_factor = 1.1
scene.display.shading.curvature_valley_factor = 1.0
scene.display.shading.show_object_outline = False
scene.display.shading.show_specular_highlight = False
scene.view_settings.view_transform = 'Standard'
bpy.ops.object.camera_add()
camera = bpy.context.object
camera.name = '마을 주민 직교 카메라'
target = Vector((0, 0, 1.22))
direction = Vector((1, 1, .83)).normalized()
camera.location = target + direction * 8
camera.rotation_euler = (target - camera.location).to_track_quat('-Z', 'Y').to_euler()
camera.data.type = 'ORTHO'
camera.data.ortho_scale = 3.1
scene.camera = camera
scene.frame_start, scene.frame_end = 1, STEPS
rig.rotation_euler.z = math.pi * .75
bpy.context.view_layer.update()
origin = world_to_camera_view(scene, camera, Vector((0, 0, 0)))
head = world_to_camera_view(scene, camera, Vector((0, 0, 2.20)))
anchor = 1 - origin.y
height = head.y - origin.y
HERE.mkdir(parents=True, exist_ok=True)
OUT.mkdir(parents=True, exist_ok=True)
scene['사용법'] = '채집꾼 전용. 기존 주인공은 변경하지 않음. 8방향, 고정 원점, 32보행 포즈.'
scene['지면 원점'] = [0., 0., 0.]
rig['설명'] = '몸통과 팔다리의 연속 메시, 피부/의복 가중치, 지지발/유각기 보행.'
bpy.context.view_layer.objects.active = rig
bpy.ops.wm.save_as_mainfile(filepath=str(HERE / 'gatherer.blend'))
if '--source-only' in sys.argv:
    sys.exit(0)

columns = STEPS + 1
width, image_height = SIZE * columns, SIZE * 8
atlas = array('f', [0]) * (width * image_height * 4)
temp = OUT / 'frame.png'
yaws = []
for row in range(8):
    # Rows retain the original world-heading order. This camera's screen-right axis
    # is (-X,+Y), whereas the game's Screen uses (+X,-Y). The layout below reflects
    # the rows when converting game facings to these camera views; pixels stay unchanged.
    screen_angle = row * math.pi / 4
    sx, sy = -math.sin(screen_angle), math.cos(screen_angle)
    yaw = math.atan2((-sx / .85 + sy / .43) * .5, (sx / .85 + sy / .43) * .5) + math.pi / 2
    yaws.append(yaw)
    rig.rotation_euler.z = yaw
    for col in range(columns):
        pose('idle' if col == 0 else 'walk', 0 if col == 0 else (col - 1) / STEPS)
        scene.render.filepath = str(temp)
        bpy.ops.render.render(write_still=True)
        rendered = bpy.data.images.load(str(temp), check_existing=False)
        pixels = array('f', [0]) * (SIZE * SIZE * 4)
        rendered.pixels.foreach_get(pixels)
        for y in range(SIZE):
            destination = (((7 - row) * SIZE + y) * width + col * SIZE) * 4
            atlas[destination:destination + SIZE * 4] = pixels[y * SIZE * 4:(y + 1) * SIZE * 4]
        bpy.data.images.remove(rendered)
    print(f'GATHERER_DIRECTION {row + 1}/8', flush=True)
sheet = bpy.data.images.new('채집꾼 8방향 보행', width=width, height=image_height, alpha=True)
sheet.pixels.foreach_set(atlas)
sheet.filepath_raw = str(OUT / 'walk.png')
sheet.file_format = 'PNG'
sheet.save()
bpy.data.images.remove(sheet)
temp.unlink(missing_ok=True)
report = {'source': 'Art/Blender/TownsfolkStudy/gatherer.blend',
          'output': 'SimpleGame/Assets/Characters/Townsfolk/Gatherer/walk.png',
          'dimensions': [width, image_height], 'cellSize': SIZE,
          'rows': 8, 'columns': columns, 'firstWalkFrame': 1, 'walkFrames': STEPS,
          'directionRows': [0, 7, 6, 5, 4, 3, 2, 1], 'idleFrame': 0, 'fixedCanvas': True,
          'canvasAnchorY': anchor, 'canvasCharacterHeight': height,
          'directionYaws': yaws, 'worldStride': 44,
          'gameHeight': 67, 'runtimeMesh': False, 'originalAssetsModified': False}
(HERE / 'gatherer_export.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n',
                                        encoding='utf-8')
print(json.dumps(report, ensure_ascii=False), flush=True)
write_preview()
