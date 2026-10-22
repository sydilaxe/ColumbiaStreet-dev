#!/usr/bin/env python3
"""Draw the OnionOS app icon: a sliced breakbeat waveform over a brick-orange bar.

usage: mkicon.py OUT.png
"""
import math
import random
import sys
from PIL import Image, ImageDraw

COLORS = [(255, 90, 70), (255, 180, 40), (90, 220, 140), (70, 180, 255), (180, 120, 255)]


def main():
    s = 4
    size = 80
    im = Image.new("RGBA", (size * s, size * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 0, size * s - 1, size * s - 1], radius=16 * s, fill=(14, 15, 20, 255))
    d.rectangle([9 * s, 10 * s, 13 * s, 70 * s], fill=(255, 90, 42, 255))
    rnd = random.Random(7)
    # a drum loop: decaying hits, each slice in the next channel colour
    hits = [0, 6, 12, 17, 23, 29, 34, 40, 46, 51]
    x0, x1, mid = 18 * s, 72 * s, 40 * s
    for i, h in enumerate(hits):
        start = x0 + h * s
        end = x0 + (hits[i + 1] * s if i + 1 < len(hits) else 54 * s)
        col = COLORS[i % len(COLORS)]
        amp = (30 if i % 2 == 0 else 21) * s
        d.line([(start, mid - 22 * s), (start, mid + 22 * s)], fill=(60, 64, 80, 255), width=2)
        for x in range(start, min(end, x1), 2):
            t = (x - start) / (end - start)
            a = amp * math.exp(-3.2 * t) * (0.55 + 0.45 * rnd.random())
            d.line([(x, mid - a), (x, mid + a)], fill=col, width=3)
    im.resize((size, size), Image.LANCZOS).save(sys.argv[1])


if __name__ == "__main__":
    main()
