#!/usr/bin/env python3
"""Composite SHOTS PNGs onto the gradient background and build docs/overview.png + docs/compact.png."""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SHOTS = ROOT / "shots"
DOCS = ROOT / "docs"

def gradient(size, top=(58, 90, 122), bot=(200, 216, 192)):
    w, h = size
    im = Image.new("RGB", size)
    px = im.load()
    for y in range(h):
        t = y / max(1, h - 1)
        c = tuple(int(round(top[i] + (bot[i] - top[i]) * t)) for i in range(3))
        for x in range(w):
            px[x, y] = c
    return im

def ensure_bg():
    bg = SHOTS / "bg.png"
    if not bg.exists():
        gradient((520, 860)).save(bg)
    return Image.open(bg).convert("RGB")

def composite_one(name, off=(20, 20)):
    bg = ensure_bg().copy()
    fg = Image.open(SHOTS / f"{name}.png").convert("RGBA")
    bg.paste(fg, off, fg)
    bg.save(SHOTS / f"c_{name}.png")
    return bg, fg

def overview(names, panel_w=210, margin=12, gap=20):
    shots = [Image.open(SHOTS / f"{n}.png").convert("RGBA") for n in names]
    # scale each 2x shot to panel_w, keep aspect
    scaled = []
    for im in shots:
        h = round(im.height * panel_w / im.width)
        scaled.append(im.resize((panel_w, h), Image.Resampling.LANCZOS))
    ph = scaled[0].height
    w = margin * 2 + len(scaled) * panel_w + (len(scaled) - 1) * gap
    h = margin * 2 + ph
    canvas = gradient((w, h))
    x = margin
    for im in scaled:
        canvas.paste(im, (x, margin), im)
        x += panel_w + gap
    canvas.save(DOCS / "overview.png")
    SHOTS.joinpath(f"overview-0.5.4.png").parent.mkdir(exist_ok=True)
    canvas.save(SHOTS / "overview-0.5.4.png")
    print("overview", canvas.size)

def compact():
    fg = Image.open(SHOTS / "compact.png").convert("RGBA")
    canvas = gradient((520, 320))
    canvas.paste(fg, (20, 20), fg)
    canvas.save(DOCS / "compact.png")
    print("compact", canvas.size)

SHOWCASE = ["five", "ninety", "entered", "running", "stopwatch", "over"]

if __name__ == "__main__":
    import sys
    names = sys.argv[1:] or SHOWCASE
    for n in names:
        p = SHOTS / f"{n}.png"
        if p.exists():
            composite_one(n)
    present = [n for n in SHOWCASE if (SHOTS / f"{n}.png").exists()]
    if present:
        overview(present, panel_w=230, margin=12, gap=18)
    if (SHOTS / "compact.png").exists():
        compact()
