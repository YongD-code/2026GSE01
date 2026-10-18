"""Package Blender renders into candidate comparisons, an atlas and motion preview.

This lays out renders only. It never paints a replacement game sprite or writes to Assets.
Run with a Python installation containing Pillow, after create_forgotten.py exports poses.
"""
import hashlib
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[2]
BG = '#17242d'
TITLE = ImageFont.truetype('C:/Windows/Fonts/malgunbd.ttf', 27)
FONT = ImageFont.truetype('C:/Windows/Fonts/malgun.ttf', 20)
SMALL = ImageFont.truetype('C:/Windows/Fonts/malgun.ttf', 16)
SOURCE = ROOT / 'SimpleGame/Assets/Spirits/ash_lantern_4dir.png'
source = Image.open(SOURCE).convert('RGBA')
cw, ch = source.width // 4, source.height // 4
original = source.crop((0, 0, cw, ch))


def panel(canvas, box, label, picture, height, baseline, caption=''):
    draw = ImageDraw.Draw(canvas)
    x, y, w, h = box
    draw.rounded_rectangle((x, y, x+w, y+h), 18, fill='#22333f', outline='#40505b', width=1)
    draw.text((x+20, y+15), label, font=FONT, fill='#f3deba')
    bounds = picture.getchannel('A').getbbox()
    if bounds is None:
        raise ValueError('Empty candidate render')
    image = picture.crop(bounds)
    image = image.resize((round(image.width * height/image.height), height), Image.Resampling.LANCZOS)
    canvas.alpha_composite(image, (x+(w-image.width)//2, baseline-height))
    if caption:
        draw.text((x+20, y+h-36), caption, font=SMALL, fill='#b8c7d0')


candidate = Image.open(OUT/'candidate_perspective.png').convert('RGBA')
canvas = Image.new('RGBA', (1120, 820), BG)
draw = ImageDraw.Draw(canvas)
draw.text((30, 20), '남겨진 자 · 등롱의 기억 / 2.5D 외형 후보', font=TITLE, fill='#f3deba')
draw.text((30, 62), '기존 등롱·가면·불꽃을 유지하고 금속판과 얇은 천으로 입체화', font=SMALL, fill='#b8c7d0')
panel(canvas, (24, 106, 524, 620), '현재 · 게임 원화', original, 490, 675, '현재 스프라이트의 한 포즈를 확대한 비교')
panel(canvas, (572, 106, 524, 620), '후보 · Blender 모델 렌더', candidate, 490, 675, '검토용 모델 · 기존 외형은 그대로 유지')
draw.text((30, 754), '실루엣 높이를 맞춘 외형 비교 · 게임 화면 캡처와 실제 게임 조명 결과가 아닙니다.', font=SMALL, fill='#b8c7d0')
draw.text((30, 784), '금속 등롱 / 도자기 가면의 빈 눈·입 / 얇은 주름 천 / 시간차가 있는 부유 동작', font=SMALL, fill='#d7b98d')
canvas.save(OUT/'comparison.png')

canvas = Image.new('RGBA', (1600, 850), BG)
draw = ImageDraw.Draw(canvas)
draw.text((26, 18), '후보 모델 · 정면 / 사선 / 측면 / 후면', font=TITLE, fill='#f3deba')
for i, (name, label) in enumerate([('front','정면'), ('perspective','사선'), ('side','측면'), ('back','후면')]):
    image = Image.open(OUT/f'candidate_{name}.png').convert('RGBA')
    # All views have the same camera scale and canvas; do not normalize their silhouettes.
    image = image.resize((392,490), Image.Resampling.LANCZOS)
    canvas.alpha_composite(image, (i*400+4,150))
    draw.text((i*400+22,100), label, font=FONT, fill='#f3deba')
draw.text((26,700), '동일 직교 카메라 배율 · 눈과 입의 실제 구멍, 금속판 틈과 얇은 천 두께 확인용', font=SMALL, fill='#b8c7d0')
draw.text((26,738), '몸을 통째로 찌그러뜨리지 않고 리그의 본체·손·천·불꽃을 분리해 움직입니다.', font=SMALL, fill='#b8c7d0')
draw.text((26,780), '에셋 검토용 렌더 / 게임 미적용', font=FONT, fill='#d7b98d')
canvas.save(OUT/'turnaround.png')

if (OUT/'flame_before.png').exists():
    canvas = Image.new('RGBA',(1120,690),BG)
    draw = ImageDraw.Draw(canvas)
    draw.text((28,20),'남겨진 자 · 불꽃 두께와 속불 비교',font=TITLE,fill='#f3deba')
    draw.text((28,64),'같은 카메라·같은 영역을 확대한 모델 렌더 / 게임 미적용',font=SMALL,fill='#b8c7d0')
    for x,name,label in [(24,'flame_before.png','이전 · 단색 불꽃층'),
                         (572,'candidate_perspective.png','수정 · 입체 볼륨 불꽃')]:
        draw.rounded_rectangle((x,112,x+524,598),18,fill='#22333f',outline='#40505b')
        draw.text((x+20,128),label,font=FONT,fill='#f3deba')
        picture=Image.open(OUT/name).convert('RGBA')
        detail=picture.crop((155,300,405,510)).resize((500,420),Image.Resampling.LANCZOS)
        canvas.alpha_composite(detail,(x+12,172))
    draw.text((28,622),'내부 밀도와 온도 변화 · 가장자리 열기 · 서로 이어지는 세 갈래 불길',font=SMALL,fill='#d7b98d')
    draw.text((28,654),'사진의 밝기는 후보 렌더 기준이며 게임의 조명·후처리와는 아직 연결하지 않았습니다.',font=SMALL,fill='#b8c7d0')
    canvas.save(OUT/'fire_review.png')

manifest = OUT/'candidate_manifest.json'
if manifest.exists():
    report = json.loads(manifest.read_text(encoding='utf-8'))
    cell, columns, rows = report['cellSize'], report['columns'], report['rows']
    sheet = Image.new('RGBA', (cell*columns,cell*rows))
    union = None
    clipped = []
    for row in range(rows):
        for column in range(columns):
            image = Image.open(OUT/'Frames'/f'd{row}_f{column:02d}.png').convert('RGBA')
            assert image.size == (cell,cell)
            box = image.getchannel('A').getbbox()
            if box is None:
                raise ValueError(f'Empty pose: {row},{column}')
            if box[0] < 2 or box[1] < 2 or box[2] > cell-2 or box[3] > cell-2:
                clipped.append([row,column])
            union = box if union is None else (min(union[0],box[0]),min(union[1],box[1]),max(union[2],box[2]),max(union[3],box[3]))
            sheet.alpha_composite(image,(column*cell,row*cell))
    if clipped:
        raise ValueError(f'Poses touch canvas boundary: {clipped}')
    sheet.save(OUT/'candidate_glide_8dir.png')
    report.update({'atlas':str((OUT/'candidate_glide_8dir.png').relative_to(ROOT)),
                   'dimensions':list(sheet.size),'alphaBounds':list(union),
                   'fixedCanvas':True,'canvasAnchorY':.5+math.cos(report['camera']['tilt'])*1.92/4.8,
                   'canvasCharacterHeight':(union[3]-union[1])/cell,
                   'emptyCells':0,'clippedCells':0,'runtimeIntegrated':False})
    canvas = Image.new('RGBA',(1040,650),BG)
    draw=ImageDraw.Draw(canvas)
    draw.text((24,16),'8방향 시범 원화 · 고정 캔버스',font=TITLE,fill='#f3deba')
    labels=['정면','앞·왼쪽','왼쪽','뒤·왼쪽','후면','뒤·오른쪽','오른쪽','앞·오른쪽']
    for row,label in enumerate(labels):
        x,y=(row%4)*260+34,(row//4)*270+74
        image=sheet.crop((0,row*cell,cell,(row+1)*cell))
        canvas.alpha_composite(image,(x,y+30))
        draw.text((x,y),label,font=SMALL,fill='#f3deba')
    draw.text((24,620),'각 방향 대기 1 + 이동 부유 8포즈 · 아직 게임에 연결하지 않은 후보',font=SMALL,fill='#b8c7d0')
    canvas.save(OUT/'direction_review.png')
    frames=[]
    for index in range(16):
        image=Image.open(OUT/'Motion'/f'glide_{index:02d}.png').convert('RGBA')
        frame=Image.new('RGBA',(480,510),BG)
        frame.alpha_composite(image,(80,74))
        draw=ImageDraw.Draw(frame)
        draw.text((20,16),'남겨진 자 · 이동 부유 후보',font=FONT,fill='#f3deba')
        draw.text((20,480),'본체 → 손 → 천끝의 시간차 / 미적용',font=SMALL,fill='#b8c7d0')
        frames.append(frame.convert('RGB'))
    # Fixed palette prevents arbitrary per-frame color shifts in the review GIF.
    palette=frames[0].quantize(colors=240)
    frames=[f.quantize(palette=palette,dither=Image.Dither.NONE) for f in frames]
    frames[0].save(OUT/'glide_preview.gif',save_all=True,append_images=frames[1:],duration=83,loop=0,disposal=2)
    if report.get('fire'):
        (OUT/'fire_preview.gif').write_bytes((OUT/'glide_preview.gif').read_bytes())
    actual=hashlib.sha256(SOURCE.read_bytes()).hexdigest()
    assert report['protectedBefore'][str(SOURCE.relative_to(ROOT))] == actual
    report['reviewFiles']=['comparison.png','turnaround.png','direction_review.png','glide_preview.gif']
    if (OUT/'fire_review.png').exists():
        report['reviewFiles'].append('fire_review.png')
    if (OUT/'fire_preview.gif').exists():
        report['reviewFiles'].append('fire_preview.gif')
    manifest.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('FORGOTTEN_REVIEW_READY',flush=True)
