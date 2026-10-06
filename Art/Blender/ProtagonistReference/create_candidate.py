"""Create an independent, rigged portrait-reference candidate and matched previews."""
import bpy
import math
from pathlib import Path
from mathutils import Vector
from array import array

OUT = Path(__file__).resolve().parent
SOURCE = OUT.parent / 'protagonist.blend'
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
bpy.context.preferences.filepaths.save_version = 0
scene = bpy.context.scene
rig = bpy.data.objects['주인공_리그']
if rig.animation_data:
    rig.animation_data.action = None
for bone in rig.pose.bones:
    bone.matrix_basis.identity()
rig.rotation_euler = (0, 0, 0)
for obj in bpy.data.objects:
    if '주술 구체' in obj.name:
        obj.hide_render = True
scene.render.engine = 'BLENDER_WORKBENCH'
scene.render.resolution_x = 800
scene.render.resolution_y = 900
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.film_transparent = False
scene.display.render_aa = '32'
sh = scene.display.shading
sh.light = 'STUDIO'
sh.color_type = 'MATERIAL'
sh.show_shadows = True
sh.show_cavity = True
sh.cavity_type = 'BOTH'
sh.curvature_ridge_factor = 1.1
sh.curvature_valley_factor = 0.7
sh.show_specular_highlight = False
sh.background_type = 'WORLD'
scene.world.color = (.055, .065, .095)
scene.view_settings.view_transform = 'Standard'
camera = scene.camera
camera.data.type = 'ORTHO'

def render(label, close=False, angle=0):
    target = Vector((0, 0, 2.015 if close else 1.32))
    camera.location = target + Vector((math.sin(angle)*6, -math.cos(angle)*6, .2 if close else 1.25))
    camera.rotation_euler = (target-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.ortho_scale = 1.32 if close else 2.95
    scene.render.filepath = str(OUT / (label+'.png'))
    bpy.ops.render.render(write_still=True)

render('original_face', True)
render('original_full', False, .35)

def material(name, color):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color,1)
    mat.use_nodes = True
    bs = mat.node_tree.nodes.get('Principled BSDF')
    bs.inputs['Base Color'].default_value = (*color,1)
    bs.inputs['Roughness'].default_value = .7
    return mat

white = material('새 은백색 머리', (.88,.92,1))
shadow = material('새 머리 그늘', (.57,.65,.83))
purple = material('보라색 깃 테두리', (.18,.075,.38))
cloth = material('짙은 보라색 제복', (.035,.022,.09))
blind = material('검은 안대 천', (.018,.022,.055))
fold = material('안대 주름 빛', (.095,.12,.22))
mouth = material('표정 선', (.105,.055,.085))
for mat in bpy.data.materials:
    color = None
    if mat.name == '피부': color = (.9,.78,.74)
    if mat.name == '먹빛 남색 코트': color = (.045,.025,.11)
    if mat.name == '회청색 봉제선': color = (.16,.075,.30)
    if mat.name == '코트 안감': color = (.075,.035,.17)
    if color:
        mat.diffuse_color = (*color,1)
        mat.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value = (*color,1)

prefixes = ('머리 바탕','흐르는 은발','옆으로 흐르는 앞머리','옆머리','주름진 안대','안대 봉제선','접힌 높은 깃','깃 스티치','입술','은색 잠금 단추','비대칭 앞섶')
for obj in list(bpy.data.objects):
    if obj.name.startswith(prefixes):
        bpy.data.objects.remove(obj, do_unlink=True)

def mesh(name, verts, faces, mat, joint='head', smooth=True):
    data = bpy.data.meshes.new(name)
    data.from_pydata(verts, [], faces)
    data.update()
    ob = bpy.data.objects.new(name, data)
    scene.collection.objects.link(ob)
    data.materials.append(mat)
    for p in data.polygons: p.use_smooth = smooth
    vg = ob.vertex_groups.new(name=joint)
    vg.add(list(range(len(verts))),1,'REPLACE')
    mod = ob.modifiers.new('뼈대 변형','ARMATURE')
    mod.object = rig
    ob.parent = rig
    return ob

def line(name, points, radius, mat, joint='head'):
    curve = bpy.data.curves.new(name,'CURVE')
    curve.dimensions = '3D'
    curve.bevel_depth = radius
    curve.bevel_resolution = 3
    spline = curve.splines.new('BEZIER')
    spline.bezier_points.add(len(points)-1)
    for p,co in zip(spline.bezier_points, points):
        p.co = co
        p.handle_left_type = p.handle_right_type = 'AUTO'
    ob = bpy.data.objects.new(name,curve)
    scene.collection.objects.link(ob)
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.convert(target='MESH')
    ob.data.materials.append(mat)
    vg = ob.vertex_groups.new(name=joint)
    vg.add(list(range(len(ob.data.vertices))),1,'REPLACE')
    mod = ob.modifiers.new('뼈대 변형','ARMATURE')
    mod.object = rig
    ob.parent = rig
    return ob

def lock(name, controls, width, mat):
    p = list(map(Vector,controls))
    verts=[]
    rings=12
    sides=8
    for i in range(rings):
        t=i/(rings-1)
        u=1-t
        center=u**3*p[0]+3*u*u*t*p[1]+3*u*t*t*p[2]+t**3*p[3]
        tangent=(3*u*u*(p[1]-p[0])+6*u*t*(p[2]-p[1])+3*t*t*(p[3]-p[2])).normalized()
        across=tangent.cross(Vector((0,1,0))).normalized()
        normal=tangent.cross(across).normalized()
        radius=width*(.8+.4*math.sin(math.pi*t))*(1-t)**.75+.0005
        for j in range(sides):
            a=2*math.pi*j/sides
            verts.append(center+across*math.cos(a)*radius+normal*math.sin(a)*radius*.48)
    faces=[(i*sides+j,i*sides+(j+1)%sides,(i+1)*sides+(j+1)%sides,(i+1)*sides+j) for i in range(rings-1) for j in range(sides)]
    faces.extend([tuple(range(sides-1,-1,-1)),tuple((rings-1)*sides+j for j in range(sides))])
    ob=mesh(name,verts,faces,mat)
    mod=ob.modifiers.new('머리 곡면','SUBSURF'); mod.levels=1

# Overlapping broad tapered clumps form a lifted crown, with shorter side fans.
for row in range(3):
    for i in range(9):
        x=(i-4)*.047
        y=.105-row*.115
        z=2.09 + (.035 if row==0 else 0)
        tipx=x*1.65 + .018*math.sin(i*2+row)
        tipz=2.56-abs(x)*1.0+.035*math.sin(i*2.3+row)
        lock('위로 솟은 은발',[(x,y,z),(x*.95,y-.025,2.27),(tipx*.8,y+.025,tipz-.06),(tipx,y+.04,tipz)],.059, white if row>0 else shadow)
for side in (-1,1):
    for i in range(4):
        lock('관자놀이 은발',[(side*.17,.04,2.09+i*.03),(side*.23,.03,2.12+i*.035),(side*.28,.02,2.15+i*.055),(side*(.28+i*.018),.01,2.17+i*.06)],.04,white)

# Fitted cloth wrap: upper edge straight, lower edge dips over both cheekbones.
verts=[]; n=96
for j in range(5):
    t=j/4
    for i in range(n):
        a=2*math.pi*i/n
        x=.253*math.cos(a)
        y=.217*math.sin(a)
        front=max(0,-math.sin(a))
        lower=1.958-.026*front*math.sin(abs(x)/.253*math.pi)**2
        z=lower*(1-t)+(2.065+.008*abs(x)/.253)*t
        bulge=.0025*math.sin(a*9+t*9)*math.sin(math.pi*t)
        verts.append((x*(1+bulge),y*(1+bulge),z))
faces=[(j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i) for j in range(4) for i in range(n)]
ob=mesh('입체 천 안대',verts,faces,blind)
mod=ob.modifiers.new('천 두께','SOLIDIFY'); mod.thickness=.006
for side in (-1,1):
    for k in range(3):
        pts=[]
        for x,z in [(.08,2.018+k*.012),(.16,2.009+k*.015),(.229,2.02+k*.015)]:
            pts.append((side*x,-.22*math.sqrt(1-(x/.256)**2)-.003,z))
        line('안대 접힌 주름',pts,.0023,fold)
line('자신감 있는 미소',[(-.085,-.15,1.829),(-.045,-.168,1.813),(0,-.174,1.809),(.045,-.168,1.813),(.085,-.15,1.829)],.0032,mouth)
line('아랫입술',[(-.024,-.16,1.796),(0,-.164,1.792),(.024,-.16,1.796)],.0015,mouth)

# Open thick stand collar around the neck; no flat cap across the face.
verts=[]; n=64
for j in range(3):
    for i in range(n):
        a=-math.pi/2+.12+(2*math.pi-.24)*i/(n-1)
        x=math.cos(a); y=math.sin(a)
        z=(1.50,1.64,1.77+.025*abs(x))[j]
        rx=(.25,.29,.305)[j]; ry=(.18,.205,.21)[j]
        verts.append((rx*x,ry*y,z))
faces=[(j*n+i,j*n+i+1,(j+1)*n+i+1,(j+1)*n+i) for j in range(2) for i in range(n-1)]
ob=mesh('높게 세운 입체 옷깃',verts,faces,cloth,'chest')
mod=ob.modifiers.new('두꺼운 옷깃','SOLIDIFY'); mod.thickness=.016
mod=ob.modifiers.new('옷깃 모서리','BEVEL'); mod.width=.005; mod.segments=3
line('보라색 옷깃 상단',verts[2*n:],.009,purple,'chest')
for i in (0,n-1): line('옷깃 앞단',[verts[i],verts[n+i],verts[2*n+i]],.008,purple,'chest')
line('제복 앞섶',[(.018,-.179,1.18),(.018,-.195,1.38),(.018,-.198,1.55)],.008,purple,'chest')

rig['candidate_status']='Independent reference candidate; not applied to game assets'
rig['original_file']=str(SOURCE)
render('candidate_face',True)
render('candidate_full',False,.35)
render('candidate_three_quarter',True,.55)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'protagonist_candidate.blend'))

def compare(kind):
    images=[bpy.data.images.load(str(OUT/(prefix+'_'+kind+'.png')),check_existing=False) for prefix in ('original','candidate')]
    w,h=images[0].size
    data=[]
    for im in images:
        pixels=array('f',[0])*(w*h*4); im.pixels.foreach_get(pixels); data.append(pixels)
    combined=array('f',[0])*(w*2*h*4)
    for y in range(h):
        for col in range(2):
            offset=(y*w*2+col*w)*4
            combined[offset:offset+w*4]=data[col][y*w*4:(y+1)*w*4]
    im=bpy.data.images.new('Comparison '+kind,width=w*2,height=h)
    im.pixels.foreach_set(combined)
    im.filepath_raw=str(OUT/('comparison_'+kind+'.png'))
    im.file_format='PNG'; im.save()
compare('face'); compare('full')
print('Candidate complete; source and game assets preserved.')
