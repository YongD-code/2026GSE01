"""Bake planted-foot idle, forging and customer greeting into ordinary keyframes."""

import math

import bpy
from mathutils import Matrix, Quaternion, Vector


FPS = 24
CLIPS = {
    'idle': {'action': 'NPC_SMITH_IDLE', 'frames': 96, 'loop': True},
    'work': {'action': 'NPC_SMITH_FORGE', 'frames': 120, 'loop': True},
    'greet': {'action': 'NPC_SMITH_GREET', 'frames': 72, 'loop': False},
}
# X is the hammer's striking axis; its handle is Z in the bind mesh.
# At contact, X points down and the handle points from the anvil toward the smith.
CONTACT_ROTATION = Matrix(((0, -1, 0), (0, 0, 1), (-1, 0, 0))).to_quaternion()
WORK_KEYS = [
    (0, (.67, -.06, .795), 0., 0.),
    (16, (.66, -.22, 1.18), -.65, 1.),
    (30, (.66, -.14, 1.53), -2.05, 1.),
    (38, (.66, -.36, 1.01), 0., 1.),
    (42, (.66, -.33, 1.11), -.22, 1.),
    (62, (.66, -.14, 1.53), -2.05, 1.),
    (70, (.66, -.36, 1.01), 0., 1.),
    (74, (.66, -.33, 1.11), -.22, 1.),
    (95, (.67, -.06, .795), 0., 0.),
    (120, (.67, -.06, .795), 0., 0.),
]


def global_rotation(rig, name, rotation):
    """Set a bone's global orientation while retaining its parent-relative rest basis."""
    bone = rig.pose.bones[name]
    base = bone.bone.matrix_local.copy()
    if bone.parent:
        base = bone.parent.matrix @ bone.parent.bone.matrix_local.inverted() @ base
    bone.rotation_quaternion = base.to_quaternion().inverted() @ rotation
    bpy.context.view_layer.update()


def solve_arm(rig, side, wrist, delta):
    arm, forearm = rig.pose.bones['arm.'+side], rig.pose.bones['hand.'+side]
    shoulder = arm.head.copy()
    reach = Vector(wrist)-shoulder
    distance = min(reach.length, arm.bone.length+forearm.bone.length-.0001)
    direction = reach.normalized()
    along = (arm.bone.length**2-forearm.bone.length**2+distance**2)/(2*distance)
    height = math.sqrt(max(0., arm.bone.length**2-along**2))
    # Fixed outward elbow pole prevents elbow flips throughout the hammer arc.
    pole = Vector((-1 if side == 'L' else 1, .32, -.08))
    pole = (pole-direction*pole.dot(direction)).normalized()
    elbow = shoulder+direction*along+pole*height
    for name, aim in [('arm.'+side, elbow-shoulder), ('hand.'+side, Vector(wrist)-elbow)]:
        rest = rig.data.bones[name]
        swing = (rest.tail_local-rest.head_local).rotation_difference(aim)
        global_rotation(rig, name, swing @ rest.matrix_local.to_quaternion())
    rest = rig.data.bones['wrist.'+side].matrix_local.to_quaternion()
    global_rotation(rig, 'wrist.'+side, delta @ rest)


def sample_work(frame):
    for first, last in zip(WORK_KEYS, WORK_KEYS[1:]):
        if first[0] <= frame <= last[0]:
            t = (frame-first[0])/(last[0]-first[0])
            # The eight-frame downswing accelerates to the impact. Slow recovery
            # and raising segments ease smoothly; the impact never overshoots.
            impact = last[0] in (38, 70)
            blend = t*t if impact else t*t*(3-2*t)
            wrist = Vector(first[1]).lerp(Vector(last[1]), blend)
            angle = first[2]+(last[2]-first[2])*blend
            engaged = first[3]+(last[3]-first[3])*blend
            working = Quaternion((1, 0, 0), angle) @ CONTACT_ROTATION
            return wrist, Quaternion().slerp(working, engaged), engaged
    raise ValueError(frame)


def pose(rig, clip, frame, length):
    for bone in rig.pose.bones:
        bone.rotation_mode = 'QUATERNION'
        bone.rotation_quaternion = Quaternion()
        bone.location = (0, 0, 0)
        bone.scale = (1, 1, 1)
    phase = math.tau*frame/length
    right, left = Vector((.67, -.06, .795)), Vector((-.57, -.06, .795))
    right_rotation = Quaternion()
    bend, nod, glance = .008*math.sin(phase), -.006*math.sin(phase), 0.
    if clip == 'idle':
        right.y += .004*math.sin(phase)
        left.y -= .006*math.sin(phase)
        glance = .025*math.sin(phase)
    elif clip == 'work':
        right, right_rotation, engaged = sample_work(frame)
        bend += .05*engaged
        nod += .13*engaged
        # Empty left hand stays safely outside the hammer/anvil workspace.
        left += Vector((-.055, -.10, .09))*engaged
    else:
        attend = math.sin(math.pi*frame/length)**2
        left += Vector((-.035, -.13, .16))*attend
        right += Vector((.015, .015, .025))*attend
        glance = -.16*attend
        nod += .12*attend*math.sin(phase)
        bend += .012*attend
    rig.pose.bones['spine'].rotation_quaternion = Quaternion((1, 0, 0), bend)
    rig.pose.bones['head'].rotation_quaternion = (
        Quaternion((0, 1, 0), glance) @ Quaternion((1, 0, 0), nod))
    bpy.context.view_layer.update()
    solve_arm(rig, 'L', left, Quaternion())
    solve_arm(rig, 'R', right, right_rotation)


def author_clips(rig):
    scene = bpy.context.scene
    scene.render.fps = FPS
    rig.animation_data_create()
    result = {}
    for clip, spec in CLIPS.items():
        rig.animation_data.action = None
        for frame in range(spec['frames']+1):
            pose(rig, clip, frame, spec['frames'])
            for bone in rig.pose.bones:
                bone.keyframe_insert('rotation_quaternion', frame=frame+1, group=bone.name)
        action = rig.animation_data.action
        action.name = spec['action']
        action.use_fake_user = True
        action['loop'] = spec['loop']
        action['fps'] = FPS
        action['stationary'] = True
        result[clip] = {'action': action.name, 'firstFrame': 1,
                        'lastFrame': spec['frames']+1, 'fps': FPS,
                        'loop': spec['loop'], 'rootMotion': False}
    rig.animation_data.action = None
    for bone in rig.pose.bones:
        bone.rotation_quaternion = Quaternion()
    scene.frame_start, scene.frame_end = 1, 121
    rig['stationary'] = True
    rig['locomotionClips'] = ''
    return result
