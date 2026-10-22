"""Combine generated review renders into two labeled contact sheets."""

import json
import math
import sys
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
      '주령 7종 · 3D 모델 미리보기', '가면 · 의례 장식 · 해진 천 / 같은 화면 여백으로 비교')

npc_items = []
for entry in npc_catalog['npcs']:
    npc_items.append((entry['id'], entry['label'], '조합형 NPC · 파츠 인덱스 보유'))
board(npc_items, NPC/'Previews', NPC/'npc_overview.png',
      '벨른 NPC 조합 미리보기', '3등신 · 25개 공용 파츠 / 얼굴 · 헤어 흐름 · 재단선 수정')

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


if '--before-dir' in sys.argv:
    before = Path(sys.argv[sys.argv.index('--before-dir')+1])
    # Historical renders are comparison inputs only; all after images are actual 3D renders.
    def comparison(keys, labels, source, target, title):
        width = 440*len(keys)
        canvas = Image.new('RGBA', (width, 1260), (24, 34, 44, 255))
        draw = ImageDraw.Draw(canvas)
        draw.text((24, 16), title, font=title_font, fill='#f5ebd6')
        for row, folder in enumerate((before, source)):
            y = 100+row*580
            for col, (key, label) in enumerate(zip(keys, labels)):
                x = col*440
                draw.rounded_rectangle((x+7, y+6, x+433, y+572), 17, fill='#23303a')
                canvas.alpha_composite(Image.open(folder/(key+'.png')).convert('RGBA'), (x, y+14))
                draw.text((x+20, y+475), label, font=label_font, fill='white')
                draw.text((x+20, y+518), '수정 전' if row == 0 else '수정 후',
                          font=small_font, fill='#bfd0d2')
        canvas.convert('RGB').save(target)
    comparison(['eater', 'stag', 'hound'], ['울음 먹는 자', '길지기 사슴', '재 먹는 들개'],
               HERE/'Previews', HERE/'spirit_shape_comparison.png', '신규 주령 3종 · 형태 수정 전후')
    # NPC uses a horizontal layout for a direct face / clothing comparison.
    canvas = Image.new('RGBA', (880, 680), (24, 34, 44, 255))
    draw = ImageDraw.Draw(canvas)
    draw.text((24, 17), '조합형 NPC · 형태 수정 전후', font=title_font, fill='#f5ebd6')
    for col, folder in enumerate((before, NPC/'Previews')):
        x = col*440
        draw.rounded_rectangle((x+7, 106, x+433, 672), 17, fill='#23303a')
        canvas.alpha_composite(Image.open(folder/'hunter.png').convert('RGBA'), (x, 114))
        draw.text((x+20, 604), '수정 전' if col == 0 else '수정 후', font=label_font, fill='white')
    canvas.convert('RGB').save(NPC/'npc_shape_comparison.png')


if '--motion-dir' in sys.argv:
    folder = Path(sys.argv[sys.argv.index('--motion-dir')+1])
    for key in ('hunter_walk', 'eater_run', 'stag_walk', 'hound_run'):
        frames = []
        for path in sorted((folder/key).glob('*.png')):
            rgba = Image.open(path).convert('RGBA')
            background = Image.new('RGBA', rgba.size, '#23303a')
            background.alpha_composite(rgba)
            frames.append(background.convert('RGB'))
        if frames:
            target = NPC if key == 'hunter_walk' else HERE
            frames[0].save(target/(key+'.gif'), save_all=True, append_images=frames[1:],
                           duration=83, loop=0, optimize=False, disposal=2)
