"""Project the reviewed 3D plant templates to the existing cached world-mesh renderer."""
import math
from pathlib import Path
import bpy
from mathutils import Vector
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[4]
bpy.ops.wm.open_mainfile(filepath=str(HERE/'preview.blend'))
meshes=sorted([m for m in bpy.data.meshes if m.name.startswith(('W_FLOWER_VARIANT_','W_GRASS_VARIANT_'))],key=lambda m:m.name)
lines=['#pragma once','#include <array>','namespace StagMeadowMesh','{',
       'struct Vertex { float x, y, r, g, b; };',
       'struct Range { int first, count; };',
       'struct Plant { float x, y, scale, delay; int mesh; };']
vertices=[]; ranges=[]
for mesh in meshes:
    start=len(vertices);mesh.calc_loop_triangles()
    variant=int(mesh.name[-2:]);a=variant*2.39996323
    for triangle in mesh.loop_triangles:
        mat=mesh.materials[triangle.material_index]
        base=mat.diffuse_color
        shade=.72+.28*max(0,triangle.normal.dot(Vector((-.4,-.5,.8)).normalized()))
        for index in triangle.vertices:
            p=mesh.vertices[index].co
            x=(p.x*math.cos(a)-p.y*math.sin(a))*34
            y=(p.x*math.sin(a)+p.y*math.cos(a))*34
            z=p.z*34
            vertices.append(((x-y)*.85,(x+y)*.43-z,*[min(1.2,c*shade) for c in base[:3]]))
    ranges.append((start,len(vertices)-start))
def num(x): return f'{x:.6f}f'
lines.append(f'constexpr std::array<Vertex, {len(vertices)}> Vertices = {{{{')
lines.extend('    {'+', '.join(num(x) for x in v)+'},' for v in vertices)
lines.extend(['}};',f'constexpr std::array<Range, {len(ranges)}> Ranges = {{{{'])
lines.extend('    {'+f'{a}, {b}'+'},' for a,b in ranges)
plants=sorted([o for o in bpy.context.scene.objects if o.name.startswith(('W_FLOWER_','W_GRASS_'))],key=lambda o:o.name)
bpy.context.scene.frame_set(68)
lines.extend(['}};',f'constexpr std::array<Plant, {len(plants)}> Plants = {{{{'])
for obj in plants:
    x,y=obj.location[:2];idx=meshes.index(obj.data)
    delay=.6+math.hypot(x,y)*.29
    lines.append('    {'+', '.join(num(x) for x in (x/2.25,y/2.25,obj.scale.x,delay))+f', {idx}'+'},')
lines.extend(['}};','} // namespace StagMeadowMesh',''])
(ROOT/'SimpleGame/StagMeadowMesh.h').write_text('\n'.join(lines),encoding='utf-8')
print('MEADOW_EXPORTED',len(vertices),len(plants),len(meshes),ROOT)
