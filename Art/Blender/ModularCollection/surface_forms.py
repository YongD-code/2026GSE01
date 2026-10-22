"""Small, exportable polygon surfaces used by the modular character artists."""

import math
from contextlib import contextmanager

import bpy
from mathutils import Vector

import create_collection as geom


def stylize_scene():
    """Use the same four diffuse bands as the established spirit source materials."""
    materials = {slot.material for obj in bpy.context.scene.objects if obj.type == 'MESH'
                 for slot in obj.material_slots if slot.material}
    for mat in materials:
        if not mat.name.startswith('MAT_'):
            continue
        base = tuple(mat.diffuse_color)
        principled = mat.node_tree.nodes.get('Principled BSDF')
        if principled:
            mat['exportMetallic'] = float(principled.inputs['Metallic'].default_value)
            mat['exportRoughness'] = float(principled.inputs['Roughness'].default_value)
        mat['visualFamily'] = 'four-band ritual illustration'
        nodes, links = mat.node_tree.nodes, mat.node_tree.links
        nodes.clear()
        output = nodes.new('ShaderNodeOutputMaterial')
        emission = nodes.new('ShaderNodeEmission')
        glow = any(t in mat.name for t in ('TEAR_BLUE', 'STAG_GLOW', 'HOUND_RED'))
        if glow:
            emission.inputs['Color'].default_value = base
            emission.inputs['Strength'].default_value = 1.25
        else:
            diffuse = nodes.new('ShaderNodeBsdfDiffuse')
            diffuse.inputs['Color'].default_value = (1, 1, 1, 1)
            rgb = nodes.new('ShaderNodeShaderToRGB')
            ramp = nodes.new('ShaderNodeValToRGB')
            ramp.color_ramp.interpolation = 'CONSTANT'
            ramp.color_ramp.elements.remove(ramp.color_ramp.elements[1])
            for i, (position, intensity) in enumerate(((0, .20), (.16, .40), (.40, .68), (.72, 1))):
                element = ramp.color_ramp.elements[0] if i == 0 else ramp.color_ramp.elements.new(position)
                element.position = position
                element.color = (*(c*intensity for c in base[:3]), 1)
            links.new(diffuse.outputs[0], rgb.inputs[0])
            links.new(rgb.outputs[0], ramp.inputs[0])
            links.new(ramp.outputs[0], emission.inputs['Color'])
        links.new(emission.outputs[0], output.inputs['Surface'])


@contextmanager
def export_materials():
    """glTF cannot represent ShaderToRGB: explicitly export diffuse colors with PBR nodes.

    The editable .blend keeps its live cel lighting. A game renderer can apply its own
    banded lighting to these base colors. Never export an unsupported linked emission
    graph, which otherwise turns the established spirit materials into solid white.
    """
    assignments, copies, seen = [], {}, set()
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH' or obj.data in seen:
            continue
        seen.add(obj.data)
        for i, mat in enumerate(obj.data.materials):
            if mat is None:
                continue
            if mat not in copies:
                copied = bpy.data.materials.new(mat.name+'_SURFACE')
                copied.diffuse_color = mat.diffuse_color
                copied.use_nodes = True
                node = copied.node_tree.nodes.get('Principled BSDF')
                node.inputs['Base Color'].default_value = mat.diffuse_color
                node.inputs['Metallic'].default_value = mat.get('exportMetallic', 0)
                node.inputs['Roughness'].default_value = mat.get('exportRoughness', .8)
                if mat.get('lantern_fire'):
                    # glTF has no volume shader. Keep the closed flame meshes as a warm
                    # emissive surface and make the outer heat volume transparent.
                    heat = '열기' in mat.name
                    color = (.95, .16, .006, .08 if heat else .75)
                    node.inputs['Base Color'].default_value = color
                    node.inputs['Alpha'].default_value = color[3]
                    node.inputs['Emission Color'].default_value = color
                    node.inputs['Emission Strength'].default_value = .6 if heat else 1.5
                    copied.surface_render_method = 'DITHERED'
                for original in mat.node_tree.nodes if mat.use_nodes else []:
                    if original.type == 'EMISSION' and not original.inputs['Color'].is_linked:
                        strength = original.inputs['Strength'].default_value
                        if strength > 1:
                            node.inputs['Emission Color'].default_value = original.inputs['Color'].default_value
                            node.inputs['Emission Strength'].default_value = min(strength-1, 1)
                copies[mat] = copied
            assignments.append((obj, i, mat))
            obj.data.materials[i] = copies[mat]
    try:
        yield
    finally:
        for obj, i, mat in assignments:
            obj.data.materials[i] = mat
        for mat in copies.values():
            bpy.data.materials.remove(mat)


def surface(name, vertices, faces, material, smooth=False):
    obj = geom.mesh(name, vertices, faces, material)
    for face in obj.data.polygons:
        face.use_smooth = smooth
    return obj


def loft(name, rings, material, sides=16, smooth=True, center=(0, 0), ripple=0):
    """Rings are (height, half width, half depth, optional forward offset)."""
    vertices = []
    for j, ring in enumerate(rings):
        z, rx, ry = ring[:3]
        cy = ring[3] if len(ring) > 3 else 0
        for i in range(sides):
            a = math.tau*i/sides
            r = 1 + ripple*math.cos(a*6+j*.35)
            vertices.append((center[0]+rx*math.sin(a)*r,
                             center[1]-ry*math.cos(a)*r+cy, z))
    faces = [(j*sides+i, j*sides+(i+1)%sides,
              (j+1)*sides+(i+1)%sides, (j+1)*sides+i)
             for j in range(len(rings)-1) for i in range(sides)]
    faces += [tuple(reversed(range(sides))),
              tuple(range((len(rings)-1)*sides, len(rings)*sides))]
    obj = surface(name, vertices, faces, material, smooth)
    # Keep the end planes crisp rather than rounding them into the side walls.
    for face in list(obj.data.polygons)[-2:]:
        face.use_smooth = False
    return obj


def sweep(name, points, radii, material, sides=10, smooth=True, flatten=1):
    vertices = []
    for j, (point, radius) in enumerate(zip(points, radii)):
        p = Vector(point)
        tangent = Vector(points[min(j+1, len(points)-1)])-Vector(points[max(0, j-1)])
        tangent.normalize()
        axis = tangent.cross(Vector((0, 1, 0)))
        if axis.length < .01:
            axis = tangent.cross(Vector((1, 0, 0)))
        axis.normalize()
        other = tangent.cross(axis).normalized()
        for i in range(sides):
            a = math.tau*i/sides
            vertices.append(tuple(p+radius*(math.cos(a)*axis+math.sin(a)*other*flatten)))
    faces = [(j*sides+i, j*sides+(i+1)%sides,
              (j+1)*sides+(i+1)%sides, (j+1)*sides+i)
             for j in range(len(points)-1) for i in range(sides)]
    faces += [tuple(reversed(range(sides))),
              tuple(range((len(points)-1)*sides, len(points)*sides))]
    return surface(name, vertices, faces, material, smooth)


def oval(name, pos, scale, material, sides=16, levels=8, smooth=True):
    x, y, z = pos
    sx, sy, sz = scale
    rings = []
    for j in range(levels+1):
        a = -math.pi/2+math.pi*j/levels
        r = max(.006, math.cos(a))
        rings.append((z+sz*math.sin(a), sx*r, sy*r, y))
    return loft(name, rings, material, sides, smooth, center=(x, 0))


def line(name, points, radius, material, sides=6):
    return sweep(name, points, [radius]*len(points), material, sides)


def panel(name, points, material, thickness=.01, smooth=False):
    obj = surface(name, points, [tuple(range(len(points)))], material, smooth)
    if thickness:
        modifier = obj.modifiers.new('SURFACE_THICKNESS', 'SOLIDIFY')
        modifier.thickness = thickness
    return obj


def ribbon(name, points, widths, material, thickness=.008):
    vertices = []
    for point, width in zip(points, widths):
        x, y, z = point
        vertices.extend([(x-width/2, y, z), (x+width/2, y, z)])
    faces = [(j*2, j*2+1, j*2+3, j*2+2) for j in range(len(points)-1)]
    obj = surface(name, vertices, faces, material, True)
    if thickness:
        modifier = obj.modifiers.new('CLOTH_THICKNESS', 'SOLIDIFY')
        modifier.thickness = thickness
    return obj


def lock(name, points, widths, material):
    """A curved, ridged hair/fur blade with a closed back and an actual tapered tip."""
    vertices = []
    for (x, y, z), w in zip(points, widths):
        vertices += [(x-w, y, z), (x, y-w*.20, z+.012), (x+w, y, z),
                     (x, y+w*.18, z)]
    faces = [(j*4+i, j*4+(i+1)%4, (j+1)*4+(i+1)%4, (j+1)*4+i)
             for j in range(len(points)-1) for i in range(4)]
    faces += [(3, 2, 1, 0), tuple(range((len(points)-1)*4, len(points)*4))]
    return surface(name, vertices, faces, material, False)


def ring(name, pos, radius, material, thickness=.012, axis='Z', sides=24):
    x, y, z = pos
    points = []
    for i in range(sides+1):
        a = math.tau*i/sides
        if axis == 'Y':
            points.append((x+radius*math.cos(a), y, z+radius*math.sin(a)))
        else:
            points.append((x+radius*math.cos(a), y+radius*math.sin(a), z))
    return line(name, points, thickness, material)


def charm(pos, scale=1, material='gold'):
    x, y, z = pos
    ring('charm_rim', pos, .065*scale, material, .008*scale, 'Y')
    oval('charm_plate', pos, (.060*scale, .008*scale, .060*scale), 'leather', 16, 4)
    line('charm_engraving', [(x-.025*scale, y-.012, z), (x+.025*scale, y-.012, z)],
         .004*scale, material)
    line('charm_engraving', [(x, y-.012, z-.03*scale), (x, y-.012, z+.03*scale)],
         .004*scale, material)


def bell(pos, scale=1):
    x, y, z = pos
    loft('bell', [(z-.06*scale, .065*scale, .065*scale),
                 (z-.04*scale, .07*scale, .07*scale),
                 (z+.025*scale, .043*scale, .043*scale),
                 (z+.055*scale, .016*scale, .016*scale)],
         'gold', 16, True, (x, y))
    ring('bell_lip', (x, y, z-.045*scale), .065*scale, 'gold', .006*scale)
    line('bell_slot', [(x-.035*scale, y-.057*scale, z-.028*scale),
                       (x+.035*scale, y-.057*scale, z-.028*scale)], .004*scale, 'shadow')
