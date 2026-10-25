#!/usr/bin/env python3
"""Pure-Python, static sanity checks of a Windows x64 dinput8.dll build.

Does NOT execute the DLL, inspect safety or guarantee in-game compatibility.
"""
from pathlib import Path
import hashlib
import struct
import sys

if len(sys.argv) != 2:
    print("Usage: python scripts/check_build.py dist/dinput8.dll")
    raise SystemExit(2)
path = Path(sys.argv[1])
b = path.read_bytes()
assert b[0:2] == b"MZ", "Not a PE file"
p = struct.unpack_from("<I", b, 0x3C)[0]
assert b[p:p+4] == b"PE\0\0", "Missing PE signature"
assert struct.unpack_from("<H", b, p+4)[0] == 0x8664, "Not Windows x64"
assert struct.unpack_from("<H", b, p+22)[0] & 0x2000, "Not DLL"
assert struct.unpack_from("<H", b, p+24)[0] == 0x20B, "Not PE32+"
flags = struct.unpack_from("<H", b, p+24+0x46)[0]
assert flags & 0x40 and flags & 0x100 and flags & 0x20, "Missing ASLR/NX/HIGH_ENTROPY_VA"
for name in ["DirectInput8Create", "DllCanUnloadNow", "DllGetClassObject", "GetdfDIJoystick", "OBS_GetVersion"]:
    assert name.encode("ascii") in b, f"Missing expected export: {name}"
for name in ["VS_VERSION_INFO", "Spawn The World", "OriginalFilename", "dinput8.dll"]:
    assert name.encode("utf-16le") in b, f"Missing version resource field: {name}"
assert b"\xcd\x29" in b, "No expected __fastfail instruction (GS cookie runtime)"
source = Path(__file__).resolve().parent.parent / "src/SpawnTheWorld.cpp"
code = source.read_text(encoding="utf-8")
assert "0x80,0x3D,0xF9,0x0F,0x00,0x00,0x00,0x74,0x0E" in code, "Expected corrected gateway sequence missing"
assert "hookEnabled=(volatile u8*)(gw+4096)" in code, "Expected separate RW switch missing"
print(f"PASS Windows x64 DLL, DInput8 exports, VERSIONINFO, /GS runtime, gateway source")
print(f"SHA-256 {hashlib.sha256(b).hexdigest()}  {path}")
