// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 the Rheia authors
// The emulator (tools/build_emu.py -> build/emu/rheia-emu.wasm) without a browser: it boots to the TRACKS screen,
// the panel reaches the drum track's GEN page, OCT+ writes a pattern, PLAY makes sound, STOP brings silence back.
//   node tests/emu_test.mjs [build/emu/rheia-emu.wasm]
import { readFileSync } from "node:fs";

const path = process.argv[2] || "build/emu/rheia-emu.wasm";
const { instance } = await WebAssembly.instantiate(readFileSync(path), {});
const e = instance.exports;
let failed = 0;
const ok = (c, what) => { console.log(`emu: ${what.padEnd(66)} ${c ? "ok" : "FAIL"}`); if (!c) failed++; };
const B = { SEQ: 9, PLAY: 10, OCTUP: 13 }, EN = { ALGO: 1 };

let peak = 0;
function frames(n) {                                 // n UI passes, the audio between them as in the worklet
  for (let f = 0; f < n; f++) {
    for (let q = 0; q < 23; q++) {
      const a = new Float32Array(e.memory.buffer, e.emu_render(), e.emu_frames() * 2);
      for (const v of a) peak = Math.max(peak, Math.abs(v));
    }
    e.emu_frame(16);
  }
}
const screen = () => new Uint16Array(e.memory.buffer, e.emu_screen(), 240 * 240).slice();
const lit = (s) => s.reduce((n, v) => n + (v !== 0), 0);
const differs = (a, b) => a.some((v, i) => v !== b[i]);
const tap = (b) => { e.emu_button(b, 1); frames(1); e.emu_button(b, 0); frames(1); };

e.emu_init();
frames(30);
const home = screen();
ok(e.emu_rate() === 44100 && e.emu_frames() > 0, "44.1 kHz, a block of frames");
ok(lit(home) > 5000, "boots to a drawn screen (TRACKS)");
peak = 0; frames(20);
ok(peak === 0, "stopped and nothing played: silence");

for (let i = 0; i < 3; i++) { e.emu_enc(EN.ALGO, 1); frames(2); }   // track 4: the drum track (a detent a pass)
const drums = screen();
ok(differs(home, drums), "ALGORITHM: the drum track's screen");
tap(B.SEQ);                                          // TRACKS -> the DRUMS screen (GRID)
const grid = screen();
for (let i = 0; i < 3; i++) tap(B.SEQ);              // GRID -> KIT -> SOUND -> GEN
const gen = screen();
ok(differs(drums, grid) && differs(grid, gen), "SEQ: the DRUMS screen, then three more: the GEN page");
peak = 0;
tap(B.PLAY); frames(60); tap(B.PLAY); frames(60);
ok(peak === 0, "PLAY before a write: the empty drum track is silent");
tap(B.OCTUP);                                        // write the pattern (TRAD, 1 bar, ACCENT, HUMAN 1)
peak = 0;
tap(B.PLAY); frames(90);
ok(peak > 0.01, `PLAY: the written pattern sounds (peak ${peak.toFixed(3)})`);
tap(B.PLAY); frames(60);
peak = 0; frames(30);
ok(peak < 0.001, "PLAY again: stopped, the tails gone");
const leds = Array.from({ length: 41 }, (_, i) => e.emu_led(i));
ok(leds.every((v) => v >= 0 && v <= 3), "the lights: 41, each 0..3");
// (Rheia) the page's theme picker: emu_palette puts the screen in that palette (the background: the most common colour)
const rgb565 = (r, g, b) => { const c = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3); return ((c >> 8) | (c << 8)) & 0xFFFF; };
const commonest = () => { const n = new Map(); for (const v of screen()) n.set(v, (n.get(v) || 0) + 1); return [...n].sort((a, b) => b[1] - a[1])[0][0]; };
const ashBg = commonest();
e.emu_palette(9); e.emu_frame(16); e.emu_frame(17);
const gazeBg = commonest();
e.emu_palette(99); e.emu_frame(16);
ok(ashBg === rgb565(12, 11, 10) && gazeBg === rgb565(20, 14, 30) && commonest() === gazeBg,
   "theme: starts on ASH; emu_palette(9): GAZE's background; a palette that is not there: no change");
console.log(failed ? `emu: ${failed} FAILED` : "emu: all passed");
process.exit(failed ? 1 : 0);
