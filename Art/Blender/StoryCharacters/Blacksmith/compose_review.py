"""Label the real Blender renders for a first character-design discussion."""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
FONT = 'C:/Windows/Fonts/malgun.ttf'
title = ImageFont.truetype(FONT, 32)
label = ImageFont.truetype(FONT, 23)
note = ImageFont.truetype(FONT, 17)
canvas = Image.new('RGBA', (1640, 1510), '#17212a')
draw = ImageDraw.Draw(canvas)
draw.text((30, 20), '대장장이 · 호탕한 드워프 / 외형 1차안', font=title, fill='#f3e3c6')
draw.text((32, 69), '전용 얼굴과 체형 · 덥수룩한 수염 · 큰 손 · 그을린 가죽 작업복', font=note, fill='#b8c3c6')
items = [('quarter', '전체 인상'), ('front', '정면과 체격'),
         ('profile', '옆모습과 장비'), ('back', '등과 작업복')]
for i, (key, text) in enumerate(items):
    x, y = (i%2)*820, 110+(i//2)*690
    draw.rounded_rectangle((x+12, y, x+808, y+673), 18, fill='#23313a')
    image = Image.open(HERE/'Previews'/(key+'.png')).convert('RGBA')
    image.thumbnail((750, 610), Image.Resampling.LANCZOS)
    canvas.alpha_composite(image, (x+(820-image.width)//2, y+10))
    draw.text((x+28, y+622), text, font=label, fill='#f7ecd9')
canvas.convert('RGB').save(HERE/'blacksmith_review.png')
portrait = Image.new('RGBA', (860, 930), '#23313a')
portrait.alpha_composite(Image.open(HERE/'Previews/portrait.png').convert('RGBA'), (30, 45))
draw = ImageDraw.Draw(portrait)
draw.text((28, 18), '대장장이 · 얼굴과 덥수룩한 수염', font=label, fill='#f7ecd9')
draw.text((30, 870), '외형 검토용 3D 렌더 · 게임 적용 전', font=note, fill='#b8c3c6')
portrait.convert('RGB').save(HERE/'blacksmith_portrait.png')
