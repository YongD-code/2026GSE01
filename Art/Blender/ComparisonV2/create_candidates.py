"""Independent comparison candidates. Never writes game assets or original models."""
import bpy
import math
from pathlib import Path
from mathutils import Vector
from array import array

BASE = Path(__file__).resolve().parents[1]
OUT = Path(__file__).resolve().parent
bpy.context.preferences.filepaths.save_version = 0


def build(name):
    bpy.ops.wm.open_mainfile(filepath=str(BASE / (name + '.blend')))
    scene = bpy.context.scene
    rig = next(o for o in scene.objects if o.type == 'ARMATURE')
    rig.animation_data_clear()
    for p in rig.pose.bones:
        p.rotation_euler = (0, 0, 0)
        p.location = (0, 0, 0)
    bpy.context.view_layer.update()
    first = name == 'npc1'
    def mat(label, rgb, rough=.6):
        m = bpy.data.materials.new(label)
        m.diffuse_color = (*rgb, 1)
        m.use_nodes = True
        shader = m.node_tree.nodes.get('Principled BSDF')
        shader.inputs['Base Color'].default_value = (*rgb, 1)
        shader.inputs['Roughness'].default_value = rough
        return m
    hair = mat('자연스러운 머리 바탕', (.17,.065,.034) if first else (.51,.20,.20), .62)
    strand = mat('머릿결 음영', (.10,.036,.022) if first else (.34,.10,.105))
    sheen = mat('머릿결 밝은 면', (.23,.091,.046) if first else (.62,.28,.25))
    lidmat = mat('눈꺼풀 피부', (.62,.40,.31))
    lash = mat('속눈썹과 눈썹', (.046,.022,.019))
    sclera = mat('차분한 눈 흰자', (.59,.54,.46))
    iris = mat('홍채', (.13,.06,.023))
    pupil = mat('동공', (.007,.008,.009))
    lip = mat('입술 음영', (.31,.14,.11))
    # Remove conspicuous separated hair tubes, protruding eye balls and nose pieces.
    prefixes = ['단발', '옆가르마', '정수리', '분홍 머리', '짧은', '짧고', '앞 정수리',
                '눈매', '눈 흰자', '홍채', '동공', '눈 반사광', '눈썹', '아래 눈꺼풀', '콧대', '입술']
    for ob in list(scene.objects):
        if any(ob.name.startswith(p) for p in prefixes):
            bpy.data.objects.remove(ob, do_unlink=True)

    def mesh(label, vertices, faces, material, bone='head'):
        data = bpy.data.meshes.new(label)
        data.from_pydata(vertices, [], faces)
        data.update()
        ob = bpy.data.objects.new(label, data)
        scene.collection.objects.link(ob)
        data.materials.append(material)
        for poly in data.polygons:
            poly.use_smooth = True
        group = ob.vertex_groups.new(name=bone)
        group.add(list(range(len(vertices))), 1, 'REPLACE')
        ob.parent = rig
        modifier = ob.modifiers.new('뼈대', 'ARMATURE')
        modifier.object = rig
        return ob

    def line(label, points, radius, material, bone='head'):
        curve = bpy.data.curves.new(label, 'CURVE')
        curve.dimensions = '3D'
        curve.bevel_depth = radius
        curve.bevel_resolution = 2
        sp = curve.splines.new('BEZIER')
        sp.bezier_points.add(len(points)-1)
        for p,xyz in zip(sp.bezier_points,points):
            p.co = xyz
            p.handle_left_type = p.handle_right_type = 'AUTO'
        ob = bpy.data.objects.new(label, curve)
        scene.collection.objects.link(ob)
        bpy.ops.object.select_all(action='DESELECT')
        ob.select_set(True)
        bpy.context.view_layer.objects.active = ob
        bpy.ops.object.convert(target='MESH')
        ob = bpy.context.object
        ob.data.materials.append(material)
        ob.parent = rig
        group = ob.vertex_groups.new(name=bone)
        group.add(list(range(len(ob.data.vertices))), 1, 'REPLACE')
        mod = ob.modifiers.new('뼈대', 'ARMATURE');mod.object = rig
        return ob

    # Shape the existing head as one continuous face: nose bridge, cheeks, mouth plane.
    for ob in scene.objects:
        if ob.type != 'MESH': continue
        if ob.name.startswith('얼굴 조형'):
            for v in ob.data.vertices:
                x,y,z = v.co
                if y < -.075:
                    nose = .045*math.exp(-(x/.027)**2-((z-1.92)/.046)**2)
                    bridge = .018*math.exp(-(x/.022)**2-((z-1.97)/.065)**2)
                    cheeks = .011*math.exp(-((abs(x)-.13)/.06)**2-((z-1.915)/.048)**2)
                    v.co.y -= nose+bridge+cheeks
            ob.data.update()
        # Reduce bulky sleeves and soften shoulder width without changing joint centers.
        if ob.name.startswith('연속 소매'):
            side = -1 if sum(v.co.x for v in ob.data.vertices)<0 else 1
            for v in ob.data.vertices:
                center=side*.357
                v.co.x=center+(v.co.x-center)*.83
                v.co.y*=.88
        if ob.name.startswith('맞춤 코트 몸판'):
            for v in ob.data.vertices:
                if 1.28<v.co.z<1.6:
                    v.co.x*=.96

    # Almond-shaped eyes are thin curved patches, not spheres.
    for sign in [-1,1]:
        cx=sign*.085
        verts=[(cx,-.201,1.998)]
        n=32
        for i in range(n):
            a=2*math.pi*i/n
            x=cx+.045*math.cos(a)
            z=1.998+.017*math.sin(a)
            y=-.201 + .026*(abs(x)/.15)**2
            verts.append((x,y,z))
        mesh('아몬드 눈 흰자',verts,[(0,i+1,(i+1)%n+1) for i in range(n)],sclera)
        for label,rx,rz,y,material in [('홍채',.014,.016,-.203,iris),('동공',.006,.011,-.205,pupil)]:
            vv=[(cx,y,1.998)]+[(cx+rx*math.cos(i*2*math.pi/24),y,1.998+rz*math.sin(i*2*math.pi/24)) for i in range(24)]
            mesh(label,vv,[(0,i+1,(i+1)%24+1) for i in range(24)],material)
        upper=[];lower=[]
        for i in range(9):
            t=i/8
            x=cx-.046+.092*t
            y=-.203+.026*(abs(x)/.15)**2
            upper.append((x,y,1.998+.018*math.sin(math.pi*t)))
            lower.append((x,y,1.998-.016*math.sin(math.pi*t)))
        line('윗눈꺼풀',upper,.0035,lash)
        line('아랫눈꺼풀',lower,.0025,lidmat)
        line('자연스러운 눈썹',[(cx-.041,-.188,2.039),(cx,-.197,2.046),(cx+.040,-.177,2.037)],.0045,strand)
    line('입 모양',[(-.035,-.165,1.814),(0,-.177,1.811),(.035,-.165,1.814)],.0028,lip)

    # One continuous hair shell with shallow flowing ridges, instead of tube clumps.
    rows,cols=32,96
    def hair_point(t,a):
        if first:
            front=max(0,min(1,(math.cos(a)-.05)/.65))
            bottom=1.81+.255*front
            # Side-swept fringe, gently asymmetric hem.
            bottom-=.022*max(0,math.sin(a))*front**3
            phi=min(1,t/.65)*math.pi/2
            rr=math.sin(phi)
            radius=(.267+.004*math.sin(11*a+2*t)*t)*rr
            x=math.sin(a)*radius
            y=-math.cos(a)*radius*.89+.014
            z=2.04+.235*math.cos(phi) if t<=.65 else 2.04+(bottom-2.04)*(t-.65)/.35
            z+=.008*math.sin(a*5)*t**8
            return (x,y,z)
        phi=t*1.47
        ridge=.009*math.sin(a*13+phi*7)+.005*math.sin(a*23-phi*5)
        r=.247+ridge*math.sin(phi)
        return (math.sin(a)*r*math.sin(phi),-math.cos(a)*r*.84*math.sin(phi)+.012,
                2.078+(.22+ridge)*math.cos(phi))
    v=[hair_point(j/(rows-1),i*2*math.pi/cols) for j in range(rows) for i in range(cols)]
    f=[(j*cols+i,j*cols+(i+1)%cols,(j+1)*cols+(i+1)%cols,(j+1)*cols+i) for j in range(rows-1) for i in range(cols)]
    shell=mesh('연속 머리 형태',v,f,hair)
    shell.data.materials.append(sheen)
    for poly in shell.data.polygons:
        if (poly.index%cols)//6%4==0:poly.material_index=1
    # Fine grooves follow the scalp; low relief preserves a clean silhouette.
    for i in range(38 if first else 48):
        a=i*2*math.pi/(38 if first else 48)
        points=[]
        for j in range(10):
            t=.12+j*.084
            p=Vector(hair_point(t,a+.08*math.sin(t*2)))
            p.x*=1.006;p.y=(p.y-.014)*1.006+.014
            points.append(tuple(p))
        line('머릿결 선',points,.0015,strand if i%3 else sheen)
    if not first:
        # Small closed, flattened tips on the silhouette, rooted inside the shell.
        for i in range(27):
            a=i*2.39996;t=.23+.69*(i%7)/6
            base=Vector(hair_point(t,a))
            radial=Vector((math.sin(a),-math.cos(a),.35)).normalized()
            across=Vector((math.cos(a),math.sin(a),0))*.020
            tip=base+radial*.045+Vector((0,0,.02))
            vv=[tuple(base-across),tuple(base+across),tuple(base+Vector((0,0,.020))),tuple(tip)]
            mesh('작은 머리 끝',vv,[(0,1,2),(0,3,1),(1,3,2),(2,3,0)],sheen if i%4==0 else hair)
    # Preserve the carrying pose but keep the original animations in their separate source model.
    if first:
        rig.pose.bones['arm.R'].rotation_euler.z=.12
        rig.pose.bones['forearm.R'].rotation_euler.x=-.12
    rig.rotation_euler.z=-math.pi/8
    scene.render.resolution_x=768;scene.render.resolution_y=768
    scene.render.resolution_percentage=100
    scene.render.film_transparent=True
    scene.render.engine='BLENDER_WORKBENCH'
    scene.display.render_aa='16'
    scene.render.filepath=str(OUT/(name+'_after_studio.png'))
    bpy.ops.render.render(write_still=True)
    # Physical lighting preview, kept separate from the equal-lighting comparison.
    for ob in list(scene.objects):
        if ob.type=='LIGHT':bpy.data.objects.remove(ob,do_unlink=True)
    def light(label,location,power,size):
        data=bpy.data.lights.new(label,'AREA');data.energy=power;data.shape='DISK';data.size=size
        ob=bpy.data.objects.new(label,data);scene.collection.objects.link(ob);ob.location=location
        ob.rotation_euler=(Vector((0,0,1.3))-ob.location).to_track_quat('-Z','Y').to_euler()
    light('큰 주광',(-3,-4,5),380,4)
    light('약한 보조광',(3,-2,3),160,3)
    light('윤곽광',(1,3,4),450,3)
    scene.world.use_nodes=True
    scene.world.node_tree.nodes.get('Background').inputs[0].default_value=(.18,.21,.27,1)
    scene.world.node_tree.nodes.get('Background').inputs[1].default_value=.35
    scene.render.engine='CYCLES';scene.cycles.samples=24;scene.cycles.use_denoising=True
    scene.render.filepath=str(OUT/(name+'_after_lit.png'))
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/(name+'_candidate.blend')))
    bpy.ops.render.render(write_still=True)
    # Comparison uses identical Workbench lighting on both sides.
    source=BASE.parents[1]/'SimpleGame/Assets/Characters/Blender'/name/'preview.png'
    pair=[bpy.data.images.load(str(source)),bpy.data.images.load(str(OUT/(name+'_after_studio.png')))]
    w,h=pair[0].size
    pixels=array('f',[0])*(w*2*h*4)
    for index,im in enumerate(pair):
        data=array('f',[0])*(w*h*4);im.pixels.foreach_get(data)
        for row in range(h):
            start=(row*w*2+index*w)*4
            pixels[start:start+w*4]=data[row*w*4:(row+1)*w*4]
    image=bpy.data.images.new(name+' 전후',width=w*2,height=h,alpha=True)
    image.pixels.foreach_set(pixels);image.filepath_raw=str(OUT/(name+'_before_after.png'));image.file_format='PNG';image.save()
    print(name+' CANDIDATE COMPLETE',flush=True)

for name in ('npc1','npc2'):
    build(name)
