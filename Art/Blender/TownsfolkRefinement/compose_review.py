"""Lay out unchanged Blender renders for selection; keep a common camera scale."""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parent
BACKGROUND = (34, 39, 48, 255)
title_font = ImageFont.truetype('C:/Windows/Fonts/malgunbd.ttf', 28)
font = ImageFont.truetype('C:/Windows/Fonts/malgun.ttf', 22)
small = ImageFont.truetype('C:/Windows/Fonts/malgun.ttf', 18)


def board(name, title, subjects, caption):
    canvas = Image.new('RGBA', (640 * len(subjects), 940), BACKGROUND)
    draw = ImageDraw.Draw(canvas)
    draw.text((24, 14), title, font=title_font, fill='#f2e6c5')
    for column, (image_name, label) in enumerate(subjects):
        image = Image.open(OUT / (image_name + '.png')).convert('RGBA')
        if image.size != (640, 800):
            raise ValueError(f'Inconsistent source camera canvas: {image_name}')
        canvas.alpha_composite(image, (column * 640, 85))
        draw.text((column * 640 + 24, 57), label, font=font, fill='white')
    draw.text((24, 902), caption, font=small, fill='#ccd3df')
    canvas.save(OUT / (name + '.png'))


board('comparison', '주인공 기준 · 채집꾼 모델 정교화 후보',
      [('reference_protagonist', '기준 · 현재 주인공'),
       ('candidate_perspective', '새 후보 · 채집꾼')],
      '동일 카메라·조명·배율로 렌더한 모델 비교 / 게임 캡처가 아니며 새 후보는 미적용')
board('previous_comparison', '이전 채집꾼 모델과 새 후보',
      [('reference_previous', '이전 · 1차 보행 후보'), ('candidate_perspective', '새 후보 · 정교화')],
      '같은 카메라·조명에서 기존 메시와 새 후보를 렌더한 비교 / 모델을 따로 확대하지 않음')
board('turnaround', '새 후보 · 정면 / 측면 / 후면',
      [('candidate_front', '정면'), ('candidate_right', '측면'), ('candidate_back', '후면')],
      '머리·손가락·겹친 작업복·등짐 조형 확인용 / 게임에는 아직 적용하지 않음')

paths = sorted((OUT / 'Motion').glob('walk_*.png'))
if len(paths) == 16:
    frames = []
    for path in paths:
        image = Image.open(path).convert('RGB')
        draw = ImageDraw.Draw(image)
        draw.text((12, 12), '걷기 · 후보 미리보기', font=small, fill='#f2e6c5')
        frames.append(image)
    frames[0].save(OUT / 'walk_preview.gif', save_all=True, append_images=frames[1:],
                   duration=65, loop=0, disposal=2)
print('CANDIDATE_REVIEW_READY')
