"""NPC source variations of the shared Blender character rig.
Usage: blender --background --python Art/Blender/create_npcs.py -- --npc npc1
Add --preview-only to render the model preview without sprite atlases.
"""
import sys
import math
import runpy
from pathlib import Path
import bpy

args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
style = args[args.index('--npc') + 1] if '--npc' in args else 'npc1'
if style not in ('npc1', 'npc2'):
    raise ValueError('NPC는 npc1 또는 npc2를 지정하세요.')


def customize(ctx):
    material = ctx['material']
    ellipsoid = ctx['ellipsoid']
    loft = ctx['loft']
    seam = ctx['seam']
    lock = ctx['lock']
    surface = ctx['surface']
    bind = ctx['bind']
    rig = ctx['rig']
    coat, black, skin = ctx['coat'], ctx['black'], ctx['skin']
    first = style == 'npc1'
    rig.name = '미라_리그' if first else '라엔_리그'
    rig.data.name = 'NPC_공통뼈대'

    def color(mat, rgb, roughness=.7, metallic=0):
        mat.diffuse_color = (*rgb, 1)
        shader = mat.node_tree.nodes.get('Principled BSDF')
        shader.inputs['Base Color'].default_value = (*rgb, 1)
        shader.inputs['Roughness'].default_value = roughness
        shader.inputs['Metallic'].default_value = metallic

    color(coat, (.075, .063, .073) if first else (.048, .065, .084))
    color(ctx['edge'], (.26, .17, .12) if first else (.26, .058, .062))
    color(ctx['metal'], (.52, .34, .15), .35, .6)
    color(skin, (.82, .63, .54) if first else (.77, .57, .48), .58)
    brown = material('붉은 밤색 머리' if first else '분홍색 머리', (.27, .105, .064) if first else (.66, .27, .29))
    highlight = material('머리 밝은 결', (.39, .17, .095) if first else (.87, .44, .44))
    shade = material('머리 어두운 결', (.13, .054, .041) if first else (.33, .10, .13))
    leather = material('적갈색 가죽', (.17, .075, .042))
    red = material('붉은 원단', (.36, .035, .046))
    gold = ctx['metal']
    ivory = material('부적 종이', (.72, .62, .43))
    whites = material('눈 흰자', (.78, .75, .69))
    iris = material('갈색 홍채', (.21, .083, .025))

    # Remove protagonist-only shapes rather than recoloring the same character.
    remove = ['주름진 안대', '안대 봉제선', '머리 바탕', '흐르는 은발', '옆으로 흐르는 앞머리',
              '옆머리', '등 주술 자수', '접힌 높은 깃', '깃 스티치']
    if first:
        remove += ['곡면 코트 자락', '코트 가장자리']
    for ob in list(bpy.data.objects):
        if any(ob.name.startswith(prefix) for prefix in remove):
            bpy.data.objects.remove(ob, do_unlink=True)

    # Iris, upper lash, eyebrow and lower lid remain readable at sprite scale.
    for sign in [-1, 1]:
        x = sign * .087
        ellipsoid('눈매', (x, -.181, 1.998), (.056, .019, .031 if first else .026), black, 'head', 20)
        ellipsoid('눈 흰자', (x, -.19, 1.998), (.047, .015, .025 if first else .020), whites, 'head', 20)
        ellipsoid('홍채', (x + .004, -.207, 1.997), (.020, .006, .023 if first else .020), iris, 'head', 20)
        ellipsoid('동공', (x + .004, -.212, 1.999), (.009, .003, .016), black, 'head', 16)
        ellipsoid('눈 반사광', (x -.005, -.216, 2.008), (.006, .002, .006), whites, 'head', 12)
        seam('눈썹', [(x-sign*.047, -.181, 2.041), (x, -.19, 2.05), (x+sign*.045, -.172, 2.042)], .007, shade, 'head')
        seam('아래 눈꺼풀', [(x-.04,-.187,1.976),(x,-.197,1.967),(x+.04,-.187,1.976)],.0025,leather,'head')

    if first:
        # A continuous bob cap with a high forehead opening and low sides/back.
        vertices, faces = [], []
        columns, rows = 48, 12
        for j in range(rows):
            t = (j+.01)/(rows-1+.01)
            for i in range(columns):
                a = 2*math.pi*i/columns
                bottom = 2.075 if math.cos(a) > .6 else 1.84
                radius = math.sin(t*1.65)
                vertices.append((math.sin(a)*.265*radius, -.005-math.cos(a)*.235*radius, 2.255+(bottom-2.255)*t))
        for j in range(rows-1):
            for i in range(columns):
                faces.append((j*columns+i,j*columns+(i+1)%columns,(j+1)*columns+(i+1)%columns,(j+1)*columns+i))
        surface('단발 머리 바탕',vertices,faces,brown,'head')
        ellipsoid('단발 정수리',(0,.015,2.19),(.15,.14,.072),brown,'head',32)
        for i in range(29):
            a = .70 + (2*math.pi-1.4)*i/28
            sx, cy = math.sin(a), -math.cos(a)
            lock('단발 층', [(sx*.1,cy*.09,2.235), (sx*.29,cy*.25,2.19),
                  (sx*.30,cy*.267,1.94), (sx*.246,cy*.228,1.80+.025*math.sin(i*2))], .042, highlight if i%4==0 else brown)
        for i in range(9):
            x=-.19+i*.044
            lock('옆가르마 앞머리',[(x*.68,-.025,2.18),(x+.045,-.17,2.24),
                  (x+.095,-.225,2.10),(x+.02,-.214,2.035- .035*(1-i/8))], .048, highlight if i%3==0 else brown)
        for i in range(7):
            x=(i-3)*.031
            lock('정수리 가르마',[(x-.045,.005,2.235),(x-.025,-.02,2.273),
                 (x+.045,-.10,2.258),(x+.095,-.17,2.205)],.026,highlight if i%3==0 else brown)
        # Short layered skirt: the trousers and boots remain visible below.
        for layer in range(2):
            verts=[]
            columns=49
            for j in range(6):
                t=j/5
                for i in range(columns):
                    a=.17+(2*math.pi-.34)*i/(columns-1)
                    radius=.248+.10*t-layer*.014
                    fold=.009*math.cos(a*14)*t
                    verts.append((math.sin(a)*(radius+fold),-math.cos(a)*(radius*.72+fold),1.14-(.32+layer*.065)*t+.015*math.sin(a*7)*t))
            faces=[(j*columns+i,j*columns+i+1,(j+1)*columns+i+1,(j+1)*columns+i) for j in range(5) for i in range(columns-1)]
            ob=surface('겹친 치마',verts,faces,red if layer else coat,'hips')
            solid=ob.modifiers.new('치마 두께','SOLIDIFY');solid.thickness=.009
        loft('가죽 조끼',[(0,-.012,1.20,.254,.173),(0,-.012,1.30,.272,.184),(0,-.012,1.49,.301,.192),(0,-.012,1.55,.307,.178)],leather,'chest')
        seam('목깃', [(-.12,-.08,1.7),(0,-.13,1.66),(.12,-.08,1.7)], .024,coat,'chest')
        seam('어깨 가죽띠',[(-.22,-.13,1.57),(0,-.207,1.37),(.22,-.15,1.19)], .022,leather,'chest')
        # Hammer grip and beveled iron head, attached to the carrying forearm.
        seam('망치 손잡이',[(.357,-.01,.96),(.43,-.045,.49)],.021,leather,'forearm.R')
        for j in range(7):
            z=.78+j*.021
            seam('손잡이 감개',[(.379,-.066,z),(.407,-.055,z+.009)],.007,gold,'forearm.R')
        bpy.ops.mesh.primitive_cube_add(size=1, location=(.435,-.05,.49))
        ob=bpy.context.object;ob.scale=(.265,.12,.12)
        bind(ob,'각진 철제 망치',black,'forearm.R')
        bevel=ob.modifiers.new('망치 모서리','BEVEL');bevel.width=.014;bevel.segments=3
        for x in [.32,.55]:
            ellipsoid('망치 금속 테두리',(x,-.05,.49),(.018,.072,.073),gold,'forearm.R',12)
        # Pouches and hanging paper seals with red calligraphic marks.
        for side in [-1,1]:
            x=side*.26
            ellipsoid('허리 가죽 주머니',(x,-.025,1.055),(.067,.092,.105),leather,'hips')
            seam('주머니 덮개',[(x-.05,-.101,1.08),(x,-.118,1.05),(x+.05,-.101,1.08)],.006,gold,'hips')
            for k in range(2):
                x=side*(.22+k*.058)
                ob=surface('종이 부적',[(x-.022,-.16,.98),(x+.022,-.16,.98),(x+.035,-.18,.78),(x-.009,-.18,.78)],[(0,1,2,3)],ivory,'hips')
                solid=ob.modifiers.new('종이 두께','SOLIDIFY');solid.thickness=.003
                seam('부적 붉은 문자',[(x,-.164,.953),(x+.012,-.172,.90),(x-.006,-.178,.86),(x+.01,-.182,.805)],.004,red,'hips')
        for x in [-.10,0,.10]:
            ellipsoid('조끼 징',(x,-.204,1.39),(.009,.005,.009),gold,'chest',12)
    else:
        # Closely cropped dark sides beneath a dense pink crown.
        loft('짧은 옆머리',[(0,.022,2.033,.233,.19),(0,.018,2.12,.246,.20),(0,.018,2.18,.19,.17)],shade,'head')
        ellipsoid('분홍 머리 바탕',(0,.018,2.155),(.244,.208,.14),brown,'head',24)
        for i in range(52):
            a=i*2.39996
            radius=.055+.15*(i%7)/6
            x=math.sin(a)*radius;y=math.cos(a)*radius*.85
            crown=2.155+.14*math.sqrt(max(0,1-(radius/.245)**2))
            lock('짧고 굽은 분홍 머리',[(x,y,crown-.015),(x+math.sin(a)*.025,y+math.cos(a)*.02,crown+.045),
                 (x+math.sin(a+.8)*.055,y+math.cos(a+.8)*.045,crown+.075),
                 (x+math.sin(a+.8)*.08,y+math.cos(a+.8)*.06,crown+.035+.018*(i%3))], .033,highlight if i%3 else brown)
        for i in range(9):
            x=(i-4)*.047
            lock('짧은 앞머리',[(x,-.13,2.19),(x-.02,-.185,2.17),
                 (x+.018,-.218,2.12),(x+.009,-.202,2.064)],.030,brown if i%2 else highlight)
        for row in range(4):
            y=-.17+row*.046
            for col in range(7):
                x=(col-3)*.053
                crown=2.155+.14*math.sqrt(max(0,1-(x/.244)**2-(y/.208)**2))
                lock('앞 정수리 짧은 결',[(x,y,crown-.025),(x-.015,y-.012,crown+.052),
                     (x+.02,y-.036,crown+.075),(x+.03,y-.055,crown+.035)],.032,highlight if (row+col)%3 else brown)
        # Folded neck wrap and hollow draped hood on the upper back.
        loft('붉은 목도리',[(0,0,1.64,.147,.124),(0,0,1.665,.176,.148),
             (0,.008,1.71,.168,.142),(0,.008,1.747,.121,.102)],red,'chest')
        for j in range(3):
            seam('목도리 접힘',[(-.12,-.085,1.666+j*.023),(0,-.15,1.653+j*.022),(.12,-.087,1.677+j*.022)],.008,ctx['edge'],'chest')
        verts=[]
        for j in range(12):
            t=j/11
            for i in range(25):
                a=-math.pi/2+math.pi*i/24
                verts.append((math.sin(a)*(.17+.06*t),.10+.13*math.cos(a)*math.sin(t*math.pi/2),1.72-.27*t+.03*math.cos(a)))
        faces=[(j*25+i,j*25+i+1,(j+1)*25+i+1,(j+1)*25+i) for j in range(11) for i in range(24)]
        ob=surface('등에 내려진 후드',verts,faces,red,'chest')
        solid=ob.modifiers.new('후드 두께','SOLIDIFY');solid.thickness=.022
        sub=ob.modifiers.new('후드 곡면','SUBSURF');sub.levels=1
        seam('후드 테두리',[verts[11*25+i] for i in range(25)],.012,ctx['edge'],'chest')
        for side,x in [('L',-.17),('R',.17)]:
            loft('붉은 부츠 테두리 '+side,[(x,-.10,.043,.112,.203),(x,-.10,.064,.112,.203)],red,'foot.'+side)
            loft('발목 붉은 띠 '+side,[(x,0,.28,.091,.10),(x,0,.32,.091,.10)],red,'foot.'+side)
            ax=x*2.1
            loft('붉은 소매끝 '+side,[(ax,0,.974,.079,.092),(ax,0,1.003,.081,.094)],red,'forearm.'+side)
            for z in [.15,.18,.21]:
                seam('붉은 부츠 끈',[(x-.05,-.137,z),(x+.05,-.137,z+.018)],.006,red,'foot.'+side)
        # Double-breasted fastening and red back insignia.
        for x in [-.07,.07]:
            for j in range(3):
                ellipsoid('재킷 황동 단추',(x,-.191,1.30+j*.093),(.014,.009,.014),gold,'chest',12)
        seam('후드 끈 왼쪽',[(-.084,-.127,1.68),(-.075,-.194,1.48)],.006,black,'chest')
        seam('후드 끈 오른쪽',[(.084,-.127,1.68),(.08,-.194,1.47)],.006,black,'chest')

    # A distinct embroidered crest on each character's back.
    crest=gold if first else red
    z=1.49 if first else 1.36
    for points in [[(0,.192,z-.1),(0,.197,z+.06)],[(-.045,.196,z),(.045,.196,z)],
                   [(-.034,.194,z+.028),(0,.20,z+.06),(.034,.194,z+.028)]]:
        seam('등 문장 자수',points,.005,crest,'chest')
    # Bake subdivision/bevel in the rest pose to avoid rebuilding surfaces per frame.
    # Armature stays live, with interpolated skin weights on the baked vertices.
    for ob in list(bpy.data.objects):
        if ob.type!='MESH':
            continue
        bpy.context.view_layer.objects.active=ob
        ob.select_set(True)
        for modifier in list(ob.modifiers):
            if modifier.type != 'ARMATURE':
                bpy.ops.object.modifier_apply(modifier=modifier.name)
        ob.select_set(False)


runpy.run_path(str(Path(__file__).with_name('create_protagonist.py')),
               init_globals={'ASSET_NAME':style,'CUSTOMIZE':customize})
