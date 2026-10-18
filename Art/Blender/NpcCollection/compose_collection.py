"""Selection boards from actual Blender renders and preserved reference sprites."""
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[2]
report = json.loads((OUT / 'collection_manifest.json').read_text(encoding='utf-8'))
background = (34, 39, 48, 255)
title = ImageFont.truetype('C:/Windows/Fonts/malgunbd.ttf', 25)
label_font = ImageFont.truetype('C:/Windows/Fonts/malgun.ttf', 19)
small = ImageFont.truetype('C:/Windows/Fonts/malgun.ttf', 16)


def reference(spec):
    if spec['category'] == 'story':
        image = Image.open(ROOT / 'SimpleGame/Assets/Characters/Story' / (spec['source'] + '.png'))
        return image.crop((0, 0, image.width // 2, image.height // 2)).convert('RGBA')
    image = Image.open(ROOT / 'SimpleGame/Assets/Characters/Townsfolk/residents.png')
    cell_w, cell_h = image.width // 4, image.height // 4
    return image.crop((0, spec['role'] * cell_h, cell_w, (spec['role'] + 1) * cell_h)).convert('RGBA')


def board(category, columns, filename):
    designs = [d for d in report['designs'] if d['category'] == category]
    rows = math.ceil(len(designs) / columns)
    result = Image.new('RGBA', (columns * 400, rows * 640 + 90), background)
    draw = ImageDraw.Draw(result)
    draw.text((20, 12), '생활 NPC 후보' if category == 'crowd' else '이야기 NPC 후보',
              font=title, fill='#f2e6c5')
    draw.text((20, 48), '주인공·채집꾼의 셀 명암 기준 / 외형 선택 전 게임에 미적용',
              font=small, fill='#cbd4e0')
    for i, spec in enumerate(designs):
        x, y = i % columns * 400, 80 + i // columns * 640
        image = Image.open(OUT / spec['id'] / 'perspective.png').convert('RGBA')
        result.alpha_composite(image, (x, y + 25))
        draw.text((x + 20, y), spec['label'], font=label_font, fill='white')
        old = reference(spec)
        box = old.getchannel('A').getbbox()
        if box:
            old = old.crop(box)
        old.thumbnail((85, 140), Image.Resampling.LANCZOS)
        result.alpha_composite(old, (x + 12, y + 480 - old.height))
        draw.text((x + 12, y + 487), '현재 외형', font=small, fill='#cbd4e0')
        # A per-character board includes all views so silhouette/depth can be selected.
        views = Image.new('RGBA', (1200, 655), background)
        vd = ImageDraw.Draw(views)
        for j, (kind, label) in enumerate([('perspective','새 후보'),('profile','측면'),('back','후면')]):
            view = Image.open(OUT / spec['id'] / (kind + '.png')).convert('RGBA')
            views.alpha_composite(view, (400 * j, 40))
            vd.text((400 * j + 20, 10), spec['label'] + ' · ' + label, font=label_font, fill='white')
        views.save(OUT / spec['id'] / 'views.png')
    result.save(OUT / filename)


board('crowd', 3, 'crowd_candidates.png')
board('story', 4, 'story_candidates.png')
report['reviewBoards'] = ['crowd_candidates.png', 'story_candidates.png']
(OUT / 'collection_manifest.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n',
                                               encoding='utf-8')
print('NPC_SELECTION_BOARDS_READY')
