"""Unit-Tests fuer tools/verify_android_apk.py mit kuenstlichen APK-Zips."""
import os
import struct
import sys
import zipfile

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import verify_android_apk as v  # noqa: E402


def make_apk(path, extra=(), drop=()):
    entries = {name: b'x' for name in v.REQUIRED_ENTRIES}
    for name, marker in v.REQUIRED_TEXT.items():
        entries[name] = marker.encode()
    for name in drop:
        entries.pop(name)
    for name in extra:
        entries[name] = b'x'
    with zipfile.ZipFile(path, 'w') as zf:
        for name, data in entries.items():
            zf.writestr(name, data)
    return str(path)


def test_valid_apk_passes(tmp_path):
    apk = make_apk(tmp_path / 'ok.apk', extra=['DebugProbesKt.bin', 'META-INF/kotlinx_coroutines_core.version',
                                                'META-INF/kotlin-stdlib.bin'])
    assert v.check_entries(apk) == []


@pytest.mark.parametrize('name', [
    'assets/skyemu-open-signing-store',
    'res/raw/release.jks',
    'assets/debug.KEYSTORE',
    'assets/cert.p12',
    'assets/cert.pfx',
])
def test_keystores_rejected(tmp_path, name):
    errors = v.check_entries(make_apk(tmp_path / 'ks.apk', extra=[name]))
    assert len(errors) == 1 and name in errors[0]


@pytest.mark.parametrize('name', ['assets/drastic_bios_arm7.bin', 'assets/bios7.BIN', 'META-INF/other.bin'])
def test_foreign_bin_rejected(tmp_path, name):
    errors = v.check_entries(make_apk(tmp_path / 'bin.apk', extra=[name]))
    assert len(errors) == 1 and 'unerwartete .bin' in errors[0]


@pytest.mark.parametrize('name', ['assets/licenses/OpenSSL-LICENSE.txt', 'assets/licenses/curl-COPYING.txt',
                                  'assets/licenses/Drastic-NDS-BIOS-LICENSE.txt', 'lib/arm64-v8a/libSkyEmu.so'])
def test_missing_required_rejected(tmp_path, name):
    errors = v.check_entries(make_apk(tmp_path / 'miss.apk', drop=[name]))
    assert errors == [f'Pflichteintrag fehlt: {name}']


def test_license_without_marker_rejected(tmp_path):
    path = tmp_path / 'marker.apk'
    with zipfile.ZipFile(path, 'w') as zf:
        for name in v.REQUIRED_ENTRIES:
            zf.writestr(name, b'x')
    errors = v.check_entries(str(path))
    assert len(errors) == len(v.REQUIRED_TEXT)


def test_signer_debug_ok():
    out = 'Signer #1 certificate DN: CN=Android Debug, O=Android, C=US\nSigner #1 certificate SHA-256 digest: ab\n'
    assert v.check_signer_output(out) == []


def test_signer_other_rejected():
    out = 'Signer #1 certificate DN: CN=SkyEmu Release, O=Example\n'
    assert v.check_signer_output(out)


def test_signer_missing_rejected():
    assert v.check_signer_output('') == ['apksigner: kein Signer in der Ausgabe']


def test_invalid_elf_rejected():
    assert v.check_elf_alignment('lib/arm64-v8a/libSkyEmu.so', b'not an elf') == \
        ['lib/arm64-v8a/libSkyEmu.so: ungueltiger ELF-Header']


def _elf64(align, offset, vaddr):
    header = bytearray(64)
    header[:6] = b'\x7fELF\x02\x01'
    struct.pack_into('<Q', header, 32, 64)        # e_phoff
    struct.pack_into('<HH', header, 54, 56, 1)    # e_phentsize, e_phnum
    ph = struct.pack('<IIQQQQQQ', 1, 5, offset, vaddr, vaddr, 0, 0, align)
    return bytes(header) + ph


def test_elf_16k_aligned_ok():
    assert v.check_elf_alignment('x.so', _elf64(16384, 0, 0)) == []


def test_elf_4k_aligned_rejected():
    assert v.check_elf_alignment('x.so', _elf64(4096, 0, 0)) == ['x.so: LOAD-Segment nicht 16-KB-kompatibel']
