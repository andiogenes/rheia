#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 the Rheia authors
"""Compile the Rheia emulator (firmware/host/emu.c: the firmware's sound, sequencer and screens) for the browser.

python3 tools/build_emu.py [OUT_DIR]        (default build/emu)

Needs clang with the wasm32 target and wasm-ld (lld), or Emscripten. Writes OUT_DIR/rheia-emu.wasm and copies
web/emulator/* next to it. No JieLi toolchain, SDK or FM-1 is needed.
"""
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / 'build/gen'
EXPORTS = ['init', 'button', 'enc', 'key', 'frame', 'screen', 'led', 'frames', 'rate', 'btn_id', 'render', 'palette']
FLAGS = ['-O2', '-fwrapv', '-fno-strict-aliasing', '-w', '-I' + str(GEN), '-I' + str(ROOT / 'firmware/src'),
         '-I' + str(ROOT / 'firmware/host')]


def main():
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'build/emu'
    out.mkdir(parents=True, exist_ok=True)
    GEN.mkdir(parents=True, exist_ok=True)
    for script, header in [('gen_tables.py', 'felucca_tables.h'), ('gen_samples.py', 'felucca_samples.h'),
                           ('gen_drumkits.py', 'felucca_drumkits.h'), ('gen_fm6_patches.py', 'felucca_fm6.h'),
                           ('gen_font.py', 'felucca_font.h'), ('gen_icons.py', 'felucca_icons.h'),
                           ('gen_logo.py', 'sloop_logo.h')]:
        subprocess.run([sys.executable, str(ROOT / 'tools' / script), str(GEN / header)], cwd=ROOT, check=True,
                       stdout=subprocess.DEVNULL)
    wasm = out / 'rheia-emu.wasm'
    emcc = shutil.which('emcc')
    if shutil.which('clang') and shutil.which('wasm-ld'):
        shim = ROOT / 'tools' / 'wasm_shim'
        mem = out / 'mem.o'
        subprocess.run(['clang', '--target=wasm32', '-O2', '-fno-builtin', '-ffreestanding', '-c', str(shim / 'mem.c'),
                        '-o', str(mem)], cwd=ROOT, check=True)
        subprocess.run(['clang', '--target=wasm32', *FLAGS, '-ffreestanding', '-nostdlib', '-I' + str(shim),
                        str(ROOT / 'firmware/host/emu.c'), str(mem), '-Wl,--no-entry', '-Wl,--export-memory',
                        '-Wl,--initial-memory=33554432', '-Wl,-z,stack-size=262144']
                       + ['-Wl,--export=emu_' + n for n in EXPORTS] + ['-o', str(wasm)], cwd=ROOT, check=True)
        mem.unlink()
    elif emcc:
        subprocess.run(['emcc', str(ROOT / 'firmware/host/emu.c'), *FLAGS, '--no-entry', '-sSTANDALONE_WASM=1',
                        '-sFILESYSTEM=0', '-sINITIAL_MEMORY=33554432', '-sSTACK_SIZE=262144',
                        '-sEXPORTED_FUNCTIONS=' + str(['_emu_' + n for n in EXPORTS]), '-o', str(wasm)],
                       cwd=ROOT, check=True)
    else:
        sys.exit('Needs clang + wasm-ld (lld), or Emscripten.')
    for f in (ROOT / 'web/emulator').iterdir():
        if f.is_file():
            shutil.copy(f, out / f.name)
    print('Emulator built:', wasm, f'({wasm.stat().st_size // 1024} KB)')


if __name__ == '__main__':
    main()
