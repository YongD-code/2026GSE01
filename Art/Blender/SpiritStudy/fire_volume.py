"""Editable volume fire for the lantern candidate, rendered into transparent sprites.

The density and radiance live inside closed 3D meshes. The game never runs this shader.
All turbulence, bend keys and small embers loop over the same review cycle.
"""
import math

import bpy

TAU = math.tau


def volume_material(name, haze=False):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.clear()
    output = nodes.new('ShaderNodeOutputMaterial')
    volume = nodes.new('ShaderNodeVolumePrincipled')
    volume.inputs['Color'].default_value = (.30, .055, .006, 1)
    volume.inputs['Anisotropy'].default_value = .15
    coord = nodes.new('ShaderNodeTexCoord')
    drift = nodes.new('ShaderNodeVectorMath')
    drift.name = 'FLAME_DRIFT'
    drift.operation = 'ADD'
    drift.inputs[1].default_value = (0, .14, 0)
    links.new(coord.outputs['Generated'], drift.inputs[0])
    noise = nodes.new('ShaderNodeTexNoise')
    noise.name = 'FLAME_TURBULENCE'
    noise.noise_dimensions = '4D'
    noise.inputs['Scale'].default_value = 4.5
    noise.inputs['Detail'].default_value = 3
    noise.inputs['Roughness'].default_value = .67
    links.new(drift.outputs[0], noise.inputs['Vector'])
    channels = nodes.new('ShaderNodeMapRange')
    channels.inputs['From Min'].default_value = .32 if haze else .40
    channels.inputs['From Max'].default_value = .68 if haze else .62
    channels.inputs['To Min'].default_value = 0
    channels.inputs['To Max'].default_value = 1
    channels.clamp = True
    links.new(noise.outputs['Fac'], channels.inputs['Value'])
    center = nodes.new('ShaderNodeVectorMath')
    center.operation = 'SUBTRACT'
    center.inputs[1].default_value = (.5, .5, .45)
    links.new(coord.outputs['Generated'], center.inputs[0])
    stretch = nodes.new('ShaderNodeVectorMath')
    stretch.operation = 'MULTIPLY'
    stretch.inputs[1].default_value = (2.0, 2.0, 1.25)
    links.new(center.outputs[0], stretch.inputs[0])
    length = nodes.new('ShaderNodeVectorMath')
    length.operation = 'LENGTH'
    links.new(stretch.outputs[0], length.inputs[0])
    inward = nodes.new('ShaderNodeMath')
    inward.operation = 'SUBTRACT'
    inward.inputs[0].default_value = 1
    inward.use_clamp = True
    links.new(length.outputs['Value'], inward.inputs[1])
    density = nodes.new('ShaderNodeMath')
    density.operation = 'MULTIPLY'
    links.new(inward.outputs[0], density.inputs[0])
    links.new(channels.outputs['Result'], density.inputs[1])
    strength = nodes.new('ShaderNodeMath')
    strength.operation = 'MULTIPLY'
    strength.inputs[1].default_value = .65 if haze else 12
    links.new(density.outputs[0], strength.inputs[0])
    links.new(strength.outputs[0], volume.inputs['Density'])
    radiance = nodes.new('ShaderNodeMath')
    radiance.operation = 'MULTIPLY'
    radiance.inputs[1].default_value = .75 if haze else 17
    links.new(density.outputs[0], radiance.inputs[0])
    links.new(radiance.outputs[0], volume.inputs['Emission Strength'])
    ramp = nodes.new('ShaderNodeValToRGB')
    ramp.color_ramp.interpolation = 'EASE'
    ramp.color_ramp.elements.remove(ramp.color_ramp.elements[1])
    for i, (at, color) in enumerate([(0, (.9, .016, .001, 1)),
                                    (.14, (1, .12, .003, 1)),
                                    (.28, (1, .46, .025, 1)),
                                    (.43, (1, .88, .38, 1))]):
        element = ramp.color_ramp.elements[0] if i == 0 else ramp.color_ramp.elements.new(at)
        element.position = at
        element.color = color
    links.new(density.outputs[0], ramp.inputs[0])
    links.new(ramp.outputs[0], volume.inputs['Emission Color'])
    links.new(volume.outputs[0], output.inputs['Volume'])
    mat['lantern_fire'] = True
    return mat


def flame_geometry(height, radius, offset, twist):
    vertices, faces = [(offset[0], offset[1], 1.13)], []
    segments, rings = 40, 30
    for j in range(1, rings):
        t = j / rings
        # Closed round cross-sections have equal X/Y thickness, with irregular lobes.
        radius_at = radius * math.sin(math.pi*t)**.70 * (1-.43*t)
        cx = offset[0] + .11*math.sin(t*4.4+twist)*t
        cy = offset[1] + .075*math.sin(t*5.2+twist)*t
        for i in range(segments):
            a = i*TAU/segments
            lobe = 1 + .12*math.sin(a*3+t*7+twist) + .055*math.sin(a*5-t*9)
            vertices.append((cx+radius_at*lobe*math.cos(a),
                             cy+radius_at*lobe*math.sin(a), 1.13+t*height))
    top = len(vertices)
    vertices.append((offset[0]+.11*math.sin(4.4+twist),
                     offset[1]+.075*math.sin(5.2+twist), 1.13+height))
    for i in range(segments):
        faces.append((0, 1+(i+1)%segments, 1+i))
    for j in range(rings-2):
        for i in range(segments):
            a = 1+j*segments+i
            b = 1+j*segments+(i+1)%segments
            faces.append((a,b,b+segments,a+segments))
    base = 1+(rings-2)*segments
    for i in range(segments):
        faces.append((base+i,base+(i+1)%segments,top))
    return vertices, faces


def create_fire(mesh, rig):
    fire = volume_material('불꽃 · 내부 밀도와 온도')
    haze = volume_material('불꽃 · 가장자리 열기', True)
    # These volumes overlap in the cage. They are not depth-offset colored cutouts.
    for name,height,radius,offset,twist,mat in [
        ('등롱 불길 · 중심',.95,.30,(0,-.025),.4,fire),
        ('등롱 불길 · 왼 불혀',.69,.15,(-.16,.035),2.7,fire),
        ('등롱 불길 · 뒤 불혀',.77,.14,(.14,.075),4.5,fire),
        ('등롱 불길 · 열기',1.02,.35,(0,-.025),.4,haze)]:
        verts,faces = flame_geometry(height,radius,offset,twist)
        ob = mesh(name,verts,faces,mat,'flame')
        ob['lantern_fire'] = True
        ob.shape_key_add(name='Basis')
        for label,sign in [('불혀 휘어짐 A',1),('불혀 휘어짐 B',-1)]:
            key = ob.shape_key_add(name=label)
            for original,changed in zip(ob.data.vertices,key.data):
                t = max(0,min(1,(original.co.z-1.13)/height))
                changed.co.x += sign*.075*t**1.7*math.sin(t*3+twist)
                changed.co.y += sign*.055*t**1.6*math.cos(t*4+twist)
                changed.co.z += sign*.025*t*math.sin(t*6+twist)
    # The light is part of the model, following the hovering body rather than world space.
    light = bpy.data.lights.new('등롱 속불의 반사광','POINT')
    light.energy = 24
    light.color = (1,.22,.025)
    light.shadow_soft_size = .27
    ob = bpy.data.objects.new('등롱 속불의 반사광',light)
    bpy.context.scene.collection.objects.link(ob)
    ob.location = (0,-.18,1.58)
    bpy.context.view_layer.update()
    world = ob.matrix_world.copy()
    # Bone parenting uses the bone tail; restore world placement after resolving it.
    ob.parent = rig
    ob.parent_type = 'BONE'
    ob.parent_bone = 'body'
    bpy.context.view_layer.update()
    ob.matrix_world = world
    light['lantern_fire'] = True
    scene = bpy.context.scene
    # Avoid spending depth samples over the default 100-unit range for a one-unit fire.
    # All review/export cameras share this distance; keep this range with that camera.
    for key,value in [('volumetric_samples',128),('volumetric_tile_size','2'),
                      ('use_volume_custom_range',True),('volumetric_start',7.2),
                      ('volumetric_end',10.8)]:
        if hasattr(scene.eevee,key):
            setattr(scene.eevee,key,value)


def animate_fire(phase, frame=None):
    angle = TAU*phase
    # Two circular offsets return exactly to the start, avoiding a noise jump at the seam.
    for mat in bpy.data.materials:
        if not mat.get('lantern_fire'):
            continue
        noise = mat.node_tree.nodes['FLAME_TURBULENCE']
        drift = mat.node_tree.nodes['FLAME_DRIFT']
        noise.inputs['W'].default_value = .55*math.sin(angle)
        drift.inputs[1].default_value = (.10*math.sin(angle),.14*math.cos(angle),.25*math.cos(angle))
        if frame is not None:
            noise.inputs['W'].keyframe_insert('default_value',frame=frame)
            drift.inputs[1].keyframe_insert('default_value',frame=frame)
    for ob in bpy.data.objects:
        if not ob.get('lantern_fire') or not ob.data.shape_keys:
            continue
        for i,label in enumerate(['불혀 휘어짐 A','불혀 휘어짐 B']):
            key = ob.data.shape_keys.key_blocks[label]
            key.value = .5+.5*math.sin(angle+i*math.pi)
            if frame is not None:
                key.keyframe_insert('value',frame=frame)
    for light in bpy.data.lights:
        if light.get('lantern_fire'):
            light.energy = 24*(1+.13*math.sin(angle*2)+.06*math.sin(angle*3+.5))
            if frame is not None:
                light.keyframe_insert('energy',frame=frame)
