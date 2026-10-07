"""Derive game palettes and timer props from the approved native sheep sources.

Development only: Pillow, numpy and scipy are used to preserve existing contours.
Firmware builds consume the committed PBMs without these dependencies.
"""
from pathlib import Path
import numpy as np
from PIL import Image
from scipy.ndimage import binary_erosion

ART = Path(__file__).resolve().parent.parent / 'data/habit_sheep/art'


def contour(mask):
    runs = []
    for y, row in enumerate(mask):
        edges = np.diff(np.r_[False, row, False].astype(int))
        for a, b in zip(np.where(edges == 1)[0], np.where(edges == -1)[0]):
            runs.append(f'M{a} {y}h{b-a}v1h{a-b}Z')
    return ''.join(runs)


def palettes():
    base = np.asarray(Image.open(ART / 'sheep_00.pbm').convert('L')) < 128
    interior = binary_erosion(base, iterations=3)
    yy, xx = np.indices(base.shape)
    face = interior & (yy >= 55) & (yy <= 163) & (xx >= 164)
    feet = interior & (yy >= 212)
    features = np.zeros_like(base)
    # Retain the approved eye and smile contours, inverted on the white face.
    for x0, y0, x1, y1 in [(230, 87, 251, 117), (279, 79, 299, 112), (256, 125, 288, 150)]:
        features[y0:y1, x0:x1] = ~base[y0:y1, x0:x1]
    for variant in range(4):
        mask = base.copy()
        if variant & 1:
            mask[face] = False
            mask[features] = True
        if variant & 2:
            mask[feet] = False
        source = '<!-- Palette derived from approved sheep_00.pbm; silhouette retained. -->'
        (ART / f'pair_{variant:02}.svg').write_text(
            '<svg xmlns="http://www.w3.org/2000/svg" width="384" height="288" viewBox="0 0 384 288">'
            + source + f'<path fill="black" d="{contour(mask)}"/></svg>\n')


def compose(name, source, prop, transform=''):
    text = (ART / f'sheep_{source:02}.svg').read_text()
    body = text[text.index('<g '):text.rindex('</svg>')]
    if transform:
        body = f'<g transform="{transform}">{body}</g>'
    (ART / f'{name}.svg').write_text(
        '<svg xmlns="http://www.w3.org/2000/svg" width="384" height="288" viewBox="0 0 384 288">'
        f'<!-- Approved sheep_{source:02} contours with original timer prop. -->{body}{prop}</svg>\n')


def timer_props():
    book = ('<g stroke="black" stroke-width="4" stroke-linejoin="round" fill="white">'
            '<path d="M142 210Q169 204 192 218Q215 204 242 210V260Q215 253 192 266Q169 253 142 260Z"/>'
            '<path d="M192 218V266M153 223l26 4m-26 8 26 4m24-12 26-4m-26 16 26-4" fill="none"/>'
            '</g><ellipse cx="142" cy="239" rx="9" ry="14" fill="black"/>'
            '<ellipse cx="242" cy="239" rx="9" ry="14" fill="black"/>')
    for index, source in enumerate((9, 0, 12)):
        prop = book if index == 0 else f'<g transform="translate({60 if index == 1 else 70} -20)">{book}</g>'
        compose(f'reading_{index:02}', source, prop)
    bulb = ('<g stroke="black" stroke-width="4" stroke-linecap="round" fill="white">'
            '<path d="M271 51C248 22 288 2 300 29Q306 42 293 52V63H272Z"/>'
            '<path d="M273 69h19M283 6V0m-32 17-8-4m64 4 8-4m-32 46V37l-8-8m8 8 8-8" fill="none"/></g>')
    bubble = ('<g stroke="black" stroke-width="4" fill="white">'
              '<ellipse cx="277" cy="28" rx="45" ry="22"/>'
              '<circle cx="242" cy="61" r="7"/><circle cx="230" cy="79" r="4"/>'
              '</g><circle cx="259" cy="28" r="3"/><circle cx="277" cy="28" r="3"/>'
              '<circle cx="295" cy="28" r="3"/>')
    for index, (source, prop) in enumerate(((10, bulb), (9, bubble), (2, bulb))):
        if index == 0:
            prop = f'<g transform="translate(-135 0)">{prop}</g>'
        compose(f'thinking_{index:02}', source, prop, 'translate(0 61) scale(.78)')


if __name__ == '__main__':
    palettes()
    timer_props()
