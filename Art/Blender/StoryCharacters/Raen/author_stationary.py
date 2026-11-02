"""Bake a stationary sorceress breathing, focusing and attentive greeting."""
import importlib.util
import math
from pathlib import Path

import bpy
from mathutils import Quaternion, Vector

spec = importlib.util.spec_from_file_location(
    'shop_motion_helper', Path(__file__).resolve().parent.parent/'Merchant'/'author_stationary.py')
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)
REST_L, REST_R = helper.REST_L, helper.REST_R
CLIPS = {'idle': (96, True), 'focus': (120, True), 'greet': (72, False)}


def solve_seal_arm(rig, wrist, delta):
    arm, forearm = rig.pose.bones['arm.R'], rig.pose.bones['hand.R']
    shoulder = arm.head.copy()
    reach = Vector(wrist)-shoulder
    assert reach.length <= arm.bone.length+forearm.bone.length+.0001
    distance = min(reach.length,arm.bone.length+forearm.bone.length-.0001)
    direction = reach.normalized()
    along = (arm.bone.length**2-forearm.bone.length**2+distance**2)/(2*distance)
    height = math.sqrt(max(0.,arm.bone.length**2-along**2))
    pole = Vector((.22,.12,-1.))
    pole = (pole-direction*pole.dot(direction)).normalized()
    elbow = shoulder+direction*along+pole*height
    for name, aim in [('arm.R',elbow-shoulder),('hand.R',Vector(wrist)-elbow)]:
        rest = rig.data.bones[name]
        rotation = (rest.tail_local-rest.head_local).rotation_difference(aim)
        helper.global_rotation(rig,name,rotation @ rest.matrix_local.to_quaternion())
    helper.global_rotation(rig,'wrist.R',delta @ rig.data.bones['wrist.R'].matrix_local.to_quaternion())


def pose(rig, clip, frame, length):
    for bone in rig.pose.bones:
        bone.rotation_mode = 'QUATERNION'
        bone.rotation_quaternion = Quaternion()
        bone.location = (0, 0, 0)
        bone.scale = (1, 1, 1)
    phase = math.tau*frame/length
    left, right = REST_L.copy(), REST_R.copy()
    bend, nod, glance = .007*math.sin(phase), -.006*math.sin(phase), .015*math.sin(phase)
    qr = Quaternion()
    seal = 0.
    if clip == 'focus':
        active = helper.smooth(frame/26)*(1-helper.smooth((frame-96)/24))
        right = REST_R.lerp(Vector((.22, -.34, 1.45)), active)
        qr = Quaternion().slerp(Quaternion((0,1,0), math.pi-.12), active)
        seal = helper.smooth((frame-8)/24)*(1-helper.smooth((frame-90)/22))
        bend += .012*active
        nod += .035*active
    elif clip == 'greet':
        active = math.sin(math.pi*frame/length)**2
        bend += .025*active
        nod += .07*active
        glance -= .06*active
    rig.pose.bones['spine'].rotation_quaternion = Quaternion((1, 0, 0), bend)
    rig.pose.bones['head'].rotation_quaternion = Quaternion((0, 1, 0), glance) @ Quaternion((1, 0, 0), nod)
    for side, sign in [('L', -1), ('R', 1)]:
        rig.pose.bones['hair.'+side].rotation_quaternion = (
            Quaternion((1, 0, 0), -.35*nod+.010*math.sin(phase+sign*.2)) @
            Quaternion((0, 1, 0), sign*.008*math.sin(phase)))
    bpy.context.view_layer.update()
    helper.solve_arm(rig, 'L', left, Quaternion())
    solve_seal_arm(rig, right, qr)
    for j in range(5):
        for segment in range(2):
            bone = rig.pose.bones[f'finger_{j}_{segment}.R']
            rest = bone.bone.matrix_local.to_quaternion()
            if j < 2:
                # Opposing lateral angles form an X; depth separates the crossing.
                angle = (-.34 if j == 0 else .30) if segment == 0 else (-.10 if j == 0 else .04)
                curl = (-.40 if j == 0 else .40) if segment == 0 else 0.
            elif j < 4:
                angle, curl = 0., (1.25 if segment == 0 else 1.5)
            else:
                angle, curl = -.45, .75 if segment == 0 else .65
            delta = Quaternion((0,1,0), angle*seal) @ Quaternion((1,0,0),curl*seal)
            bone.rotation_quaternion = rest.inverted() @ delta @ rest



def author_clips(rig):
    scene = bpy.context.scene
    scene.render.fps = 24
    rig.animation_data_create()
    result = {}
    for clip, (length, loop) in CLIPS.items():
        rig.animation_data.action = None
        for frame in range(length+1):
            pose(rig, clip, frame, length)
            for bone in rig.pose.bones:
                bone.keyframe_insert('rotation_quaternion', frame=frame+1, group=bone.name)
        action = rig.animation_data.action
        action.name = 'NPC_RAEN_'+clip.upper()
        action.use_fake_user = True
        action['loop'], action['fps'], action['stationary'] = loop, 24, True
        result[clip] = {'action': action.name, 'firstFrame': 1, 'lastFrame': length+1,
                        'fps': 24, 'loop': loop, 'rootMotion': False}
    rig.animation_data.action = None
    for bone in rig.pose.bones:
        bone.rotation_quaternion = Quaternion()
    scene.frame_start, scene.frame_end = 1, 121
    return result
