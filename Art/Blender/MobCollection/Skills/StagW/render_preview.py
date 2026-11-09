"""W visual study: organic meadow growth using the actual stag; no runtime export."""
import math
import random
import sys
from pathlib import Path
import bpy
from mathutils import Vector

HERE = Path(__file__).resolve().parent
COLLECTION = HERE.parents[1]
sys.path.insert(0, str(COLLECTION))
import render_previews as review
from quadruped_locomotion import Baker, smooth

rng = random.Random(2457)
bpy.ops.wm.open_mainfile(filepath=str(COLLECTION/'stag/model.blend'))
scene = bpy.context.scene
rig = next(o for o in scene.objects if o.type == 'ARMATURE')
rig.animation_data.action = None
baker = Baker(rig, 'stag')
review.set_stage(.9, 6.5)
scene.render.film_transparent = False
scene.eevee.taa_render_samples = 32
scene.render.fps = 24
scene.world.color = (.035,.045,.042)
target = Vector((0,-.12,.80))
scene.camera.location = target+Vector((4.8,-7,5.2))
scene.camera.rotation_euler = (target-scene.camera.location).to_track_quat('-Z','Y').to_euler()
scene.camera.data.ortho_scale = 6.3

def material(name, color, glow=0):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color,1)
    m.use_nodes = True
    node = m.node_tree.nodes.get('Principled BSDF')
    node.inputs['Base Color'].default_value = (*color,1)
    node.inputs['Roughness'].default_value = .84
    node.inputs['Emission Color'].default_value = (*color,1)
    node.inputs['Emission Strength'].default_value = glow
    return m

mats = [material('W_STEM',(.08,.16,.045)), material('W_LEAF',(.18,.31,.08)),
        material('W_LEAF_TIP',(.32,.43,.14)), material('W_IVORY_PETAL',(.91,.80,.44),.24),
        material('W_PALE_PETAL',(1,.92,.70),.18), material('W_POLLEN_CORE',(.93,.51,.07),1.4)]
ground = material('W_REVIEW_EARTH',(.075,.095,.068))
bpy.ops.mesh.primitive_plane_add(size=200)
bpy.context.object.name = 'W_REVIEW_GROUND'
bpy.context.object.data.materials.append(ground)

class Form:
    def __init__(self): self.v, self.f, self.mi = [], [], []
    def face(self, points, mat):
        i=len(self.v); self.v.extend(points); self.f.append(tuple(range(i,i+len(points)))); self.mi.append(mat)
    def blade(self, origin, angle, length, width, rise, mat, curl=0):
        # Tapered folded surfaces, with a curved centerline and irregular tip.
        direction=Vector((math.cos(angle),math.sin(angle),0)); side=Vector((-direction.y,direction.x,0))
        rows=[]
        for j in range(7):
            t=j/6; center=Vector(origin)+direction*(length*t)+side*(curl*math.sin(t*math.pi)*length)
            center.z += rise*t + length*.14*math.sin(t*math.pi)
            half=width*(math.sin(math.pi*t)**.8)*.5
            rows.append([center-side*half, center+Vector((0,0,half*.20)), center+side*half])
        for a,b in zip(rows,rows[1:]):
            for k in range(2): self.face([a[k],b[k],b[k+1],a[k+1]],mat)
    def tube(self, points, radius, mat):
        for j,(a,b) in enumerate(zip(points,points[1:])):
            for k in range(5):
                ang=k*math.tau/5; nxt=(k+1)*math.tau/5
                ra=radius*(1-.6*j/len(points)); rb=radius*(1-.6*(j+1)/len(points))
                self.face([Vector(a)+Vector((math.cos(ang)*ra,math.sin(ang)*ra,0)),
                           Vector(a)+Vector((math.cos(nxt)*ra,math.sin(nxt)*ra,0)),
                           Vector(b)+Vector((math.cos(nxt)*rb,math.sin(nxt)*rb,0)),
                           Vector(b)+Vector((math.cos(ang)*rb,math.sin(ang)*rb,0))],mat)
    def mesh(self,name):
        data=bpy.data.meshes.new(name); data.from_pydata(self.v,[],self.f); data.update()
        for m in mats: data.materials.append(m)
        for p,idx in zip(data.polygons,self.mi): p.material_index=idx; p.use_smooth=True
        return data

flowers=[]
for variant in range(8):
    f=Form(); height=rng.uniform(.18,.34); bend=rng.uniform(-.10,.10)
    stem=[(bend*(j/6)**2,.035*math.sin(j/6*2),height*j/6) for j in range(7)]
    f.tube(stem,.009,0)
    for j in range(3):
        f.blade(stem[1+j],j*2.4+variant,.17,.065,.055,1+j%2,.2)
    top=Vector(stem[-1]); petals=5+variant%3
    for j in range(petals):
        angle=j*math.tau/petals+rng.uniform(-.12,.12)
        f.blade(top,angle,rng.uniform(.085,.145),rng.uniform(.07,.10),rng.uniform(-.025,.025),3+j%2,.15)
    for j in range(7):
        a=j*2.399; off=Vector((math.cos(a)*.023,math.sin(a)*.023,.012))
        f.blade(top+off,a,.018,.012,.018,5)
    flowers.append(f.mesh(f'W_FLOWER_VARIANT_{variant:02}'))

grass=[]
for variant in range(6):
    f=Form()
    for j in range(5):
        angle=rng.uniform(0,math.tau)
        f.blade((0,0,0),angle,rng.uniform(.06,.18),rng.uniform(.014,.035),rng.uniform(.09,.24),1+j%2,.35)
    grass.append(f.mesh(f'W_GRASS_VARIANT_{variant:02}'))

plants=[]
for i in range(144):
    # Uneven drifts rather than rings or a regular grid; preserve some bare ground.
    angle=rng.uniform(0,math.tau); radius=math.sqrt(rng.random())*2.25
    x,y=math.cos(angle)*radius,math.sin(angle)*radius
    if abs(x)<.36 and -.9<y<.8: x += .48 if x>=0 else -.48
    flower=i<48
    obj=bpy.data.objects.new(f'W_{"FLOWER" if flower else "GRASS"}_{i:03}',
                             flowers[i%8] if flower else grass[i%6])
    scene.collection.objects.link(obj); obj.location=(x,y,.012)
    obj.rotation_euler.z=rng.uniform(0,math.tau)
    scale=rng.uniform(.65,1.25)
    delay=.60+radius*.29+rng.uniform(-.13,.18)
    plants.append((obj,scale,delay,rng.random()*math.tau))

pollen=material('W_FLOATING_POLLEN',(1,.72,.24),2.0)
data=bpy.data.meshes.new('W_POLLEN_POINT')
data.from_pydata([(-1,0,0),(1,0,0),(0,-.5,0),(0,.5,0),(0,0,1),(0,0,-1)],[],
                [(0,2,4),(2,1,4),(1,3,4),(3,0,4),(2,0,5),(1,2,5),(3,1,5),(0,3,5)])
data.materials.append(pollen)
motes=[]
for i in range(48):
    ob=bpy.data.objects.new(f'W_POLLEN_{i:02}',data); scene.collection.objects.link(ob)
    a=rng.random()*math.tau; r=math.sqrt(rng.random())*2.15
    motes.append((ob,r*math.cos(a),r*math.sin(a),rng.random(),rng.uniform(.007,.017)))

def pose(age, key=False):
    frame=round(age*24)+1
    scene.frame_set(frame)
    baker.pose('idle',age/4)
    bow=smooth(age/.6)*(1-smooth((age-.8)/.7))
    rig.pose.bones['head'].rotation_euler.x += .19*bow
    for bone in rig.pose.bones:
        if key:
            bone.keyframe_insert('location',frame=frame)
            bone.keyframe_insert('rotation_euler',frame=frame)
    for ob,scale,delay,phase in plants:
        growth=smooth((age-delay)/.55)*(1-smooth((age-4.15)/.65))
        ob.scale=(max(.0001,scale*growth),)*3
        ob.rotation_euler.x=.055*math.sin(age*1.8+phase)*growth
        ob.rotation_euler.y=.04*math.cos(age*1.5+phase)*growth
        if key:
            ob.keyframe_insert('scale',frame=frame); ob.keyframe_insert('rotation_euler',frame=frame)
    for i,(ob,x,y,phase,size) in enumerate(motes):
        if age<.62 and i<12:
            # Initial lights draw close to the crown; no geometric orbit.
            head=rig.pose.bones['head'].head
            ob.location=(head.x+(phase-.5)*.7,head.y+.12*math.sin(i),head.z+.3+phase*.35)
            visibility=math.sin(age/.62*math.pi)
        else:
            progress=(age*.28+phase)%1
            ob.location=(x+.09*math.sin(age+i),y+.06*math.cos(age*.8+i),.08+progress*.75)
            visibility=smooth((age-1.0)/.55)*(1-smooth((age-4.0)/.8))*math.sin(progress*math.pi)
        ob.scale=(max(.00001,size*visibility),)*3
        if key: ob.keyframe_insert('location',frame=frame); ob.keyframe_insert('scale',frame=frame)
    bpy.context.view_layer.update()

for frame in range(1,117): pose((frame-1)/24,True)
rig.animation_data.action.name='MOB_STAG_W_PREVIEW'
scene.frame_start,scene.frame_end=1,116
pose(2.8)
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'preview.blend'))
scene.render.resolution_x,scene.render.resolution_y=960,760
for name,age in [('prepare',.42),('bloom',1.15),('sustain',2.8)]:
    pose(age); review.render(HERE/(name+'.png'))
if '--animate' in sys.argv:
    scene.render.resolution_x,scene.render.resolution_y=640,500
    scene.eevee.taa_render_samples=16
    for i in range(40):
        pose(i/8); review.render(HERE/'Frames'/f'{i:03}.png')
print('STAG_W_PREVIEW_READY plants=144 pollen=48 mesh_variants=14 runtime=false',flush=True)
