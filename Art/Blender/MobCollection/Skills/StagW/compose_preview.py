"""Label the real Blender concept renders; no runtime implementation implied."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
HERE=Path(__file__).resolve().parent
font='C:/Windows/Fonts/malgun.ttf'
title=ImageFont.truetype(font,34)
label=ImageFont.truetype(font,25)
small=ImageFont.truetype(font,19)
board=Image.new('RGB',(1920,680),'#19241e')
d=ImageDraw.Draw(board)
d.text((26,18),'W · 숲의 숨결 — 연출 검토안',font=title,fill='#f1e8cb')
for i,(file,heading,desc) in enumerate([
    ('prepare','01 · 숨을 모으다','고개를 숙이고 뿔 주변에 작은 빛이 모입니다.'),
    ('bloom','02 · 꽃밭이 번지다','발밑부터 풀과 꽃이 서로 다른 속도로 자랍니다.'),
    ('sustain','03 · 숲이 숨 쉬다','낮은 꽃밭 위로 따뜻한 꽃가루가 떠오릅니다.')]):
    im=Image.open(HERE/(file+'.png')).convert('RGB'); im.thumbnail((630,500))
    board.paste(im,(i*640+5,85))
    d.text((i*640+20,590),heading,font=label,fill='#f1e8cb')
    d.text((i*640+20,630),desc,font=small,fill='#b8c6ad')
board.save(HERE/'stag_w_stages.png')
frames=[]
for f in sorted((HERE/'Frames').glob('*.png')):
    im=Image.new('RGB',(640,560),'#19241e'); im.paste(Image.open(f).convert('RGB'),(0,45))
    d=ImageDraw.Draw(im); d.text((16,9),'W 숲의 숨결 · 3D 연출 미리보기',font=label,fill='#f1e8cb')
    d.text((16,532),'검토용 장면 · 회복 판정과 게임 연결은 미구현',font=small,fill='#b8c6ad')
    frames.append(im)
if frames:
    frames[0].save(HERE/'stag_w_preview.gif',save_all=True,append_images=frames[1:],duration=125,loop=0,disposal=2)
print('W_REVIEW_COMPOSED',len(frames))
