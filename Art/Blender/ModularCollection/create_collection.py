"""Build shared low-poly NPC and scenery meshes without touching game runtime files.

Run with Blender 5.2:
  blender --background --factory-startup --python-exit-code 1 --python Art/Blender/ModularCollection/create_collection.py
"""

import json
import math
from pathlib import Path

import bpy


OUT = Path(__file__).resolve().parent
PARTS = []
SCENERY = []
CREATED = []

COLORS = {
    "skin": (.67, .47, .35, 1), "skin_light": (.78, .58, .44, 1),
    "hair_dark": (.13, .09, .07, 1), "hair_brown": (.30, .18, .10, 1),
    "hair_ash": (.55, .54, .50, 1), "hair_silver": (.77, .80, .78, 1),
    "eye": (.075, .065, .060, 1), "linen": (.68, .62, .48, 1),
    "cloth": (.30, .32, .29, 1), "cloth_dark": (.16, .20, .22, 1),
    "blue": (.22, .29, .35, 1), "red": (.39, .13, .13, 1),
    "purple": (.29, .20, .32, 1), "ochre": (.55, .35, .15, 1),
    "leather": (.31, .20, .12, 1), "wood": (.30, .20, .13, 1),
    "wood_light": (.46, .31, .19, 1), "roof": (.25, .23, .25, 1),
    "roof_red": (.40, .22, .19, 1), "stone": (.39, .40, .39, 1),
    "stone_light": (.56, .54, .48, 1), "iron": (.35, .36, .36, 1),
    "gold": (.64, .48, .23, 1), "paper": (.82, .75, .58, 1),
    "leaf": (.24, .34, .24, 1), "leaf_light": (.37, .45, .29, 1),
    "leaf_dead": (.42, .35, .24, 1), "grass": (.33, .43, .24, 1),
    "shadow": (.11, .11, .12, 1),
}


def material(key):
    name = "MAT_" + key.upper()
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = COLORS[key]
    mat.use_nodes = True
    node = mat.node_tree.nodes.get("Principled BSDF")
    node.inputs["Base Color"].default_value = COLORS[key]
    node.inputs["Roughness"].default_value = .82
    return mat


def record(obj, mat):
    obj.data.materials.append(material(mat))
    for face in obj.data.polygons:
        face.use_smooth = False
    CREATED.append(obj)
    return obj


def cube(pos, scale, mat, rot=0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=pos)
    obj = bpy.context.object
    obj.dimensions = scale
    obj.rotation_euler[2] = rot
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return record(obj, mat)


def ico(pos, scale, mat, subdivisions=1):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=subdivisions, radius=1, location=pos)
    obj = bpy.context.object
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return record(obj, mat)


def cone(pos, radius1, radius2, depth, mat, vertices=8, rotation=None):
    bpy.ops.mesh.primitive_cone_add(vertices=vertices, radius1=radius1,
                                    radius2=radius2, depth=depth, location=pos)
    obj = bpy.context.object
    if rotation:
        obj.rotation_euler = rotation
    return record(obj, mat)


def cylinder(pos, radius, depth, mat, vertices=8, rotation=None):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius,
                                        depth=depth, location=pos)
    obj = bpy.context.object
    if rotation:
        obj.rotation_euler = rotation
    return record(obj, mat)


def mesh(name, verts, faces, mat):
    data = bpy.data.meshes.new(name)
    data.from_pydata(verts, [], faces)
    data.update()
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    return record(obj, mat)


def beam(a, b, width, mat, sides=6):
    from mathutils import Vector
    start, end = Vector(a), Vector(b)
    obj = cylinder((start + end) / 2, width, (end - start).length, mat, sides)
    obj.rotation_euler = (end - start).to_track_quat('Z', 'Y').to_euler()
    return obj


def finish(name, slot, index, target):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in CREATED:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = CREATED[0]
    bpy.ops.object.convert(target='MESH')
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = name
    obj.data.name = name
    bpy.context.scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    obj["slot"] = slot
    obj["index"] = index
    target.append({"index": index, "mesh": name, "slot": slot,
                   "vertices": len(obj.data.vertices), "triangles": sum(len(p.vertices) - 2 for p in obj.data.polygons)})
    CREATED.clear()
    return obj


def start_collection(name):
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    col = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(col)
    bpy.context.view_layer.active_layer_collection = bpy.context.view_layer.layer_collection.children[col.name]


def body(name, female, index):
    skin = "skin_light" if female else "skin"
    # Three-head proportion: 2.4 m total, with a 0.78 m head.
    ico((0, 0, 1.96), (.33 if female else .35, .29, .39), skin, 2)
    ico((0, 0, 1.42), (.23 if female else .27, .17, .31), skin)
    cylinder((0, 0, 1.62), .10, .15, skin)
    for sign in (-1, 1):
        ico((sign * .43, 0, 1.28), (.10, .10, .31), skin)
        ico((sign * .43, -.015, .96), (.095, .09, .11), skin)
        ico((sign * .15, 0, .55), (.11, .12, .43), skin)
        ico((sign * .15, -.07, .15), (.13, .19, .12), "leather")
        ico((sign * .115, -.279, 2.00), (.034, .019, .026), "eye")
        ico((sign * .32, 0, 1.96), (.07, .07, .11), skin)
    ico((0, -.293, 1.89), (.045, .035, .035), skin)
    finish(name, "body", index, PARTS)


def hair(name, kind, color, index):
    # Open-front cap preserves the large, readable face.
    ico((0, .045, 2.245), (.35, .31, .20), color, 1)
    for x, z in ((-.23, 2.24), (-.09, 2.31), (.08, 2.29), (.23, 2.22)):
        ico((x, -.235, z), (.115, .095, .10), color)
    if kind in ("short", "tousled"):
        for x, y, z in ((-.27, .0, 2.34), (.02, .1, 2.39), (.24, .1, 2.32)):
            cone((x, y, z), .085, 0, .22, color, 5)
    if kind in ("bob", "long", "braid", "bun"):
        for s in (-1, 1):
            ico((s * .30, .025, 2.00), (.10, .17, .26 if kind == "bob" else .38), color)
    if kind in ("long", "braid"):
        ico((0, .20, 1.74), (.24, .115, .42), color)
    if kind == "braid":
        for z, r in ((1.85, .105), (1.70, .09), (1.55, .07)):
            ico((.19, .30, z), (r, r, .105), color)
        cone((.19, .30, 1.46), .065, 0, .18, color, 5)
    if kind == "bun":
        ico((0, .30, 2.15), (.18, .16, .17), color)
    finish(name, "hair", index, PARTS)


def clothing(name, kind, base, trim, index):
    # Garments use the same attachment coordinates on both body meshes.
    ico((0, 0, 1.34), (.32, .235, .39), base, 1)
    for s in (-1, 1):
        ico((s * .42, 0, 1.34), (.145, .14, .27), base)
        ico((s * .15, 0, .60), (.15, .15, .36), base)
        ico((s * .15, -.065, .20), (.16, .20, .16), "leather")
    cylinder((0, 0, 1.00), .30, .12, trim)
    if kind in ("tunic", "traveller", "ragged"):
        cone((0, 0, .98), .37, .25, .43, base)
    if kind == "apron":
        cube((0, -.248, 1.08), (.40, .035, .83), "leather")
        cube((0, -.27, 1.43), (.30, .035, .11), trim)
    if kind == "armor":
        ico((0, -.18, 1.38), (.285, .12, .30), "iron")
        for s in (-1, 1):
            ico((s * .36, 0, 1.56), (.18, .20, .115), "iron")
            cube((s * .24, -.29, 1.35), (.075, .035, .25), "gold")
    if kind == "archer":
        cube((.16, -.29, 1.36), (.10, .035, .65), "leather", -.18)
        ico((-.34, .13, 1.41), (.13, .18, .30), trim)
    if kind == "ritual":
        cone((0, 0, 1.02), .47, .29, .72, base, 10)
        cube((0, -.305, 1.39), (.16, .03, .43), "paper")
        cube((0, -.33, 1.43), (.025, .035, .27), trim)
    if kind == "traveller":
        ico((0, .24, 1.36), (.31, .17, .37), "leather")
        cube((0, -.26, 1.59), (.50, .04, .10), trim)
    if kind == "ragged":
        for s in (-1, 1):
            cube((s * .21, -.20, .78), (.17, .035, .39), "cloth_dark", s * .12)
    finish(name, "outfit", index, PARTS)


def accessory(name, kind, index):
    if kind == "satchel":
        cube((.38, .08, .91), (.25, .17, .30), "leather")
        beam((-.23, -.25, 1.57), (.39, .07, 1.05), .025, "ochre")
    elif kind == "bow":
        beam((.47, .02, .80), (.63, .02, 1.70), .025, "wood")
        beam((.47, .02, .80), (.57, -.03, 1.30), .009, "linen")
        beam((.57, -.03, 1.30), (.63, .02, 1.70), .009, "linen")
        cylinder((-.35, .16, 1.42), .11, .46, "leather")
        for x in (-.40, -.35, -.30):
            beam((x, .16, 1.22), (x, .16, 1.85), .01, "wood")
    elif kind == "hammer":
        beam((.48, -.01, .80), (.48, -.01, 1.61), .03, "wood")
        cube((.48, -.01, 1.58), (.39, .15, .14), "iron")
    elif kind == "seal":
        cube((0, -.34, 1.35), (.18, .025, .27), "paper")
        cube((0, -.36, 1.35), (.025, .027, .18), "red")
        cylinder((-.45, .06, .98), .09, .15, "gold")
    elif kind == "ledger":
        cube((-.43, -.12, 1.00), (.23, .06, .30), "leather")
        cube((-.43, -.155, 1.01), (.15, .013, .23), "paper")
    elif kind == "ribbon":
        ico((.24, .27, 2.18), (.11, .035, .065), "red")
        cube((.29, .28, 2.06), (.035, .02, .22), "red", -.13)
    elif kind == "scarf":
        cylinder((0, 0, 1.61), .28, .11, "red")
        cube((.21, -.23, 1.42), (.15, .04, .43), "red")
    elif kind == "tools":
        for x in (-.34, -.23):
            beam((x, -.15, .75), (x, -.14, 1.12), .018, "iron")
        cube((-.28, -.15, .81), (.20, .10, .17), "leather")
    elif kind == "staff":
        beam((.50, 0, .05), (.50, 0, 1.87), .035, "wood")
        ico((.50, 0, 1.91), (.10, .10, .10), "gold")
    finish(name, "accessory", index, PARTS)


def roof(width, depth, z, rise, mat):
    x, y = width / 2, depth / 2
    for sign in (-1, 1):
        edge_y = sign * (y + .12)
        top = [(-x-.12, edge_y, z), (x+.12, edge_y, z),
               (x+.12, 0, z+rise), (-x-.12, 0, z+rise)]
        bottom = [(vx, vy, vz-.07) for vx, vy, vz in top]
        faces = [(0, 1, 2, 3) if sign < 0 else (3, 2, 1, 0),
                 (7, 6, 5, 4) if sign < 0 else (4, 5, 6, 7),
                 (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)]
        mesh("roof_half", top+bottom, faces, mat)
    for yy in (-y, y):
        beam((-x, yy, z), (0, yy, z+rise), .035, "wood")
        beam((0, yy, z+rise), (x, yy, z), .035, "wood")


def house(name, kind, index):
    width, depth, height = (3.1, 2.5, 2.20)
    cube((0, 0, .18), (width+.35, depth+.30, .36), "stone")
    cube((0, 0, 1.26), (width, depth, height-.36), "stone_light")
    for x in (-1.35, 1.35):
        for y in (-1.08, 1.08):
            cube((x, y, 1.25), (.18, .19, 1.80), "wood")
    roof(width, depth, height, .82, "roof_red" if kind == "home" else "roof")
    for y in (-depth/2, depth/2):
        mesh("gable", [(-width/2, y, height-.02), (width/2, y, height-.02),
                       (0, y, height+.80)], [(0, 1, 2) if y < 0 else (2, 1, 0)],
             "stone_light")
    cube((0, -depth/2-.035, .96), (.78, .08, 1.55), "wood")
    cylinder((.26, -depth/2-.09, 1.03), .035, .035, "gold", 8, (math.pi/2, 0, 0))
    for x in (-1.0, 1.0):
        cube((x, -depth/2-.06, 1.47), (.48, .08, .50), "shadow")
        cube((x, -depth/2-.10, 1.47), (.04, .04, .52), "wood_light")
        cube((x, -depth/2-.11, 1.47), (.48, .04, .04), "wood_light")
    if kind == "smithy":
        cube((1.05, 0, 2.73), (.47, .47, 1.1), "stone")
        cube((1.05, 0, 3.32), (.58, .58, .13), "stone_light")
        cube((-.55, -1.43, .75), (.55, .25, .42), "iron")
    elif kind == "shop":
        cube((0, -1.60, 1.75), (1.70, .65, .12), "ochre")
        for x in (-.72, .72):
            beam((x, -1.60, .23), (x, -1.60, 1.75), .045, "wood")
        cube((0, -1.65, .63), (1.40, .32, .72), "wood_light")
    else:
        cube((-.70, -1.40, .67), (.67, .24, .68), "wood")
    finish(name, "building", index, SCENERY)


def ruin(name, index):
    for x in (-1.7, 1.7):
        cube((x, 0, .22), (.62, 3.8, .44), "stone")
        for y in (-1.4, 1.4):
            cube((x, y, 1.45), (.48, .54, 2.45 if x < 0 else 1.70), "stone_light")
    cube((0, 1.55, .22), (3.8, .54, .44), "stone")
    cube((0, 1.55, 2.55), (3.8, .38, .45), "stone_light")
    cube((0, 1.30, 2.00), (.95, .09, .75), "shadow")
    cube((0, 1.23, 2.0), (.05, .06, .55), "red")
    cylinder((0, 0, .32), .63, .64, "stone", 10)
    finish(name, "building", index, SCENERY)


def cemetery(name, index):
    for x in (-1.35, 1.35):
        cube((x, 0, 1.12), (.38, .48, 2.24), "stone")
        ico((x, 0, 2.35), (.27, .29, .22), "stone_light")
    cube((0, 0, 2.25), (2.85, .40, .36), "stone_light")
    beam((-.90, -.22, .10), (-.90, -.22, 2.0), .035, "iron")
    beam((.90, -.22, .10), (.90, -.22, 2.0), .035, "iron")
    for z in (.40, 1.85):
        beam((-.92, -.22, z), (.92, -.22, z), .028, "iron")
    finish(name, "building", index, SCENERY)


def well(name, index):
    cylinder((0, 0, .47), .68, .94, "stone", 10)
    cylinder((0, 0, .97), .52, .035, "shadow", 10)
    for x in (-.65, .65):
        beam((x, 0, .65), (x, 0, 2.06), .065, "wood")
    roof(1.75, 1.45, 2.10, .45, "roof_red")
    cylinder((0, 0, 1.35), .10, 1.55, "wood", 8, (0, math.pi/2, 0))
    finish(name, "building", index, SCENERY)


def tree(name, kind, index):
    if kind == "pine":
        cone((0, 0, 1.32), .22, .16, 2.5, "wood", 7)
        for z, r in ((1.15, .88), (1.76, .72), (2.31, .51)):
            cone((0, 0, z), r, 0, 1.26, "leaf", 7)
    else:
        cone((0, 0, 1.25), .30, .17, 2.5, "wood", 7)
        for s in (-1, 1):
            beam((0, 0, 1.65), (s*.72, 0, 2.30), .10, "wood", 6)
        if kind == "oak":
            for x, y, z, sc in ((0, 0, 2.58, .82), (-.70, .05, 2.40, .65),
                                (.70, -.02, 2.43, .68), (0, -.45, 2.22, .60)):
                ico((x, y, z), (sc, sc*.72, sc*.72), "leaf_light" if x < 0 else "leaf")
        else:
            for x, y, z in ((-.72, 0, 2.30), (.72, 0, 2.30), (0, 0, 2.50)):
                beam((x, y, z), (x*.85, y, z+.32), .025, "wood", 5)
    finish(name, "nature", index, SCENERY)


def nature_small(name, kind, index):
    if kind == "bush":
        for x, y, z, r in ((0, 0, .45, .43), (-.38, .07, .34, .34), (.38, .02, .39, .36)):
            ico((x, y, z), (r, r*.85, r*.80), "leaf")
    elif kind == "grass":
        for x, y, h, tilt in ((-.24, .06, .50, -.18), (-.10, -.10, .66, -.06),
                              (.07, .02, .55, .16), (.22, -.09, .61, .26)):
            cone((x+tilt*.3, y, h/2), .075, 0, h, "grass", 4,
                 (0, tilt, 0))
    elif kind == "rock":
        ico((0, 0, .39), (.67, .55, .47), "stone", 1)
        ico((.30, -.17, .30), (.27, .30, .28), "stone_light")
    elif kind == "stones":
        for x, y, z, r in ((-.45, .05, .31, .34), (.18, .12, .44, .47),
                            (.49, -.25, .23, .25)):
            ico((x, y, z), (r, r*.84, z), "stone_light" if x < 0 else "stone")
    elif kind == "stump":
        cylinder((0, 0, .38), .34, .76, "wood", 7)
        cylinder((0, 0, .78), .32, .025, "wood_light", 7)
    finish(name, "nature", index, SCENERY)


def export_current(filepath):
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.export_scene.gltf(filepath=str(filepath), export_format='GLB',
                              use_selection=True, export_apply=True,
                              export_extras=True)


def write_catalog():
    npc = [
        ("travelling_merchant", "행상인", 0, 1, 1, 0, 0, 1.00),
        ("artisan", "생활 장인", 1, 4, 2, 7, 0, 1.00),
        ("hunter", "주령 사냥꾼", 0, 3, 4, 1, 0, 1.00),
        ("sibling", "동생", 1, 2, 0, 5, 0, .84),
        ("parent", "부모", 1, 4, 2, 0, 0, 1.00),
        ("village_merchant", "잡화상", 0, 0, 1, 4, 0, 1.05),
        ("child", "마을 아이", 0, 1, 0, 0, 0, .82),
        ("stranger", "돌아가지 못한 사람", 0, 3, 6, -1, 0, .98),
        ("raen", "라엔", 1, 5, 3, 6, 0, 1.00),
        ("sealer", "봉인사", 1, 4, 5, 3, 0, 1.00),
        ("smith", "대장장이", 0, 0, 2, 2, 0, 1.07),
    ]
    catalog = {
        "schemaVersion": 1,
        "coordinateSystem": "Blend: Z-up/front -Y; GLB: glTF Y-up conversion; metres, origin at feet",
        "characterHeightMetres": 2.4,
        "proportions": "three-head chibi",
        "partFile": "npc_parts.glb",
        "sceneryFile": "world_props.glb",
        "parts": {slot: [p for p in PARTS if p["slot"] == slot]
                  for slot in ("body", "hair", "outfit", "accessory")},
        "scenery": SCENERY,
        "npcs": [{"id": n, "label": label, "partIndices":
                  {"body": b, "hair": h, "outfit": o, "accessory": a},
                  "scale": scale, "storyRole": "story" if n not in
                  ("travelling_merchant", "artisan", "hunter") else "resident"}
                 for n, label, b, h, o, a, _, scale in npc],
        "notes": ["Part indices are zero-based and stable within this catalog.",
                  "Accessory index -1 means no accessory.",
                  "All pieces share the same local origin; instantiate selected meshes together.",
                  "NPC assembly, animation, collision and game import are future integration work."],
    }
    (OUT / "catalog.json").write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main():
    bpy.context.preferences.filepaths.save_version = 0
    start_collection("NPC_PARTS")
    body("NPC_BODY_M_00", False, 0)
    body("NPC_BODY_F_01", True, 1)
    for i, (kind, color) in enumerate([
        ("short", "hair_dark"), ("tousled", "hair_brown"),
        ("bob", "hair_brown"), ("long", "hair_dark"),
        ("bun", "hair_ash"), ("bob", "hair_silver"),
        ("braid", "hair_brown")]):
        hair(f"NPC_HAIR_{kind.upper()}_{i:02d}", kind, color, i)
    for i, (kind, base, trim) in enumerate([
        ("tunic", "linen", "leather"), ("traveller", "ochre", "leather"),
        ("apron", "cloth", "ochre"), ("armor", "cloth_dark", "red"),
        ("archer", "blue", "leather"), ("ritual", "purple", "gold"),
        ("ragged", "cloth_dark", "stone")]):
        clothing(f"NPC_OUTFIT_{kind.upper()}_{i:02d}", kind, base, trim, i)
    for i, kind in enumerate(("satchel", "bow", "hammer", "seal", "ledger",
                              "ribbon", "scarf", "tools", "staff")):
        accessory(f"NPC_ACCESSORY_{kind.upper()}_{i:02d}", kind, i)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / "npc_parts.blend"))
    export_current(OUT / "npc_parts.glb")

    start_collection("WORLD_PROPS")
    for i, kind in enumerate(("home", "smithy", "shop")):
        house(f"BUILDING_{kind.upper()}_{i:02d}", kind, i)
    ruin("BUILDING_MONASTERY_RUIN_03", 3)
    cemetery("BUILDING_CEMETERY_GATE_04", 4)
    well("BUILDING_WELL_05", 5)
    for i, kind in enumerate(("oak", "pine", "dead"), 6):
        tree(f"NATURE_TREE_{kind.upper()}_{i:02d}", kind, i)
    for i, kind in enumerate(("bush", "grass", "rock", "stones", "stump"), 9):
        nature_small(f"NATURE_{kind.upper()}_{i:02d}", kind, i)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / "world_props.blend"))
    export_current(OUT / "world_props.glb")
    write_catalog()
    print(f"MODULAR_COLLECTION_READY {len(PARTS)} parts {len(SCENERY)} scenery", flush=True)


if __name__ == "__main__":
    main()
