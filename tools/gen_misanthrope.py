#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 the Rheia authors
"""(Rheia) the boot screen's picture: Pieter Bruegel the Elder, The Misanthrope (1568, tempera on canvas, Museo di
Capodimonte, Naples; public domain), as line art in screen pixels -> firmware/src/misanthrope.h.

  python3 tools/gen_misanthrope.py PAINTING.jpg [PREVIEW.png]

PAINTING.jpg: Wikimedia Commons' 960 px rendition of "Pieter Bruegel d. Ä. 035.jpg":
  https://upload.wikimedia.org/wikipedia/commons/thumb/8/8f/Pieter_Bruegel_d._%C3%84._035.jpg/960px-Pieter_Bruegel_d._%C3%84._035.jpg
The cloaked man's outline is traced from the painting (its dark area); the rest (his face, hands and cloak's folds,
the thief in his glass globe, the purse, the landscape, the windmill, the shepherd and his sheep, the caltrops) is
drawn by hand over the painting's pixels (the coordinates below, in the 960 px picture). Needs numpy, Pillow,
scikit-image; the header it writes is kept in the repository, so a build needs none of this.

The header: polylines, each [class, n, x0, y0, ... x(n-1), y(n-1)] in screen pixels (uint8), the list ending with
MZ_END. The classes are drawn in the theme's colours (splash.c misanthrope_draw).
"""
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
OUT = HERE.parent / "firmware" / "src" / "misanthrope.h"

CX, CY, R = 482, 476, 452             # the painting's circle in the 960 px picture
SX, SY, SR = 120, 120, 117            # on the screen: the circle's centre and radius (the name and version go
                                      # where Bruegel's inscription is, splash.c)
CLASSES = ("MZ_FRAME", "MZ_LAND", "MZ_FLOCK", "MZ_MAN", "MZ_THIEF", "MZ_GLOBE", "MZ_PURSE")


def scr(p):
    k = SR / R
    return (SX + (p[0] - CX) * k, SY + (p[1] - CY) * k)


# hand tracings were read off 2x close-ups of the painting with a grid; each helper maps a close-up's pixels back
def T(*pts):                          # the thief close-up: (520, 340) at 2x
    return [(520 + x / 2, 340 + y / 2) for x, y in pts]


def C(*pts):                          # the thief, closer: (520, 380) at 2x
    return [(520 + x / 2, 380 + y / 2) for x, y in pts]


def H(*pts):                          # the hooded head: (280, 110) at 2x
    return [(280 + x / 2, 110 + y / 2) for x, y in pts]


def B(*pts):                          # the right background: (440, 170) at 2x
    return [(440 + x / 2, 170 + y / 2) for x, y in pts]


def L(*pts):                          # the left background: (20, 100) at 2x
    return [(20 + x / 2, 100 + y / 2) for x, y in pts]


def ellipse(cx, cy, rx, ry, a0=0, a1=360, n=40, rot=0):
    out = []
    for i in range(n + 1):
        a = math.radians(a0 + (a1 - a0) * i / n)
        x, y = rx * math.cos(a), ry * math.sin(a)
        c, s = math.cos(math.radians(rot)), math.sin(math.radians(rot))
        out.append((cx + x * c - y * s, cy + x * s + y * c))
    return out


def drawn():
    P = []
    add = lambda cls, pts: P.append((cls, pts))            # noqa: E731
    # the man: his face and beard in the hood, the hood's point, the cape, his hands, the cloak's folds
    add("MZ_MAN", H((70, 128), (100, 140), (92, 165), (100, 186), (108, 212), (112, 250), (116, 292)))
    add("MZ_MAN", H((128, 134), (146, 166), (153, 206), (146, 246), (130, 278), (116, 292)))
    add("MZ_MAN", H((122, 214), (124, 272)))
    add("MZ_MAN", H((240, 62), (264, 116), (302, 160), (336, 196)))
    add("MZ_MAN", H((100, 346), (160, 342), (222, 332), (290, 312), (352, 288), (414, 262)))
    add("MZ_MAN", H((150, 226), (196, 236), (246, 234), (298, 214)))
    add("MZ_MAN", [(292, 402), (306, 399), (315, 412), (313, 430), (303, 442), (292, 438), (287, 421), (292, 402)])
    add("MZ_MAN", [(296, 416), (309, 419)])
    add("MZ_MAN", [(338, 460), (334, 560), (330, 660), (336, 760)])
    add("MZ_MAN", [(400, 300), (416, 420), (424, 540), (418, 660), (404, 770)])
    add("MZ_MAN", [(470, 330), (488, 430), (508, 520), (540, 610), (586, 668)])
    add("MZ_MAN", [(368, 280), (360, 360), (352, 440)])
    # the thief, crouched in his glass globe, cutting the purse
    add("MZ_THIEF", T((350, 200), (380, 175), (420, 170), (460, 185), (478, 220), (475, 260), (462, 295), (430, 325),
                      (395, 330), (365, 310), (348, 270), (345, 230), (350, 200)))
    add("MZ_THIEF", T((374, 240), (386, 238)))
    add("MZ_THIEF", T((414, 246), (426, 248)))
    add("MZ_THIEF", T((378, 288), (395, 296), (410, 290)))
    add("MZ_THIEF", C((362, 236), (395, 250), (440, 240)))
    add("MZ_THIEF", C((470, 215), (560, 280), (620, 350), (635, 420), (630, 500), (630, 560), (615, 620), (605, 690),
                      (620, 760), (625, 790), (580, 810), (545, 805), (570, 790), (550, 740), (500, 690), (440, 640),
                      (415, 620), (420, 600), (470, 560), (520, 525), (580, 500)))
    add("MZ_THIEF", C((350, 215), (300, 190), (240, 182), (200, 200), (165, 250), (120, 300), (85, 340), (60, 360),
                      (70, 385), (95, 385), (110, 365), (150, 320), (190, 272), (230, 262), (300, 250), (332, 256),
                      (330, 330), (328, 390)))
    add("MZ_THIEF", C((548, 350), (470, 370), (400, 395), (330, 420), (270, 430), (215, 440), (195, 470), (205, 495),
                      (250, 490), (320, 470), (390, 455), (460, 440), (556, 424)))
    add("MZ_THIEF", C((250, 500), (245, 580), (255, 640), (262, 680), (215, 685), (205, 695), (250, 700), (310, 700),
                      (325, 690), (320, 660), (305, 580), (310, 505), (380, 520), (450, 525), (520, 522)))
    add("MZ_THIEF", T((50, 500), (82, 436)))
    # the globe and its cross; the purse
    add("MZ_GLOBE", ellipse(752, 512, 106, 108, n=48))
    add("MZ_GLOBE", T((240, 45), (312, 140)))
    add("MZ_GLOBE", T((310, 48), (245, 138)))
    add("MZ_GLOBE", T((300, 140), (330, 172)))
    add("MZ_PURSE", T((130, 500), (175, 488), (210, 510), (215, 545), (195, 570), (160, 575), (138, 550), (130, 500)))
    add("MZ_PURSE", T((100, 460), (130, 500)))
    # the landscape: the horizon, trees, the windmill, the bare tree, the ground; the caltrops on the path
    add("MZ_LAND", [(488, 262), (560, 262), (640, 262), (760, 262), (900, 268)])
    add("MZ_LAND", [(490, 262), (492, 246), (505, 238), (520, 241), (535, 232), (550, 236), (565, 231), (580, 240),
                    (587, 250)])
    add("MZ_LAND", [(620, 262), (622, 246), (634, 240), (644, 248), (646, 262)])
    add("MZ_LAND", B((300, 92), (355, 92), (355, 150), (300, 150), (300, 92)))
    add("MZ_LAND", B((296, 92), (326, 72), (358, 92)))
    add("MZ_LAND", B((326, 150), (326, 185)))
    add("MZ_LAND", B((292, 140), (348, 20)))
    add("MZ_LAND", B((298, 60), (338, 124)))
    add("MZ_LAND", [(852, 264), (851, 226)])
    add("MZ_LAND", [(851, 240), (842, 228)])
    add("MZ_LAND", [(851, 236), (860, 226)])
    add("MZ_LAND", L((330, 50), (370, 120), (410, 170), (440, 190), (470, 230), (490, 260), (500, 300)))
    add("MZ_LAND", L((60, 400), (90, 440), (120, 490), (150, 560), (170, 620), (185, 660)))
    add("MZ_LAND", L((80, 395), (115, 430), (150, 480), (175, 540), (195, 600), (215, 650)))
    add("MZ_LAND", L((165, 668), (200, 672), (240, 664)))
    add("MZ_LAND", [(236, 262), (290, 262)])
    add("MZ_LAND", [(240, 250), (250, 240), (262, 246), (272, 238), (284, 248), (288, 262)])
    add("MZ_LAND", [(60, 630), (150, 662), (262, 690)])
    add("MZ_LAND", ellipse(92, 546, 10, 6, n=8))
    for x, y in ((150, 752), (164, 776), (232, 745)):
        add("MZ_LAND", [(x - 7, y - 7), (x + 7, y + 7)])
        add("MZ_LAND", [(x - 7, y + 7), (x + 7, y - 7)])
    # the shepherd and his flock
    add("MZ_FLOCK", B((622, 176), (632, 172), (640, 180)))
    add("MZ_FLOCK", B((630, 182), (618, 220), (630, 262)))
    add("MZ_FLOCK", B((640, 186), (656, 220), (644, 262)))
    add("MZ_FLOCK", B((612, 262), (626, 200)))
    for x, y in ((520, 285), (537, 297), (590, 290), (642, 292), (667, 287), (685, 277), (727, 270), (732, 297),
                 (795, 303), (810, 305), (837, 315), (840, 275), (905, 310), (780, 270), (880, 282)):
        add("MZ_FLOCK", [(x - 8, y), (x + 6, y)])
        add("MZ_FLOCK", [(x + 6, y - 4), (x + 10, y - 4)])
    add("MZ_FRAME", ellipse(CX, CY, R, R, n=72))
    return P


FACE = [(318, 160), (330, 168), (352, 175), (360, 200), (358, 240), (352, 282), (342, 292), (330, 270), (326, 230),
        (316, 196)]
CHEST = [(334, 280), (400, 280), (402, 360), (336, 362)]
HOOD = [(316, 196), (312, 175), (315, 158), (330, 137), (355, 125), (380, 125), (400, 140), (420, 170), (440, 200),
        (440, 236), (360, 240), (350, 200)]
LEFT = [(335, 276), (348, 276), (348, 440), (300, 446), (292, 400), (310, 330)]          # his lit shoulder


def cloak(painting):
    """the cloaked man's outline: the painting's dark area around him, smoothed"""
    from skimage import color, filters, measure, morphology
    a = np.asarray(Image.open(painting).convert("RGB")).astype(float) / 255
    v = filters.gaussian(color.rgb2hsv(a)[..., 2], 2)
    yy, xx = np.mgrid[:v.shape[0], :v.shape[1]]
    m = (v < 0.30) & ((xx - CX) ** 2 + (yy - CY) ** 2 < (R - 12) ** 2) & (xx > 260)
    m = morphology.opening(m, morphology.disk(4))
    lab = measure.label(m)
    m = lab == lab[500, 420]
    for poly in (FACE, CHEST, HOOD, LEFT):                             # the light face, lit cloth, the blue hood: his
        im = Image.new("1", (m.shape[1], m.shape[0]), 0)
        ImageDraw.Draw(im).polygon(poly, fill=1)
        m |= np.asarray(im, bool)
    m = morphology.closing(m, morphology.disk(6))
    m = filters.gaussian(m.astype(float), 3) > 0.5
    c = max(measure.find_contours(m.astype(float), 0.5), key=len)
    c = measure.approximate_polygon(c, 2.5)
    return [(float(x), float(y)) for y, x in c]


def encode(paths):
    out, kept = [], []
    for cls, pts in paths:
        q = []
        for p in pts:
            x, y = scr(p)
            x, y = int(round(x)), int(round(y))
            if not (0 <= x < 240 and 0 <= y < 240):
                raise SystemExit(f"{cls}: {p} is off the screen")
            if not q or q[-1] != (x, y):
                q.append((x, y))
        if len(q) == 1:
            q.append(q[0])
        if len(q) > 255:
            raise SystemExit(f"{cls}: {len(q)} points (255 at most)")
        kept.append((cls, q))
        out += [CLASSES.index(cls), len(q)] + [c for xy in q for c in xy]
    return out + [255], kept


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    paths = [("MZ_MAN", cloak(sys.argv[1]))] + drawn()
    data, kept = encode(paths)
    body = ",\n".join("    " + ", ".join(f"{b:3d}" for b in data[i:i + 20]) for i in range(0, len(data), 20))
    enums = "".join(f"#define {c} {i}u\n" for i, c in enumerate(CLASSES))
    OUT.write_text(f"""/* SPDX-License-Identifier: GPL-3.0-only */
/* Generated by tools/gen_misanthrope.py; do not edit. (Rheia) the boot screen's picture: Pieter Bruegel the Elder,
 * The Misanthrope (1568, public domain), as line art: polylines [class, n, x, y ...] in screen pixels, then MZ_END.
 * {len(kept)} lines, {sum(len(q) for _, q in kept)} points, {len(data)} bytes. */
{enums}#define MZ_END 255u
#define MZ_CX {SX}
#define MZ_CY {SY}
#define MZ_R {SR}
static const uint8_t MISANTHROPE[{len(data)}] = {{
{body}
}};
""", encoding="utf-8")
    print(f"{OUT}: {len(kept)} lines, {len(data)} bytes")
    if len(sys.argv) > 2:                                  # a preview, 3x, one colour per class
        cols = {"MZ_FRAME": (150, 143, 134), "MZ_LAND": (150, 143, 134), "MZ_FLOCK": (200, 193, 182),
                "MZ_MAN": (236, 230, 220), "MZ_THIEF": (176, 206, 250), "MZ_GLOBE": (246, 242, 234),
                "MZ_PURSE": (200, 36, 32)}
        im = Image.new("RGB", (240, 240), (12, 11, 10))
        d = ImageDraw.Draw(im)
        for cls, q in kept:
            d.line(q, fill=cols[cls], width=1)
        im.resize((720, 720), Image.NEAREST).save(sys.argv[2])


if __name__ == "__main__":
    main()
