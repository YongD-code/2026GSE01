"""Compare side-view footwork against the preserved previous sprites."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

here = Path(__file__).resolve().parent
old = here.parent / 'CharacterVersions/BeforeFullMovement'
font = ImageFont.truetype('C:/Windows/Fonts/malgun.ttf', 20)
frames = []
atlases = {(kind, version): Image.open(folder / (kind + '.png')).convert('RGBA')
           for kind in ['walk', 'run'] for version, folder in [('이전', old), ('수정', here/'Sprites')]}
for frame in range(16):
    canvas = Image.new('RGBA', (960, 340), (31,36,48,255))
    draw = ImageDraw.Draw(canvas)
    for index, (kind, version) in enumerate([('walk','이전'),('walk','수정'),('run','이전'),('run','수정')]):
        atlas = atlases[kind, version]
        count = atlas.width // 192 - 1
        col = int(frame * count / 16) + 1
        sprite = atlas.crop((col*192,2*192,(col+1)*192,3*192))
        canvas.alpha_composite(sprite.resize((240,240), Image.Resampling.NEAREST), (index*240,55))
        draw.text((index*240+50,16), ('걷기' if kind=='walk' else '달리기')+' · '+version, font=font, fill='white')
    frames.append(canvas.convert('RGB'))
frames[0].save(here/'movement_comparison.gif', save_all=True, append_images=frames[1:],duration=60,loop=0)
sheet=Image.new('RGB',(960,340*4),(31,36,48))
for row,index in enumerate([0,4,8,12]):
    sheet.paste(frames[index],(0,row*340))
sheet.save(here/'movement_poses.png')
