#!/usr/bin/env python3
"""Create a standard VERSIONINFO Win32 .res without Windows SDK dependency.
Transparent metadata, not an AV circumvention mechanism.
"""
from pathlib import Path
import struct
import sys

VERSION = (1, 0, 0, 0)
LANG = 0x0409
CODEPAGE = 1200
STRINGS = {
    'CompanyName': 'faytleingod84',
    'FileDescription': 'Spawn The World - Enshrouded Mod',
    'FileVersion': '1.0.0.0',
    'InternalName': 'Spawn The World',
    'LegalCopyright': 'Community mod - no affiliation with Keen Games',
    'OriginalFilename': 'dinput8.dll',
    'ProductName': 'Spawn The World',
    'ProductVersion': '1.0.0.0',
}

def w(s): return s.encode('utf-16le') + b'\x00\x00'
def align(b): return b + bytes((-len(b)) % 4)

def block(key, value=b'', children=(), val_type=1, value_len=None):
    # WORD Length, WORD ValueLength, WORD Type, WCHAR Key[] NUL, DWORD padding,
    # Value[] (DWORD aligned), children (each DWORD aligned).
    head = struct.pack('<HHH', 0, value_len if value_len is not None else
        (len(value) // 2 if val_type == 1 else len(value)), val_type) + w(key)
    buf = align(head) + align(value) + b''.join(align(x) for x in children)
    assert len(buf) < 65536
    return struct.pack('<H', len(buf)) + buf[2:]

strings = [block(k, w(v)) for k, v in STRINGS.items()]
stringtable = block(f'{LANG:04X}{CODEPAGE:04X}', children=strings)
stringfileinfo = block('StringFileInfo', children=[stringtable])
translation = block('Translation', struct.pack('<HH', LANG, CODEPAGE), val_type=0)
varfileinfo = block('VarFileInfo', children=[translation])
fixed = struct.pack('<13I',
    0xFEEF04BD, 0x00010000,
    (VERSION[0] << 16) | VERSION[1], (VERSION[2] << 16) | VERSION[3],
    (VERSION[0] << 16) | VERSION[1], (VERSION[2] << 16) | VERSION[3],
    0x3F, 0, 0x40004, 0x2, 0, 0, 0)
root = block('VS_VERSION_INFO', fixed, children=[stringfileinfo, varfileinfo],val_type=0)
# standard 32-byte (required) null resource and RT_VERSION = 16, ID 1.
def record(data, ty, name, lang):
    header = (struct.pack('<II', len(data), 32) +
              struct.pack('<HHHH', 0xFFFF, ty, 0xFFFF, name) +
              struct.pack('<IHHII', 0, 0x0030 if ty else 0, lang, 0, 0))
    assert len(header) == 32
    return header + align(data)
res = record(b'',0,0,0) + record(root, 16, 1, LANG)
out = Path(sys.argv[1] if len(sys.argv)>1 else 'version.res')
out.write_bytes(res)
print(f'Wrote {out}: {len(res)} bytes, ProductName={STRINGS["ProductName"]}')
