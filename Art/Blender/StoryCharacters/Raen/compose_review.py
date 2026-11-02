"""Compose labelled raen model views and stationary animation previews."""

import json
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
FONT = 'C:/Windows/Fonts/malgun.ttf'
title, label, note = [ImageFont.truetype(FONT, size) for size in (30, 22, 16)]
canvas = Image.new('RGBA', (1640, 1510), '#17212a')
draw = ImageDraw.Draw(canvas)
draw.text((30, 20), '라엔 · 은발 주술사', font=title, fill='#f3e3c6')
draw.text((32, 69), '긴 은발 · 적갈색 목도리 · 짙은 긴 외투와 봉인 부적', font=note, fill='#b8c3c6')
for i, (key, text) in enumerate([('quarter', '전체 인상'), ('front', '정면과 비율'),
                                ('profile', '옆모습과 외투'), ('back', '뒷모습과 겹옷')]):
    x, y = (i%2)*820, 110+(i//2)*690
    draw.rounded_rectangle((x+12, y, x+808, y+673), 18, fill='#23313a')
    image = Image.open(HERE/'Previews'/(key+'.png')).convert('RGBA')
    image.thumbnail((750, 610), Image.Resampling.LANCZOS)
    canvas.alpha_composite(image, (x+(820-image.width)//2, y+10))
    draw.text((x+28, y+622), text, font=label, fill='#f7ecd9')
canvas.convert('RGB').save(HERE/'raen_review.png')
portrait = Image.new('RGBA', (860, 930), '#23313a')
portrait.alpha_composite(Image.open(HERE/'Previews/portrait.png').convert('RGBA'), (30, 45))
draw = ImageDraw.Draw(portrait)
draw.text((28, 18), '라엔 · 얼굴과 은발', font=label, fill='#f7ecd9')
draw.text((30, 870), '실제 모델 렌더 · 게임 실행 화면 아님', font=note, fill='#b8c3c6')
portrait.convert('RGB').save(HERE/'raen_portrait.png')
if '--motion' in sys.argv:
    meta = json.loads((HERE/'character.json').read_text())
    folder = HERE/'Previews'/'Motion'
    for clip, step, text in [('idle', 8, '제자리 대기'), ('focus', 3, '주력에 집중'),
                             ('greet', 3, '상대에게 고개 인사')]:
        frames = []
        for number in range(1, meta['clips'][clip]['lastFrame']+1, step):
            frame = Image.new('RGBA', (440, 600), '#23313a')
            frame.alpha_composite(Image.open(folder/f'{clip}_{number:03}.png').convert('RGBA'), (0, 30))
            draw = ImageDraw.Draw(frame)
            draw.text((16, 8), text, font=label, fill='#f3e3c6')
            draw.text((16, 573), '실제 키프레임 렌더 · 게임 화면 아님', font=note, fill='#b8c3c6')
            frames.append(frame.convert('RGB'))
        frames = frames[:-1]
        frames[0].save(HERE/f'raen_{clip}.gif', save_all=True, append_images=frames[1:],
                       duration=round(1000*step/24), loop=0, disposal=2)
print('RAEN_REVIEW_READY')
