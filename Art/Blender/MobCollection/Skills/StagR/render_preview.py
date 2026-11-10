"""Author the sanctuary's stag pose, asymmetric trees and growth review."""
import math
import random
import sys
from pathlib import Path

import bpy
from mathutils import Vector

HERE = Path(__file__).resolve().parent
COLLECTION = HERE.parents[1]
sys.path.insert(0, str(COLLECTION))
from quadruped_locomotion import Baker, smooth
import render_previews as review

bpy.ops.wm.open_mainfile(filepath=str(COLLECTION/'stag/model.blend'))
scene = bpy.context.scene
rig = next(o for o in scene.objects if o.type == 'ARMATURE')
rig.animation_data_create()
rig.animation_data.action = None
baker = Baker(rig, 'stag')
review.set_stage(.95, 7.8)
scene.render.film_transparent = False
scene.render.resolution_x, scene.render.resolution_y = 1000, 760
scene.eevee.taa_render_samples = 24
target = Vector((0, 0, .85))
scene.camera.location = target + Vector((5, -8, 6.5))
scene.camera.rotation_euler = (target-scene.camera.location).to_track_quat('-Z','Y').to_euler()

def material(name, rgb, emission=0.):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*rgb, 1)
    mat.use_nodes = True
    node = mat.node_tree.nodes.get('Principled BSDF')
    node.inputs['Base Color'].default_value = (*rgb, 1)
    node.inputs['Emission Color'].default_value = (*rgb, 1)
    node.inputs['Emission Strength'].default_value = emission
    node.inputs['Roughness'].default_value = .83
    return mat

bark = material('R_WILLOW_BARK', (.33,.46,.30))
vein = material('R_MINT_SAP', (.32,.78,.52), .8)
leaf = material('R_GOLD_LEAF', (.59,.52,.22), .14)
light = material('R_ANTLER_LIGHT', (1,.65,.19), 2)
soil = material('R_EARTH', (.052,.075,.053))
bpy.ops.mesh.primitive_plane_add(size=80)
bpy.context.object.data.materials.append(soil)

def curve(name, points, widths, mat, parent):
    data = bpy.data.curves.new(name, 'CURVE')
    data.dimensions = '3D'
    data.resolution_u = 8
    data.bevel_depth = 1
    data.bevel_resolution = 1
    spline = data.splines.new('BEZIER')
    spline.bezier_points.add(len(points)-1)
    for b, p, radius in zip(spline.bezier_points, points, widths):
        b.co = p
        b.handle_left_type = b.handle_right_type = 'AUTO'
        b.radius = radius
    obj = bpy.data.objects.new(name, data)
    scene.collection.objects.link(obj)
    obj.parent = parent
    data.materials.append(mat)
    return obj

rng = random.Random(76)
trees = []
for i, (x,y,h) in enumerate([(-2.0,.5,2.55),(-1.24,2.1,2.15),
                             (1.05,2.4,2.64),(2.28,.75,2.29),(1.88,-1.47,1.67)]):
    tree = bpy.data.objects.new(f'R_SANCTUARY_TREE_{i+1:02}', None)
    scene.collection.objects.link(tree)
    tree.location = (x,y,0)
    trees.append(tree)
    lean = rng.choice((-1,1))
    trunk = [(0,0,.03),(-.16*lean,.04,h*.28),(.22*lean,-.08,h*.64),(.46*lean,.05,h)]
    curve(f'R_TRUNK_{i}', trunk, [.17,.119,.073,.014], bark, tree)
    curve(f'R_SAP_{i}', [(p[0],p[1]-.105+z*.025,p[2]) for z,p in enumerate(trunk)],
          [.026,.025,.016,.004], vein, tree)
    for j in range(5):
        a = j*math.tau/5 + rng.uniform(-.3,.3)
        d = rng.uniform(.36,.72)
        curve(f'R_ROOT_{i}_{j}', [(0,0,.09),(.33*d*math.cos(a),.33*d*math.sin(a),.13),
                                 (d*math.cos(a),d*math.sin(a),.025)],
              [.085,.047,.003], bark, tree)
    vertices, faces = [], []
    for j in range(5):
        a = j*2.39996 + rng.uniform(-.25,.25)
        z = h*(.52+j*.085)
        base = Vector((lean*(z/h-.36)*.66,-.06,z))
        direction = Vector((math.cos(a),math.sin(a),0))
        tip = base + direction*rng.uniform(.43,.78) + Vector((0,0,rng.uniform(.32,.61)))
        middle = base.lerp(tip,.53)-Vector((0,0,.04))
        curve(f'R_BRANCH_{i}_{j}', [base,middle,tip],[.055,.032,.006],bark,tree)
        for k in range(8):
            center = middle.lerp(tip, rng.uniform(.02,1))
            twig = Vector((rng.uniform(-.23,.23),rng.uniform(-.23,.23),rng.uniform(.02,.16)))
            center += twig
            axis = (twig + direction*.15).normalized()
            width = Vector((-axis.y,axis.x,.1)).normalized()*rng.uniform(.036,.07)
            length = axis*rng.uniform(.10,.18)
            idx = len(vertices)
            vertices.extend([center,center+length*.55+width,center+length,
                             center+length*.5-width,center+length*.47+Vector((0,0,.024))])
            faces.extend((idx+m,idx+(m+1)%4,idx+4) for m in range(4))
    mesh = bpy.data.meshes.new(f'R_LEAVES_{i}')
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(leaf)
    ob = bpy.data.objects.new(mesh.name, mesh)
    scene.collection.objects.link(ob)
    ob.parent = tree

# Small points of light drift inward during the charge.
motes = []
for i in range(15):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=.014 + i%3*.004)
    ob = bpy.context.object
    ob.name = f'R_CHARGE_MOTE_{i:02}'
    ob.data.materials.append(light)
    motes.append(ob)

def pose(age, frame=None):
    baker.pose('idle', age*.35)
    bow = smooth(age/.8)*(1-smooth((age-.88)/.48))
    rig.pose.bones['head'].rotation_euler.x += .20*bow
    if frame is not None:
        for bone in rig.pose.bones:
            bone.keyframe_insert('location', frame=frame, group=bone.name)
            bone.keyframe_insert('rotation_euler', frame=frame, group=bone.name)
    for i, tree in enumerate(trees):
        growth = smooth((age-.8-i*.09)/.52)
        fade = 1-smooth((age-6.8)/.5)
        tree.scale = (max(.0001,growth*fade),)*3
        if frame is not None:
            tree.keyframe_insert('scale', frame=frame)
    for i, ob in enumerate(motes):
        angle = i*2.39996 + age*1.2
        t = smooth(age/.8)
        radius = .13+(1-t)*.66
        ob.location = (math.cos(angle)*radius, -.66+math.sin(angle)*radius,
                       1.56+(i%5)*.14 + .17*t)
        strength = smooth(age/.20)*(1-smooth((age-.70)/.45))
        ob.scale = (max(.0001,strength),)*3
        if frame is not None:
            ob.keyframe_insert('location', frame=frame)
            ob.keyframe_insert('scale', frame=frame)
    bpy.context.view_layer.update()

# Bake every preview frame. The source stag asset stays untouched.
for frame in range(1,177):
    pose((frame-1)/24, frame)
rig.animation_data.action.name = 'MOB_STAG_R_SANCTUARY'
scene.frame_start, scene.frame_end = 1, 176
scene.frame_set(61)
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'sanctuary.blend'))
for name, frame in [('prepare',16), ('rise',31), ('sanctuary',61)]:
    scene.frame_set(frame)
    review.render(HERE/(name+'.png'))
