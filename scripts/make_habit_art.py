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


BODY = 'M47 141C25 148 12 131 22 115C7 99 17 80 34 79C27 60 46 46 65 52C69 33 92 32 105 43C121 27 142 31 151 47C172 37 191 49 191 66C211 67 215 88 202 99C214 119 199 136 181 134C171 154 149 156 133 143C116 156 98 156 86 145C72 154 54 154 47 141Z'


def character(i, face_color='black', leg_color='black'):
    rest = 12 <= i < 16
    eyes = 'white' if face_color == 'black' else 'black'
    tail = '<path d="M23 92q-17-15-21 0q-5 12 8 17q11 4 17-5" fill="white" stroke="black" stroke-width="2.5"/>'
    legs = '<path d="M52 130v39q6 9 13 0v-39M79 132v33q6 9 13 0v-33M144 130v39q6 9 13 0v-39M173 126v39q6 9 13 0v-39" fill="%s" stroke="black" stroke-width="2.5"/>' % leg_color
    if i in (4, 5, 7):
        legs = '<path d="m55 131-22 30q-2 9 9 9l25-30m12-10 11 30q7 9 15 0l-11-30m49 0-12 32q4 11 14 5l15-30m14-10 23 25q11 5 13-5l-22-29" fill="%s" stroke="black" stroke-width="2.5"/>' % leg_color
    if rest:
        legs = '<ellipse cx="55" cy="148" rx="23" ry="9" fill="black"/><ellipse cx="151" cy="148" rx="25" ry="9" fill="black"/>'
    if i == 14:
        legs = '<path d="m39 123-30 20q-6 12 8 13l36-19m15 0-30 18q-2 12 13 12l34-17m39-14 25 17q15 3 15-9l-27-18" fill="black"/>'
    if i == 15:
        legs = '<path d="m161 135 51 17q16 12-3 17l-60-19m27-20 53 9q18 8 1 17l-58-11" fill="black"/>' + legs
    if i == 6:
        legs = '<path d="m150 132 46 21q17 13-4 16l-55-21m-75-13 12 28q8 9 16 0l-14-32m40-10 3 39q8 10 16-1l-3-36" fill="black"/>'
    if i == 8:
        legs = '<path d="M57 127v42q8 9 16 0v-42M91 126v39q8 9 16 0v-39M170 85q25-20 28 1q2 23-22 17M167 114q26-14 28 6q-4 20-28 6" fill="black"/>'
    face = '<path d="M181 64q-16 15-10 43q5 38 26 40q24 2 27-33q3-37-15-49Z" fill="%s" stroke="black" stroke-width="2.5"/>' % face_color
    face += '<path d="M179 72q-28 4-33 25q1 16 16 7l24-22M211 72q24 4 33 22q3 17-12 12l-21-22" fill="%s"/>' % face_color
    closed = rest and i != 12 or i in (6, 8, 18)
    face += ('<path d="M183 100q5 7 10 0m9 0q5 7 10 0" fill="none" stroke="%s" stroke-width="2.5" stroke-linecap="round"/>' % eyes if closed else '<ellipse cx="187" cy="102" rx="3" ry="4" fill="%s"/><ellipse cx="207" cy="100" rx="3" ry="4" fill="%s"/>' % (eyes, eyes))
    face += '<path d="M190 126q7 8 14 0" fill="none" stroke="%s" stroke-width="2.5" stroke-linecap="round"/>' % eyes
    face += '<path d="M175 65q-8-12 4-16q2-12 13-7q8-9 16 0q13-2 13 10q10 8 0 16q-9 7-17 1q-9 7-15 0q-12 5-14-4Z" fill="white" stroke="black" stroke-width="2.5"/>'
    angle = {1: 8, 2: -15, 3: -25, 5: -8, 9: -90, 10: -55, 11: 20, 13: 20, 14: 45, 15: 15, 16: 60, 17: 10, 18: 15, 19: -15}.get(i, 0)
    face = '<g transform="rotate(%d 183 93)">%s</g>' % (angle, face)
    body = '<path d="%s" fill="white" stroke="black" stroke-width="2.5" stroke-linejoin="round"/>' % BODY
    if i == 9:
        face = '<g transform="translate(-25 -4)">%s</g>' % face
    if i == 3:
        face = '<g transform="translate(0 -8)">%s</g>' % face
    if i == 6:
        body = '<g transform="rotate(12 120 100)">%s</g>' % body
        face = '<g transform="translate(-10 18)">%s</g>' % face
    if i == 16:
        face = '<g transform="translate(-5 14)">%s</g>' % face
    if i == 8:
        body = '<g transform="rotate(-22 120 100)">%s</g>' % body
        face = '<g transform="translate(-6 -12)">%s</g>' % face
    scene = tail + legs + body
    if i == 8:
        scene += '<path d="M158 101q20-14 23 2q1 18-18 18m-18-26q-16-15-19 3q-1 15 16 16" fill="black"/>'
    if i == 19:
        scene += '<path d="M169 133q-11-18-3-29q8-8 16 1l7 34Z" fill="black"/>'
    scene += face
    if i == 7:
        scene = '<g transform="translate(8 -10) rotate(-12 128 100)">%s</g>' % scene
    if i == 13:
        scene = '<g transform="translate(12 18) scale(.92 .85)">%s</g>' % scene
    if i == 14:
        scene = '<g transform="rotate(8 128 100)">%s</g>' % scene
    if i == 16:
        scene += '<path d="M212 179l-7-15 12 9 4-21 4 21 12-12-5 18m-43 0-7-12 12 6 4-14 4 20" stroke="black" stroke-width="2.5" fill="none"/>'
    if i == 17:
        scene += '<path d="m210 132 19 9m-18-7 12 16m-12-16 21-1" stroke="black" stroke-width="2"/>'
    return '<g transform="translate(12 10) scale(.9)">%s</g>' % scene


# Approved sheep SVG contours are source assets; do not redraw them here.
for variant in range(4):
    svg(f'pair_{variant:02}', character(0, 'white' if variant & 1 else 'black',
                                     'white' if variant & 2 else 'black'))

outline = 'fill="none" stroke="black" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"'
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
                    '--export-background-opacity=255', '--export-width=384' if path.stem.startswith(('sheep_', 'pair_')) else '--export-width=192', f'--export-filename={png}'], check=True, capture_output=True)
    im = Image.open(png).convert('L').point(lambda x: 255 if x >= 160 else 0).convert('1')
    width, height = im.size
    pixels = im.load()
    packed = bytearray()
    for y in range(height):
        for x in range(0, width, 8):
            packed.append(sum((1 << (7 - bit)) for bit in range(8) if x + bit < width and pixels[x + bit, y] == 0))
    (ART / f'{path.stem}.pbm').write_bytes(f'P4\n{width} {height}\n'.encode() + packed)
    png.unlink()
