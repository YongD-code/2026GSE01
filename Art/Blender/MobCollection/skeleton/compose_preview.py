"""Make the review board from the Blender renders."""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

here = Path(__file__).resolve().parent
preview = here/'Previews'
font_name = 'C:/Windows/Fonts/malgun.ttf'
font = ImageFont.truetype(font_name, 23)
small = ImageFont.truetype(font_name, 16)
title = ImageFont.truetype(font_name, 34)
rig_review = '--rig-review' in sys.argv
board = Image.new('RGBA', (1390 if rig_review else 1660, 832), '#151b20')
draw = ImageDraw.Draw(board)
draw.text((28, 20), '해골형 주령', font=title, fill='#f1e3c9')
subtitle = 'Blender 리깅 · 검토 자세' if rig_review else 'Blender 모델 · 외형 검토'
draw.text((30, 69), subtitle, font=small, fill='#adbac2')

def place(key, label, x, y, w, h, limit):
    draw.rounded_rectangle((x, y, x+w, y+h), radius=13, fill='#243039')
    model = Image.open(preview/f'{key}.png').convert('RGBA')
    model = model.crop(model.getbbox())
    model.thumbnail(limit, Image.Resampling.LANCZOS)
    board.alpha_composite(model, (x+(w-model.width)//2, y+(h-36-model.height)//2))
    draw.text((x+18, y+h-34), label, font=font, fill='#e6d9bd')

if rig_review:
    place('rig_rest', '기본 자세', 20, 110, 440, 695, (402, 620))
    place('rig_reach', '팔 뻗기와 턱 열기', 475, 110, 440, 695, (402, 620))
    place('rig_crouch', '몸 낮추기', 930, 110, 440, 695, (402, 620))
    output = 'skeleton_rig_review.png'
else:
    place('quarter', '전체', 20, 110, 350, 695, (320, 620))
    place('front', '정면', 385, 110, 285, 695, (250, 620))
    place('side', '측면', 685, 110, 260, 695, (225, 620))
    place('back', '후면', 960, 110, 285, 695, (250, 620))
    place('skull', '두개골 확대', 1260, 110, 380, 695, (344, 605))
    output = 'skeleton_review.png'
board.convert('RGB').save(here/output)
