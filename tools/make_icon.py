# Generates src/app.ico: a red kitchen timer / stopwatch (original artwork, MIT). Requires Pillow.
from PIL import Image, ImageDraw

RED, DARK, FACE, INK = (214, 30, 48), (130, 6, 24), (255, 250, 240), (40, 30, 34)

def draw(size):
    S = size * 4                      # draw 4x, then downscale for smooth edges
    u = S / 64.0                      # 64-unit design grid
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    b = lambda x0, y0, x1, y1: [v * u for v in (x0, y0, x1, y1)]
    d.rounded_rectangle(b(26, 2, 38, 10), 2 * u, fill=DARK)        # crown button
    d.rectangle(b(29, 8, 35, 14), fill=DARK)                         # stem
    d.polygon([(p[0] * u, p[1] * u) for p in [(47, 9), (55, 17), (51, 21), (43, 13)]], fill=DARK)  # side button
    d.ellipse(b(7, 13, 57, 63), fill=DARK)                       # body (with darker rim)
    d.ellipse(b(10, 16, 54, 60), fill=RED)
    d.ellipse(b(15, 21, 49, 55), fill=FACE)                      # clock face
    if size >= 32:
        for x0, y0, x1, y1 in [(31, 22, 33, 27), (31, 49, 33, 54), (16, 37, 21, 39), (43, 37, 48, 39)]:
            d.rectangle(b(x0, y0, x1, y1), fill=RED)                 # 12 / 6 / 9 / 3 ticks
    w = max(2.6, 64 / size * 1.3) * u                                # hands thicker when small
    d.line([(32 * u, 38 * u), (32 * u, 25 * u)], fill=INK, width=int(w))
    d.line([(32 * u, 38 * u), (42 * u, 32 * u)], fill=RED, width=int(w))
    d.ellipse(b(29.5, 35.5, 34.5, 40.5), fill=INK)
    return im.resize((size, size), Image.LANCZOS)

imgs = [draw(s) for s in (256, 48, 32, 24, 16)]
imgs[0].save("src/app.ico", sizes=[im.size for im in imgs], append_images=imgs[1:])
preview = Image.new("RGBA", (256 + 48 + 32 + 24 + 16 + 5 * 16, 256), (74, 109, 140, 255))
x = 8
for im in imgs:
    preview.alpha_composite(im, (x, 256 - im.size[1] - 8 if im.size[0] < 256 else 0))
    x += im.size[0] + 16
preview.save("docs/icon-preview.png")
imgs[0].save("docs/icon-256.png")
