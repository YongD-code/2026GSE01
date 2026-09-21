"""Blender-native protagonist source. Run with blender --background --python this_file.
Creates a rigged prototype, editable actions, and consistent transparent sprite atlases.
No external Python packages. Existing hand-painted assets are preserved.
"""
import bpy
import math
import random
import sys
from pathlib import Path
from mathutils import Vector
from array import array

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'SimpleGame/Assets/Characters/Blender'
OUT.mkdir(parents=True, exist_ok=True)
PREVIEW = '--preview-only' in sys.argv
SIZE = 192
random.seed(17)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
bpy.context.preferences.filepaths.save_version = 0
scene.render.engine = 'BLENDER_WORKBENCH'
scene.render.resolution_x = SIZE
scene.render.resolution_y = SIZE
scene.render.resolution_percentage = 100
scene.render.film_transparent = True
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'
scene.render.fps = 24
scene.world.color = (.15, .15, .15)
sh = scene.display.shading
sh.light = 'STUDIO'
sh.studiolight_rotate_z = .4
sh.color_type = 'MATERIAL'
sh.show_shadows = True
sh.show_cavity = True
sh.cavity_type = 'BOTH'
sh.curvature_ridge_factor = 1.3
sh.curvature_valley_factor = 1.1
sh.show_object_outline = True
sh.object_outline_color = (.025, .03, .05)
scene.view_settings.view_transform = 'Standard'

def material(name, color):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    return m

coat = material('먹빛 남색 코트', (.055, .065, .105))
edge = material('회청색 봉제선', (.16, .19, .27))
lining = material('코트 안감', (.075, .1, .18))
black = material('안대와 가죽', (.018, .021, .035))
skin = material('피부', (.78, .62, .53))
hair = material('은백색 머리', (.8, .85, .94))
hair_shadow = material('머리 음영', (.47, .55, .69))
metal = material('은색 장식', (.42, .48, .57))
blue = material('푸른 주력', (.12, .6, 1))

rig_data = bpy.data.armatures.new('방랑자 뼈대')
rig = bpy.data.objects.new('주인공_리그', rig_data)
scene.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig
rig.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')

def bone(name, head, tail, parent=None):
    b = rig_data.edit_bones.new(name)
    b.head, b.tail = head, tail
    if parent:
        b.parent = rig_data.edit_bones[parent]
    return b

bone('root', (0,0,0), (0,0,.25))
bone('hips', (0,0,1.02), (0,0,1.28), 'root')
bone('chest', (0,0,1.28), (0,0,1.68), 'hips')
bone('head', (0,0,1.68), (0,0,2.1), 'chest')
for side, x in [('L', -.17), ('R', .17)]:
    bone('thigh.'+side, (x,0,1.05), (x,0,.59), 'hips')
    bone('shin.'+side, (x,0,.59), (x,0,.18), 'thigh.'+side)
    bone('foot.'+side, (x,0,.18), (x,-.23,.12), 'shin.'+side)
    x *= 2.1
    bone('arm.'+side, (x,0,1.59), (x,0,1.22), 'chest')
    bone('forearm.'+side, (x,0,1.22), (x,0,.91), 'arm.'+side)
bpy.ops.object.mode_set(mode='OBJECT')
rig.select_set(False)
rig.show_in_front = True

def bind(obj, name, mat, joint):
    obj.name = name
    obj.data.materials.append(mat)
    # Bake object transforms before bone deformation; geometry stays editable.
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    vg = obj.vertex_groups.new(name=joint)
    vg.add(list(range(len(obj.data.vertices))), 1, 'REPLACE')
    modifier = obj.modifiers.new('뼈대 변형', 'ARMATURE')
    modifier.object = rig
    obj.parent = rig
    obj.select_set(False)
    return obj

def ellipsoid(name, center, scale, mat, joint, segments=16):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=10, location=center)
    ob = bpy.context.object
    ob.scale = scale
    for poly in ob.data.polygons:
        poly.use_smooth = True
    return bind(ob, name, mat, joint)

def taper(name, start, end, r1, r2, mat, joint, vertices=10):
    delta = Vector(end)-Vector(start)
    bpy.ops.mesh.primitive_cone_add(vertices=vertices, radius1=r1, radius2=r2, depth=delta.length, location=(Vector(start)+Vector(end))*.5)
    ob = bpy.context.object
    ob.rotation_euler = delta.to_track_quat('Z','Y').to_euler()
    return bind(ob,name,mat,joint)

ellipsoid('허리', (0,0,1.12), (.27,.18,.24),coat,'hips')
ellipsoid('상의', (0,0,1.43), (.32,.205,.32),coat,'chest')
ellipsoid('목', (0,0,1.73), (.1,.095,.16),skin,'head')
ellipsoid('얼굴', (0,-.015,1.96), (.255,.215,.30),skin,'head',24)
ellipsoid('턱', (0,-.07,1.81), (.16,.14,.10),skin,'head')
ellipsoid('코', (0,-.227,1.91), (.032,.038,.06),skin,'head')
# Blindfold as a curved ribbon around the head, rather than a floating box.
verts=[]
for z, radius in [(1.935,1.), (2.04,1.)]:
    for i in range(32):
        a=2*math.pi*i/32
        verts.append((.257*math.cos(a), -.015+.219*math.sin(a), z))
faces=[(i,(i+1)%32,(i+1)%32+32,i+32) for i in range(32)]
mesh=bpy.data.meshes.new('안대 띠');mesh.from_pydata(verts,[],faces);mesh.update()
ob=bpy.data.objects.new('안대',mesh);scene.collection.objects.link(ob);bind(ob,'안대',black,'head')
taper('입',(-.038,-.203,1.823),(.038,-.203,1.819),.005,.005,edge,'head',6)
# Structured collar, seams, buckles and sleeves.
for sign in [-1,1]:
    ellipsoid('깃', (sign*.105,-.11,1.695), (.095,.13,.12),coat,'chest')
    taper('깃 테두리', (sign*.035,-.231,1.63), (sign*.17,-.155,1.75), .012,.009,edge,'chest')
    ellipsoid('귀', (sign*.255,0,1.95),(.035,.055,.075),skin,'head')
for i in range(4):
    ellipsoid('단추', (.035,-.199,1.29+i*.075), (.012,.012,.012),metal,'chest',8)
taper('앞섶', (-.055,-.18,1.2),(-.055,-.205,1.62),.009,.009,edge,'chest')
ellipsoid('허리띠',(0,-.01,1.17),(.285,.193,.05),black,'hips')
ellipsoid('버클',(0,-.205,1.17),(.045,.015,.034),metal,'hips')
for side, x in [('L',-.17),('R',.17)]:
    ellipsoid('바지 허벅지 '+side,(x,0,.83),(.122,.135,.28),coat,'thigh.'+side)
    ellipsoid('바지 종아리 '+side,(x,0,.40),(.095,.105,.25),black,'shin.'+side)
    ellipsoid('부츠 '+side,(x,-.095,.135),(.115,.22,.12),black,'foot.'+side)
    ellipsoid('부츠 밑창 '+side,(x,-.105,.055),(.12,.22,.045),edge,'foot.'+side)
    ax=x*2.1
    ellipsoid('소매 위 '+side,(ax,0,1.425),(.112,.12,.235),coat,'arm.'+side)
    ellipsoid('소매 아래 '+side,(ax,0,1.095),(.091,.10,.20),coat,'forearm.'+side)
    ellipsoid('손 '+side,(ax,-.005,.91),(.069,.065,.105),skin,'forearm.'+side)
    ellipsoid('소매끝 '+side,(ax,0,.987),(.098,.105,.035),edge,'forearm.'+side)
# Six tapered coat panels. Solidify gives visible dark lining from every direction.
for i in range(6):
    a0=.20+i*(2*math.pi-.4)/6
    a1=a0+(2*math.pi-.4)/6-.045
    # Angle zero is forward; preserve the opening over the legs.
    v=[]
    for z,r in [(1.18,.275),(.87,.34),(.48,.43)]:
        for a in [a0,a1]:
            v.append((math.sin(a)*r,-math.cos(a)*r*.76,z))
    m=bpy.data.meshes.new('코트 패널');m.from_pydata(v,[],[(0,1,3,2),(2,3,5,4)]);m.update()
    o=bpy.data.objects.new('코트 자락',m);scene.collection.objects.link(o)
    joint='thigh.L' if sum(vv[0] for vv in v)<0 else 'thigh.R'
    bind(o,'코트 자락',lining,joint)
    # Reduce leg-following to avoid a rigid skirt while keeping separate tails.
    group=o.vertex_groups.get(joint)
    for vi in range(6):
        weight=.08 if vi<2 else (.16 if vi<4 else .27)
        group.add([vi],weight,'REPLACE')
    hips=o.vertex_groups.new(name='hips')
    for vi in range(6): hips.add([vi],1-(.08 if vi<2 else .16 if vi<4 else .27),'REPLACE')
    sol=o.modifiers.new('원단 두께','SOLIDIFY');sol.thickness=.025
    bevel=o.modifiers.new('자락 모서리','BEVEL');bevel.width=.016;bevel.segments=2
# Stylized hair mass with swept, tapered locks.
ellipsoid('머리 바탕',(0,.015,2.105),(.27,.235,.22),hair_shadow,'head',20)
for i in range(42):
    a=i*2.39996
    ring=(i%7)/6
    start=Vector((math.cos(a)*(.07+.12*ring),math.sin(a)*(.055+.1*ring),2.13+.09*(1-ring)))
    end=start+Vector((math.cos(a+.65)*(.17+.07*ring)-.055,math.sin(a+.65)*(.13+.05*ring),.07+.13*(1-ring)))
    taper('은빛 머리카락',start,end,.065+.025*(1-ring),.002,hair if i%4 else hair_shadow,'head',5)
for i in range(9):
    x=(i-4)*.047
    taper('앞머리',(x,-.15,2.17),(x-.085,-.232,2.035+abs(x)*.22),.068,.003,hair,'head',5)
# Blue sigil on the back, useful for reading facing direction.
for a,b in [((0,.206,1.3),(0,.206,1.56)), ((-.07,.204,1.47),(.07,.204,1.39)), ((.07,.204,1.47),(-.07,.204,1.39))]:
    taper('등 주술 문양',a,b,.009,.009,blue,'chest',6)
# A visible palm orb only for cast frames.
orb=ellipsoid('주술 구체',(.357,-.08,.83),(.14,.14,.14),blue,'forearm.R',16)
orb.hide_render=True

# Orthographic sprite camera; local -Y is the character front.
bpy.ops.object.camera_add(location=(0,-6,3.7))
cam=bpy.context.object
cam.name='스프라이트_직교카메라'
target=Vector((0,0,1.18))
cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
cam.data.type='ORTHO';cam.data.ortho_scale=3.15
scene.camera=cam

for p in rig.pose.bones: p.rotation_mode='XYZ'

def pose(kind,t):
    for p in rig.pose.bones:
        p.rotation_euler=(0,0,0);p.location=(0,0,0)
    phase=t*2*math.pi
    if kind in ('walk','run'):
        run=kind=='run'
        amp=.66 if run else .40
        rig.pose.bones['hips'].location.y=.025*(1-math.cos(phase*2))
        rig.pose.bones['chest'].rotation_euler.x=.15 if run else .025
        for side,offset in [('L',0),('R',math.pi)]:
            wave=math.sin(phase+offset)
            rig.pose.bones['thigh.'+side].rotation_euler.x=wave*amp
            rig.pose.bones['shin.'+side].rotation_euler.x=max(0,wave)*(1.05 if run else .62)
            rig.pose.bones['arm.'+side].rotation_euler.x=-wave*(.72 if run else .35)
            rig.pose.bones['forearm.'+side].rotation_euler.x=-.8 if run else -.18
        rig.pose.bones['chest'].rotation_euler.z=.05*math.sin(phase)
    elif kind=='cast':
        strength=math.sin(math.pi*(.12+.76*t))
        rig.pose.bones['arm.R'].rotation_euler.x=-1.15*strength
        rig.pose.bones['forearm.R'].rotation_euler.x=-.45*strength
        rig.pose.bones['arm.L'].rotation_euler.x=-.35
        rig.pose.bones['chest'].rotation_euler.z=-.15*strength
    orb.hide_render=kind!='cast'
    bpy.context.view_layer.update()

# Store proper actions with matching beginning/end poses for editing in Blender.
for kind, frames in [('idle',24),('walk',24),('run',16),('cast',16)]:
    rig.animation_data_create()
    rig.animation_data.action=None
    for f in range(1,frames+2):
        pose(kind,(f-1)/frames)
        for p in rig.pose.bones:
            p.keyframe_insert('rotation_euler',frame=f,group=p.name)
            p.keyframe_insert('location',frame=f,group=p.name)
    rig.animation_data.action.name={'idle':'대기','walk':'걷기','run':'달리기','cast':'주술 시전'}[kind]
    rig.animation_data.action.use_fake_user=True
rig.animation_data.action=None
pose('idle',0)
scene.frame_start=1;scene.frame_end=24
rig['설명']='앞쪽은 -Y. 걷기/달리기 동작은 좌우 다리 반주기 교대. 스프라이트는 아래에서 시계 방향 8행.'
scene['에셋 단계']='사용자 원화를 참고한 절차적 3D 초안. 원본 그림체의 정밀 복제 아님.'
bpy.context.view_layer.objects.active=rig
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Art/Blender/protagonist.blend'))

# Single preview is useful without rendering every animation.
scene.render.resolution_x=512;scene.render.resolution_y=512
rig.rotation_euler.z=-math.pi/8
scene.render.filepath=str(OUT/'preview.png')
bpy.ops.render.render(write_still=True)
if PREVIEW: sys.exit(0)
scene.render.resolution_x=SIZE;scene.render.resolution_y=SIZE
# Read the saved render through Blender itself; no external image processing.
for kind,columns in [('walk',9),('run',9),('cast',4)]:
    width=columns*SIZE;height=8*SIZE
    atlas=array('f',[0])*(width*height*4)
    for row in range(8):
        rig.rotation_euler.z=-row*math.pi/4
        for col in range(columns):
            pose('idle' if kind!='cast' and col==0 else kind,
                 col/3 if kind=='cast' else (col-1)/8)
            temp=OUT/'_frame.png'
            scene.render.filepath=str(temp)
            bpy.ops.render.render(write_still=True)
            im=bpy.data.images.load(str(temp),check_existing=False)
            pixels=array('f',[0])*(SIZE*SIZE*4)
            im.pixels.foreach_get(pixels)
            for y in range(SIZE):
                dest=(((7-row)*SIZE+y)*width+col*SIZE)*4
                atlas[dest:dest+SIZE*4]=pixels[y*SIZE*4:(y+1)*SIZE*4]
            bpy.data.images.remove(im)
        print(f'ATLAS {kind} direction {row+1}/8',flush=True)
    image=bpy.data.images.new('主_'+kind,width=width,height=height,alpha=True)
    image.pixels.foreach_set(atlas)
    image.filepath_raw=str(OUT/(kind+'.png'))
    image.file_format='PNG'
    image.save()
    bpy.data.images.remove(image)
(OUT/'_frame.png').unlink(missing_ok=True)
print('PROTAGONIST_ASSETS_COMPLETE',flush=True)
