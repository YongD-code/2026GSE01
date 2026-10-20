"""Combine generated review renders into two labeled contact sheets."""

import json
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


HERE = Path(__file__).resolve().parent
NPC = HERE.parent / 'ModularCollection'
mob_catalog = json.loads((HERE/'mob_catalog.json').read_text(encoding='utf-8'))
npc_catalog = json.loads((NPC/'catalog.json').read_text(encoding='utf-8'))
FONT = Path('C:/Windows/Fonts/malgun.ttf')
title_font = ImageFont.truetype(str(FONT), 30)
label_font = ImageFont.truetype(str(FONT), 21)
small_font = ImageFont.truetype(str(FONT), 15)


def board(items, source, target, title, note, columns=4):
    cell_w, cell_h = 440, 580
    rows = math.ceil(len(items)/columns)
    image = Image.new('RGBA', (columns*cell_w, 100+rows*cell_h), (24, 34, 44, 255))
    draw = ImageDraw.Draw(image)
    draw.text((24, 17), title, font=title_font, fill='#f5ebd6')
    draw.text((24, 62), note, font=small_font, fill='#b9c8cc')
    for i, (key, label, detail) in enumerate(items):
        x, y = (i%columns)*cell_w, 100+(i//columns)*cell_h
        draw.rounded_rectangle((x+7, y+6, x+cell_w-7, y+cell_h-8),
                               radius=17, fill=(35, 48, 58, 255))
        preview = Image.open(source/(key+'.png')).convert('RGBA')
        image.alpha_composite(preview, (x, y+14))
        draw.text((x+20, y+471), label, font=label_font, fill='white')
        draw.text((x+20, y+515), detail, font=small_font, fill='#bfd0d2')
    image.convert('RGB').save(target, quality=95)


mob_items = []
terms = {'gentle': '선한 주령', 'dangerous': '위험한 주령', 'ambivalent': '중립·갈등 주령'}
gaits = {'hover': '부유형', 'biped': '2족 보행', 'quadruped': '4족 보행'}
for entry in mob_catalog['species']:
    mob_items.append((entry['key'], entry['name'],
                      terms[entry['disposition']]+' · '+gaits[entry['gait']]))
board(mob_items, HERE/'Previews', HERE/'spirit_overview.png',
      '주령 7종 · 3D 모델 미리보기', '기존 4종과 신규 3종 / 리그 원본의 대기 자세')

npc_items = []
for entry in npc_catalog['npcs']:
    npc_items.append((entry['id'], entry['label'], '조합형 NPC · 파츠 인덱스 보유'))
board(npc_items, NPC/'Previews', NPC/'npc_overview.png',
      '벨른 NPC 조합 미리보기', '남녀 몸 · 머리 · 의상 · 소품의 공용 파츠 조합')

prop_labels = {
    'BUILDING_HOME_00': '마을 집', 'BUILDING_SMITHY_01': '대장간',
    'BUILDING_SHOP_02': '상점', 'BUILDING_MONASTERY_RUIN_03': '폐수도원',
    'BUILDING_CEMETERY_GATE_04': '묘지 문', 'BUILDING_WELL_05': '우물',
    'NATURE_TREE_OAK_06': '참나무', 'NATURE_TREE_PINE_07': '침엽수',
    'NATURE_TREE_DEAD_08': '고목', 'NATURE_BUSH_09': '덤불',
    'NATURE_GRASS_10': '풀', 'NATURE_ROCK_11': '바위',
    'NATURE_STONES_12': '돌무더기', 'NATURE_STUMP_13': '그루터기',
}
prop_items = [(entry['mesh'].lower(), prop_labels[entry['mesh']],
               '건물' if entry['slot'] == 'building' else '자연물')
              for entry in npc_catalog['scenery']]
board(prop_items, NPC/'Previews', NPC/'environment_overview.png',
      '벨른 환경 3D 모델 미리보기', '건물 6종 · 자연물 8종')
