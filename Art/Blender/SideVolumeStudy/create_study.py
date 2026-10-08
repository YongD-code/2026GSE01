"""Stage one: improve side silhouette without replacing approved game assets."""
import bpy
import math
import ast
from array import array
from pathlib import Path
from mathutils import Vector

OUT = Path(__file__).resolve().parent
SOURCE = OUT.parent / 'ProtagonistReference/protagonist_candidate.blend'
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
bpy.context.preferences.filepaths.save_version = 0
scene = bpy.context.scene
rig = bpy.data.objects['주인공_리그']
rig.animation_data.action = None
rig.rotation_euler = (0, 0, 0)
for bone in rig.pose.bones:
    bone.matrix_basis.identity()
scene.render.engine = 'BLENDER_WORKBENCH'
scene.render.resolution_x = 500
scene.render.resolution_y = 720
scene.render.resolution_percentage = 100
scene.render.film_transparent = False
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'
scene.display.render_aa = '32'
camera = scene.camera
camera.data.type = 'ORTHO'

def render(name, angle, close=False):
    target = Vector((0, 0, 2.01 if close else 1.32))
    camera.location = target + Vector((6 * math.sin(angle), -6 * math.cos(angle), .15 if close else .5))
    camera.rotation_euler = (target-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.ortho_scale = 1.4 if close else 2.85
    scene.render.filepath = str(OUT / (name + '.png'))
    bpy.ops.render.render(write_still=True)

views = [('front',0),('right',math.pi/2),('back',math.pi),('left',-math.pi/2)]
for name, angle in views:
    render('original_'+name,angle)
render('original_profile',math.pi/2,True)

def smooth(a,b,x):
    t=max(0,min(1,(x-a)/(b-a)))
    return t*t*(3-2*t)

# Baked rest vertices retain vertex groups, armature modifiers and existing actions.
# Only depth changes: front/back width, height and limb positions remain consistent.
for ob in bpy.data.objects:
    if ob.type != 'MESH' or ob.parent != rig or ob.name.startswith('주술 구체'):
        continue
    for vert in ob.data.vertices:
        z=vert.co.z
        if ob.name.startswith(('위로 솟은 은발','관자놀이 은발')):
            vert.co.y *= 1.8
            vert.co.y += .055 * smooth(2.12,2.55,z)
        else:
            depth = 1.18 + .25*smooth(.8,1.3,z) - .15*smooth(1.65,1.9,z)
            vert.co.y *= depth
    ob.data.update()

# Fill the exposed scalp and break the three-row silhouette with staggered side clumps.
helpers = ast.parse((SOURCE.parent / 'create_candidate.py').read_text(encoding='utf-8'))
for node in helpers.body:
    if isinstance(node, ast.FunctionDef) and node.name in ('mesh', 'lock'):
        exec(compile(ast.Module(body=[node], type_ignores=[]), '<mesh helpers>', 'exec'))
white = bpy.data.materials['새 은백색 머리']
bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, location=(0,.015,2.17))
cap = bpy.context.object
cap.scale = (.205,.18,.12)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
cap.name='은발 안쪽 볼륨'
cap.data.materials.append(white)
for face in cap.data.polygons:
    face.use_smooth=True
vg=cap.vertex_groups.new(name='head')
vg.add(list(range(len(cap.data.vertices))),1,'REPLACE')
modifier=cap.modifiers.new('뼈대 변형','ARMATURE')
modifier.object=rig
cap.parent=rig
for side in (-1,1):
    for i in range(9):
        y=(i-4)*.049+.025
        top=2.44-.11*abs((i-4)/4)+.018*math.sin(i*2)
        x=side*(.165+.012*math.sin(i))
        lock('측면 은발 층',[(x,y,2.13),(x*1.05,y*1.05,2.25),
                            (x*.95,y*1.4,top-.06),(x*.8,y*1.5,top)],.057,white)

rig['candidate_status']='Stage 1 side-volume study; awaiting selection; not applied to game'
rig['original_file']=str(SOURCE)
for name, angle in views:
    render('candidate_'+name,angle)
render('candidate_profile',math.pi/2,True)
render('candidate_perspective',.65)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'protagonist_volume_candidate.blend'))

def sheet(name, names, columns):
    images=[bpy.data.images.load(str(OUT/(n+'.png')),check_existing=False) for n in names]
    w,h=images[0].size
    rows=(len(names)+columns-1)//columns
    pixels=array('f',[0])*(w*columns*h*rows*4)
    for index, im in enumerate(images):
        source=array('f',[0])*(w*h*4)
        im.pixels.foreach_get(source)
        col=index%columns; row=rows-1-index//columns
        for y in range(h):
            offset=((row*h+y)*w*columns+col*w)*4
            pixels[offset:offset+w*4]=source[y*w*4:(y+1)*w*4]
    result=bpy.data.images.new(name,width=w*columns,height=h*rows)
    result.pixels.foreach_set(pixels)
    result.filepath_raw=str(OUT/(name+'.png'))
    result.file_format='PNG'
    result.save()

sheet('side_comparison',['original_right','candidate_right','original_left','candidate_left'],4)
sheet('front_back_comparison',['original_front','candidate_front','original_back','candidate_back'],4)
sheet('profile_comparison',['original_profile','candidate_profile'],2)
print('STUDY_COMPLETE',flush=True)
