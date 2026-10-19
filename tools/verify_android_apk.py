#!/usr/bin/env python3
"""Fail-closed-Pruefung des SkyEmu-Android-APK (Inhalt, Lizenzen, 16-KB-Alignment, Debug-Signatur, kein testOnly, nicht debuggable).

Nutzung: python3 tools/verify_android_apk.py <apk> [--build-tools <dir>]
aapt2, zipalign und apksigner kommen aus --build-tools. Ohne --build-tools wird $ANDROID_HOME bzw. $ANDROID_SDK_ROOT/build-tools/36.0.0 verwendet.
Exit-Code 0 nur, wenn alle Pruefungen bestehen.
"""
import argparse
import os
import posixpath
import re
import struct
import subprocess
import sys
import zipfile

FORBIDDEN_NAMES = {'skyemu-open-signing-store'}
FORBIDDEN_SUFFIXES = ('.jks', '.keystore', '.p12', '.pfx')
ALLOWED_BIN_BASENAMES = {'DebugProbesKt.bin'}
ALLOWED_BIN_PREFIXES = ('META-INF/kotlin',)
REQUIRED_ENTRIES = (
    'lib/arm64-v8a/libSkyEmu.so',
    'assets/licenses/THIRD-PARTY-NOTICES.md',
    'assets/licenses/SkyEmu-LICENSE.txt',
    'assets/licenses/ACKNOWLEDGMENTS.md',
    'assets/licenses/Cult-of-GBA-BIOS-LICENSE.txt',
    'assets/licenses/Drastic-NDS-BIOS-LICENSE.txt',
    'assets/licenses/OpenSSL-LICENSE.txt',
    'assets/licenses/curl-COPYING.txt',
    'assets/licenses/Capstone-LICENSE.txt',
    'assets/licenses/Capstone-LICENSE_LLVM.txt',
    'assets/licenses/cimgui-LICENSE.txt',
    'assets/licenses/DearImGui-LICENSE.txt',
    'assets/licenses/rcheevos-LICENSE.txt',
    'assets/licenses/utf8proc-LICENSE.md',
    'assets/licenses/Lua-LICENSE.txt',
    'assets/licenses/sokol-LICENSE.txt',
    'assets/licenses/miniz-LICENSE.txt',
    'assets/licenses/stb-LICENSE.txt',
    'assets/licenses/xxHash-LICENSE.txt',
    'assets/licenses/tinydir-LICENSE.txt',
)
# Inhaltsmarker, damit leere oder falsche Lizenzdateien auffallen.
REQUIRED_TEXT = {
    'assets/licenses/Drastic-NDS-BIOS-LICENSE.txt': 'Copyright (c) 2013, Gilead Kutnick',
    'assets/licenses/Cult-of-GBA-BIOS-LICENSE.txt': 'Permission is hereby granted',
    'assets/licenses/OpenSSL-LICENSE.txt': 'OpenSSL License',
    'assets/licenses/curl-COPYING.txt': 'Daniel Stenberg',
}
DEBUG_SIGNER = 'CN=Android Debug'
# android:testOnly hat die Ressourcen-ID 0x01010272. Ein APK mit testOnly=true lehnt der
# Paketinstaller auf Geraeten ab ("Paket ungueltig"); nur ein ausdrueckliches false/0 ist erlaubt.
TESTONLY_RE = re.compile(r':testOnly\(0x01010272\)=(\S+)')
TESTONLY_FALSE = {'false', '0', '0x0', '0x00000000'}
DEBUGGABLE_RE = re.compile(r':debuggable\(0x0101000f\)=(\S+)')
# NativeActivity laedt die Bibliothek ueber diese meta-data; fehlt sie, sucht Android libmain.so.
LIB_NAME_RE = re.compile(r':name\(0x01010003\)="android\.app\.lib_name"[^\n]*\n\s*A: \S+:value\(0x01010024\)="SkyEmu"')


def check_entries(apk):
    """Prueft die Zip-Eintraege des APK und gibt eine Liste von Fehlern zurueck."""
    errors = []
    with zipfile.ZipFile(apk) as zf:
        infos = zf.infolist()
        names = {i.filename for i in infos}
        for name in sorted(names):
            base = posixpath.basename(name)
            lower = base.lower()
            if base in FORBIDDEN_NAMES:
                errors.append(f'verbotener Eintrag (Keystore): {name}')
            elif lower.endswith(FORBIDDEN_SUFFIXES):
                errors.append(f'verbotener Eintrag (Keystore-Endung): {name}')
            elif lower.endswith('.bin') and base not in ALLOWED_BIN_BASENAMES \
                    and not name.startswith(ALLOWED_BIN_PREFIXES):
                errors.append(f'unerwartete .bin-Datei: {name}')
        for required in REQUIRED_ENTRIES:
            if required not in names:
                errors.append(f'Pflichteintrag fehlt: {required}')
            elif zf.getinfo(required).file_size == 0:
                errors.append(f'Pflichteintrag ist leer: {required}')
        for entry, marker in REQUIRED_TEXT.items():
            if entry in names and marker not in zf.read(entry).decode('utf-8', 'replace'):
                errors.append(f'{entry}: erwarteter Text fehlt: {marker!r}')
    return errors


def check_elf_alignment(name, data, page=16384):
    """Prueft, ob alle PT_LOAD-Segmente einer ELF-Datei mit 16-KB-Seiten kompatibel sind."""
    if data[:4] != b'\x7fELF' or data[4] not in (1, 2) or data[5] not in (1, 2):
        return [f'{name}: ungueltiger ELF-Header']
    endian = '<' if data[5] == 1 else '>'
    is64 = data[4] == 2
    phoff = struct.unpack_from(endian + ('Q' if is64 else 'I'), data, 32 if is64 else 28)[0]
    entry_size, count = struct.unpack_from(endian + 'HH', data, 54 if is64 else 42)
    loads = 0
    for i in range(count):
        hdr = struct.unpack_from(endian + ('IIQQQQQQ' if is64 else 'IIIIIIII'), data, phoff + entry_size * i)
        if hdr[0] != 1:  # PT_LOAD
            continue
        loads += 1
        offset, vaddr = (hdr[2], hdr[3]) if is64 else (hdr[1], hdr[2])
        if hdr[7] < page or (vaddr - offset) % page:
            return [f'{name}: LOAD-Segment nicht 16-KB-kompatibel']
    return [] if loads else [f'{name}: keine LOAD-Segmente']


def check_native_libs(apk):
    """Prueft alle lib/*/*.so im APK auf 16-KB-LOAD-Alignment."""
    errors = []
    with zipfile.ZipFile(apk) as zf:
        for name in zf.namelist():
            if name.startswith('lib/') and name.endswith('.so'):
                errors.extend(check_elf_alignment(name, zf.read(name)))
                if not errors:
                    print(f'{name}: LOAD-Segmente 16-KB-aligned')
    return errors


def check_signer_output(output):
    """Prueft die Ausgabe von `apksigner verify --print-certs`."""
    signers = [l for l in output.splitlines() if 'certificate DN:' in l]
    if not signers:
        return ['apksigner: kein Signer in der Ausgabe']
    bad = [l for l in signers if DEBUG_SIGNER not in l]
    if bad:
        return [f'apksigner: Signer ist nicht der Debug-Key: {l.strip()}' for l in bad]
    return []


def check_manifest_xmltree(output):
    """Prueft die Ausgabe von `aapt2 dump xmltree --file AndroidManifest.xml` (fail-closed)."""
    if not re.search(r'^\s*E: manifest\b', output, re.MULTILINE):
        return ['aapt2: kein <manifest>-Element in der xmltree-Ausgabe']
    errors = []
    for value in TESTONLY_RE.findall(output):
        if value.lower() not in TESTONLY_FALSE:
            errors.append(f'Manifest: android:testOnly={value} (Geraete lehnen das APK ab)')
    for value in DEBUGGABLE_RE.findall(output):
        if value.lower() not in TESTONLY_FALSE:
            errors.append(f'Manifest: android:debuggable={value} (Debug-Variante, native Bibliothek -O0)')
    if not LIB_NAME_RE.search(output):
        errors.append('Manifest: meta-data android.app.lib_name="SkyEmu" fehlt')
    return errors


def run_tool(cmd):
    print('+ ' + ' '.join(cmd), flush=True)
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return proc.returncode, proc.stdout


def default_build_tools():
    sdk = os.environ.get('ANDROID_HOME') or os.environ.get('ANDROID_SDK_ROOT')
    return os.path.join(sdk, 'build-tools', '36.0.0') if sdk else None


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('apk')
    parser.add_argument('--build-tools', default=default_build_tools(),
                        help='Verzeichnis mit aapt2, zipalign und apksigner (Default: $ANDROID_HOME/build-tools/36.0.0)')
    args = parser.parse_args(argv)
    if not args.build_tools or not os.path.isdir(args.build_tools):
        print(f'FEHLER: build-tools nicht gefunden: {args.build_tools}', file=sys.stderr)
        return 2

    errors = check_entries(args.apk)
    if not errors:
        print(f'Inhalt: OK ({len(REQUIRED_ENTRIES)} Pflichteintraege, keine Keystores, keine fremden .bin)')
    errors.extend(check_native_libs(args.apk))

    code, out = run_tool([os.path.join(args.build_tools, 'aapt2'), 'dump', 'xmltree', '--file',
                          'AndroidManifest.xml', args.apk])
    if code != 0:
        errors.append(f'aapt2 dump xmltree fehlgeschlagen (Exit {code}):\n{out[-2000:]}')
    else:
        manifest_errors = check_manifest_xmltree(out)
        errors.extend(manifest_errors)
        if not manifest_errors:
            print('Manifest: OK (kein testOnly/debuggable, lib_name=SkyEmu)')

    code, out = run_tool([os.path.join(args.build_tools, 'zipalign'), '-c', '-P', '16', '-v', '4', args.apk])
    if code != 0:
        errors.append(f'zipalign -c -P 16 -v 4 fehlgeschlagen (Exit {code}):\n{out[-2000:]}')
    else:
        print('zipalign: ' + (out.strip().splitlines() or [''])[-1])

    code, out = run_tool([os.path.join(args.build_tools, 'apksigner'), 'verify', '--print-certs', args.apk])
    if code != 0:
        errors.append(f'apksigner verify fehlgeschlagen (Exit {code}):\n{out[-2000:]}')
    else:
        signer_errors = check_signer_output(out)
        errors.extend(signer_errors)
        if not signer_errors:
            print(out.strip())

    if errors:
        print('\nVERIFIKATION FEHLGESCHLAGEN:', file=sys.stderr)
        for e in errors:
            print(f'  - {e}', file=sys.stderr)
        return 1
    print('\nVERIFIKATION BESTANDEN')
    return 0


if __name__ == '__main__':
    sys.exit(main())
