// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 the Rheia authors
// (Rheia) the site's colours: the firmware's own themes (firmware/src/gfx.c PALETTES, ASH .. GAZE), dark or light as
// the system is set. web/make_site.py puts this script, with the themes' colours worked out (THEMES below: for each,
// its palette number on the FM-1 and the page's colours in both modes), into each page's <head>, so the page is in
// its colours before it is drawn. A page shows the picker where it has an element with data-theme-picker; the choice
// is kept in this browser. 'rheia-theme' is sent on window when the theme changes (the emulator: its screen too).
(function () {
  const THEMES = /*THEMES*/[{ name: 'ASH', index: 5, dark: {}, light: {} }]/*/THEMES*/;
  const KEY = 'rheia-theme';
  const media = window.matchMedia ? window.matchMedia('(prefers-color-scheme: light)') : null;
  let name = 'ASH';
  try { name = localStorage.getItem(KEY) || name; } catch (e) { /* (storage blocked: the default) */ }
  const find = (n) => THEMES.find((t) => t.name === n) || THEMES[0];
  const mode = () => (media && media.matches ? 'light' : 'dark');

  function apply() {
    const t = find(name), root = document.documentElement, c = t[mode()];
    for (const k in c) root.style.setProperty('--' + k, c[k]);
    for (const k in t.dark) root.style.setProperty('--dev-' + k, t.dark[k]);   // (a dark device: the emulator)
    root.style.colorScheme = mode();
    root.dataset.rheiaTheme = t.name.toLowerCase();
    root.dataset.rheiaMode = mode();
    document.querySelectorAll('[data-theme-picker] button').forEach((b) =>
      b.setAttribute('aria-pressed', String(b.dataset.name === t.name)));
  }

  function choose(n) {
    name = find(n).name;
    try { localStorage.setItem(KEY, name); } catch (e) { /* (only this visit) */ }
    apply();
    window.dispatchEvent(new CustomEvent('rheia-theme', { detail: { name, index: find(name).index } }));
  }

  function pickers() {
    const css = document.createElement('style');
    css.textContent =
      '[data-theme-picker]{display:flex;flex-wrap:wrap;gap:6px;align-items:center}' +
      '[data-theme-picker] button{font:600 11px/1 system-ui,sans-serif;letter-spacing:.08em;cursor:pointer;' +
      'display:inline-flex;align-items:center;gap:6px;padding:6px 8px;border-radius:999px;' +
      'border:1px solid var(--edge);background:var(--card);color:var(--muted)}' +
      '[data-theme-picker] button[aria-pressed=true]{color:var(--ink);border-color:var(--accent)}' +
      '[data-theme-picker] button:focus-visible{outline:2px solid var(--accent);outline-offset:2px}' +
      '[data-theme-picker] i{display:inline-flex;width:24px;height:10px;border-radius:2px;overflow:hidden}' +
      '[data-theme-picker] i b{flex:1}';
    document.head.appendChild(css);
    document.querySelectorAll('[data-theme-picker]').forEach((el) => {
      el.setAttribute('role', 'group');
      el.setAttribute('aria-label', 'Theme');
      for (const t of THEMES) {
        const b = document.createElement('button');
        b.type = 'button';
        b.dataset.name = t.name;
        b.title = t.name + ': the FM-1 theme of the same name (MENU > COLOR)';
        const sw = document.createElement('i');                 // the theme's four track colours
        for (const k of ['t1', 't2', 't3', 't4']) {
          const s = document.createElement('b');
          s.style.background = t.dark[k];
          sw.appendChild(s);
        }
        b.append(sw, t.name);
        b.addEventListener('click', () => choose(t.name));
        el.appendChild(b);
      }
    });
    apply();
  }

  apply();
  if (media) {
    if (media.addEventListener) media.addEventListener('change', apply);
    else if (media.addListener) media.addListener(apply);
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', pickers);
  else pickers();
  window.rheiaTheme = { themes: THEMES, current: () => find(name), choose, mode };
})();
