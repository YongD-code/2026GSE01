"""Package the rendered skill timeline with a synchronized close-up of its real model."""
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageOps

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[4]
FRAMES = ROOT/'tmp/stag_q_preview/frames'
FONT = 'C:/Windows/Fonts/malgun.ttf'
BOLD = 'C:/Windows/Fonts/malgunbd.ttf'
CATALOG = json.loads((HERE.parents[1]/'mob_catalog.json').read_text(encoding='utf-8'))
SPEC = next(e for e in CATALOG['species'] if e['key'] == 'stag')['skills']['Q']['timing']


def label(age):
    if age < SPEC['Charge']:
        return '뿔에 생명력 응축'
    if age < SPEC['Charge']+SPEC['Travel']:
        return '세 갈래 뿌리 확산'
    if age < SPEC['Charge']+SPEC['Travel']+SPEC['BloomRise']:
        return '덩굴 솟구침'
    if age < SPEC['Duration']-.3:
        return '공중 개화 · 표적 결박'
    return '잔광 소멸'


def main():
    frames = []
    small = ImageFont.truetype(FONT, 13)
    medium = ImageFont.truetype(BOLD, 16)
    title = ImageFont.truetype(BOLD, 22)
    files = sorted(FRAMES.glob('[0-9][0-9][0-9].png'))
    if not files:
        raise RuntimeError('Render the preview frames first.')
    crop = json.loads((FRAMES.parent/'layout.json').read_text(encoding='utf-8'))['actorCrop']
    for i, path in enumerate(files):
        raw = Image.open(path).convert('RGB')
        canvas = Image.new('RGB', (940, 486), '#0c1714')
        canvas.paste(raw, (0, 40))
        draw = ImageDraw.Draw(canvas)
        draw.text((18, 8), '길지기 사슴 Q · 길틔움', fill='#e5f4bf', font=title)
        draw.text((490, 13), label(i/20), fill='#bee9a5', font=medium)
        # The same frame is enlarged: no replacement model or separate imagined motion.
        close = ImageOps.pad(raw.crop(crop), (190, 229), Image.Resampling.LANCZOS, color='#12241e')
        draw.rounded_rectangle((730, 55, 934, 425), radius=8, fill='#12241e', outline='#597350', width=1)
        canvas.paste(close, (737, 90))
        draw.text((745, 64), '시전 동작 확대', fill='#d8edc5', font=medium)
        draw.text((747, 344), '세 줄기 덩굴 상승', fill='#bad09d', font=medium)
        draw.text((747, 369), '꼭대기 금빛 개화', fill='#bad09d', font=medium)
        draw.text((747, 394), f"재사용 {SPEC['Cooldown']:.0f}초", fill='#bad09d', font=medium)
        draw.text((18, 465), '실제 모델 기반 블렌더 미리보기 · 게임 화면 아님', fill='#a3b4aa', font=small)
        frames.append(canvas)
    palette_image = Image.new('RGB', (940, 486*4))
    for row, index in enumerate((0, 10, 26, 33)):
        palette_image.paste(frames[index], (0, row*486))
    palette = palette_image.quantize(colors=192)
    encoded = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
    encoded[0].save(HERE/'stag_q_rising_vines.gif', save_all=True, append_images=encoded[1:],
                    duration=[50]*(len(encoded)-1)+[500], loop=0, disposal=2, optimize=False)
    frames[33].save(HERE/'stag_q_rising_peak.png')
    sheet = Image.new('RGB', (1880, 972), '#0c1714')
    for tile, index in enumerate((6, 17, 26, 33)):
        sheet.paste(frames[index], ((tile%2)*940, (tile//2)*486))
    sheet.save(HERE/'stag_q_rising_stages.png')
    print('STAG_Q_GIF_READY', HERE/'stag_q_rising_vines.gif')


if __name__ == '__main__':
    main()
