"""Independent, editable NPC selection candidates. Never writes runtime assets.

blender --background --factory-startup --python Art/Blender/NpcCollection/create_collection.py
Optional -- --only raen for one candidate. Compose selection boards with compose_collection.py.
"""
import argparse
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
SOURCE = ROOT / 'Art/Blender/TownsfolkRefinement/gatherer_candidate.blend'
DESIGNS = [
    dict(id='travelling_merchant', label='행상인', category='crowd', role=1,
         coat=(.20,.11,.055), trim=(.58,.40,.19), hair=(.15,.09,.04), style='short',
         outfit='haori', gear='pack', width=1.07),
    dict(id='artisan', label='생활 장인', category='crowd', role=2,
         coat=(.69,.56,.37), trim=(.34,.09,.04), hair=(.035,.018,.013), style='bun',
         outfit='apron', gear='tools', female=True),
    dict(id='hunter', label='주령 사냥꾼', category='crowd', role=3,
         coat=(.035,.105,.12), trim=(.62,.49,.31), hair=(.025,.019,.017), style='long',
         outfit='splitcoat', gear='sword'),
    dict(id='sibling', label='동생', category='story', source='sibling',
         coat=(.76,.69,.52), trim=(.095,.12,.20), hair=(.065,.035,.022), style='bob',
         outfit='haori', gear='ribbon', child=True, female=True),
    dict(id='parent', label='부모', category='story', source='parent',
         coat=(.22,.095,.13), trim=(.69,.59,.40), hair=(.045,.028,.017), style='bun',
         outfit='kimono', gear='apron', female=True),
    dict(id='village_merchant', label='잡화상', category='story', source='merchant',
         coat=(.52,.29,.055), trim=(.17,.24,.20), hair=(.14,.10,.065), style='bald',
         outfit='haori', gear='ledger', width=1.24, elder=True),
    dict(id='child', label='마을 아이', category='story', source='child',
         coat=(.15,.20,.085), trim=(.37,.27,.15), hair=(.115,.06,.022), style='tousled',
         outfit='haori', gear='satchel', child=True),
    dict(id='stranger', label='돌아가지 못한 사람', category='story', source='stranger',
         coat=(.12,.135,.13), trim=(.32,.30,.24), hair=(.055,.045,.03), style='long',
         outfit='ragged', gear='rope', width=.90),
    dict(id='raen', label='라엔', category='story', source='raen',
         coat=(.035,.045,.065), trim=(.23,.035,.055), hair=(.78,.82,.85), style='bob',
         outfit='armour', gear='scarf', female=True),
    dict(id='sealer', label='봉인사', category='story', source='sealer',
         coat=(.12,.055,.18), trim=(.69,.54,.24), hair=(.68,.65,.57), style='bun',
         outfit='ceremonial', gear='seals', female=True, elder=True),
    dict(id='smith', label='대장장이', category='story', source='smith',
         coat=(.04,.045,.05), trim=(.25,.115,.045), hair=(.025,.018,.012), style='short',
         outfit='apron', gear='hammer', width=1.16),
]


def definitions(path, names):
    parsed = ast.parse(path.read_text(encoding='utf-8'))
    nodes = [n for n in parsed.body if isinstance(n, ast.FunctionDef) and n.name in names]
    exec(compile(ast.Module(body=nodes, type_ignores=[]), str(path), 'exec'), globals())


definitions(ROOT / 'Art/Blender/TownsfolkRefinement/create_candidate.py', {'material'})
definitions(ROOT / 'Art/Blender/create_protagonist.py',
            {'bind', 'ellipsoid', 'surface', 'loft', 'blend_height'})
definitions(ROOT / 'Art/Blender/ProtagonistReference/create_candidate.py', {'mesh', 'line', 'lock'})
sys.path.insert(0, str(ROOT / 'Art/Blender/CharacterRefinement'))
from locomotion import apply_locomotion


def remove_prefixes(prefixes):
    for ob in list(scene.objects):
        if ob.name.startswith(prefixes):
            bpy.data.objects.remove(ob, do_unlink=True)


def box(name, at, size, mat, joint='hips'):
    bpy.ops.mesh.primitive_cube_add(size=1, location=at)
    ob = bpy.context.object
    ob.scale = size
    bind(ob, name, mat, joint)
    bevel = ob.modifiers.new('마모된 모서리', 'BEVEL')
    bevel.width, bevel.segments = .012, 3
    return ob


def panel(name, coords, mat, joint='hips', thickness=.008):
    ob = mesh(name, coords, [tuple(range(len(coords)))], mat, joint)
    solid = ob.modifiers.new('천과 가죽 두께', 'SOLIDIFY')
    solid.thickness = thickness
    bevel = ob.modifiers.new('둥근 가장자리', 'BEVEL')
    bevel.width, bevel.segments = .003, 2
    return ob


def garment(name, top, bottom, radius, mat, split=False, folds=.012):
    for sign in ([-1, 1] if split else [1]):
        verts, faces = [], []
        cols, rows = 48, 16
        for j in range(rows):
            t = j / (rows - 1)
            for i in range(cols):
                a = .14 + (math.pi - .24 if split else math.tau - .28) * i / (cols - 1)
                fold = folds * math.sin(a * 10 + .4) * math.sin(t * math.pi)
                r = radius + .045 * t + fold
                hem = (.018 * math.sin(a * 13) if name == '해진 로브' else .0) * t ** 5
                verts.append((sign * math.sin(a) * r, -math.cos(a) * r * .93,
                              top + (bottom - top) * t + hem))
        for j in range(rows - 1):
            for i in range(cols - 1):
                k = j * cols + i
                faces.append((k, k + 1, k + cols + 1, k + cols))
        ob = mesh(name, verts, faces, mat, 'hips')
        mod = ob.modifiers.new('의복 곡면', 'SUBSURF')
        mod.levels = mod.render_levels = 2
        mod = ob.modifiers.new('의복 안감', 'SOLIDIFY')
        mod.thickness = .008
        # Low leg influence preserves hip anchoring without freezing the hem during walking.
        if split:
            blend_height(ob, 'thigh.L' if sign < 0 else 'thigh.R', 'hips', .80, .8)
        line(name + ' 끝단', verts[-cols:], .0025, trim, 'hips')


def hairstyle(kind, color):
    dark = material('머리 안쪽', tuple(c * .50 for c in color))
    hair = material('머리 바깥 결', color)
    light = material('머리 밝은 결', tuple(min(.95, c * 1.35 + .014) for c in color))
    # Curved connected cap, an open forehead, and tapering strands of different lengths.
    verts, faces = [], []
    for j in range(14):
        t = (j + .01) / 13.01
        for i in range(64):
            a = i * math.tau / 64
            bottom = 2.07 if math.cos(a) > .5 else (1.83 if kind in ('bob', 'long') else 1.99)
            radial = math.sin(t * 1.61)
            verts.append((math.sin(a) * .241 * radial, .025 - math.cos(a) * .256 * radial,
                          2.25 + (bottom - 2.25) * t))
    for j in range(13):
        for i in range(64):
            faces.append((j * 64 + i, j * 64 + (i + 1) % 64,
                          (j + 1) * 64 + (i + 1) % 64, (j + 1) * 64 + i))
    if kind != 'bald':
        ob = mesh('연결된 머리 바탕', verts, faces, dark)
        mod = ob.modifiers.new('머리 곡면', 'SUBSURF')
        mod.levels = mod.render_levels = 2
    if kind in ('short', 'tousled'):
        for i in range(24):
            a = i * 2.39996
            r = .06 + (i % 4) * .041
            x, y = math.sin(a) * r, math.cos(a) * r
            lock('짧은 머리 층', [(x, y, 2.17), (x * 1.3, y * 1.2, 2.29),
                 (x * 1.4 + .025, y * 1.3, 2.28), (x * 1.5 + .025, y * 1.4, 2.18)],
                 .040, light if i % 5 == 0 else hair)
    elif kind == 'bald':
        for side in [-1, 1]:
            for i in range(6):
                y = -.03 + i * .035
                lock('옆으로 남은 머리', [(side * .21, y, 2.07), (side * .26, y, 2.10),
                     (side * .26, y + .03, 2.04), (side * .23, y + .05, 1.99)], .027, hair)
    else:
        for i in range(22):
            a = .65 + (math.tau - 1.3) * i / 21
            length = 1.58 if kind == 'long' else 1.82 if kind == 'bob' else 2.03
            sx, cy = math.sin(a), -math.cos(a)
            lock('뒤로 흐르는 머리', [(sx * .12, cy * .11, 2.21),
                 (sx * .28, cy * .27, 2.22), (sx * .29, cy * .28, length + .12),
                 (sx * .255, cy * .255, length + .035 * math.sin(i))],
                 .046, light if i % 5 == 0 else hair)
    if kind not in ('bald',):
        for i in range(9):
            x = -.18 + i * .042
            lock('옆가르마 앞머리', [(x * .65 - .02, -.025, 2.21),
                 (x, -.15, 2.27), (x + .042, -.258, 2.13),
                 (x + .018, -.250, 2.06 - .028 * (i / 8))],
                 .034, light if i % 4 == 0 else hair)
    if kind == 'bun':
        ellipsoid('틀어 올린 머리', (0, .17, 2.25), (.12, .13, .11), hair, 'head', 32)
        for i in range(8):
            a = i * math.pi / 8
            line('올린 머리의 결', [(math.sin(a) * .10, .17, 2.26 + math.cos(a) * .10),
                 (0, .29, 2.25), (-math.sin(a) * .10, .17, 2.26 - math.cos(a) * .10)],
                 .004, light)


def build(spec):
    global scene, rig, trim
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
    bpy.context.preferences.filepaths.save_version = 0
    scene = bpy.context.scene
    rig = bpy.data.objects['채집꾼_정교화_리그']
    rig.animation_data_clear()
    rig.rotation_euler = (0, 0, 0)
    for b in rig.pose.bones:
        b.matrix_basis.identity()
    bpy.ops.object.select_all(action='DESELECT')
    keep = ('아몬드형 눈', '홍채', '동공', '눈빛', '윗눈꺼풀', '가는 눈썹',
            '눈가', '편안한 입매', '아래 입술', '목', '드러난 팔목')
    for ob in list(scene.objects):
        if ob.name.startswith('채집꾼 · ') and not any(ob.name.startswith('채집꾼 · ' + k) for k in keep):
            bpy.data.objects.remove(ob, do_unlink=True)
    cloth = material(spec['label'] + ' 원단', spec['coat'], 'cloth')
    trim = material(spec['label'] + ' 대비 원단', spec['trim'], 'cloth')
    ivory = material('생성리 안감', (.66,.59,.43), 'cloth')
    leather = material('가죽 소품', (.15,.075,.035), 'leather')
    gold = material('황동 장식', (.53,.37,.12))
    black = material('어두운 가죽', (.025,.027,.032), 'leather')
    skin = material('고유 피부색', (.88,.68,.52) if spec.get('child') else
                    (.86,.65,.50) if spec.get('female') else (.72,.48,.31))
    hair_color = spec['hair']
    for ob in list(scene.objects):
        if ob.type != 'MESH' or ob.parent != rig:
            continue
        old_name = ob.data.materials[0].name if ob.data.materials else ''
        if '피부' in old_name or ob.name.startswith(('목','얼굴 조형','콧대','귀','손바닥','손가락','엄지')):
            mat = skin
        elif ob.name.startswith('밑창'):
            mat = leather
        elif ob.name.startswith('가죽 부츠'):
            mat = black
        elif ob.name.startswith(('맞춤 코트','연속 소매')):
            mat = cloth
        elif ob.name.startswith(('연속 바지','바지 허리')):
            mat = black
        else:
            continue
        ob.data.materials.clear()
        ob.data.materials.append(mat)
    hairstyle(spec['style'], hair_color)
    outfit = spec['outfit']
    # Follow the actual chest rather than putting flat cloth through its volume.
    bpy.context.view_layer.update()
    evaluated = bpy.data.objects['맞춤 코트 몸판'].evaluated_get(bpy.context.evaluated_depsgraph_get())
    evaluated_mesh = evaluated.to_mesh()
    bvh = BVHTree.FromPolygons([v.co for v in evaluated_mesh.vertices],
                              [list(p.vertices) for p in evaluated_mesh.polygons])
    evaluated.to_mesh_clear()
    def chest_point(x,z,offset):
        hit=bvh.ray_cast(Vector((x,-1,z)),Vector((0,1,0)))[0]
        if hit is None:
            raise ValueError('Cloth misses chest surface')
        return (x,hit.y-offset,z)
    vertices,faces=[],[]
    for j in range(17):
        t=j/16
        for i in range(9):
            vertices.append(chest_point((i/8*2-1)*(.102*(1-t)+.004),1.65-.23*t,.006))
    for j in range(16):
        for i in range(8):
            k=j*9+i
            faces.append((k,k+1,k+10,k+9))
    mesh('겹친 속깃',vertices,faces,ivory,'chest')
    for sign in [-1,1]:
        start=Vector((sign*.113,1.651))
        end=Vector((0,1.422)) if sign==1 else Vector((.16,1.21))
        direction=(end-start).normalized()
        across=Vector((-direction.y,direction.x))*.022
        vertices,faces=[],[]
        for j in range(25):
            center=start.lerp(end,j/24)
            for side in [-1,1]:
                x,z=center+across*side
                vertices.append(chest_point(x,z,.015 if sign==-1 else .012))
        for j in range(24):
            k=j*2
            faces.append((k,k+1,k+3,k+2))
        ob=mesh('겹친 넓은 앞깃',vertices,faces,trim,'chest')
        mod=ob.modifiers.new('옷깃 두께','SOLIDIFY')
        mod.thickness=.006
    loft('허리띠', [(0,0,1.10,.269,.265),(0,0,1.20,.272,.267)], trim,'hips',48)
    for side in ['L','R']:
        ax = -.357 if side=='L' else .357
        loft('소매 끝단 '+side,[(ax,0,1.11,.093,.116),(ax,0,1.17,.095,.118)],
             trim,'forearm.'+side,32)
    if outfit in ('kimono','ceremonial'):
        garment('긴 겹옷',1.16,.22,.285,cloth,False,.012)
        # Wider upper sleeves are attached to the existing arm bones.
        for side,x in [('L',-.357),('R',.357)]:
            ob=loft('넓은 예복 소매 '+side,[(x,0,1.08,.12,.19),
                (x,0,1.23,.15,.21),(x,0,1.54,.14,.17)],cloth,'arm.'+side,40)
            blend_height(ob,'forearm.'+side,'arm.'+side,1.22,.20)
    elif outfit in ('haori','splitcoat','ragged'):
        garment('해진 로브' if outfit=='ragged' else '트인 겉옷',1.16,
                .40 if outfit in ('splitcoat','ragged') else .78,.285,cloth,True)
    if outfit=='apron' or spec['gear']=='apron':
        panel('가죽 작업 앞치마' if outfit=='apron' else '천 앞치마',
              [(-.21,-.28,1.52),(.21,-.28,1.52),(.27,-.31,.47),(-.27,-.31,.47)],
              trim,'hips',.012)
        for sign in [-1,1]:
            line('앞치마 어깨끈',[(sign*.20,-.28,1.50),(sign*.23,-.10,1.65),
                 (sign*.23,.20,1.51)],.014,leather,'chest')
        if outfit=='apron':
            for i in range(3):
                box('공구 주머니',(-.17+i*.17,-.325,1.02),(.12,.045,.15),leather)
    if outfit=='armour':
        panel('곡면 흉갑',[(-.25,-.28,1.54),(.25,-.28,1.54),(.20,-.30,1.27),
              (0,-.33,1.23),(-.20,-.30,1.27)],black,'chest',.018)
        garment('갈라진 전투복',1.14,.68,.28,cloth,True,.006)
        for sign in [-1,1]:
            for j in range(3):
                panel('허리 갑옷판',[(sign*.22,-.19-j*.002,1.08-j*.085),
                    (sign*.34,-.14-j*.002,1.06-j*.085),(sign*.35,-.15,.99-j*.085),
                    (sign*.23,-.21,1.00-j*.085)],black)
    gear=spec['gear']
    if gear=='pack':
        box('큰 행상 배낭',(0,.41,1.40),(.48,.30,.65),leather,'chest')
        ellipsoid('말아 묶은 담요',(0,.43,1.80),(.37,.14,.14),trim,'chest',32)
        for sign in [-1,1]:
            line('배낭 어깨끈',[(sign*.23,.45,1.72),(sign*.23,-.01,1.64),
                 (sign*.21,-.28,1.24)],.022,leather,'chest')
            line('담요 조임끈',[(sign*.18,.32,1.79),(sign*.18,.44,1.95),
                 (sign*.18,.57,1.79)],.012,leather,'chest')
    if gear in ('ledger','satchel'):
        box('서책 가방' if gear=='ledger' else '작은 어깨 가방',(.29,-.02,1.0),(.17,.19,.24),leather)
        line('대각선 가방끈',[(-.22,-.14,1.62),(0,-.30,1.36),(.30,-.20,1.04)],.015,trim,'chest')
    if gear=='ledger':
        for sign in [-1,1]:
            x=sign*.092
            line('둥근 안경',[(x+.061*math.cos(a),-.279,1.998+.045*math.sin(a))
                 for a in [i*math.tau/32 for i in range(33)]],.0032,gold)
        line('안경 연결부',[(-.03,-.28,2.0),(0,-.287,2.01),(.03,-.28,2.0)],.0028,gold)
    if gear in ('sword','seals'):
        for i in range(4 if gear=='seals' else 2):
            x=(i-1.5)*.10
            ob=panel('매달린 종이 부적',[(x-.035,-.30,1.11),(x+.035,-.30,1.11),
                (x+.045,-.32,.75),(x-.025,-.32,.75)],ivory)
            line('부적의 붉은 문자',[(x,-.325,1.07),(x+.012,-.33,.95),
                (x-.01,-.335,.87),(x+.01,-.34,.79)],.004,trim,'hips')
    if gear=='sword':
        line('등에 멘 칼집',[(-.25,.36,.60),(.26,.36,1.81)],.035,black,'chest')
        line('칼 손잡이',[(.26,.36,1.81),(.36,.36,2.06)],.028,leather,'chest')
        box('칼의 코등이',(.26,.36,1.81),(.17,.075,.035),gold,'chest')
    if gear in ('hammer','tools'):
        line('망치 손잡이',[(.357,-.03,.96),(.39,-.06,.54)],.019,leather,'forearm.R')
        box('모서리를 다듬은 쇠망치',(.39,-.06,.56),(.25,.12,.12),black,'forearm.R')
    if gear=='scarf':
        loft('붉은 목도리',[(0,0,1.65,.14,.14),(0,0,1.74,.15,.15)],trim,'chest',48)
        panel('뒤로 늘어진 목도리',[(.10,.15,1.71),(.20,.17,1.71),(.36,.24,.93),
              (.26,.23,.98)],trim,'chest')
    if gear=='ribbon':
        for sign in [-1,1]:
            panel('머리 장식 리본',[(0,.28,2.07),(sign*.10,.29,2.13),
                (sign*.11,.29,2.02),(0,.28,2.05)],trim,'head')
        ellipsoid('앞섶의 꽃 장식',(.10,-.30,1.53),(.025,.008,.025),gold,'chest',20)
    if gear in ('rope','seals'):
        for j in range(2):
            line('예복 허리 매듭' if gear=='seals' else '허리 밧줄',
                 [(.28*math.cos(a),.27*math.sin(a),1.15+j*.025)
                  for a in [i*math.tau/48 for i in range(49)]],.008,ivory,'hips')
    if spec.get('elder'):
        for sign in [-1,1]:
            for j in range(2):
                line('눈가의 잔주름',[(sign*.14,-.227,1.968-j*.01),
                     (sign*.18,-.205,1.956-j*.01)],.0016,leather)
    if spec['id']=='sealer':
        for sign in [-1,1]:
            for j in range(4):
                x=sign*(.08+j*.045)
                line('예복 금실 자수',[(x,-.29,.37),(x+sign*.025,-.30,.53),
                    (x,-.29,.69),(x+sign*.02,-.29,.86)],.0025,gold,'hips')
            line('관자놀이 머리 장식',[(sign*.21,-.02,2.22),(sign*.26,-.03,2.15),
                 (sign*.24,-.04,2.06)],.008,gold)
    if spec.get('child'):
        for ob in list(scene.objects):
            if ob.name.startswith('연속 바지'):
                ob.data.materials.clear()
                ob.data.materials.append(skin)
            elif ob.name.startswith('가죽 부츠'):
                ob.data.materials.clear()
                ob.data.materials.append(ivory)
        for side,x in [('L',-.17),('R',.17)]:
            ob=loft('아이 반바지 '+side,[(x,0,.64,.12,.15),(x,0,.70,.13,.16),
                (x,0,1.05,.145,.16),(x,0,1.10,.13,.15)],black,'thigh.'+side,32)
            loft('양말 끝 '+side,[(x,0,.32,.09,.11),(x,0,.40,.095,.115)],ivory,'shin.'+side,32)

    # Change the skeleton and all corresponding rest vertices together, before authoring actions.
    female=spec.get('female',False)
    width=spec.get('width',.94 if female else 1.)
    def reshape(v):
        x,y,z=v
        body=z<1.68
        head_width=1.10 if spec['id']=='village_merchant' else .94 if female else 1.
        x*=width if body else head_width
        if spec['id']=='village_merchant' and body:
            y*=1.12
        if female and .95<z<1.50:
            x*=.93+ .07*abs(z-1.25)/.30
        if spec.get('child'):
            x*=.83 if body else 1.03
            y*=.85 if body else 1.
            z=z*.68 if body else 1.68*.68+(z-1.68)*1.04
        return Vector((x,y,z))
    for ob in scene.objects:
        if ob.type=='MESH' and ob.parent==rig:
            for v in ob.data.vertices:
                if '눈썹' in ob.name and spec['id'] in ('hunter','smith','stranger'):
                    v.co.z+=(abs(v.co.x)-.09)*(.17 if spec['id']!='stranger' else -.13)
                if any(part in ob.name for part in ['아몬드형 눈','홍채','동공','눈빛','윗눈꺼풀']):
                    v.co.z=1.996+(v.co.z-1.996)*(.82 if spec.get('elder') else .92 if female else 1.)
                v.co=reshape(v.co)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.object.mode_set(mode='EDIT')
    for b in rig.data.edit_bones:
        b.head=reshape(b.head)
        b.tail=reshape(b.tail)
    bpy.ops.object.mode_set(mode='OBJECT')
    rig.name=spec['label']+'_리그'
    rig['상태']='외형 선택 후보, 게임에 미적용'
    for kind in ['idle','walk']:
        rig.animation_data_create()
        rig.animation_data.action=None
        for frame in range(33):
            t=frame/32
            for b in rig.pose.bones:
                b.matrix_basis.identity()
            if kind=='walk':
                if spec.get('child'):
                    # Keep deformation consistent with the shortened rest skeleton.
                    for sign,offset in [('L',0),('R',.5)]:
                        phase=(t+offset)*math.tau
                        rig.pose.bones['thigh.'+sign].rotation_euler.x=.38*math.cos(phase)
                        rig.pose.bones['shin.'+sign].rotation_euler.x=.45*max(0,math.sin(phase))
                        rig.pose.bones['foot.'+sign].rotation_euler.x=-.14*math.cos(phase)
                        rig.pose.bones['arm.'+sign].rotation_euler.x=-.28*math.cos(phase)
                else:
                    apply_locomotion(rig,kind,t)
            else:
                rig.pose.bones['chest'].rotation_euler.x=.006*math.sin(t*math.tau)
            for b in rig.pose.bones:
                b.keyframe_insert('rotation_euler',frame=frame+1,group=b.name)
                b.keyframe_insert('location',frame=frame+1,group=b.name)
        rig.animation_data.action.name=spec['label']+' · '+('대기' if kind=='idle' else '걷기')
        rig.animation_data.action.use_fake_user=True
    rig.animation_data.action=None
    for b in rig.pose.bones:
        b.matrix_basis.identity()
    if spec['id']=='stranger':
        rig.pose.bones['chest'].rotation_euler.x=.10
        rig.pose.bones['head'].rotation_euler.x=.06
    scene.render.resolution_x,scene.render.resolution_y=400,600
    scene.render.resolution_percentage=100
    scene.eevee.taa_render_samples=32
    scene.render.film_transparent=False
    camera=scene.camera
    camera.data.type='ORTHO'
    camera.data.ortho_scale=2.90
    def set_view(angle):
        target=Vector((0,0,1.29))
        camera.location=target+Vector((6*math.sin(angle),-6*math.cos(angle),.55))
        camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
    target_dir=OUT/spec['id']
    target_dir.mkdir(exist_ok=True)
    set_view(.55)
    bpy.ops.wm.save_as_mainfile(filepath=str(target_dir/'candidate.blend'))
    for label,angle in [('perspective',.55),('profile',math.pi/2),('back',math.pi)]:
        set_view(angle)
        scene.render.filepath=str(target_dir/(label+'.png'))
        bpy.ops.render.render(write_still=True)
    print('NPC_CANDIDATE_READY '+spec['id'],flush=True)


parser=argparse.ArgumentParser()
parser.add_argument('--only')
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
protected=[SOURCE,ROOT/'Art/Blender/CharacterRefinement/protagonist_refined.blend']
protected+=list((ROOT/'SimpleGame/Assets/Characters/Story').glob('*.png'))
protected+=list((ROOT/'SimpleGame/Assets/Characters/Townsfolk').glob('*.png'))
before={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
for design in DESIGNS:
    if args.only is None or args.only==design['id']:
        build(design)
after={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
if before!=after:
    raise RuntimeError('Protected source or runtime art changed')
(OUT/'collection_manifest.json').write_text(json.dumps(
    {'status':'candidates_not_applied','designs':DESIGNS,'protectedBefore':before,
     'protectedAfter':after,'runtimeImagesExported':False,'gameRunTested':False},
    ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('NPC_COLLECTION_READY',flush=True)
