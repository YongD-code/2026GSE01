"""First bespoke blacksmith sculpt: a boisterous, broad, shaggy-bearded dwarf.

Blender --background --factory-startup --python this_file
Only writes this character's modelling sources; no game files or game build.
"""

import json
import math
import sys
from pathlib import Path

import bpy

HERE = Path(__file__).resolve().parent
BLENDER = HERE.parents[1]
sys.path.insert(0, str(BLENDER/'ModularCollection'))
import create_collection as g
import surface_forms as f


PALETTE = {
    'smith_skin': (.56, .30, .175, 1), 'smith_skin_warm': (.53, .235, .125, 1),
    'smith_skin_shade': (.27, .11, .055, 1), 'smith_soot': (.22, .15, .11, 1),
    'smith_hair': (.16, .059, .023, 1), 'smith_hair_mid': (.23, .085, .030, 1),
    'smith_hair_tip': (.32, .14, .052, 1), 'smith_hair_ash': (.30, .24, .17, 1),
    'smith_eye': (.10, .055, .021, 1), 'smith_iris': (.30, .18, .06, 1),
    'smith_white': (.70, .61, .43, 1), 'smith_mouth': (.075, .023, .014, 1),
    'smith_shirt': (.065, .09, .09, 1), 'smith_linen': (.39, .36, .27, 1),
    'smith_pants': (.07, .075, .068, 1), 'smith_leather': (.22, .095, .040, 1),
    'smith_leather_edge': (.36, .19, .085, 1), 'smith_belt': (.095, .045, .021, 1),
    'smith_boot': (.11, .065, .038, 1), 'smith_sole': (.023, .025, .022, 1),
    'smith_iron': (.15, .17, .17, 1), 'smith_iron_edge': (.30, .33, .31, 1),
    'smith_brass': (.44, .26, .085, 1), 'smith_wood': (.24, .105, .034, 1),
}
g.COLORS.update(PALETTE)
PARTS = []
SKIN_RULES = {}


def finish(suffix, bone):
    name = 'NPC_SMITH_'+suffix
    obj = g.finish(name, 'bespoke', len(PARTS), PARTS)
    obj['characterId'] = 'smith'
    obj['design'] = 'boisterous dwarf / unique anatomy and costume'
    SKIN_RULES[name] = bone
    return obj


def face():
    f.loft('broad_jaw_and_brow', [
        (1.25, .20, .16, -.005), (1.32, .32, .235),
        (1.46, .375, .29), (1.61, .38, .292),
        (1.75, .34, .263, .012), (1.84, .23, .18, .025),
        (1.875, .015, .015, .025)], 'smith_skin', 20, True)
    for s in (-1, 1):
        f.oval('broad_ear', (s*.375, .015, 1.57), (.072, .065, .11), 'smith_skin', 12, 6)
        f.oval('ear_hollow', (s*.411, -.035, 1.57), (.029, .016, .055), 'smith_skin_shade', 10, 5)
        f.surface('high_cheek_plane', [(s*.205, -.255, 1.535), (s*.30, -.198, 1.54),
                                       (s*.337, -.163, 1.49), (s*.28, -.224, 1.455),
                                       (s*.20, -.266, 1.47), (s*.265, -.241, 1.505)],
                  [(5, i, (i+1)%5) for i in range(5)], 'smith_skin', True)
        x = s*.151
        f.oval('squinting_eye', (x, -.276, 1.608), (.079, .017, .026), 'smith_white', 16, 6)
        f.oval('warm_iris', (x-s*.006, -.294, 1.610), (.027, .008, .023), 'smith_iris', 12, 5)
        f.oval('pupil', (x-s*.006, -.302, 1.610), (.014, .004, .020), 'smith_eye', 10, 5)
        f.oval('eye_light', (x-.013, -.306, 1.615), (.006, .002, .007), 'smith_white', 8, 4)
        f.line('heavy_upper_lid', [(x-.078, -.274, 1.601), (x-.035, -.294, 1.630),
                                  (x+.026, -.295, 1.632), (x+.078, -.272, 1.605)],
               .009, 'smith_skin_shade')
        f.panel('raised_lower_lid', [(x-.075, -.282, 1.587), (x, -.298, 1.597),
                                     (x+.075, -.282, 1.587), (x+.065, -.282, 1.568),
                                     (x-.065, -.282, 1.568)], 'smith_skin', .003)
        f.line('laughing_lower_lid', [(x-.065, -.282, 1.589), (x, -.299, 1.597),
                                      (x+.064, -.281, 1.589)], .004, 'smith_skin_shade')
        for j in range(2):
            f.line('crows_foot', [(s*.231, -.257, 1.592-j*.015),
                                  (s*(.28+j*.009), -.225, 1.600-j*.027)], .0045, 'smith_skin_shade')
        f.sweep('bushy_eyebrow', [(s*.037, -.275, 1.681), (s*.12, -.287, 1.706),
                                 (s*.215, -.259, 1.708), (s*.285, -.214, 1.681)],
                [.017, .031, .026, .003], 'smith_hair', 8, False, .62)
        for j in range(3):
            f.line('brow_hair', [(s*(.08+j*.045), -.297+j*.008, 1.707),
                                (s*(.10+j*.045), -.30+j*.008, 1.732-j*.005)], .006, 'smith_hair_mid')
    # Nose bridge and broad flattened tip are sculpted as separate planes.
    f.surface('wide_nose_bridge', [(-.045, -.276, 1.662), (.045, -.276, 1.662),
                                  (-.072, -.355, 1.546), (.072, -.355, 1.546),
                                  (-.052, -.425, 1.512), (.052, -.425, 1.512)],
              [(0, 1, 3, 2), (2, 3, 5, 4), (0, 2, 4), (1, 5, 3)], 'smith_skin', True)
    f.loft('broad_nose_tip', [(1.470, .068, .040, -.365), (1.486, .112, .063, -.373),
                             (1.526, .108, .058, -.374), (1.551, .055, .035, -.36)],
           'smith_skin_warm', 12, True)
    for s in (-1, 1):
        f.oval('nostril', (s*.067, -.419, 1.484), (.023, .009, .011), 'smith_skin_shade', 10, 4)
    f.panel('open_grin', [(-.153, -.298, 1.448), (-.105, -.327, 1.424),
                          (0, -.342, 1.409), (.105, -.327, 1.424), (.153, -.298, 1.448),
                          (.108, -.328, 1.393), (0, -.345, 1.376), (-.108, -.328, 1.393)],
            'smith_mouth', .004)
    f.surface('smiling_teeth', [(-.118, -.326, 1.433), (0, -.350, 1.419),
                               (.118, -.326, 1.433), (-.108, -.327, 1.416),
                               (0, -.351, 1.402), (.108, -.327, 1.416)],
              [(0, 1, 4, 3), (1, 2, 5, 4)], 'smith_white')
    # Small work stains and creases, readable as an experienced craftsman rather than a child.
    for s in (-1, 1):
        f.line('smile_crease', [(s*.157, -.286, 1.448), (s*.191, -.275, 1.476),
                               (s*.215, -.257, 1.486)], .007, 'smith_skin_shade')
        f.panel('cheek_soot', [(s*.26, -.265, 1.49), (s*.31, -.237, 1.475),
                               (s*.29, -.249, 1.46)], 'smith_soot', .002)
    f.line('forehead_crease', [(-.11, -.258, 1.727), (0, -.275, 1.738),
                              (.11, -.258, 1.727)], .004, 'smith_skin_shade')
    f.ring('single_bronze_earring', (-.423, -.008, 1.489), .038, 'smith_brass', .008, 'Y', 16)
    finish('HEAD_FACE', 'head')


def hair_and_beard():
    n = 24
    vertices = []
    head_outline = [(1.46, .375, .29), (1.61, .38, .292),
                    (1.75, .34, .263), (1.84, .23, .18), (1.875, .015, .015)]
    def cap_radius(z):
        if z >= head_outline[-1][0]:
            return .025, .025
        for low, high in zip(head_outline, head_outline[1:]):
            if low[0] <= z <= high[0]:
                blend = (z-low[0])/(high[0]-low[0])
                return tuple(low[k]+(high[k]-low[k])*blend+.038 for k in (1, 2))
        return .393, .308
    rings = 12
    for j in range(rings):
        t = j/(rings-1)
        for i in range(n):
            a = math.tau*i/n
            theta = .06+1.5*t
            front = max(0, math.cos(a))
            z = 1.84+.066*math.cos(theta)-(1.84-(1.51+.245*front))*t*t
            rx, ry = cap_radius(z)
            vertices.append((rx*math.sin(a), .02-ry*math.cos(a), z))
    faces = [(j*n+i, j*n+(i+1)%n, (j+1)*n+(i+1)%n, (j+1)*n+i)
             for j in range(rings-1) for i in range(n)]
    faces.append(tuple(reversed(range(n))))
    f.surface('receding_hair_cap', vertices, faces, 'smith_hair', False)
    for j in range(7):
        x = -.27+j*.09
        f.lock('unruly_crown', [(x*.6, -.10, 1.855), (x*.85-.025, -.02, 1.925+.025*math.sin(j*1.7)),
                               (x+.045, .13, 1.87), (x+.06, .25, 1.74)],
               [.053, .069, .058, .003], 'smith_hair_mid' if j%3 == 0 else 'smith_hair')
    for s in (-1, 1):
        for j in range(3):
            f.lock('swept_temple', [(s*.27, -.15+j*.08, 1.79),
                                   (s*.37, -.075+j*.08, 1.69),
                                   (s*.36, .02+j*.075, 1.49)],
                   [.05, .068, .005], 'smith_hair_ash' if j == 1 else 'smith_hair')
    for j in range(7):
        x = -.28+j*.093
        f.lock('ragged_nape', [(x*.9, .255, 1.77), (x, .29, 1.61),
                              (x+.025, .29, 1.43+.025*(j%3))], [.055, .074, .003], 'smith_hair')
    finish('HAIR_UNKEMPT', 'head')

    # One coherent beard volume, with overlapping directional tufts and a ragged perimeter.
    rows = [(1.40, .29, -.295), (1.31, .345, -.335), (1.17, .315, -.365),
            (1.04, .24, -.36), (.945, .10, -.335)]
    vertices = []
    for z, width, cy in rows:
        for i in range(15):
            u = -1+i/7
            vertices.append((u*width, cy+.13*u*u, z+.022*math.cos(i*2.2)))
    faces = [(j*15+i, j*15+i+1, (j+1)*15+i+1, (j+1)*15+i)
             for j in range(4) for i in range(14)]
    beard = f.surface('full_beard_mass', vertices, faces, 'smith_hair', False)
    beard_shell = beard.modifiers.new('BEARD_THICKNESS', 'SOLIDIFY')
    beard_shell.thickness, beard_shell.offset = .07, 1
    for layer, (z, width, cy, length) in enumerate([
            (1.41, .31, -.327, .25), (1.28, .32, -.375, .28),
            (1.14, .24, -.393, .24)]):
        for j in range(9 if layer < 2 else 7):
            count = 9 if layer < 2 else 7
            u = -1+2*j/(count-1)
            x = width*u
            y = cy+.11*u*u
            top = z-.075*max(0, 1-abs(u)/.62) if layer == 0 else z
            tip = z-length+.035*math.sin(j*2.6+layer)
            f.lock('shaggy_beard_lock', [(x, y, top), (x*.99, y-.028, z-length*.53),
                                        (x*.89+.012*math.sin(j), y-.015, tip)],
                   [.042 if layer == 0 else .048, .059, .002],
                   'smith_hair_mid' if (j+layer)%3 else 'smith_hair_tip')
    for s in (-1, 1):
        for j in range(3):
            f.lock('broad_sideburn', [(s*(.29+j*.022), -.14+j*.025, 1.61-j*.04),
                                     (s*(.35+j*.018), -.205, 1.42-j*.035),
                                     (s*(.27+j*.015), -.29, 1.20-j*.035)],
                   [.042, .065, .003], 'smith_hair')
        for j in range(3):
            f.sweep('wild_moustache', [(s*.032, -.398-j*.004, 1.489-j*.010),
                                       (s*.13, -.385-j*.01, 1.478-j*.006),
                                       (s*.23, -.323, 1.475-j*.015),
                                       (s*.265, -.282, 1.50-j*.023)],
                    [.020, .029, .021, .002],
                    'smith_hair_tip' if j == 0 else 'smith_hair_mid', 8, False, .55)
    finish('BEARD_WILD', 'head')


def clothing():
    f.loft('barrel_chested_workshirt', [(.64, .33, .225), (.78, .385, .26),
                                       (.96, .405, .265), (1.15, .43, .24),
                                       (1.27, .32, .18), (1.30, .16, .125)],
           'smith_shirt', 18, False)
    f.loft('strong_neck', [(1.22, .14, .13), (1.39, .16, .14)], 'smith_skin', 12)
    for s in (-1, 1):
        f.sweep('short_sleeve', [(s*.36, 0, 1.22), (s*.46, .005, 1.20),
                                (s*.53, .005, 1.10)], [.17, .18, .155], 'smith_shirt', 12, False)
        f.sweep('rolled_sleeve', [(s*.51, .005, 1.13), (s*.54, .005, 1.075)],
                [.17, .168], 'smith_linen', 12, False)
        f.line('sleeve_fold', [(s*.43, -.14, 1.20), (s*.50, -.16, 1.14),
                              (s*.56, -.15, 1.105)], .007, 'smith_pants')
    finish('TORSO_WORKSHIRT', 'spine')

    for s in (-1, 1):
        f.loft('stout_trouser_leg', [(.23, .145, .15), (.39, .17, .18),
                                    (.53, .18, .19), (.73, .19, .19)],
               'smith_pants', 12, False, (s*.215, 0))
        f.line('outer_trouser_seam', [(s*.365, .03, .28), (s*.39, .03, .45),
                                     (s*.39, .03, .64)], .006, 'smith_linen')
    finish('PANTS', 'legs')
    for side, s in (('L', -1), ('R', 1)):
        f.loft('broad_workboot', [(.025, .18, .27, -.055), (.085, .183, .27, -.055),
                                 (.16, .17, .235, -.04), (.23, .145, .175),
                                 (.34, .145, .16)], 'smith_boot', 14, False, (s*.23, 0))
        f.loft('thick_sole', [(.015, .185, .277, -.055), (.06, .185, .277, -.055)],
               'smith_sole', 14, False, (s*.23, 0))
        f.panel('boot_toe_guard', [(s*.23-.135, -.25, .12), (s*.23, -.315, .14),
                                  (s*.23+.135, -.25, .12), (s*.23+.12, -.19, .19),
                                  (s*.23-.12, -.19, .19)], 'smith_iron', .015)
        for z in (.22, .27):
            f.line('boot_lace', [(s*.23-.08, -.16, z), (s*.23+.08, -.17, z+.018)], .009, 'smith_leather_edge')
        finish('BOOT_'+side, 'shin.'+side)

    apron_rows = [(1.205, .225, -.205), (1.10, .275, -.27), (.95, .33, -.305),
                  (.78, .375, -.33), (.61, .37, -.335), (.44, .335, -.31)]
    vertices = []
    for j, (z, width, cy) in enumerate(apron_rows):
        for i in range(9):
            u = -1+i/4
            vertices.append((u*width, cy+.085*u*u+.007*math.cos(i*2+j), z))
    faces = [(j*9+i, j*9+i+1, (j+1)*9+i+1, (j+1)*9+i)
             for j in range(5) for i in range(8) if not (j == 4 and i in (3, 4))]
    apron = f.surface('split_leather_apron', vertices, faces, 'smith_leather', False)
    apron.modifiers.new('APRON_THICKNESS', 'SOLIDIFY').thickness = .018
    for s in (-1, 1):
        f.line('apron_edge', [(s*w*.96, cy+.075, z+.008) for z, w, cy in apron_rows],
               .008, 'smith_leather_edge')
        f.ribbon('neck_strap', [(s*.14, -.055, 1.29), (s*.20, -.16, 1.25),
                                (s*.20, -.23, 1.14)], [.042]*3, 'smith_leather')
        f.ribbon('back_cross_strap', [(s*.14, .105, 1.29), (s*.22, .22, 1.10),
                                      (-s*.28, .255, .81)], [.05]*3, 'smith_leather')
        f.oval('bib_rivet', (s*.20, -.235, 1.17), (.013, .009, .013), 'smith_brass', 8, 4)
    f.ribbon('apron_pocket', [(0, -.344, .76), (0, -.366, .60)], [.32, .30], 'smith_belt')
    for j in range(6):
        x = -.125+j*.05
        f.line('pocket_stitch', [(x, -.371, .62), (x+.012, -.371, .637)], .003, 'smith_leather_edge')
    f.panel('old_patch', [(-.27, -.28, .57), (-.16, -.32, .58),
                          (-.15, -.325, .48), (-.25, -.29, .47)], 'smith_belt', .005)
    for j in range(3):
        f.line('patch_stitch', [(-.26+j*.036, -.321, .575),
                               (-.25+j*.036, -.327, .554)], .004, 'smith_linen')
    finish('APRON_LEATHER', 'apron')

    f.loft('heavy_belt', [(.755, .397, .286), (.83, .40, .28)], 'smith_belt', 18, False)
    for x in (-.065, .065):
        f.line('buckle', [(x, -.365, .758), (x, -.364, .829)], .012, 'smith_brass')
    for z in (.758, .829):
        f.line('buckle', [(-.065, -.365, z), (.065, -.365, z)], .012, 'smith_brass')
    f.line('buckle_pin', [(-.055, -.379, .794), (.045, -.379, .794)], .008, 'smith_brass')
    f.ring('hammer_belt_loop', (-.405, .015, .745), .064, 'smith_iron_edge', .012, 'Y', 16)
    for s in (-1, 1):
        f.sweep('hanging_tongs', [(-.42+s*.025, .02, .76), (-.45+s*.035, .01, .56),
                                 (-.43+s*.045, -.01, .46)], [.012, .015, .01], 'smith_iron', 6)
    f.line('tongs_pivot', [(-.485, -.015, .57), (-.415, -.015, .57)], .014, 'smith_brass')
    finish('BELT_AND_TOOLS', 'hips')


def arms_and_hands():
    for side, s in (('L', -1), ('R', 1)):
        wrist_x = s*(.57 if s < 0 else .67)
        f.sweep('thick_bare_forearm', [(s*.52, .005, 1.105), (s*.595, -.005, .99),
                                      (wrist_x, -.045, .85), (wrist_x, -.06, .795)],
                [.146, .155, .13, .108], 'smith_skin', 14, False)
        f.line('forearm_crease', [(wrist_x-.065, -.15, .84), (wrist_x, -.17, .83),
                                 (wrist_x+.065, -.15, .84)], .005, 'smith_skin_shade')
        f.sweep('wrist_guard', [(wrist_x, -.047, .87), (wrist_x, -.06, .80)],
                [.133, .121], 'smith_belt' if s < 0 else 'smith_linen', 12, False)
        if s < 0:
            for z in (.815, .855):
                f.oval('wrist_rivet', (wrist_x, -.183, z), (.010, .008, .010), 'smith_brass', 8, 4)
        finish('ARM_'+side, 'arm_skin.'+side)
        palm_x = wrist_x+s*.015
        f.oval('large_calloused_palm', (palm_x, -.072, .725), (.125, .085, .125), 'smith_skin', 14, 6, False)
        for j in range(4):
            x = palm_x-.088+j*.055
            f.sweep('curled_finger', [(x, -.14, .77), (x, -.19, .72),
                                     (x, -.18, .655), (x, -.12, .655)],
                    [.031, .032, .026, .020], 'smith_skin', 8, False)
            f.line('knuckle_fold', [(x-.017, -.205, .711), (x+.017, -.205, .711)], .003, 'smith_skin_shade')
        f.sweep('large_thumb', [(palm_x-s*.10, -.075, .79), (palm_x-s*.13, -.13, .75),
                               (palm_x-s*.09, -.18, .708)], [.044, .044, .028], 'smith_skin', 10, False)
        finish('HAND_'+side, 'wrist.'+side)
    # The hammer hangs naturally from the right hand; it is a separately named prop.
    f.sweep('hammer_handle', [(.70, -.12, .37), (.70, -.12, .72), (.70, -.12, 1.00)],
            [.034, .031, .025], 'smith_wood', 10, False)
    hammer = g.cube((.70, -.12, .395), (.43, .23, .205), 'smith_iron')
    bevel = hammer.modifiers.new('FORGED_EDGE', 'BEVEL')
    bevel.width, bevel.segments = .026, 1
    g.cube((.70, -.12, .395), (.14, .245, .22), 'smith_iron_edge')
    for z in (.81, .855, .90):
        f.ring('hammer_grip_wrap', (.70, -.12, z), .031, 'smith_belt', .006, 'Z', 12)
    f.panel('hammer_makers_mark', [(.66, -.249, .42), (.70, -.251, .44),
                                  (.74, -.249, .42), (.70, -.251, .365)], 'smith_brass', .003)
    finish('FORGING_HAMMER', 'wrist.R')


def workstation():
    # A single named rigid part shares depth and scale with the moving hammer.
    for location, size, color in [
        ((.72, -.76, .27), (.36, .34, .54), 'smith_wood'),
        ((.72, -.76, .56), (.43, .32, .09), 'smith_iron'),
        ((.72, -.76, .65), (.22, .21, .15), 'smith_iron'),
        ((.72, -.76, .740), (.62, .28, .05), 'smith_iron_edge'),
    ]:
        g.cube(location, size, color)
    finish('WORK_ANVIL', 'root')


def make_rig():
    armature = bpy.data.armatures.new('NPC_SMITH_SKELETON')
    rig = bpy.data.objects.new('NPC_SMITH_RIG', armature)
    bpy.context.scene.collection.objects.link(rig)
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode='EDIT')
    def bone(name, head, tail, parent=None):
        b = armature.edit_bones.new(name)
        b.head, b.tail = head, tail
        if parent:
            b.parent = armature.edit_bones[parent]
    bone('root', (0, 0, 0), (0, 0, .15))
    bone('hips', (0, 0, .66), (0, 0, .84), 'root')
    bone('spine', (0, 0, .84), (0, 0, 1.28), 'hips')
    bone('head', (0, 0, 1.30), (0, 0, 1.85), 'spine')
    for side, s in (('L', -1), ('R', 1)):
        x = s*(.57 if s < 0 else .67)
        bone('arm.'+side, (s*.39, 0, 1.235), (s*.595, -.005, .99), 'spine')
        bone('hand.'+side, (s*.595, -.005, .99), (x, -.06, .795), 'arm.'+side)
        bone('wrist.'+side, (x, -.06, .795), (x+s*.015, -.072, .65), 'hand.'+side)
        bone('thigh.'+side, (s*.215, 0, .67), (s*.23, 0, .40), 'hips')
        bone('shin.'+side, (s*.23, 0, .40), (s*.23, -.05, .10), 'thigh.'+side)
    bpy.ops.object.mode_set(mode='OBJECT')
    rig.show_in_front = True
    for obj in [o for o in bpy.context.scene.objects if o.type == 'MESH']:
        rule = SKIN_RULES[obj.name]
        groups = {b.name: obj.vertex_groups.new(name=b.name) for b in armature.bones}
        for vertex in obj.data.vertices:
            x, y, z = vertex.co
            weights = {rule: 1}
            if rule == 'legs':
                side = 'L' if x < 0 else 'R'
                w = max(0, min(1, (.47-z)/.14))
                weights = {'thigh.'+side: 1-w, 'shin.'+side: w}
            elif rule.startswith('arm_skin.'):
                side = rule[-1]
                w = max(0, min(1, (1.045-z)/.12))
                weights = {'arm.'+side: 1-w, 'hand.'+side: w}
            elif rule == 'apron':
                w = max(0, min(1, (z-.77)/.25))
                weights = {'hips': 1-w, 'spine': w}
            elif rule == 'spine' and abs(x) > .375 and z > 1.0:
                side = 'L' if x < 0 else 'R'
                w = max(0, min(1, (abs(x)-.375)/.10))
                weights = {'spine': 1-w, 'arm.'+side: w}
            elif rule == 'spine' and abs(x) < .18 and z > 1.25:
                w = max(0, min(1, (z-1.25)/.14))
                weights = {'spine': 1-w, 'head': w}
            for name, weight in weights.items():
                if weight > 0:
                    groups[name].add([vertex.index], weight, 'REPLACE')
        modifier = obj.modifiers.new('SMITH_SKIN', 'ARMATURE')
        modifier.object = rig
        obj.parent = rig
    rig['stage'] = 'approved appearance; stationary work and interaction animations'
    return rig


def main():
    bpy.context.preferences.filepaths.save_version = 0
    g.start_collection('NPC_SMITH_BESPOKE')
    face()
    hair_and_beard()
    clothing()
    arms_and_hands()
    workstation()
    rig = make_rig()
    sys.path.insert(0, str(HERE))
    from author_stationary import author_clips
    clips = author_clips(rig)
    for key in ('smith_iron', 'smith_iron_edge', 'smith_brass'):
        shader = g.material(key).node_tree.nodes.get('Principled BSDF')
        shader.inputs['Metallic'].default_value = .70
        shader.inputs['Roughness'].default_value = .46
    f.stylize_scene()
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
    info = {'characterId': 'smith', 'stage': 'game_connection_01',
            'concept': 'boisterous dwarf, wild unbraided beard, bespoke anatomy and workwear',
            'proportions': 'approximately three heads; short legs, barrel chest, large hands',
            'source': 'model.blend', 'export': 'model.glb', 'rig': rig.name,
            'boneCount': len(rig.data.bones), 'meshes': PARTS,
            'gameIntegrated': True, 'gameExecutionVerified': False,
            'animationClipsAuthored': True,
            'stationary': True, 'locomotionClips': [], 'clips': clips,
            'workContact': {'anvilCenter': [.72, -.76], 'anvilTopZ': .765,
                            'impactFrames': [39, 71], 'workstationMesh': 'NPC_SMITH_WORK_ANVIL',
                            'workstationExported': True},
            'materialExport': 'Blend four-band lighting; GLB PBR base-color fallback'}
    (HERE/'character.json').write_text(json.dumps(info, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('BLACKSMITH_SCULPT_READY', len(PARTS), sum(p['triangles'] for p in PARTS), flush=True)


if __name__ == '__main__':
    main()
