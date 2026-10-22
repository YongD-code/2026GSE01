"""Export modular NPC parts and seven keyed spirits for the shared game mesh renderer.

Blender --background --factory-startup --python this_file -- --asset npc
Other assets: forgotten, pilgrim, hands, saint, eater, stag, hound.
Reads the approved blend sources without saving or simplifying them.
"""

import argparse
import json
import math
import struct
import sys
from pathlib import Path

import bpy
from mathutils import Matrix

ROOT = Path(__file__).resolve().parents[3]
SLOTS = {'body': 0, 'hair': 1, 'outfit': 2, 'accessory': 3}
CLIPS = ('idle', 'walk', 'run', 'cast', 'jump', 'turn_left', 'turn_right', 'attack')


def export(key):
    npc = key == 'npc'
    source = ROOT/'Art/Blender'/('ModularCollection/npc_rigged.blend' if npc
                               else 'MobCollection/'+key+'/model.blend')
    output = ROOT/'SimpleGame/Assets'/('Characters/Runtime/npc_parts.meshbin' if npc
                                     else 'Spirits/Runtime/'+key+'.meshbin')
    bpy.ops.wm.open_mainfile(filepath=str(source))
    rig = next(obj for obj in bpy.context.scene.objects if obj.type == 'ARMATURE')
    rig.animation_data_create()
    rig.animation_data.action = None
    for joint in rig.pose.bones:
        joint.matrix_basis.identity()
    objects = sorted((o for o in bpy.context.scene.objects if o.type == 'MESH'), key=lambda o: o.name)
    bones = []

    def visit(bone):
        bones.append(bone)
        for child in bone.children:
            visit(child)

    for bone in rig.data.bones:
        if bone.parent is None:
            visit(bone)
    if not 1 <= len(bones) <= 32:
        raise ValueError('Unsupported runtime skeleton')
    indices = {bone.name: i for i, bone in enumerate(bones)}
    parts = {}
    if npc:
        catalog = json.loads((source.parent/'catalog.json').read_text(encoding='utf-8'))
        parts = {p['mesh']: (SLOTS[slot], p['index'])
                 for slot, values in catalog['parts'].items() for p in values}
    raw, ranges = [], []
    rig_inverse = rig.matrix_world.inverted()
    for obj in objects:
        for modifier in obj.modifiers:
            if modifier.type == 'ARMATURE':
                modifier.show_viewport = False
        if obj.data.shape_keys:
            for block in obj.data.shape_keys.key_blocks:
                block.value = 0
    bpy.context.view_layer.update()
    graph = bpy.context.evaluated_depsgraph_get()
    for obj in objects:
        start = len(raw)
        evaluated = obj.evaluated_get(graph)
        mesh = evaluated.to_mesh(preserve_all_data_layers=True, depsgraph=graph)
        transform = rig_inverse @ obj.matrix_world
        normal_transform = transform.to_3x3().inverted().transposed()
        groups = {group.index: indices[group.name] for group in obj.vertex_groups if group.name in indices}
        mesh.calc_loop_triangles()
        for triangle in mesh.loop_triangles:
            material = (obj.material_slots[triangle.material_index].material
                        if triangle.material_index < len(obj.material_slots) else None)
            color = tuple(material.diffuse_color) if material else (.3, .3, .4, 1)
            if material and material.get('lantern_fire'):
                color = (1.9, .35, .015, .0 if '열기' in material.name else 1)
            elif material and any(t in material.name for t in ('TEAR_BLUE', 'STAG_GLOW', 'HOUND_RED')):
                color = tuple(c*1.35 for c in color[:3])+(1,)
            for loop_index in triangle.loops:
                loop = mesh.loops[loop_index]
                vertex = mesh.vertices[loop.vertex_index]
                influence = sorted(((groups[g.group], g.weight) for g in vertex.groups
                                    if g.group in groups and g.weight > .00001),
                                   key=lambda item: item[1], reverse=True)[:4]
                if not influence:
                    influence = [(indices.get(obj.parent_bone, indices.get('root', 0)), 1)]
                total = sum(w for _, w in influence)
                joints = [float(i) for i, _ in influence]+[0.]*(4-len(influence))
                weights = [w/total for _, w in influence]+[0.]*(4-len(influence))
                normal = normal_transform @ loop.normal
                if normal.length_squared < 1e-12:
                    normal = normal_transform @ triangle.normal
                normal.normalize()
                raw.append((transform @ vertex.co, normal, color, joints, weights))
        evaluated.to_mesh_clear()
        if npc:
            slot, index = parts[obj.name]
            ranges.append((slot, index, start, len(raw)-start))
    if not raw or len(raw) > 600000:
        raise ValueError('Invalid runtime mesh vertex count')
    if not npc:
        ranges = [(-1, 0, 0, len(raw))]
    ground = min(p.z for p, *_ in raw)
    height = max(p.z for p, *_ in raw)-ground
    normalization = Matrix.Scale(1/height, 4) @ Matrix.Translation((0, 0, -ground))
    inverse = normalization.inverted()
    binds = [normalization @ bone.matrix_local.inverted() @ inverse for bone in bones]
    radius = max(math.hypot(p.x, p.y)/height for p, *_ in raw)
    # Extra room for wide quadrupeds, long equipment, turns and the keyed jump arc.
    framing = min(.70, 112/max(1, radius*math.sqrt(2)*.85*180))
    prefix = 'NPC_' if npc else 'MOB_'+key.upper()+'_'

    def action_for(name):
        action = bpy.data.actions.get(prefix+name.upper())
        if action:
            return action
        if name in ('cast', 'attack'):
            action = bpy.data.actions.get(prefix+'ATTACK') or bpy.data.actions.get('cast')
            if action:
                return action
            for candidate in bpy.data.actions:
                if 'cast' in candidate.name.lower():
                    return candidate
        return bpy.data.actions[prefix+'IDLE']

    clips = []
    for name in CLIPS:
        action = action_for(name)
        rig.animation_data.action = action
        first, last = [int(value) for value in action.frame_range]
        loop = name in ('idle', 'walk', 'run')
        count = max(2, min(120, last-first+(0 if loop else 1)))
        samples = []
        for frame in range(count):
            sample = first+(last-first)*frame/(count if loop else count-1)
            bpy.context.scene.frame_set(int(sample), subframe=sample-int(sample))
            if name.startswith('turn_'):
                # World heading is interpolated by the Actor. Retain anticipation in the
                # torso/head while avoiding a second 45-degree rotation of the root.
                rig.pose.bones['root'].rotation_euler.y = 0
            bpy.context.view_layer.update()
            for bone in bones:
                posed = rig.pose.bones[bone.name]
                local = posed.parent.matrix.inverted() @ posed.matrix if posed.parent else posed.matrix
                location, rotation, scale = (normalization @ local @ inverse).decompose()
                samples.extend((*location, rotation.x, rotation.y, rotation.z, rotation.w, *scale))
        clips.append((name, count, samples))
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix('.meshbin.tmp')
    with temporary.open('wb') as stream:
        stream.write(b'CHAR3D01')
        stream.write(struct.pack('<4I', 2, len(raw), len(bones), len(clips)))
        stream.write(struct.pack('<fI', framing, len(ranges)))
        for entry in ranges:
            stream.write(struct.pack('<iiII', *entry))
        for bone, bind in zip(bones, binds):
            stream.write(struct.pack('<i', indices[bone.parent.name] if bone.parent else -1))
            stream.write(struct.pack('<16f', *(bind[row][col] for col in range(4) for row in range(4))))
        for position, normal, color, joints, weights in raw:
            stream.write(struct.pack('<18f', *(normalization @ position), *normal, *color, *joints, *weights))
        for name, count, samples in clips:
            stream.write(name.encode('ascii').ljust(16, b'\0'))
            stream.write(struct.pack('<I', count))
            stream.write(struct.pack('<'+str(len(samples))+'f', *samples))
    temporary.replace(output)
    print('COLLECTION_RUNTIME_READY', key, len(raw)//3, 'triangles', len(bones), 'bones',
          len(ranges), 'parts', len(clips), 'clips', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--asset', required=True,
                        choices=('npc', 'forgotten', 'pilgrim', 'hands', 'saint', 'eater', 'stag', 'hound'))
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    export(args.asset)
