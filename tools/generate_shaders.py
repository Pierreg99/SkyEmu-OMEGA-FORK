#!/usr/bin/env python3
"""Regenerate all GPU backends with the compiler bundled for this Sokol version."""
import argparse
import pathlib
import platform
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check', action='store_true', help='fail when the checked-in header is stale')
parser.add_argument('--compiler', type=pathlib.Path, help='path to a compatible sokol-shdc compiler')
args = parser.parse_args()
system = {'Linux': 'linux', 'Darwin': 'osx', 'Windows': 'win32'}[platform.system()]
compiler = args.compiler or ROOT / 'tools/sokol-tools/bin' / system / ('sokol-shdc.exe' if system == 'win32' else 'sokol-shdc')
if not compiler.is_file():
    parser.exit(1, 'No compatible shader compiler found. Run on Linux x86_64 or pass --compiler PATH.\n')
with tempfile.TemporaryDirectory() as folder:
    output = pathlib.Path(folder) / 'lcd_shaders.h'
    subprocess.run([str(compiler), '-i', 'lcd_shaders.shd', '-o', str(output), '-l',
                    'glsl330:glsl100:glsl300es:hlsl4:metal_macos:metal_ios:metal_sim:wgpu'],
                   cwd=ROOT / 'src', check=True)
    generated = output.read_text().replace(str(output), 'lcd_shaders.h')
    generated = '\n'.join(line.rstrip() for line in generated.splitlines()) + '\n'
    target = ROOT / 'src/lcd_shaders.h'
    if args.check:
        if generated != target.read_text():
            parser.exit(1, 'Shader header is stale. Run python3 tools/generate_shaders.py\n')
    else:
        target.write_text(generated)
    print('All shader backends are current.')
