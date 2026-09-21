"""Create a side-by-side NPC review image using Blender image data."""
import bpy
from array import array
from pathlib import Path
root = Path(__file__).resolve().parents[2]
folder = root / 'SimpleGame/Assets/Characters/Blender'
images = [bpy.data.images.load(str(folder / name / 'preview.png')) for name in ('npc1', 'npc2')]
w, h = images[0].size
pixels = array('f', [0]) * (w * 2 * h * 4)
for index, source in enumerate(images):
    data = array('f', [0]) * (w * h * 4)
    source.pixels.foreach_get(data)
    for row in range(h):
        start = (row * w * 2 + index * w) * 4
        pixels[start:start + w * 4] = data[row * w * 4:(row + 1) * w * 4]
preview = bpy.data.images.new('NPC 미리보기', width=w*2, height=h, alpha=True)
preview.pixels.foreach_set(pixels)
preview.filepath_raw = str(root / 'Art/Blender/npc_preview.png')
preview.file_format = 'PNG'
preview.save()
