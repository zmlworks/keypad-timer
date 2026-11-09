# Generates src/app.ico (original artwork, MIT). Requires Pillow.
from PIL import Image, ImageDraw
S = 256
im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(im)
d.rounded_rectangle((16, 8, 240, 248), 44, fill=(196, 22, 42, 255), outline=(110, 0, 14, 255), width=6)
d.rounded_rectangle((24, 14, 232, 110), 36, fill=(232, 90, 100, 255))
d.rounded_rectangle((40, 36, 216, 118), 14, fill=(26, 22, 24, 255))
d.rounded_rectangle((50, 46, 206, 108), 8, fill=(182, 194, 162, 255))
# "5" on the LCD as seven segments
seg = (30, 34, 26)
x0, y0, w, h, t = 108, 52, 40, 50, 9
d.polygon([(x0, y0), (x0 + w, y0), (x0 + w - t, y0 + t), (x0 + t, y0 + t)], fill=seg)
d.polygon([(x0, y0), (x0 + t, y0 + t), (x0 + t, y0 + h / 2 - 3), (x0, y0 + h / 2)], fill=seg)
d.rectangle((x0 + 3, y0 + h / 2 - 4, x0 + w - 3, y0 + h / 2 + 4), fill=seg)
d.polygon([(x0 + w, y0 + h / 2), (x0 + w, y0 + h), (x0 + w - t, y0 + h - t), (x0 + w - t, y0 + h / 2 + 3)], fill=seg)
d.polygon([(x0, y0 + h), (x0 + w, y0 + h), (x0 + w - t, y0 + h - t), (x0 + t, y0 + h - t)], fill=seg)
for r in range(3):
    for c in range(3):
        x, y = 44 + c * 60, 132 + r * 36
        d.rounded_rectangle((x, y, x + 48, y + 28), 8, fill=(250, 214, 200, 255) if (r + c) % 2 else (236, 120, 120, 255))
im.save("src/app.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (256, 256)])
im.save("docs/icon-256.png")
