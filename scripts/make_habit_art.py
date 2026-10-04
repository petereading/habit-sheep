"""Development-only SVG rasterisation: requires Inkscape and Pillow.

The build uses committed PBMs and build_habit_art.py; no rasteriser is needed.
"""
from pathlib import Path
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
ART = ROOT / 'data/habit_sheep/art'
ART.mkdir(parents=True, exist_ok=True)


def svg(name, body, width=256, height=192):
    (ART / f'{name}.svg').write_text(
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}">{body}</svg>\n')


BODY = 'M62 151C40 158 21 143 29 125C10 114 16 93 33 86C24 66 43 50 64 56C66 34 89 25 108 36C120 16 148 22 154 40C181 29 200 45 194 62C215 61 223 79 207 91L191 125C191 147 166 157 149 146C136 164 112 163 102 151C90 162 73 160 62 151Z'

for i in range(18):
    asleep = 12 <= i < 16
    mirror = i in (1, 5, 10, 13, 15)
    jump = i == 11
    tilt = [0, 0, -12, 12, 0, 0, -8, 5, -5, 24, -20, -8, 0, 0, 5, -5, 30, 22][i]
    body_shift = -10 if jump else (8 if asleep else 0)
    front = '<path d="M71 139v27q8 12 17 0v-27M132 139v27q8 12 17 0v-27" fill="black"/>'
    if asleep:
        front = '<ellipse cx="77" cy="155" rx="21" ry="10" fill="black"/><ellipse cx="150" cy="155" rx="21" ry="10" fill="black"/>'
    elif i in (4, 5):
        front = '<path d="m71 137-10 29q2 12 12 7l19-32m36-3 12 27q8 9 16 0l-8-30" fill="black"/>'
    elif i == 6:
        front = '<path d="m71 137-21 28q1 12 12 8l30-32m37-3 17 28q9 9 16-1l-17-27" fill="black"/>'
    face = '<path d="M171 65C192 62 224 74 232 100C238 125 211 143 183 137C160 133 153 108 163 88Z" fill="black"/>'
    eye = '<path d="M205 104q5 8 11 0" stroke="white" fill="none" stroke-width="4" stroke-linecap="round"/>' if asleep or i == 8 else '<circle cx="208" cy="103" r="4" fill="white"/>'
    if i == 2:
        eye += '<circle cx="188" cy="103" r="4" fill="white"/>'
    face += '<path d="M179 75q-30 9-24 35q21 4 28-23Z" fill="black"/>' + eye
    face += '<path d="M212 122q5 5 9 0" stroke="white" fill="none" stroke-width="3" stroke-linecap="round"/>'
    marks = ''
    if i == 7:
        marks = '<path d="m15 51 9 6m-9 15 9 0m207-35-9 7m13 9-10 2" stroke="black" stroke-width="3"/>'
    if i == 8:
        marks = '<path d="m63 143-19-16 10-5 20 24" stroke="black" stroke-width="5" fill="none"/>'
    if i in (14, 15):
        marks = '<path d="M219 40h14l-14 14h14m5-32h10l-10 10h10" stroke="black" stroke-width="2" fill="none"/>'
    if i == 17:
        marks = '<path d="m224 149 6 3m-9 4 6 4" stroke="black" stroke-width="2"/>'
    transform = 'translate(256 0) scale(-1 1)' if mirror else ''
    svg(f'sheep_{i:02}', f'<g transform="{transform}"><g transform="translate(0 {body_shift})">{front}'
        f'<g transform="rotate({tilt} 171 95)">{face}</g>'
        f'<path d="{BODY}" fill="white" stroke="black" stroke-width="3" stroke-linejoin="round"/>'
        f'{marks}</g></g>')

for variant in range(4):
    face_color = 'white' if variant & 1 else 'black'
    eye_color = 'black' if variant & 1 else 'white'
    leg_color = 'white' if variant & 2 else 'black'
    legs = f'<path d="M71 139v27q8 12 17 0v-27M132 139v27q8 12 17 0v-27" fill="{leg_color}" stroke="black" stroke-width="3"/>'
    face = f'<path d="M171 65C192 62 224 74 232 100C238 125 211 143 183 137C160 133 153 108 163 88Z" fill="{face_color}" stroke="black" stroke-width="3"/>'
    face += f'<circle cx="208" cy="103" r="4" fill="{eye_color}"/><path d="M212 122q5 5 9 0" stroke="{eye_color}" fill="none" stroke-width="3"/>'
    svg(f'pair_{variant:02}', legs + face + f'<path d="{BODY}" fill="white" stroke="black" stroke-width="3"/>')

outline = 'fill="none" stroke="black" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"'
svg('grass', f'<g {outline}><path d="M24 43V8M24 28 10 13M24 35l14-19M12 43 6 27m30 16 7-14"/></g>',48,48)
# Three independent blades preserve exact one-unit stock changes at small sizes.
stock_blades = (
    'M22 43C12 38 7 25 4 16C15 20 22 31 22 43Z',
    'M24 43C18 30 20 14 25 4C31 17 30 32 24 43Z',
    'M26 43C25 31 34 20 43 15C40 30 35 40 26 43Z',
)
for stock in range(4):
    blades = []
    for index, path in enumerate(stock_blades):
        fill = 'black' if index < stock else 'white'
        blades.append(f'<path d="{path}" fill="{fill}" stroke="black" stroke-width="1.5" '
                      'stroke-linejoin="round"/>')
    svg(f'grass_stock_{stock}', ''.join(blades), 48, 48)

svg('heart', '<path d="M24 42 6 24C-3 13 11 3 24 16 37 3 51 13 42 24L24 42Z" fill="black"/>',48,48)
svg('heart_empty', f'<path d="M24 42 6 24C-3 13 11 3 24 16 37 3 51 13 42 24L24 42Z" {outline}/>',48,48)
svg('pet', f'<g {outline}><path d="M9 38 5 23q0-4 3-3l6 9V10q0-5 4-1v14-17q0-5 4-1v18-16q0-5 4-1v18-12q0-5 4-1v20l7-9q4-2 4 2L31 41H13Z"/></g>',48,48)
svg('call', f'<g {outline}><path d="M10 22q-8-9 1-10l6 3q7-10 14 0l6-3q9 1 1 10M13 21q0-8 11-8t11 8v12q-11 12-22 0Z"/><circle cx="19" cy="26" r="1"/><circle cx="29" cy="26" r="1"/><path d="m39 6 4-3m-3 12h5M20 34q4 3 8 0"/></g>',48,48)
svg('play', f'<g {outline}><path d="M14 12h20q6 0 8 9l3 15q0 7-6 4l-8-7H17l-8 7q-6 3-6-4l3-15q2-9 8-9Z"/><path d="M14 20v10m-5-5h10"/><circle cx="31" cy="22" r="2"/><circle cx="37" cy="28" r="2"/></g>',48,48)

for path in sorted(ART.glob('*.svg')):
    png = ART / f'{path.stem}.png'
    subprocess.run(['inkscape', str(path), '--export-type=png', '--export-background=white',
                    '--export-background-opacity=255', f'--export-filename={png}'], check=True, capture_output=True)
    im = Image.open(png).convert('L').point(lambda x: 255 if x >= 160 else 0).convert('1')
    width, height = im.size
    pixels = im.load()
    packed = bytearray()
    for y in range(height):
        for x in range(0, width, 8):
            packed.append(sum((1 << (7 - bit)) for bit in range(8) if x + bit < width and pixels[x + bit, y] == 0))
    (ART / f'{path.stem}.pbm').write_bytes(f'P4\n{width} {height}\n'.encode() + packed)
    png.unlink()
