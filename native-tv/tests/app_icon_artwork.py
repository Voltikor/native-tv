#!/usr/bin/env python3
"""Offline integrity and streaming-service coverage checks for bundled artwork."""
import hashlib
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
RECORD = ROOT / "design/app-icons/official.json"


def png_size(data):
    assert data[:16] == b"\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR", "Invalid artwork PNG"
    return struct.unpack(">II", data[16:24])


def check():
    record = json.loads(RECORD.read_text(encoding="utf-8"))
    data = (ROOT / "assets/app-icons/app-icons.pack").read_bytes()
    header, payload = data.split(b"\n\n", 1)
    assert header.startswith(b"NTVICONS 1\n")
    assert len(header) + 2 <= 16 * 1024, "Header exceeds the daemon's read limit"
    packed = {}
    for line in header.splitlines()[1:]:
        key, offset, length = line.decode("ascii").split()
        offset, length = int(offset), int(length)
        assert key not in packed and offset >= 0 and length > 0
        assert offset + length <= len(payload), "Artwork extends past the pack"
        packed[key] = payload[offset:offset + length]
        assert png_size(packed[key]) == (192, 192), "Tile resolution changed"

    official = set()
    for name, entry in record["apps"].items():
        source = (RECORD.parent / entry["file"]).read_bytes()
        assert hashlib.sha256(source).hexdigest() == entry["sha256"], name + ": source changed"
        width, height = png_size(source)
        assert width == height and 192 <= width <= 2048
        assert entry["listing"].startswith("https://play.google.com/store/apps/details?")
        assert entry["url"].startswith("https://play-lh.googleusercontent.com/")
        assert entry["publisher"], name + ": publisher was not recorded"
        assert entry["keys"], name + ": no launcher aliases"
        image = packed[entry["keys"][0]]
        for key in entry["keys"]:
            assert key not in official, "Duplicate official artwork alias: " + key
            assert packed[key] == image, name + ": aliases do not share artwork"
            official.add(key)

    fallback = set(record["fallbacks"])
    recognized = set(re.findall(r'\{"([^\"]+)", "(?:video|music)"\}',
                               (ROOT / "src/service/app_kinds.cpp").read_text(encoding="utf-8")))
    assert not official & fallback, "An app is marked both official and fallback"
    assert not recognized - official - fallback, "Streaming apps missing from the artwork audit: " + str(recognized - official - fallback)
    assert all(record["fallbacks"].values()), "A fallback needs an explanation"
    original = json.loads((ROOT / "design/app-icons.json").read_text(encoding="utf-8"))
    assert set(original["logos"]) | set(original["glyphs"]) <= set(packed), "Original tile coverage was lost"
    print(f"{len(record['apps'])} official services, {len(official)} official aliases; "
          f"all {len(recognized)} recognized streaming aliases accounted for; "
          f"{len(fallback)} documented legacy/webOS fallbacks")


if __name__ == "__main__":
    check()
