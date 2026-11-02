"""Bespoke Raen, based on the original silver-haired sorceress character sheet."""

import importlib.util
import json
import math
import sys
from pathlib import Path

import bpy

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1]/'ModularCollection'))
import create_collection as g
import surface_forms as f

COLORS = {
    'raen_skin': (.88, .67, .55, 1), 'raen_shade': (.43, .27, .22, 1),
    'raen_lip': (.47, .23, .23, 1), 'raen_eye': (.12, .035, .04, 1),
    'raen_iris': (.40, .10, .09, 1), 'raen_white': (.80, .76, .67, 1),
    'raen_hair': (.65, .68, .68, 1), 'raen_hair_light': (.81, .82, .79, 1),
    'raen_hair_shade': (.53, .55, .56, 1), 'raen_coat': (.065, .08, .12, 1),
    'raen_coat_light': (.12, .145, .19, 1), 'raen_lining': (.25, .08, .105, 1),
    'raen_scarf': (.34, .08, .105, 1), 'raen_scarf_shade': (.20, .04, .055, 1),
    'raen_gold': (.48, .35, .19, 1), 'raen_darkgold': (.27, .20, .11, 1),
    'raen_cream': (.71, .67, .60, 1), 'raen_paper': (.71, .64, .49, 1),
    'raen_ink': (.42, .12, .10, 1), 'raen_black': (.03, .035, .045, 1),
    'raen_boot': (.07, .065, .06, 1), 'raen_sole': (.025, .025, .03, 1),
    'raen_belt': (.20, .09, .075, 1),
}
g.COLORS.update(COLORS)
PARTS, RULES = [], {}
FINGER_BONES = []
HEAD = [(1.69, .055, .105), (1.73, .115, .145), (1.79, .183, .177),
        (1.86, .233, .198), (1.95, .263, .211), (2.06, .27, .215),
        (2.17, .23, .19), (2.24, .15, .13), (2.29, .012, .012)]


def finish(suffix, rule):
    obj = g.finish('NPC_RAEN_'+suffix, 'bespoke', len(PARTS), PARTS)
    obj['characterId'] = 'raen'
    obj['design'] = 'long silver hair, burgundy scarf, dark split coat and sealing charms'
    RULES[obj.name] = rule
    return obj


def face():
    # Short jaw and continuous cheek normals keep the illustrated young-adult face.
    f.loft('soft_young_face', HEAD, 'raen_skin', 32, True)
    for sign in (-1, 1):
        f.oval('ear', (sign*.265, .008, 1.95), (.027, .034, .052), 'raen_skin', 12, 6)
        x = sign*.112
        # Almond eye surfaces follow the cheek, rather than floating flat in front.
        outline = [(-.075, -.004), (-.046, .027), (0, .039), (.046, .028),
                   (.075, .005), (.044, -.025), (0, -.032), (-.045, -.023)]
        verts = [(x, -.211, 1.973)]
        for dx, dz in outline:
            xx = x+dx
            verts.append((xx, -.211+ .47*xx*xx, 1.973+dz))
        f.surface('almond_eye', verts, [(0,i+1,(i+1)%8+1) for i in range(8)], 'raen_white', True)
        f.oval('ruby_iris', (x, -.211, 1.976), (.031, .009, .034), 'raen_iris', 16, 8)
        f.oval('pupil', (x, -.219, 1.979), (.013, .004, .024), 'raen_eye', 12, 6)
        f.oval('eye_light', (x-.010, -.224, 1.991), (.008, .002, .009), 'raen_white', 10, 5)
        upper = [(xx, yy-.004, zz) for xx, yy, zz in verts[1:6]]
        f.sweep('upper_lash', upper, [.002,.005,.006,.006,.002], 'raen_eye', 6)
        f.sweep('lash_tip', [(sign*.167, -.201, 1.991), (sign*.198, -.191, 2.006)],
                [.005,.0005], 'raen_eye', 6)
        f.sweep('soft_brow', [(sign*.044,-.210,2.042),(sign*.104,-.207,2.055),
                              (sign*.177,-.181,2.043)], [.002,.005,.001], 'raen_hair_shade', 6)
    f.oval('small_nose', (0, -.206, 1.908), (.015, .017, .019), 'raen_skin', 12, 6)
    f.sweep('quiet_mouth', [(-.034,-.191,1.839),(0,-.200,1.836),(.034,-.191,1.839)],
            [.001,.0025,.001], 'raen_lip', 6)
    finish('HEAD_FACE', 'head')


def hair():
    n, rows, vertices = 24, 12, []
    def radius(z):
        if z >= HEAD[-1][0]:
            return .025, .025
        for a, b in zip(HEAD, HEAD[1:]):
            if a[0] <= z <= b[0]:
                t = (z-a[0])/(b[0]-a[0])
                return tuple(a[k]+(b[k]-a[k])*t+.035 for k in (1, 2))
        return .29, .23
    for j in range(rows):
        t = j/(rows-1)
        for i in range(n):
            a = math.tau*i/n
            z = 2.295-(2.295-(1.79+.325*max(0, math.cos(a))))*t**1.8
            rx, ry = radius(z)
            vertices.append((rx*math.sin(a), -ry*math.cos(a), z))
    faces = [(j*n+i, j*n+(i+1)%n, (j+1)*n+(i+1)%n, (j+1)*n+i)
             for j in range(rows-1) for i in range(n)]
    faces.append(tuple(reversed(range(n))))
    f.surface('silver_hair_cap', vertices, faces, 'raen_hair')
    for s in (-1, 1):
        for j in range(4):
            f.lock('layered_bob', [(s*(.045+j*.045), -.10+j*.045, 2.285-j*.012),
                                   (s*(.17+j*.035), -.20+j*.045, 2.16-j*.018),
                                   (s*(.25+j*.015), -.16+j*.04, 1.89-j*.035)],
                   [.030, .044, .002], 'raen_hair_light' if j%2==0 else 'raen_hair_shade')
    for j in range(5):
        x = -.20+j*.095
        f.lock('asymmetric_fringe', [(x*.55, -.14, 2.27), (x, -.224, 2.17),
                                    (x+.025, -.235, 2.04+(.025 if j%2 else 0))],
               [.036, .045, .002], 'raen_hair_light' if j%2 else 'raen_hair')
    # Gold hair ornament and red cord are deliberately visible from the front.
    f.ring('temple_clasp', (.255, -.155, 2.085), .042, 'raen_gold', .007, 'Y', 16)
    f.oval('clasp_center', (.255, -.166, 2.085), (.016, .008, .016), 'raen_scarf', 10, 5)
    for j in range(5):
        a = math.tau*j/5
        f.oval('clasp_petal', (.255+.032*math.cos(a), -.164, 2.085+.032*math.sin(a)),
               (.011, .005, .014), 'raen_gold', 8, 4)
    for i in (-1, 1):
        f.ribbon('temple_ribbon', [(.29, -.04, 2.04), (.30+i*.03, -.015, 1.94),
                                   (.30+i*.04, .01, 1.84)], [.018, .02, .002], 'raen_scarf', .005)
    finish('HAIR_SILVER_FRONT', 'head')

    # A continuous rear silhouette fills the space between layered hair strands.
    rows = [(2.08, .24, .225), (1.91, .275, .258), (1.79, .286, .265), (1.58, .292, .252),
            (1.37, .285, .245), (1.14, .265, .245), (.97, .21, .238)]
    vertices = []
    columns = 13
    for j, (z, width, depth) in enumerate(rows):
        for i in range(columns):
            u = -1+2*i/(columns-1)
            vertices.append((width*u, depth-.048*u*u,
                             z + (.025*math.sin(i*1.8) if j == len(rows)-1 else 0)))
    faces = [(j*columns+i, j*columns+i+1, (j+1)*columns+i+1, (j+1)*columns+i)
             for j in range(len(rows)-1) for i in range(columns-1)]
    mass = f.surface('long_back_hair_mass', vertices, faces, 'raen_hair', False)
    shell = mass.modifiers.new('LONG_HAIR_THICKNESS', 'SOLIDIFY')
    shell.thickness, shell.offset = .025, -1
    for j in range(11):
        u = -1+2*j/10
        points = [(.24*u, .235-.048*u*u, 2.08),
                  (.275*u, .269-.048*u*u, 1.91),
                  (.288*u, .280-.048*u*u, 1.72),
                  (.292*u, .273-.048*u*u, 1.46),
                  (.265*u+.012*math.sin(j), .266-.048*u*u, 1.18),
                  (.21*u+.018*math.sin(j*.8), .258-.048*u*u, .96+.035*math.sin(j*1.7))]
        f.lock('layered_long_hair', points, [.028, .035, .042, .048, .04, .002],
               'raen_hair_light' if j%3 == 0 else 'raen_hair_shade' if j%3 == 1 else 'raen_hair')
    finish('HAIR_LONG_BACK', 'head')
    for side, s in [('L', -1), ('R', 1)]:
        for j in range(3):
            x = s*(.235+j*.027)
            f.lock('long_face_framing_lock', [(x*.94, -.15+j*.018, 2.02),
                                              (x*1.03, -.17+j*.02, 1.79),
                                              (x*1.05, -.16+j*.02, 1.53),
                                              (x*.96+s*.008, -.16+j*.02, 1.25+j*.04)],
                   [.036, .042, .036, .002],
                   'raen_hair_light' if j == 0 else 'raen_hair' if j == 1 else 'raen_hair_shade')
        finish('HAIR_LONG_SIDE_'+side, 'head')


def outfit():
    f.loft('fitted_jacket', [(.88, .25, .17), (1.03, .245, .18), (1.23, .265, .20),
                              (1.39, .29, .19), (1.50, .28, .16), (1.56, .13, .11)],
           'raen_coat', 20, False)
    f.loft('neck', [(1.48, .10, .09), (1.70, .105, .095)], 'raen_skin', 12, True)
    f.panel('wrapped_light_chest', [(-.19, -.158, 1.43), (.19, -.158, 1.43),
                                   (.21, -.20, 1.22), (-.21, -.20, 1.22)], 'raen_cream', .007)
    for s in (-1, 1):
        f.panel('jacket_front', [(s*.16, -.16, 1.49), (s*.29, -.11, 1.43),
                                 (s*.27, -.14, 1.10), (s*.19, -.17, .91),
                                 (s*.12, -.20, 1.20)], 'raen_coat_light', .008)
        f.line('jacket_gold_edge', [(s*.16, -.17, 1.46), (s*.13, -.206, 1.29),
                                    (s*.19, -.19, 1.10), (s*.19, -.18, .92)], .006, 'raen_gold')
        f.sweep('coat_sleeve', [(s*.275, 0, 1.465), (s*.355, -.005, 1.39),
                                (s*.395, -.018, 1.30)], [.118, .12, .11], 'raen_coat', 12, False)
        f.sweep('brass_cuff', [(s*.385, -.015, 1.33), (s*.405, -.02, 1.285)],
                [.115, .108], 'raen_darkgold', 12, False)
    f.line('crossed_chest_wrap', [(-.15, -.218, 1.38), (0, -.227, 1.29),
                                   (.16, -.20, 1.23)], .009, 'raen_belt')
    for sign in (-1, 1):
        # Small clasp and cuff engraving retain the original outfit's gold accents.
        f.ring('collar_brooch', (sign*.219, -.164, 1.424), .025, 'raen_gold', .004, 'Y', 12)
        f.line('collar_brooch_cross', [(sign*.219,-.171,1.443),(sign*.219,-.171,1.406)], .003,'raen_gold')
    finish('JACKET_AND_WRAP', 'dress')
    f.loft('shorts', [(.73, .25, .175), (.84, .275, .19), (1.01, .25, .18)],
           'raen_coat', 18, False)
    for s in (-1, 1):
        f.loft('upper_thigh', [(.66, .09, .09), (.80, .098, .10), (.84, .103, .10)],
               'raen_skin', 12, True, center=(s*.17, 0))
        f.loft('stocking', [(.26, .075, .08), (.50, .084, .09), (.69, .095, .095)],
               'raen_black', 12, False, center=(s*.17, 0))
    finish('SHORTS_AND_STOCKINGS', 'hips')
    for side, s in [('L', -1), ('R', 1)]:
        f.loft('tall_boot', [(.02, .09, .15, -.045), (.065, .105, .17, -.05),
                              (.18, .09, .14, -.04), (.42, .075, .085),
                              (.49, .083, .09)], 'raen_boot', 12, False, center=(s*.17, 0))
        f.loft('boot_sole', [(.012, .106, .17, -.05), (.055, .108, .17, -.05)],
               'raen_sole', 12, False, center=(s*.17, 0))
        for z in (.20, .31, .42):
            f.line('boot_lace', [(s*.17-.04, -.122, z), (s*.17+.04, -.122, z+.025)],
                   .004, 'raen_scarf')
        finish('BOOT_'+side, 'shin.'+side)
    # Two independent split coat panels leave the stockings and boots visible from the front.
    for side, s in [('L', -1), ('R', 1)]:
        verts = [(s*.20, .11, 1.22), (s*.30, -.06, 1.15), (s*.32, -.09, .86),
                 (s*.37, -.06, .38), (s*.40, .02, .17), (s*.27, .23, .22),
                 (s*.14, .25, .43), (s*.13, .23, .90)]
        f.panel('long_split_coat', verts, 'raen_coat', .012)
        f.panel('coat_lining', [(s*.29, .012, .48), (s*.37, .015, .20),
                               (s*.27, .235, .24), (s*.20, .23, .50)], 'raen_lining', .005)
        f.line('coat_hem_gold', [(s*.32, -.10, .86), (s*.37, -.07, .38),
                                  (s*.40, .01, .18), (s*.27, .235, .23)], .006, 'raen_darkgold')
        # Raised low-poly embroidery follows the visible front edge of each tail.
        f.line('embroidered_vine', [(s*.348,-.085,.29),(s*.324,-.103,.42),
                                   (s*.301,-.111,.56),(s*.281,-.117,.71)], .0035,'raen_gold')
        for z, x in ((.38,.331),(.49,.312),(.60,.294)):
            for direction in (-1,1):
                f.panel('embroidered_leaf', [(s*x,-.119,z),(s*(x+direction*.029),-.119,z+.025),
                                             (s*(x+direction*.022),-.119,z+.051),
                                             (s*x,-.119,z+.022)],'raen_darkgold',.001)
        finish('COAT_TAIL_'+side, 'hips')
    f.loft('scarf_collar', [(1.49, .15, .13), (1.57, .18, .155), (1.695, .125, .115)],
           'raen_scarf', 16, False)
    f.line('scarf_fold', [(-.14, -.12, 1.57), (0, -.16, 1.55), (.14, -.12, 1.57)],
           .007, 'raen_scarf_shade')
    finish('SCARF_COLLAR', 'spine')
    for j in range(2):
        s = -1 if j == 0 else 1
        f.ribbon('trailing_scarf', [(s*.09, .12, 1.61), (s*.17, .22, 1.50),
                                    (s*(.23+j*.04), .34, 1.30),
                                    (s*(.29+j*.05), .40, 1.03-j*.10)],
                 [.08, .085, .075, .012], 'raen_scarf' if j==0 else 'raen_scarf_shade', .008)
    finish('SCARF_TAILS', 'spine')
    f.loft('layered_belt', [(.94, .26, .19), (1.01, .255, .185)], 'raen_belt', 18, False)
    f.ring('belt_clasp', (0, -.201, .98), .036, 'raen_gold', .008, 'Y', 14)
    for j in range(2):
        x = -.18+j*.28
        f.panel('sealing_talisman', [(x, -.198, .95), (x+.06, -.20, .95),
                                    (x+.06, -.207, .69), (x, -.207, .69)], 'raen_paper', .005)
        for k in range(3):
            f.line('seal_ink', [(x+.015, -.215, .89-k*.052),
                                (x+.045, -.215, .90-k*.052)], .003, 'raen_ink')
    f.ring('round_belt_charm', (.29, -.09, .87), .052, 'raen_gold', .010, 'Y', 16)
    f.oval('round_belt_charm_center', (.29, -.10, .87), (.033, .012, .033), 'raen_darkgold', 12, 6)
    finish('BELT_AND_SEALS', 'hips')


def hands():
    for side, s in [('L', -1), ('R', 1)]:
        f.sweep('arm', [(s*.40, -.017, 1.30), (s*.445, -.032, 1.20),
                        (s*.445, -.095, 1.045), (s*.44, -.10, 1.01)],
                [.095, .085, .065, .06], 'raen_coat', 12, True)
        f.sweep('gauntlet', [(s*.445, -.05, 1.16), (s*.44, -.10, 1.02)],
                [.078, .060], 'raen_coat_light', 12, False)
        for z in (1.05, 1.11):
            f.line('gauntlet_band', [(s*.44-.05, -.16, z), (s*.44+.05, -.16, z)], .005, 'raen_gold')
        finish('ARM_'+side, 'arm_skin.'+side)
        x = s*.445
        f.oval('gloved_palm', (x, -.108, .971), (.052, .033, .052), 'raen_black', 12, 6)
        if side == 'R':
            # Fingerless glove: visible fingertips make the crossed index/middle readable.
            for j, length in enumerate((.100,.112,.091,.073)):
                fx = x-.034+j*.023
                points = [(fx,-.110,.944),(fx,-.110,.944-length*.52),
                          (fx,-.102,.944-length)]
                for segment in range(2):
                    name = f'finger_{j}_{segment}.R'
                    parent = 'wrist.R' if segment == 0 else f'finger_{j}_0.R'
                    FINGER_BONES.append((name,points[segment],points[segment+1],parent))
                    ob = f.sweep(name, points[segment:segment+2], [.012,.009] if segment else [.013,.012],
                                 'raen_skin', 8, True)
                    group = ob.vertex_groups.new(name=name)
                    group.add(list(range(len(ob.data.vertices))),1,'REPLACE')
            points = [(x-.043,-.108,.982),(x-.067,-.102,.954),(x-.067,-.089,.931)]
            for segment in range(2):
                name = f'finger_4_{segment}.R'
                FINGER_BONES.append((name,points[segment],points[segment+1],
                                     'wrist.R' if segment == 0 else 'finger_4_0.R'))
                ob = f.sweep(name,points[segment:segment+2],[.017,.012],'raen_skin',8,True)
                ob.vertex_groups.new(name=name).add(list(range(len(ob.data.vertices))),1,'REPLACE')
        else:
            for j, length in enumerate((.060,.078,.072,.052)):
                fx = x+s*(-.034+j*.023)
                f.sweep('relaxed_finger', [(fx,-.110,.944),(fx,-.114,.944-length*.55),
                                          (fx,-.098,.944-length),(fx,-.088,.948-length)],
                        [.012,.012,.010,.005],'raen_skin',8,True)
            f.sweep('relaxed_thumb', [(x-s*.042,-.108,.984),(x-s*.064,-.101,.957),
                                      (x-s*.062,-.084,.936)],[.018,.015,.008],'raen_skin',8,True)
        finish('HAND_'+side, 'wrist.'+side)


def add_finger_rig(rig):
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode='EDIT')
    for name, head, tail, parent in FINGER_BONES:
        bone = rig.data.edit_bones.new(name)
        bone.head, bone.tail = head, tail
        bone.parent = rig.data.edit_bones[parent]
    bpy.ops.object.mode_set(mode='OBJECT')
    obj = bpy.data.objects['NPC_RAEN_HAND_R']
    finger_groups = {obj.vertex_groups[name].index for name, *_ in FINGER_BONES}
    for vertex in obj.data.vertices:
        if any(group.group in finger_groups for group in vertex.groups):
            obj.vertex_groups['wrist.R'].remove([vertex.index])


def add_hair_rig(rig):
    """Blend the long strands between the head and two separately keyed hair bones."""
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode='EDIT')
    for side, s in [('L', -1), ('R', 1)]:
        bone = rig.data.edit_bones.new('hair.'+side)
        bone.head = (s*.13, .23, 1.95)
        bone.tail = (s*.19, .25, 1.12)
        bone.parent = rig.data.edit_bones['head']
    bpy.ops.object.mode_set(mode='OBJECT')
    for suffix in ('HAIR_LONG_BACK', 'HAIR_LONG_SIDE_L', 'HAIR_LONG_SIDE_R'):
        obj = bpy.data.objects['NPC_RAEN_'+suffix]
        head = obj.vertex_groups['head']
        left = obj.vertex_groups.new(name='hair.L')
        right = obj.vertex_groups.new(name='hair.R')
        for vertex in obj.data.vertices:
            blend = max(0., min(1., (1.88-vertex.co.z)/.62))
            if blend < 1:
                head.add([vertex.index], 1-blend, 'REPLACE')
            else:
                head.remove([vertex.index])
            side = max(0., min(1., (vertex.co.x+.08)/.16))
            if suffix.endswith('_L'):
                side = 0
            elif suffix.endswith('_R'):
                side = 1
            if blend*(1-side) > 0:
                left.add([vertex.index], blend*(1-side), 'REPLACE')
            if blend*side > 0:
                right.add([vertex.index], blend*side, 'REPLACE')


def main():
    bpy.context.preferences.filepaths.save_version = 0
    g.start_collection('NPC_RAEN_BESPOKE')
    face()
    hair()
    outfit()
    hands()
    spec = importlib.util.spec_from_file_location('shop_rig_helper', HERE.parent/'Merchant'/'create_merchant.py')
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    helper.RULES = RULES
    rig = helper.rig_character()
    rig.name, rig.data.name = 'NPC_RAEN_RIG', 'NPC_RAEN_SKELETON'
    add_hair_rig(rig)
    add_finger_rig(rig)
    sys.path.insert(0, str(HERE))
    from author_stationary import author_clips
    clips = author_clips(rig)
    f.stylize_scene()
    for mat in bpy.data.materials:
        if 'RAEN_SKIN' in mat.name.upper() and mat.use_nodes:
            for node in mat.node_tree.nodes:
                if node.type == 'VALTORGB':
                    node.color_ramp.interpolation = 'LINEAR'
                    for element, intensity in zip(node.color_ramp.elements, (.65,.78,.9,1.)):
                        element.color = (*(c*intensity for c in mat.diffuse_color[:3]),1)

    bpy.context.scene.render.engine = 'BLENDER_EEVEE'
    bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'model.blend'))
    bpy.ops.object.select_all(action='SELECT')
    temporary = HERE/'model.pending.glb'
    with f.export_materials():
        bpy.ops.export_scene.gltf(filepath=str(temporary), export_format='GLB', use_selection=True,
                                  export_extras=True, export_animations=True,
                                  export_animation_mode='ACTIONS', export_force_sampling=True,
                                  export_anim_slide_to_zero=True)
    temporary.replace(HERE/'model.glb')
    info = {'characterId': 'raen', 'stage': 'bespoke_sorceress_crossed_seal_04',
            'reference': 'SimpleGame/Assets/Characters/Story/raen.png',
            'concept': 'long layered silver hair, burgundy scarf, dark split coat, talismans and boots',
            'source': 'model.blend', 'export': 'model.glb', 'rig': rig.name,
            'boneCount': len(rig.data.bones), 'meshes': PARTS, 'clips': clips,
            'gameIntegrated': True, 'gameExecutionVerified': False, 'stationary': True,
            'locomotionClips': []}
    (HERE/'character.json').write_text(json.dumps(info, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('RAEN_READY', len(PARTS), sum(p['triangles'] for p in PARTS), flush=True)


if __name__ == '__main__':
    main()
