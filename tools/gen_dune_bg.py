#!/usr/bin/env python3
"""
Generate a small, math-friendly dune background from an input image.

Outputs:
  - src/dune_bg.h: contains:
      - uint8_t  g_dune_hmap[]  (heightmap)
      - uint16_t g_dune_tex[]   (RGB565 shaded texture derived from heightmap)

Design goals:
  - Texture is low-res (LCD/2) so runtime background fill is just x>>1,y>>1 sampling.
  - Heightmap is kept alongside the shaded texture so it can evolve later (sand flow).
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path

from PIL import Image, ImageFilter, ImageOps


def rgb565(r: int, g: int, b: int) -> int:
    r = max(0, min(255, int(r)))
    g = max(0, min(255, int(g)))
    b = max(0, min(255, int(b)))
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def shade_from_height(h: list[int], w: int, hgt: int) -> list[int]:
    # Cheap bump shading from height gradient.
    # Light from top-left.
    Lx, Ly, Lz = -24, -40, 96

    out: list[int] = [0] * (w * hgt)
    for y in range(hgt):
        y0 = max(0, y - 1)
        y1 = min(hgt - 1, y + 1)
        for x in range(w):
            x0 = max(0, x - 1)
            x1 = min(w - 1, x + 1)
            c = h[y * w + x]
            hx = h[y * w + x1] - h[y * w + x0]
            hy = h[y1 * w + x] - h[y0 * w + x]

            # Normal is (-hx, -hy, const)
            nx = -hx
            ny = -hy
            nz = 96

            dot = nx * Lx + ny * Ly + nz * Lz
            if dot < 0:
                dot = 0

            # Normalize-ish to 0..255
            I = dot // 64
            if I > 255:
                I = 255

            # Sand palette with height tinting
            # Base sand: warm. Darken slightly with "I", then add height-based warm boost.
            base_r = 210 + (c - 128) // 10
            base_g = 190 + (c - 128) // 14
            base_b = 150 + (c - 128) // 18

            r = (base_r * (60 + I)) // 255
            g = (base_g * (60 + I)) // 255
            b = (base_b * (60 + I)) // 255

            # Subtle horizon haze near top
            haze = max(0, 80 - y)
            r = r + (haze * 2) // 3
            g = g + (haze * 2) // 3
            b = b + haze

            out[y * w + x] = rgb565(r, g, b)
    return out


def emit_c_array_u8(name: str, data: list[int], cols: int = 16) -> str:
    lines = [f"static const uint8_t {name}[{len(data)}] = {{"]
    for i in range(0, len(data), cols):
        chunk = data[i : i + cols]
        lines.append("    " + ", ".join(str(v) for v in chunk) + ",")
    lines.append("};")
    return "\n".join(lines)


def emit_c_array_u16(name: str, data: list[int], cols: int = 12) -> str:
    lines = [f"static const uint16_t {name}[{len(data)}] = {{"]
    for i in range(0, len(data), cols):
        chunk = data[i : i + cols]
        lines.append("    " + ", ".join(f"0x{v:04X}u" for v in chunk) + ",")
    lines.append("};")
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="inp", required=True, help="Input image path (jpg/png)")
    ap.add_argument("--out", dest="outp", required=True, help="Output header path")
    ap.add_argument("--w", type=int, default=240, help="Texture width (default 240)")
    ap.add_argument("--h", type=int, default=160, help="Texture height (default 160)")
    args = ap.parse_args()

    inp = Path(args.inp)
    outp = Path(args.outp)
    outp.parent.mkdir(parents=True, exist_ok=True)

    img = Image.open(inp).convert("L")
    img = ImageOps.autocontrast(img)
    img = img.filter(ImageFilter.GaussianBlur(radius=1.2))
    img = img.resize((args.w, args.h), Image.Resampling.LANCZOS)
    img = ImageOps.autocontrast(img)

    hmap = list(img.getdata())
    tex = shade_from_height(hmap, args.w, args.h)

    rel_in = os.path.relpath(str(inp), start=str(outp.parent))

    header = []
    header.append("#pragma once")
    header.append("")
    header.append("#include <stdint.h>")
    header.append("")
    header.append("/* Auto-generated. Do not edit by hand.")
    header.append(f" * Source image: {rel_in}")
    header.append(" * Regenerate: python3 tools/gen_dune_bg.py --in downloads/sanddune.jpg --out src/dune_bg.h")
    header.append(" */")
    header.append("")
    header.append(f"#define DUNE_TEX_W {args.w}u")
    header.append(f"#define DUNE_TEX_H {args.h}u")
    header.append("")
    header.append(emit_c_array_u8("g_dune_hmap", hmap))
    header.append("")
    header.append(emit_c_array_u16("g_dune_tex", tex))
    header.append("")

    outp.write_text("\n".join(header), encoding="utf-8")
    print(f"Wrote: {outp}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
