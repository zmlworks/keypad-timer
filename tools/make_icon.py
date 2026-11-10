# Generates src/app.ico from the app's own rendering: title bar down to the 4-5-6 key row
# of the SHOTS build's "icon" state (shots/icon.png, 2x). Run tools/shots.sh icon first. Requires Pillow.
from PIL import Image

shot = Image.open("shots/icon.png").convert("RGBA")
crop = shot.crop((0, 0, shot.width, 270 * shot.width // 240))   # 4-5-6 row ends at y=270 (logical)
side = max(crop.size)
square = Image.new("RGBA", (side, side), (0, 0, 0, 0))           # pad to square, transparent, centred
square.alpha_composite(crop, ((side - crop.width) // 2, (side - crop.height) // 2))

imgs = [square.resize((s, s), Image.LANCZOS) for s in (256, 48, 32, 24, 16)]
imgs[0].save("src/app.ico", sizes=[im.size for im in imgs], append_images=imgs[1:])
preview = Image.new("RGBA", (256 + 48 + 32 + 24 + 16 + 5 * 16, 256), (74, 109, 140, 255))
x = 8
for im in imgs:
    preview.alpha_composite(im, (x, 256 - im.size[1] - 8 if im.size[0] < 256 else 0))
    x += im.size[0] + 16
preview.save("docs/icon-preview.png")
imgs[0].save("docs/icon-256.png")
