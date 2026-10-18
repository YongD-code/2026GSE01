"""Independent lantern-spirit candidate; never writes into the game asset tree.

Blender --background --factory-startup --python this_file [-- --source-only]
Use --views-only to regenerate the review renders, or --export-only for sprite poses.
The rig, cel materials and fabric geometry remain editable in forgotten_candidate.blend.
"""
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector

OUT = Path(__file__).resolve().parent
sys.path.insert(0, str(OUT))
from fire_volume import create_fire, animate_fire
ROOT = OUT.parents[2]
SOURCE = ROOT / 'SimpleGame/Assets/Spirits/ash_lantern_4dir.png'
PROTECTED = [SOURCE, ROOT / 'SimpleGame/SpiritCatalog.h',
             ROOT / 'SimpleGame/PrototypeRendering.cpp']
before = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
          for p in PROTECTED}
TAU = math.tau
CELL, STEPS = 192, 8
OUT.mkdir(parents=True, exist_ok=True)


def material(name, base, fabric=False, glow=0):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*base, 1)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.clear()
    output = nodes.new('ShaderNodeOutputMaterial')
    emission = nodes.new('ShaderNodeEmission')
    emission.inputs['Strength'].default_value = 1 + glow
    if glow:
        emission.inputs['Color'].default_value = (*base, 1)
    else:
        diffuse = nodes.new('ShaderNodeBsdfDiffuse')
        diffuse.inputs['Color'].default_value = (1, 1, 1, 1)
        rgb = nodes.new('ShaderNodeShaderToRGB')
        ramp = nodes.new('ShaderNodeValToRGB')
        ramp.color_ramp.interpolation = 'CONSTANT'
        bands = [(0, .17), (.16, .37), (.40, .65), (.72, 1.0)]
        ramp.color_ramp.elements.remove(ramp.color_ramp.elements[1])
        for i, (position, intensity) in enumerate(bands):
            e = ramp.color_ramp.elements[0] if i == 0 else ramp.color_ramp.elements.new(position)
            e.position = position
            e.color = (*(c * intensity for c in base), 1)
        links.new(diffuse.outputs[0], rgb.inputs[0])
        links.new(rgb.outputs[0], ramp.inputs[0])
        color = ramp.outputs[0]
        if fabric:
            noise = nodes.new('ShaderNodeTexNoise')
            noise.inputs['Scale'].default_value = 150
            noise.inputs['Detail'].default_value = 2
            grain = nodes.new('ShaderNodeMapRange')
            grain.inputs['To Min'].default_value = .81
            grain.inputs['To Max'].default_value = 1
            multiply = nodes.new('ShaderNodeMixRGB')
            multiply.blend_type = 'MULTIPLY'
            multiply.inputs[0].default_value = 1
            links.new(noise.outputs['Fac'], grain.inputs['Value'])
            links.new(color, multiply.inputs[1])
            links.new(grain.outputs['Result'], multiply.inputs[2])
            color = multiply.outputs[0]
        links.new(color, emission.inputs['Color'])
    links.new(emission.outputs[0], output.inputs['Surface'])
    return mat


def attach(ob, bone='body', weights=None):
    ob.parent = rig
    if ob.type == 'MESH':
        group = ob.vertex_groups.new(name=bone)
        group.add(list(range(len(ob.data.vertices))), 1, 'REPLACE')
        if weights:
            group.remove(list(range(len(ob.data.vertices))))
            for name, values in weights.items():
                g = ob.vertex_groups.get(name) or ob.vertex_groups.new(name=name)
                for index, value in values:
                    if value > 0:
                        g.add([index], value, 'REPLACE')
        modifier = ob.modifiers.new('浮遊リグ', 'ARMATURE')
        modifier.object = rig
    return ob


def mesh(name, vertices, faces, mat, bone='body', smooth=True, weights=None):
    data = bpy.data.meshes.new(name)
    data.from_pydata(vertices, [], faces)
    data.update()
    ob = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(mat)
    for face in data.polygons:
        face.use_smooth = smooth
    attach(ob, bone, weights)
    return ob


def curve(name, points, radius, mat, bone='body', cyclic=False):
    data = bpy.data.curves.new(name, 'CURVE')
    data.dimensions = '3D'
    data.bevel_depth = radius
    data.bevel_resolution = 2
    spline = data.splines.new('POLY')
    spline.points.add(len(points) - 1)
    for vertex, point in zip(spline.points, points):
        vertex.co = (*point, 1)
    spline.use_cyclic_u = cyclic
    ob = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(mat)
    # Convert so armature deformation also follows every engraved line and finger.
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.convert(target='MESH')
    return attach(bpy.context.object, bone)


def sphere(name, center, scale, mat, bone='body', segments=32, rings=16, bind=True):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, location=center)
    ob = bpy.context.object
    ob.name = name
    ob.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    # Store vertices in rig-local coordinates, not an object-specific scaled space.
    bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)
    ob.data.materials.append(mat)
    for polygon in ob.data.polygons:
        polygon.use_smooth = True
    return attach(ob, bone) if bind else ob


def ring(name, z, rx, ry, mat, tube=.023, bone='body'):
    points = [(rx * math.cos(i * TAU / 96), ry * math.sin(i * TAU / 96), z)
              for i in range(96)]
    return curve(name, points, tube, mat, bone, True)


def loft(name, levels, mat, n=48, bone='body', smooth=False):
    vertices = [(rx * math.cos(i * TAU / n), ry * math.sin(i * TAU / n), z)
                for z, rx, ry in levels for i in range(n)]
    faces = [(j*n+i, j*n+(i+1)%n, (j+1)*n+(i+1)%n, (j+1)*n+i)
             for j in range(len(levels)-1) for i in range(n)]
    faces.extend([tuple(reversed(range(n))), tuple(range((len(levels)-1)*n, len(levels)*n))])
    return mesh(name, vertices, faces, mat, bone, smooth)


def cloth_panel(index, center, width, length):
    """A folded two-dimensional surface, never a chain of thick ellipsoids."""
    vertices, faces = [], []
    nu, nv = 18, 26
    for j in range(nv+1):
        t = j/nv
        for i in range(nu+1):
            u = i/nu
            angle = center + (u-.5)*width*(1-.80*t**1.5)
            angle += (.24 if center>0 else -.24)*t**2
            hem = .15*math.sin(u*math.pi*5+index) + .08*math.sin(u*math.pi*11+.5)
            z = 2.98 - length*t + hem*t**6
            radius = .57 + .10*math.sin(t*math.pi) + .24*t**3
            fold = (.025+.025*t)*math.sin(u*math.pi*8+t*1.2+index)
            x = math.sin(angle)*(radius+fold)
            y = -math.cos(angle)*(radius*.82+fold)
            x += .11*math.sin(t*4+index)*t**2
            y += .12*t**3
            vertices.append((x,y,z))
    for j in range(nv):
        for i in range(nu):
            a=j*(nu+1)+i
            faces.append((a,a+1,a+nu+2,a+nu+1))
    top, tip = f'cloth.{index}', f'tip.{index}'
    weights={top:[],tip:[]}
    for v in range(len(vertices)):
        t=(v//(nu+1))/nv
        w=max(0,min(1,(t-.35)/.55))
        weights[top].append((v,1-w)); weights[tip].append((v,w))
    ob=mesh(f'裂けた織布 {index}',vertices,faces,cloth,top,True,weights)
    solid=ob.modifiers.new('薄い布の厚み','SOLIDIFY'); solid.thickness=.012
    # A quiet seam follows the actual fold instead of a separate thick tubular border.
    for edge in [0,nu]:
        coords=[vertices[j*(nu+1)+edge] for j in range(nv+1)]
        seam=curve(f'布端 {index}-{edge}',coords,.006,cloth_edge,top)
        seam.vertex_groups.clear()
        for name in [top,tip]:
            g=seam.vertex_groups.new(name=name)
            # Curves converted to mesh contain rings; use vertex height for continuous weights.
            for vertex in seam.data.vertices:
                t=max(0,min(1,(2.98-vertex.co.z)/length))
                w=max(0,min(1,(t-.35)/.55))
                value=w if name==tip else 1-w
                if value>0: g.add([vertex.index],value,'REPLACE')


def pose(kind, phase):
    angle=TAU*phase
    for joint in rig.pose.bones:
        joint.location=(0,0,0)
        joint.rotation_euler=(0,0,0)
        joint.scale=(1,1,1)
    # A vertical Blender bone's local Y points along world Z.
    rig.pose.bones['body'].location.y=.045*math.sin(angle)
    rig.pose.bones['body'].rotation_euler.y=.027*math.sin(angle)
    rig.pose.bones['body'].rotation_euler.x=.025*math.cos(angle)
    rig.pose.bones['mask'].rotation_euler.y=.018*math.sin(angle-.4)
    moving=kind=='glide'
    for side,sign in [('L',-1),('R',1)]:
        rig.pose.bones['arm.'+side].rotation_euler.y=sign*(.04+.07*math.sin(angle-.6))
        rig.pose.bones['arm.'+side].rotation_euler.x=-.12 if moving else .025*math.cos(angle-.3)
        rig.pose.bones['hand.'+side].rotation_euler.z=sign*.07*math.sin(angle-1.1)
    for i in range(4):
        rig.pose.bones[f'cloth.{i}'].rotation_euler.x=(.10 if moving else 0)+.045*math.sin(angle-.55+i*.45)
        rig.pose.bones[f'cloth.{i}'].rotation_euler.y=.035*math.sin(angle-.8+i)
        rig.pose.bones[f'tip.{i}'].rotation_euler.x=(.11 if moving else 0)+.09*math.sin(angle-1.2+i*.45)
    rig.pose.bones['flame'].scale=(1+.09*math.sin(angle*2),1+.12*math.sin(angle*2+.8),1+.06*math.cos(angle*2))
    animate_fire(phase)
    bpy.context.view_layer.update()


def camera_view(angle=0, scale=4.8):
    target=Vector((0,0,1.92))
    camera.location=target+Vector((8*math.sin(angle),-8*math.cos(angle),4.05))
    camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.ortho_scale=scale


def render(path, x=560, y=700):
    scene.render.resolution_x=x; scene.render.resolution_y=y
    scene.render.filepath=str(path)
    bpy.ops.render.render(write_still=True)


if '--export-only' in sys.argv or '--views-only' in sys.argv:
    bpy.ops.wm.open_mainfile(filepath=str(OUT/'forgotten_candidate.blend'))
    scene=bpy.context.scene
    rig=bpy.data.objects['남겨진 자 · 부유 리그']
    camera=scene.camera
    rig.animation_data_clear()
else:
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    bpy.context.preferences.filepaths.save_version=0
    scene=bpy.context.scene
    scene.render.engine='BLENDER_EEVEE'
    scene.eevee.taa_render_samples=24
    scene.render.resolution_percentage=100
    scene.render.film_transparent=True
    scene.render.image_settings.file_format='PNG'
    scene.render.image_settings.color_mode='RGBA'
    scene.view_settings.view_transform='Standard'
    scene.world.color=(.14,.14,.14)
    scene.render.fps=24
    scene.frame_start=1; scene.frame_end=33
    # All original inputs remain untouched; the candidate has its own scene and rig.
    armature=bpy.data.armatures.new('남겨진 자 · 편집용 뼈대')
    rig=bpy.data.objects.new('남겨진 자 · 부유 리그',armature)
    bpy.context.collection.objects.link(rig)
    bpy.context.view_layer.objects.active=rig
    rig.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    def bone(name,head,tail,parent=None):
        b=armature.edit_bones.new(name); b.head=head; b.tail=tail
        if parent: b.parent=armature.edit_bones[parent]
    bone('root',(0,0,0),(0,0,.3))
    bone('body',(0,0,1.35),(0,0,2.5),'root')
    bone('mask',(0,0,2.4),(0,0,2.9),'body')
    bone('flame',(0,0,1.05),(0,0,1.6),'body')
    for side,sign in [('L',-1),('R',1)]:
        bone('arm.'+side,(sign*.54,0,2.05),(sign*.77,-.08,1.69),'body')
        bone('hand.'+side,(sign*.77,-.08,1.69),(sign*.91,-.13,1.39),'arm.'+side)
    for i,center in enumerate([-.95,.95,2.06,-2.06]):
        p=(.56*math.sin(center),-.46*math.cos(center),2.98)
        mid=(p[0],p[1],1.9)
        bone(f'cloth.{i}',p,mid,'body')
        bone(f'tip.{i}',mid,(p[0],p[1]+.15,.6),f'cloth.{i}')
    bpy.ops.object.mode_set(mode='OBJECT')
    rig.show_in_front=True
    for joint in rig.pose.bones: joint.rotation_mode='XYZ'
    bronze=material('古銅 · 深い面',(.16,.062,.022),True)
    gold=material('擦れた銅縁',(.42,.205,.069),True)
    engraving=material('銅の刻線',(.033,.015,.009))
    patina=material('青銅の錆',(.065,.16,.15))
    cloth=material('炭色の織布',(.024,.032,.046),True)
    cloth_edge=material('傷んだ布端',(.068,.077,.088),True)
    ivory=material('焼けた白い仮面',(.68,.63,.49))
    black=material('眼窩と口の奥',(.008,.006,.01),glow=.01)
    ember=material('灯火 · 橙',(.95,.16,.012),glow=.7)
    yellow=material('灯火 · 芯',(1,.59,.10),glow=1)
    warm=material('瞳の残り火',(1,.27,.025),glow=.5)
    # Fine faceted rings and ribs give the lantern thickness and a rigid silhouette.
    loft('灯籠の下皿',[(.72,.03,.025),(.94,.39,.30),(1.08,.53,.40),(1.16,.54,.41)],bronze,n=48)
    for z,rx,ry in [(1.1,.54,.41),(1.20,.53,.40),(2.18,.57,.43),(2.24,.59,.45)]:
        ring('打ち出し銅の口縁',z,rx,ry,gold,.025)
    loft('尖った底飾り',[(.34,.005,.005),(.59,.12,.10),(.69,.10,.08),(.76,.19,.14)],bronze,n=24)
    for i in range(8):
        a=TAU*i/8+math.pi/8
        points=[((.53+.035*math.sin(t*math.pi))*math.sin(a),
                 -(.40+.035*math.sin(t*math.pi))*math.cos(a),1.15+t*1.06)
                for t in [j/16 for j in range(17)]]
        curve('曲がった灯籠の骨',points,.022,bronze)
        for j in [.08,.88]:
            x,y,z=points[int(j*16)]
            sphere('留め鋲',(x,y,z),(.036,.036,.036),gold,segments=12,rings=8)
    # Perforated cap: actual gaps between the radial plates, not painted holes.
    levels=[(2.99,.66,.50),(3.06,.72,.54),(3.14,.59,.45),(3.38,.28,.22),(3.48,.10,.08)]
    for sector in range(16):
        a=sector*TAU/16
        verts=[(rx*math.sin(a+d),-ry*math.cos(a+d),z)
               for z,rx,ry in levels for d in [-.189,.189]]
        faces=[(j*2,j*2+1,j*2+3,j*2+2) for j in range(len(levels)-1)]
        roof=mesh('透かし銅板',verts,faces,bronze,smooth=False)
        thick=roof.modifiers.new('銅板の厚さ','SOLIDIFY'); thick.thickness=.018
        if sector%2==0:
            curve('屋根の稜線',[(rx*math.sin(a),-ry*math.cos(a),z+.012) for z,rx,ry in levels],.012,gold)
        # Recessed, repeating crescent engravings and tiny dents break up the roof plates.
        for row in range(2):
            z=3.14+row*.10
            rx=.59-row*.13; ry=.45-row*.095
            points=[((rx+.005)*math.sin(a+d),-(ry+.005)*math.cos(a+d),z+.016*math.cos(d*26))
                    for d in [j*.012-.06 for j in range(11)]]
            curve('笠の刻み模様',points,.008,engraving)
    ring('笠の広い縁',3.045,.72,.54,gold,.032)
    ring('笠の細い縁',3.10,.65,.50,bronze,.015)
    for i in range(48):
        a=i*TAU/48
        curve('口縁の槌目',[(.715*math.sin(a),-.537*math.cos(a),3.044),
                           (.701*math.sin(a+.012),-.527*math.cos(a+.012),3.067)],.007,engraving)
    curve('吊り環',[(.17*math.cos(i*TAU/80),.02,3.65+.19*math.sin(i*TAU/80)) for i in range(80)],.032,bronze,cyclic=True)
    for i in range(8):
        a=i*TAU/8
        curve('縁の小さな吊り金具',[(.64*math.sin(a),-.49*math.cos(a),3.03),(.64*math.sin(a),-.49*math.cos(a),2.94)],.018,gold)
    # Thin hood dome, open at the face. Its taper reads as fabric, not a round head.
    verts=[]; faces=[]
    for j in range(13):
        t=j/12
        for i in range(33):
            a=.65+(TAU-1.3)*i/32
            r=.56+.035*math.sin(a*9+t*2)
            verts.append((r*math.sin(a),-.78*r*math.cos(a),3.0-.91*t))
    for j in range(12):
        for i in range(32):
            k=j*33+i; faces.append((k,k+1,k+34,k+33))
    mesh('顔の後ろの薄い頭巾',verts,faces,cloth)
    for i,(a,w,l) in enumerate([(-.95,.75,2.17),(.95,.75,2.06),(2.06,1.18,2.47),(-2.06,1.18,2.36)]):
        cloth_panel(i,a,w,l)
    # One porcelain shell with holes cut through it; socket backs are recessed.
    mask=sphere('割れた陶器の仮面',(0,-.36,2.58),(.43,.20,.49),ivory,'mask',48,32,False)
    for i,(x,z,sx,sz) in enumerate([(-.19,2.68,.145,.115),(.19,2.68,.145,.115),(0,2.38,.092,.15)]):
        cutter=sphere('穴の加工',(x,-.47,z),(sx,.40,sz),black,bind=False)
        bpy.context.view_layer.objects.active=mask
        boolean=mask.modifiers.new('くり抜いた眼窩' if i<2 else 'くり抜いた口','BOOLEAN')
        boolean.operation='DIFFERENCE'; boolean.object=cutter
        bpy.ops.object.modifier_apply(modifier=boolean.name)
        bpy.data.objects.remove(cutter,do_unlink=True)
        sphere('深い眼窩' if i<2 else '声の空洞',(x,-.325,z),(sx*1.04,.035,sz*1.05),black,'mask')
        if i<2:
            sphere('小さな瞳の灯',(x,-.375,z-.035),(.027,.018,.040),warm,'mask',20,12)
            curve('涙の煤',[(x,-.546,z-.09),(x+(.015 if x<0 else -.015),-.54,z-.18),(x,-.51,z-.24)],.011,bronze,'mask')
    attach(mask,'mask')
    bevel=mask.modifiers.new('欠けた陶器の縁','BEVEL'); bevel.width=.012; bevel.segments=2
    curve('眉の割れ目',[(-.02,-.48,3.02),(-.045,-.53,2.92),(.018,-.554,2.87),(-.01,-.56,2.80)],.009,bronze,'mask')
    curve('頬の割れ目',[(.35,-.475,2.48),(.28,-.51,2.43),(.29,-.51,2.35),(.25,-.48,2.28)],.007,bronze,'mask')
    # Distinct long, jointed hands. Tendons and articulated fingers replace mitten blobs.
    for side,sign in [('L',-1),('R',1)]:
        bone_name='arm.'+side
        curve('細い袖の腕',[(sign*.52,-.02,2.02),(sign*.66,-.06,1.88),(sign*.77,-.12,1.67)],.055,cloth,bone_name)
        sphere('手の甲',(sign*.79,-.15,1.61),(.095,.055,.13),bronze,'hand.'+side,24,12)
        for finger in range(4):
            x=sign*(.73+finger*.048)
            length=.20+.07*math.sin((finger+1)*math.pi/5)
            curve('関節のある指',[(x,-.18,1.60),(x+sign*.045,-.235,1.49),(x+sign*.04,-.22,1.60-length),(x+sign*.017,-.18,1.56-length)],.017,gold,'hand.'+side)
        curve('曲がった親指',[(sign*.73,-.13,1.65),(sign*.66,-.20,1.56),(sign*.69,-.24,1.51)],.021,bronze,'hand.'+side)
    create_fire(mesh, rig)
    for i in range(8):
        a=i*2.4
        sphere('消えかけの火の粉',(.32*math.sin(a),-.11+.18*math.cos(a),1.3+i*.08),(.009,.009,.019),ember,'flame',12,8)
    for i in range(10):
        a=i*2.399
        curve('青錆の傷',[(.532*math.sin(a+t),-.402*math.cos(a+t),1.1+.008*math.sin(t*8)) for t in [j*.015 for j in range(8)]],.008,patina)
    # Neutral cel-lit orthographic asset views; no world background is baked in.
    for name,location,power,size,color in [('大きい主光',(-3,-4,7),420,4,(1,.89,.76)),('青い縁光',(3,2,5),380,3,(.50,.72,1)),('正面補助',(2,-3,3),70,4,(1,.70,.40))]:
        data=bpy.data.lights.new(name,'AREA'); data.energy=power; data.shape='DISK'; data.size=size; data.color=color
        ob=bpy.data.objects.new(name,data); scene.collection.objects.link(ob); ob.location=location
        ob.rotation_euler=(Vector((0,0,2))-ob.location).to_track_quat('-Z','Y').to_euler()
    data=bpy.data.cameras.new('等角寄りの比較カメラ')
    camera=bpy.data.objects.new('等角寄りの比較カメラ',data); scene.collection.objects.link(camera)
    scene.camera=camera; camera.data.type='ORTHO'
    for kind,label in [('idle','남겨진 자 · 대기 부유'),('glide','남겨진 자 · 이동 부유')]:
        rig.animation_data_create(); rig.animation_data.action=None
        for frame in range(33):
            pose(kind,frame/32)
            animate_fire(frame/32, frame+1)
            for joint in rig.pose.bones:
                joint.keyframe_insert('location',frame=frame+1,group=joint.name)
                joint.keyframe_insert('rotation_euler',frame=frame+1,group=joint.name)
                joint.keyframe_insert('scale',frame=frame+1,group=joint.name)
        rig.animation_data.action.name=label
        rig.animation_data.action.use_fake_user=True
        for fcurve in rig.animation_data.action.layers[0].strips[0].channelbag(rig.animation_data.action_slot).fcurves:
            for key in fcurve.keyframe_points: key.interpolation='LINEAR'
    rig.animation_data.action=None
    pose('idle',0); camera_view(.42)
    scene.render.resolution_x=560; scene.render.resolution_y=700
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'forgotten_candidate.blend'))

if '--source-only' not in sys.argv and '--export-only' not in sys.argv:
    pose('idle',0); rig.rotation_euler.z=0
    for name,angle in [('front',0),('perspective',.42),('side',math.pi/2),('back',math.pi)]:
        camera_view(angle)
        render(OUT/f'candidate_{name}.png')
        print('REVIEW_VIEW',name,flush=True)

if '--source-only' not in sys.argv and '--views-only' not in sys.argv:
    folder=OUT/'Frames'; folder.mkdir(exist_ok=True)
    camera_view(0,4.8)
    yaws=[]
    tilt=math.sin(math.atan2(4.05,8))
    for direction in range(8):
        phi=direction*TAU/8
        yaw=math.atan2(-math.sin(phi),math.cos(phi)/tilt)
        yaws.append(yaw)
        rig.rotation_euler.z=yaw
        for column in range(STEPS+1):
            pose('idle' if column==0 else 'glide',0 if column==0 else (column-1)/STEPS)
            render(folder/f'd{direction}_f{column:02d}.png',CELL,CELL)
        print('SPRITE_DIRECTION',direction+1,'/8',flush=True)
    rig.rotation_euler.z=.20
    motion=OUT/'Motion'; motion.mkdir(exist_ok=True)
    for frame in range(16):
        pose('glide',frame/16)
        render(motion/f'glide_{frame:02d}.png',320,400)
    report={'status':'candidate_not_applied','name':'남겨진 자','reference':str(SOURCE.relative_to(ROOT)),
            'source':str((OUT/'forgotten_candidate.blend').relative_to(ROOT)),
            'cellSize':CELL,'rows':8,'columns':STEPS+1,'firstWalkFrame':1,'walkFrames':STEPS,
            'directionRows':list(range(8)),'directionYaws':yaws,
            'fixedCanvas':True,'canvasAnchorY':.5+math.cos(math.atan2(4.05,8))*1.92/4.8,
            'loopSeconds':16/12,'bones':len(rig.data.bones),
            'camera':{'orthoScale':4.8,'tilt':math.atan2(4.05,8)},
            'renderer':'BLENDER_EEVEE','gameAssetsModified':False,'gameBuildRunTested':False,
            'fire':'closed 3D volumes / density-noise / radiance core / looped turbulence and bend keys',
            'protectedBefore':before,
            'protectedAfter':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in PROTECTED}}
    if report['protectedBefore']!=report['protectedAfter']: raise RuntimeError('A protected game input changed')
    (OUT/'candidate_manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('FORGOTTEN_CANDIDATE_READY',flush=True)
