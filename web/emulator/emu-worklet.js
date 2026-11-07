// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 the Rheia authors
// The Rheia emulator in the audio thread: the firmware (rheia-emu.wasm) renders the sound block by block, as the FM-1's
// audio interrupt does, and every 735 samples (1/60 s) runs one UI pass, as the FM-1's main loop does. The page sends
// the panel (buttons, knobs, keys); the screen and the lights go back to it 30 times a second.
const UI_SAMPLES = 735;

class RheiaEmu extends AudioWorkletProcessor {
  constructor() {
    super();
    this.e = null;
    this.queue = [];
    this.buf = null;
    this.pos = 0;
    this.acc = 0;
    this.uiFrames = 0;
    this.ms = 16;
    this.port.onmessage = (ev) => this.message(ev.data);
  }

  async message(m) {
    if (m.type === 'wasm') {
      const { instance } = await WebAssembly.instantiate(m.bytes, {});
      this.e = instance.exports;
      this.e.emu_init();
      this.n = this.e.emu_frames();
      this.btnIds = Array.from({ length: 14 }, (_, b) => this.e.emu_btn_id(b));
      this.port.postMessage({ type: 'ready', rate: this.e.emu_rate(), btnIds: this.btnIds });
      this.post();
    } else {
      this.queue.push(m);
    }
  }

  // the panel events since the last UI pass, in order; a release of something pressed in this same batch waits for the
  // next pass, so a quick tap is still seen by the firmware (it reads the panel once a pass)
  applyInput() {
    const down = new Set();
    let i = 0;
    for (; i < this.queue.length; i++) {
      const m = this.queue[i];
      const id = m.type + ':' + m.id;
      if (m.type === 'enc') { this.e.emu_enc(m.id, m.d); continue; }
      if (!m.down && down.has(id)) break;
      if (m.down) down.add(id);
      if (m.type === 'btn') this.e.emu_button(m.id, m.down ? 1 : 0);
      else if (m.type === 'key') this.e.emu_key(m.id, m.down ? 1 : 0);
    }
    this.queue.splice(0, i);
  }

  post() {
    const e = this.e;
    const px = new Uint16Array(e.memory.buffer, e.emu_screen(), 240 * 240).slice();
    const leds = new Uint8Array(41);
    for (let id = 0; id < 41; id++) leds[id] = e.emu_led(id);
    this.port.postMessage({ type: 'screen', px, leds }, [px.buffer, leds.buffer]);
  }

  process(inputs, outputs) {
    const out = outputs[0];
    const L = out[0];
    const R = out[1] || out[0];
    if (!this.e) return true;
    for (let i = 0; i < L.length; i++) {
      if (!this.buf || this.pos >= this.n) {
        this.buf = new Float32Array(this.e.memory.buffer, this.e.emu_render(), this.n * 2);
        this.pos = 0;
        this.acc += this.n;
        if (this.acc >= UI_SAMPLES) {
          this.acc -= UI_SAMPLES;
          this.applyInput();
          this.ms = this.ms === 16 ? 17 : 16;      // (16.67 ms on average)
          this.e.emu_frame(this.ms);
          if (++this.uiFrames % 2 === 0) this.post();
        }
      }
      L[i] = this.buf[this.pos * 2];
      R[i] = this.buf[this.pos * 2 + 1];
      this.pos++;
    }
    return true;
  }
}
registerProcessor('rheia-emu', RheiaEmu);
