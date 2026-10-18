"""Create an independent gatherer candidate using the approved character's modeling/render basis.

blender --background --factory-startup --python Art/Blender/TownsfolkRefinement/create_candidate.py
Outputs only to this study folder. Never replaces game images or the approved source.
"""
import ast
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).resolve().parent
SOURCE = ROOT / 'Art/Blender/CharacterRefinement/protagonist_refined.blend'
TRACKED = [SOURCE, ROOT / 'Art/Blender/TownsfolkStudy/gatherer.blend',
           ROOT / 'SimpleGame/Assets/Characters/Blender/walk.png',
           ROOT / 'SimpleGame/Assets/Characters/Townsfolk/Gatherer/walk.png']
before = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in TRACKED}
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
bpy.context.preferences.filepaths.save_version = 0
scene = bpy.context.scene
rig = bpy.data.objects['주인공_리그']
rig.animation_data_clear()
rig.rotation_euler = (0, 0, 0)
for joint in rig.pose.bones:
    joint.matrix_basis.identity()
    joint.rotation_mode = 'XYZ'
bpy.ops.object.select_all(action='DESELECT')

# Reuse the approved anatomical surfaces, finger topology and joint weights.
# Costume, hair, eyes, facial expression and props below are specific to the gatherer.
remove = ('곡면 코트 자락', '코트 가장자리', '등 주술 자수', '입체 천 안대',
          '안대 접힌 주름', '자신감 있는 미소', '아랫입술', '주술 구체',
          '큰 은발 가닥', '관자놀이 잔가닥', '두께 있는 제복 깃', '깃 가장자리 봉제',
          '깃 앞단 봉제', '겹친 앞섶', '앞섶 가장자리', '은발 내부 덩어리',
          '가죽 허리띠', '버클 세로', '버클 가로', '소매 가죽 테두리', '소매 접힘',
          '부츠 끈', '목')
for ob in list(scene.objects):
    if ob.name.startswith(remove):
        bpy.data.objects.remove(ob, do_unlink=True)

# Load mesh helpers only; neither earlier source program nor its export loop is executed.
for path, wanted in [(ROOT / 'Art/Blender/create_protagonist.py',
                      {'bind', 'ellipsoid', 'surface', 'loft', 'blend_height'}),
                     (ROOT / 'Art/Blender/ProtagonistReference/create_candidate.py',
                      {'mesh', 'line', 'lock'})]:
    parsed = ast.parse(path.read_text(encoding='utf-8'))
    definitions = [n for n in parsed.body if isinstance(n, ast.FunctionDef) and n.name in wanted]
    exec(compile(ast.Module(body=definitions, type_ignores=[]), str(path), 'exec'), globals())


def material(name, base, kind='smooth'):
    """Same three light bands as the approved hero, with restrained material-specific grain."""
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*base, 1)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.clear()
    output = nodes.new('ShaderNodeOutputMaterial')
    diffuse = nodes.new('ShaderNodeBsdfDiffuse')
    diffuse.inputs['Color'].default_value = (1, 1, 1, 1)
    rgb = nodes.new('ShaderNodeShaderToRGB')
    ramp = nodes.new('ShaderNodeValToRGB')
    ramp.color_ramp.interpolation = 'CONSTANT'
    ramp.color_ramp.elements.remove(ramp.color_ramp.elements[1])
    for i, (position, intensity) in enumerate([(.0, .40), (.32, .70), (.66, 1.)]):
        e = ramp.color_ramp.elements[0] if i == 0 else ramp.color_ramp.elements.new(position)
        e.position = position
        e.color = (*(c * intensity for c in base), 1)
    links.new(diffuse.outputs[0], rgb.inputs[0])
    links.new(rgb.outputs[0], ramp.inputs[0])
    color = ramp.outputs[0]
    if kind != 'smooth':
        coord = nodes.new('ShaderNodeTexCoord')
        noise = nodes.new('ShaderNodeTexNoise')
        noise.inputs['Scale'].default_value = 110 if kind == 'cloth' else 75
        noise.inputs['Detail'].default_value = 2
        links.new(coord.outputs['Generated'], noise.inputs['Vector'])
        grain = nodes.new('ShaderNodeMapRange')
        grain.inputs['From Min'].default_value = 0
        grain.inputs['From Max'].default_value = 1
        grain.inputs['To Min'].default_value = .94 if kind == 'cloth' else .90
        grain.inputs['To Max'].default_value = 1
        links.new(noise.outputs['Fac'], grain.inputs['Value'])
        mix = nodes.new('ShaderNodeMixRGB')
        mix.blend_type = 'MULTIPLY'
        mix.inputs[0].default_value = 1
        links.new(color, mix.inputs[1])
        links.new(grain.outputs['Result'], mix.inputs[2])
        color = mix.outputs[0]
    emission = nodes.new('ShaderNodeEmission')
    links.new(color, emission.inputs['Color'])
    links.new(emission.outputs[0], output.inputs['Surface'])
    return mat


cloth = material('채집꾼 · 이끼색 삼베', (.105, .20, .135), 'cloth')
cloth_edge = material('채집꾼 · 소매와 천 가장자리', (.20, .30, .19), 'cloth')
linen = material('채집꾼 · 빛바랜 안감', (.65, .55, .37), 'cloth')
pants = material('채집꾼 · 먹빛 바지', (.075, .10, .13), 'cloth')
leather = material('채집꾼 · 밤색 가죽', (.15, .075, .038), 'leather')
leather_edge = material('채집꾼 · 가죽 모서리', (.29, .17, .075), 'leather')
skin = material('채집꾼 · 햇볕에 그을린 피부', (.79, .55, .38))
skin_shadow = material('채집꾼 · 입술과 눈꺼풀', (.36, .16, .10))
hair = material('채집꾼 · 진갈색 머리', (.045, .025, .013))
hair_highlight = material('채집꾼 · 앞머리 밝은 면', (.10, .056, .025))
hair_shade = material('채집꾼 · 머리 안쪽', (.022, .014, .01))
whites = material('채집꾼 · 눈 흰자', (.79, .73, .61))
iris = material('채집꾼 · 호박색 홍채', (.21, .095, .025))
black = material('채집꾼 · 동공과 속눈썹', (.012, .009, .006))
glint = material('채집꾼 · 눈 반사광', (.95, .94, .84))
wicker = material('채집꾼 · 등짐 바구니', (.36, .215, .088), 'wood')
wicker_light = material('채집꾼 · 바구니 밝은 엮음', (.50, .32, .13), 'wood')
leaf = material('채집꾼 · 약초 잎', (.12, .25, .065))
leaf_light = material('채집꾼 · 잎맥', (.28, .40, .10))
root_mat = material('채집꾼 · 채집한 뿌리', (.66, .56, .34), 'wood')

retained = []
for ob in list(scene.objects):
    if ob.type != 'MESH' or ob.parent != rig:
        continue
    retained.append(ob.name)
    if ob.name.startswith(('목', '얼굴 조형', '콧대', '귀', '손바닥', '손가락', '엄지')):
        mat = skin
    elif ob.name.startswith('귓바퀴'):
        mat = skin_shadow
    elif ob.name.startswith(('연속 바지', '바지 허리')):
        mat = pants
    elif ob.name.startswith('가죽 부츠'):
        mat = leather
    elif ob.name.startswith('밑창'):
        mat = leather_edge
    else:
        mat = cloth
    ob.data.materials.clear()
    ob.data.materials.append(mat)
    for modifier in ob.modifiers:
        if modifier.type == 'SUBSURF':
            modifier.levels = modifier.render_levels = 2
    if ob.name.startswith('얼굴 조형'):
        for v in ob.data.vertices:
            # A broader, calmer adult jaw than the protagonist; keep the continuous face.
            chin = max(0, min(1, (1.95 - v.co.z) / .20))
            v.co.x *= 1 + .10 * chin
    if ob.name.startswith('콧대'):
        for v in ob.data.vertices:
            v.co.x *= 1.12
    if ob.name.startswith('연속 소매'):
        for v in ob.data.vertices:
            if v.co.z < 1.23:
                v.co.z = 1.23 - (1.23 - v.co.z) * .45
    if ob.name.startswith('맞춤 코트 몸판'):
        blend_height(ob, 'hips', 'chest', 1.25, .28)

loft('채집꾼 · 목', [(0, .02, 1.64, .082, .075),
     (0, .018, 1.74, .078, .078), (0, .02, 1.79, .07, .07)], skin, 'head', count=48)
bpy.context.view_layer.update()
face_ob = bpy.data.objects['얼굴 조형']
deps = bpy.context.evaluated_depsgraph_get()
evaluated = face_ob.evaluated_get(deps)
face_mesh = evaluated.to_mesh()
face_bvh = BVHTree.FromPolygons([v.co for v in face_mesh.vertices],
                              [list(p.vertices) for p in face_mesh.polygons])
evaluated.to_mesh_clear()


def face_point(x, z, offset=.003):
    hit = face_bvh.ray_cast(Vector((x, -1, z)), Vector((0, 1, 0)))[0]
    if hit is None:
        raise ValueError(f'Facial detail misses face surface: {x}, {z}')
    return (x, hit.y - offset, z)


def face_patch(name, coords, mat, offset=.003):
    center = (sum(x for x, z in coords) / len(coords), sum(z for x, z in coords) / len(coords))
    points = [face_point(*center, offset)] + [face_point(x, z, offset) for x, z in coords]
    return mesh(name, points, [(0, i + 1, (i + 1) % len(coords) + 1)
                              for i in range(len(coords))], mat)


for sign in [-1, 1]:
    xc, zc = sign * .088, 1.996
    eye_loop = [(xc + .055 * math.cos(a), zc + .022 * math.sin(a) * abs(math.sin(a)) ** .35)
                for a in [i * math.tau / 32 for i in range(32)]]
    face_patch('채집꾼 · 아몬드형 눈', eye_loop, whites, .004)
    for label, rx, rz, mat, offset in [('홍채', .016, .019, iris, .006),
                                     ('동공', .007, .014, black, .008)]:
        face_patch('채집꾼 · ' + label,
                   [(xc + rx * math.cos(a), zc + rz * math.sin(a))
                    for a in [i * math.tau / 24 for i in range(24)]], mat, offset)
    face_patch('채집꾼 · 눈빛', [(xc - .005 + .004 * math.cos(a), zc + .007 + .004 * math.sin(a))
                              for a in [i * math.tau / 12 for i in range(12)]], glint, .010)
    upper = [face_point(xc + .055 * math.cos(a), zc + .022 * math.sin(a), .007)
             for a in [i * math.pi / 12 for i in range(13)]]
    line('채집꾼 · 윗눈꺼풀', upper, .0023, black)
    eyebrow = [(xc - .047, 2.046), (xc - .015, 2.054), (xc + .037, 2.048),
               (xc + .042, 2.041), (xc - .011, 2.044), (xc - .045, 2.038)]
    face_patch('채집꾼 · 가는 눈썹', eyebrow, hair, .005)
    line('채집꾼 · 눈가', [face_point(sign * x, z, .003)
                         for x, z in [(.14, 1.967), (.155, 1.960), (.166, 1.956)]],
         .0012, skin_shadow)
line('채집꾼 · 편안한 입매', [face_point(x, z, .003)
                           for x, z in [(-.052, 1.823), (-.023, 1.819), (0, 1.818),
                                        (.024, 1.821), (.051, 1.827)]], .0018, skin_shadow)
line('채집꾼 · 아래 입술', [face_point(x, z, .002)
                         for x, z in [(-.023, 1.805), (0, 1.802), (.025, 1.807)]],
     .0013, skin_shadow)

# A short, directional hairstyle: a fitted cap and flattened tapered clumps.
# The cap stops above the brows at the front and grows lower over the occiput.
verts, faces = [], []
n, rings = 64, 14
for j in range(rings):
    t = (j + .01) / (rings - 1 + .01)
    for i in range(n):
        a = i * math.tau / n
        bottom = 2.075 if math.cos(a) > .45 else 1.97
        radial = math.sin(t * 1.61)
        verts.append((math.sin(a) * .239 * radial, .025 - math.cos(a) * .253 * radial,
                      2.25 + (bottom - 2.25) * t))
for j in range(rings - 1):
    for i in range(n):
        faces.append((j * n + i, j * n + (i + 1) % n,
                      (j + 1) * n + (i + 1) % n, (j + 1) * n + i))
cap = mesh('채집꾼 · 연결된 머리 바탕', verts, faces, hair)
sub = cap.modifiers.new('머리 표면', 'SUBSURF')
sub.levels = sub.render_levels = 2
for i in range(13):
    x = -.19 + i * .031
    lock('채집꾼 · 비스듬한 앞머리', [(x * .65 - .035, -.015, 2.23),
         (x - .015, -.16, 2.275), (x + .06, -.254, 2.18),
         (x + .04, -.248, 2.075 + .018 * math.cos(i))],
         .037, hair_highlight if i % 4 == 0 else hair)
for side in [-1, 1]:
    for i in range(9):
        y = -.16 + i * .043
        lock('채집꾼 · 옆머리 층', [(side * .16, y * .85, 2.17),
             (side * .25, y, 2.18), (side * .26, y + .04, 2.09),
             (side * .219, y + .055, 1.986 + .025 * math.sin(i))],
             .030, hair if i % 4 else hair_highlight)
for i in range(12):
    a = i * math.tau / 12
    lock('채집꾼 · 뒤통수 결', [(.12 * math.sin(a), .13, 2.18),
         (.20 * math.sin(a), .25, 2.15), (.19 * math.sin(a), .27, 2.07),
         (.14 * math.sin(a), .23, 1.972)], .030, hair_shade if i % 3 == 0 else hair)

# Flat ribbon headband with a folded knot, distinct from the protagonist's blindfold.
loft('채집꾼 · 이마 두건', [(0, .013, 2.060, .247, .267),
     (0, .016, 2.086, .248, .267), (0, .02, 2.100, .246, .264)], linen, 'head', count=64)
for side in [-1, 1]:
    ob = mesh('채집꾼 · 두건 매듭 꼬리', [(0, .289, 2.085), (side * .052, .292, 2.085),
              (side * .08, .302, 1.945), (side * .032, .304, 1.956)], [(0, 1, 2, 3)], linen)
    solid = ob.modifiers.new('천 가장자리 두께', 'SOLIDIFY')
    solid.thickness = .004

# Sample the actual refined chest surface so the lapels cannot disappear into its volume.
bpy.context.view_layer.update()
evaluated = bpy.data.objects['맞춤 코트 몸판'].evaluated_get(bpy.context.evaluated_depsgraph_get())
body_mesh = evaluated.to_mesh()
body_bvh = BVHTree.FromPolygons([v.co for v in body_mesh.vertices],
                              [list(p.vertices) for p in body_mesh.polygons])
evaluated.to_mesh_clear()


def body_point(x, z, offset=.008):
    hit = body_bvh.ray_cast(Vector((x, -1, z)), Vector((0, 1, 0)))[0]
    if hit is None:
        raise ValueError(f'Lapel misses chest surface: {x}, {z}')
    return (x, hit.y - offset, z)


verts, faces = [], []
for j in range(17):
    t = j / 16
    z = 1.65 - .23 * t
    for i in range(9):
        x = (i / 8 * 2 - 1) * (.102 * (1 - t) + .004)
        verts.append(body_point(x, z, .005))
for j in range(16):
    for i in range(8):
        k = j * 9 + i
        faces.append((k, k + 1, k + 10, k + 9))
mesh('채집꾼 · 겹쳐 입은 속옷', verts, faces, linen, 'chest')
for side in [-1, 1]:
    start = Vector((side * .113, 1.651))
    end = Vector((-.0, 1.422)) if side == 1 else Vector((.16, 1.21))
    direction = (end - start).normalized()
    across = Vector((-direction.y, direction.x)) * .022
    verts, faces, seam_points = [], [], []
    for j in range(25):
        center = start.lerp(end, j / 24)
        for sign in [-1, 1]:
            x, z = center + across * sign
            verts.append(body_point(x, z, .014 if side == -1 else .011))
        seam_points.append(verts[-1])
    for j in range(24):
        k = j * 2
        faces.append((k, k + 1, k + 3, k + 2))
    ob = mesh('채집꾼 · 겹친 넓은 옷깃', verts, faces, linen, 'chest')
    solid = ob.modifiers.new('옷깃 안감', 'SOLIDIFY')
    solid.thickness = .006
    line('채집꾼 · 옷깃 봉제', seam_points, .0018, cloth_edge, 'chest')
loft('채집꾼 · 묶은 허리띠', [(0, 0, 1.10, .264, .256),
     (0, 0, 1.19, .268, .259)], leather, 'hips', count=48)
line('채집꾼 · 허리띠 윗단', [(.272 * math.cos(a), .264 * math.sin(a), 1.185)
                           for a in [i * math.tau / 48 for i in range(49)]],
     .003, leather_edge, 'hips')
for side in [-1, 1]:
    # Short split overskirt with modeled fold valleys and a narrow cloth edge.
    verts, faces = [], []
    rows, columns = 10, 32
    for j in range(rows):
        t = j / (rows - 1)
        for i in range(columns):
            a = .11 + (math.pi - .20) * i / (columns - 1)
            radius = .263 + .027 * t
            fold = .011 * math.sin(a * 8 + .4) * math.sin(t * math.pi)
            verts.append((side * math.sin(a) * (radius + fold),
                          -math.cos(a) * (radius * .94 + fold),
                          1.13 - .25 * t + .025 * math.cos(a * 2) * t ** 4))
    for j in range(rows - 1):
        for i in range(columns - 1):
            k = j * columns + i
            faces.append((k, k + 1, k + columns + 1, k + columns))
    ob = mesh('채집꾼 · 갈라진 작업복 자락', verts, faces, cloth, 'hips')
    mod = ob.modifiers.new('작업복 곡면', 'SUBSURF')
    mod.levels = mod.render_levels = 2
    mod = ob.modifiers.new('작업복 천 두께', 'SOLIDIFY')
    mod.thickness = .008
    line('채집꾼 · 자락 끝 봉제', verts[-columns:], .0025, cloth_edge, 'hips')
    side_name, ax, x = ('L' if side < 0 else 'R'), side * .357, side * .17
    loft('채집꾼 · 드러난 팔목', [(ax, 0, .96, .055, .055),
         (ax, 0, 1.08, .064, .074), (ax, 0, 1.15, .073, .082)],
         skin, 'forearm.' + side_name, count=32)
    loft('채집꾼 · 접어 올린 소매', [(ax, .003, 1.108, .089, .111),
         (ax, .005, 1.125, .098, .12), (ax, .005, 1.157, .098, .12),
         (ax, .004, 1.169, .088, .107)], cloth_edge, 'forearm.' + side_name, count=48)
    for z in [.991, 1.018, 1.043, 1.067]:
        loft('채집꾼 · 손목 천 감개', [(ax, 0, z, .067, .076),
             (ax, 0, z + .020, .069, .077)], linen, 'forearm.' + side_name, count=32)
    for z in [.30, .34, .38]:
        loft('채집꾼 · 각반 감개', [(x, -.003, z, .092, .117),
             (x, -.003, z + .028, .094, .119)], linen, 'shin.' + side_name, count=32)
    # Broad, flat knee folds with blended original leg weights still underneath.
    for j in range(2):
        line('채집꾼 · 무릎 접힘', [(x - .065, -.103, .57 + j * .04),
             (x, -.144, .555 + j * .04), (x + .075, -.093, .58 + j * .04)],
             .003, cloth_edge, 'shin.' + side_name)
    line('채집꾼 · 신발 조임끈', [(x - .06, -.143, .19), (x, -.17, .225),
         (x + .06, -.14, .19)], .006, linen, 'foot.' + side_name)

# Flattened woven straps avoid round, inflated tubing on the basket silhouette.
loft('채집꾼 · 바구니 안쪽', [(0, .31, 1.08, .20, .105),
     (0, .32, 1.18, .23, .13), (0, .33, 1.58, .26, .155)], wicker, 'chest', count=48)
for j in range(9):
    z = 1.10 + j * .059
    radius = .205 + (z - 1.10) * .11
    vertices = []
    for dz in [-.012, .012]:
        for i in range(65):
            a = math.tau * i / 64
            r = radius + .003 * math.sin(a * 20 + j * math.pi)
            vertices.append((math.sin(a) * r, .326 + math.cos(a) * r * .60, z + dz))
    ob = mesh('채집꾼 · 납작한 가로 엮음', vertices,
              [(i, i + 1, i + 66, i + 65) for i in range(64)],
              wicker_light if j % 2 == 0 else wicker, 'chest')
    solid = ob.modifiers.new('등나무 띠 두께', 'SOLIDIFY')
    solid.thickness = .003
for i in range(24):
    a = i * math.tau / 24
    vertices = []
    for j in range(11):
        z = 1.08 + .052 * j
        radius = .202 + (z - 1.08) * .11 + .003 * math.cos(j * math.pi + i * math.pi)
        for offset in [-.018, .018]:
            vertices.append((math.sin(a + offset) * radius,
                             .326 + math.cos(a + offset) * radius * .60, z))
    ob = mesh('채집꾼 · 납작한 세로 엮음', vertices,
              [(j * 2, j * 2 + 1, j * 2 + 3, j * 2 + 2) for j in range(10)],
              wicker_light, 'chest')
    solid = ob.modifiers.new('등나무 세로 띠 두께', 'SOLIDIFY')
    solid.thickness = .003
for side in [-1, 1]:
    # Shoulder straps are ribbons with a broad face and a thin side, not ropes.
    centers = [(side * .20, .34, 1.59), (side * .22, .12, 1.65),
               (side * .23, -.11, 1.58), (side * .23, -.259, 1.35),
               (side * .21, -.20, 1.15), (side * .20, .18, 1.12)]
    points = [(x + offset, y, z) for x, y, z in centers for offset in [-.021, .021]]
    ob = mesh('채집꾼 · 납작한 등짐 어깨끈', points,
              [(i * 2, i * 2 + 1, i * 2 + 3, i * 2 + 2) for i in range(len(centers) - 1)],
              leather, 'chest')
    mod = ob.modifiers.new('어깨끈 곡면', 'SUBSURF')
    mod.levels = mod.render_levels = 2
    mod = ob.modifiers.new('어깨끈 두께', 'SOLIDIFY')
    mod.thickness = .007
    line('채집꾼 · 어깨끈 봉제', [(x + side * .015, y - .004, z) for x, y, z in centers],
         .0018, linen, 'chest')
for i in range(14):
    a = i * 2.399963
    cx, cy = .16 * math.sin(a), .33 + .09 * math.cos(a)
    top = 1.75 + .10 * (i % 3) / 2
    line('채집꾼 · 약초 줄기', [(cx, cy, 1.54), (cx * 1.08, cy, top)], .003, leaf, 'chest')
    for sign in [-1, 1]:
        z = top - .07
        ob = mesh('채집꾼 · 접힌 약초 잎', [(cx, cy, z - .07),
             (cx + sign * .06, cy - .012, z + .015), (cx + sign * .045, cy, z + .09),
             (cx, cy + .016, z + .02)], [(0, 1, 3), (1, 2, 3)], leaf, 'chest')
        mod = ob.modifiers.new('잎의 두께', 'SOLIDIFY')
        mod.thickness = .002
        line('채집꾼 · 약초 잎맥', [(cx, cy - .003, z - .04),
             (cx + sign * .025, cy - .003, z + .035),
             (cx + sign * .045, cy - .003, z + .09)], .0015, leaf_light, 'chest')

rig.name = '채집꾼_정교화_리그'
rig['제작 기준'] = '승인된 주인공 신체 곡면·손가락·관절 가중치 / 채집꾼 고유 의복·머리·눈·등짐'
rig['상태'] = '비교 후보. 게임에 미적용.'
scene['후보 상태'] = '게임용 PNG 내보내기 및 교체 전, 외형 선택을 위한 후보'
sys.path.insert(0, str(ROOT / 'Art/Blender/CharacterRefinement'))
from locomotion import apply_locomotion


def pose(kind, t):
    for joint in rig.pose.bones:
        joint.location = (0, 0, 0)
        joint.rotation_euler = (0, 0, 0)
    if kind == 'walk':
        apply_locomotion(rig, kind, t, walk_stride=.39)
        rig.pose.bones['chest'].rotation_euler.x = .06
        for side in ['L', 'R']:
            rig.pose.bones['arm.' + side].rotation_euler.x *= .72
    else:
        rig.pose.bones['chest'].rotation_euler.x = .006 * math.sin(t * math.tau)
        for side in ['L', 'R']:
            rig.pose.bones['forearm.' + side].rotation_euler.x = -.12
    bpy.context.view_layer.update()


for kind in ['idle', 'walk']:
    rig.animation_data_create()
    rig.animation_data.action = None
    for frame in range(33):
        pose(kind, frame / 32)
        for joint in rig.pose.bones:
            joint.keyframe_insert('location', frame=frame + 1, group=joint.name)
            joint.keyframe_insert('rotation_euler', frame=frame + 1, group=joint.name)
    rig.animation_data.action.name = '채집꾼 정교화 · ' + ('대기' if kind == 'idle' else '걷기')
    rig.animation_data.action.use_fake_user = True
rig.animation_data.action = None
pose('idle', 0)
scene.render.engine = 'BLENDER_EEVEE'
scene.eevee.taa_render_samples = 32
scene.render.resolution_x, scene.render.resolution_y = 640, 800
scene.render.resolution_percentage = 100
scene.render.film_transparent = False
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'
scene.view_settings.view_transform = 'Standard'
scene.frame_start, scene.frame_end = 1, 32
camera = scene.camera
camera.data.type = 'ORTHO'


def render(name, angle=.55, portrait=False):
    target = Vector((0, 0, 2.015 if portrait else 1.31))
    camera.location = target + Vector((6 * math.sin(angle), -6 * math.cos(angle),
                                        .20 if portrait else .55))
    camera.rotation_euler = (target - camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.data.ortho_scale = 1.15 if portrait else 2.86
    scene.render.filepath = str(OUT / (name + '.png'))
    bpy.ops.render.render(write_still=True)


# Save only the independent candidate; never save the protagonist source.
render('candidate_front', 0)
render('candidate_perspective')
render('candidate_right', math.pi / 2)
render('candidate_back', math.pi)
render('candidate_portrait', .35, True)
pose('idle', 0)
render('candidate_perspective')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'gatherer_candidate.blend'))

# Same camera, resolution and actual light rig on all three comparison subjects.
candidate_collection = bpy.data.collections.new('비교용 채집꾼 후보')
scene.collection.children.link(candidate_collection)
for ob in list(scene.objects):
    if ob == rig or ob.parent == rig:
        for collection in list(ob.users_collection):
            collection.objects.unlink(ob)
        candidate_collection.objects.link(ob)
candidate_collection.hide_render = True


def append_subject(path, rig_name):
    with bpy.data.libraries.load(str(path), link=False) as (source, destination):
        destination.objects = list(source.objects)
    objects = destination.objects
    source_rig = next(ob for ob in objects if ob and ob.name.startswith(rig_name))
    for ob in objects:
        if ob and (ob == source_rig or ob.parent == source_rig):
            scene.collection.objects.link(ob)
    source_rig.animation_data_clear()
    source_rig.rotation_euler = (0, 0, 0)
    for joint in source_rig.pose.bones:
        joint.matrix_basis.identity()
    return [ob for ob in objects if ob and (ob == source_rig or ob.parent == source_rig)]


hero_objects = append_subject(SOURCE, '주인공_리그')
render('reference_protagonist')
for ob in hero_objects:
    ob.hide_render = True
old_objects = append_subject(ROOT / 'Art/Blender/TownsfolkStudy/gatherer.blend', '채집꾼_리그')
render('reference_previous')
for ob in old_objects:
    ob.hide_render = True
after = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in TRACKED}
if before != after:
    raise RuntimeError('A protected original or game asset changed during candidate creation')
report = {'source': str(SOURCE.relative_to(ROOT)), 'output': str((OUT / 'gatherer_candidate.blend').relative_to(ROOT)),
          'status': 'candidate_not_applied', 'bodyTopologyReference': retained,
          'bones': len(rig.data.bones), 'camera': {'resolution': [640, 800], 'orthoScale': 2.86},
          'renderer': 'BLENDER_EEVEE', 'material': 'three-band cel / subtle woven and leather grain',
          'protectedBefore': before, 'protectedAfter': after, 'gameAssetsModified': False,
          'gameBuildRunTested': False}
(OUT / 'candidate_manifest.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n',
                                            encoding='utf-8')
print('GATHERER_CANDIDATE_READY', flush=True)
