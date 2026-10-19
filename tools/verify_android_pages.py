#!/usr/bin/env python3
"""Check every packaged native library's ELF LOAD alignment in an Android AAR or APK."""
import argparse
import struct
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('aar', help='an .aar (jni/<abi>/*.so) or .apk (lib/<abi>/*.so)')
parser.add_argument('--abis', default='arm64-v8a,x86_64,armeabi-v7a,x86',
                    help='comma separated ABIs that must be present (default: all four)')
args = parser.parse_args()
expected_abis = set(args.abis.split(','))
seen = set()
with zipfile.ZipFile(args.aar) as aar:
    for info in aar.infolist():
        name = info.filename
        if not name.startswith(('jni/', 'lib/')) or not name.endswith('.so'):
            continue
        # Uncompressed libraries in an APK are mapped straight from the zip, so they must start on a 16 KB boundary.
        if name.startswith('lib/') and info.compress_type == zipfile.ZIP_STORED:
            aar.fp.seek(info.header_offset + 26)
            name_length, extra_length = struct.unpack('<HH', aar.fp.read(4))
            if (info.header_offset + 30 + name_length + extra_length) % 16384:
                parser.exit(1, f'{name}: not stored on a 16 KB boundary in the APK\n')
        data = aar.read(name)
        if data[:4] != b'\x7fELF' or data[4] not in (1, 2) or data[5] not in (1, 2):
            parser.exit(1, f'{name}: invalid ELF header\n')
        endian = '<' if data[5] == 1 else '>'
        is64 = data[4] == 2
        phoff = struct.unpack_from(endian + ('Q' if is64 else 'I'), data, 32 if is64 else 28)[0]
        entry_size, count = struct.unpack_from(endian + 'HH', data, 54 if is64 else 42)
        loads = 0
        for i in range(count):
            offset = phoff + entry_size * i
            header = struct.unpack_from(endian + ('IIQQQQQQ' if is64 else 'IIIIIIII'), data, offset)
            if header[0] != 1:  # PT_LOAD
                continue
            loads += 1
            file_offset, virtual_address = (header[2], header[3]) if is64 else (header[1], header[2])
            alignment = header[7]
            if alignment < 16384 or (virtual_address - file_offset) % 16384:
                parser.exit(1, f'{name}: LOAD segment is incompatible with 16 KB pages\n')
        if not loads:
            parser.exit(1, f'{name}: missing LOAD segments\n')
        seen.add(name.split('/')[1])
        print(f'{name}: {loads} LOAD segments aligned to 16 KB')
missing = expected_abis - seen
if missing:
    parser.exit(1, f'Missing native ABIs: {sorted(missing)}\n')
