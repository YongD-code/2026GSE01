"""Three authored spirit silhouettes sharing the established ritual/fabric language."""

import math

import create_collection as g
import surface_forms as f


def tattered_mantle(name, z, length, radius, material, bind, bone, count=24):
    vertices = []
    for j in range(6):
        t = j/5
        for i in range(count):
            a = math.tau*i/count
            r = radius*(1+.24*t)+.025*math.cos(a*8)*(0.3+t)
            hem = (.09*math.sin(i*2.7)+.05*(i%3))*t**4
            vertices.append((r*math.sin(a), -r*.65*math.cos(a)+.08*t,
                             z-length*t+hem))
    faces = [(j*count+i, j*count+(i+1)%count,
              (j+1)*count+(i+1)%count, (j+1)*count+i)
             for j in range(5) for i in range(count)
             if not (j == 4 and i%7 == 0)]
    obj = f.surface(name, vertices, faces, material, True)
    mod = obj.modifiers.new('RAGGED_FABRIC', 'SOLIDIFY')
    mod.thickness = .012
    for i in range(0, count, 3):
        f.line('mantle_seam', [vertices[j*count+i] for j in range(6)], .005, 'eater_edge')
    bind(name, bone)


def eater(bind):
    # Narrow waist, rounded shoulders, very long forearms; a deliberate hunched silhouette.
    f.loft('hollow_torso', [(1.02, .23, .17), (1.25, .28, .20),
                           (1.65, .36, .24), (1.89, .40, .23),
                           (2.00, .19, .17)], 'eater_cloth', 20, True, ripple=.025)
    for s in (-1, 1):
        f.panel('crossed_lapel', [(s*.16, -.12, 2.00), (s*.35, -.20, 1.86),
                                 (-s*.10, -.265, 1.46), (-s*.19, -.24, 1.53)], 'eater_edge')
        f.line('collar_embroidery', [(s*.16, -.135, 1.97), (s*.29, -.214, 1.85),
                                    (-s*.10, -.28, 1.48)], .006, 'gold')
    f.loft('rope_waist', [(1.12, .275, .21), (1.16, .27, .21)], 'leather', 20)
    for s in (-1, 1):
        f.line('rope_dangle', [(s*.12, -.235, 1.15), (s*.18, -.27, 1.04),
                               (s*.13, -.28, .91)], .012, 'gold')
        f.bell((s*.13, -.28, .88), .65)
    f.charm((0, -.273, 1.50), .9)
    bind('TORSO', 'body')
    tattered_mantle('LAYERED_SKIRT', 1.20, .56, .285, 'eater_cloth', bind, 'body')

    # Recessed ceramic face under a fabric hood, with a long open crying mouth.
    f.oval('hood_inner', (0, .015, 2.26), (.36, .285, .41), 'shadow', 24, 12)
    f.loft('hood', [(1.98, .26, .19, .10), (2.13, .38, .27, .07),
                    (2.42, .34, .27, .08), (2.61, .19, .17, .07),
                    (2.66, .02, .02, .07)], 'eater_cloth', 24)
    # Mask perimeter is angular at jaw and temples, with a raised center ridge.
    contour = [(-.20, 2.51), (-.29, 2.37), (-.25, 2.14), (-.13, 1.99),
               (0, 1.94), (.13, 1.99), (.25, 2.14), (.29, 2.37), (.20, 2.51), (0, 2.55)]
    vertices = [(0, -.382, 2.29)] + [(x, -.30+.20*x*x, z) for x, z in contour]
    faces = [(0, i+1, (i+1)%len(contour)+1) for i in range(len(contour))]
    face = f.surface('ceramic_mask', vertices, faces, 'eater_bone', True)
    solid = face.modifiers.new('CERAMIC_THICKNESS', 'SOLIDIFY')
    solid.thickness = .025
    for s in (-1, 1):
        f.panel('empty_eye', [(s*.055, -.381, 2.37), (s*.205, -.349, 2.41),
                              (s*.18, -.365, 2.29), (s*.10, -.382, 2.275)], 'shadow', .004)
        f.oval('eye_core', (s*.125, -.390, 2.305), (.012, .006, .024), 'tear_blue', 12, 6)
        f.line('running_tear', [(s*.13, -.382, 2.29), (s*.15, -.365, 2.19),
                                (s*.12, -.341, 2.08), (s*.10, -.32, 2.04)], .012, 'tear_blue')
        f.line('mask_crack', [(s*.19, -.323, 2.47), (s*.16, -.349, 2.41),
                              (s*.21, -.321, 2.38)], .004, 'leather')
        f.sweep('curved_horn', [(s*.24, .015, 2.48), (s*.36, .045, 2.66),
                               (s*.35, .04, 2.82), (s*.28, -.01, 2.87)],
                [.07, .055, .031, .002], 'eater_bone', 10)
        f.line('hood_welt', [(s*.19, -.11, 2.58), (s*.32, -.18, 2.43),
                             (s*.34, -.20, 2.16), (s*.26, -.18, 1.98)], .011, 'eater_edge')
    f.oval('crying_mouth', (0, -.364, 2.10), (.064, .015, .105), 'shadow', 20, 10)
    f.line('brow_crack', [(-.07, -.348, 2.51), (-.025, -.374, 2.44),
                         (.015, -.374, 2.45), (.05, -.371, 2.39)], .0045, 'leather')
    bind('MASK_AND_HOOD', 'head')

    for side, s in (('L', -1), ('R', 1)):
        f.sweep('hanging_sleeve', [(s*.36, 0, 1.91), (s*.49, -.01, 1.75),
                                  (s*.56, -.07, 1.47), (s*.58, -.09, 1.35)],
                [.16, .17, .12, .11], 'eater_cloth', 12, False)
        for j in range(4):
            x = s*(.28+j*.065)
            f.ribbon('shoulder_rag', [(x, .0, 1.96-j*.02), (x+s*.08, -.02, 1.75),
                                      (x+s*.06, -.06, 1.50+.05*(j%2))],
                     [.13, .14, .003], 'eater_edge' if j%2 else 'eater_cloth')
        for i in range(3):
            x = s*(.49+i*.035)
            f.line('sleeve_fold', [(x, -.16, 1.78), (x+s*.03, -.19, 1.57),
                                   (x+s*.02, -.18, 1.39)], .005, 'eater_edge')
        bind('SLEEVE_'+side, 'arm.'+side)
        f.sweep('gaunt_forearm', [(s*.58, -.08, 1.39), (s*.65, -.12, 1.17),
                                 (s*.72, -.20, 1.02)], [.075, .065, .06], 'eater_skin', 12)
        f.oval('claw_palm', (s*.73, -.21, 1.01), (.085, .06, .13), 'eater_skin', 14, 7)
        for i in range(4):
            x = s*(.65+i*.048)
            end = .80+.02*abs(i-1)
            f.sweep('crooked_finger', [(x, -.25, 1.02), (x+s*.018, -.31, .93),
                                      (x+s*.035, -.36, end), (x+s*.02, -.42, end+.04)],
                    [.020, .018, .012, .001], 'eater_bone', 8)
        for j in range(3):
            f.line('wrist_binding', [(s*.63-.06, -.18, 1.26-j*.045),
                                     (s*.63, -.21, 1.25-j*.045),
                                     (s*.63+.06, -.18, 1.26-j*.045)], .014, 'eater_edge')
        bind('CLAW_'+side, 'hand.'+side)
        f.sweep('thigh', [(s*.19, 0, 1.04), (s*.20, .02, .78), (s*.22, -.02, .55)],
                [.115, .10, .08], 'eater_cloth', 12)
        bind('THIGH_'+side, 'leg.'+side)
        f.sweep('shin', [(s*.22, -.02, .56), (s*.23, .02, .32),
                         (s*.22, -.02, .14)], [.075, .057, .06], 'eater_skin', 12)
        f.loft('foot', [(.035, .085, .19, -.10), (.09, .10, .20, -.10),
                        (.17, .07, .11, -.04)], 'eater_skin', 12, True, (s*.22, 0))
        for j in range(4):
            f.line('ankle_wrap', [(s*.22-.06, -.04, .23+j*.045),
                                  (s*.22, -.072, .225+j*.045),
                                  (s*.22+.06, -.04, .23+j*.045)], .012, 'eater_edge')
        bind('SHIN_'+side, 'foot.'+side)
    # Two independently moving, torn tails of the mourning cloak.
    for s in (-1, 1):
        f.ribbon('cloak_streamer', [(s*.17, .23, 1.92), (s*.39, .31, 1.46),
                                    (s*.55, .38, .98), (s*.65, .41, .35)],
                 [.28, .37, .33, .015], 'eater_cloth')
        f.line('cloak_hem', [(s*.29, .23, 1.86), (s*.57, .31, 1.46),
                             (s*.71, .38, 1.00), (s*.65, .41, .36)], .006, 'eater_edge')
        for j in range(3):
            x = s*(.36+j*.065)
            f.ribbon('hanging_prayer_strip', [(x, .25, 1.45-j*.06),
                                             (x+s*.07, .31, 1.10-j*.07),
                                             (x+s*.11, .35, .88-j*.09)],
                     [.055, .06, .035], 'paper' if j == 1 else 'eater_edge')
    bind('TORN_CLOAK', 'cloak')


def animal_body(gentle, bind):
    hide = 'stag_fur' if gentle else 'hound_hide'
    # The spine profile, rib cage and tucked abdomen are distinct from the head/neck.
    f.sweep('rib_cage', [(0, .75, 1.18), (0, .46, 1.21), (0, .02, 1.25),
                         (0, -.43, 1.29), (0, -.64, 1.26)],
            [.28, .34, .37, .36, .27] if gentle else [.24, .22, .32, .41, .31],
            hide, 18, True, flatten=1.13)
    f.sweep('rising_neck', [(0, -.50, 1.23), (0, -.69, 1.39),
                           (0, -.82, 1.62), (0, -.93, 1.68)],
            [.26, .24, .18, .15] if gentle else [.32, .30, .24, .17], hide, 16)
    if gentle:
        for i in range(7):
            a = math.tau*i/7
            x, y = .27*math.sin(a), -.60+.18*math.cos(a)
            f.lock('neck_leaf', [(x*.65, y, 1.57), (x, y-.05, 1.34),
                                 (x*.9, y-.04, 1.10)], [.045, .07, .001], 'stag_light')
        f.ribbon('saddle_cloth', [(0, -.13, 1.62), (0, .27, 1.59),
                                 (0, .63, 1.45)], [.57, .60, .44], 'stag_cloth')
        for s in (-1, 1):
            f.panel('saddle_fall', [(s*.25, -.20, 1.54), (s*.28, .40, 1.50),
                                    (s*.40, .41, 1.07), (s*.37, -.21, 1.10)], 'stag_cloth')
            f.line('saddle_border', [(s*.38, -.18, 1.12), (s*.41, .16, 1.08),
                                     (s*.40, .38, 1.09)], .010, 'gold')
            f.line('saddle_sigils', [(s*.405, .03, 1.33), (s*.415, .12, 1.41),
                                     (s*.415, .22, 1.31), (s*.405, .12, 1.22),
                                     (s*.405, .03, 1.33)], .008, 'stag_glow')
        f.line('neck_rope', [(-.24, -.65, 1.46), (0, -.84, 1.22), (.24, -.65, 1.46)], .015, 'gold')
        f.charm((0, -.87, 1.16), 1.1)
        f.ribbon('votive_strip', [(0, -.87, 1.10), (0, -.89, .93),
                                  (.03, -.87, .80)], [.12, .13, .09], 'paper')
        for j in range(3):
            f.line('votive_ink', [(-.025, -.905, 1.04-j*.055), (.025, -.905, 1.04-j*.055)], .005, 'red')
    else:
        for j in range(7):
            y = -.67+j*.16
            for s in (-1, 1):
                f.lock('ash_mane', [(s*.12, y, 1.56), (s*.27, y+.10, 1.65),
                                    (s*.41, y+.20, 1.44)], [.07, .10, .001], 'hound_ash')
        for s in (-1, 1):
            for j in range(3):
                y = -.28+j*.24
                f.line('rib_ember', [(s*.33, y, 1.38), (s*.36, y+.05, 1.25),
                                     (s*.29, y+.10, 1.15)], .007, 'hound_red')
                f.line('ash_rib', [(s*.34, y-.04, 1.43), (s*.39, y+.025, 1.29),
                                   (s*.32, y+.08, 1.13)], .018, 'hound_ash')
    bind('BODY', 'body')


def animal_head(gentle, bind):
    hide = 'stag_fur' if gentle else 'hound_hide'
    light = 'stag_light' if gentle else 'hound_ash'
    f.sweep('skull', [(0, -.91, 1.65), (0, -1.08, 1.58),
                      (0, -1.30, 1.48), (0, -1.48, 1.43)],
            [.20, .23, .15, .09] if gentle else [.24, .27, .17, .12],
            hide, 16, True, flatten=.85)
    for s in (-1, 1):
        f.oval('eye_socket', (s*.172, -1.19, 1.62), (.060, .044, .042), 'shadow', 16, 6)
        f.oval('eye', (s*.179, -1.22, 1.625), (.034, .025, .020),
               'stag_glow' if gentle else 'hound_red', 12, 6)
        f.line('eye_ridge', [(s*.11, -1.24, 1.68), (s*.19, -1.20, 1.68),
                             (s*.24, -1.09, 1.63)], .014, light)
    if gentle:
        f.oval('muzzle', (0, -1.47, 1.41), (.11, .11, .08), light, 14, 6)
        f.oval('nose', (0, -1.565, 1.435), (.055, .025, .035), 'shadow', 12, 6)
        f.line('forehead_mark', [(0, -1.19, 1.78), (-.04, -1.24, 1.72),
                                 (0, -1.29, 1.67), (.04, -1.24, 1.72),
                                 (0, -1.19, 1.78)], .008, 'stag_glow')
        for s in (-1, 1):
            points = [(s*.13, -.98, 1.79), (s*.24, -.87, 2.02),
                      (s*.32, -.76, 2.27), (s*.37, -.69, 2.52),
                      (s*.31, -.68, 2.65)]
            f.sweep('branch_antler', points, [.055, .048, .035, .022, .001], 'stag_horn', 10)
            for j, (base, tip) in enumerate([
                    ((s*.22, -.88, 1.99), (s*.45, -1.04, 2.21)),
                    ((s*.31, -.77, 2.25), (s*.60, -.79, 2.40)),
                    ((s*.37, -.70, 2.49), (s*.55, -.62, 2.62))]):
                middle = tuple((a+b)*.5 for a, b in zip(base, tip))
                f.sweep('antler_tine', [base, middle, tip], [.027, .018, .001], 'stag_horn', 8)
            f.line('antler_thread', [(s*.31, -.78, 2.26), (s*.40, -.84, 2.12),
                                     (s*.49, -.82, 2.32)], .008, 'gold')
            f.bell((s*.39, -.84, 2.07), .55)
    else:
        f.sweep('lower_jaw', [(0, -1.08, 1.38), (0, -1.29, 1.28),
                              (0, -1.51, 1.29)], [.13, .105, .08], hide, 12, flatten=.55)
        f.oval('mouth_void', (0, -1.37, 1.35), (.15, .23, .045), 'shadow', 16, 6)
        f.oval('nose', (0, -1.57, 1.45), (.11, .06, .06), 'shadow', 12, 6)
        for s in (-1, 1):
            for j in range(4):
                y = -1.26-j*.075
                x = s*(.14-j*.018)
                f.sweep('fang', [(x, y, 1.415), (x, y-.015, 1.37),
                                 (x*.85, y-.01, 1.32)], [.019, .013, .001], 'hound_teeth', 8)
            f.line('face_fissure', [(s*.04, -1.26, 1.74), (s*.09, -1.31, 1.63),
                                    (s*.055, -1.39, 1.56), (s*.09, -1.46, 1.49)], .007, 'hound_red')
            f.lock('cheek_tuft', [(s*.20, -1.03, 1.64), (s*.30, -.88, 1.55),
                                  (s*.32, -.65, 1.40)], [.08, .11, .001], light)
        f.panel('broken_forehead_plate', [(-.11, -1.22, 1.78), (-.25, -1.12, 1.65),
                                           (-.20, -1.31, 1.52), (-.08, -1.36, 1.60),
                                           (-.04, -1.29, 1.72)], 'hound_teeth', .018)
    bind('HEAD', 'head')
    for side, s in (('L', -1), ('R', 1)):
        f.lock('ear', [(s*.17, -.96, 1.74), (s*.31, -.93, 1.92),
                       (s*.40, -.91, 2.04 if gentle else 2.02)], [.065, .105, .001], hide)
        f.lock('inner_ear', [(s*.22, -1.00, 1.80), (s*.31, -.975, 1.93),
                             (s*.38, -.951, 2.01)], [.028, .055, .001], light)
        bind('EAR_'+side, 'ear.'+side)


def animal_legs(gentle, bind):
    hide = 'stag_fur' if gentle else 'hound_hide'
    light = 'stag_light' if gentle else 'hound_ash'
    for side, s in (('L', -1), ('R', 1)):
        for which, y in (('front', -.62), ('back', .58)):
            back = which == 'back'
            hip_y = y+.10 if back else y
            knee_y = y-.13 if back else y+.025
            f.sweep('upper_leg', [(s*.30, hip_y, 1.20), (s*.37, y, .93),
                                  (s*.39, knee_y, .55)],
                    [.13, .10, .055] if gentle else [.16, .12, .065], hide, 12)
            bind(which.upper()+'_LEG_'+side, which+'.'+side)
            f.sweep('lower_leg', [(s*.39, knee_y, .55), (s*.39, y+.035, .34),
                                  (s*.39, y-.04, .12)], [.057, .042, .048], light, 12)
            if gentle:
                for toe in (-1, 1):
                    f.loft('cloven_hoof', [(.035, .034, .105, y-.075),
                                          (.10, .038, .10, y-.07),
                                          (.19, .030, .055, y-.035)],
                           'stag_horn', 10, True, (s*.39+toe*.037, 0))
                f.line('anklet', [(s*.39-.052, y-.055, .25), (s*.39, y-.08, .24),
                                  (s*.39+.052, y-.055, .25)], .012, 'gold')
            else:
                f.oval('paw', (s*.39, y-.11, .09), (.105, .16, .07), hide, 12, 6)
                for toe in (-1, 0, 1):
                    f.sweep('toe_claw', [(s*.39+toe*.052, y-.20, .10),
                                        (s*.39+toe*.052, y-.27, .055)],
                            [.021, .001], 'hound_teeth', 8)
            bind(which.upper()+'_PAW_'+side, 'paw.'+which+'.'+side)


def quadruped(species, bind):
    gentle = species == 'stag'
    animal_body(gentle, bind)
    animal_head(gentle, bind)
    animal_legs(gentle, bind)
    if gentle:
        f.sweep('deer_tail', [(0, .73, 1.29), (0, .96, 1.40), (0, 1.14, 1.36)],
                [.10, .10, .003], 'stag_light', 12)
    else:
        f.sweep('whiplash_tail', [(0, .71, 1.30), (0, .98, 1.44),
                                 (.06, 1.22, 1.63), (.13, 1.50, 1.58),
                                 (.17, 1.67, 1.40)], [.09, .085, .07, .045, .001], 'hound_hide', 12)
        for j in range(3):
            f.lock('tail_cinder', [(0, 1.08+j*.10, 1.53), (.06, 1.31+j*.10, 1.68),
                                   (.17, 1.61+j*.10, 1.52)], [.04, .07, .001], 'hound_ash')
    bind('TAIL', 'tail')
