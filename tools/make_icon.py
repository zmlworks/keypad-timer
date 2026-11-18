# Generates src/app.ico. The 256px image is a tight crop of the app's own UI
# (shots/icon.png from tools/shots.sh icon). Sizes 48/32/24/16 are hand-drawn
# so the red timer and LCD stay legible when the icon is small.
# Requires Pillow.
from PIL import Image, ImageDraw

SEG = [0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F]
CASE = (190, 22, 42, 255)
CASE_EDGE = (110, 4, 20, 255)
BEZEL = (34, 30, 32, 255)
PANEL = (176, 188, 156, 255)
INK = (24, 28, 22, 255)
RED = (220, 16, 24, 255)
CREAM = (246, 212, 120, 255)
KEY = (226, 52, 66, 255)
KEY5 = (204, 34, 52, 255)
ADD = (140, 12, 30, 255)

def rr(draw, box, radius, fill):
    draw.rounded_rectangle(box, radius=max(1, radius), fill=fill)

def body(size, radius=None):
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    if radius is None:
        radius = max(2, size // 8)
    # slightly darker rim, then the case face — fills the canvas
    rr(d, (0, 0, size - 1, size - 1), radius, CASE_EDGE)
    inset = 1 if size <= 32 else 2
    rr(d, (inset, inset, size - 1 - inset, size - 1 - inset), max(1, radius - inset), CASE)
    return im, d

def seg_digit(draw, x, y, w, h, t, digit, color):
    """Axis-aligned 7-segment digit (no slant) — clearer at small sizes."""
    if digit < 0:
        return
    bits = SEG[digit]
    x, y, w, h, t = int(round(x)), int(round(y)), max(3, int(round(w))), max(5, int(round(h))), max(1, int(round(t)))
    mid = y + h // 2
    # horizontal: a, g, d
    def horiz(cy):
        draw.rectangle((x + t, cy - t // 2, x + w - 1 - t, cy - t // 2 + t - 1), fill=color)
    # vertical: x0 = left, x1 = right; y0..y1
    def vert(xv, y0, y1):
        draw.rectangle((xv, y0, xv + t - 1, y1), fill=color)
    if bits & 0x01: horiz(y + t // 2)              # a
    if bits & 0x40: horiz(mid)                      # g
    if bits & 0x08: horiz(y + h - 1 - t // 2)       # d
    if bits & 0x20: vert(x, y + t, mid - 1)         # f
    if bits & 0x02: vert(x + w - t, y + t, mid - 1) # b
    if bits & 0x10: vert(x, mid + 1, y + h - 1 - t) # e
    if bits & 0x04: vert(x + w - t, mid + 1, y + h - 1 - t)  # c

def colon(draw, x, y, h, color, dot=None):
    if dot is None:
        dot = max(1, h // 10)
    for cy in (y + h * 30 // 100, y + h * 68 // 100):
        draw.rectangle((x, cy, x + dot - 1, cy + dot - 1), fill=color)

def keys_grid(draw, x, y, w, h, rows, cols, colors):
    gap = 1 if w < 80 else max(1, w // 40)
    cw = max(1, (w - gap * (cols - 1)) // cols)
    rh = max(1, (h - gap * (rows - 1)) // rows)
    i = 0
    for r in range(rows):
        for c in range(cols):
            x0 = x + c * (cw + gap)
            y0 = y + r * (rh + gap)
            rad = 1 if min(cw, rh) < 8 else max(1, min(cw, rh) // 6)
            rr(draw, (x0, y0, x0 + cw - 1, y0 + rh - 1), rad, colors[i % len(colors)])
            i += 1

# 7 8 9 / 4 5 6  (cream on 8, 4, 6)
KEYS_2 = [KEY, CREAM, KEY, CREAM, KEY5, CREAM]
# plus a top add-button row
KEYS_3 = [ADD, ADD, ADD] + KEYS_2

def paint_small(size):
    """Hand-tuned icon: big LCD, bolder digits, no title chrome."""
    im, d = body(size)
    m = 1 if size <= 16 else 2
    # LCD takes a bit more than half the canvas so the digits stay chunky
    lcd_bottom = {16: 8, 24: 13, 32: 18, 48: 28}.get(size, size * 14 // 24)
    bezel = (m, m, size - 1 - m, lcd_bottom)
    rr(d, bezel, 1 if size <= 24 else 2, BEZEL)
    inset = 1
    panel = (bezel[0] + inset, bezel[1] + inset, bezel[2] - inset, bezel[3] - inset)
    rr(d, panel, 1, PANEL)

    px, py, pr, pb = panel
    pw, ph = pr - px + 1, pb - py + 1
    pad = 0 if size <= 16 else 1 if size <= 24 else 2

    def fit_digits(n_digits, n_colons, th):
        """Return (dw, dh, th, col, gap, x0, y0) that fits, or None."""
        dh = ph - 2 * pad
        if dh < 4:
            return None
        th = max(1, min(th, dh // 5))
        col = 1
        gap = 1
        # dw * n + gap * (n-1) + (col + gap) * colons <= pw - 2
        budget = pw - 2 - gap * (n_digits - 1) - (col + gap) * n_colons
        dw = budget // n_digits
        min_dw = 2 * th + 1
        if dw < min_dw:
            return None
        dw = min(dw, max(min_dw, dh * 3 // 5))
        total = dw * n_digits + gap * (n_digits - 1) + (col + gap) * n_colons
        x0 = px + (pw - total) // 2
        y0 = py + (ph - dh) // 2
        return dw, dh, th, col, gap, x0, y0

    def blit(digits, colors, th):
        n_colons = 1 if len(digits) == 3 else (2 if len(digits) == 6 else 0)
        layout = fit_digits(len(digits), n_colons, th)
        if not layout:
            return False
        dw, dh, th, col, gap, x0, y0 = layout
        colon_after = {3: {0}, 6: {1, 3}}.get(len(digits), set())
        x = x0
        for i, val in enumerate(digits):
            seg_digit(d, x, y0, dw, dh, th, val, colors[i])
            x += dw + gap
            if i in colon_after:
                colon(d, x, y0, dh, INK, th)
                x += col + gap
        return True

    # Prefer the most complete readout that still stays chunky
    if size >= 32:
        # 32 and 48: "0:30" with chunky digits (six digits get too thin to read)
        ok = blit([0, 3, 0], [INK, RED, INK], 2) or blit([0, 3, 0], [INK, RED, INK], 1)
    else:
        # 16 and 24: just "30" — two large digits, 3 in red
        ok = blit([3, 0], [RED, INK], 1)
    if not ok:
        raise RuntimeError(f"could not fit digits at size {size}")

    # keys under the LCD, filling the remaining body
    ky = lcd_bottom + (2 if size >= 32 else 1)
    kx = m + (1 if size >= 24 else 0)
    kw = size - 2 * kx
    kh = size - m - 1 - ky
    if kh >= 3 and kw >= 5:
        keys_grid(d, kx, ky, kw, kh, 2, 3, KEYS_2)
    return im


def icon_256(shot_path):
    shot = Image.open(shot_path).convert("RGBA")
    # 2x screenshot is 480 x 836. Use the full width and a square that
    # reaches through the 4-5-6 row, then scale up slightly so the red
    # body fills the 256 canvas (little or no empty margin).
    sw = shot.width  # 480
    # Cover-crop a square a bit smaller than the body so we zoom in ~10%
    side = int(sw * 0.90)  # 432
    # centre horizontally (cuts a sliver of the side radius), start a few
    # pixels down so the title bar is slightly cropped and the LCD grows
    left = (sw - side) // 2
    # Title-bar buttons occupy logical y=6..26 (2x: 12..52). Start just below
    # them so a sliver of red case sits above the LCD, with no leftover chrome.
    top = 56
    crop = shot.crop((left, top, left + side, top + side))
    big = crop.resize((256, 256), Image.Resampling.LANCZOS)
    # Mask to a rounded square that reaches the edges (radius matches the
    # body, but the zoom already ate most of the transparent corners).
    mask = Image.new("L", (256, 256), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, 255, 255), radius=28, fill=255)
    out = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    out.paste(big, (0, 0))
    out.putalpha(mask)
    # Where the mask cut a transparent corner, leave it transparent; fill
    # any leftover semi-transparent fringe near the edge with case red so
    # the motif reads as a solid red timer at small preview sizes too.
    return out

def main():
    imgs = [icon_256("shots/icon.png")]
    for s in (48, 32, 24, 16):
        imgs.append(paint_small(s))
    imgs[0].save("src/app.ico", sizes=[im.size for im in imgs], append_images=imgs[1:])
    preview = Image.new("RGBA", (256 + 48 + 32 + 24 + 16 + 5 * 16, 256), (74, 109, 140, 255))
    x = 8
    for im in imgs:
        preview.alpha_composite(im, (x, 256 - im.size[1] - 8 if im.size[0] < 256 else 0))
        x += im.size[0] + 16
    preview.save("docs/icon-preview.png")
    imgs[0].save("docs/icon-256.png")
    # also write a nearest-neighbour zoom of the small sizes, for checking
    zoom = Image.new("RGBA", (16 * 8 + 24 * 8 + 32 * 8 + 48 * 8 + 5 * 20, 48 * 8 + 24), (74, 109, 140, 255))
    x = 10
    for im in imgs[1:][::-1]:  # 16,24,32,48? imgs is 256,48,32,24,16 — use last 4 reversed
        pass
    x = 10
    for im in (imgs[4], imgs[3], imgs[2], imgs[1]):  # 16, 24, 32, 48
        z = im.resize((im.size[0] * 8, im.size[1] * 8), Image.Resampling.NEAREST)
        zoom.alpha_composite(z, (x, 12))
        x += z.size[0] + 20
    zoom.save("shots/icon-small-zoom.png")

if __name__ == "__main__":
    main()
