"""Independent five-stage character refinement; never writes active game assets."""
import ast
import bpy
import math
from pathlib import Path
from mathutils import Vector
from array import array

OUT = Path(__file__).resolve().parent
SOURCE = OUT.parent/'SideVolumeStudy/protagonist_volume_candidate.blend'
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
bpy.context.preferences.filepaths.save_version = 0
scene = bpy.context.scene
rig = bpy.data.objects['주인공_리그']
rig.animation_data.action = None
rig.rotation_euler = (0,0,0)
for bone in rig.pose.bones: bone.matrix_basis.identity()
camera = scene.camera
scene.render.resolution_x = 640
scene.render.resolution_y = 800
scene.render.resolution_percentage = 100
scene.render.film_transparent = False
scene.render.image_settings.color_mode = 'RGBA'
scene.render.image_settings.file_format = 'PNG'
scene.view_settings.view_transform = 'Standard'
scene.world.color = (.055,.065,.095)

def render(name, angle, close=False):
    target=Vector((0,0,2.0 if close else 1.31))
    camera.location=target+Vector((6*math.sin(angle),-6*math.cos(angle),.20 if close else .55))
    camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.ortho_scale=1.3 if close else 2.86
    scene.render.filepath=str(OUT/(name+'.png'))
    bpy.ops.render.render(write_still=True)

views=[('front',0),('right',math.pi/2),('back',math.pi),('left',-math.pi/2)]
scene.render.engine='BLENDER_WORKBENCH'
for name,a in views: render('before_'+name,a)
render('before_portrait',.35,True)

# Reuse only mesh construction helpers, not the original asset-writing program.
tree=ast.parse((OUT.parent/'ProtagonistReference/create_candidate.py').read_text(encoding='utf-8'))
for node in tree.body:
    if isinstance(node,ast.FunctionDef) and node.name in ('mesh','line','lock'):
        exec(compile(ast.Module(body=[node],type_ignores=[]),'<construction helpers>','exec'))
white=bpy.data.materials['새 은백색 머리']
shadow=bpy.data.materials['새 머리 그늘']
cloth=bpy.data.materials['짙은 보라색 제복']
trim=bpy.data.materials['보라색 깃 테두리']
for ob in list(bpy.data.objects):
    if ob.name.startswith(('위로 솟은 은발','관자놀이 은발','측면 은발 층','은발 안쪽 볼륨','높게 세운 입체 옷깃','보라색 옷깃 상단','옷깃 앞단')):
        bpy.data.objects.remove(ob,do_unlink=True)

# Stage 2: continuous hair volume plus three staggered, radial clump tiers.
bpy.ops.mesh.primitive_uv_sphere_add(segments=40,ring_count=24,location=(0,.016,2.155))
cap=bpy.context.object
cap.scale=(.231,.245,.155)
bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
cap.name='은발 내부 덩어리'
cap.data.materials.append(white)
for p in cap.data.polygons: p.use_smooth=True
group=cap.vertex_groups.new(name='head');group.add(list(range(len(cap.data.vertices))),1,'REPLACE')
mod=cap.modifiers.new('뼈대 변형','ARMATURE');mod.object=rig;cap.parent=rig
for tier,count in enumerate((14,12,9)):
    radius=(.20,.14,.07)[tier]
    for i in range(count):
        a=i*math.tau/count+tier*.28
        rx=radius*math.cos(a); ry=.015+radius*1.08*math.sin(a)
        rootz=2.10+tier*.065
        spread=(.29,.235,.16)[tier]
        tx=spread*math.cos(a)+.025
        ty=.025+spread*math.sin(a)+.045
        top=(2.36,2.46,2.55)[tier]+.028*math.sin(i*2.17+tier)
        lock('큰 은발 가닥',[(rx,ry,rootz),(rx*1.15,ry*1.1,rootz+.16),
                            (tx-.025,ty-.025,top-.09),(tx,ty,top)],
             (.075,.067,.057)[tier]*(.92+.08*math.cos(i)),white if i%5 else shadow)
# Short temple wisps integrate the hairline with the blindfold instead of a bare scalp strip.
for side in (-1,1):
    for i in range(4):
        y=-.16+i*.095
        lock('관자놀이 잔가닥',[(side*.18,y,2.15),(side*.245,y,2.17),
                              (side*.26,y+.025,2.11),(side*.23,y+.04,2.055)],.035,white)

# Stage 3: subtler nose projection and a less rounded lower face.
for ob in bpy.data.objects:
    if ob.type!='MESH': continue
    if ob.name.startswith(('얼굴 조형','자신감 있는 미소','아랫입술')):
        for v in ob.data.vertices:
            t=max(0,min(1,(1.94-v.co.z)/.18))
            v.co.x*=1-.085*t
    if ob.name.startswith('콧대'):
        for v in ob.data.vertices:
            v.co.x*=.82
            v.co.y=-.215+(v.co.y+.215)*.62

# Stage 3: substantial open collar, asymmetrical overlapping front and restrained seams.
n=72;verts=[]
for ring in range(4):
    for i in range(n):
        a=-math.pi/2+.11+(math.tau-.22)*i/(n-1)
        x,y=math.cos(a),math.sin(a)
        z=(1.50,1.59,1.745,1.765)[ring]+(.025*abs(x) if ring>=2 else 0)
        verts.append(((.25,.286,.302,.305)[ring]*x,(.25,.278,.291,.295)[ring]*y,z))
faces=[(j*n+i,j*n+i+1,(j+1)*n+i+1,(j+1)*n+i) for j in range(3) for i in range(n-1)]
collar=mesh('두께 있는 제복 깃',verts,faces,cloth,'chest')
mod=collar.modifiers.new('깃 안감 두께','SOLIDIFY');mod.thickness=.016
mod=collar.modifiers.new('부드러운 깃 모서리','BEVEL');mod.width=.004;mod.segments=3
line('깃 가장자리 봉제',verts[3*n:],.0045,trim,'chest')
for i in (0,n-1): line('깃 앞단 봉제',[verts[j*n+i] for j in range(4)],.0045,trim,'chest')
flap=mesh('겹친 앞섶',[(.015,-.294,1.54),(.097,-.291,1.58),(.087,-.304,1.746),(.018,-.305,1.758)],[(0,1,2,3)],cloth,'chest')
mod=flap.modifiers.new('앞섶 두께','SOLIDIFY');mod.thickness=.009
line('앞섶 가장자리',[(.097,-.296,1.58),(.087,-.309,1.746),(.018,-.31,1.758)],.003,trim,'chest')
# Seam lines follow existing sleeve surfaces and keep their bone assignment.
for side,joint in [(-1,'L'),(1,'R')]:
    line('소매 접힘',[(side*.31,-.105,1.20),(side*.36,-.135,1.18),(side*.40,-.11,1.19)],.003,trim,'forearm.'+joint)

for name,a in views: render('shape_'+name,a)
render('shape_portrait',.35,True)

# Stage 4: real Eevee cel materials; three light bands, emission output avoids double lighting.
scene.render.engine='BLENDER_EEVEE'
scene.render.film_transparent=False
scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.055,.065,.095,1)
scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.3
palette={
    '새 은백색 머리':(.87,.93,1), '새 머리 그늘':(.69,.77,.91),
    '피부':(.94,.79,.72), '짙은 보라색 제복':(.105,.055,.21),
    '먹빛 남색 코트':(.105,.055,.21),'보라색 깃 테두리':(.25,.14,.43),
    '회청색 봉제선':(.23,.14,.36),'코트 안감':(.12,.07,.24),
    '검은 안대 천':(.034,.038,.082),'안대와 가죽':(.025,.03,.06),
    '안대 주름 빛':(.12,.15,.25)}
for mat in bpy.data.materials:
    if mat.name=='푸른 주력': continue
    base=palette.get(mat.name,tuple(mat.diffuse_color[:3]))
    mat.diffuse_color=(*base,1)
    mat.use_nodes=True
    nodes=mat.node_tree.nodes;nodes.clear();links=mat.node_tree.links
    output=nodes.new('ShaderNodeOutputMaterial')
    diffuse=nodes.new('ShaderNodeBsdfDiffuse');diffuse.inputs['Color'].default_value=(1,1,1,1)
    rgb=nodes.new('ShaderNodeShaderToRGB')
    ramp=nodes.new('ShaderNodeValToRGB');ramp.color_ramp.interpolation='CONSTANT'
    ramp.color_ramp.elements.remove(ramp.color_ramp.elements[1])
    for i,(pos,factor) in enumerate(((.0,.40),(.32,.70),(.66,1.0))):
        element=ramp.color_ramp.elements[0] if i==0 else ramp.color_ramp.elements.new(pos)
        element.position=pos;element.color=(*(c*factor for c in base),1)
    emission=nodes.new('ShaderNodeEmission')
    links.new(diffuse.outputs[0],rgb.inputs[0]);links.new(rgb.outputs[0],ramp.inputs[0])
    links.new(ramp.outputs[0],emission.inputs['Color']);links.new(emission.outputs[0],output.inputs[0])
for ob in list(bpy.data.objects):
    if ob.type=='LIGHT': bpy.data.objects.remove(ob,do_unlink=True)
for name,location,power,size in [('주광',(-3,-4,6),750,4),('보조광',(4,-1,3),300,5),('윤곽광',(0,4,4),600,3)]:
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size
    ob=bpy.data.objects.new(name,data);scene.collection.objects.link(ob);ob.location=location
    ob.rotation_euler=(Vector((0,0,1.4))-ob.location).to_track_quat('-Z','Y').to_euler()
for name,a in views: render('final_'+name,a)
render('final_portrait',.35,True)
render('final_perspective',.65)
rig['candidate_status']='Five-stage refinement candidate; active game assets unchanged'
scene['review_notes']='Orthographic reference views are model studies, not newly supplied original character sheets.'
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'protagonist_refined.blend'))

def sheet(name,names,columns):
    ims=[bpy.data.images.load(str(OUT/(n+'.png')),check_existing=False) for n in names]
    w,h=ims[0].size;rows=(len(ims)+columns-1)//columns
    pixels=array('f',[0])*(w*columns*h*rows*4)
    for index,im in enumerate(ims):
        source=array('f',[0])*(w*h*4);im.pixels.foreach_get(source)
        col=index%columns;row=rows-1-index//columns
        for y in range(h):
            offset=((row*h+y)*w*columns+col*w)*4
            pixels[offset:offset+w*4]=source[y*w*4:(y+1)*w*4]
    result=bpy.data.images.new(name,width=w*columns,height=h*rows)
    result.pixels.foreach_set(pixels);result.filepath_raw=str(OUT/(name+'.png'));result.file_format='PNG';result.save()
sheet('portrait_comparison',['before_portrait','shape_portrait','final_portrait'],3)
sheet('turnaround',['final_front','final_right','final_back','final_left'],4)
sheet('shape_comparison',['before_front','shape_front','before_right','shape_right'],4)
print('REFINEMENT_READY',flush=True)
