#!/usr/bin/env python3
"""Generate the m8c PS4 icon (512x512): M8 device face with tracker screen."""
from PIL import Image, ImageDraw, ImageFilter

S = 512
AMBER = (255, 159, 46, 255)
AMBER_DIM = (82, 52, 22, 255)
SCREEN_BG = (5, 7, 10, 255)
ROW_GRAY = (150, 156, 166, 255)
BODY = (35, 38, 45, 255)
BODY_EDGE = (58, 63, 73, 255)


def rounded(draw, box, r, **kw):
    draw.rounded_rectangle(box, radius=r, **kw)


def px_letter_m8(draw, x, y, s, color):
    """Blocky 'M8' pixel text, each pixel s×s."""
    m = [
        "X...X",
        "XX.XX",
        "X.X.X",
        "X...X",
        "X...X",
    ]
    eight = [
        "XXXX",
        "X..X",
        "XXXX",
        "X..X",
        "XXXX",
    ]
    for row, line in enumerate(m):
        for col, c in enumerate(line):
            if c == "X":
                draw.rectangle([x + col * s, y + row * s, x + col * s + s - 1, y + row * s + s - 1], fill=color)
    x2 = x + 6 * s
    for row, line in enumerate(eight):
        for col, c in enumerate(line):
            if c == "X":
                draw.rectangle([x2 + col * s, y + row * s, x2 + col * s + s - 1, y + row * s + s - 1], fill=color)


def main():
    # Background: vertical gradient, dark slate.
    img = Image.new("RGBA", (S, S), (16, 18, 22, 255))
    d = ImageDraw.Draw(img)
    for y in range(S):
        t = y / S
        r = int(24 - 8 * t)
        g = int(27 - 9 * t)
        b = int(33 - 11 * t)
        d.line([(0, y), (S, y)], fill=(r, g, b, 255))

    # Soft shadow under the device.
    shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.rounded_rectangle([64, 84, 456, 452], radius=44, fill=(0, 0, 0, 160))
    shadow = shadow.filter(ImageFilter.GaussianBlur(10))
    img.alpha_composite(shadow)

    # Device body.
    rounded(d, [58, 62, 454, 444], 42, fill=BODY, outline=BODY_EDGE, width=3)
    # face highlight: subtle lighter band INSIDE the top edge (solid color —
    # low-alpha draws flatten opaque on convert("RGB"))
    rounded(d, [72, 70, 440, 76], 3, fill=(64, 70, 82, 255))

    # Screen bezel + screen.
    rounded(d, [86, 96, 426, 306], 16, fill=(12, 14, 18, 255), outline=BODY_EDGE, width=2)
    rounded(d, [96, 106, 416, 296], 10, fill=SCREEN_BG)

    # Tracker UI: header + rows.
    d.rectangle([96, 106, 416, 130], fill=(18, 21, 27, 255))
    px_letter_m8(d, 106, 112, 3, AMBER)
    # header right: three tiny dashes as fake status text
    for i in range(3):
        d.rectangle([380 - i * 14, 116, 388 - i * 14, 122], fill=ROW_GRAY)

    rows_y = [140, 158, 176, 194, 212, 230, 248, 266]
    for idx, ry in enumerate(rows_y):
        if idx == 3:
            # selected row: amber band + dark content blocks + cursor
            d.rectangle([100, ry - 6, 412, ry + 12], fill=AMBER_DIM)
            d.rectangle([104, ry - 4, 118, ry + 10], fill=AMBER)  # cursor
            for c in range(5):
                d.rectangle([128 + c * 54, ry, 128 + c * 54 + 40, ry + 6], fill=(30, 32, 38, 255))
        else:
            for c in range(5):
                w = 28 + (c * 7) % 18
                d.rectangle([128 + c * 54, ry + 2, 128 + c * 54 + w, ry + 8], fill=ROW_GRAY)
            # bar/wave column at right
            d.rectangle([404, ry, 408, ry + 10], fill=(90, 200, 190, 255) if idx % 2 else ROW_GRAY)

    # D-pad (left).
    cx, cy = 165, 372
    arm = 34
    rounded(d, [cx - arm, cy - 12, cx + arm, cy + 12], 8, fill=(201, 205, 212, 255))
    rounded(d, [cx - 12, cy - arm, cx + 12, cy + arm], 8, fill=(201, 205, 212, 255))
    d.ellipse([cx - 9, cy - 9, cx + 9, cy + 9], fill=(120, 124, 132, 255))

    # Buttons (right): A amber, B light, small shoulder dots above.
    d.ellipse([352, 336, 396, 380], fill=AMBER)
    d.ellipse([312, 366, 352, 406], fill=(232, 233, 235, 255))
    d.ellipse([398, 372, 424, 398], fill=(160, 164, 172, 255))
    d.ellipse([300, 330, 322, 352], fill=(160, 164, 172, 255))

    img.convert("RGB").save("icon0.png")
    print("wrote icon0.png")


if __name__ == "__main__":
    main()
