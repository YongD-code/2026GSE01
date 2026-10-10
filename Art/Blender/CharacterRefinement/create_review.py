"""Compose model review and validate every exported sprite cell."""
from pathlib import Path
import json
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
font = ImageFont.truetype('C:/Windows/Fonts/malgun.ttf', 19)
small = ImageFont.truetype('C:/Windows/Fonts/malgun.ttf', 14)
bg = (31, 36, 48, 255)
directions = ['정면', '우전방', '우측', '우후방', '후면', '좌후방', '좌측', '좌전방']
atlases = {k: Image.open(HERE/'Sprites'/f'{k}.png').convert('RGBA') for k in ['walk','run','cast']}
checks = {}
for kind, atlas in atlases.items():
    columns = 4 if kind == 'cast' else 17
    assert atlas.size == (columns*192, 1536), (kind, atlas.size)
    boxes = []
    for row in range(8):
        for col in range(columns):
            alpha = atlas.crop((col*192,row*192,(col+1)*192,(row+1)*192)).getchannel('A')
            box = alpha.point(lambda a: 255 if a > 12 else 0).getbbox()
            assert box and box[0]>0 and box[1]>0 and box[2]<192 and box[3]<192, (kind,row,col,box)
            boxes.append(box)
    checks[kind] = {'size': atlas.size, 'frames':len(boxes), 'all_frames_have_margin':True}

def cell(kind, row, col):
    return atlases[kind].crop((col*192,row*192,(col+1)*192,(row+1)*192))

frames=[]
for tick in range(32):
    canvas=Image.new('RGBA',(8*144,3*176+44),bg)
    draw=ImageDraw.Draw(canvas)
    for row,label in enumerate(directions): draw.text((row*144+40,10),label,font=small,fill='white')
    for k,kind in enumerate(['walk','run','cast']):
        col=(tick%16)+1 if kind!='cast' else (tick//4)%4
        for direction in range(8):
            sprite=cell(kind,direction,col).resize((140,140),Image.Resampling.LANCZOS)
            canvas.alpha_composite(sprite,(direction*144,44+k*176))
        draw.text((12,44+k*176+143),{'walk':'걷기','run':'달리기','cast':'시전'}[kind],font=small,fill='white')
    frames.append(canvas.convert('RGB'))
frames[0].save(HERE/'animation_review.gif',save_all=True,append_images=frames[1:],duration=50,loop=0)
frames[0].save(HERE/'animation_contact.png')

canvas=Image.new('RGBA',(8*120,240),bg)
draw=ImageDraw.Draw(canvas)
draw.text((15,10),'축소 실루엣 확인: 78px 셀 / 192px 원본',font=font,fill='white')
for row,label in enumerate(directions):
    canvas.alpha_composite(cell('walk',row,0).resize((78,78),Image.Resampling.LANCZOS),(row*120+21,65))
    draw.text((row*120+30,161),label,font=small,fill='white')
canvas.save(HERE/'game_size_review.png')

# Compare the currently selected model with the new candidate, not only the prior volume study.
canvas=Image.new('RGBA',(4*400,554),bg)
draw=ImageDraw.Draw(canvas)
for i,(path,label) in enumerate([
    (HERE.parent/'SideVolumeStudy/original_front.png','현재 모델 · 정면'),
    (HERE/'final_front.png','새 후보 · 정면'),
    (HERE.parent/'SideVolumeStudy/original_right.png','현재 모델 · 측면'),
    (HERE/'final_right.png','새 후보 · 측면')]):
    im=Image.open(path).convert('RGBA');im.thumbnail((400,500),Image.Resampling.LANCZOS)
    canvas.alpha_composite(im,(i*400+(400-im.width)//2,54))
    draw.text((i*400+70,16),label,font=font,fill='white')
canvas.save(HERE/'selection_comparison.png')
(HERE/'validation.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(checks))
