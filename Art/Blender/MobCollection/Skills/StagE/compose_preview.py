"""Assemble three authored Blender poses for review."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

here = Path(__file__).resolve().parent
font = 'C:/Windows/Fonts/malgun.ttf'
board = Image.new('RGB', (1800, 645), '#18231c')
draw = ImageDraw.Draw(board)
draw.text((22, 14), 'E · 꽃길 도약 — 동작 검토안', fill='#f4ead2',
          font=ImageFont.truetype(font, 32))
for i, (name, title, detail) in enumerate([
    ('prepare', '01 · 도약 직전', '몸을 낮춰 네 발로 지면을 딛는다.'),
    ('airborne', '02 · 공중', '앞뒤 발을 몸 아래로 접는다.'),
    ('landing', '03 · 착지', '앞발이 닿으면 잎과 꽃잎이 낮게 흩어진다.'),
]):
    image = Image.open(here/(name+'.png')).convert('RGB')
    image.thumbnail((590, 485), Image.Resampling.LANCZOS)
    board.paste(image, (i*600+5, 65))
    draw.text((i*600+19, 555), title, fill='#f4ead2', font=ImageFont.truetype(font, 24))
    draw.text((i*600+19, 594), detail, fill='#bfcbb6', font=ImageFont.truetype(font, 18))
board.save(here/'stag_e_stages.png')
