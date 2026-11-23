"""Render the authored Q pose and bone geometry; this is not a game screenshot."""
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
FRAMES = HERE.parents[3] / 'tmp' / 'skeleton_q_review'


def render():
    import bpy
    from mathutils import Vector
    sys.path.insert(0, str(HERE.parent))
    import render_previews as review
    bpy.ops.wm.open_mainfile(filepath=str(HERE / 'model.blend'))
    rig = bpy.data.objects['MOB_SKELETON_RIG']
    rig.animation_data.action = bpy.data.actions['MOB_SKELETON_Q_SHARDS']
    rig.animation_data.action_slot = rig.animation_data.action.slots[0]
    with bpy.data.libraries.load(str(HERE/'q_shards/model.blend'), link=False) as (source, dest):
        dest.objects = [name for name in source.objects if name.startswith('MOB_SKELETON_Q_SHARD_')]
    shards = sorted(dest.objects, key=lambda obj: obj.name)
    for obj in shards:
        bpy.context.scene.collection.objects.link(obj)
    scene = bpy.context.scene
    review.set_stage(1.7, 4.9)
    scene.render.resolution_x, scene.render.resolution_y = 510, 570
    scene.eevee.taa_render_samples = 16
    center = Vector((0, -.25, 1.70))
    scene.camera.location = center + Vector((4, -8, 1.9))
    scene.camera.rotation_euler = (center-scene.camera.location).to_track_quat('-Z','Y').to_euler()

    def place(age):
        scene.frame_set(1 + int(min(1., age)*24), subframe=(min(1., age)*24) % 1)
        progress = max(0., min(1., (age-.4)/.46))
        for index, obj in enumerate(shards):
            angle = (index-1)*.13
            forward = 20 + 260*progress
            side = (index-1)*6*(1-progress)
            obj.location = ((math.sin(angle)*forward + side)/26,
                            -math.cos(angle)*forward/26,
                            (55+(index-1)*5*(1-progress))/26)
            obj.rotation_euler = (0,0,-math.pi/2+angle)
            scale = (.55+.45*min(1., age/.4))/26
            obj.scale = (scale, scale, scale)
            obj.hide_render = age <= 0 or age >= .86

    for index, age in enumerate((.0, .25, .4, .62)):
        place(age)
        if index == 3:
            center = Vector((0, -5, 2.1))
            scene.camera.location = center + Vector((5,-8,3))
            scene.camera.rotation_euler = (center-scene.camera.location).to_track_quat('-Z','Y').to_euler()
            scene.camera.data.ortho_scale = 4.3
            for obj in scene.objects:
                if obj.type == 'MESH' and obj not in shards:
                    obj.hide_render = True
        review.render(FRAMES / f'pose_{index}.png')

    if '--stills-only' in sys.argv:
        return
    for obj in scene.objects:
        if obj.type == 'MESH' and obj not in shards:
            obj.hide_render = False
    center = Vector((0, -4.5, 1.65))
    scene.camera.location = center + Vector((10,-5,7))
    scene.camera.rotation_euler = (center-scene.camera.location).to_track_quat('-Z','Y').to_euler()
    scene.camera.data.ortho_scale = 13.0
    scene.render.resolution_x, scene.render.resolution_y = 900, 440
    for index in range(16):
        place(index/15)
        review.render(FRAMES / f'motion_{index:02}.png')
    print('SKELETON_Q_PREVIEW_READY', flush=True)


def compose():
    from PIL import Image, ImageDraw, ImageFont
    font = ImageFont.truetype('C:/Windows/Fonts/malgun.ttf', 25)
    small = ImageFont.truetype('C:/Windows/Fonts/malgun.ttf', 18)
    board = Image.new('RGB', (1440, 505), '#18242b')
    draw = ImageDraw.Draw(board)
    draw.text((22,15), '해골형 Q · 골편 사출 / 제작 모델·키프레임 검토', font=font, fill='#e8dcc4')
    for index, label in enumerate(('준비', '가슴 앞 응축', '손을 펴며 사출', '골편 비행')):
        with Image.open(FRAMES/f'pose_{index}.png') as source:
            image = source.convert('RGBA')
        image.thumbnail((350, 396), Image.Resampling.LANCZOS)
        x = index*360
        draw.rounded_rectangle((x+5,58,x+355,458),radius=8,fill='#2a383f')
        board.paste(image,(x+(360-image.width)//2,60),image)
        draw.text((x+20,468),label,font=small,fill='#e8dcc4')
    board.save(HERE/'skeleton_q_review.png')
    frames = []
    for path in sorted(FRAMES.glob('motion_*.png')):
        with Image.open(path) as source:
            frame = Image.new('RGB', source.size, '#25343c')
            frame.paste(source.convert('RGBA'),(0,0),source.convert('RGBA'))
        frames.append(frame)
    frames[0].save(HERE/'skeleton_q.gif',save_all=True,append_images=frames[1:]+[frames[-1]]*4,
                   duration=67,loop=0,disposal=2)


if __name__ == '__main__':
    compose() if '--compose' in sys.argv else render()
