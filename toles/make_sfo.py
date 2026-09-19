#!/usr/bin/env python3
# FILE: toles/make_sfo.py
import struct, sys

def make_sfo(entries):
    entries = sorted(entries.items())
    key_table = b''; data_table = b''; entry_data = b''
    for key, (fmt, val, max_len) in entries:
        key_off = len(key_table)
        key_table += key.encode('ascii') + b'\x00'
        if fmt == 0x0204: val_bin = val.encode('utf-8') + b'\x00'
        else: val_bin = struct.pack('<I', val)
        data_off = len(data_table)
        entry_data += struct.pack('<HHIII', key_off, fmt, len(val_bin), max_len, data_off)
        data_table += val_bin + b'\x00' * (max_len - len(val_bin))
    while len(key_table) % 4 != 0: key_table += b'\x00'
    header = struct.pack('<IIIII', 0x46535000, 0x00000101,
        20 + len(entry_data), 20 + len(entry_data) + len(key_table), len(entries))
    return header + entry_data + key_table + data_table

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'PARAM.SFO'
    sfo = make_sfo({
        'APP_VER': (0x0204, '01.00', 8),
        'ATTRIBUTE': (0x0404, 0, 4),
        'BOOT_FILE': (0x0204, '/USRDIR/EBOOT.BIN', 64),
        'CATEGORY': (0x0204, 'HG', 4),
        'CONTENT_ID': (0x0204, 'UP0001-NEONCITY1_00-NEONCITY00000001', 48),
        'LICENSE': (0x0204, '', 512),
        'PARENTAL_LEVEL': (0x0404, 1, 4),
        'RESOLUTION': (0x0404, 63, 4),
        'SOUND_FORMAT': (0x0404, 1, 4),
        'TITLE': (0x0204, 'Neon City', 128),
        'TITLE_ID': (0x0204, 'NEONCITY1', 16),
        'VERSION': (0x0204, '01.00', 8),
    })
    with open(out, 'wb') as f: f.write(sfo)
    print(f"PARAM.SFO created: {out} ({len(sfo)} bytes)")