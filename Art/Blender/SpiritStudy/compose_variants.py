"""Compare whole-character lantern alternatives without changing any game assets."""
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUT=Path(__file__).resolve().parent
ROOT=OUT.parents[2]
DEST=OUT/'Variants'
BG='#17242d'
TITLE=ImageFont.truetype('C:/Windows/Fonts/malgunbd.ttf',28)
FONT=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',21)
SMALL=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',17)
choices=[('A · 기존 후보',OUT/'candidate_perspective.png','기본 등롱 / 긴 천 / 열린 가면'),
         ('B · 기다리는 등불',DEST/'round_lantern/perspective.png','넓고 둥근 등롱 / 작은 가면 / 짧은 천'),
         ('C · 갈라진 기억',DEST/'split_mask/perspective.png','좁은 육각 등롱 / 갈라진 가면 / 비대칭 천')]

canvas=Image.new('RGBA',(1680,920),BG)
draw=ImageDraw.Draw(canvas)
draw.text((28,20),'남겨진 자 · 모델 전체 후보 A / B / C',font=TITLE,fill='#f3deba')
draw.text((28,66),'같은 카메라·조명·배율의 Blender 렌더 / 모델마다 따로 크기를 맞추지 않았습니다.',font=SMALL,fill='#b8c7d0')
for i,(label,path,detail) in enumerate(choices):
    x=i*560
    draw.rounded_rectangle((x+10,108,x+550,836),18,fill='#22333f',outline='#40505b')
    draw.text((x+28,126),label,font=FONT,fill='#f3deba')
    picture=Image.open(path).convert('RGBA')
    assert picture.size==(560,700)
    picture=picture.resize((532,665),Image.Resampling.LANCZOS)
    canvas.alpha_composite(picture,(x+14,166))
    draw.text((x+26,850),detail,font=SMALL,fill='#d7b98d')
draw.text((28,890),'추가 후보는 B와 C 두 개입니다. 기존 후보 A와 게임 원화는 보존하며 선택 전까지 게임에 적용하지 않습니다.',font=SMALL,fill='#b8c7d0')
canvas.save(DEST/'model_choices.png')

for key,label in [('round_lantern','B · 기다리는 등불'),('split_mask','C · 갈라진 기억')]:
    folder=DEST/key
    report=json.loads((folder/'candidate_manifest.json').read_text(encoding='utf-8'))
    for name,h in report['protectedBefore'].items():
        assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==h==report['protectedAfter'][name]
    canvas=Image.new('RGBA',(1440,760),BG)
    draw=ImageDraw.Draw(canvas)
    draw.text((24,18),label+' · 네 방향 조형',font=TITLE,fill='#f3deba')
    for i,(name,view) in enumerate([('front','정면'),('perspective','사선'),('side','측면'),('back','후면')]):
        image=Image.open(folder/f'{name}.png').convert('RGBA')
        assert image.size==(560,700)
        canvas.alpha_composite(image.resize((360,450),Image.Resampling.LANCZOS),(i*360,125))
        draw.text((i*360+22,86),view,font=FONT,fill='#f3deba')
    draw.text((24,640),'지붕·얼굴·등롱 몸통·손·천을 다시 만든 독립 후보 / 같은 직교 배율',font=SMALL,fill='#b8c7d0')
    draw.text((24,686),'편집 가능한 Blender 원본과 16뼈 부유 리그 보관 / 게임 미적용',font=SMALL,fill='#d7b98d')
    canvas.save(folder/'turnaround.png')
    sheet=Image.new('RGBA',(192*8,192))
    for direction in range(8):
        image=Image.open(folder/'Directions'/f'd{direction}.png').convert('RGBA')
        assert image.size==(192,192)
        bounds=image.getchannel('A').getbbox()
        assert bounds and bounds[0]>=2 and bounds[1]>=2 and bounds[2]<=190 and bounds[3]<=190
        sheet.alpha_composite(image,(direction*192,0))
    sheet.save(folder/'direction_study.png')
    report.update({'reviewFiles':['turnaround.png','direction_study.png'],
                   'emptyDirectionCells':0,'clippedDirectionCells':0})
    (folder/'candidate_manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

frames=[]
for frame in range(16):
    canvas=Image.new('RGBA',(800,570),BG)
    draw=ImageDraw.Draw(canvas)
    draw.text((22,14),'추가 모델 후보 · 부유 동작',font=TITLE,fill='#f3deba')
    for column,(key,label) in enumerate([('round_lantern','B · 기다리는 등불'),('split_mask','C · 갈라진 기억')]):
        x=column*400
        draw.text((x+24,66),label,font=FONT,fill='#f3deba')
        image=Image.open(DEST/key/'Motion'/f'float_{frame:02d}.png').convert('RGBA')
        assert image.size==(320,400)
        canvas.alpha_composite(image,(x+40,105))
    draw.text((22,530),'같은 카메라와 부유 위상 / 체형과 천의 길이·흔들림은 후보별로 다름 / 미적용',font=SMALL,fill='#b8c7d0')
    frames.append(canvas.convert('RGB'))
palette=frames[0].quantize(colors=240)
frames=[f.quantize(palette=palette,dither=Image.Dither.NONE) for f in frames]
frames[0].save(DEST/'model_motion.gif',save_all=True,append_images=frames[1:],duration=83,loop=0,disposal=2)
print('LANTERN_VARIANT_REVIEWS_READY',flush=True)
