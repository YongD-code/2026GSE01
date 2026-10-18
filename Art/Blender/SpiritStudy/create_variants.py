"""Build two independent, whole-character lantern-spirit alternatives.

Blender --background --factory-startup --python this_file -- --variant round_lantern
Blender --background --factory-startup --python this_file -- --variant split_mask
Add --source-only to skip review renders. No game export or asset replacement occurs.
The original candidate is loaded for its lighting/rig basis and is never saved back.
"""
import ast
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[2]
TAU = math.tau
sys.path.insert(0,str(OUT))
from fire_volume import create_fire, animate_fire

args = sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
STYLE = args[args.index('--variant')+1] if '--variant' in args else 'round_lantern'
if STYLE not in ('round_lantern','split_mask'):
    raise ValueError(STYLE)
ROUND = STYLE == 'round_lantern'
LABEL = 'B · 기다리는 등불' if ROUND else 'C · 갈라진 기억'
DEST = OUT/'Variants'/STYLE
DEST.mkdir(parents=True,exist_ok=True)
PROTECTED = [OUT/'forgotten_candidate.blend',OUT/'candidate_perspective.png',
             ROOT/'SimpleGame/Assets/Spirits/ash_lantern_4dir.png',
             ROOT/'SimpleGame/SpiritCatalog.h',ROOT/'SimpleGame/PrototypeRendering.cpp']
before={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in PROTECTED}

# Extract only reusable mesh/rig functions. The original script's output loop is not executed.
parsed=ast.parse((OUT/'create_forgotten.py').read_text(encoding='utf-8'))
wanted={'material','attach','mesh','curve','sphere','ring','loft','pose','camera_view','render'}
functions=[n for n in parsed.body if isinstance(n,ast.FunctionDef) and n.name in wanted]
exec(compile(ast.Module(body=functions,type_ignores=[]),'<lantern mesh helpers>','exec'),globals())
base_pose=pose

bpy.ops.wm.open_mainfile(filepath=str(OUT/'forgotten_candidate.blend'))
bpy.context.preferences.filepaths.save_version=0
scene=bpy.context.scene
camera=scene.camera
rig=bpy.data.objects['남겨진 자 · 부유 리그']
rig.animation_data_clear()
rig.rotation_euler=(0,0,0)
for joint in rig.pose.bones:
    joint.matrix_basis.identity()
    joint.rotation_mode='XYZ'

# Replace every character mesh, including the face, hands, roof, cloak and cage.
# Keep only the shared camera, three review lights and armature structure.
for ob in list(scene.objects):
    if ob.type=='MESH' or (ob.type=='LIGHT' and ob.data.get('lantern_fire')):
        bpy.data.objects.remove(ob,do_unlink=True)
for mat in list(bpy.data.materials):
    if mat.get('lantern_fire'):
        bpy.data.materials.remove(mat,do_unlink=True)
for action in list(bpy.data.actions):
    bpy.data.actions.remove(action)

bronze=material('B · 古銅' if ROUND else 'C · 黒鉄',
                (.23,.087,.030) if ROUND else (.055,.065,.077),True)
edge=material('B · 磨かれた縁' if ROUND else 'C · 摩耗した鉄の縁',
              (.51,.24,.068) if ROUND else (.22,.205,.16),True)
dark=material('彫りの暗部',(.023,.012,.008) if ROUND else (.009,.013,.019))
cloth=material('B · 短い藍布' if ROUND else 'C · 裂けた墨布',
               (.027,.036,.055) if ROUND else (.016,.025,.028),True)
seam=material('擦れた織糸',(.083,.098,.115) if ROUND else (.057,.079,.080),True)
ivory=material('B · 焼けた白磁' if ROUND else 'C · 灰色の割れた白磁',
               (.72,.64,.46) if ROUND else (.48,.52,.49))
black=material('目と口の奥',(.004,.003,.005),glow=.01)
warm=material('小さな灯の瞳',(1,.25,.012),glow=.7)
paper=material('褪せた封紙',(.47,.35,.19),True)
red=material('朱の封印',(.31,.027,.018))

# Fit the reusable bone naming scheme to the new proportions before binding new meshes.
panels = [(-1.02,.87,1.35), (1.02,.87,1.20), (2.10,1.20,1.61), (-2.10,1.20,1.49)] if ROUND else [
    (-.97,.66,1.31),(.98,.57,2.37),(2.09,1.05,2.72),(-2.09,1.05,2.20)]
cloth_top=2.88 if ROUND else 3.10
bpy.ops.object.select_all(action='DESELECT')
rig.select_set(True)
bpy.context.view_layer.objects.active=rig
bpy.ops.object.mode_set(mode='EDIT')
for i,(angle,width,length) in enumerate(panels):
    radius=.63 if ROUND else .44
    x,y=radius*math.sin(angle),-radius*.80*math.cos(angle)
    bone=rig.data.edit_bones[f'cloth.{i}']
    bone.head=(x,y,cloth_top); bone.tail=(x,y,cloth_top-length*.52)
    tip=rig.data.edit_bones[f'tip.{i}']
    tip.head=bone.tail; tip.tail=(x,y+.1,cloth_top-length)
for side,sign in [('L',-1),('R',1)]:
    upper=rig.data.edit_bones['arm.'+side]
    hand=rig.data.edit_bones['hand.'+side]
    upper.head=(sign*(.64 if ROUND else .41),0,2.01 if ROUND else 2.22)
    upper.tail=(sign*(.82 if ROUND else .65),-.05,1.77 if ROUND else 1.69)
    hand.head=upper.tail
    hand.tail=(sign*(.88 if ROUND else .74),-.15,1.48 if ROUND else 1.33)
head=rig.data.edit_bones['mask']
head.head=(0,0,2.39 if ROUND else 2.67)
head.tail=(0,0,2.85 if ROUND else 3.10)
bpy.ops.object.mode_set(mode='OBJECT')


def outline_loop(name,coords,radius,mat,bone='body'):
    return curve(name,coords,radius,mat,bone,True)


def panel(index,center,width,length):
    """A short scalloped cape for B, or winding asymmetrical ribbon cloth for C."""
    vertices,faces=[],[]
    nu,nv=22,30
    for j in range(nv+1):
        t=j/nv
        for i in range(nu+1):
            u=i/nu
            if ROUND:
                angle=center+(u-.5)*width*(1-.22*t)+.14*math.sin(t*4+index)*t
                radius=.63+.12*math.sin(t*math.pi)+.18*t**2
                hem=.15*math.cos(u*math.pi*4)+.055*math.sin(u*math.pi*11+index)
                z=cloth_top-length*t+hem*t**5
            else:
                angle=center+(u-.5)*width*(1-.75*t**1.4)+.62*t**2
                radius=.44+.15*math.sin(t*math.pi)+(.41 if index==1 else .29)*t**2
                hem=.10*math.sin(u*math.pi*5+index)+.055*math.cos(u*math.pi*13)
                z=cloth_top-length*t+hem*t**5+.16*math.sin(t*4+index)*t**3
            fold=(.023+.016*t)*math.sin(u*math.pi*10+t*3+index)
            vertices.append(((radius+fold)*math.sin(angle),
                             -(radius*.82+fold)*math.cos(angle)+.10*t**2,z))
    for j in range(nv):
        for i in range(nu):
            k=j*(nu+1)+i
            faces.append((k,k+1,k+nu+2,k+nu+1))
    top,tip=f'cloth.{index}',f'tip.{index}'
    weights={top:[],tip:[]}
    for i in range(len(vertices)):
        t=(i//(nu+1))/nv
        w=max(0,min(1,(t-.27)/.63))
        weights[top].append((i,1-w)); weights[tip].append((i,w))
    ob=mesh(f'新しい外套 {index}',vertices,faces,cloth,top,True,weights)
    mod=ob.modifiers.new('織布の厚み','SOLIDIFY'); mod.thickness=.010
    # Attach trim to the same changing bone weights as the sheet underneath it.
    for edge_index in [0,nu]:
        trim=curve(f'外套の継ぎ目 {index}',[vertices[j*(nu+1)+edge_index] for j in range(nv+1)],.005,seam,top)
        trim.vertex_groups.clear()
        groups={name:trim.vertex_groups.new(name=name) for name in [top,tip]}
        for v in trim.data.vertices:
            t=max(0,min(1,(cloth_top-v.co.z)/length))
            w=max(0,min(1,(t-.27)/.63))
            if w<1: groups[top].add([v.index],1-w,'REPLACE')
            if w>0: groups[tip].add([v.index],w,'REPLACE')


def make_mask():
    xradius,zradius=(.36,.385) if ROUND else (.31,.43)
    center=(0,-.43,2.51) if ROUND else (0,-.35,2.60)
    ob=sphere('小さい仮面' if ROUND else '裂けた仮面',center,(xradius,.15,zradius),ivory,bind=False,segments=48,rings=32)
    # Slanted real apertures retain depth at oblique views.
    for side,sign in [('L',-1),('R',1)]:
        x=sign*(.145 if ROUND else .125)
        z=center[2]+(.035 if ROUND else .065)
        rx,rz=(.104,.102) if ROUND else ((.105,.072) if sign<0 else (.078,.115))
        y=center[1]-.07
        cutter=sphere('目の加工',(x,y,z),(rx,.34,rz),black,bind=False)
        angle=sign*(.38 if ROUND else .16)
        for v in cutter.data.vertices:
            dx,dz=v.co.x-x,v.co.z-z
            v.co.x=x+dx*math.cos(angle)-dz*math.sin(angle)
            v.co.z=z+dx*math.sin(angle)+dz*math.cos(angle)
        bpy.context.view_layer.objects.active=ob
        mod=ob.modifiers.new('実際の眼窩','BOOLEAN'); mod.operation='DIFFERENCE'; mod.object=cutter
        bpy.ops.object.modifier_apply(modifier=mod.name)
        bpy.data.objects.remove(cutter,do_unlink=True)
        sphere('暗い目の奥',(x,center[1]+.035,z),(rx*1.06,.025,rz*1.06),black,'mask')
        if ROUND or sign<0:
            sphere('目に残る火',(x,center[1]-.006,z-.026),(.039,.015,.021 if ROUND else .035),warm,'mask',20,12)
    mouth_z=center[2]-(.19 if ROUND else .22)
    cutter=sphere('口の加工',(0,center[1]-.07,mouth_z),(.043 if ROUND else .056,.33,.056 if ROUND else .098),black,bind=False)
    bpy.context.view_layer.objects.active=ob
    mod=ob.modifiers.new('小さい声の穴','BOOLEAN'); mod.operation='DIFFERENCE'; mod.object=cutter
    bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.data.objects.remove(cutter,do_unlink=True)
    sphere('口の奥',(0,center[1]+.035,mouth_z),(.046 if ROUND else .06,.023,.06 if ROUND else .105),black,'mask')
    if not ROUND:
        # Extrude a jagged slit through the porcelain, leaving separated shell islands.
        vertices=[]
        for y in [center[1]-.34,center[1]+.34]:
            for z,x in [(2.20,-.005),(2.40,.025),(2.55,-.018),(2.73,.035),(2.89,-.012),(3.10,.015),(3.25,-.005)]:
                vertices.extend([(x-.016,y,z),(x+.016,y,z)])
        faces=[]
        for i in range(6):
            a=i*2
            faces.extend([(a,a+1,a+3,a+2),(14+a+2,14+a+3,14+a+1,14+a),
                          (a,a+2,16+a,14+a),(a+1,15+a,17+a,a+3)])
        faces.extend([(0,14,15,1),(12,13,27,26)])
        blade=mesh('割れ目の加工',vertices,faces,black)
        bpy.context.view_layer.objects.active=ob
        mod=ob.modifiers.new('断裂した仮面','BOOLEAN'); mod.operation='DIFFERENCE'; mod.object=blade
        bpy.ops.object.modifier_apply(modifier=mod.name)
        bpy.data.objects.remove(blade,do_unlink=True)
        for v in ob.data.vertices:
            if v.co.x>0:
                v.co.x+=.025
                v.co.y+=.035
    attach(ob,'mask')
    bevel=ob.modifiers.new('白磁の断面','BEVEL'); bevel.width=.008; bevel.segments=2
    for sign in [-1,1]:
        x=sign*(.155 if ROUND else .14)
        curve('目の下の煤',[(x,center[1]-.147,center[2]-.06),
                            (x+sign*.015,center[1]-.144,center[2]-.17)],.006,dark,'mask')
    if ROUND:
        curve('額の細い亀裂',[(-.025,-.53,2.86),(-.045,-.57,2.78),(.012,-.579,2.73)],.006,dark,'mask')


def make_hands():
    for side,sign in [('L',-1),('R',1)]:
        sx=sign*(.64 if ROUND else .41)
        hx=sign*(.83 if ROUND else .65)
        hz=1.73 if ROUND else (1.76 if sign<0 else 1.57)
        curve('先細りの袖',[(sx,0,2.02 if ROUND else 2.20),(hx,-.025,hz+.13),(hx,-.10,hz)],.046 if ROUND else .038,cloth,'arm.'+side)
        sphere('手の甲',(hx,-.12,hz-.07),(.073 if ROUND else .064,.04,.090),bronze,'hand.'+side,24,12)
        for i in range(4):
            x=hx+sign*(i-1.5)*(.035 if ROUND else .030)
            length=(.14 if ROUND else .25)+.035*math.sin(i*math.pi/3)
            curve('丸く曲げた指' if ROUND else '長く曲がった指',
                  [(x,-.13,hz-.05),(x+sign*.025,-.19,hz-.13),
                   (x+sign*.020,-.23,hz-.06-length),(x,-.19,hz-.09-length)],
                  .015 if ROUND else .013,edge,'hand.'+side)
        curve('親指',[(hx-sign*.06,-.10,hz),(hx-sign*.10,-.16,hz-.10),
                      (hx-sign*.06,-.20,hz-.15)],.017,bronze,'hand.'+side)


def make_roof():
    levels=[(2.88,.62,.47),(2.95,.67,.51),(3.015,.55,.42),(3.12,.47,.36),(3.25,.26,.20),(3.30,.11,.09)] if ROUND else [
        (3.08,.44,.35),(3.14,.50,.40),(3.23,.39,.31),(3.40,.29,.23),(3.53,.10,.08)]
    sectors=12 if ROUND else 6
    for sector in range(sectors):
        a=sector*TAU/sectors
        half=TAU/sectors*.478
        vertices=[(rx*math.sin(a+d),-ry*math.cos(a+d),z)
                  for z,rx,ry in levels for d in [-half,0,half]]
        faces=[]
        for j in range(len(levels)-1):
            for i in range(2):
                k=j*3+i; faces.append((k,k+1,k+4,k+3))
        ob=mesh('丸い笠の銅板' if ROUND else '六角の鉄屋根',vertices,faces,bronze,smooth=False)
        mod=ob.modifiers.new('金属板の厚さ','SOLIDIFY'); mod.thickness=.016
        curve('屋根の縁の稜',[(rx*math.sin(a-half),-ry*math.cos(a-half),z+.008) for z,rx,ry in levels],.011,edge)
        for offset in [-.085,.085]:
            z=3.10 if ROUND else 3.32
            rx,ry=(.49,.375) if ROUND else (.33,.263)
            curve('小さい彫り',[(rx*math.sin(a+offset+t),-ry*math.cos(a+offset+t),z+.012*math.cos(t*35))
                               for t in [k*.01-.045 for k in range(10)]],.006,dark)
    z,rx,ry=levels[1]
    ring('厚い笠の口縁',z,rx,ry,edge,.025)
    if ROUND:
        for x,z,r in [(-.085,3.45,.14),(.085,3.43,.12)]:
            outline_loop('二つの吊り環',[(x+r*math.cos(i*TAU/72),0,z+r*1.1*math.sin(i*TAU/72)) for i in range(72)],.026,bronze)
    else:
        outline_loop('長い吊り金具',[(.105*math.cos(i*TAU/72),0,3.71+.20*math.sin(i*TAU/72)) for i in range(72)],.019,edge)
        for sign in [-1,1]:
            curve('折れた角飾り',[(sign*.35,0,3.20),(sign*.43,.015,3.37),(sign*.39,.05,3.49)],.018,bronze)


def seal_strip(x,y,z,length,bone):
    vertices=[]
    for j in range(13):
        t=j/12
        for side in [-1,1]:
            vertices.append((x+side*.045+.045*math.sin(t*3)*t,y-.035*math.sin(t*4),z-length*t))
    faces=[(j*2,j*2+1,j*2+3,j*2+2) for j in range(12)]
    ob=mesh('擦れた封紙',vertices,faces,paper,bone)
    mod=ob.modifiers.new('紙の断面','SOLIDIFY'); mod.thickness=.004
    for j in range(5):
        t=.12+j*.13
        xx=x+.045*math.sin(t*3)*t
        yy=y-.035*math.sin(t*4)-.006
        zz=z-length*t
        curve('朱の短い筆跡',[(xx-.024,yy,zz),(xx+.02,yy,zz-.012),(xx-.006,yy-.002,zz-.04)],.004,red,bone)


if ROUND:
    loft('丸い灯籠の下皿',[(.74,.06,.05),(.88,.37,.28),(1.04,.61,.46),(1.15,.64,.49)],bronze,n=64,smooth=True)
    loft('短い飾り房の受け',[(.53,.025,.020),(.64,.13,.105),(.74,.14,.11)],edge,n=32)
    ring('丸い下縁',1.14,.64,.49,edge,.030)
    ring('内側の細い縁',1.23,.64,.49,bronze,.016)
    ring('上の丸い縁',2.22,.62,.47,edge,.026)
    for i in range(8):
        angle=i*TAU/8+math.pi/8
        points=[((.60+.06*math.sin(t*math.pi))*math.sin(angle),
                 -(.46+.04*math.sin(t*math.pi))*math.cos(angle),1.17+t*1.05)
                for t in [j/24 for j in range(25)]]
        curve('膨らんだ丸い骨',points,.020,bronze)
        for index in [1,23]: sphere('鋲',points[index],(.028,.028,.028),edge,segments=12,rings=8)
    for i in range(32):
        a=i*TAU/32
        curve('下皿の刻み',[(.602*math.sin(a),-.456*math.cos(a),1.04),
                           (.635*math.sin(a+.01),-.481*math.cos(a+.01),1.11)],.005,dark)
else:
    loft('細い六角の下皿',[(.59,.025,.02),(.84,.20,.16),(1.03,.41,.33),(1.16,.43,.35)],bronze,n=6)
    loft('細長い底飾り',[(.29,.005,.005),(.51,.075,.06),(.63,.105,.084)],edge,n=12)
    for z,rx,ry in [(1.15,.43,.35),(1.27,.42,.34),(2.36,.42,.34),(2.45,.43,.35)]:
        ring('六角胴の細い縁',z,rx,ry,edge,.016)
    for i in range(6):
        a=i*TAU/6
        curve('長い鉄の骨',[(.405*math.sin(a),-.328*math.cos(a),1.16),
                           (.44*math.sin(a),-.356*math.cos(a),1.79),
                           (.42*math.sin(a),-.34*math.cos(a),2.44)],.015,bronze)
    for i in range(3):
        x=(i-1)*.16
        curve('胸の細い鎖',[(x,-.36,2.38-j*.055) for j in range(6)],.006,edge)
    seal_strip(-.30,-.16,2.45,.55,'cloth.0')
    seal_strip(.34,-.08,2.51,.79,'cloth.1')

make_roof()
for i,(angle,width,length) in enumerate(panels): panel(i,angle,width,length)

# The face opening is deliberately wider on B; the mask does not hide under a curtain.
vertices=[];faces=[]
for j in range(11):
    t=j/10
    for i in range(37):
        a=(.92 if ROUND else .77)+(TAU-(1.84 if ROUND else 1.54))*i/36
        radius=(.56 if ROUND else .43)+.018*math.sin(a*10+t*2)
        vertices.append((radius*math.sin(a),-.80*radius*math.cos(a),cloth_top-t*(.65 if ROUND else .88)))
for j in range(10):
    for i in range(36):
        k=j*37+i;faces.append((k,k+1,k+38,k+37))
mesh('仮面の後ろの頭巾',vertices,faces,cloth)
make_mask()
make_hands()
create_fire(mesh,rig)
if not ROUND:
    # Fit every shape-key position consistently to the narrower, taller cage.
    for ob in scene.objects:
        if ob.type=='MESH' and ob.get('lantern_fire'):
            for key in ob.data.shape_keys.key_blocks:
                for v in key.data:
                    v.co.x*=.76; v.co.y*=.80; v.co.z=1.19+(v.co.z-1.13)*1.12
            for v,basis in zip(ob.data.vertices,ob.data.shape_keys.key_blocks['Basis'].data):
                v.co=basis.co
for i in range(7):
    a=i*2.399
    sphere('灯籠の小さな火の粉',(.26*math.sin(a),-.06+.14*math.cos(a),1.30+i*.09),(.008,.008,.015),warm,'flame',12,8)


def pose(kind,phase):
    base_pose(kind,phase)
    rig.pose.bones['body'].location.y*=.8 if ROUND else 1.12
    for i in range(4):
        rig.pose.bones[f'tip.{i}'].rotation_euler.x*=.7 if ROUND else 1.3
    bpy.context.view_layer.update()


for kind,name in [('idle','대기 부유'),('glide','이동 부유')]:
    rig.animation_data_create();rig.animation_data.action=None
    for frame in range(33):
        pose(kind,frame/32);animate_fire(frame/32,frame+1)
        for joint in rig.pose.bones:
            for data_path in ['location','rotation_euler','scale']:
                joint.keyframe_insert(data_path,frame=frame+1,group=joint.name)
    rig.animation_data.action.name=LABEL+' · '+name
    rig.animation_data.action.use_fake_user=True
    curves=rig.animation_data.action.layers[0].strips[0].channelbag(rig.animation_data.action_slot).fcurves
    for fcurve in curves:
        for key in fcurve.keyframe_points:key.interpolation='LINEAR'
rig.animation_data.action=None
pose('idle',0);camera_view(.42)
scene.render.resolution_x=560;scene.render.resolution_y=700
bpy.ops.wm.save_as_mainfile(filepath=str(DEST/'candidate.blend'))

if '--source-only' not in args:
    for name,angle in [('front',0),('perspective',.42),('side',math.pi/2),('back',math.pi)]:
        rig.rotation_euler.z=0;pose('idle',0);camera_view(angle)
        render(DEST/f'{name}.png')
        print('VARIANT_VIEW',STYLE,name,flush=True)
    camera_view(0)
    directions=DEST/'Directions';directions.mkdir(exist_ok=True)
    for row in range(8):
        phi=row*TAU/8
        rig.rotation_euler.z=math.atan2(-math.sin(phi),math.cos(phi)/math.sin(math.atan2(4.05,8)))
        pose('idle',0);render(directions/f'd{row}.png',192,192)
    camera_view(.42);rig.rotation_euler.z=0
    motion=DEST/'Motion';motion.mkdir(exist_ok=True)
    for frame in range(16):
        pose('glide',frame/16);render(motion/f'float_{frame:02d}.png',320,400)
    print('VARIANT_MOTION',STYLE,flush=True)

after={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in PROTECTED}
if before!=after:raise RuntimeError('A protected original changed')
report={'label':LABEL,'style':STYLE,'status':'candidate_not_applied',
        'source':str((DEST/'candidate.blend').relative_to(ROOT)),
        'bones':len(rig.data.bones),'camera':{'resolution':[560,700],'orthoScale':4.8,'azimuth':.42},
        'meshParts':len([ob for ob in scene.objects if ob.type=='MESH']),
        'materials':'cel bands / fine fabric and metal grain / procedural volume flame',
        'wholeModelRebuilt':True,'reviewDirections':0 if '--source-only' in args else 8,
        'motionFrames':0 if '--source-only' in args else 16,
        'gameBuildRunTested':False,'gameAssetsModified':False,'runtimeIntegrated':False,
        'protectedBefore':before,'protectedAfter':after}
(DEST/'candidate_manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('LANTERN_VARIANT_READY',STYLE,flush=True)
