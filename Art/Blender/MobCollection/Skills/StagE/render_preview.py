"""Render the stag E concept against the real stag rig, without altering its actions."""
import math
import random
import sys
from pathlib import Path

import bpy
from mathutils import Vector

HERE = Path(__file__).resolve().parent
COLLECTION = HERE.parents[1]
sys.path.insert(0, str(COLLECTION))
from quadruped_locomotion import Baker
import render_previews as review

bpy.ops.wm.open_mainfile(filepath=str(COLLECTION/'stag/model.blend'))
scene = bpy.context.scene
rig = next(obj for obj in scene.objects if obj.type == 'ARMATURE')
rig.animation_data.action = None
review.set_stage(.9, 4.8)
scene.render.film_transparent = False
scene.render.resolution_x, scene.render.resolution_y = 900, 740
scene.eevee.taa_render_samples = 24
scene.world.color = (.04, .05, .042)


class LeapPreview(Baker):
    """Use the existing hoof IK with different fore/hind targets in each pose."""
    def leg(self, name, phase, duty, distance, lift, desired, turn=0):
        front = name.startswith('front')
        y, z = self.fore if front else self.hind
        if z > 0:
            # Mid-swing yields the chosen lift at zero longitudinal offset.
            phase, duty, distance, lift = .85, .7, 0., z
        else:
            phase, duty, distance, lift = (.69 if y > 0 else 0), .7, abs(y)*2, 0.
        super().leg(name, phase, duty, distance, lift, desired, turn)


def material(name, rgb):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*rgb, 1)
    mat.use_nodes = True
    shader = mat.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*rgb, 1)
    shader.inputs['Roughness'].default_value = .8
    return mat


ground = material('E_FOREST_GROUND', (.061, .088, .063))
bpy.ops.mesh.primitive_plane_add(size=150)
bpy.context.object.data.materials.append(ground)

rng = random.Random(93)
materials = [material('E_WILD_LEAF', (.19,.35,.075)),
             material('E_NEW_LEAF', (.34,.48,.14)),
             material('E_PETAL_CREAM', (.88,.79,.45)),
             material('E_PETAL_GOLD', (.80,.55,.19))]
fragments = []
for index in range(29):
    angle = rng.uniform(0, math.tau)
    radius = rng.uniform(.22, 1.3)
    origin = Vector((math.cos(angle)*radius, -.54+math.sin(angle)*radius,
                     rng.uniform(.035,.24)*(1-radius*.36)))
    direction = Vector((math.cos(angle+.35),math.sin(angle+.35),0))
    side = Vector((-direction.y, direction.x, 0))
    length = rng.uniform(.085,.25)
    width = length*(.24 if index<18 else .54)
    vertices, faces = [], []
    for row in range(6):
        t = row/5
        center = direction*(length*t)+side*(.23*length*math.sin(t*math.pi))
        center.z = length*(.38*math.sin(t*math.pi)+.2*t)
        half = width*.5*math.sin(t*math.pi)
        vertices.extend((center-side*half,center+Vector((0,0,half*.38)),center+side*half))
    for row in range(5):
        for edge in range(2):
            k=row*3+edge
            faces.append((k,k+3,k+4,k+1))
    data=bpy.data.meshes.new(f'E_FRAGMENT_{index:02}')
    data.from_pydata(vertices,[],faces)
    data.materials.append(materials[index%2+(0 if index<18 else 2)])
    obj=bpy.data.objects.new(data.name,data)
    scene.collection.objects.link(obj)
    obj.location=origin
    obj.rotation_euler.x=rng.uniform(-.55,.6)
    fragments.append(obj)

baker = LeapPreview(rig, 'stag')
baker.profile = baker.profile.copy()
poses = [
    ('prepare', .25, (-.085, 0.), (.055, 0.), 0., -.105, False),
    ('airborne', .14, (0., .26), (0., .31), .55, .07, False),
    ('landing', .23, (-.14, 0.), (0., .17), .0, -.13, True),
]
for name, drop, fore, hind, height, tilt, impact in poses:
    baker.profile['drop'] = drop
    baker.fore, baker.hind = fore, hind
    baker.pose('idle',0)
    root = rig.pose.bones['root']
    root.location += root.bone.matrix_local.to_3x3().inverted() @ Vector((0, 0, height))
    rig.pose.bones['head'].rotation_euler.x += tilt
    for obj in fragments:
        obj.hide_render = not impact
    target = Vector((0,-.10,1.02+height*.42))
    camera = scene.camera
    camera.location = target+Vector((6,-8,4.1))
    camera.rotation_euler = (target-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.ortho_scale=4.25
    bpy.context.view_layer.update()
    review.render(HERE/(name+'.png'))
