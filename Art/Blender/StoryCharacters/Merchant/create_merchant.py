"""Bespoke village shopkeeper, with a pear-shaped dress and tied-back hair."""

import json
import math
import sys
from pathlib import Path

import bpy

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1]/'ModularCollection'))
import create_collection as g
import surface_forms as f

PALETTE = {
    'shop_skin': (.58, .35, .22, 1), 'shop_warm': (.57, .28, .18, 1),
    'shop_crease': (.30, .145, .085, 1), 'shop_lip': (.35, .12, .095, 1),
    'shop_hair': (.065, .040, .029, 1), 'shop_hair_light': (.12, .078, .05, 1),
    'shop_grey': (.31, .28, .23, 1), 'shop_eyes': (.075, .05, .025, 1),
    'shop_iris': (.26, .19, .085, 1), 'shop_white': (.76, .68, .52, 1),
    'shop_dress': (.19, .105, .15, 1), 'shop_fold': (.12, .065, .10, 1),
    'shop_cuff': (.57, .49, .34, 1), 'shop_apron': (.24, .33, .28, 1),
    'shop_apron_edge': (.35, .43, .35, 1), 'shop_patch': (.35, .20, .145, 1),
    'shop_belt': (.14, .08, .045, 1), 'shop_leather': (.12, .075, .045, 1),
    'shop_sole': (.035, .035, .03, 1), 'shop_brass': (.44, .29, .095, 1),
    'shop_iron': (.24, .26, .23, 1), 'shop_wood': (.25, .135, .065, 1),
    'shop_board': (.38, .235, .12, 1), 'shop_paper': (.62, .47, .29, 1),
    'shop_twine': (.32, .25, .14, 1), 'shop_cloth': (.43, .32, .20, 1),
    'shop_bottle': (.12, .23, .21, 1), 'shop_herb': (.20, .28, .105, 1),
}
g.COLORS.update(PALETTE)
PARTS, RULES = [], {}
HEAD = [(1.61, .10, .12), (1.67, .21, .185), (1.79, .30, .232),
        (1.94, .32, .247), (2.09, .30, .23), (2.21, .21, .17), (2.29, .015, .015)]


def finish(suffix, rule):
    obj = g.finish('NPC_MERCHANT_'+suffix, 'bespoke', len(PARTS), PARTS)
    obj['characterId'] = 'merchant'
    obj['design'] = 'kind middle-aged shopkeeper; unique anatomy and clothing'
    RULES[obj.name] = rule
    return obj


def face():
    f.loft('mature_oval_face', HEAD, 'shop_skin', 20, True)
    for s in (-1, 1):
        f.oval('small_ear', (s*.318, .01, 1.94), (.05, .051, .079), 'shop_skin', 12, 6)
        f.oval('ear_fold', (s*.345, -.028, 1.945), (.019, .013, .04), 'shop_crease', 10, 5)
        x = s*.127
        f.oval('almond_eye', (x, -.235, 1.981), (.071, .012, .023), 'shop_white', 16, 6)
        f.oval('hazel_iris', (x-s*.005, -.247, 1.981), (.024, .006, .024), 'shop_iris', 12, 5)
        f.oval('pupil', (x-s*.005, -.253, 1.981), (.012, .003, .019), 'shop_eyes', 10, 5)
        f.oval('eye_light', (x-.011, -.257, 1.989), (.005, .002, .006), 'shop_white', 8, 4)
        f.line('upper_lid', [(x-.070, -.231, 1.981), (x-.030, -.248, 2.007),
                              (x+.032, -.248, 2.008), (x+.070, -.228, 1.985)], .006, 'shop_crease')
        f.panel('lower_lid', [(x-.064, -.235, 1.960), (x, -.250, 1.973),
                              (x+.064, -.235, 1.962), (x+.056, -.231, 1.948),
                              (x-.056, -.231, 1.946)], 'shop_skin', .002)
        f.sweep('arched_brow', [(s*.045, -.229, 2.056), (s*.112, -.239, 2.081),
                                (s*.18, -.209, 2.077), (s*.23, -.170, 2.050)],
                [.008, .014, .011, .0015], 'shop_hair', 8, False, .65)
        for j in range(2):
            f.line('eye_corner_crease', [(s*.206, -.203, 1.984-j*.013),
                                         (s*.238, -.177, 1.986-j*.025)], .003, 'shop_crease')
        f.surface('cheek_plane', [(s*.163, -.222, 1.930), (s*.25, -.160, 1.934),
                                  (s*.273, -.137, 1.86), (s*.225, -.198, 1.826),
                                  (s*.145, -.230, 1.858)], [(0, 1, 2), (0, 2, 3, 4)], 'shop_skin')
        f.line('gentle_smile_fold', [(s*.086, -.247, 1.852), (s*.123, -.237, 1.816),
                                    (s*.136, -.217, 1.785)], .0035, 'shop_crease')
        f.ring('small_earring', (s*.35, -.012, 1.866), .022, 'shop_brass', .005, 'Y', 12)
    f.surface('soft_nose_bridge', [(-.032, -.242, 2.017), (.032, -.242, 2.017),
                                 (-.045, -.283, 1.903), (.045, -.283, 1.903),
                                 (-.04, -.317, 1.886), (.04, -.317, 1.886)],
              [(0, 1, 3, 2), (2, 3, 5, 4), (0, 2, 4), (1, 5, 3)], 'shop_skin', True)
    f.oval('rounded_nose_tip', (0, -.29, 1.877), (.071, .046, .038), 'shop_warm', 12, 6)
    for s in (-1, 1):
        f.line('nostril', [(s*.031, -.326, 1.865), (s*.053, -.314, 1.864)], .004, 'shop_crease')
    f.line('closed_smile', [(-.108, -.220, 1.788), (-.062, -.239, 1.778),
                            (0, -.247, 1.774), (.062, -.239, 1.778), (.108, -.220, 1.788)],
           .006, 'shop_lip', 8)
    f.sweep('lower_lip', [(-.07, -.239, 1.767), (0, -.249, 1.759), (.07, -.239, 1.767)],
            [.004, .009, .003], 'shop_warm', 8, False, .6)
    f.line('forehead_crease', [(-.085, -.224, 2.108), (0, -.238, 2.115),
                              (.085, -.224, 2.108)], .0025, 'shop_crease')
    finish('HEAD_FACE', 'head')


def hair():
    def radius(z):
        if z >= HEAD[-1][0]:
            return .03, .03
        for low, high in zip(HEAD, HEAD[1:]):
            if low[0] <= z <= high[0]:
                t = (z-low[0])/(high[0]-low[0])
                return tuple(low[k]+(high[k]-low[k])*t+.035 for k in (1, 2))
        return .34, .27
    n, rings, vertices = 24, 12, []
    for j in range(rings):
        t = j/(rings-1)
        for i in range(n):
            a = math.tau*i/n
            front = max(0, math.cos(a))
            z = 2.315-(2.315-(1.87+.245*front))*t**1.8
            rx, ry = radius(z)
            vertices.append((rx*math.sin(a), -ry*math.cos(a), z))
    faces = [(j*n+i, j*n+(i+1)%n, (j+1)*n+(i+1)%n, (j+1)*n+i)
             for j in range(rings-1) for i in range(n)]
    faces.append(tuple(reversed(range(n))))
    f.surface('swept_back_cap', vertices, faces, 'shop_hair')
    for s in (-1, 1):
        for j in range(4):
            f.lock('combed_temple', [(s*.035, -.10+j*.05, 2.29-j*.012),
                                     (s*.19, -.17+j*.05, 2.21-j*.015),
                                     (s*.303, -.10+j*.045, 2.02-j*.03)],
                   [.035, .048, .002], 'shop_grey' if j == 2 else 'shop_hair_light')
    f.oval('tied_hair_bun', (0, .245, 2.16), (.175, .145, .16), 'shop_hair', 14, 8, False)
    for j in range(5):
        a = math.pi*(j+.5)/5
        rx, ry, z = .175*math.sin(a), .145*math.sin(a), 2.16+.145*math.cos(a)
        f.line('bun_coil', [(-.9*rx, .245+.5*ry, z),
                            (0, .250+ry, z), (.9*rx, .245+.5*ry, z)],
               .008, 'shop_hair_light')
    f.line('bun_pin', [(-.19, .29, 2.18), (.19, .30, 2.22)], .012, 'shop_wood')
    f.oval('bun_pin_end', (.195, .30, 2.22), (.025, .021, .025), 'shop_brass', 10, 5)
    finish('HAIR_BUN', 'head')


def clothes():
    f.loft('pear_shaped_dress', [(.30, .37, .255), (.40, .43, .29), (.63, .435, .30),
                                (.82, .405, .29), (1.01, .31, .24), (1.13, .29, .225),
                                (1.32, .315, .24), (1.47, .32, .20), (1.55, .15, .12)],
           'shop_dress', 20, False)
    f.loft('neck', [(1.48, .12, .105), (1.73, .125, .11)], 'shop_skin', 12, True)
    for s in (-1, 1):
        f.panel('folded_collar', [(s*.035, -.124, 1.57), (s*.13, -.112, 1.57),
                                (s*.22, -.173, 1.43), (s*.088, -.217, 1.40)], 'shop_cuff', .009)
        f.sweep('short_work_sleeve', [(s*.275, 0, 1.465), (s*.355, -.005, 1.39),
                                     (s*.395, -.018, 1.30)], [.135, .133, .12], 'shop_dress', 12, False)
        f.sweep('rolled_sleeve', [(s*.385, -.015, 1.33), (s*.405, -.02, 1.285)],
                [.128, .12], 'shop_cuff', 12, False)
        for j in range(3):
            u = s*(.45+j*.13)
            f.line('long_skirt_pleat', [(rx*u, -dep*math.sqrt(1-u*u)-.004, z)
                                      for z, rx, dep in [(.96, .334, .254),
                                                        (.66, .431, .298), (.34, .394, .269)]],
                   .0035, 'shop_fold')
    finish('DRESS_BODY', 'dress')
    rows = [(1.43, .15, .20, .32, .24), (1.23, .18, .23, .315, .24),
            (1.04, .255, .24, .315, .245), (.83, .335, .29, .405, .29),
            (.59, .35, .30, .435, .30), (.38, .33, .285, .42, .285)]
    vertices = []
    for z, width, dep, rx, ry in rows:
        for i in range(13):
            x = width*(-1+i/6)
            y = -ry*math.sqrt(max(.01, 1-(x/rx)**2))-.014
            vertices.append((x, y, z+.007*math.cos(i*1.6)))
    apron = f.surface('full_length_apron', vertices,
                      [(j*13+i, j*13+i+1, (j+1)*13+i+1, (j+1)*13+i)
                       for j in range(len(rows)-1) for i in range(12)], 'shop_apron')
    modifier = apron.modifiers.new('APRON_THICKNESS', 'SOLIDIFY')
    modifier.thickness, modifier.offset = .012, 1
    for s in (-1, 1):
        f.ribbon('apron_strap', [(s*.115, -.245, 1.43), (s*.20, -.15, 1.50),
                                 (s*.22, 0, 1.53), (s*.20, .145, 1.50),
                                 (s*.22, .155, 1.43), (s*.18, .205, 1.08)],
                 [.025]*6, 'shop_apron_edge', .008)
        f.oval('strap_button', (s*.115, -.218, 1.414), (.016, .007, .016), 'shop_brass', 10, 5)
    def pocket_y(x, offset=.023):
        return -.30*math.sqrt(1-(x/.425)**2)-offset
    pocket_vertices = [(x, pocket_y(x), z) for z in (.85, .63) for x in (-.19, -.095, 0, .095, .19)]
    pocket = f.surface('deep_apron_pocket', pocket_vertices,
                       [(i, i+1, i+6, i+5) for i in range(4)], 'shop_apron_edge')
    shell = pocket.modifiers.new('POCKET_THICKNESS', 'SOLIDIFY')
    shell.thickness, shell.offset = .006, 1
    f.panel('pocket_patch', [(x, pocket_y(x, .032), z) for x,z in
                            [(.06,.79),(.16,.80),(.17,.67),(.065,.66)]], 'shop_patch', .003)
    for j in range(7):
        x = -.16+j*.052
        f.line('pocket_stitch', [(x, pocket_y(x, .032), .646),
                                (x+.015, pocket_y(x+.015, .032), .651)], .0027, 'shop_cuff')
    for j in range(3):
        x = .078+j*.026
        f.line('patch_stitch', [(x, pocket_y(x, .039), .793),
                                (x+.006, pocket_y(x+.006, .039), .777)], .0025, 'shop_cuff')
    f.panel('lower_apron_mend', [(-.26, -.265, .49), (-.14, -.29, .50),
                                (-.15, -.29, .407), (-.25, -.269, .40)], 'shop_patch', .004)
    finish('APRON', 'apron')
    f.loft('waist_tie', [(1.05, .321, .253), (1.095, .312, .245)], 'shop_belt', 20, False)
    for s in (-1, 1):
        f.line('back_bow', [(0, .26, 1.075), (s*.13, .29, 1.12),
                            (s*.16, .29, 1.06), (0, .26, 1.075),
                            (s*.08, .285, .91)], .014, 'shop_apron_edge')
    f.ring('key_ring', (.32, -.075, 1.03), .042, 'shop_brass', .008, 'Y', 14)
    for j in range(3):
        x = .29+j*.035
        f.ring('key_bow', (x, -.087-j*.012, .98), .017, 'shop_brass', .005, 'Y', 10)
        f.line('key_stem', [(x, -.087-j*.012, .962), (x+.01, -.09-j*.012, .87-j*.015)],
               .007, 'shop_iron')
        g.cube((x+.02, -.09-j*.012, .88-j*.015), (.03, .014, .018), 'shop_iron')
    finish('BELT_KEYS', 'hips')
    for side, s in [('L', -1), ('R', 1)]:
        f.loft('sturdy_low_shoe', [(.025, .105, .16, -.045), (.065, .118, .18, -.055),
                                  (.15, .108, .17, -.045), (.245, .077, .095, .0)],
               'shop_leather', 12, False, center=(s*.17, 0))
        f.loft('shoe_sole', [(.012, .113, .17, -.05), (.055, .12, .18, -.05)],
               'shop_sole', 12, False, center=(s*.17, 0))
        f.line('shoe_seam', [(s*.17-.06, -.17, .16), (s*.17, -.205, .17),
                             (s*.17+.06, -.17, .16)], .005, 'shop_board')
        finish('SHOE_'+side, 'shin.'+side)


def arms():
    for side, s in [('L', -1), ('R', 1)]:
        f.sweep('working_forearm', [(s*.40, -.017, 1.30), (s*.445, -.032, 1.20),
                                    (s*.445, -.095, 1.045), (s*.44, -.10, 1.01)],
                [.112, .106, .075, .07], 'shop_skin', 12, True)
        f.line('wrist_crease', [(s*.44-.035, -.17, 1.045), (s*.44+.035, -.17, 1.045)],
               .003, 'shop_crease')
        finish('ARM_'+side, 'arm_skin.'+side)
        palm_x = s*.445
        f.oval('broad_working_palm', (palm_x, -.108, .958), (.078, .050, .073), 'shop_skin', 12, 6)
        for j in range(4):
            x = palm_x-.050+j*.033
            if side == 'L':
                tip = [.85, .83, .825, .85][j]
                points = [(x, -.12, .94), (x+(j-1.5)*.003, -.13, .89),
                          (x+(j-1.5)*.006, -.13, tip+.018), (x+(j-1.5)*.007, -.12, tip)]
            else:
                points = [(x, -.142, .975), (x, -.17, .93), (x, -.16, .889), (x, -.13, .895)]
            f.sweep('working_finger', points, [.018, .018, .014, .010], 'shop_skin', 8, True)
        thumb = [(palm_x-s*.06, -.11, .99), (palm_x-s*.085, -.145, .955),
                 (palm_x-s*.065, -.173, .931)]
        if side == 'L':
            thumb = [(palm_x-s*.055, -.11, .98), (palm_x-s*.09, -.13, .94),
                     (palm_x-s*.10, -.13, .90)]
        f.sweep('thumb', thumb, [.026, .027, .018], 'shop_skin', 8)
        finish('HAND_'+side, 'wrist.'+side)
    f.loft('folded_fabric_roll', [(.92, .045, .035, -.18), (.95, .055, .043, -.18),
                                 (1.075, .055, .043, -.18), (1.09, .045, .035, -.18)],
           'shop_cloth', 12, False, center=(.445, 0))
    f.ring('fabric_tie', (.445, -.18, 1.005), .056, 'shop_twine', .006, 'Z', 12)
    finish('HELD_ROLL', 'wrist.R')


def counter():
    g.cube((0, -.58, 1.055), (.98, .38, .09), 'shop_wood')
    for j in range(5):
        g.cube((-.39+j*.195, -.58, 1.105), (.19, .38, .025), 'shop_board')
    for s in (-1, 1):
        for y in (-.72, -.44):
            g.cube((s*.40, y, .53), (.055, .055, 1.06), 'shop_wood')
        g.cube((s*.40, -.58, .43), (.045, .32, .05), 'shop_board')
    g.cube((0, -.735, .86), (.86, .025, .23), 'shop_wood')
    for j in range(5):
        f.line('counter_nail', [(-.36+j*.18, -.753, .94), (-.36+j*.18, -.757, .94)],
               .009, 'shop_iron', 6)
    finish('COUNTER', 'root')
    parcel = g.cube((-.035, -.58, 1.197), (.275, .225, .15), 'shop_paper')
    bevel = parcel.modifiers.new('PAPER_FOLDS', 'BEVEL')
    bevel.width, bevel.segments = .012, 1
    for x in (-.04,):
        f.line('parcel_twine', [(x, -.698, 1.19), (x, -.696, 1.279),
                               (x, -.465, 1.279), (x, -.461, 1.19)], .004, 'shop_twine')
    f.line('parcel_cross_twine', [(-.177, -.58, 1.18), (-.176, -.58, 1.279),
                                  (.108, -.58, 1.279), (.108, -.58, 1.18)], .004, 'shop_twine')
    f.line('twine_bow', [(-.04, -.58, 1.28), (-.10, -.61, 1.30), (-.09, -.55, 1.30),
                          (-.04, -.58, 1.28), (.02, -.61, 1.30), (.01, -.55, 1.30),
                          (-.04, -.58, 1.28)], .005, 'shop_twine')
    finish('PARCEL', 'root')
    for j in range(2):
        f.loft('medicine_jar', [(1.12, .045, .045), (1.14, .058, .058),
                               (1.24, .055, .055), (1.26, .034, .034), (1.29, .031, .031)],
               'shop_bottle', 10, False, center=(.29+j*.13, -.62))
        f.loft('jar_cork', [(1.28, .034, .034), (1.315, .034, .034)],
               'shop_wood', 10, False, center=(.29+j*.13, -.62))
    g.cube((-.34, -.62, 1.16), (.17, .18, .075), 'shop_cloth')
    f.ribbon('folded_cloth_edge', [(-.34, -.718, 1.12), (-.34, -.715, 1.20)],
             [.074, .074], 'shop_cuff', .003)
    finish('COUNTER_GOODS', 'root')


def rig_character():
    skeleton = bpy.data.armatures.new('NPC_MERCHANT_SKELETON')
    rig = bpy.data.objects.new('NPC_MERCHANT_RIG', skeleton)
    bpy.context.scene.collection.objects.link(rig)
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode='EDIT')
    def bone(name, head, tail, parent=None):
        b = skeleton.edit_bones.new(name)
        b.head, b.tail = head, tail
        if parent:
            b.parent = skeleton.edit_bones[parent]
    bone('root', (0, 0, 0), (0, 0, .15))
    bone('hips', (0, 0, .76), (0, 0, 1.10), 'root')
    bone('spine', (0, 0, 1.10), (0, 0, 1.54), 'hips')
    bone('head', (0, 0, 1.60), (0, 0, 2.26), 'spine')
    for side, s in [('L', -1), ('R', 1)]:
        bone('arm.'+side, (s*.29, 0, 1.48), (s*.445, -.032, 1.20), 'spine')
        bone('hand.'+side, (s*.445, -.032, 1.20), (s*.44, -.10, 1.01), 'arm.'+side)
        bone('wrist.'+side, (s*.44, -.10, 1.01), (s*.445, -.11, .91), 'hand.'+side)
        bone('thigh.'+side, (s*.17, 0, .76), (s*.17, 0, .43), 'hips')
        bone('shin.'+side, (s*.17, 0, .43), (s*.17, -.02, .07), 'thigh.'+side)
    bpy.ops.object.mode_set(mode='OBJECT')
    rig.show_in_front = True
    for obj in [o for o in bpy.context.scene.objects if o.type == 'MESH']:
        rule = RULES[obj.name]
        groups = {bone.name: obj.vertex_groups.new(name=bone.name) for bone in skeleton.bones}
        for vertex in obj.data.vertices:
            x, y, z = vertex.co
            weights = {rule: 1}
            if rule == 'dress':
                w = max(0, min(1, (z-1.02)/.16))
                weights = {'hips': 1-w, 'spine': w}
                if abs(x) > .31 and z > 1.26:
                    side = 'L' if x < 0 else 'R'
                    blend = max(0, min(1, (abs(x)-.31)/.10))
                    weights = {'spine': 1-blend, 'arm.'+side: blend}
                if abs(x) < .14 and z > 1.52:
                    blend = max(0, min(1, (z-1.52)/.17))
                    weights = {'spine': 1-blend, 'head': blend}
            elif rule == 'apron':
                w = max(0, min(1, (z-1.02)/.20))
                weights = {'hips': 1-w, 'spine': w}
            elif rule.startswith('arm_skin.'):
                side = rule[-1]
                w = max(0, min(1, (1.25-z)/.11))
                weights = {'arm.'+side: 1-w, 'hand.'+side: w}
            for name, weight in weights.items():
                if weight > 0:
                    groups[name].add([vertex.index], weight, 'REPLACE')
        modifier = obj.modifiers.new('MERCHANT_SKIN', 'ARMATURE')
        modifier.object, obj.parent = rig, rig
    return rig


def main():
    bpy.context.preferences.filepaths.save_version = 0
    g.start_collection('NPC_MERCHANT_BESPOKE')
    face()
    hair()
    clothes()
    arms()
    counter()
    rig = rig_character()
    clips = {}
    if '--appearance-only' not in sys.argv:
        sys.path.insert(0, str(HERE))
        from author_stationary import author_clips
        clips = author_clips(rig)
    for key in ('shop_brass', 'shop_iron'):
        shader = g.material(key).node_tree.nodes.get('Principled BSDF')
        shader.inputs['Metallic'].default_value = .65
    f.stylize_scene()
    bpy.context.scene.render.engine = 'BLENDER_EEVEE'
    bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'model.blend'))
    bpy.ops.object.select_all(action='SELECT')
    temporary = HERE/'model.pending.glb'
    with f.export_materials():
        bpy.ops.export_scene.gltf(filepath=str(temporary), export_format='GLB', use_selection=True,
                                  export_extras=True, export_animations=bool(clips),
                                  export_animation_mode='ACTIONS', export_force_sampling=True,
                                  export_anim_slide_to_zero=True)
    temporary.replace(HERE/'model.glb')
    info = {'characterId': 'merchant', 'stage': 'bespoke_shopkeeper_01',
            'concept': 'kind middle-aged woman; tied hair, pear-shaped dress and patched apron',
            'source': 'model.blend', 'export': 'model.glb', 'rig': rig.name,
            'boneCount': len(rig.data.bones), 'meshes': PARTS, 'clips': clips,
            'gameIntegrated': True, 'gameExecutionVerified': False,
            'stationary': True, 'locomotionClips': [],
            'counterContact': {'topZ': 1.1175, 'parcelTopZ': 1.30,
                               'rigidMeshes': ['COUNTER', 'PARCEL', 'COUNTER_GOODS']},
            'materialExport': 'four-band Blender preview; GLB PBR color fallback'}
    (HERE/'character.json').write_text(json.dumps(info, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('MERCHANT_READY', len(PARTS), sum(p['triangles'] for p in PARTS), flush=True)


if __name__ == '__main__':
    main()
