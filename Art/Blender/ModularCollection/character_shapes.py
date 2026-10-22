"""Authored three-head silhouettes with stable modular attachment positions."""

import math

import create_collection as g
import surface_forms as f


def body(name, female, index):
    skin = 'skin_light' if female else 'skin'
    width = .305 if female else .325
    # The jaw, cheek and forehead have independent rings rather than a round primitive.
    f.loft('face', [(1.66, .10, .12, -.035), (1.72, .23, .205, -.025),
                   (1.86, width, .255, 0), (2.05, width, .27, .01),
                   (2.24, width*.88, .235, .025), (2.34, .14, .12, .035),
                   (2.37, .01, .01, .035)], skin, 20)
    f.loft('neck', [(1.50, .095, .09), (1.74, .105, .095)], skin, 12)
    f.loft('torso', [(1.00, .20, .14), (1.25, .23, .16),
                    (1.48, .26 if female else .28, .16), (1.56, .16, .13)], skin)
    for s in (-1, 1):
        x = s*.115
        f.oval('ear', (s*(width+.013), .005, 1.98), (.047, .055, .09), skin, 12, 5)
        # Narrow almond contour, visible iris and upper lid establish a face at game scale.
        f.oval('eye_white', (x, -.252, 2.015), (.069, .018, .036), 'eye_white', 12, 5)
        f.oval('iris', (x+s*.005, -.270, 2.014), (.028, .008, .031), 'eye', 12, 5)
        f.oval('eye_glint', (x-.008, -.278, 2.026), (.007, .003, .009), 'eye_white', 8, 4)
        f.line('eyelid', [(x-.068, -.256, 2.014), (x-.032, -.274, 2.045),
                         (x+.025, -.274, 2.045), (x+.069, -.255, 2.016)], .006, 'skin_shadow')
        f.line('brow', [(x-.061, -.254, 2.089), (x, -.268, 2.10),
                       (x+.059, -.252, 2.089)], .010 if female else .013, 'hair_dark')
        f.sweep('arm', [(s*.29, 0, 1.48), (s*.40, 0, 1.32),
                        (s*.43, -.015, 1.12), (s*.43, -.025, .99)],
                [.105, .087, .075, .060], skin, 12)
        f.oval('hand', (s*.43, -.025, .96), (.072, .055, .10), skin, 12, 6)
        f.oval('thumb', (s*.38, -.065, .98), (.035, .035, .06), skin, 10, 5)
        f.sweep('leg', [(s*.145, 0, 1.00), (s*.15, 0, .60),
                        (s*.15, -.005, .25)], [.105, .09, .07], skin, 12)
    f.surface('nose', [(-.025, -.256, 1.99), (.025, -.256, 1.99),
                       (0, -.310, 1.93), (-.023, -.269, 1.91), (.023, -.269, 1.91)],
              [(0, 1, 2), (0, 2, 3), (1, 4, 2), (3, 2, 4)], skin, True)
    f.line('mouth', [(-.047, -.239, 1.834), (0, -.255, 1.827),
                    (.047, -.239, 1.834)], .0055, 'skin_shadow')
    g.finish(name, 'body', index, g.PARTS)


def hair(name, kind, color, index):
    # A continuous skull cap with a lower back edge and an open forehead.
    n = 24
    vertices = []
    for j in range(6):
        t = j/5
        for i in range(n):
            a = math.tau*i/n
            edge = 1.98+.20*max(0, math.cos(a))
            theta = .08+(math.pi*.49-.08)*t
            z = 2.34+.085*math.cos(theta)-(2.34-edge)*t*t
            vertices.append((.342*math.sin(theta)*math.sin(a),
                             .025-.285*math.sin(theta)*math.cos(a), z))
    faces = [(j*n+i, j*n+(i+1)%n, (j+1)*n+(i+1)%n, (j+1)*n+i)
             for j in range(5) for i in range(n)]
    faces.append(tuple(reversed(range(n))))
    f.surface('hair_cap', vertices, faces, color, True)
    # Swept locks overlap as sheets. No round beads or cone spikes.
    for k, (x, tip_z) in enumerate([(-.25, 2.08), (-.13, 2.12),
                                     (.005, 2.16), (.14, 2.10), (.27, 2.07)]):
        shift = -.055 if kind in ('short', 'tousled') else .025
        f.lock('fringe', [(x*.65+shift, -.12, 2.37), (x*.90, -.245, 2.29),
                          (x, -.279, 2.20), (x-.035, -.271, tip_z)],
               [.07, .09, .075, .003], color)
    for s in (-1, 1):
        end = (1.90 if kind in ('short', 'tousled') else 1.72 if kind == 'bob'
               else 1.98 if kind == 'bun' else 1.82 if kind == 'braid' else 1.50)
        f.lock('temple', [(s*.25, -.025, 2.30), (s*.32, -.06, 2.13),
                         (s*.326, -.02, 1.96), (s*.29, .015, end)],
               [.09, .08, .066, .004], color)
    if kind in ('bob', 'long', 'braid', 'bun'):
        end = 1.75 if kind == 'bob' else 1.94 if kind == 'bun' else 1.82 if kind == 'braid' else 1.49
        for i in range(7):
            a = math.pi/2+math.pi*i/6
            x, y = .31*math.sin(a), .025-.26*math.cos(a)
            f.lock('back_lock', [(x*.80, y*.7, 2.28), (x, y, 2.04),
                                (x*1.07, y+.025, 1.83),
                                (x*.93, y+.06, end+.045*(i%2))],
                   [.08, .09, .08, .003], color)
    if kind == 'tousled':
        for s in (-1, 1):
            f.lock('crown_sweep', [(s*.05, .03, 2.37), (s*.16, -.03, 2.44),
                                  (s*.28, -.08, 2.35)], [.07, .065, .002], color)
    if kind == 'braid':
        for j in range(6):
            s = -1 if j%2 else 1
            f.sweep('woven_braid', [(s*.045, .285, 1.94-j*.07),
                                    (-s*.045, .31, 1.89-j*.07),
                                    (s*.035, .285, 1.84-j*.07)],
                    [.048, .05, .035], color, 8)
        f.ribbon('braid_tie', [(0, .33, 1.54), (0, .34, 1.44)], [.11, .04], 'red')
    if kind == 'bun':
        f.oval('bun_core', (0, .295, 2.16), (.15, .12, .14), color, 16, 6)
        for j in range(3):
            f.line('bun_strand', [(-.12, .31+j*.01, 2.11+j*.04),
                                  (0, .41, 2.11+j*.04), (.12, .31, 2.11+j*.04)], .010, color)
    g.finish(name, 'hair', index, g.PARTS)


def clothing(name, kind, base, trim, index):
    f.loft('fitted_coat', [(1.04, .245, .18), (1.20, .245, .195),
                          (1.42, .29, .19), (1.52, .29, .17),
                          (1.59, .13, .11)], base, 16, False, ripple=.012)
    for s in (-1, 1):
        f.sweep('sleeve', [(s*.29, 0, 1.49), (s*.37, 0, 1.41),
                           (s*.42, -.008, 1.21), (s*.43, -.015, 1.12)],
                [.105, .11, .085, .077], base, 10, False)
        f.loft('cuff', [(1.105, .084, .087), (1.15, .087, .09)], trim,
               12, True, (s*.43, -.015))
        f.loft('trousers', [(.27, .09, .105), (.52, .12, .14),
                           (.76, .12, .14), (1.03, .135, .145)],
               'cloth_dark' if kind != 'ritual' else base, 12, False, (s*.15, 0))
        f.loft('boot', [(.045, .12, .22, -.07), (.10, .12, .22, -.07),
                        (.19, .105, .17, -.035), (.29, .095, .12),
                        (.40, .105, .125)], 'leather', 12, False, (s*.15, 0))
        f.loft('sole', [(.025, .122, .222, -.07), (.066, .122, .222, -.07)],
               'shadow', 12, False, (s*.15, 0))
        for z in (.22, .28, .34):
            f.line('boot_lace', [(s*.15-.055, -.123, z), (s*.15+.055, -.13, z+.015)],
                   .006, 'wood_light')
    # Crossed collar and an inset seam keep the torso readable without round bulges.
    for s in (-1, 1):
        f.panel('collar', [(s*.09, -.115, 1.61), (s*.24, -.17, 1.51),
                          (s*.12, -.214, 1.35), (0, -.202, 1.51)], trim)
    f.line('front_seam', [(0, -.198, 1.38), (0, -.208, 1.20), (0, -.19, 1.05)], .005, trim)
    f.loft('belt', [(1.055, .255, .193), (1.12, .253, .199)], 'leather', 16)
    for x in (-.048, .048):
        f.line('buckle', [(x, -.210, 1.06), (x, -.213, 1.115)], .008, 'gold')
    for z in (1.06, 1.115):
        f.line('buckle', [(-.048, -.212, z), (.048, -.212, z)], .008, 'gold')
    if kind in ('tunic', 'traveller', 'ragged', 'archer'):
        f.loft('coat_skirt', [(.79, .32, .225), (.94, .285, .21),
                             (1.065, .25, .19)], base, 16, False, ripple=.025)
        f.line('hem', [(-.23, -.18, .805), (0, -.23, .79), (.23, -.18, .805)], .008, trim)
    if kind == 'apron':
        f.ribbon('apron', [(0, -.205, 1.48), (0, -.225, 1.28),
                          (0, -.255, .99), (0, -.27, .76)], [.27, .32, .39, .43], 'leather')
        for s in (-1, 1):
            f.line('apron_stitch', [(s*.128, -.218, 1.45), (s*.15, -.238, 1.22),
                                    (s*.20, -.28, .79)], .004, 'ochre')
        f.ribbon('apron_pocket', [(0, -.275, 1.03), (0, -.286, .90)], [.24, .23], trim)
    if kind == 'armor':
        f.loft('breastplate', [(1.13, .245, .205), (1.34, .30, .235),
                              (1.48, .285, .20), (1.56, .17, .145)], 'iron', 12, False)
        for s in (-1, 1):
            f.panel('pauldron', [(s*.25, -.18, 1.57), (s*.44, -.15, 1.54),
                                 (s*.49, .09, 1.45), (s*.29, .15, 1.55)], 'iron', .035)
            f.panel('tasset', [(s*.02, -.215, 1.06), (s*.25, -.18, 1.06),
                               (s*.30, -.19, .84), (s*.035, -.24, .87)], 'iron', .022)
            f.line('plate_edge', [(s*.03, -.238, 1.16), (s*.23, -.205, 1.21),
                                  (s*.29, -.185, 1.42)], .007, 'gold')
    if kind == 'archer':
        f.ribbon('crossbelt', [(-.22, -.20, 1.53), (0, -.235, 1.31),
                               (.22, -.22, 1.08)], [.07, .075, .07], 'leather')
        f.panel('shoulder_cape', [(-.08, .07, 1.58), (-.34, -.15, 1.51),
                                  (-.50, -.04, 1.23), (-.38, .25, 1.21),
                                  (-.10, .25, 1.45)], trim, .015)
    if kind == 'ritual':
        f.loft('ritual_robe', [(.42, .36, .25), (.70, .32, .23),
                              (1.07, .25, .20)], base, 20, False, ripple=.025)
        for s in (-1, 1):
            f.ribbon('stole', [(s*.10, -.15, 1.58), (s*.13, -.22, 1.28),
                               (s*.18, -.26, .63)], [.08, .075, .09], 'linen')
            f.line('stole_embroidery', [(s*.18, -.272, .69), (s*.18, -.272, .88)], .007, trim)
        f.charm((0, -.231, 1.36), .65)
    if kind == 'traveller':
        f.oval('pack', (0, .22, 1.30), (.24, .14, .29), 'leather', 12, 6)
        f.ribbon('scarf', [(0, -.17, 1.59), (0, -.215, 1.52),
                           (.20, -.23, 1.30)], [.30, .35, .10], trim)
    if kind == 'ragged':
        for s in (-1, 1):
            f.ribbon('patched_hem', [(s*.18, -.225, .95), (s*.21, -.245, .72)],
                     [.14, .09], 'cloth')
            for z in (.80, .86, .92):
                f.line('repair_stitch', [(s*.16, -.255, z), (s*.23, -.256, z+.015)], .004, 'linen')
    g.finish(name, 'outfit', index, g.PARTS)
