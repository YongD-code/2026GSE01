"""Bake a stationary ward-writing ritual, idle breathing and a restrained greeting."""

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
CLIPS = {'idle': (96, True), 'ritual': (144, True), 'greet': (72, False)}


def pose(rig, clip, frame, length):
    for bone in rig.pose.bones:
        bone.rotation_mode = 'QUATERNION'
        bone.rotation_quaternion = Quaternion()
        bone.location = (0, 0, 0)
        bone.scale = (1, 1, 1)
    phase = math.tau*frame/length
    left, right = REST_L.copy(), REST_R.copy()
    left_rotation = Quaternion()
    bend, nod = .006*math.sin(phase), -.005*math.sin(phase)
    if clip == 'ritual':
        right, engaged = helper.key_sample(frame, [
            (0, REST_R, 0), (22, (.30, -.35, 1.395), 1),
            (42, (.16, -.45, 1.395), 1), (56, (.23, -.46, 1.395), 1),
            (68, (.23, -.42, 1.395), 1), (84, (.30, -.46, 1.395), 1),
            (100, (.30, -.42, 1.395), 1), (112, (.19, -.45, 1.395), 1),
            (132, REST_R, 0), (144, REST_R, 0)])
        left = REST_L.lerp(Vector((-.12, -.36, 1.25)), engaged)
        left_rotation = Quaternion().slerp(helper.PALM_DOWN, engaged)
        bend += .035*engaged
        nod += .11*engaged
    elif clip == 'greet':
        t = frame/length
        attend = helper.smooth(t/.25)*(1-helper.smooth((t-.75)/.25))
        left = REST_L.lerp(Vector((-.38, -.18, 1.19)), attend)
        nod += .14*math.sin(math.pi*t)*attend
        bend += .018*attend
    rig.pose.bones['spine'].rotation_quaternion = Quaternion((1, 0, 0), bend)
    rig.pose.bones['head'].rotation_quaternion = Quaternion((1, 0, 0), nod)
    bpy.context.view_layer.update()
    helper.solve_arm(rig, 'L', left, left_rotation)
    helper.solve_arm(rig, 'R', right, Quaternion())


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
        action.name = 'NPC_SEALER_'+clip.upper()
        action.use_fake_user = True
        action['loop'], action['fps'], action['stationary'] = loop, 24, True
        result[clip] = {'action': action.name, 'firstFrame': 1, 'lastFrame': length+1,
                        'fps': 24, 'loop': loop, 'rootMotion': False}
    rig.animation_data.action = None
    for bone in rig.pose.bones:
        bone.rotation_quaternion = Quaternion()
    scene.frame_start, scene.frame_end = 1, 145
    return result
