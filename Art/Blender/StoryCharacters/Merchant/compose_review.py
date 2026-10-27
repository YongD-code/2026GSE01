"""Compose labelled body/portrait sheets and the actual keyed motion GIFs."""

import json
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
FONT = 'C:/Windows/Fonts/malgun.ttf'
title, label, note = [ImageFont.truetype(FONT, size) for size in (30, 22, 16)]
canvas = Image.new('RGBA', (1640, 1510), '#17212a')
draw = ImageDraw.Draw(canvas)
draw.text((30, 20), '잡화상 · 후덕한 중년 여성 점주', font=title, fill='#f3e3c6')
draw.text((32, 69), '전용 얼굴·체형 · 묶은 머리 · 빛바랜 앞치마와 열쇠 · 실제 모델 렌더', font=note, fill='#b8c3c6')
for i, (key, text) in enumerate([('quarter', '전체 인상'), ('front', '정면과 비율'),
                                ('profile', '옆모습과 묶은 머리'), ('back', '뒷모습과 작업복')]):
    x, y = (i%2)*820, 110+(i//2)*690
    draw.rounded_rectangle((x+12, y, x+808, y+673), 18, fill='#23313a')
    image = Image.open(HERE/'Previews'/(key+'.png')).convert('RGBA')
    image.thumbnail((750, 610), Image.Resampling.LANCZOS)
    canvas.alpha_composite(image, (x+(820-image.width)//2, y+10))
    draw.text((x+28, y+622), text, font=label, fill='#f7ecd9')
canvas.convert('RGB').save(HERE/'merchant_review.png')
portrait = Image.new('RGBA', (860, 930), '#23313a')
portrait.alpha_composite(Image.open(HERE/'Previews/portrait.png').convert('RGBA'), (30, 45))
draw = ImageDraw.Draw(portrait)
draw.text((28, 18), '잡화상 · 얼굴과 머리', font=label, fill='#f7ecd9')
draw.text((30, 870), '모델 외형 미리보기 · 게임 실행 화면 아님', font=note, fill='#b8c3c6')
portrait.convert('RGB').save(HERE/'merchant_portrait.png')
if '--motion' in sys.argv:
    meta = json.loads((HERE/'character.json').read_text())
    folder = HERE/'Previews'/'Motion'
    for clip, step, text in [('idle', 8, '제자리 대기'), ('sort', 3, '물건 정리'),
                             ('pack', 3, '꾸러미 포장'), ('greet', 3, '손님에게 손짓 인사')]:
        frames = []
        for number in range(1, meta['clips'][clip]['lastFrame']+1, step):
            frame = Image.new('RGBA', (440, 600), '#23313a')
            frame.alpha_composite(Image.open(folder/f'{clip}_{number:03}.png').convert('RGBA'), (0, 30))
            draw = ImageDraw.Draw(frame)
            draw.text((16, 8), text, font=label, fill='#f3e3c6')
            draw.text((16, 573), '실제 키프레임 렌더 · 게임 실행 화면 아님', font=note, fill='#b8c3c6')
            frames.append(frame.convert('RGB'))
        frames = frames[:-1]
        frames[0].save(HERE/f'merchant_{clip}.gif', save_all=True, append_images=frames[1:],
                       duration=round(1000*step/24), loop=0, disposal=2)
    sheet = Image.new('RGBA', (1320, 650), '#17212a')
    draw = ImageDraw.Draw(sheet)
    draw.text((20, 10), '잡화상 · 고정 위치용 작업·응대 동작', font=label, fill='#f3e3c6')
    for i, (key, text) in enumerate([('sort_055', '물건 정리'), ('pack_046', '꾸러미 포장'),
                                    ('greet_025', '손짓 인사')]):
        sheet.alpha_composite(Image.open(folder/(key+'.png')).convert('RGBA'), (i*440, 50))
        draw.text((i*440+20, 596), text, font=label, fill='#f3e3c6')
    draw.text((20, 627), '발 고정 · 이동 클립 없음 · 계산대와 물품 포함', font=note, fill='#b8c3c6')
    sheet.convert('RGB').save(HERE/'merchant_motion_review.png')
print('MERCHANT_REVIEW_READY')
