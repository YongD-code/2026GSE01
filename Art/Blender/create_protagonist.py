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
ASSET_NAME = globals().get('ASSET_NAME', 'protagonist')
CUSTOMIZE = globals().get('CUSTOMIZE')
OUT = ROOT / 'SimpleGame/Assets/Characters/Blender'
if ASSET_NAME != 'protagonist':
    OUT = OUT / ASSET_NAME
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
if CUSTOMIZE:
    scene.display.render_aa = '8'
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
sh.show_object_outline = False
sh.show_specular_highlight = False
sh.object_outline_color = (.025, .03, .05)
scene.view_settings.view_transform = 'Standard'

def material(name, color):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    shader = m.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*color, 1)
    shader.inputs['Roughness'].default_value = .8
    return m

coat = material('먹빛 남색 코트', (.065, .077, .115))
edge = material('회청색 봉제선', (.16, .19, .27))
lining = material('코트 안감', (.075, .1, .18))
black = material('안대와 가죽', (.018, .021, .035))
skin = material('피부', (.78, .62, .53))
hair = material('은백색 머리', (.8, .85, .94))
hair_shadow = material('머리 음영', (.47, .55, .69))
metal = material('은색 장식', (.42, .48, .57))
blue = material('푸른 주력', (.12, .6, 1))
for mat, roughness, metallic in [(skin,.55,0),(hair,.46,0),(hair_shadow,.5,0),(black,.38,0),(metal,.28,.75)]:
    shader=mat.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Roughness'].default_value=roughness
    shader.inputs['Metallic'].default_value=metallic
shader=blue.node_tree.nodes.get('Principled BSDF')
shader.inputs['Emission Color'].default_value=(.06,.32,1,1)
shader.inputs['Emission Strength'].default_value=1.5
for mat in [coat,lining]:
    noise=mat.node_tree.nodes.new('ShaderNodeTexNoise')
    noise.inputs['Scale'].default_value=150
    bump=mat.node_tree.nodes.new('ShaderNodeBump')
    bump.inputs['Strength'].default_value=.12
    bump.inputs['Distance'].default_value=.008
    mat.node_tree.links.new(noise.outputs['Fac'],bump.inputs['Height'])
    mat.node_tree.links.new(bump.outputs['Normal'],mat.node_tree.nodes.get('Principled BSDF').inputs['Normal'])

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


# Ring lofts form continuous surfaces rather than disconnected capsule joints.
def surface(name, vertices, faces, mat, joint, smooth=True):
    mesh=bpy.data.meshes.new(name)
    mesh.from_pydata(vertices,[],faces)
    mesh.update()
    ob=bpy.data.objects.new(name,mesh)
    scene.collection.objects.link(ob)
    for polygon in mesh.polygons:
        polygon.use_smooth=smooth
    bind(ob,name,mat,joint)
    return ob


def loft(name, rings, mat, joint, count=32, folds=0):
    verts=[]
    for cx,cy,z,rx,ry in rings:
        for i in range(count):
            a=2*math.pi*i/count
            r=1+folds*math.sin(a*7+z*11)
            verts.append((cx+rx*math.cos(a)*r,cy+ry*math.sin(a)*r,z))
    faces=[]
    for j in range(len(rings)-1):
        for i in range(count):
            n=j*count+i
            faces.append((n,j*count+(i+1)%count,(j+1)*count+(i+1)%count,n+count))
    faces.append(tuple(range(count-1,-1,-1)))
    faces.append(tuple((len(rings)-1)*count+i for i in range(count)))
    ob=surface(name,verts,faces,mat,joint)
    sub=ob.modifiers.new('표면 다듬기','SUBSURF')
    sub.levels=1
    sub.render_levels=1
    return ob


def blend_height(ob, low, high, pivot, width):
    ob.vertex_groups.clear()
    groups=[ob.vertex_groups.new(name=n) for n in (low,high)]
    for v in ob.data.vertices:
        t=max(0,min(1,(v.co.z-pivot)/width+.5))
        groups[0].add([v.index],1-t,'REPLACE')
        groups[1].add([v.index],t,'REPLACE')


def seam(name, points, radius, mat, joint):
    data=bpy.data.curves.new(name,'CURVE')
    data.dimensions='3D'
    data.bevel_depth=radius
    data.bevel_resolution=2
    spline=data.splines.new('BEZIER')
    spline.bezier_points.add(len(points)-1)
    for point,xyz in zip(spline.bezier_points,points):
        point.co=xyz
        point.handle_left_type='AUTO'
        point.handle_right_type='AUTO'
    ob=bpy.data.objects.new(name,data)
    scene.collection.objects.link(ob)
    bpy.context.view_layer.objects.active=ob
    ob.select_set(True)
    bpy.ops.object.convert(target='MESH')
    return bind(bpy.context.object,name,mat,joint)


loft('맞춤 코트 몸판',[(0,0,1.12,.245,.155),(0,0,1.18,.248,.16),
     (0,0,1.3,.265,.17),(0,0,1.47,.30,.185),(0,0,1.56,.325,.17),
     (0,0,1.63,.285,.14),(0,0,1.69,.115,.105)],coat,'chest',folds=.018)
loft('바지 허리',[(0,0,1.0,.23,.15),(0,0,1.12,.25,.165),(0,0,1.19,.245,.15)],coat,'hips')
loft('목',[(0,0,1.64,.092,.085),(0,0,1.80,.105,.092)],skin,'head')
# Narrow jaw, cheekbones and forehead; nose merges into facial silhouette.
loft('얼굴 조형',[(0,-.07,1.755,.065,.065),(0,-.053,1.78,.115,.105),
     (0,-.026,1.84,.18,.15),(0,-.007,1.91,.225,.183),(0,0,1.99,.236,.192),
     (0,.012,2.09,.231,.19),(0,.025,2.16,.18,.15),(0,.025,2.19,.07,.07)],skin,'head',48)
loft('콧대',[(0,-.168,1.86,.014,.019),(0,-.19,1.9,.026,.041),
     (0,-.185,1.93,.017,.025),(0,-.165,1.975,.011,.013)],skin,'head',16)
seam('입술',[(-.039,-.164,1.813),(0,-.172,1.808),(.039,-.164,1.813)],.004,edge,'head')
for sign in [-1,1]:
    ellipsoid('귀',(sign*.235,.018,1.955),(.027,.04,.066),skin,'head',20)
    seam('귓바퀴',[(sign*.249,-.008,1.99),(sign*.255,-.019,1.958),(sign*.242,-.01,1.928)],.006,edge,'head')
# Fabric blindfold follows a subtly folded elliptical band.
loft('주름진 안대',[(0,0,1.955,.242,.199),(0,0,1.964,.247,.204),
     (0,0,2.006,.249,.208),(0,0,2.049,.243,.2),(0,0,2.055,.24,.197)],black,'head',48,folds=.005)
seam('안대 봉제선',[(x,-math.sqrt(max(0,1-(x/.246)**2))*.207,2.044) for x in [-.22,-.15,0,.15,.22]],.0025,edge,'head')
# A standing collar with an open V and folded lapels, not two round lobes.
for side in [-1,1]:
    v=[(side*.015,-.184,1.58),(side*.17,-.167,1.63),(side*.135,-.096,1.765),
       (side*.045,-.111,1.735),(side*.24,-.156,1.51)]
    ob=surface('접힌 높은 깃',v,[(0,1,2,3),(0,4,1)],coat,'chest')
    solid=ob.modifiers.new('깃 두께','SOLIDIFY');solid.thickness=.018
    bevel=ob.modifiers.new('깃 모서리','BEVEL');bevel.width=.008;bevel.segments=3
    seam('깃 스티치',[v[3],v[2],v[1],v[4]],.0035,edge,'chest')
seam('비대칭 앞섶',[(-.065,-.164,1.18),(-.04,-.188,1.38),(.02,-.19,1.56),(.045,-.13,1.7)],.005,edge,'chest')
for i in range(5):
    ellipsoid('은색 잠금 단추',(.032,-.192,1.28+i*.065),(.011,.008,.012),metal,'chest',12)
loft('가죽 허리띠',[(0,0,1.145,.257,.17),(0,0,1.19,.257,.17)],black,'hips')
for x in [-.036,.036]:
    seam('버클 세로',[(x,-.181,1.145),(x,-.181,1.19)],.006,metal,'hips')
for z in [1.145,1.19]:
    seam('버클 가로',[(-.036,-.181,z),(.036,-.181,z)],.006,metal,'hips')
# Sleeves and trousers have continuous topology and blended elbow/knee weights.
for side,x in [('L',-.17),('R',.17)]:
    ob=loft('연속 바지 '+side,[(x,0,.19,.074,.084),(x,0,.4,.083,.091),
            (x,-.012,.55,.095,.102),(x,0,.64,.102,.115),(x,0,.83,.12,.129),
            (x,0,1.05,.133,.14),(x,0,1.1,.12,.13)],coat,'thigh.'+side,folds=.028)
    blend_height(ob,'shin.'+side,'thigh.'+side,.59,.16)
    # Fitted leather boots, wide toe box and layered sole.
    loft('가죽 부츠 '+side,[(x,-.09,.045,.106,.194),(x,-.10,.065,.108,.20),
         (x,-.098,.12,.102,.192),(x,-.03,.195,.089,.124),(x,0,.28,.087,.095),
         (x,0,.34,.085,.094)],black,'foot.'+side)
    loft('밑창 '+side,[(x,-.1,.025,.111,.202),(x,-.1,.059,.111,.202)],edge,'foot.'+side)
    for z in [.19,.23,.27]:
        seam('부츠 끈',[(x-.055,-.10,z),(x+.055,-.10,z+.009)],.0035,metal,'foot.'+side)
    ax=x*2.1
    ob=loft('연속 소매 '+side,[(ax,0,.97,.071,.083),(ax,0,1.01,.077,.088),
         (ax,.004,1.1,.083,.096),(ax,.007,1.21,.092,.103),(ax,0,1.27,.099,.11),
         (ax,0,1.46,.12,.123),(ax*.94,0,1.575,.13,.116),(ax*.88,0,1.64,.07,.08)],coat,'arm.'+side,folds=.025)
    blend_height(ob,'forearm.'+side,'arm.'+side,1.22,.18)
    loft('소매 가죽 테두리 '+side,[(ax,0,.97,.078,.09),(ax,0,1.012,.08,.092)],black,'forearm.'+side)
    loft('손바닥 '+side,[(ax,0,.864,.042,.044),(ax,-.014,.885,.057,.047),
         (ax,-.01,.95,.059,.042),(ax,0,.99,.045,.036)],skin,'forearm.'+side,20)
    for finger in range(4):
        fx=ax+(finger-1.5)*.022
        seam('손가락',[(fx,-.013,.91),(fx,-.023,.862),(fx,-.029,.841+abs(finger-1.5)*.008)],.010,skin,'forearm.'+side)
    seam('엄지',[(ax+(.05 if x<0 else -.05),-.01,.94),(ax+(.066 if x<0 else -.066),-.038,.907)],.015,skin,'forearm.'+side)
    seam('소매 접힘',[(ax-.065,-.057,1.24),(ax,-.096,1.225),(ax+.065,-.057,1.245)],.004,edge,'forearm.'+side)
# Two continuous curved cloth halves with front opening and a back vent.
for side in [-1,1]:
    verts=[]
    rows=15;cols=25
    for j in range(rows):
        t=j/(rows-1)
        z=1.18-.66*t
        for i in range(cols):
            a=.30+(math.pi-.36)*i/(cols-1)
            flare=.25+.16*t*t
            fold=.012*math.sin(a*9+t*1.4)*math.sin(t*math.pi*.8)
            x=side*math.sin(a)*(flare+fold)
            y=-math.cos(a)*(flare*.76+fold)
            hem=.08*t**6*(math.cos(a*2)+.4)
            verts.append((x,y,z+hem))
    faces=[]
    for j in range(rows-1):
        for i in range(cols-1):
            n=j*cols+i
            faces.append((n,n+1,n+cols+1,n+cols))
    ob=surface('곡면 코트 자락',verts,faces,coat,'hips')
    ob.vertex_groups.clear()
    hips=ob.vertex_groups.new(name='hips')
    thigh=ob.vertex_groups.new(name='thigh.L' if side<0 else 'thigh.R')
    for v in ob.data.vertices:
        w=.18*((1.18-v.co.z)/.74)**2
        hips.add([v.index],1-w,'REPLACE');thigh.add([v.index],w,'REPLACE')
    sub=ob.modifiers.new('천 곡면','SUBSURF');sub.levels=1
    solid=ob.modifiers.new('천 두께','SOLIDIFY');solid.thickness=.014
    # Separate lower hem curve shares weighted skirt deformation.
    for index in [0,cols-1]:
        pts=[verts[j*cols+index] for j in range(rows)]
        trim=seam('코트 가장자리',pts,.0045,edge,'hips')
        trim.vertex_groups.clear()
        hg=trim.vertex_groups.new(name='hips');tg=trim.vertex_groups.new(name='thigh.L' if side<0 else 'thigh.R')
        for v in trim.data.vertices:
            w=.18*((1.18-v.co.z)/.74)**2
            hg.add([v.index],1-w,'REPLACE');tg.add([v.index],w,'REPLACE')
# Curved, flattened hair locks follow a swept flow instead of straight cones.
ellipsoid('머리 바탕',(0,.018,2.116),(.253,.213,.16),hair_shadow,'head',32)

def lock(name, controls, width, mat):
    controls=[Vector(v) for v in controls]
    verts=[];count=9;sides=8
    for j in range(count):
        t=j/(count-1)
        p=(1-t)**3*controls[0]+3*t*(1-t)**2*controls[1]+3*t*t*(1-t)*controls[2]+t**3*controls[3]
        tangent=3*(1-t)**2*(controls[1]-controls[0])+6*t*(1-t)*(controls[2]-controls[1])+3*t*t*(controls[3]-controls[2])
        tangent.normalize()
        across=tangent.cross(Vector((0,1,0)))
        if across.length<.01: across=tangent.cross(Vector((1,0,0)))
        across.normalize();normal=tangent.cross(across).normalized()
        radius=width*(.7+.5*math.sin(math.pi*t))*(1-t)**.7+.001
        for i in range(sides):
            a=i*2*math.pi/sides
            q=p+across*math.cos(a)*radius+normal*math.sin(a)*radius*.38
            verts.append(tuple(q))
    faces=[]
    for j in range(count-1):
        for i in range(sides):
            faces.append((j*sides+i,j*sides+(i+1)%sides,(j+1)*sides+(i+1)%sides,(j+1)*sides+i))
    ob=surface(name,verts,faces,mat,'head')
    sub=ob.modifiers.new('머릿결 곡면','SUBSURF');sub.levels=1

for i in range(24):
    a=i*2.39996
    radius=.075+.105*(i%4)/3
    x=math.cos(a)*radius;y=math.sin(a)*radius
    lock('흐르는 은발',[(x,y,2.17),(x-.035,y+.025,2.29),
         (x+math.cos(a+.5)*.15-.07,y+math.sin(a+.5)*.10,2.34),
         (x+math.cos(a+.5)*.23-.07,y+math.sin(a+.5)*.15,2.22+.04*(i%3))],.060 if i%2 else .075,hair if i%5 else hair_shadow)
for i in range(11):
    x=(i-5)*.043
    lock('옆으로 흐르는 앞머리',[(x,-.105,2.21),(x-.035,-.185,2.21),
         (x-.075,-.219,2.095),(x-.10,-.215,2.043+abs(x)*.17)],.047,hair)
for side in [-1,1]:
    for i in range(3):
        lock('옆머리',[(side*.19,.06-i*.07,2.17),(side*.27,.055-i*.06,2.14),
             (side*.26,.035-i*.06,2.07),(side*.23,.025-i*.06,2.015)],.044,hair_shadow if i==0 else hair)
for a,b in [((0,.18,1.3),(0,.189,1.56)), ((-.07,.188,1.47),(.07,.183,1.39)), ((.07,.188,1.47),(-.07,.183,1.39))]:
    seam('등 주술 자수',[a,b],.006,blue,'chest')
# A visible palm orb only for cast frames.
orb=ellipsoid('주술 구체',(.357,-.08,.83),(.14,.14,.14),blue,'forearm.R',16)
orb.hide_render=True
if CUSTOMIZE:
    CUSTOMIZE(globals())

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
    if kind == 'idle' and CUSTOMIZE:
        rig.pose.bones['chest'].rotation_euler.x = .008 * math.sin(phase)
        rig.pose.bones['head'].rotation_euler.z = .012 * math.sin(phase)
    if kind in ('walk','run'):
        run=kind=='run'
        # Analytic two-bone IK in the sagittal plane. The supporting foot stays
        # on the ground and moves backward at constant speed during stance.
        hip_offset=(-.075 if run else -.045)+.009*math.cos(phase*2)
        rig.pose.bones['hips'].location.y=hip_offset
        rig.pose.bones['chest'].rotation_euler.x=.10 if run else .018
        for side,offset in [('L',0),('R',.5)]:
            u=(t+offset)%1
            stride=.26 if run else .18
            stance=.52 if run else .62
            if u<stance:
                foot_y=-stride+2*stride*u/stance
                lift=0
            else:
                swing=(u-stance)/(1-stance)
                foot_y=stride*math.cos(math.pi*swing)
                lift=(.18 if run else .095)*math.sin(math.pi*swing)
            vertical=1.05+hip_offset-(.18+lift)
            distance=min(.869,max(.10,math.hypot(foot_y,vertical)))
            upper=.46;lower=.41
            theta=math.atan2(foot_y,vertical)-math.acos(max(-1,min(1,(upper*upper+distance*distance-lower*lower)/(2*upper*distance))))
            knee=math.pi-math.acos(max(-1,min(1,(upper*upper+lower*lower-distance*distance)/(2*upper*lower))))
            rig.pose.bones['thigh.'+side].rotation_euler.x=theta
            rig.pose.bones['shin.'+side].rotation_euler.x=knee
            rig.pose.bones['foot.'+side].rotation_euler.x=-theta-knee
            wave=math.sin(phase+offset*2*math.pi)
            rig.pose.bones['arm.'+side].rotation_euler.x=-wave*(.48 if run else .25)
            rig.pose.bones['forearm.'+side].rotation_euler.x=-.75 if run else -.20
        rig.pose.bones['chest'].rotation_euler.z=.035*math.sin(phase)
    elif kind=='cast':
        strength=math.sin(math.pi*(.12+.76*t))
        rig.pose.bones['arm.R'].rotation_euler.x=-1.15*strength
        rig.pose.bones['forearm.R'].rotation_euler.x=-.45*strength
        rig.pose.bones['arm.L'].rotation_euler.x=-.35
        rig.pose.bones['chest'].rotation_euler.z=-.15*strength
    if ASSET_NAME == 'npc1':
        rig.pose.bones['arm.R'].rotation_euler.x = -.06 + .035 * math.sin(phase)
        rig.pose.bones['arm.R'].rotation_euler.z = .12
        rig.pose.bones['forearm.R'].rotation_euler.x = -.12
    orb.hide_render = kind != 'cast' or bool(CUSTOMIZE)
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
scene['에셋 단계']='정교화 2차: 연속 의상 메시, 관절 가중치, 곡면 머리카락, 봉제선과 손가락.'
bpy.context.view_layer.objects.active=rig
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Art/Blender'/ (ASSET_NAME + '.blend')))

# Single preview is useful without rendering every animation.
scene.render.resolution_x=768;scene.render.resolution_y=768
rig.rotation_euler.z=-math.pi/8
scene.render.filepath=str(OUT/'preview.png')
bpy.ops.render.render(write_still=True)
if PREVIEW: sys.exit(0)
scene.render.resolution_x=SIZE;scene.render.resolution_y=SIZE
# Read the saved render through Blender itself; no external image processing.
exports = [('idle',8),('walk',9),('run',9)] if CUSTOMIZE else [('walk',9),('run',9),('cast',4)]
for kind,columns in exports:
    width=columns*SIZE;height=8*SIZE
    atlas=array('f',[0])*(width*height*4)
    for row in range(8):
        rig.rotation_euler.z=-row*math.pi/4
        for col in range(columns):
            pose('idle' if kind in ('walk', 'run') and col == 0 else kind,
                 col/8 if kind == 'idle' else col/3 if kind=='cast' else (col-1)/8)
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
print(ASSET_NAME.upper() + '_ASSETS_COMPLETE',flush=True)
