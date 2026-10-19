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


XMLTREE_HEAD = '''N: android=http://schemas.android.com/apk/res/android (line=2)
  E: manifest (line=2)
    A: package="com.skyemu.omega" (Raw: "com.skyemu.omega")
      E: application (line=29)
        A: http://schemas.android.com/apk/res/android:label(0x01010001)=@0x7f0e001c
          E: meta-data (line=125)
            A: http://schemas.android.com/apk/res/android:name(0x01010003)="android.app.lib_name" (Raw: "android.app.lib_name")
            A: http://schemas.android.com/apk/res/android:value(0x01010024)="SkyEmu" (Raw: "SkyEmu")
'''


def test_manifest_without_testonly_ok():
    assert v.check_manifest_xmltree(XMLTREE_HEAD) == []


@pytest.mark.parametrize('value', ['false', '0x00000000', '0'])
def test_manifest_testonly_false_ok(value):
    out = XMLTREE_HEAD + f'        A: http://schemas.android.com/apk/res/android:testOnly(0x01010272)={value}\n'
    assert v.check_manifest_xmltree(out) == []


@pytest.mark.parametrize('value', ['true', '0xffffffff', '1', '@0x7f050001', 'garbage'])
def test_manifest_testonly_true_rejected(value):
    out = XMLTREE_HEAD + f'        A: http://schemas.android.com/apk/res/android:testOnly(0x01010272)={value}\n'
    errors = v.check_manifest_xmltree(out)
    assert errors and 'testOnly' in errors[0]


def test_manifest_xmltree_without_manifest_rejected():
    assert v.check_manifest_xmltree('') != []
    assert v.check_manifest_xmltree('ERROR: something went wrong') != []


PERMISSIONS_OK = """package: com.skyemu.omega
uses-permission: name='android.permission.WRITE_EXTERNAL_STORAGE' maxSdkVersion='28'
uses-permission: name='android.permission.READ_EXTERNAL_STORAGE' maxSdkVersion='32'
permission: com.skyemu.omega.DYNAMIC_RECEIVER_NOT_EXPORTED_PERMISSION
uses-permission: name='com.skyemu.omega.DYNAMIC_RECEIVER_NOT_EXPORTED_PERMISSION'
"""
PERMISSIONS_INTERNET = PERMISSIONS_OK + "uses-permission: name='android.permission.INTERNET'\n"


def _run_main(tmp_path, monkeypatch, xmltree, aapt2_code=0, permissions=PERMISSIONS_OK, permissions_code=0):
    apk = make_apk(tmp_path / 'ok.apk')
    bt = tmp_path / 'bt'
    bt.mkdir()
    calls = []

    def fake_run_tool(cmd):
        calls.append(cmd)
        tool = os.path.basename(cmd[0])
        if tool == 'aapt2' and cmd[2] == 'permissions':
            return permissions_code, permissions
        if tool == 'aapt2':
            return aapt2_code, xmltree
        if tool == 'apksigner':
            return 0, 'Signer #1 certificate DN: C=US, O=Android, CN=Android Debug\n'
        return 0, 'Verification succesful\n'

    monkeypatch.setattr(v, 'run_tool', fake_run_tool)
    monkeypatch.setattr(v, 'check_native_libs', lambda apk: [])
    code = v.main([apk, '--build-tools', str(bt)])
    return code, calls


def test_main_runs_aapt2_xmltree(tmp_path, monkeypatch):
    code, calls = _run_main(tmp_path, monkeypatch, XMLTREE_HEAD)
    assert code == 0
    assert any(os.path.basename(c[0]) == 'aapt2' and c[1:5] == ['dump', 'xmltree', '--file', 'AndroidManifest.xml']
               for c in calls)


def test_main_fails_on_testonly_true(tmp_path, monkeypatch):
    out = XMLTREE_HEAD + '        A: http://schemas.android.com/apk/res/android:testOnly(0x01010272)=true\n'
    code, _ = _run_main(tmp_path, monkeypatch, out)
    assert code == 1


def test_main_fails_when_aapt2_fails(tmp_path, monkeypatch):
    code, _ = _run_main(tmp_path, monkeypatch, XMLTREE_HEAD, aapt2_code=1)
    assert code == 1


@pytest.mark.parametrize('value', ['true', '0xffffffff'])
def test_manifest_debuggable_rejected(value):
    # Debug-Variante = native -O0 (CMake Debug): Emulator praktisch unbenutzbar langsam.
    out = XMLTREE_HEAD + f'        A: http://schemas.android.com/apk/res/android:debuggable(0x0101000f)={value}\n'
    errors = v.check_manifest_xmltree(out)
    assert errors and 'debuggable' in errors[0]


def test_manifest_debuggable_false_ok():
    out = XMLTREE_HEAD + '        A: http://schemas.android.com/apk/res/android:debuggable(0x0101000f)=false\n'
    assert v.check_manifest_xmltree(out) == []


def test_manifest_without_lib_name_rejected():
    out = XMLTREE_HEAD.split('          E: meta-data')[0]
    errors = v.check_manifest_xmltree(out)
    assert errors and 'lib_name' in errors[0]


def test_main_fails_on_debuggable(tmp_path, monkeypatch):
    out = XMLTREE_HEAD + '        A: http://schemas.android.com/apk/res/android:debuggable(0x0101000f)=true\n'
    code, _ = _run_main(tmp_path, monkeypatch, out)
    assert code == 1


def test_permissions_offline_ok():
    assert v.check_permissions(PERMISSIONS_OK) == []


@pytest.mark.parametrize('line', ["uses-permission: name='android.permission.INTERNET'",
                                  "uses-permission-sdk-23: name='android.permission.INTERNET'"])
def test_permissions_internet_rejected(line):
    errors = v.check_permissions(PERMISSIONS_OK + line + '\n')
    assert errors == ['Manifest: verbotene Permission android.permission.INTERNET (Offline-Build)']


@pytest.mark.parametrize('output', ['', 'ERROR: failed opening zip'])
def test_permissions_without_package_rejected(output):
    assert v.check_permissions(output) == ['aapt2: keine package-Zeile in der permissions-Ausgabe']


def test_main_runs_aapt2_permissions(tmp_path, monkeypatch):
    code, calls = _run_main(tmp_path, monkeypatch, XMLTREE_HEAD)
    assert code == 0
    assert any(os.path.basename(c[0]) == 'aapt2' and c[1:3] == ['dump', 'permissions'] for c in calls)


def test_main_fails_on_internet_permission(tmp_path, monkeypatch):
    code, _ = _run_main(tmp_path, monkeypatch, XMLTREE_HEAD, permissions=PERMISSIONS_INTERNET)
    assert code == 1


def test_main_fails_when_aapt2_permissions_fails(tmp_path, monkeypatch):
    code, _ = _run_main(tmp_path, monkeypatch, XMLTREE_HEAD, permissions_code=1)
    assert code == 1
