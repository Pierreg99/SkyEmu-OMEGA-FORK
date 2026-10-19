"""Quelltext-Checks: Android-Build ist offline und Touch-Eingaben erreichen ImGui."""
import os
import re

import pytest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))


def read(rel):
    with open(os.path.join(ROOT, rel), encoding='utf-8-sig') as f:
        return f.read()


def strip_comments(src):
    src = re.sub(r'/\*.*?\*/', '', src, flags=re.DOTALL)
    return re.sub(r'//[^\n]*', '', src)


def function_body(src, signature):
    start = src.index(signature)
    open_brace = src.index('{', start)
    depth = 0
    for i in range(open_brace, len(src)):
        if src[i] == '{':
            depth += 1
        elif src[i] == '}':
            depth -= 1
            if depth == 0:
                return src[open_brace:i + 1]
    raise AssertionError(f'Funktionsende nicht gefunden: {signature}')


def test_hcs_binds_localhost_only():
    src = strip_comments(read('src/http_control_server.cpp'))
    assert re.search(r'listen\(\s*"0\.0\.0\.0"', src) is None
    assert re.search(r'listen\(\s*"127\.0\.0\.1"', src)


def test_android_does_not_autostart_hcs():
    sokol = strip_comments(read('src/sokol/sokol_app.h'))
    assert re.search(r'\bse_android_start_hcs\s*\(\s*\)\s*;', sokol) is None
    body = strip_comments(function_body(read('src/main.c'), 'void se_android_start_hcs()'))
    assert 'hcs_update' not in body
    assert re.search(r'http_control_server_enable\s*=\s*(true|1)\b', body) is None


def test_android_forces_hcs_off_every_frame():
    body = function_body(read('src/main.c'), 'void se_update_frame()')
    assert re.search(r'#ifdef SE_PLATFORM_ANDROID\s*\n\s*gui_state\.settings\.http_control_server_enable\s*=\s*false;', body)


def test_hcs_checkbox_not_forced_on():
    src = read('src/main.c')
    m = re.search(r'se_checkbox\("Enable HTTP Control Server",&enable_hcs\);\s*\n\s*([^\n]+)', src)
    assert m and re.search(r'http_control_server_enable\s*=\s*enable_hcs\s*;', m.group(1))


def test_hcs_default_disabled():
    src = strip_comments(read('src/se_settings.c'))
    assert re.search(r'http_control_server_enable\s*=\s*1\b', src) is None
    assert re.search(r'http_control_server_enable\s*=\s*0\b', src)


def test_android_https_requests_offline():
    body = function_body(read('src/https.cpp'), 'void https_request(')
    guard = '#if defined(SE_PLATFORM_ANDROID) && !defined(SE_ANDROID_ALLOW_NETWORK)'
    assert body.index(guard) < body.index('pool.push_job')


def test_android_build_disables_retro_achievements():
    assert "'-DENABLE_RETRO_ACHIEVEMENTS=OFF'" in strip_comments(read('tools/android_project/app/build.gradle'))


def test_host_manifest_removes_internet():
    manifest = re.sub(r'<!--.*?-->', '', read('tools/android_project/host/src/main/AndroidManifest.xml'), flags=re.DOTALL)
    assert re.search(r'<uses-permission\s+android:name="android\.permission\.INTERNET"\s+tools:node="remove"\s*/>',
                     manifest)


@pytest.mark.parametrize('first, second', [
    ('AMOTION_EVENT_ACTION_DOWN', 'AMOTION_EVENT_ACTION_POINTER_DOWN'),
    ('AMOTION_EVENT_ACTION_UP', 'AMOTION_EVENT_ACTION_POINTER_UP'),
])
def test_android_single_touch_reaches_imgui(first, second):
    # Ohne Fallthrough liefert ein Ein-Finger-Tipp weder TOUCHES_BEGAN noch TOUCHES_ENDED:
    # Menues, Buttons und Touch-Controls reagieren dann nicht.
    body = strip_comments(function_body(read('src/sokol/sokol_app.h'),
                                        '_SOKOL_PRIVATE bool _sapp_android_touch_event(const AInputEvent* e)'))
    case = body[body.index(f'case {first}:'):body.index(f'case {second}:')]
    assert 'break' not in case and 'type' not in case


def test_android_offline_hides_google_drive_section():
    # Offline liefert https_request leer; ein sichtbarer Login-Button waere tot.
    body = function_body(strip_comments(read('src/main.c')), 'void se_draw_menu_panel()')
    section = body.index('" Google Drive"')
    guard = body.rfind('#if', 0, section)
    assert guard != -1, 'Google-Drive-Abschnitt ohne Praeprozessor-Guard'
    line = body[guard:body.index('\n', guard)]
    assert 'SE_PLATFORM_ANDROID' in line and 'SE_ANDROID_ALLOW_NETWORK' in line
    end = body.index('#endif', section)
    assert body.index('Action Replay Codes') > end
