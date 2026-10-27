"""Draw authored village coordinates; this diagram is not a game screenshot."""

import itertools
import math
from pathlib import Path
import re
import subprocess

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
FONT = 'C:/Windows/Fonts/malgun.ttf'
title, label, note = [ImageFont.truetype(FONT, size) for size in (30, 21, 16)]
source = (ROOT/'SimpleGame/StoryLayout.h').read_text(encoding='utf-8')
previous = subprocess.check_output(['git', 'show', 'HEAD:SimpleGame/StoryLayout.h'], cwd=ROOT).decode('utf-8')
pattern = r'\{"([^"]+)", "([^"]+)", Kind::(\w+), \{(-?[\d.]+), (-?[\d.]+)\}, "([^"]+)", "([^"]*)"\}'


def sites(text):
    return {m[0]: {'name': m[1], 'kind': m[2], 'x': float(m[3]), 'y': float(m[4]),
                   'from': m[5], 'until': m[6]} for m in re.findall(pattern, text)}


before, after = sites(previous), sites(source)
building_pattern = r'\{0, (-?[\d.]+), (-?[\d.]+), ([\d.]+), ([\d.]+), ([\d.]+)\}'
village = (ROOT/'SimpleGame/VillageLayout.h').read_text(encoding='utf-8').split('return result;')[0]
buildings = [tuple(map(float, match)) for match in re.findall(building_pattern, source+village)]
visible = ['parent', 'merchant', 'child', 'sealer', 'smith', 'raen_hub']
colors = {'parent': '#b6b6df', 'merchant': '#dfbd77', 'child': '#a4ce9c',
          'sealer': '#8dcacb', 'smith': '#de9b71', 'raen_hub': '#b1a1da'}
image = Image.new('RGB', (1800, 840), '#17212a')
draw = ImageDraw.Draw(image)
draw.text((30, 18), '전용 NPC 배치 · 기존 / 수정', font=title, fill='#f1e4ce')
draw.text((32, 66), '거점 진입 이후 시점 · 소스 좌표 기준 도식 (게임 화면 아님) · 원은 대화 반경 62', font=note, fill='#b8c3c6')
for panel, (caption, points) in enumerate([('기존 · 서비스 NPC가 한 줄에 밀집', before), ('수정 · 건물과 작업 공간에 분산', after)]):
    ox, oy, scale = 22+panel*890, 155, .72
    def xy(x, y):
        return ox+40+(x+500)*scale, oy+(y+430)*scale
    draw.rounded_rectangle((ox, 110, ox+870, 760), 15, fill='#25343b')
    draw.text((ox+20, 121), caption, font=label, fill='#f6e8d1')
    def rectangle(x, y, w, d, color):
        draw.rectangle((*xy(x, y), *xy(x+w, y+d)), fill=color)
    rectangle(-500, -52, 1100, 104, '#43504d')
    rectangle(-52, -430, 104, 750, '#43504d')
    for x, y, w, d, h in buildings:
        if -500 <= x <= 600 and -430 <= y <= 280:
            rectangle(x, y, w, d, '#786f5b')
            rectangle(x+w*.4, y+d-4, w*.2, 4, '#c2ae80')
    for key in visible:
        point = points[key]
        x, y = xy(point['x'], point['y'])
        radius = 62*scale
        draw.ellipse((x-radius, y-radius, x+radius, y+radius), outline=colors[key], width=1)
    for index, key in enumerate(visible):
        point = points[key]
        x, y = xy(point['x'], point['y'])
        draw.ellipse((x-7, y-7, x+7, y+7), fill=colors[key])
        # Numbered markers keep the original overlapping cluster legible.
        draw.text((x+9, y-17), str(index+1), font=label, fill=colors[key])
        draw.text((ox+22+(index%3)*280, 665+(index//3)*36),
                  f'{index+1} {point["name"]} ({point["x"]:g}, {point["y"]:g})',
                  font=note, fill=colors[key])
    service = ['merchant', 'sealer', 'smith', 'raen_hub']
    closest = min(math.hypot(points[a]['x']-points[b]['x'], points[a]['y']-points[b]['y'])
                  for a, b in itertools.combinations(service, 2))
    draw.text((ox+22, 727), f'서비스·거점 NPC 사이 최소 간격: {closest:.0f}', font=note, fill='#d4dfdc')
draw.text((32, 794), '가족도 같은 집 앞에서 간격 조정 · 집 옆 라엔은 별도 이야기 시점에 등장 · 퀘스트 목적지 동기화', font=note, fill='#b8c3c6')
image.save(HERE/'npc_placement_review.png')
print('PLACEMENT_REVIEW_READY')
