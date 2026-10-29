"""Create a bespoke elderly sealer, her fitted rig and stationary ritual animation."""

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
    'seal_skin': (.49, .31, .21, 1), 'seal_warm': (.50, .27, .18, 1),
    'seal_line': (.26, .13, .09, 1), 'seal_lip': (.29, .11, .10, 1),
    'seal_hair': (.42, .42, .38, 1), 'seal_hair_shade': (.28, .29, .27, 1),
    'seal_hair_light': (.58, .57, .51, 1), 'seal_eye': (.075, .055, .04, 1),
    'seal_iris': (.22, .19, .13, 1), 'seal_white': (.68, .63, .51, 1),
    'seal_robe': (.065, .105, .13, 1), 'seal_robe_light': (.12, .19, .20, 1),
    'seal_inner': (.28, .27, .23, 1), 'seal_trim': (.39, .39, .32, 1),
    'seal_red': (.32, .075, .055, 1), 'seal_cord': (.22, .065, .045, 1),
    'seal_wood': (.19, .105, .055, 1), 'seal_wood_light': (.32, .20, .105, 1),
    'seal_gold': (.40, .28, .13, 1), 'seal_ink': (.07, .075, .075, 1),
    'seal_paper': (.64, .56, .39, 1), 'seal_oldpaper': (.47, .40, .29, 1),
    'seal_boot': (.09, .065, .05, 1), 'seal_sole': (.025, .025, .023, 1),
}
g.COLORS.update(COLORS)
PARTS, RULES = [], {}


def finish(suffix, rule):
    obj = g.finish('NPC_SEALER_'+suffix, 'bespoke', len(PARTS), PARTS)
    obj['characterId'] = 'sealer'
    obj['design'] = 'calm elderly woman; low silver bun and ritual robes'
    RULES[obj.name] = rule
    return obj


def face():
    f.loft('elderly_tapered_face', [(1.63, .10, .115), (1.69, .19, .16),
                                    (1.81, .255, .205), (1.95, .282, .226),
                                    (2.09, .267, .212), (2.20, .19, .155),
                                    (2.28, .012, .012)], 'seal_skin', 20, True)
    for s in (-1, 1):
        f.oval('ear', (s*.28, .01, 1.94), (.046, .047, .079), 'seal_skin', 12, 6)
        f.oval('ear_fold', (s*.307, -.029, 1.94), (.016, .01, .036), 'seal_line', 10, 5)
        x = s*.112
        f.oval('quiet_eye', (x, -.214, 1.976), (.062, .012, .021), 'seal_white', 14, 6)
        f.oval('iris', (x-s*.004, -.226, 1.976), (.021, .006, .020), 'seal_iris', 12, 5)
        f.oval('pupil', (x-s*.004, -.232, 1.976), (.011, .003, .015), 'seal_eye', 10, 5)
        f.line('heavy_upper_lid', [(x-.062, -.207, 1.976), (x-.028, -.225, 1.991),
                                   (x+.027, -.226, 1.994), (x+.061, -.205, 1.980)],
               .006, 'seal_line')
        f.panel('lower_lid', [(x-.06, -.212, 1.962), (x, -.228, 1.966),
                              (x+.06, -.212, 1.962), (x+.05, -.211, 1.95),
                              (x-.05, -.211, 1.95)], 'seal_skin', .002)
        f.sweep('silver_brow', [(s*.04, -.21, 2.045), (s*.105, -.217, 2.055),
                                (s*.17, -.193, 2.049), (s*.215, -.155, 2.034)],
                [.006, .012, .009, .001], 'seal_hair_shade', 8, False, .65)
        for j in range(3):
            f.line('eye_wrinkle', [(s*.18, -.183, 1.97-j*.013),
                                   (s*(.215+j*.006), -.157, 1.97-j*.019)], .0028, 'seal_line')
        f.line('cheek_wrinkle', [(s*.15, -.19, 1.87), (s*.18, -.17, 1.83),
                                (s*.185, -.155, 1.80)], .003, 'seal_line')
        f.ring('worn_earring', (s*.304, -.008, 1.855), .017, 'seal_gold', .004, 'Y', 12)
    f.surface('long_nose_bridge', [(-.025, -.222, 2.015), (.025, -.222, 2.015),
                                   (-.035, -.268, 1.887), (.035, -.268, 1.887),
                                   (-.037, -.295, 1.87), (.037, -.295, 1.87)],
              [(0, 1, 3, 2), (2, 3, 5, 4), (0, 2, 4), (1, 5, 3)], 'seal_skin', True)
    f.oval('nose_tip', (0, -.272, 1.862), (.055, .035, .035), 'seal_warm', 12, 6)
    f.line('calm_mouth', [(-.08, -.196, 1.768), (-.04, -.211, 1.763),
                         (0, -.216, 1.761), (.04, -.211, 1.763), (.08, -.196, 1.768)],
           .0045, 'seal_lip', 8)
    for z, width in [(2.088, .09), (2.112, .07)]:
        f.line('forehead_line', [(-width, -.199, z), (0, -.215, z+.006),
                                 (width, -.199, z)], .0025, 'seal_line')
    finish('HEAD_FACE', 'head')


def hair():
    outline = [(1.69, .19, .16), (1.81, .255, .205), (1.95, .282, .226),
               (2.09, .267, .212), (2.20, .19, .155), (2.28, .012, .012)]
    def radius(z):
        if z >= 2.28:
            return .025, .025
        for lo, hi in zip(outline, outline[1:]):
            if lo[0] <= z <= hi[0]:
                t = (z-lo[0])/(hi[0]-lo[0])
                return tuple(lo[i]+(hi[i]-lo[i])*t+.030 for i in (1, 2))
        return .285, .235
    n, rings, points = 24, 12, []
    for j in range(rings):
        t = j/(rings-1)
        for i in range(n):
            a = math.tau*i/n
            front = max(0, math.cos(a))
            z = 2.302-(2.302-(1.86+.245*front))*t**1.8
            rx, ry = radius(z)
            points.append((rx*math.sin(a), -ry*math.cos(a), z))
    faces = [(j*n+i, j*n+(i+1)%n, (j+1)*n+(i+1)%n, (j+1)*n+i)
             for j in range(rings-1) for i in range(n)]
    faces.append(tuple(reversed(range(n))))
    f.surface('silver_hair_cap', points, faces, 'seal_hair')
    for s in (-1, 1):
        for j in range(3):
            f.lock('swept_silver_hair', [(s*.025, -.10+j*.06, 2.275-j*.01),
                                         (s*.15, -.17+j*.055, 2.19-j*.01),
                                         (s*.265, -.10+j*.045, 1.99-j*.025)],
                   [.027, .037, .002], 'seal_hair_light' if j != 1 else 'seal_hair_shade')
    f.oval('low_woven_bun', (0, .225, 1.98), (.145, .135, .13), 'seal_hair_shade', 14, 8, False)
    for j in range(4):
        a = math.pi*(j+.5)/4
        rx, ry, z = .145*math.sin(a), .135*math.sin(a), 1.98+.13*math.cos(a)
        f.line('bun_coil', [(-.9*rx, .225+.5*ry, z), (0, .23+ry, z),
                            (.9*rx, .225+.5*ry, z)], .006, 'seal_hair_light')
    f.line('wood_hairpin', [(-.17, .27, 1.99), (.17, .27, 2.02)], .009, 'seal_wood')
    f.oval('hairpin_end', (.17, .27, 2.02), (.022, .018, .022), 'seal_gold', 10, 5)
    finish('HAIR_LOW_BUN', 'head')


def clothes():
    f.loft('long_ritual_robe', [(.30, .33, .225), (.40, .37, .25), (.65, .36, .245),
                                (.88, .335, .23), (1.07, .29, .215), (1.30, .30, .215),
                                (1.47, .31, .19), (1.55, .14, .11)], 'seal_robe', 20, False)
    f.loft('elder_neck', [(1.49, .105, .095), (1.71, .11, .10)], 'seal_skin', 12, True)
    for s in (-1, 1):
        f.panel('layered_front_lapel', [(s*.035, -.12, 1.56), (s*.17, -.13, 1.49),
                                       (s*.27, -.155, 1.31), (s*.075, -.228, 1.19)],
                'seal_inner', .007)
        f.line('lapel_edge', [(s*.035, -.132, 1.55), (s*.08, -.20, 1.40),
                               (s*.16, -.215, 1.25), (s*.18, -.22, 1.10)], .010, 'seal_trim')
        f.sweep('loose_sleeve', [(s*.275, 0, 1.465), (s*.355, -.005, 1.39),
                                 (s*.395, -.018, 1.30)], [.12, .13, .115], 'seal_robe_light', 12, False)
        f.sweep('sleeve_cuff', [(s*.385, -.015, 1.33), (s*.405, -.02, 1.285)],
                [.12, .115], 'seal_trim', 12, False)
        for j in range(2):
            u = s*(.48+j*.18)
            f.line('robe_crease', [(rx*u, -dep*math.sqrt(1-u*u)-.004, z)
                                  for z, rx, dep in [(.95, .316, .225),
                                                    (.65, .36, .245), (.34, .346, .237)]],
                   .0035, 'seal_robe_light')
    finish('LAYERED_ROBE', 'dress')
    # Narrow shoulder cape falls on either side of the layered collar.
    for s in (-1, 1):
        f.panel('shoulder_cape', [(s*.09, -.10, 1.58), (s*.27, -.13, 1.52),
                                   (s*.35, -.105, 1.35), (s*.28, -.16, 1.30),
                                   (s*.18, -.19, 1.42)], 'seal_robe_light', .010)
        f.panel('cape_back', [(s*.09, .09, 1.58), (s*.27, .13, 1.52),
                              (s*.35, .10, 1.35), (s*.24, .16, 1.25),
                              (s*.11, .18, 1.40)], 'seal_robe_light', .010)
    finish('SHOULDER_MANTLE', 'spine')
    f.loft('ritual_sash', [(1.065, .304, .228), (1.12, .299, .227)], 'seal_cord', 20, False)
    f.line('sash_knot', [(-.06, -.24, 1.10), (0, -.255, 1.07), (.06, -.24, 1.10)], .014, 'seal_red')
    for j in range(3):
        x = -.11+j*.095
        f.panel('hanging_ward', [(x, -.252, 1.04), (x+.065, -.25, 1.04),
                                  (x+.062, -.263, .80), (x+.006, -.263, .79)], 'seal_oldpaper', .005)
        for k in range(3):
            f.line('ward_ink', [(x+.018, -.269, .98-k*.043),
                                (x+.049, -.269, .99-k*.043)], .003, 'seal_red')
    for i in range(9):
        a = i*math.tau/9
        f.oval('prayer_bead', (.30+.045*math.cos(a), -.08, .97+.045*math.sin(a)),
               (.012, .011, .012), 'seal_wood_light', 8, 4)
    finish('SASH_AND_TALISMANS', 'hips')
    for side, s in [('L', -1), ('R', 1)]:
        f.loft('soft_boot', [(.025, .09, .145, -.045), (.065, .105, .16, -.05),
                             (.15, .10, .145, -.04), (.245, .073, .09, 0)],
               'seal_boot', 12, False, center=(s*.17, 0))
        f.loft('boot_sole', [(.012, .106, .16, -.05), (.055, .108, .165, -.05)],
               'seal_sole', 12, False, center=(s*.17, 0))
        finish('SHOE_'+side, 'shin.'+side)


def arms():
    for side, s in [('L', -1), ('R', 1)]:
        f.sweep('forearm', [(s*.40, -.017, 1.30), (s*.445, -.032, 1.20),
                             (s*.445, -.095, 1.045), (s*.44, -.10, 1.01)],
                [.10, .09, .065, .06], 'seal_skin', 12, True)
        finish('ARM_'+side, 'arm_skin.'+side)
        x = s*.445
        f.oval('palm', (x, -.108, .958), (.07, .047, .073), 'seal_skin', 12, 6)
        for j in range(4):
            fx = x-.045+j*.03
            f.sweep('finger', [(fx, -.12, .94), (fx, -.14, .89),
                                (fx, -.13, .845), (fx, -.12, .83)],
                    [.016, .016, .013, .009], 'seal_skin', 8, True)
        f.sweep('thumb', [(x-s*.05, -.11, .98), (x-s*.08, -.13, .94),
                           (x-s*.09, -.13, .90)], [.023, .024, .016], 'seal_skin', 8)
        finish('HAND_'+side, 'wrist.'+side)
    f.sweep('worn_brush_handle', [(.445, -.15, .78), (.445, -.15, .95),
                                 (.445, -.15, 1.075)], [.015, .016, .013], 'seal_wood', 8)
    f.sweep('brush_tip', [(.445, -.15, .78), (.445, -.15, .74)], [.018, .002], 'seal_ink', 8)
    f.ring('brush_binding', (.445, -.15, .83), .017, 'seal_gold', .004, 'Z', 12)
    finish('WRITING_BRUSH', 'wrist.R')


def ritual_desk():
    g.cube((0, -.58, 1.055), (.88, .38, .09), 'seal_wood')
    g.cube((0, -.58, 1.108), (.90, .40, .025), 'seal_wood_light')
    for s in (-1, 1):
        for y in (-.72, -.44):
            g.cube((s*.36, y, .53), (.05, .05, 1.06), 'seal_wood')
    finish('RITUAL_DESK', 'root')
    f.panel('unrolled_talisman', [(-.09, -.68, 1.124), (.33, -.68, 1.124),
                                   (.33, -.47, 1.124), (-.09, -.47, 1.124)], 'seal_paper', .003)
    for i in range(4):
        f.line('seal_stroke', [(.04+i*.055, -.65, 1.13), (.055+i*.055, -.53, 1.13)],
               .004, 'seal_red')
    f.ring('inkstone', (-.29, -.60, 1.135), .064, 'seal_ink', .013, 'Z', 14)
    g.cube((-.30, -.60, 1.126), (.10, .09, .018), 'seal_ink')
    for s in (-1, 1):
        f.sweep('scroll_roller', [(s*.34, -.70, 1.14), (s*.34, -.46, 1.14)],
                [.014, .014], 'seal_wood', 8)
    finish('RITUAL_PAPER_AND_TOOLS', 'root')


def main():
    bpy.context.preferences.filepaths.save_version = 0
    g.start_collection('NPC_SEALER_BESPOKE')
    face()
    hair()
    clothes()
    arms()
    ritual_desk()
    # Reuse only the rigging algorithm, not any shopkeeper geometry or mesh.
    spec = importlib.util.spec_from_file_location('shop_rig_helper', HERE.parent/'Merchant'/'create_merchant.py')
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    helper.RULES = RULES
    rig = helper.rig_character()
    rig.name, rig.data.name = 'NPC_SEALER_RIG', 'NPC_SEALER_SKELETON'
    sys.path.insert(0, str(HERE))
    from author_stationary import author_clips
    clips = author_clips(rig)
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
    info = {'characterId': 'sealer', 'stage': 'bespoke_sealer_01',
            'concept': 'calm elderly woman; low silver bun, layered robes and old sealing tools',
            'source': 'model.blend', 'export': 'model.glb', 'rig': rig.name,
            'boneCount': len(rig.data.bones), 'meshes': PARTS, 'clips': clips,
            'gameIntegrated': True, 'gameExecutionVerified': False, 'stationary': True,
            'locomotionClips': [], 'ritualPaperTopZ': 1.124}
    (HERE/'character.json').write_text(json.dumps(info, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('SEALER_READY', len(PARTS), sum(p['triangles'] for p in PARTS), flush=True)


if __name__ == '__main__':
    main()
