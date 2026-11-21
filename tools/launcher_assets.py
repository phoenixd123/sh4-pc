#!/usr/bin/env python3
"""Draws the launcher's two images from the font atlas. No PIL, no network: a BMP writer and the port's own
8-bit font.bmp (ASCII 32-127, 16 columns of 6 cells, 40x80 each, advance 36, baseline 60).

    python tools/launcher_assets.py            # assets/splash.bmp and assets/logo.bmp
"""
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ATLAS = ROOT / "assets" / "font.bmp"


def read_bmp8(path):
    d = path.read_bytes()
    assert d[:2] == b"BM", "not a BMP"
    off = struct.unpack_from("<I", d, 10)[0]
    hdr = struct.unpack_from("<I", d, 14)[0]
    w, h = struct.unpack_from("<ii", d, 18)
    bpp = struct.unpack_from("<H", d, 28)[0]
    comp = struct.unpack_from("<I", d, 30)[0]
    assert hdr == 40 and bpp == 8 and comp == 0, "font.bmp must be an uncompressed 8-bit BMP"
    palette = [tuple(d[54 + 4 * i + 2:54 + 4 * i + 5][::-1]) for i in range(256)]
    stride = (w + 3) & ~3
    rows = []
    for y in range(h):
        row = d[off + (h - 1 - y) * stride: off + (h - 1 - y) * stride + w]
        rows.append([palette[b][0] for b in row])
    return w, h, rows


class Canvas:
    def __init__(self, w, h, bg):
        self.w, self.h = w, h
        self.px = [list(bg) for _ in range(w * h)]

    def blend(self, x, y, color, cov):
        if cov <= 0 or x < 0 or y < 0 or x >= self.w or y >= self.h:
            return
        a = cov / 255.0
        d = self.px[y * self.w + x]
        for i in range(3):
            d[i] = int(d[i] + (color[i] - d[i]) * a + 0.5)

    def rect(self, x, y, w, h, color, cov=255):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.blend(xx, yy, color, cov)

    def text(self, x, y, size, color, s, track=0):
        """y is the baseline, as in the launcher."""
        atlas_w, atlas_h, glyphs = ATLAS_CACHE
        k = size / 80.0
        adv = int(36 * k + 0.5)
        top = y - int(60 * k)
        for ch in s:
            c = ord(ch) - 32
            if 0 < c < 96:
                gx, gy = c % 16 * 40, c // 16 * 80
                for py in range(size):
                    sy = gy + int(py / k)
                    if sy >= gy + 80:
                        break
                    for pxx in range(int(40 * k)):
                        sx = gx + int(pxx / k)
                        if sx >= gx + 40:
                            break
                        cov = glyphs[sy][sx]
                        if cov:
                            self.blend(x + pxx, top + py, color, cov)
            x += adv + track

    def text_w(self, size, s, track=0):
        k = size / 80.0
        adv = int(36 * k + 0.5)
        return len(s) * (adv + track) - track

    def centered(self, y, size, color, s, track=0):
        self.text((self.w - self.text_w(size, s, track)) // 2, y, size, color, s, track)

    def save(self, path):
        stride = (self.w * 3 + 3) & ~3
        body = bytearray()
        for y in range(self.h - 1, -1, -1):
            row = bytearray()
            for x in range(self.w):
                r, g, b = self.px[y * self.w + x]
                row += bytes((b, g, r))
            row += b"\0" * (stride - len(row))
            body += row
        hdr = b"BM" + struct.pack("<IHHI", 14 + 40 + len(body), 0, 0, 54)
        info = struct.pack("<IiiHHIIiiII", 40, self.w, self.h, 1, 24, 0, len(body), 2835, 2835, 0, 0)
        path.write_bytes(hdr + info + bytes(body))
        print(f"{path.relative_to(ROOT)}  {self.w}x{self.h}")


ATLAS_CACHE = read_bmp8(ATLAS)
BG = (8, 8, 10)
TEXT = (200, 200, 200)
DIM = (110, 110, 110)
RUST = (60, 42, 34)


def make_splash():
    """The screen every launch starts with: who this is, and that the game is the player's own."""
    c = Canvas(1920, 1080, (2, 2, 3))
    for y in range(1080):                       # a slow vertical wash, dark at the corners
        v = int(18 * (1 - abs(y - 540) / 540.0) ** 2)
        c.rect(0, y, 1920, 1, (v, v // 2, v // 2), 90)
    c.centered(360, 150, TEXT, "SILENT HILL 4", 22)
    c.rect(660, 420, 600, 2, (90, 90, 94))
    c.centered(500, 54, DIM, "T H E   R O O M")
    c.centered(700, 26, DIM, "Unofficial fan port, made from your own copy")
    c.centered(770, 21, DIM, "You need Silent Hill 4: The Room from GOG, version 1.0 gog v5.")
    c.centered(806, 21, DIM, "No game files are included with this port.")
    c.centered(890, 19, (80, 80, 84), "Silent Hill 4 is Konami's. This project is not affiliated with Konami.")
    c.save(ROOT / "assets" / "splash.bmp")


def make_logo():
    """The menu's wordmark, on the menu's own background so it needs no alpha."""
    c = Canvas(1000, 300, BG)
    c.centered(150, 92, TEXT, "SILENT HILL 4", 12)
    c.rect(330, 196, 340, 1, (70, 70, 74))
    c.centered(250, 34, DIM, "T H E   R O O M")
    c.save(ROOT / "assets" / "logo.bmp")


if __name__ == "__main__":
    if not ATLAS.exists():
        sys.exit(f"missing {ATLAS}")
    make_splash()
    make_logo()
