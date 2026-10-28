"""Render actual stag Q poses plus a Blender interpretation of the game's shared timings/path."""
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view

HERE = Path(__file__).resolve().parent
COLLECTION = HERE.parents[1]
ROOT = COLLECTION.parents[2]
sys.path.insert(0, str(COLLECTION))
from stag_q import specification, smooth
import render_previews as review

SPEC = specification()
FRAMES = ROOT/'tmp/stag_q_preview/frames'


def material(name, color, emission=0):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*color, 1)
    bsdf.inputs['Roughness'].default_value = .68
    bsdf.inputs['Emission Color'].default_value = (*color, 1)
    bsdf.inputs['Emission Strength'].default_value = emission
    return mat


def ball(name, at, scale, mat):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=12, ring_count=8, radius=1, location=at)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    obj.data.materials.append(mat)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def curve(name, points, width, mat):
    data = bpy.data.curves.new(name, 'CURVE')
    data.dimensions = '3D'
    data.resolution_u = 1
    data.bevel_depth = width
    data.bevel_resolution = 2
    spline = data.splines.new('POLY')
    spline.points.add(len(points)-1)
    for out, p in zip(spline.points, points):
        out.co = (*p, 1)
    for i, out in enumerate(spline.points):
        out.radius = 1-.68*i/max(1, len(spline.points)-1)
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    data.materials.append(mat)
    return obj


def main():
    bpy.ops.wm.open_mainfile(filepath=str(COLLECTION/'stag/model.blend'))
    scene = bpy.context.scene
    rig = next(o for o in scene.objects if o.type == 'ARMATURE')
    character_objects = [o for o in scene.objects if o.type == 'MESH']
    rig.animation_data.action = bpy.data.actions['MOB_STAG_ATTACK']
    scene.frame_set(1)
    height = max((o.matrix_world @ v.co).z for o in scene.objects if o.type == 'MESH'
                 for v in o.data.vertices)
    unit = height/80
    review.set_stage(height*.5, height*4)
    scene.render.resolution_x = 720
    scene.render.resolution_y = 420
    scene.render.film_transparent = False
    scene.eevee.taa_render_samples = 16
    scene.world.color = (.018, .028, .026)
    focus = Vector((0, -SPEC['Reach']*unit*.43, height*.3))
    camera = scene.camera
    camera.location = focus+Vector((-10, -10, 9))
    camera.rotation_euler = (focus-camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.data.ortho_scale = height*3.15
    ground = material('REVIEW_GROUND', (.045, .065, .055))
    root_mat = material('PATH_ROOT', (.10, .22, .035))
    vein_mat = material('PATH_VEIN', (.35, .85, .13), 1.8)
    leaf_mat = material('PATH_LEAF', (.12, .42, .055), .2)
    gold_mat = material('PATH_FLOWER', (1., .67, .11), 1.3)
    core_mat = material('PATH_CORE', (.65, 1., .35), 3)
    stone_mat = material('REVIEW_TARGET', (.15, .20, .20))
    fill_data = bpy.data.lights.new('Q_BLOOM_FILL', 'AREA')
    fill_data.energy = 500
    fill_data.size = 5
    fill_data.color = (.8, 1., .72)
    fill = bpy.data.objects.new('Q_BLOOM_FILL', fill_data)
    scene.collection.objects.link(fill)
    fill.location = (-2, -SPEC['Reach']*unit, height+2)
    fill.rotation_euler = (Vector((0, -SPEC['Reach']*unit, height*.5))-fill.location).to_track_quat('-Z', 'Y').to_euler()
    bpy.ops.mesh.primitive_plane_add(size=200)
    bpy.context.object.data.materials.append(ground)
    roots, leaves, flowers, coils, tendrils, towers, upright_leaves = [], [], [], [], [], [], []

    def point(branch, t):
        split = min(1., max(0., (t-.2)/.8))
        side = math.sin(t*math.pi)*12+(branch-1)*62*smooth(split)
        return Vector((side*unit, -SPEC['Reach']*t*unit, .025))

    def stem(branch, strand, t):
        angle = strand*2.094395102+branch*.8+t*2.7
        spread = (4+8*math.sin(t*math.pi))*(.4+.6*t)
        return point(branch, 1)+Vector(((math.cos(angle)*spread+math.sin(branch*1.1+strand)*t*t*9)*unit,
                                        (math.sin(angle)*spread+math.cos(branch+strand*.7)*t*t*6)*unit,
                                        (SPEC['VineHeight']-strand*8)*t*unit))

    for branch in range(3):
        pts = [point(branch, i/48) for i in range(49)]
        roots.append(curve('Q_ROOT_'+str(branch), pts, .065, root_mat))
        roots.append(curve('Q_VEIN_'+str(branch), [p+Vector((0, 0, .035)) for p in pts], .018, vein_mat))
        for i in range(6, 49, 6):
            at = point(branch, i/48)
            for side in (-1, 1):
                leaf = ball('Q_LEAF', at+Vector((side*.09, 0, .04)), (.15, .055, .018), leaf_mat)
                leaf.rotation_euler.z = side*.7
                leaves.append((i/48, leaf, leaf.scale.copy()))
        for i in (12, 24, 36, 46):
            at = point(branch, i/48)
            curl = [at+Vector((math.cos(j*.3)*j/24*.4, math.sin(j*.3)*j/24*.4, .01+j/24*.04))
                    for j in range(25)]
            tendrils.append((i/48, curve('Q_TENDRIL', curl, .025, leaf_mat)))
        at = point(branch, 1)
        for strand in range(3):
            pts = [stem(branch, strand, i/32) for i in range(33)]
            towers.append(curve('Q_RISING_VINE', pts, .085, root_mat))
            towers.append(curve('Q_RISING_VEIN', [p+Vector((-.075, -.075, .02)) for p in pts], .035, vein_mat))
            for t in (.34, .68, .9):
                obj = ball('Q_RISING_LEAF', stem(branch, strand, t), (.18, .06, .025), leaf_mat)
                obj.rotation_euler.z = branch+strand+t*3
                upright_leaves.append((t, obj, obj.scale.copy()))
        for petal in range(5):
            angle = petal*math.tau/5+branch
            offset = Vector((math.cos(angle)*.12, math.sin(angle)*.12, .07))
            obj = ball('Q_FLOWER_PETAL', stem(branch, 0, 1)+offset,
                       (.16, .065, .025), gold_mat)
            obj.rotation_euler.z = angle
            flowers.append((obj, obj.scale.copy(), branch, offset))
        offset = Vector((0, 0, .09))
        obj = ball('Q_FLOWER_CENTER', stem(branch, 0, 1)+offset, (.075, .075, .055), core_mat)
        flowers.append((obj, obj.scale.copy(), branch, offset))
    for branch, fraction in ((1, .58), (2, .78)):
        at = point(branch, fraction)
        bpy.ops.mesh.primitive_cylinder_add(vertices=10, radius=.18, depth=.8, location=at+Vector((0, 0, .4)))
        bpy.context.object.data.materials.append(stone_mat)
        pts = [at+Vector((math.cos(i*.3)*.24, math.sin(i*.3)*.24, i/50*.8)) for i in range(51)]
        coils.append((fraction, curve('Q_BINDING', pts, .025, vein_mat)))
    motes = []
    for i in range(14):
        obj = ball('Q_LIGHT', (0, 0, 0), (.04, .04, .04), core_mat)
        motes.append(obj)

    def pose(age):
        frame = 1+min(1., age/SPEC['Animation'])*32
        scene.frame_set(int(frame), subframe=frame-int(frame))
        growth = min(1., max(0., (age-SPEC['Charge'])/SPEC['Travel']))
        fade = min(1., max(0., (SPEC['Duration']-age)/SPEC['Fade']))
        local = age-SPEC['Charge']-SPEC['Travel']
        rise = min(1., max(0., local/SPEC['BloomRise']))
        rise = 1-(1-rise)**3
        for obj in roots:
            obj.hide_render = growth <= .001 or fade <= .001
            obj.data.bevel_factor_end = max(.001, growth)
            obj.data.bevel_depth = (.018 if 'VEIN' in obj.name else .065)*max(.001, fade)
        for t, obj, scale in leaves:
            bloom = smooth((growth-t)*8+.12)*fade
            obj.hide_render = growth < t or bloom < .001
            obj.scale = scale*max(.001, bloom)
        for fraction, obj, scale in upright_leaves:
            bloom = smooth((rise-fraction)*6)*fade
            obj.hide_render = local < 0 or rise < fraction or bloom < .001
            obj.scale = scale*max(.001, bloom)
        for fraction, obj in tendrils:
            obj.hide_render = growth < fraction or fade < .001
            obj.data.bevel_factor_end = max(.001, smooth((growth-fraction)*8))
        for obj in towers:
            obj.hide_render = rise <= .001 or fade <= .001
            obj.data.bevel_factor_end = max(.001, rise)
            obj.data.bevel_depth = (.035 if 'VEIN' in obj.name else .085)*max(.001, fade)
        bloom = smooth((local-SPEC['BloomRise']*.7)/.18)*fade
        for obj, scale, branch, offset in flowers:
            obj.hide_render = bloom < .001
            obj.scale = scale*max(.001, bloom)
            obj.location = stem(branch, 0, rise)+offset
        for fraction, obj in coils:
            obj.hide_render = growth < fraction or fade < .001
            obj.data.bevel_factor_end = max(.001, smooth((growth-fraction)*8))
        charge = min(1., age/SPEC['Charge'])
        for i, obj in enumerate(motes):
            visible = age < SPEC['Charge']+.25
            obj.hide_render = not visible
            a = i*2.39996323+age*2
            radius = .08+(1-charge)*(.4+i%4*.12)
            obj.location = (math.cos(a)*radius, -.9+math.sin(a)*radius, height*.85+charge*.06)
            obj.scale = (.035+charge*.015,)*3
        bpy.context.view_layer.update()

    # Fit all poses and full-length effects; never crop antlers or an end flower.
    points = []
    for age in (0., SPEC['Charge'], SPEC['Animation'], SPEC['Charge']+SPEC['Travel']+SPEC['BloomRise']+.2):
        pose(age)
        graph = bpy.context.evaluated_depsgraph_get()
        for original in scene.objects:
            if original.type not in ('MESH', 'CURVE') or original.name.startswith('Plane'):
                continue
            obj = original.evaluated_get(graph)
            points.extend(obj.matrix_world @ Vector(corner) for corner in obj.bound_box)
    rotation = camera.rotation_euler.to_quaternion()
    right, up = rotation @ Vector((1, 0, 0)), rotation @ Vector((0, 1, 0))
    lo_x, hi_x = min(p.dot(right) for p in points), max(p.dot(right) for p in points)
    lo_y, hi_y = min(p.dot(up) for p in points), max(p.dot(up) for p in points)
    center = focus+right*((lo_x+hi_x)/2-focus.dot(right))+up*((lo_y+hi_y)/2-focus.dot(up))
    camera.location = center+Vector((-10, -10, 9))
    camera.data.ortho_scale = max(hi_x-lo_x, (hi_y-lo_y)*720/420)*1.20
    crop_points = []
    for age in (0., SPEC['Charge'], SPEC['Animation']):
        pose(age)
        graph = bpy.context.evaluated_depsgraph_get()
        for original in character_objects:
            obj = original.evaluated_get(graph)
            crop_points.extend(world_to_camera_view(scene, camera, obj.matrix_world @ Vector(c)) for c in obj.bound_box)
    crop = [max(0, int(min(p.x for p in crop_points)*720)-6),
            max(0, int((1-max(p.y for p in crop_points))*420)-6),
            min(720, int(max(p.x for p in crop_points)*720)+6),
            min(420, int((1-min(p.y for p in crop_points))*420)+6)]
    FRAMES.parent.mkdir(parents=True, exist_ok=True)
    (FRAMES.parent/'layout.json').write_text(json.dumps({'actorCrop':crop}), encoding='utf-8')
    if '--animate' in sys.argv:
        for i in range(math.ceil((SPEC['Duration']+.1)*20)+1):
            pose(i/20)
            review.render(FRAMES/f'{i:03d}.png')
    else:
        pose(.98)
        review.render(HERE/'stag_q_peak.png')
    print('STAG_Q_PREVIEW_READY', flush=True)


if __name__ == '__main__':
    main()
