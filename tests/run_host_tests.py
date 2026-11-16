"""Focused host checks. AudioOut is adapted; this does not replace PS5 hardware tests."""
from pathlib import Path
import binascii
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
SELECTED = sys.argv[1:] or ['media', 'music', 'music_wire', 'javascript', 'i18n']

def png(w=32, h=16, depth=8, interlace=0, colour=2):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', binascii.crc32(kind + data))
    head = struct.pack('>IIBBBBB', w, h, depth, colour, 0, 0, interlace)
    pixels = b'\0' * ((32 * (4 if colour == 6 else 1 if colour == 0 else 3) + 1) * 16)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', head) + chunk(b'IDAT', zlib.compress(pixels)) + chunk(b'IEND', b'')

def jpeg(marker=0xc0, channels=3):
    # Header-only fixture: verifies metadata bounds, not JPEG pixel decoding.
    sof = struct.pack('>BHHB', 8, 16, 32, channels) + bytes([1, 0x11, 0]) * channels
    return b'\xff\xd8\xff' + bytes([marker]) + struct.pack('>H', len(sof) + 2) + sof + b'\xff\xd9'

with tempfile.TemporaryDirectory(prefix='liveshell-tests-') as scratch:
    work = Path(scratch)
    fixtures = work / 'fixtures'
    fixtures.mkdir()
    cases = {'rgb.png': png(), 'gray.png': png(colour=0), 'rgba.png': png(colour=6),
             'zero.png': png(w=0), 'huge.png': png(w=4097), 'depth16.png': png(depth=16),
             'interlaced.png': png(interlace=1), 'bad.png': b'not an image',
             'rgb.jpg': jpeg(), 'progressive.jpeg': jpeg(0xc2), 'cmyk.jpg': jpeg(channels=4),
             'truncated.jpg': b'\xff\xd8\xff\xc0\0', 'bad_segment.jpg': b'\xff\xd8\xff\xe0\xff\xff', 'empty.png': b''}
    for name, data in cases.items():
        (fixtures / name).write_bytes(data)
    with (fixtures / 'oversized.png').open('wb') as f:
        f.truncate(32 * 1024 * 1024 + 1)
    music = fixtures / 'music'
    music.mkdir()
    frame = b'\xff\xfb\x90\x64' + b'\0' * 413
    for name in ['a.mp3', 'b.mp3']:
        (music / name).write_bytes(frame * 10)
    (music / 'bad.mp3').write_bytes(b'not music')
    (music / 'ignore.png').write_bytes(png())
    (music / 'ignore.mp4').write_bytes(b'video')
    compiler = os.environ.get('CXX', 'g++')
    for name in ['media_test', 'music_test', 'music_wire_test']:
        if name.removesuffix('_test') not in SELECTED:
            continue
        binary = work / (name + ('.exe' if os.name == 'nt' else ''))
        subprocess.run([compiler, '-std=c++20', '-O1', '-Wall', '-Wextra', '-pthread',
                        str(ROOT / 'tests' / (name + '.cpp')), '-o', str(binary)], check=True)
        subprocess.run([str(binary), str(fixtures)], check=True)
    for name in ['liveshell_page.html', 'liveshell_remote.html']:
        if 'javascript' not in SELECTED:
            continue
        source = (ROOT / 'src' / name).read_text(encoding='utf-8')
        for i, script in enumerate(re.findall(r'<script(?:\s[^>]*)?>(.*?)</script>', source, re.S)):
            js = work / f'{name}.{i}.js'
            js.write_text(script, encoding='utf-8')
            subprocess.run(['node', '--check', str(js)], check=True)
        print(name, 'JavaScript syntax OK', flush=True)
    if 'javascript' in SELECTED:
        subprocess.run(['node', '--check', str(ROOT / 'src/liveshell_i18n.js')], check=True)
    if 'i18n' in SELECTED:
        subprocess.run(['node', str(ROOT / 'tests/i18n_test.js')], check=True)
