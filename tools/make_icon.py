# Generates src/app.ico from a tight crop of the app's own UI
# (shots/icon.png from tools/shots.sh icon). Every size (16/20/24/32/40/48/64/256)
# is the same enlarged artwork, so the shell (taskbar, desktop, alt-tab) does not
# fall back to a different, un-enlarged frame. Sizes under 256 are stored as BMP;
# 256 is PNG (the Windows ICO convention). Requires Pillow.
from PIL import Image, ImageDraw
import struct

SIZES = (16, 20, 24, 32, 40, 48, 64, 256)
# v0.5.2 crop: zoom ~10% so the red body fills the canvas (title chrome cropped).
CROP_SCALE = 0.90
CROP_TOP = 56
RADIUS_AT_256 = 28

def icon_master(shot_path, size=512):
    shot = Image.open(shot_path).convert("RGBA")
    sw = shot.width
    side = int(sw * CROP_SCALE)
    left = (sw - side) // 2
    top = CROP_TOP
    crop = shot.crop((left, top, left + side, top + side))
    return crop.resize((size, size), Image.Resampling.LANCZOS)

def mask_rounded(im):
    s = im.size[0]
    radius = max(1, round(RADIUS_AT_256 * s / 256))
    mask = Image.new("L", (s, s), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, s - 1, s - 1), radius=radius, fill=255)
    out = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    out.paste(im, (0, 0))
    out.putalpha(mask)
    return out

def ico_bmp(im):
    """32-bpp ICO BMP payload (BITMAPINFOHEADER + BGRA XOR + AND mask)."""
    im = im.convert("RGBA")
    w, h = im.size
    src = im.tobytes()
    xor = bytearray()
    for y in range(h - 1, -1, -1):
        off = y * w * 4
        for x in range(w):
            r, g, b, a = src[off + 4 * x:off + 4 * x + 4]
            xor += bytes((b, g, r, a))
    row_bytes = ((w + 31) // 32) * 4
    mask = bytearray()
    for y in range(h - 1, -1, -1):
        row = bytearray(row_bytes)
        off = y * w * 4
        for x in range(w):
            a = src[off + 4 * x + 3]
            if a < 128:
                row[x // 8] |= 0x80 >> (x % 8)
        mask += row
    header = struct.pack("<IIIHHIIIIII", 40, w, h * 2, 1, 32, 0, len(xor) + len(mask), 0, 0, 0, 0)
    return header + bytes(xor) + bytes(mask)

def save_ico(path, images):
    """Write a multi-size ICO: BMP for sizes < 256, PNG for 256."""
    blobs = []
    for im in images:
        w, h = im.size
        if w >= 256 and h >= 256:
            from io import BytesIO
            buf = BytesIO()
            im.save(buf, format="PNG")
            blobs.append((w, h, buf.getvalue(), True))
        else:
            blobs.append((w, h, ico_bmp(im), False))
    count = len(blobs)
    offset = 6 + 16 * count
    out = bytearray(struct.pack("<HHH", 0, 1, count))
    for w, h, data, is_png in blobs:
        out += struct.pack("<BBBBHHII",
                           w if w < 256 else 0, h if h < 256 else 0,
                           0, 0,
                           1, 32, len(data), offset)
        offset += len(data)
    for _, _, data, _ in blobs:
        out += data
    with open(path, "wb") as f:
        f.write(out)

def main():
    master = icon_master("shots/icon.png", 512)
    imgs = []
    for s in SIZES:
        im = master.resize((s, s), Image.Resampling.LANCZOS)
        imgs.append(mask_rounded(im))
    save_ico("src/app.ico", imgs)

    gap = 16
    preview_w = sum(s for s in SIZES) + gap * (len(SIZES) + 1)
    preview = Image.new("RGBA", (preview_w, 256 + 24), (74, 109, 140, 255))
    x = gap
    for im in imgs:
        preview.alpha_composite(im, (x, 256 - im.size[1] if im.size[0] < 256 else 0))
        x += im.size[0] + gap
    preview.save("docs/icon-preview.png")
    imgs[-1].save("docs/icon-256.png")

    # nearest-neighbour zoom of every size except 256, plus 256 at 1x
    small = imgs[:-1]
    zooms = [im.resize((im.size[0] * 8, im.size[1] * 8), Image.Resampling.NEAREST) for im in small]
    zw = sum(z.size[0] for z in zooms) + gap * (len(zooms) + 1)
    zh = max(z.size[1] for z in zooms) + 24
    zoom = Image.new("RGBA", (zw, zh), (74, 109, 140, 255))
    x = gap
    for z in zooms:
        zoom.alpha_composite(z, (x, 12))
        x += z.size[0] + gap
    zoom.save("shots/icon-small-zoom.png")

    # full size sheet used in the release notes folder
    sheet_w = max(preview.width, zoom.width)
    sheet = Image.new("RGBA", (sheet_w, preview.height + zoom.height + 32), (74, 109, 140, 255))
    sheet.alpha_composite(preview, (0, 0))
    sheet.alpha_composite(zoom, (0, preview.height + 16))
    sheet.save("shots/icon-size-sheet.png")

if __name__ == "__main__":
    main()
