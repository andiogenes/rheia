// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 the Rheia authors
// The Rheia emulator page: the panel (buttons, knobs, keys) to the firmware in emu-worklet.js, its screen and lights back.
// Panel roles (firmware/src/panel.c): buttons B_*, knobs EN_*, keys 0..26 from F3.
const BTN = { FX: 0, SEL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11,
  'OCT-': 12, 'OCT+': 13 };
const ENC = { SELECT: 0, ALGORITHM: 1, PRESETS: 2, 'KNOB 1': 3, 'KNOB 2': 4, 'KNOB 3': 5, 'KNOB 4': 6 };
const KNOB_COL = { 'KNOB 1': 'k1', 'KNOB 2': 'k2', 'KNOB 3': 'k3', 'KNOB 4': 'k4' };
// computer keys: the white keys on two rows, the black keys above them; buttons on the number row
const WHITE_KEYS = ['a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', 'z', 'x', 'c', 'v', 'b', 'n'];
const BLACK_KEYS = ['q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '['];
const BTN_KEYS = { '1': 'FX', '2': 'SEL', '3': 'ENV', '4': 'LFO', '5': 'EDIT', '6': 'GLO', '7': 'HOME', '8': 'SAVE',
  '9': 'ARP', '0': 'SEQ', ' ': 'PLAY', Enter: 'REC', '-': 'OCT-', '=': 'OCT+' };
const isBlack = (k) => [1, 3, 6, 8, 10].includes((53 + k) % 12);   // key 0 is F3 (MIDI 53)

const $ = (s) => document.querySelector(s);
let node = null;
let ctx = null;
let btnIds = [];
const btnEls = {};
const keyEls = [];
const latched = new Set();

function send(m) { if (node) node.port.postMessage(m); }

// ---- the panel
function makeButton(name, parent) {
  const b = document.createElement('button');
  b.className = 'btn';
  b.textContent = name;
  b.title = `${name}${Object.entries(BTN_KEYS).find(([, v]) => v === name)?.[0] ? ' (key ' +
    Object.entries(BTN_KEYS).find(([, v]) => v === name)[0].replace(' ', 'space') + ')' : ''} — shift-click to hold`;
  const id = BTN[name];
  const down = (ev) => {
    ev.preventDefault();
    if (ev.shiftKey) {                                 // shift-click: held until clicked again (two hands with a mouse)
      if (latched.has(id)) { latched.delete(id); b.classList.remove('latched'); send({ type: 'btn', id, down: false }); }
      else { latched.add(id); b.classList.add('latched'); send({ type: 'btn', id, down: true }); }
      return;
    }
    if (latched.has(id)) return;
    b.setPointerCapture(ev.pointerId);
    b.classList.add('pressed');
    send({ type: 'btn', id, down: true });
  };
  const up = () => {
    if (!b.classList.contains('pressed')) return;
    b.classList.remove('pressed');
    send({ type: 'btn', id, down: false });
  };
  b.addEventListener('pointerdown', down);
  b.addEventListener('pointerup', up);
  b.addEventListener('pointercancel', up);
  btnEls[id] = b;
  parent.appendChild(b);
}

function makeKnob(name, parent, small) {
  const wrap = document.createElement('div');
  wrap.className = 'knob-wrap' + (small ? ' small' : '');
  const k = document.createElement('div');
  k.className = 'knob ' + (KNOB_COL[name] || '');
  k.tabIndex = 0;
  k.setAttribute('role', 'slider');
  k.setAttribute('aria-label', name);
  k.title = `${name}: drag up / down, or the mouse wheel`;
  const label = document.createElement('div');
  label.className = 'knob-label';
  label.textContent = name;
  wrap.append(k, label);
  parent.appendChild(wrap);
  const id = ENC[name];
  let angle = 0;
  const turn = (d) => {
    if (!d) return;
    angle += d * 15;
    k.style.setProperty('--a', angle + 'deg');
    send({ type: 'enc', id, d });
  };
  let lastY = null;
  let rest = 0;
  k.addEventListener('pointerdown', (ev) => { k.setPointerCapture(ev.pointerId); lastY = ev.clientY; rest = 0; });
  k.addEventListener('pointermove', (ev) => {
    if (lastY === null) return;
    rest += (lastY - ev.clientY) / 8;                  // 8 px a detent
    lastY = ev.clientY;
    const d = Math.trunc(rest);
    rest -= d;
    turn(d);
  });
  const end = () => { lastY = null; };
  k.addEventListener('pointerup', end);
  k.addEventListener('pointercancel', end);
  k.addEventListener('wheel', (ev) => { ev.preventDefault(); turn(ev.deltaY < 0 ? 1 : -1); }, { passive: false });
  k.addEventListener('keydown', (ev) => {
    if (ev.key === 'ArrowUp' || ev.key === 'ArrowRight') { turn(1); ev.preventDefault(); ev.stopPropagation(); }
    if (ev.key === 'ArrowDown' || ev.key === 'ArrowLeft') { turn(-1); ev.preventDefault(); ev.stopPropagation(); }
  });
}

function makeKeys(parent) {
  let white = 0, black = 0;
  for (let k = 0; k < 27; k++) {
    const el = document.createElement('div');
    const b = isBlack(k);
    el.className = 'key ' + (b ? 'black' : 'white');
    if (b) el.style.left = `calc(${white} * var(--kw) - var(--bw) / 2)`;
    const label = document.createElement('span');
    label.textContent = (b ? BLACK_KEYS[black++] : WHITE_KEYS[white++] || '').toUpperCase();
    el.appendChild(label);
    const down = (ev) => { ev.preventDefault(); el.setPointerCapture(ev.pointerId); el.classList.add('pressed'); send({ type: 'key', id: k, down: true }); };
    const up = () => { if (el.classList.contains('pressed')) { el.classList.remove('pressed'); send({ type: 'key', id: k, down: false }); } };
    el.addEventListener('pointerdown', down);
    el.addEventListener('pointerup', up);
    el.addEventListener('pointercancel', up);
    keyEls[k] = el;
    parent.appendChild(el);
  }
}

function buildPanel() {
  for (const n of ['SELECT', 'ALGORITHM', 'PRESETS']) makeKnob(n, $('#encoders'), true);
  for (const n of ['KNOB 1', 'KNOB 2', 'KNOB 3', 'KNOB 4']) makeKnob(n, $('#knobs'));
  for (const n of ['FX', 'SEL', 'ENV', 'LFO', 'EDIT', 'GLO']) makeButton(n, $('#row1'));
  for (const n of ['HOME', 'SAVE', 'ARP', 'SEQ', 'PLAY', 'REC']) makeButton(n, $('#row2'));
  for (const n of ['OCT-', 'OCT+']) makeButton(n, $('#oct'));
  makeKeys($('#keys'));
}

// ---- the computer keyboard
const keyDown = new Set();
function keyIndex(ch) {
  let w = WHITE_KEYS.indexOf(ch), bl = BLACK_KEYS.indexOf(ch);
  if (w < 0 && bl < 0) return -1;
  for (let k = 0, wi = 0, bi = 0; k < 27; k++) {
    if (isBlack(k)) { if (bi++ === bl) return k; } else if (wi++ === w) return k;
  }
  return -1;
}
function onKey(ev, down) {
  if (ev.metaKey || ev.ctrlKey || ev.altKey) return;
  const ch = ev.key.length === 1 ? ev.key.toLowerCase() : ev.key;
  if (down && keyDown.has(ch)) { ev.preventDefault(); return; }
  if (down) keyDown.add(ch); else keyDown.delete(ch);
  if (BTN_KEYS[ch]) {
    const id = BTN[BTN_KEYS[ch]];
    btnEls[id]?.classList.toggle('pressed', down);
    send({ type: 'btn', id, down });
    ev.preventDefault();
    return;
  }
  const k = keyIndex(ch);
  if (k >= 0) {
    keyEls[k].classList.toggle('pressed', down);
    send({ type: 'key', id: k, down });
    ev.preventDefault();
  }
}

// ---- the screen and the lights
const canvas = $('#screen');
const g = canvas.getContext('2d');
const img = g.createImageData(240, 240);
function showScreen(px, leds) {
  const d = img.data;
  for (let i = 0; i < px.length; i++) {
    const v = px[i];
    const p = ((v & 255) << 8) | (v >> 8);          // (stored byte-swapped, as the LCD takes it)
    d[i * 4] = ((p >> 11) * 527 + 23) >> 6;
    d[i * 4 + 1] = (((p >> 5) & 63) * 259 + 33) >> 6;
    d[i * 4 + 2] = ((p & 31) * 527 + 23) >> 6;
    d[i * 4 + 3] = 255;
  }
  g.putImageData(img, 0, 0);
  btnIds.forEach((id, b) => btnEls[b]?.setAttribute('data-led', leds[id] ?? 0));
  for (let k = 0; k < 27; k++) keyEls[k]?.setAttribute('data-led', leds[14 + k] ?? 0);
}

// ---- power
async function powerOn() {
  $('#power').disabled = true;
  $('#power').textContent = 'Starting…';
  try {
    ctx = new AudioContext({ sampleRate: 44100, latencyHint: 'interactive' });
    await ctx.audioWorklet.addModule('emu-worklet.js');
    node = new AudioWorkletNode(ctx, 'rheia-emu', { numberOfInputs: 0, outputChannelCount: [2] });
    node.connect(ctx.destination);
    node.port.onmessage = (ev) => {
      const m = ev.data;
      if (m.type === 'ready') {
        btnIds = m.btnIds;
        $('#overlay').hidden = true;
        $('#status').textContent = ctx.sampleRate === m.rate ? 'Running' : `Running (audio at ${ctx.sampleRate} Hz)`;
      } else if (m.type === 'screen') {
        showScreen(m.px, m.leds);
      }
    };
    const bytes = await (await fetch('rheia-emu.wasm')).arrayBuffer();
    node.port.postMessage({ type: 'wasm', bytes }, [bytes]);
    await ctx.resume();
  } catch (err) {
    $('#power').textContent = 'Could not start';
    $('#status').textContent = String(err && err.message || err);
  }
}

buildPanel();
$('#power').addEventListener('click', powerOn);
window.addEventListener('keydown', (ev) => onKey(ev, true));
window.addEventListener('keyup', (ev) => onKey(ev, false));
window.addEventListener('blur', () => {
  for (const ch of keyDown) onKey({ key: ch, preventDefault() {} }, false);
});
document.addEventListener('visibilitychange', () => {
  if (!ctx) return;
  if (document.hidden) ctx.suspend(); else ctx.resume();
});
