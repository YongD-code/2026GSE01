"""Render Blender clips to temporary frames and assemble review images with Pillow."""
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
FRAMES = HERE.parents[3] / 'tmp' / 'skeleton_motion_review'
META = json.loads((HERE/'model.json').read_text(encoding='utf-8'))
CLIPS = META['clips']
LABELS = {'idle':'대기', 'walk':'걷기', 'run':'달리기', 'cast':'시전',
          'jump':'점프', 'turn_left':'좌회전', 'turn_right':'우회전', 'attack':'팔 휘두르기'}


def render():
    import bpy
    from mathutils import Vector

    sys.path.insert(0, str(HERE.parent))
    import render_previews

    bpy.ops.wm.open_mainfile(filepath=str(HERE/'model.blend'))
    rig = bpy.data.objects[META['rig']]
    scene = bpy.context.scene
    render_previews.set_stage(1.7, 5.0)
    scene.render.resolution_x, scene.render.resolution_y = 460, 520
    scene.eevee.taa_render_samples = 20
    feet = [bpy.data.objects['MOB_SKELETON_FOOT_'+side] for side in ('R', 'L')]
    floor = min((ob.matrix_world @ vert.co).z for ob in feet for vert in ob.data.vertices)
    worst = 0.
    for key, clip in CLIPS.items():
        action = bpy.data.actions[clip['action']]
        rig.animation_data.action = action
        rig.animation_data.action_slot = action.slots[0]
        camera_target = Vector((0, -.16, 1.80))
        scene.camera.location = camera_target + Vector((4, -8, 1.9))
        scene.camera.rotation_euler = (camera_target - scene.camera.location).to_track_quat('-Z', 'Y').to_euler()
        for frame in range(clip['firstFrame'], clip['lastFrame']+1):
            scene.frame_set(frame)
            graph = bpy.context.evaluated_depsgraph_get()
            ground = min((ob.matrix_world @ vert.co).z
                         for base in feet for ob in [base.evaluated_get(graph)]
                         for vert in ob.data.vertices)
            assert ground > floor - .006, (key, frame, ground, floor)
            if key in ('idle', 'walk', 'attack', 'cast', 'turn_left', 'turn_right'):
                worst = max(worst, abs(ground - floor))
            if frame % 2:
                render_previews.render(FRAMES/key/f'{frame:03}.png')
    assert worst < .008, worst
    print('SKELETON_PREVIEW_GROUND_OK', worst, flush=True)


def compose():
    from PIL import Image, ImageDraw, ImageFont

    font_path = 'C:/Windows/Fonts/malgun.ttf'
    title = ImageFont.truetype(font_path, 29)
    label = ImageFont.truetype(font_path, 21)
    background = '#151d23'
    card = '#253239'
    ivory = '#ece0ca'
    board = Image.new('RGB', (1520, 935), background)
    draw = ImageDraw.Draw(board)
    draw.text((23, 18), '해골형 주령 · 게임용 동작', font=title, fill=ivory)
    fractions = {'idle':.27, 'walk':.36, 'run':.40, 'cast':.35,
                 'jump':.50, 'turn_left':.28, 'turn_right':.66, 'attack':.51}
    frames = {}
    for index, (key, clip) in enumerate(CLIPS.items()):
        paths = sorted((FRAMES/key).glob('*.png'))
        assert len(paths) == (clip['lastFrame'] + 1)//2, (key, len(paths))
        frames[key] = paths[:-1] if clip['loop'] else paths
        x, y = 380 * (index % 4), 70 + 430 * (index // 4)
        draw.rounded_rectangle((x+8, y, x+372, y+415), radius=10, fill=card)
        with Image.open(paths[round(fractions[key] * (len(paths)-1))]) as src:
            model = src.convert('RGBA')
        model.thumbnail((357, 375), Image.Resampling.LANCZOS)
        board.paste(model, (x+(380-model.width)//2, y+4), model)
        draw.text((x+23, y+379), LABELS[key], font=label, fill=ivory)
    board.save(HERE/'skeleton_motion_review.png')

    width, height = 340, 375
    for suffix, keys in [('gait', ('walk','run')), ('combat', ('attack','jump'))]:
        count = max(len(frames[key]) for key in keys)
        result = []
        for i in range(count + (0 if suffix == 'gait' else 5)):
            image = Image.new('RGB', (width*2, height+38), background)
            draw = ImageDraw.Draw(image)
            for column, key in enumerate(keys):
                paths = frames[key]
                sample = i % len(paths) if CLIPS[key]['loop'] else min(i,len(paths)-1)
                with Image.open(paths[sample]) as src:
                    model = src.convert('RGBA')
                model.thumbnail((width, height), Image.Resampling.LANCZOS)
                x = width * column
                draw.rectangle((x+3, 4, x+width-4, height), fill=card)
                image.paste(model, (x+(width-model.width)//2, 1), model)
                draw.text((x+15, height+4), LABELS[key], font=label, fill=ivory)
            result.append(image)
        result[0].save(HERE/f'skeleton_{suffix}.gif', save_all=True,
                       append_images=result[1:], duration=83, loop=0, disposal=2)


if __name__ == '__main__':
    if '--compose' in sys.argv:
        compose()
    else:
        render()
