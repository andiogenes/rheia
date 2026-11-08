#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Make the site (GitHub Pages):

  index.html                  redirect to the installer
  firmware/felucca-VER.fwsc   the package
  webapp/installer/index.html index_pkg.html with fm1pkg.js, fm1ota.js and the metadata inlined
  webapp/editor/index.html    editor.html (+ fukiai.ttf, FUKIAI-LICENSE.txt)
  webapp/editor/sloop-editor.html  the editor in one file, for offline use (SLOOP 2.6): its icon font inlined,
                              its links to the installer and the font licence online; opened from the disk in
                              Chrome / Edge (file:// is a secure context: Web MIDI asks for permission)
  webapp/emulator/            (Rheia) the emulator, when build/emu has it (tools/build_emu.py)
  themes/                     (Rheia) the themes' screens as the firmware draws them, when build/host/ui has them
                              (tests/ui_pages_test.c theme-NAME-SCREEN.ppm; PNG with Pillow)
  webapp/sw.js, webapp/sloop.webmanifest, webapp/icon-*.png  the site without internet (SLOOP 2.6): an app to
                              install; opened once online, the installer, the editor and the firmware are kept
                              by the browser and work offline, still on https (on Android a downloaded file opens
                              as content://, where Chrome refuses Web MIDI)
  src/                        not touched

  web/make_site.py build/felucca-X.Y.fwsc X.Y OUT_DIR [--beta]
  (--beta: web/beta_banner.html at the top of the installer: the beta channel, docs/beta)

The package identity (FM-1_9xx) is read from the package; the device reports it
after the install.
"""
import base64
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
THEMES = ("ash", "frost", "rust", "blood", "gaze")         # (Rheia) firmware/src/gfx.c PALETTES from PAL_RHEIA
SCREENS = ("boot", "tracks", "gen", "song", "about")
ONLINE = "https://andiogenes.github.io/rheia/"          # (Rheia: its own site)                 # (the offline editor's links)


def offline_editor(html):
    """editor.html as one file for the disk: the icon font inlined, the download link out, the relative links online"""
    font = base64.b64encode((HERE / "fukiai.ttf").read_bytes()).decode()
    for a, b in (('url("fukiai.ttf")', 'url(data:font/ttf;base64,' + font + ')'),
                 ('href="../installer/"', 'href="' + ONLINE + 'webapp/installer/" target="_blank" rel="noopener"'),
                 ('href="FUKIAI-LICENSE.txt"', 'href="' + ONLINE + 'webapp/editor/FUKIAI-LICENSE.txt" target="_blank" rel="noopener"')):
        if html.count(a) != 1:
            raise SystemExit(f"editor.html must contain {a} once; update make_site.py")
        html = html.replace(a, b)
    for pat, what in ((r"<!--DL-->.*?<!--/DL-->", "<!--DL-->...<!--/DL-->"),       # (the file has no site
                      (r"<!--PWA-->.*?<!--/PWA-->", "<!--PWA-->...<!--/PWA-->"),    # around it: no app to
                      (r"/\*PWA\*/.*?/\*/PWA\*/", "/*PWA*/.../*/PWA*/"),           # install, no copy kept)
                      (r'<button id="install"[^>]*></button>\s*', '<button id="install">')):
        html, n = re.subn(pat, "", html, flags=re.S)
        if n != 1:
            raise SystemExit(f"editor.html must contain {what} once; update make_site.py")
    return html


ICONS = ("icon-192.png", "icon-512.png", "icon-maskable-512.png")


def app_files(out, version, fw_name):
    """webapp/: the manifest, the icons and sw.js with the list of what it keeps (relative to webapp/) and a
    stamp of their contents, so any change makes a new sw.js and the browser takes the new site"""
    app = out / "webapp"
    for f in ICONS:
        shutil.copy(HERE / f, app / f)
    manifest = {
        "name": "Rheia", "short_name": "Rheia", "id": "editor/", "start_url": "editor/", "scope": "./",
        "display": "standalone", "background_color": "#000000", "theme_color": "#000000",
        "description": "Rheia for the M-VAVE FM-1: the installer, the editor and the emulator, also without internet",
        "icons": [{"src": "icon-192.png", "sizes": "192x192", "type": "image/png", "purpose": "any"},
                  {"src": "icon-512.png", "sizes": "512x512", "type": "image/png", "purpose": "any"},
                  {"src": "icon-maskable-512.png", "sizes": "512x512", "type": "image/png", "purpose": "maskable"}],
    }
    (app / "sloop.webmanifest").write_text(json.dumps(manifest, indent=1) + "\n", encoding="utf-8")
    keep = {"installer/": app / "installer" / "index.html", "editor/": app / "editor" / "index.html",
            "../firmware/" + fw_name: out / "firmware" / fw_name, "sloop.webmanifest": app / "sloop.webmanifest"}
    for f in ("fukiai.ttf", "FUKIAI-LICENSE.txt"):
        if (app / "editor" / f).exists():
            keep["editor/" + f] = app / "editor" / f
    for f in ("preview.js", "preview-worklet.js", "sloop-preview.wasm"):
        if (app / "editor" / f).exists():
            keep["editor/" + f] = app / "editor" / f
    emu = app / "emulator"
    if emu.exists():
        for f in sorted(emu.iterdir()):
            keep["emulator/" + (f.name if f.name != "index.html" else "")] = f
    for f in ICONS:
        keep[f] = app / f
    h = hashlib.sha256()
    for k in sorted(keep):
        h.update(k.encode() + b"\0" + keep[k].read_bytes())
    sw = (HERE / "sw.js").read_text(encoding="utf-8")
    for mark in ("/*STAMP*/", "/*FILES*/[]"):
        if sw.count(mark) != 1:
            raise SystemExit(f"sw.js must contain {mark} once; update make_site.py")
    sw = sw.replace("/*STAMP*/", f"{version}-{h.hexdigest()[:12]}").replace("/*FILES*/[]", json.dumps(sorted(keep)))
    (app / "sw.js").write_text(sw, encoding="utf-8")
    return sorted(keep)
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F
RHEIA_LOGO = '<span style="font:700 40px/1 system-ui,sans-serif;letter-spacing:.04em">RHEIA</span>'


def theme_gallery(out):
    """(Rheia) themes/: each Rheia theme's screens, drawn by the firmware itself on the host (ui_pages_test)."""
    src, dst = HERE.parent / "build" / "host" / "ui", out / "themes"
    if dst.exists():
        shutil.rmtree(dst)                         # never show old screens as the current ones
    if not all((src / f"theme-{t}-{k}.ppm").exists() for t in THEMES for k in SCREENS):
        return False
    try:
        from PIL import Image
    except ImportError:
        return False
    dst.mkdir(parents=True)
    rows = []
    for t in THEMES:
        imgs = []
        for k in SCREENS:
            Image.open(src / f"theme-{t}-{k}.ppm").save(dst / f"{t}-{k}.png", optimize=True)
            imgs.append(f'<figure><img src="{t}-{k}.png" width="240" height="240" alt="{t.upper()}: {k}" '
                        f'loading="lazy"><figcaption>{k.upper()}</figcaption></figure>')
        rows.append(f'<section><h2>{t.upper()}</h2><div class="row">{"".join(imgs)}</div></section>')
    (dst / "index.html").write_text((HERE / "themes.html").read_text(encoding="utf-8")
                                    .replace("<!--THEMES-->", "\n".join(rows)), encoding="utf-8")
    return True


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def product_of(raw):
    """the package identity: one marker byte after each of the first 20 blocks (fm1pkg.js productOf)"""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_9\d\d", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not a Felucca package (FM-1_9xx)")
    if b"FELUCCA-LOADER-1" not in raw:              # marker of firmware/loader
        raise SystemExit(f"{pkg}: no Felucca update loader in it")
    html = (HERE / "index_pkg.html").read_text(encoding="utf-8")
    lib = strip_module((HERE / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1ota.js").read_text(encoding="utf-8"))
    name = f"rheia-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    meta = json.dumps({"version": version, "product": product, "pkg": "../../firmware/" + name,
                       "sha256": hashlib.sha256(raw).hexdigest()})   # the page checks the download against it
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"index_pkg.html must contain {mark} once; update make_site.py")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta)
    html = html.replace("<!--LOGO-->", RHEIA_LOGO)   # (Rheia: its name; SLOOP's logo is SLOOP's)
    inst, ed, fw = out / "webapp" / "installer", out / "webapp" / "editor", out / "firmware"
    for d in (inst, ed, fw):
        d.mkdir(parents=True, exist_ok=True)
    guide = HERE.parent / "output/pdf/Studio-0.3-guide-rapide-FR.pdf"
    guide_link = ""
    if guide.exists() and "--studio-guide" in sys.argv:   # LIVE: that guide describes the removed RYTHME tab
        shutil.copy(guide, out / guide.name)
        guide_link = '<p lang="fr"><a href="../../' + guide.name + '">Guide rapide illustré (PDF, 4 pages)</a></p>'
    html = html.replace("<!--STUDIO_GUIDE-->", guide_link)
    banner = ""
    if "--beta" in sys.argv:                       # the beta channel (docs/beta): what it is, what it implies
        notes = HERE / "beta_banner.html"
        banner = notes.read_text(encoding="utf-8") if notes.exists() else "<p><b>BETA</b></p>"
    html = html.replace("<!--BANNER-->", banner)
    for old in list(fw.glob("felucca-*.fwsc")) + list(fw.glob("sloop-*.fwsc")) + list(fw.glob("rheia-*.fwsc")):   # one package: the current one
        old.unlink()
    shutil.copy(pkg, fw / name)
    ed_html = (HERE / "editor.html").read_text(encoding="utf-8")
    dl = f'download="sloop-editor-{re.sub(r"[^A-Za-z0-9.-]", "-", version)}.html"'
    if ed_html.count('download="sloop-editor.html"') != 1 or html.count('download="sloop-editor.html"') != 1:
        raise SystemExit('editor.html and index_pkg.html must link download="sloop-editor.html" once; update make_site.py')
    (ed / "index.html").write_text(ed_html.replace('download="sloop-editor.html"', dl), encoding="utf-8")
    (ed / "sloop-editor.html").write_text(offline_editor(ed_html), encoding="utf-8")
    (inst / "index.html").write_text(html.replace('download="sloop-editor.html"', dl), encoding="utf-8")
    for f in ("fukiai.ttf", "FUKIAI-LICENSE.txt"):
        if (HERE / f).exists():
            shutil.copy(HERE / f, ed / f)
    # Optional browser DSP, built separately without the hardware toolchain.
    for f in ("preview.js", "preview-worklet.js"):
        shutil.copy(HERE / f, ed / f)
    dsp = HERE.parent / "build" / "preview" / "sloop-preview.wasm"
    if dsp.exists():
        shutil.copy(dsp, ed / dsp.name)
    elif (ed / dsp.name).exists():
        (ed / dsp.name).unlink()  # never serve an old audio build as the current one
    emu_src, emu_out = HERE.parent / "build" / "emu", out / "webapp" / "emulator"
    if emu_out.exists():
        shutil.rmtree(emu_out)                     # never serve an old emulator as the current one
    if (emu_src / "rheia-emu.wasm").exists():
        shutil.copytree(emu_src, emu_out)
    app_files(out, version, name)
    landing = (HERE / "landing.html").read_text(encoding="utf-8").replace("<!--VERSION-->", version)
    card = ('<a class="card" href="themes/"><b>Themes</b><span>ASH, FROST, RUST, BLOOD and GAZE on the FM-1\'s '
            'screen, as the firmware draws them.</span></a>')
    (out / "index.html").write_text(landing.replace("<!--THEMES-->", card if theme_gallery(out) else ""), encoding="utf-8")
    print(f"site: {out}: webapp/installer ({len(html)} B), webapp/editor, firmware/{name} ({len(raw)} B, {product})")


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a not in ("--studio-guide", "--beta")]
    if len(args) != 3:
        sys.exit(__doc__)
    main(*args)
