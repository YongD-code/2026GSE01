"""Export an editable character to a compact skinned runtime mesh; never render or save the source."""
import argparse
import ast
import json
import math
import struct
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'Art/Blender/CharacterRefinement/protagonist_refined.blend'
OUTPUT = ROOT / 'SimpleGame/Assets/Characters/Runtime/protagonist.meshbin'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--inspect', action='store_true')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
    rig = bpy.data.objects['주인공_리그']
    rig.animation_data_clear()
    rig.rotation_euler = (0, 0, 0)
    for bone in rig.pose.bones:
        bone.matrix_basis.identity()
        bone.rotation_mode = 'XYZ'
    bpy.context.view_layer.update()
    objects = [ob for ob in bpy.context.scene.objects
               if ob.type in {'MESH', 'CURVE'} and not ob.hide_render and ob.name != '주술 구체']
    if args.inspect:
        print(json.dumps({'bones': [b.name for b in rig.data.bones],
                          'objects': [{'name': ob.name, 'type': ob.type,
                                       'vertices': len(ob.data.vertices) if ob.type == 'MESH' else 0,
                                       'groups': len(ob.vertex_groups),
                                       'parent': ob.parent.name if ob.parent else '',
                                       'parent_bone': ob.parent_bone,
                                       'modifiers': [m.type for m in ob.modifiers]}
                                      for ob in objects]}, ensure_ascii=False))
        return
    export(rig, objects)


def export(rig, objects):
    bones = []

    def visit(bone):
        bones.append(bone)
        for child in bone.children:
            visit(child)

    for bone in rig.data.bones:
        if bone.parent is None:
            visit(bone)
    if not 1 <= len(bones) <= 32:
        raise ValueError('Runtime skeleton supports 1..32 bones')
    bone_index = {b.name: i for i, b in enumerate(bones)}
    # Keep surface modifiers but apply deformation only on the GPU at runtime.
    for ob in objects:
        for modifier in ob.modifiers:
            if modifier.type == 'ARMATURE':
                modifier.show_viewport = False
            elif modifier.type == 'SUBSURF':
                modifier.levels = min(modifier.levels, 1)
        if ob.type == 'MESH' and len(ob.data.polygons) > 32:
            decimate = ob.modifiers.new('게임용 면 수 조정', 'DECIMATE')
            decimate.ratio = .22
            decimate.use_collapse_triangulate = True
    bpy.context.view_layer.update()
    depsgraph = bpy.context.evaluated_depsgraph_get()
    rig_inverse = rig.matrix_world.inverted()
    raw = []
    for ob in objects:
        evaluated = ob.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh(preserve_all_data_layers=True, depsgraph=depsgraph)
        if not mesh:
            continue
        transform = rig_inverse @ ob.matrix_world
        normal_transform = transform.to_3x3().inverted().transposed()
        mesh.calc_loop_triangles()
        groups = {group.index: bone_index[group.name] for group in ob.vertex_groups
                  if group.name in bone_index}
        for triangle in mesh.loop_triangles:
            material = (ob.material_slots[triangle.material_index].material
                        if triangle.material_index < len(ob.material_slots) else None)
            color = tuple(material.diffuse_color) if material else (.3, .3, .4, 1.)
            for loop_index in triangle.loops:
                loop = mesh.loops[loop_index]
                vertex = mesh.vertices[loop.vertex_index]
                influences = [(groups[g.group], g.weight) for g in vertex.groups
                              if g.group in groups and g.weight > .00001]
                if not influences:
                    fallback = bone_index.get(ob.parent_bone, bone_index.get('root', 0))
                    influences = [(fallback, 1.)]
                influences = sorted(influences, key=lambda pair: pair[1], reverse=True)[:4]
                total = sum(weight for _, weight in influences)
                joints = [float(index) for index, _ in influences]
                weights = [weight / total for _, weight in influences]
                joints += [0.] * (4 - len(joints))
                weights += [0.] * (4 - len(weights))
                normal = normal_transform @ loop.normal
                if normal.length_squared < 1.e-12:
                    normal = normal_transform @ triangle.normal
                normal.normalize()
                raw.append((transform @ vertex.co, normal, color, joints, weights))
        evaluated.to_mesh_clear()
    if not raw or len(raw) > 600000:
        raise ValueError('Invalid mesh vertex count')
    low = min(position.z for position, *_ in raw)
    height = max(position.z for position, *_ in raw) - low
    if height < .1:
        raise ValueError('Invalid character height')
    normalization = Matrix.Scale(1 / height, 4) @ Matrix.Translation((0, 0, -low))
    denormalization = normalization.inverted()
    inverse_bind = [normalization @ b.matrix_local.inverted() @ denormalization for b in bones]
    source = (ROOT / 'Art/Blender/create_protagonist.py').read_text(encoding='utf-8')
    pose_node = next(node for node in ast.parse(source).body
                     if isinstance(node, ast.FunctionDef) and node.name == 'pose')
    namespace = {'rig': rig, 'orb': bpy.data.objects['주술 구체'], 'bpy': bpy,
                 'math': math, 'CUSTOMIZE': None, 'ASSET_NAME': 'protagonist'}
    exec(compile(ast.Module(body=[pose_node], type_ignores=[]), '<character pose>', 'exec'), namespace)
    sys.path.insert(0, str(ROOT / 'Art/Blender/CharacterRefinement'))
    from locomotion import apply_locomotion
    clips = []
    for name, count in [('idle', 32), ('walk', 48), ('run', 40), ('cast', 32)]:
        samples = []
        for frame in range(count):
            t = frame / (count - 1 if name == 'cast' else count)
            namespace['pose'](name, t)
            apply_locomotion(rig, name, t)
            if name == 'idle':
                rig.pose.bones['chest'].rotation_euler.x = .008 * math.sin(t * math.tau)
            bpy.context.view_layer.update()
            for bone in bones:
                posed = rig.pose.bones[bone.name]
                local = posed.parent.matrix.inverted() @ posed.matrix if posed.parent else posed.matrix
                local = normalization @ local @ denormalization
                location, rotation, scale = local.decompose()
                samples.extend((*location, rotation.x, rotation.y, rotation.z, rotation.w, *scale))
        clips.append((name, count, samples))
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    temporary = OUTPUT.with_suffix('.meshbin.tmp')
    with temporary.open('wb') as stream:
        stream.write(b'CHAR3D01')
        stream.write(struct.pack('<4I', 1, len(raw), len(bones), len(clips)))
        for bone, inverse in zip(bones, inverse_bind):
            stream.write(struct.pack('<i', bone_index[bone.parent.name] if bone.parent else -1))
            stream.write(struct.pack('<16f', *(inverse[row][col] for col in range(4) for row in range(4))))
        for position, normal, color, joints, weights in raw:
            normalized = normalization @ position
            stream.write(struct.pack('<18f', *normalized, *normal, *color, *joints, *weights))
        for name, count, samples in clips:
            stream.write(name.encode('ascii').ljust(16, b'\0'))
            stream.write(struct.pack('<I', count))
            stream.write(struct.pack('<' + str(len(samples)) + 'f', *samples))
    temporary.replace(OUTPUT)
    report = {'source': str(SOURCE.relative_to(ROOT)), 'output': str(OUTPUT.relative_to(ROOT)),
              'vertices': len(raw), 'triangles': len(raw) // 3, 'bones': [b.name for b in bones],
              'sourceHeight': height, 'sourceGround': low, 'layout': '18 float32 vertex, 10 float32 TRS',
              'clips': [{'name': n, 'frames': count} for n, count, _ in clips],
              'sourceModified': False, 'imagesRendered': False}
    (Path(__file__).parent / 'character_export.json').write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False))


if __name__ == '__main__':
    main()
