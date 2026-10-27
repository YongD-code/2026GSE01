"""Create small GIF previews and a labelled contact sheet from Blender renders."""

import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
FOLDER = HERE/'Previews'/'Motion'
FONT = 'C:/Windows/Fonts/malgun.ttf'
label = ImageFont.truetype(FONT, 21)
small = ImageFont.truetype(FONT, 16)
meta = json.loads((HERE/'character.json').read_text())
for clip, step, text in [('idle', 4, '제자리 대기'), ('work', 3, '망치질 · 두 번 타격'),
                         ('greet', 3, '손님 반응 · 고개 인사')]:
    frames = []
    for number in range(1, meta['clips'][clip]['lastFrame']+1, step):
        frame = Image.new('RGBA', (480, 570), '#23313a')
        frame.alpha_composite(Image.open(FOLDER/f'{clip}_{number:03}.png').convert('RGBA'), (0, 35))
        draw = ImageDraw.Draw(frame)
        draw.text((16, 10), text, font=label, fill='#f3e3c6')
        draw.text((16, 542), '실제 3D 키프레임 미리보기 · 게임 적용 전', font=small, fill='#b8c3c6')
        frames.append(frame.convert('RGB'))
    # Endpoints duplicate the first pose. Drop the duplicate for a steady GIF loop.
    frames = frames[:-1]
    frames[0].save(HERE/f'blacksmith_{clip}.gif', save_all=True, append_images=frames[1:],
                   duration=round(1000*step/24), loop=0, disposal=2)

sheet = Image.new('RGBA', (1440, 650), '#17212a')
draw = ImageDraw.Draw(sheet)
draw.text((20, 12), '대장장이 · 고정 위치용 키프레임 애니메이션', font=label, fill='#f3e3c6')
for i, (number, text) in enumerate([(31, '들어 올리기'), (39, '모루에 타격'), (43, '반동과 회복')]):
    sheet.alpha_composite(Image.open(FOLDER/f'work_{number:03}.png').convert('RGBA'), (i*480, 55))
    draw.text((i*480+20, 587), text, font=label, fill='#f3e3c6')
draw.text((20, 625), '발 고정 · 이동 클립 없음 · 게임용 모루 메시 포함', font=small, fill='#b8c3c6')
sheet.convert('RGB').save(HERE/'blacksmith_motion_review.png')
print('MOTION_REVIEW_READY')
