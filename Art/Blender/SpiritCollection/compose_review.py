"""Prepare old/new model comparisons and movement previews from Blender renders."""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUT=Path(__file__).resolve().parent
ROOT=OUT.parents[2]
TITLE=ImageFont.truetype('C:/Windows/Fonts/malgunbd.ttf',26)
FONT=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',20)
SMALL=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',16)
BG='#17242d'
species=[('pilgrim','목 없는 순례자','mourning_bell_4dir.png'),
         ('hands','기도하는 손','briar_fox_4dir.png'),
         ('saint','기도하지 않는 성자','unpraying_saint_4dir.png')]
canvas=Image.new('RGBA',(1560,940),BG)
draw=ImageDraw.Draw(canvas)
draw.text((25,18),'주령 3종 · 기존 원화 / 새 2.5D 모델',font=TITLE,fill='#f3deba')
draw.text((25,58),'외형 참고용 렌더 · 원화와 모델의 실루엣 높이를 맞춘 비교 / 게임 캡처 아님',font=SMALL,fill='#b8c7d0')


def place(target,picture,x,y,w,h):
    picture=picture.convert('RGBA')
    bounds=picture.getchannel('A').getbbox()
    if bounds is None:raise ValueError('Empty review image')
    picture=picture.crop(bounds)
    factor=min(w/picture.width,h/picture.height)
    picture=picture.resize((round(picture.width*factor),round(picture.height*factor)),Image.Resampling.LANCZOS)
    target.alpha_composite(picture,(x+(w-picture.width)//2,y+(h-picture.height)//2))


for column,(key,label,old_path) in enumerate(species):
    x=column*520
    draw.rounded_rectangle((x+10,98,x+510,855),16,fill='#22333f',outline='#40505b')
    draw.text((x+28,114),label,font=FONT,fill='#f3deba')
    old=Image.open(ROOT/'SimpleGame/Assets/Spirits'/old_path).convert('RGBA')
    old=old.crop((0,0,old.width//4,old.height//4))
    new=Image.open(OUT/key/'perspective.png')
    place(canvas,old,x+40,158,440,290)
    place(canvas,new,x+40,508,440,290)
    draw.text((x+28,463),'기존 원화',font=SMALL,fill='#b8c7d0')
    draw.text((x+28,818),'새 모델 · 대기/이동/시전 8방향 원화',font=SMALL,fill='#d7b98d')
    views=Image.new('RGBA',(1440,720),BG)
    vd=ImageDraw.Draw(views)
    vd.text((24,20),label+' · 네 방향 조형',font=TITLE,fill='#f3deba')
    for i,(name,caption) in enumerate([('front','정면'),('perspective','사선'),('side','측면'),('back','후면')]):
        image=Image.open(OUT/key/f'{name}.png').convert('RGBA').resize((360,450),Image.Resampling.LANCZOS)
        views.alpha_composite(image,(i*360,112))
        vd.text((i*360+20,82),caption,font=FONT,fill='#f3deba')
    vd.text((24,638),'같은 직교 카메라 배율 · 편집 가능한 Blender 원본 보관 / 게임 캡처 아님',font=SMALL,fill='#b8c7d0')
    views.save(OUT/key/'turnaround.png')
draw.text((25,890),'순례자의 종 / 여우 가면과 꼬리 / 성자의 빈 후드와 후광을 각각 살린 모델입니다.',font=SMALL,fill='#b8c7d0')
canvas.save(OUT/'spirit_comparison.png')
frames=[]
for frame in range(16):
    image=Image.new('RGBA',(1080,530),BG)
    d=ImageDraw.Draw(image)
    for column,(key,label,_) in enumerate(species):
        x=column*360
        d.text((x+18,18),label,font=FONT,fill='#f3deba')
        pose=Image.open(OUT/key/'Motion'/f'move_{frame:02d}.png').convert('RGBA')
        image.alpha_composite(pose,(x+20,66))
    d.text((20,494),'모델 동작 참고용 · 종 흔들림 / 네 발 보행과 꼬리 / 긴 예복의 부유',font=SMALL,fill='#b8c7d0')
    frames.append(image.convert('RGB'))
palette=frames[0].quantize(colors=240)
frames=[f.quantize(palette=palette,dither=Image.Dither.NONE) for f in frames]
frames[0].save(OUT/'spirit_motion.gif',save_all=True,append_images=frames[1:],duration=100,loop=0,disposal=2)
print('SPIRIT_REVIEWS_READY',flush=True)
