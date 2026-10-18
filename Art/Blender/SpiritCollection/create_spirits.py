"""Author three distinct spirit models and bake eight-direction idle/move/cast clips.

Blender --background --factory-startup --python this_file -- --species pilgrim
Species: pilgrim, hands, saint. Add --source-only to save only the editable source.
No originals, gameplay data or approved lantern sources are overwritten.
"""
import ast
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector

OUT=Path(__file__).resolve().parent
ROOT=OUT.parents[2]
TAU=math.tau
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
SPECIES=args[args.index('--species')+1] if '--species' in args else 'pilgrim'
INFO={'pilgrim':('목 없는 순례자','mourning_bell_4dir.png',1,1.8),
      'hands':('기도하는 손','briar_fox_4dir.png',2,1.6),
      'saint':('기도하지 않는 성자','unpraying_saint_4dir.png',3,2.0)}
NAME,OLD,ID,PERIOD=INFO[SPECIES]
DEST=OUT/SPECIES
DEST.mkdir(parents=True,exist_ok=True)
PROTECTED=[ROOT/'SimpleGame/Assets/Spirits'/value[1] for value in INFO.values()]
PROTECTED += [ROOT/'Art/Blender/SpiritStudy/forgotten_candidate.blend',
              ROOT/'SimpleGame/Assets/Spirits/Forgotten/lantern_a.png']
before={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in PROTECTED}
helpers=ROOT/'Art/Blender/SpiritStudy/create_forgotten.py'
tree=ast.parse(helpers.read_text(encoding='utf-8'))
wanted={'material','attach','mesh','curve','sphere','ring','loft','camera_view','render'}
exec(compile(ast.Module(body=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in wanted],type_ignores=[]),'<spirit mesh helpers>','exec'),globals())
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.context.preferences.filepaths.save_version=0
scene=bpy.context.scene
scene.render.engine='BLENDER_EEVEE'
scene.eevee.taa_render_samples=32
scene.render.resolution_percentage=100
scene.render.film_transparent=True
scene.render.image_settings.file_format='PNG'
scene.render.image_settings.color_mode='RGBA'
scene.view_settings.view_transform='Standard'
scene.world.color=(.14,.14,.14)
scene.render.fps=24
armature=bpy.data.armatures.new(NAME+' · 뼈대')
rig=bpy.data.objects.new(NAME+' · 리그',armature)
scene.collection.objects.link(rig)
bpy.context.view_layer.objects.active=rig
rig.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')


def bone(name,head,tail,parent=None):
    b=armature.edit_bones.new(name);b.head=head;b.tail=tail
    if parent:b.parent=armature.edit_bones[parent]


bone('root',(0,0,0),(0,0,.2))
bone('body',(0,0,1),(0,0,2),'root')
if SPECIES=='hands':
    bone('head',(0,-.45,1.35),(0,-.60,1.93),'body')
    bone('charm',(0,-.70,1.25),(0,-.70,.90),'body')
    for label,x,y in [('front.L',-.27,-.47),('front.R',.27,-.47),('back.L',-.31,.43),('back.R',.31,.43)]:
        bone(label,(x,y,1.08),(x,y,.53),'body')
        bone('paw.'+label,(x,y,.53),(x,y-.18,.20),label)
    for i in range(3):
        bone(f'tail.{i}',(0,.53,1.0),((i-1)*.25,.88,1.51),'body')
        bone(f'tip.{i}',((i-1)*.25,.88,1.51),((i-1)*.40,.97,2.02),f'tail.{i}')
    bone('ear.L',(-.23,-.49,1.90),(-.39,-.42,2.35),'head')
    bone('ear.R',(.23,-.49,1.90),(.39,-.42,2.35),'head')
else:
    bone('head',(0,0,2.40),(0,0,3.20),'body')
    bone('charm',(0,-.38,1.57),(0,-.38,1.18),'body')
    for side,sign in [('L',-1),('R',1)]:
        bone('arm.'+side,(sign*.36,0,2.35),(sign*.78,-.03,1.99),'body')
        bone('hand.'+side,(sign*.78,-.03,1.99),(sign*.90,-.20,1.66),'arm.'+side)
    for i,a in enumerate([-.8,.8,2.15,-2.15]):
        bone(f'cloth.{i}',(.42*math.sin(a),-.35*math.cos(a),2.24),(.48*math.sin(a),-.4*math.cos(a),1.32),'body')
        bone(f'tip.{i}',(.48*math.sin(a),-.4*math.cos(a),1.32),(.64*math.sin(a),-.5*math.cos(a),.35),f'cloth.{i}')
bpy.ops.object.mode_set(mode='OBJECT')
rig.show_in_front=True
for joint in rig.pose.bones:joint.rotation_mode='XYZ'

metal=material('古銅と金属',(.25,.17,.082),True)
gold=material('擦れた金縁',(.49,.32,.13),True)
dark=material('深い影',(.008,.012,.018))
black=material('空洞の暗闇',(.002,.002,.004),glow=.01)
blue=material('冷たい残光',(.015,.45,.62),glow=.8)
purple=material('紫の残光',(.29,.035,.48),glow=.45)
robe=material('墨藍の織布',(.030,.041,.071),True)
edge=material('傷んだ布端',(.10,.115,.14),True)
ivory=material('古い白磁',(.69,.62,.49))
linen=material('薄い生成りの織布',(.49,.45,.40),True)
rope=material('擦れた麻縄',(.27,.22,.13),True)
red=material('朱色の組紐',(.20,.025,.025),True)
fur=material('灰紫の毛並み',(.058,.035,.062),True)
fur_light=material('毛先の紫',(.12,.055,.13),True)
fur_dark=material('暗い毛の根元',(.024,.020,.032),True)
pink=material('耳の内側',(.20,.055,.105))
horn=material('枝角',(.13,.016,.022),True)


def solid(ob,thickness=.012):
    mod=ob.modifiers.new('薄い表面の厚さ','SOLIDIFY');mod.thickness=thickness
    return ob


def tube(name,points,radii,mat,joint='body',weights=None,n=12):
    """Continuous swept surface for limbs, tails and horns; not linked round primitives."""
    vertices=[]
    for j,(p,r) in enumerate(zip(points,radii)):
        p=Vector(p)
        tangent=Vector(points[min(j+1,len(points)-1)])-Vector(points[max(0,j-1)])
        tangent.normalize()
        axis=tangent.cross(Vector((1,0,0)))
        if axis.length<.01:axis=tangent.cross(Vector((0,1,0)))
        axis.normalize();other=tangent.cross(axis).normalized()
        for i in range(n):
            v=p+r*(math.cos(i*TAU/n)*axis+math.sin(i*TAU/n)*other)
            vertices.append(tuple(v))
    faces=[(j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i)
           for j in range(len(points)-1) for i in range(n)]
    faces.extend([tuple(reversed(range(n))),tuple(range((len(points)-1)*n,len(points)*n))])
    return mesh(name,vertices,faces,mat,joint,True,weights)


def medallion(x,y,z,r,joint='charm'):
    curve('丸い護符の縁',[(x+r*math.cos(i*TAU/60),y,z+r*math.sin(i*TAU/60)) for i in range(60)],.013,gold,joint,True)
    vertices=[(x,y+.007,z)]+[(x+r*.91*math.cos(i*TAU/32),y+.007,z+r*.91*math.sin(i*TAU/32)) for i in range(32)]
    mesh('護符の板',vertices,[(0,i+1,(i+1)%32+1) for i in range(32)],metal,joint)
    for a in [0,math.pi/2]:
        curve('護符の刻印',[(x+math.cos(a)*r*.55,y-.012,z+math.sin(a)*r*.55),(x,y-.016,z),
                           (x-math.cos(a)*r*.5,y-.012,z-math.sin(a)*r*.5)],.007,dark,joint)


def robe_panel(index,a,width,top,length,radius,mat,flow=.2):
    nu,nv=22,30
    vertices=[]
    for j in range(nv+1):
        t=j/nv
        for i in range(nu+1):
            u=i/nu
            theta=a+(u-.5)*width*(1-.20*t)+flow*t*t
            r=radius+.25*t+.06*math.sin(t*math.pi)
            r+=.025*math.sin(u*math.pi*12+t*3+index)
            hem=.11*math.cos(u*math.pi*8+index)+.045*math.sin(u*math.pi*19)
            vertices.append((r*math.sin(theta),-r*.83*math.cos(theta)+flow*.5*t*t,
                             top-length*t+hem*t**6))
    faces=[]
    for j in range(nv):
        for i in range(nu):
            k=j*(nu+1)+i
            # Small missing patches near the hem give actual broken silhouettes.
            if j>nv-4 and (i+3*index)%13==0:continue
            faces.append((k,k+1,k+nu+2,k+nu+1))
    groups={f'cloth.{index}':[],f'tip.{index}':[]}
    for k in range(len(vertices)):
        t=(k//(nu+1))/nv;w=max(0,min(1,(t-.28)/.60))
        groups[f'cloth.{index}'].append((k,1-w));groups[f'tip.{index}'].append((k,w))
    solid(mesh('裂けた外套',vertices,faces,mat,f'cloth.{index}',True,groups),.009)
    for side in [0,nu]:
        line=curve('細い縫い端',[vertices[j*(nu+1)+side] for j in range(nv+1)],.004,edge,f'cloth.{index}')
        line.vertex_groups.clear()
        top_group=line.vertex_groups.new(name=f'cloth.{index}');tip_group=line.vertex_groups.new(name=f'tip.{index}')
        for v in line.data.vertices:
            t=max(0,min(1,(top-v.co.z)/length));w=max(0,min(1,(t-.28)/.60))
            if w<1:top_group.add([v.index],1-w,'REPLACE')
            if w>0:tip_group.add([v.index],w,'REPLACE')


def fingers(side,center,length=.25):
    sign=-1 if side=='L' else 1
    x,y,z=center
    sphere('指の付け根',center,(.085,.047,.10),metal,'hand.'+side,24,12)
    for i in range(4):
        xx=x+(i-1.5)*.040
        curve('曲がった長い指',[(xx,y-.015,z),(xx+sign*.025,y-.10,z-.09),
                              (xx+sign*.015,y-.15,z-length),(xx,y-.10,z-length-.03)],
              .017,gold,'hand.'+side)


def bell(name,center,scale,joint='head',cracked=False):
    cx,cy,cz=center
    levels=[(0,.50),(.055,.56),(.14,.47),(.47,.41),(.73,.29),(.81,.10)]
    n=64;verts=[];faces=[]
    for h,r in levels:
        for i in range(n):
            a=i*TAU/n
            rr=r*(1+.008*math.sin(a*15))
            verts.append((cx+scale*rr*math.sin(a),cy-scale*rr*math.cos(a),cz+scale*h))
    for j in range(len(levels)-1):
        for i in range(n):
            # Irregular vertical crack and narrow, arched eye vents are genuine gaps.
            split=cracked and i in [1+(j%3),2+(j%3)]
            vent=cracked and j in [1,2] and i in [n-7,n-6,6,7]
            if not(split or vent):faces.append((j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i))
    ob=solid(mesh(name,verts,faces,metal,joint,True),.018*scale)
    for h,r in [levels[1],levels[3],levels[4]]:
        curve('鐘の打ち出し縁',[(cx+scale*r*math.sin(i*TAU/80),cy-scale*r*math.cos(i*TAU/80),cz+scale*h) for i in range(80)],.012*scale,gold,joint,True)
    curve('鐘の吊り環',[(cx+.095*scale*math.cos(i*TAU/56),cy,cz+scale*(.91+.11*math.sin(i*TAU/56))) for i in range(56)],.020*scale,metal,joint,True)
    for k in range(24):
        a=k*TAU/24
        curve('古い刻み目',[(cx+scale*.545*math.sin(a),cy-scale*.545*math.cos(a),cz+scale*.057),
                           (cx+scale*.526*math.sin(a+.02),cy-scale*.526*math.cos(a+.02),cz+scale*.081)],.006*scale,dark,joint)
    return ob


def make_pilgrim():
    loft('首のない胸',[(1.35,.34,.29),(1.70,.43,.32),(2.15,.44,.32),(2.40,.24,.23)],robe,n=48,smooth=True)
    # The dim inner volume is under a hollow bell, rather than a head wearing a bell hat.
    sphere('鐘の下の暗い空洞',(0,0,2.49),(.32,.28,.30),black,'head')
    sphere('失った首の青い光',(0,-.19,2.40),(.13,.07,.12),blue,'head')
    bell('割れた巡礼の鐘',(0,0,2.39),1.12,'head',True)
    for sign in [-1,1]:
        sphere('鐘の奥の残光',(sign*.17,-.38,2.69),(.040,.030,.085),blue,'head',20,12)
    for i,a in enumerate([-.8,.8,2.15,-2.15]):robe_panel(i,a,1.75,2.04,1.43,.39,robe,.12)
    # Crossed collars are narrow curved cloth, not two blocks over the chest.
    for sign in [-1,1]:
        solid(mesh('重なった襟',[(sign*.34,-.27,2.30),(sign*.42,-.30,2.11),
                              (-sign*.22,-.39,1.77),(-sign*.31,-.36,1.90)],[(0,1,2,3)],edge),.012)
        side='L' if sign<0 else 'R'
        pts=[(sign*.35,0,2.20),(sign*.57,-.05,2.07),(sign*.74,-.12,1.90),(sign*.80,-.15,1.73)]
        sleeve=tube('広い袂',pts,[.17,.20,.22,.23],robe,'arm.'+side,n=32)
        # Dark cuff and fine fold seams give the hanging sleeve real depth.
        sphere('袖口の影',(sign*.80,-.17,1.73),(.18,.17,.06),black,'arm.'+side)
        fingers(side,(sign*.80,-.21,1.70),.22)
        for k in range(5):
            curve('袖の折り目',[(p[0]+(k-2)*.04,p[1]-.17,p[2]+.02*math.sin(k+j)) for j,p in enumerate(pts)],.005,edge,'arm.'+side)
    for strand in range(3):
        points=[]
        for i in range(180):
            a=i*TAU/180
            r=.45+.010*math.cos(a*32+strand*TAU/3)
            points.append((r*math.sin(a),-r*.82*math.cos(a),1.64+.010*math.sin(a*32+strand*TAU/3)))
        curve('三つ編みの腰縄',points,.019,rope,cyclic=True)
    for sign in [-1,1]:
        curve('結んだ麻縄',[(sign*.02,-.39,1.65),(sign*.10,-.43,1.55),(sign*.08,-.44,1.30),(sign*.17,-.42,1.12)],.022,rope,'charm')
        for i in range(4):curve('縄の房',[(sign*.17,-.42,1.14),(sign*(.13+i*.025),-.44,1.01+.02*math.sin(i))],.008,rope,'charm')
    medallion(.20,-.40,1.51,.105)


def fur_lock(name,center,direction,width,length,mat,joint):
    p=Vector(center);d=Vector(direction).normalized()
    side=d.cross(Vector((0,-1,0)))
    if side.length<.1:side=Vector((1,0,0))
    side.normalize()
    normal=d.cross(side).normalized()
    vertices=[]
    for j in range(7):
        t=j/6
        w=width*math.sin(math.pi*(.12+.88*t))*(1-.72*t)
        mid=p+d*(length*t)+normal*(.035*math.sin(t*math.pi))
        vertices.extend([tuple(mid-side*w),tuple(mid+normal*w*.32),tuple(mid+side*w)])
    faces=[(j*3+i,j*3+i+1,j*3+i+4,j*3+i+3) for j in range(6) for i in range(2)]
    return solid(mesh(name,vertices,faces,mat,joint),.008)


def make_fox():
    sphere('しなやかな胴',(0,.08,1.03),(.38,.66,.43),fur,'body',48,28)
    sphere('肩の奥行き',(0,-.45,1.30),(.39,.37,.43),fur_dark,'body',40,24)
    sphere('面の後ろの頭',(0,-.48,1.73),(.34,.32,.39),fur,'head',40,24)
    # Layered narrow fur sheets, laid along the anatomy rather than round fur balls.
    for layer,z in enumerate([1.70,1.53,1.36]):
        for i in range(24):
            a=i*TAU/24
            p=(.35*math.sin(a),-.43-.28*math.cos(a),z)
            fur_lock('首の重なった毛',p,(.45*math.sin(a),-.10*math.cos(a),-.8),.07,.27,fur_light if (i+layer)%4==0 else fur,'head' if layer==0 else 'body')
    for label,x,y in [('front.L',-.27,-.47),('front.R',.27,-.47),('back.L',-.31,.43),('back.R',.31,.43)]:
        tube('連続した脚',[(x,y,1.03),(x,y+.025,.78),(x,y-.03,.47),(x,y-.10,.28)],
             [.14,.115,.072,.082],fur,label,n=24)
        sphere('足先',(x,y-.16,.24),(.115,.19,.09),fur_dark,'paw.'+label,24,12)
        for digit in range(3):
            curve('爪先の割れ目',[(x+(digit-1)*.055,y-.315,.25),(x+(digit-1)*.055,y-.27,.29)],.005,dark,'paw.'+label)
    # Three swept, curling tails each carry two changing bone weights.
    for tail in range(3):
        sign=tail-1
        points=[];radii=[]
        for j in range(27):
            t=j/26
            points.append((sign*.16+sign*.43*math.sin(t*math.pi*.8)+sign*.15*t**4,
                           .54+.64*math.sin(t*math.pi*.83)-.20*t**5,
                           1.0+1.03*t+.13*math.sin(t*math.pi)))
            radii.append(.006+(.07+.24*math.sin(t*math.pi)**.8)*(1-t)**.45)
        groups={f'tail.{tail}':[],f'tip.{tail}':[]}
        for j in range(27):
            w=max(0,min(1,(j/26-.32)/.6))
            for k in range(24):
                groups[f'tail.{tail}'].append((j*24+k,1-w));groups[f'tip.{tail}'].append((j*24+k,w))
        tube('曲がった大きな尾',points,radii,fur if tail==1 else fur_light,f'tail.{tail}',groups,24)
        for j in range(4,23,3):
            for side in [-1,1]:
                p=points[j]
                fur_lock('尾の流れる毛先',(p[0]+side*radii[j]*.5,p[1]-.10,p[2]),
                         (side*.40,.20,.55),.055,.23,fur_light if j%2 else fur,f'tip.{tail}' if j>13 else f'tail.{tail}')
    for sign,side in [(-1,'L'),(1,'R')]:
        # Ears are thick folded shells with a recessed inner face.
        vertices=[(sign*.18,-.53,1.92),(sign*.48,-.40,2.39),(sign*.48,-.28,1.86),
                  (sign*.18,-.43,1.92),(sign*.48,-.30,2.39),(sign*.48,-.18,1.86)]
        mesh('厚みのある耳',vertices,[(0,1,2),(5,4,3),(0,3,4,1),(1,4,5,2),(2,5,3,0)],fur_dark,'ear.'+side)
        mesh('耳の内側',[(sign*.22,-.535,1.96),(sign*.45,-.415,2.30),(sign*.43,-.32,1.94)],[(0,1,2)],pink,'ear.'+side)
        pts=[(sign*.15,-.40,1.97),(sign*.19,-.35,2.21),(sign*.27,-.31,2.37),
             (sign*.24,-.28,2.54),(sign*.32,-.25,2.66)]
        tube('枝分かれした赤い角',pts,[.048,.043,.035,.021,.003],horn,'head',n=16)
        tube('小さい枝角',pts[1:3]+[(sign*.38,-.27,2.42)], [.033,.025,.003],horn,'head',n=12)
    # The tapered muzzle is part of one curved mask shell with a deep side profile.
    verts=[];faces=[]
    for j in range(25):
        t=j/24
        z=1.36+.68*t
        width=.018+.31*math.sin(math.pi*t)**.75
        for i in range(25):
            u=i/24*2-1
            y=-.99+.34*t+.075*u*u
            verts.append((u*width,y,z))
    for j in range(24):
        for i in range(24):
            k=j*25+i;faces.append((k,k+1,k+26,k+25))
    solid(mesh('白い狐面',verts,faces,ivory,'head'),.022)
    for sign in [-1,1]:
        x=sign*.165
        vertices=[(x-sign*.10,-.816,1.74),(x+sign*.115,-.774,1.80),
                  (x+sign*.065,-.797,1.70),(x-sign*.05,-.830,1.69)]
        solid(mesh('面の細い眼窩',vertices,[(0,1,2,3)],black,'head'),.01)
        sphere('紫の瞳',(x,-.830,1.734),(.039,.015,.021),purple,'head',20,12)
    curve('鼻先',[(0,-1.007,1.43),(-.027,-.991,1.46),(.027,-.991,1.46)],.009,dark,'head',True)
    for j in range(4):
        z=1.83+j*.047;y=-.99+.34*((z-1.36)/.68)-.006
        curve('面の朱印',[(0,y-.025,z+.025),(-.025,y-.025,z),(0,y-.025,z-.025),(.025,y-.025,z),(0,y-.025,z+.025)],.008,red,'head')
    curve('首の赤い結び',[(.25*math.cos(i*TAU/80),-.63+.29*math.sin(i*TAU/80),1.28) for i in range(80)],.022,red,cyclic=True)
    medallion(0,-.91,1.20,.067,'charm')
    solid(mesh('胸の小さな札',[(-.055,-.90,1.10),(.055,-.90,1.10),(.050,-.91,.88),(-.05,-.91,.88)],[(0,1,2,3)],linen,'charm'),.008)
    for j in range(4):curve('札の短い筆跡',[(-.025,-.92,1.06-j*.045),(.023,-.92,1.05-j*.045)],.005,red,'charm')


def make_saint():
    violet=material('深紫の裏衣',(.075,.024,.12),True)
    white=material('破れた白い祭服',(.57,.53,.50),True)
    loft('空洞を包む祭服',[(.50,.52,.40),(1.18,.39,.31),(2.10,.29,.25),(2.50,.39,.30),(2.71,.20,.18)],violet,n=48,smooth=True)
    for i,a in enumerate([-.8,.8,2.15,-2.15]):
        robe_panel(i,a,1.65,2.40,1.99,.39,violet,.25)
        robe_panel(i,a+.12,1.02,2.48,1.94,.42,white,.38)
    # A curved open hood shelters darkness; there is no skin-colored head inside it.
    verts=[];faces=[]
    for j in range(23):
        t=j/22
        for i in range(37):
            a=.72+(TAU-1.44)*i/36
            rx=.10+.28*math.sin(t*math.pi*.85)
            verts.append((rx*math.sin(a),-.85*rx*math.cos(a),3.27-.77*t))
    for j in range(22):
        for i in range(36):
            k=j*37+i;faces.append((k,k+1,k+38,k+37))
    solid(mesh('厚みのある白いフード',verts,faces,white,'head'),.018)
    sphere('顔のない暗部',(0,-.03,2.94),(.18,.14,.23),black,'head',32,20)
    for sign in [-1,1]:
        curve('フードの細い縁',[(sign*.07,-.09,3.26),(sign*.16,-.22,3.10),
                              (sign*.25,-.24,2.86),(sign*.35,-.25,2.60)],.017,linen,'head')
        side='L' if sign<0 else 'R'
        tube('流れる広い袖',[(sign*.32,0,2.40),(sign*.58,-.03,2.31),
                            (sign*.79,-.04,2.16)],[.17,.25,.34],white,'arm.'+side,n=36)
        sphere('袖の中の暗闇',(sign*.80,-.08,2.12),(.24,.15,.07),violet,'arm.'+side)
        fingers(side,(sign*.81,-.13,2.05),.16)
        # Three hanging chains support an actual hollow censer bell.
        for spread in [-1,0,1]:
            curve('垂れ下がった鎖',[(sign*.86+spread*.06*(j/12),-.12,2.00-j*.032) for j in range(13)],.007,gold,'hand.'+side)
        bell('手に吊った香鐘',(sign*.86,-.12,1.33),.32,'hand.'+side)
    # Halo sits in the XZ plane with depth, separate radial pins and nested wire rings.
    for r,tube_size in [(.57,.022),(.65,.014),(.72,.010)]:
        curve('金属の後光',[(r*math.cos(i*TAU/128),.19,3.03+r*math.sin(i*TAU/128)) for i in range(128)],tube_size,gold,'head',True)
    for i in range(20):
        a=i*TAU/20
        r=.81 if i%2==0 else .76
        curve('後光の細い棘',[(.55*math.cos(a),.19,3.03+.55*math.sin(a)),
                            (r*math.cos(a),.19,3.03+r*math.sin(a))],.011,metal,'head')
        sphere('後光の小さな鋲',(.65*math.cos(a),.18,3.03+.65*math.sin(a)),(.021,.018,.021),gold,'head',12,8)
    for sign in [-1,1]:
        curve('胸を渡る飾り鎖',[(sign*.18*(1-t),-.29-.04*math.sin(t*math.pi),2.49-.28*t) for t in [i/24 for i in range(25)]],.011,gold)
    medallion(0,-.34,2.27,.09,'body')
    # Narrow modeled haze curls stay behind the cloth, avoiding a flat background image.
    for side,sign in [('L',-1),('R',1)]:
        for k in range(3):
            pts=[]
            for j in range(25):
                t=j/24
                pts.append((sign*(.47+.45*math.sin(t*math.pi*.9)),.22+.24*t,2.24-1.45*t+k*.10))
            tube('紫の霊気の筋',pts,[.004+.019*math.sin(j/24*math.pi) for j in range(25)],purple,f'tip.{2 if sign>0 else 3}',n=10)


if SPECIES=='pilgrim':make_pilgrim()
elif SPECIES=='hands':make_fox()
else:make_saint()

for name,location,power,size,color in [('主光',(-3,-4,7),420,4,(1,.89,.76)),('縁光',(3,2,5),380,3,(.50,.72,1)),('補助',(2,-3,3),70,4,(1,.70,.40))]:
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size;data.color=color
    ob=bpy.data.objects.new(name,data);scene.collection.objects.link(ob);ob.location=location
    ob.rotation_euler=(Vector((0,0,2))-ob.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('共通の直交カメラ')
camera=bpy.data.objects.new('共通の直交カメラ',data);scene.collection.objects.link(camera)
scene.camera=camera;camera.data.type='ORTHO'


def pose(kind,phase):
    a=TAU*phase
    for b in rig.pose.bones:
        b.location=(0,0,0);b.rotation_euler=(0,0,0);b.scale=(1,1,1)
    cast=math.sin(math.pi*phase) if kind=='cast' else 0
    if SPECIES=='hands':
        rig.pose.bones['body'].location.y=.018*math.sin(a*2) if kind=='glide' else .009*math.sin(a)
        rig.pose.bones['head'].rotation_euler.x=.035*math.sin(a-.5)-.16*cast
        for i,label in enumerate(['front.L','front.R','back.L','back.R']):
            swing=math.sin(a+(0 if i in [0,3] else math.pi)) if kind=='glide' else 0
            rig.pose.bones[label].rotation_euler.x=.32*swing
            rig.pose.bones['paw.'+label].rotation_euler.x=-.18*swing
            if label.startswith('front'):
                rig.pose.bones[label].rotation_euler.x-=.65*cast
                rig.pose.bones['paw.'+label].rotation_euler.z+=(.14 if label.endswith('L') else -.14)*cast
        for i in range(3):
            rig.pose.bones[f'tail.{i}'].rotation_euler.y=.10*math.sin(a-i*.5)+.12*cast
            rig.pose.bones[f'tip.{i}'].rotation_euler.z=.12*math.sin(a-i*.5-.8)
        rig.pose.bones['ear.L'].rotation_euler.y=.04*math.sin(a)
        rig.pose.bones['ear.R'].rotation_euler.y=-.04*math.sin(a-.4)
    else:
        rig.pose.bones['body'].location.y=.035*math.sin(a)
        rig.pose.bones['body'].rotation_euler.x=.020*math.sin(a)-.04*cast
        rig.pose.bones['head'].rotation_euler.y=.035*math.sin(a-.4)
        rig.pose.bones['head'].rotation_euler.x=(.08 if SPECIES=='pilgrim' else -.06)*cast
        for side,sign in [('L',-1),('R',1)]:
            rig.pose.bones['arm.'+side].rotation_euler.x=.03*math.sin(a-.5)-.55*cast
            rig.pose.bones['arm.'+side].rotation_euler.y=sign*(.03*math.cos(a)+.13*cast)
            rig.pose.bones['hand.'+side].rotation_euler.z=sign*(.06*math.sin(a-1)+.25*cast)
        for i in range(4):
            rig.pose.bones[f'cloth.{i}'].rotation_euler.x=(.06 if kind=='glide' else 0)+.025*math.sin(a+i*.5)
            rig.pose.bones[f'tip.{i}'].rotation_euler.x=(.08 if kind=='glide' else 0)+.055*math.sin(a+i*.5-.8)-.08*cast
    rig.pose.bones['charm'].rotation_euler.x=.05*math.sin(a-.8)
    bpy.context.view_layer.update()


CLIPS=[('idle',0,12),('glide',12,12),('cast',24,8)]
for kind,first,count in CLIPS:
    rig.animation_data_create();rig.animation_data.action=None
    for frame in range(33):
        pose(kind,frame/32)
        for b in rig.pose.bones:
            for channel in ['location','rotation_euler','scale']:b.keyframe_insert(channel,frame=frame+1,group=b.name)
    rig.animation_data.action.name=NAME+' · '+{'idle':'대기','glide':'이동','cast':'시전'}[kind]
    rig.animation_data.action.use_fake_user=True
    for fcurve in rig.animation_data.action.layers[0].strips[0].channelbag(rig.animation_data.action_slot).fcurves:
        for key in fcurve.keyframe_points:key.interpolation='LINEAR'
rig.animation_data.action=None
pose('idle',0);camera_view(.42)
scene.frame_start=1;scene.frame_end=33
scene.render.resolution_x=560;scene.render.resolution_y=700
bpy.ops.wm.save_as_mainfile(filepath=str(DEST/'model.blend'))
if '--source-only' not in args:
    for name,angle in [('front',0),('perspective',.42),('side',math.pi/2),('back',math.pi)]:
        rig.rotation_euler.z=0;pose('idle',0);camera_view(angle);render(DEST/f'{name}.png')
        print('SPIRIT_VIEW',SPECIES,name,flush=True)
    motion=DEST/'Motion';motion.mkdir(exist_ok=True)
    camera_view(.42)
    for frame in range(16):pose('glide',frame/16);render(motion/f'move_{frame:02d}.png',320,400)
    frames=DEST/'Frames';frames.mkdir(exist_ok=True)
    camera_view(0)
    yaws=[]
    for row in range(8):
        phi=row*TAU/8
        yaw=math.atan2(-math.sin(phi),math.cos(phi)/math.sin(math.atan2(4.05,8)))
        yaws.append(yaw);rig.rotation_euler.z=yaw
        for kind,first,count in CLIPS:
            for frame in range(count):
                pose(kind,frame/(count-1) if kind=='cast' else frame/count)
                render(frames/f'd{row}_f{first+frame:02d}.png',128,128)
        print('SPIRIT_DIRECTION',SPECIES,row+1,'/8',flush=True)
else:yaws=[]
after={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in PROTECTED}
if before!=after:raise RuntimeError('A protected original changed')
report={'species':SPECIES,'id':ID,'name':NAME,'status':'rendered_for_integration' if yaws else 'source_only',
        'cellSize':128,'columns':32,'rows':8,'clips':{kind:{'first':first,'frames':count} for kind,first,count in CLIPS},
        'directionRows':list(range(8)),'directionYaws':yaws,'loopSeconds':PERIOD,'bones':len(rig.data.bones),
        'worldOriginAnchorY':.5+math.cos(math.atan2(4.05,8))*1.92/4.8,
        'protectedBefore':before,'protectedAfter':after,'gameBuildRunTested':False}
(DEST/'render_manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('SPIRIT_MODEL_READY',SPECIES,flush=True)
