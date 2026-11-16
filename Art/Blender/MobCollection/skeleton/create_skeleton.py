"""Build, rig and export the skeletal spirit and render its Blender review."""
import json
import math
import random
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))
import render_previews as review
from rig_skeleton import build_rig, save_review_poses, set_pose

rng = random.Random(21)
COLORS = {
    'Ivory': (.68, .57, .39), 'BoneShadow': (.43, .34, .23),
    'Enamel': (.78, .67, .49), 'Socket': (.030, .022, .017),
    'Eye': (.12, .78, 1.0), 'Cloth': (.075, .080, .083),
    'ClothEdge': (.12, .122, .115), 'Cord': (.20, .15, .09),
    'Paper': (.58, .43, .24), 'Ink': (.29, .072, .045),
}
materials = []
for label, rgb in COLORS.items():
    material = bpy.data.materials.new(f'Skeleton_{label}')
    material.diffuse_color = (*rgb, 1)
    material.use_nodes = True
    shader = material.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*rgb, 1)
    shader.inputs['Roughness'].default_value = .86
    if label == 'Eye':
        shader.inputs['Emission Color'].default_value = (*rgb, 1)
        shader.inputs['Emission Strength'].default_value = 2.2
    materials.append(material)


class Part:
    """Collect the forms of one future rigging part in a named mesh."""
    def __init__(self, label):
        self.label = label
        self.vertices, self.faces, self.colors = [], [], []

    def add(self, vertices, faces, color):
        start = len(self.vertices)
        self.vertices.extend(vertices)
        self.faces.extend(tuple(start + i for i in face) for face in faces)
        self.colors.extend([list(COLORS).index(color)] * len(faces))

    def tube(self, points, radii, color='Ivory', n=7, flatten=1.):
        points = [Vector(p) for p in points]
        verts = []
        for j, (p, radius) in enumerate(zip(points, radii)):
            tangent = (points[min(j+1, len(points)-1)] - points[max(j-1, 0)]).normalized()
            reference = Vector((0, 1, 0)) if abs(tangent.y) < .96 else Vector((1, 0, 0))
            axis = tangent.cross(reference).normalized()
            other = tangent.cross(axis)
            for i in range(n):
                angle = math.tau * i / n
                verts.append(p + radius * (math.cos(angle)*axis + math.sin(angle)*other*flatten))
        faces = [(j*n+i, j*n+(i+1)%n, (j+1)*n+(i+1)%n, (j+1)*n+i)
                 for j in range(len(points)-1) for i in range(n)]
        faces.extend((tuple(reversed(range(n))), tuple(range(len(verts)-n, len(verts)))))
        self.add(verts, faces, color)

    def ellipsoid(self, center, radii, color='Ivory', n=10, rows=5):
        verts = [(center[0], center[1], center[2]+radii[2])]
        for row in range(1, rows):
            phi = math.pi * row / rows
            for i in range(n):
                angle = math.tau * (i+.5*(row%2)) / n
                verts.append((center[0]+radii[0]*math.sin(phi)*math.cos(angle),
                              center[1]+radii[1]*math.sin(phi)*math.sin(angle),
                              center[2]+radii[2]*math.cos(phi)))
        verts.append((center[0], center[1], center[2]-radii[2]))
        faces = [(0, 1+i, 1+(i+1)%n) for i in range(n)]
        for row in range(rows-2):
            for i in range(n):
                a, b = 1+row*n+i, 1+row*n+(i+1)%n
                faces.extend(((a, a+n, b), (b, a+n, b+n)))
        faces.extend((len(verts)-1, len(verts)-1-n+(i+1)%n, len(verts)-1-n+i) for i in range(n))
        self.add(verts, faces, color)

    def shaft(self, start, end, width, color='Ivory', detail=True):
        a, b = Vector(start), Vector(end)
        times = (0, .09, .23, .55, .80, .94, 1) if detail else (0, .16, .56, .87, 1)
        profile = (.89, 1.24, .81, .64, .74, 1.16, .88) if detail else (.82, 1.08, .69, .98, .79)
        self.tube([a.lerp(b, t) for t in times], [width*t for t in profile],
                  color, n=7 if detail else 5, flatten=.83)

    def finish(self):
        i = len(parts)
        name = 'MOB_SKELETON_' + self.label
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata(self.vertices, [], self.faces)
        mesh.update()
        for mat in materials:
            mesh.materials.append(mat)
        for face, material_index in zip(mesh.polygons, self.colors):
            face.material_index = material_index
        bm = bmesh.new()
        bm.from_mesh(mesh)
        bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
        bm.to_mesh(mesh)
        bm.free()
        mesh.calc_loop_triangles()
        obj = bpy.data.objects.new(name, mesh)
        collection.objects.link(obj)
        obj['partIndex'] = i
        parts.append({'index': i, 'name': name, 'triangles': len(mesh.loop_triangles)})
        return obj


def skull():
    head = Part('SKULL')
    # The from-the-front eye aperture is continuous with the cheek and brow.
    outline = [(0,-.405,3.036), (.12,-.378,3.063), (.23,-.303,3.047),
               (.275,-.235,2.96), (.273,-.225,2.86), (.231,-.295,2.767),
               (.165,-.345,2.717), (.093,-.364,2.701), (.036,-.373,2.727),
               (.045,-.355,2.775), (0,-.367,2.813), (0,-.392,2.91)]
    rim = [(.048,-.394,2.91), (.089,-.399,2.953), (.159,-.378,2.958),
           (.205,-.346,2.935), (.217,-.326,2.887), (.192,-.35,2.844),
           (.155,-.388,2.825), (.110,-.405,2.826), (.077,-.407,2.846),
           (.060,-.405,2.862), (.052,-.400,2.880), (.048,-.395,2.897)]
    for side in (-1, 1):
        loops = [[(side*x,y,z) for x,y,z in row] for row in (outline,rim)]
        inner = [(side*(.13 + (x-.13)*.68), y+.110, 2.89+(z-2.89)*.72) for x,y,z in rim]
        n = len(outline)
        for i in range(n):
            j = (i+1)%n
            head.add([loops[0][i],loops[0][j],loops[1][j],loops[1][i]], [(0,1,2,3)], 'Ivory')
            head.add([loops[1][i],loops[1][j],inner[j],inner[i]], [(0,1,2,3)], 'BoneShadow')
        head.add(inner, [tuple(reversed(range(n)))], 'Socket')
    # Broken section of the layered skull cap leaves a real hole.
    rows = [(2.77,.181,.158,-.162), (2.89,.268,.235,-.152),
            (3.015,.282,.256,-.146), (3.14,.259,.232,-.136),
            (3.24,.205,.182,-.124), (3.299,.104,.099,-.128)]
    sides = 18
    verts = []
    for j,(z, rx,ry,cy) in enumerate(rows):
        for i in range(sides):
            a = math.tau*i/sides
            verts.append((rx*math.sin(a), cy-ry*math.cos(a), z+rng.uniform(-.015,.015)))
    # The existing facial plate closes the lower front; it must not fill the eye sockets.
    missing = {(0,i) for i in (0,1,2,15,16,17)} | {(1,i) for i in (0,1,2,15,16,17)}
    missing |= {(2,3),(2,4),(3,2),(3,3),(3,4),(4,2),(4,3)}
    faces = [(j*sides+i,j*sides+(i+1)%sides,(j+1)*sides+(i+1)%sides,(j+1)*sides+i)
             for j in range(len(rows)-1) for i in range(sides) if (j,i) not in missing]
    verts.append((-.014,-.13,3.312))
    for i in range(sides):
        faces.append((len(verts)-1, (len(rows)-1)*sides+i, (len(rows)-1)*sides+(i+1)%sides))
    head.add(verts, faces, 'Ivory')
    # A dark inner membrane appears behind the broken plate, at a different depth.
    head.ellipsoid((.034,-.125,3.095),(.212,.175,.16),'Socket',12,6)
    # Upper dental arch, separate mandible and a narrow dark gap.
    head.tube([(-.171,-.286,2.755),(-.136,-.358,2.716),(-.071,-.392,2.705),
               (0,-.408,2.700),(.071,-.392,2.705),(.136,-.358,2.716),
               (.171,-.286,2.755)],[.023,.032,.036,.038,.036,.032,.023],flatten=.8)
    for i in range(-4,5):
        x=i*.027
        head.ellipsoid((x,-.424+.009*i*i/4,2.663+.003*abs(i)),(.014,.021,.023),'Enamel',6,3)
    head.finish()
    jaw = Part('JAW')
    jaw.tube([(-.183,-.202,2.785),(-.177,-.227,2.67),(-.148,-.316,2.614),
              (-.088,-.382,2.587),(0,-.406,2.579),(.088,-.382,2.587),
              (.148,-.316,2.614),(.177,-.227,2.67),(.183,-.202,2.785)],
             [.035,.040,.036,.031,.031,.031,.036,.040,.035],flatten=.84)
    for i in range(-4,5):
        jaw.ellipsoid((i*.025,-.416+.008*i*i/4,2.615+.004*abs(i)),
                       (.013,.016,.019),'Enamel',6,3)
    jaw.finish()
    eyes = Part('SOCKET_LIGHTS')
    for side in (-1,1):
        eyes.ellipsoid((side*.124,-.374,2.884),(.023,.018,.024),'Eye',8,4)
    eyes.finish()


def torso():
    spine = Part('SPINE_AND_NECK')
    route = [(0,.079,1.47),(0,.13,1.73),(0,.174,2.02),(0,.18,2.30),
             (0,.105,2.52),(0,.018,2.65),(0,-.086,2.777)]
    for a,b in zip(route, route[1:]):
        a,b=Vector(a),Vector(b)
        for k in range(3):
            p=a.lerp(b,k/3)
            spine.ellipsoid(p,(.065,.066,.046),'BoneShadow',7,3)
            spine.tube([p, p+Vector((0,.067,-.028))],[.041,.021],n=6)
    spine.tube(route,[.041]*len(route),'Ivory')
    spine.finish()
    chest = Part('RIBS')
    for side in (-1,1):
        for i in range(7):
            z = 2.52-i*.092
            x = [.225,.285,.308,.306,.289,.258,.217][i]
            end = .033 if i < 4 else [.054,.097,.140][i-4]
            path=[(side*.035,.145,z+.023),(side*x*.63,.119,z),
                  (side*x,.009,z-.048),(side*x*.91,-.139,z-.092),
                  (side*x*.55,-.206,z-.095),(side*end,-.228,z-.081)]
            chest.tube(path,[.027,.027,.026,.024,.021,.017],'Ivory',7,.8)
    chest.tube([(0,-.226,2.463),(0,-.234,2.368),(0,-.234,2.248),(0,-.224,2.137)],
               [.042,.043,.036,.016],'BoneShadow',8,.70)
    chest.finish()
    for side, label in [(-1,'R'),(1,'L')]:
        shoulder=Part('SHOULDER_'+label)
        shoulder.tube([(0,-.195,2.463),(side*.145,-.154,2.55),(side*.291,-.052,2.563),
                       (side*.373,.018,2.521)],[.032,.038,.040,.047],flatten=.78)
        shoulder.add([(side*.37,.073,2.54),(side*.222,.15,2.53),(side*.14,.18,2.28),
                      (side*.26,.118,2.31),(side*.30,.19,2.44),(side*.277,.084,2.425)],
                     [(0,1,4),(1,2,4),(2,3,4),(3,0,4),(0,5,1),(1,5,2),(2,5,3),(3,5,0)],
                     'BoneShadow')
        shoulder.finish()
    pelvis = Part('PELVIS')
    for side in (-1,1):
        pelvis.tube([(0,.085,1.56),(side*.126,.089,1.605),(side*.239,.048,1.573),
                     (side*.223,-.028,1.44),(side*.149,-.079,1.37),(0,-.095,1.386)],
                    [.044,.071,.082,.076,.057,.041],flatten=.74)
        pelvis.tube([(side*.09,.089,1.525),(side*.172,.066,1.416),(side*.124,.034,1.335),
                     (side*.086,-.069,1.388)],[.065,.047,.029,.028],'BoneShadow')
    pelvis.finish()


def limbs():
    for side,label in [(-1,'R'),(1,'L')]:
        mirror = lambda x,y,z: (side*x,y,z)
        asym = .021*side
        shoulder=mirror(.358,.005,2.509)
        elbow=mirror(.488,-.037+asym,1.881)
        wrist=mirror(.529,-.126-asym,1.253+asym)
        upper=Part('UPPER_ARM_'+label)
        upper.shaft(shoulder,elbow,.054)
        upper.ellipsoid(shoulder,(.084,.072,.081),n=8,rows=4)
        upper.finish()
        fore=Part('FOREARM_'+label)
        for shift in (-.027,.029):
            fore.shaft((elbow[0]+shift,elbow[1],elbow[2]),
                       (wrist[0]+shift*.7,wrist[1],wrist[2]),.027)
        fore.finish()
        hand=Part('HAND_'+label)
        hand.ellipsoid(wrist,(.060,.044,.045),n=8,rows=4)
        for j in range(4):
            x=wrist[0]+side*(-.052+j*.034)
            length=(.175,.205,.213,.162)[j]
            p=[(x,wrist[1]-.01,wrist[2]-.023),
               (x+side*.012,wrist[1]-.024,wrist[2]-.139),
               (x+side*.018,wrist[1]-.056,wrist[2]-.139-length*.60),
               (x+side*.006,wrist[1]-.096,wrist[2]-.139-length)]
            for a,b in zip(p,p[1:]):
                hand.shaft(a,b,.016 if a==p[0] else .014, detail=False)
        hand.tube([wrist,mirror(.434,wrist[1]-.037,wrist[2]-.105),
                   mirror(.418,wrist[1]-.08,wrist[2]-.208)],[.027,.022,.013])
        hand.finish()
        hip=mirror(.183,.034,1.43)
        knee=mirror(.212,-.110+asym,.789)
        ankle=mirror(.251,.086+asym,.165)
        thigh=Part('THIGH_'+label)
        thigh.shaft(hip,knee,.074)
        thigh.ellipsoid(knee,(.075,.071,.077),n=9,rows=5)
        thigh.finish()
        shin=Part('SHIN_'+label)
        shin.shaft(knee,ankle,.055)
        shin.shaft((knee[0]+side*.054,knee[1]+.023,knee[2]-.044),
                   (ankle[0]+side*.038,ankle[1]+.026,ankle[2]+.022),.029,'BoneShadow')
        shin.finish()
        foot=Part('FOOT_'+label)
        foot.ellipsoid((ankle[0],ankle[1]-.033,.102),(.079,.086,.072),n=9,rows=4)
        for j in range(5):
            x=ankle[0]+side*(-.081+.036*j)
            foot.tube([(x,ankle[1]-.036,.109),(x-side*.008,-.110,.065),
                       (x-side*.014,-.219+.019*j,.032), (x-side*.02,-.296+.025*j,.018)],
                      [.026,.028,.022,.015],n=6,flatten=.7)
        foot.finish()


def clothing():
    cloth=Part('WAIST_CLOTH')
    for i in range(13):
        angle=i*math.tau/13
        top_z=1.525+.026*math.sin(angle*3)
        length=.24+(i%4)*.065 + (.11 if i in (1,7) else 0)
        def p(a,r,z):
            return (math.sin(a)*r, .035-math.cos(a)*(r*.67),z)
        v=[p(angle-.25,.242,top_z),p(angle+.24,.242,top_z+.025),
           p(angle+.245,.259,top_z-length*.60),p(angle+.19,.27,top_z-length*.95),
           p(angle+.065,.283,top_z-length*.83),p(angle-.07,.29,top_z-length),
           p(angle-.24,.27,top_z-length*.64),p(angle,.27,top_z-length*.37)]
        cloth.add(v,[(7,j,(j+1)%7) for j in range(7)],'Cloth' if i%3 else 'ClothEdge')
    for layer in range(3):
        points=[(.25*math.sin(i*math.tau/28),.035-.167*math.cos(i*math.tau/28),
                 1.508+layer*.026+.021*math.sin(i*math.tau/28*2+layer*.7)) for i in range(29)]
        cloth.tube(points,[.022]*29,'Cord' if layer==2 else 'ClothEdge',6,.75)
    cloth.finish()
    charm=Part('WAIST_TALISMAN')
    charm.tube([(.054,-.140,1.552),(.077,-.163,1.501),(.077,-.179,1.449)],
               [.008,.010,.007],'Cord',6)
    charm.add([(.018,-.166,1.461),(.135,-.158,1.466),(.147,-.193,1.321),
               (.15,-.235,1.188),(.112,-.241,1.176),(.085,-.24,1.195),
               (.049,-.247,1.166),(.013,-.230,1.218),(.026,-.212,1.325)],
              [(0,1,2,8),(8,2,3,4,5,6,7)],'Paper')
    for z,y in [(1.397,-.180),(1.359,-.190),(1.302,-.213)]:
        charm.tube([(.052,y,z),(.103,y,z+.01)],[.006,.005],'Ink',5)
    charm.tube([(.076,-.174,1.423),(.082,-.195,1.353),(.090,-.214,1.294)],
               [.005,.005,.006],'Ink',5)
    charm.finish()


bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
collection = bpy.data.collections.new('SKELETON_SPIRIT')
bpy.context.scene.collection.children.link(collection)
parts = []
skull()
torso()
limbs()
clothing()
rig = build_rig(collection, parts)
save_review_poses(rig)
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'model.blend'))
bpy.ops.export_scene.gltf(filepath=str(HERE/'model.glb'), export_format='GLB',
                          export_animations=False, export_extras=True)
metadata = {'characterId':'skeleton', 'stage':'rigged_review', 'facing':'-Y',
            'rig':rig.name, 'joints':[bone.name for bone in rig.data.bones],
            'reviewAction':'MOB_SKELETON_RIG_REVIEW',
            'triangles': sum(p['triangles'] for p in parts), 'parts':parts}
(HERE/'model.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
print('SKELETON_READY', len(parts), metadata['triangles'], flush=True)

if '--review' in sys.argv:
    review.set_stage(1.70, 3.92)
    scene=bpy.context.scene
    scene.render.resolution_x=680
    scene.render.resolution_y=820
    scene.eevee.taa_render_samples=32
    for key, offset in [('quarter',(3,-8,1.1)),('front',(0,-8,0)),
                        ('side',(8,0,0)),('back',(0,8,0)),('skull',(2,-8,1.1))]:
        center=Vector((0,-.02,1.67))
        scene.camera.data.ortho_scale=3.94
        if key=='skull':
            center=Vector((0,-.11,2.82))
            scene.camera.data.ortho_scale=1.28
        scene.camera.location=center+Vector(offset)
        scene.camera.rotation_euler=(center-scene.camera.location).to_track_quat('-Z','Y').to_euler()
        review.render(HERE/'Previews'/f'{key}.png')

if '--rig-review' in sys.argv:
    review.set_stage(1.63, 4.18)
    scene = bpy.context.scene
    scene.render.resolution_x, scene.render.resolution_y = 620, 760
    scene.eevee.taa_render_samples = 32
    center = Vector((0, -.11, 1.58))
    for key, offset in [('rest', (3, -8, .95)), ('reach', (4, -8, 1.6)),
                        ('crouch', (6, -6, 1.25))]:
        set_pose(rig, key)
        scene.camera.location = center + Vector(offset)
        scene.camera.rotation_euler = (center-scene.camera.location).to_track_quat('-Z','Y').to_euler()
        review.render(HERE/'Previews'/f'rig_{key}.png')
