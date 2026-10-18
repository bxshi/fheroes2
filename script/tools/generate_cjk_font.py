#!/usr/bin/env python3

###########################################################################
#   fheroes2: https://github.com/ihhub/fheroes2                           #
#   Copyright (C) 2026                                                    #
#                                                                         #
#   This program is free software; you can redistribute it and/or modify  #
#   it under the terms of the GNU General Public License as published by  #
#   the Free Software Foundation; either version 2 of the License, or     #
#   (at your option) any later version.                                   #
#                                                                         #
#   This program is distributed in the hope that it will be useful,       #
#   but WITHOUT ANY WARRANTY; without even the implied warranty of        #
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
#   GNU General Public License for more details.                          #
#                                                                         #
#   You should have received a copy of the GNU General Public License     #
#   along with this program; if not, write to the                         #
#   Free Software Foundation, Inc.,                                       #
#   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             #
###########################################################################

"""
Converts BDF bitmap fonts into a compact glyph data file (H2D container) used by
the engine to render CJK text. ASCII characters are never included since they are
rendered using the original game fonts.

Generate:
    generate_cjk_font.py --font small=<10px.bdf> --font normal=<12px.bdf> \\
                         [--po <file.po> ...] [--charset big5] [--license <file>] \\
                         --output <file.h2d>

Verify:
    generate_cjk_font.py --dump <file.h2d> <entry> [<characters>]

Each --font becomes an H2D entry named '<name>.font'. If --license is given, the license
text is stored as the 'LICENSE.txt' entry. Every H2D entry is zlib-compressed by the H2D
container itself, so font entries are stored uncompressed. The font entry layout
(little-endian) is:
    char[4] magic = "FH2F"
    uint16  version = 1
    uint8   ascent
    uint8   descent
    uint32  glyph count
    for each glyph, sorted by codepoint ascending:
        uint32 codepoint
        uint8  advance
        int8   offsetX
        int8   offsetTop (pixels from the baseline up to the top row of the bitmap)
        uint8  width
        uint8  height
        height rows of ceil(width / 8) bytes each, MSB is the leftmost pixel
"""

import argparse
import ast
import struct
import sys
import zlib

H2D_MAGIC = b"H2D\x03"

FONT_MAGIC = b"FH2F"
FONT_VERSION = 1

LICENSE_ENTRY_NAME = "LICENSE.txt"

FIXED_RANGES = [
    (0x2010, 0x2027),
    (0x2030, 0x205E),
    (0x3000, 0x303F),
    (0x3100, 0x312F),
    (0xFF01, 0xFF5E),
    (0xFF61, 0xFF9F),
]

BIG5_RANGES = [
    # Symbols.
    (0xA140, 0xA3BF),
    # Level-1 hanzi.
    (0xA440, 0xC67E),
]


class Glyph:  # pylint: disable=too-few-public-methods,too-many-arguments
    """A single glyph of a bitmap font."""

    def __init__(self, advance, offset_x, offset_top, width, height, bitmap):
        self.advance = advance
        self.offset_x = offset_x
        self.offset_top = offset_top
        self.width = width
        self.height = height
        self.bitmap = bitmap


class Font:  # pylint: disable=too-few-public-methods
    """A bitmap font: global metrics and glyphs by codepoint."""

    def __init__(self, ascent, descent, glyphs):
        self.ascent = ascent
        self.descent = descent
        self.glyphs = glyphs


def row_bytes(width):
    return (width + 7) // 8


def parse_bdf(
    path,
):  # pylint: disable=too-many-locals,too-many-branches,too-many-statements
    ascent = None
    descent = None
    glyphs = {}

    with open(path, "r", encoding="utf-8") as bdf_file:
        lines = bdf_file.read().splitlines()

    i = 0
    while i < len(lines):
        tokens = lines[i].split()
        i += 1

        if not tokens:
            continue

        keyword = tokens[0]
        if keyword == "FONT_ASCENT":
            ascent = int(tokens[1])
        elif keyword == "FONT_DESCENT":
            descent = int(tokens[1])
        elif keyword == "STARTCHAR":
            encoding = None
            advance = None
            bbx = None
            bitmap_rows = None

            while i < len(lines):
                tokens = lines[i].split()
                i += 1

                if not tokens:
                    continue

                if tokens[0] == "ENCODING":
                    encoding = int(tokens[1])
                elif tokens[0] == "DWIDTH":
                    advance = int(tokens[1])
                elif tokens[0] == "BBX":
                    bbx = tuple(int(value) for value in tokens[1:5])
                elif tokens[0] == "BITMAP":
                    bitmap_rows = []
                    while i < len(lines) and lines[i].strip() != "ENDCHAR":
                        bitmap_rows.append(lines[i].strip())
                        i += 1
                    i += 1
                    break
                elif tokens[0] == "ENDCHAR":
                    break

            if encoding is None or encoding < 0 or advance is None or bbx is None:
                continue

            width, height, offset_x, offset_y = bbx
            if width == 0 or height == 0:
                width = 0
                height = 0
                bitmap_rows = []

            if bitmap_rows is None or len(bitmap_rows) != height:
                raise ValueError(
                    f"{path}: glyph U+{encoding:04X} has {0 if bitmap_rows is None else len(bitmap_rows)} "
                    f"bitmap rows instead of {height}"
                )

            stride = row_bytes(width)
            bitmap = bytearray()
            for row in bitmap_rows:
                data = bytes.fromhex(row)
                data = (data + bytes(stride))[:stride]
                # Clear the padding bits beyond the glyph width.
                value = int.from_bytes(data, "big") if data else 0
                padding_bits = stride * 8 - width
                value = (value >> padding_bits) << padding_bits
                bitmap += value.to_bytes(stride, "big")

            glyphs[encoding] = Glyph(
                advance, offset_x, offset_y + height, width, height, bytes(bitmap)
            )

    if ascent is None or descent is None:
        raise ValueError(f"{path}: FONT_ASCENT or FONT_DESCENT property is missing")

    return Font(ascent, descent, glyphs)


def unquote_po_string(text):
    # PO strings use C-like escape sequences which are compatible with Python string literals.
    return ast.literal_eval(text)


def parse_po_translations(path):
    """Returns all msgstr values of non-obsolete entries, skipping the PO header entry."""
    entries = []
    msgid = None
    key = None
    value = None

    def finish_field():
        nonlocal msgid
        if key == "msgid":
            msgid = value
        elif key is not None and key.startswith("msgstr") and msgid != "":
            entries.append(value)

    with open(path, "r", encoding="utf-8") as po_file:
        for raw_line in po_file:
            line = raw_line.strip()

            if not line or line.startswith("#"):
                continue

            if line.startswith('"'):
                if key is not None:
                    value += unquote_po_string(line)
                continue

            finish_field()

            key, _, rest = line.partition(" ")
            value = unquote_po_string(rest.strip())

        finish_field()

    return entries


def collect_po_characters(paths):
    characters = set()
    for path in paths:
        for text in parse_po_translations(path):
            characters.update(ord(ch) for ch in text if ord(ch) >= 0x80)
    return characters


def collect_big5_characters():
    characters = set()
    for begin, end in BIG5_RANGES:
        for code in range(begin, end + 1):
            trail = code & 0xFF
            if not (0x40 <= trail <= 0x7E or 0xA1 <= trail <= 0xFE):
                continue
            try:
                text = bytes([code >> 8, trail]).decode("big5")
            except UnicodeDecodeError:
                continue
            characters.update(ord(ch) for ch in text if ord(ch) >= 0x80)
    return characters


def collect_fixed_characters():
    characters = set()
    for begin, end in FIXED_RANGES:
        characters.update(range(begin, end + 1))
    return characters


def check_range(value, low, high, what, codepoint):
    if not low <= value <= high:
        raise ValueError(
            f"Glyph U+{codepoint:04X}: {what} value {value} is out of range [{low}, {high}]"
        )


def build_font_entry(font, codepoints):
    table = bytearray()
    check_range(font.ascent, 0, 255, "ascent", 0)
    check_range(font.descent, 0, 255, "descent", 0)
    table += struct.pack("<BBI", font.ascent, font.descent, len(codepoints))

    for codepoint in sorted(codepoints):
        glyph = font.glyphs[codepoint]
        check_range(glyph.advance, 0, 255, "advance", codepoint)
        check_range(glyph.offset_x, -128, 127, "offsetX", codepoint)
        check_range(glyph.offset_top, -128, 127, "offsetTop", codepoint)
        check_range(glyph.width, 0, 255, "width", codepoint)
        check_range(glyph.height, 0, 255, "height", codepoint)

        table += struct.pack(
            "<IBbbBB",
            codepoint,
            glyph.advance,
            glyph.offset_x,
            glyph.offset_top,
            glyph.width,
            glyph.height,
        )
        table += glyph.bitmap

    return FONT_MAGIC + struct.pack("<H", FONT_VERSION) + bytes(table)


def pack_string(text):
    data = text.encode("utf-8")
    return struct.pack("<I", len(data)) + data


def write_h2d(path, entries):
    """Writes an H2D container. Same layout as fheroes2::H2DWriter::write()."""
    # The engine stores entries in a std::map so they are sorted by name.
    names = sorted(entries)
    # Each entry is compressed as a whole, like H2DWriter::add() does.
    compressed = {name: zlib.compress(entries[name], 9) for name in names}

    info_size = sum(4 + 4 + 4 + len(name.encode("utf-8")) + 4 for name in names)
    offset = 4 + 4 + info_size

    output = bytearray(H2D_MAGIC)
    output += struct.pack("<I", len(names))
    for name in names:
        output += struct.pack("<II", offset, len(compressed[name]))
        output += pack_string(name)
        # No extra info.
        output += pack_string("")
        offset += len(compressed[name])

    for name in names:
        output += compressed[name]

    with open(path, "wb") as h2d_file:
        h2d_file.write(output)

    return len(output)


def read_h2d(path):
    with open(path, "rb") as h2d_file:
        data = h2d_file.read()

    if data[:4] != H2D_MAGIC:
        raise ValueError(f"{path}: not an H2D file")

    (count,) = struct.unpack_from("<I", data, 4)
    pos = 8
    entries = {}
    for _ in range(count):
        offset, size = struct.unpack_from("<II", data, pos)
        pos += 8
        strings = []
        for _ in range(2):
            (length,) = struct.unpack_from("<I", data, pos)
            pos += 4
            strings.append(data[pos : pos + length].decode("utf-8"))
            pos += length
        entries[strings[0]] = zlib.decompress(data[offset : offset + size])

    return entries


def decode_font_entry(data):
    if data[:4] != FONT_MAGIC:
        raise ValueError("Invalid font entry magic")

    (version,) = struct.unpack_from("<H", data, 4)
    if version != FONT_VERSION:
        raise ValueError(f"Unsupported font entry version {version}")

    table = data[6:]
    ascent, descent, count = struct.unpack_from("<BBI", table, 0)
    pos = 6
    glyphs = {}
    for _ in range(count):
        codepoint, advance, offset_x, offset_top, width, height = struct.unpack_from(
            "<IBbbBB", table, pos
        )
        pos += 9
        bitmap_size = row_bytes(width) * height
        glyphs[codepoint] = Glyph(
            advance, offset_x, offset_top, width, height, table[pos : pos + bitmap_size]
        )
        pos += bitmap_size

    if pos != len(table):
        raise ValueError("Trailing data in the glyph table")

    return Font(ascent, descent, glyphs)


def dump(path, entry_name, characters=""):
    entries = read_h2d(path)
    if entry_name not in entries:
        raise ValueError(
            f"Entry '{entry_name}' not found. Available entries: {', '.join(sorted(entries))}"
        )

    data = entries[entry_name]
    if data[:4] != FONT_MAGIC:
        # Not a font entry, for example the license text.
        print(data.decode("utf-8"))
        return

    font = decode_font_entry(entries[entry_name])
    print(
        f"{entry_name}: ascent {font.ascent}, descent {font.descent}, {len(font.glyphs)} glyphs"
    )

    for ch in characters:
        codepoint = ord(ch)
        glyph = font.glyphs.get(codepoint)
        if glyph is None:
            print(f"\nU+{codepoint:04X} '{ch}': not present")
            continue

        print(
            f"\nU+{codepoint:04X} '{ch}': advance {glyph.advance}, offsetX {glyph.offset_x}, "
            f"offsetTop {glyph.offset_top}, size {glyph.width}x{glyph.height}"
        )

        # Render the whole line box (ascent + descent) using the advance as the cell width.
        cell_width = max(glyph.advance, glyph.offset_x + glyph.width)
        stride = row_bytes(glyph.width)
        for line_y in range(font.ascent, -font.descent, -1):
            # line_y is the height of the top edge of this pixel row above the baseline.
            row = line_y - 1
            bitmap_row = glyph.offset_top - 1 - row
            pixels = []
            for x in range(cell_width):
                bitmap_x = x - glyph.offset_x
                filled = False
                if 0 <= bitmap_row < glyph.height and 0 <= bitmap_x < glyph.width:
                    byte = glyph.bitmap[bitmap_row * stride + bitmap_x // 8]
                    filled = bool(byte & (0x80 >> (bitmap_x % 8)))
                pixels.append("#" if filled else ".")
            if row == -1:
                print("    " + "-" * cell_width + " baseline")
            print("    " + "".join(pixels))


def format_characters(codepoints):
    return " ".join(f"U+{codepoint:04X}({chr(codepoint)})" for codepoint in codepoints)


def generate(args):  # pylint: disable=too-many-locals
    fonts = []
    for spec in args.font:
        name, sep, path = spec.partition("=")
        if not sep or not name or not path:
            raise ValueError(
                f"Invalid --font value '{spec}', expected <name>=<bdf file>"
            )
        fonts.append((name, path))

    requested = set()
    po_characters = collect_po_characters(args.po)
    requested |= po_characters
    print(f"Characters from PO files: {len(po_characters)}")

    if args.charset == "big5":
        big5_characters = collect_big5_characters()
        requested |= big5_characters
        print(f"Characters from Big5 charset: {len(big5_characters)}")

    fixed_characters = collect_fixed_characters()
    requested = {codepoint for codepoint in requested if codepoint >= 0x80}

    entries = {}
    for name, path in fonts:
        font = parse_bdf(path)
        missing = sorted(
            codepoint for codepoint in requested if codepoint not in font.glyphs
        )
        codepoints = {codepoint for codepoint in requested if codepoint in font.glyphs}
        codepoints |= {
            codepoint for codepoint in fixed_characters if codepoint in font.glyphs
        }

        entry_name = f"{name}.font"
        entries[entry_name] = build_font_entry(font, codepoints)

        print(
            f"{entry_name}: {len(codepoints)} glyphs (ascent {font.ascent}, descent {font.descent}), "
            f"entry size {len(entries[entry_name])} bytes, source '{path}'"
        )
        missing_po = [codepoint for codepoint in missing if codepoint in po_characters]
        if missing_po:
            print(
                f"  WARNING: {len(missing_po)} characters used by translations are missing: "
                f"{format_characters(missing_po)}"
            )
        if missing:
            print(
                f"  {len(missing)} requested characters missing (skipped): {format_characters(missing)}"
            )
        else:
            print("  No requested characters are missing")

    if args.license:
        with open(args.license, "rb") as license_file:
            license_text = license_file.read()
        # Make sure that the text is valid UTF-8.
        license_text.decode("utf-8")
        entries[LICENSE_ENTRY_NAME] = license_text
        print(
            f"{LICENSE_ENTRY_NAME}: {len(license_text)} bytes, source '{args.license}'"
        )

    size = write_h2d(args.output, entries)
    print(f"Written '{args.output}': {size} bytes")


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Convert BDF bitmap fonts into an fheroes2 CJK glyph data file (H2D)."
    )
    parser.add_argument(
        "--font",
        action="append",
        default=[],
        metavar="NAME=BDF",
        help="font to convert, stored as the '<NAME>.font' entry (can be repeated)",
    )
    parser.add_argument(
        "--po",
        action="append",
        default=[],
        metavar="PO",
        help="PO file whose translated strings define the required characters (can be repeated)",
    )
    parser.add_argument(
        "--charset",
        choices=["big5"],
        help="additional character set to include: 'big5' adds Big5 symbols and level-1 hanzi",
    )
    parser.add_argument(
        "--license",
        metavar="FILE",
        help=f"UTF-8 text file stored as the '{LICENSE_ENTRY_NAME}' entry",
    )
    parser.add_argument("--output", metavar="H2D", help="output H2D file")
    parser.add_argument(
        "--dump",
        nargs="+",
        metavar="H2D ENTRY [CHARS]",
        help="decode the given entry of an H2D file and print the given characters as ASCII art "
        "(non-font entries are printed as text)",
    )

    args = parser.parse_args()

    if args.dump is not None and len(args.dump) not in (2, 3):
        parser.error(
            "--dump expects an H2D file, an entry name and optionally characters"
        )

    if args.dump is None and (not args.font or not args.output):
        parser.error("--font and --output are required unless --dump is used")

    return args


def main():
    args = parse_arguments()

    try:
        if args.dump is not None:
            dump(*args.dump)
        else:
            generate(args)
    except (OSError, ValueError, UnicodeDecodeError, zlib.error, struct.error) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
