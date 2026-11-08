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
GFX = HERE.parent / "firmware" / "src" / "gfx.c"


# ---- (Rheia) the site's colours, from the firmware's themes (web/theme.js puts them on the pages)
def rheia_palettes():
    """The Rheia themes in gfx.c PALETTES (after SLOOP's five): name, palette number, c[5], bg, hi, trk[4] (8-bit RGB)."""
    src = GFX.read_text(encoding="utf-8")
    body = src[src.index("static const palette_t PALETTES[]"):src.index("#define PAL_RHEIA")]
    rgb = r"RGB\((\d+),\s*(\d+),\s*(\d+)\)"
    names = re.findall(r'\{"([A-Z]+)",', body)
    first = int(re.search(r"#define PAL_RHEIA (\d+)u", src).group(1))
    out = []
    for i, name in enumerate(names):
        if i < first:
            continue
        entry = body[body.index('{"' + name + '"'):]
        entry = entry[:entry.index("}},") + 2]
        cols = [tuple(int(v) for v in m) for m in re.findall(rgb, entry)]
        if len(cols) != 11:
            raise SystemExit(f"gfx.c: theme {name}: {len(cols)} colours, want 11 (c[5], bg, hi, trk[4])")
        out.append({"name": name, "index": i, "c": cols[:5], "bg": cols[5], "hi": cols[6], "trk": cols[7:]})
    return out


def lum(c):
    ch = [v / 255 for v in c]
    ch = [v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4 for v in ch]
    return 0.2126 * ch[0] + 0.7152 * ch[1] + 0.0722 * ch[2]


def contrast(a, b):
    x, y = sorted((lum(a), lum(b)), reverse=True)
    return (x + 0.05) / (y + 0.05)


def mix(a, b, t):
    return tuple(round(a[k] + (b[k] - a[k]) * t) for k in range(3))


def ensure(fg, bgs, ratio, to):
    """fg moved toward `to` (black or white) in small steps until it stands out from every colour in bgs by ratio"""
    for i in range(101):
        c = mix(fg, to, i / 100)
        if all(contrast(c, b) >= ratio for b in bgs):
            return c
    return to


RED = (255, 44, 52)                                  # the recording red (ui_studio.c TE_RED)


def theme_tokens(p):
    """the page's colours for one theme: dark (the FM-1's own) and light (the same hues on a pale ground)"""
    W, K = (255, 255, 255), (0, 0, 0)
    c, bg = p["c"], p["bg"]
    hx = lambda v: "#%02x%02x%02x" % v                                     # noqa: E731
    d_card = mix(bg, c[0], 0.55)
    dark = {"bg": bg, "card": d_card, "edge": mix(bg, c[1], 0.8), "ink": ensure(c[4], [bg, d_card], 7, W),
            "muted": ensure(c[2], [bg, d_card], 4.5, W), "accent": ensure(p["hi"], [bg, d_card], 4.5, W),
            "red": ensure(RED, [bg, d_card], 3, W)}
    dark.update({f"t{k + 1}": ensure(t, [bg, d_card], 3, W) for k, t in enumerate(p["trk"])})
    l_bg, l_card = mix(W, c[4], 0.3), mix(W, c[4], 0.1)
    light = {"bg": l_bg, "card": l_card, "edge": mix(l_bg, c[2], 0.45), "ink": ensure(bg, [l_bg, l_card], 7, K),
             "muted": ensure(c[2], [l_bg, l_card], 4.5, K), "accent": ensure(p["trk"][3], [l_bg, l_card], 4.5, K),
             "red": ensure(RED, [l_bg, l_card], 3, K)}
    light.update({f"t{k + 1}": ensure(t, [l_bg, l_card], 3, K) for k, t in enumerate(p["trk"])})
    return {m: {k: hx(v) for k, v in d.items()} for m, d in (("dark", dark), ("light", light))}


def theme_script():
    """web/theme.js with the themes' colours, as an inline <script> for a page's <head>"""
    js = (HERE / "theme.js").read_text(encoding="utf-8")
    themes = [{"name": p["name"], "index": p["index"], **theme_tokens(p)} for p in rheia_palettes()]
    js = re.sub(r"/\*THEMES\*/.*?/\*/THEMES\*/", lambda m: json.dumps(themes, separators=(",", ":")), js,
                count=1, flags=re.S)
    if "</script" in js:
        raise SystemExit("theme.js must not contain </script")
    return "<script>\n" + js + "</script>"


def put_theme(path):
    """a page's <!--THEME--> mark: the theme script"""
    html = path.read_text(encoding="utf-8")
    if html.count("<!--THEME-->") != 1:
        raise SystemExit(f"{path.name} must contain <!--THEME--> once (in <head>); update make_site.py")
    path.write_text(html.replace("<!--THEME-->", theme_script()), encoding="utf-8")
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
    put_theme(dst / "index.html")
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
        put_theme(emu_out / "index.html")
    app_files(out, version, name)
    landing = (HERE / "landing.html").read_text(encoding="utf-8").replace("<!--VERSION-->", version)
    card = ('<a class="card" href="themes/"><b>Themes</b><span>ASH, FROST, RUST, BLOOD and GAZE on the FM-1\'s '
            'screen, as the firmware draws them.</span></a>')
    (out / "index.html").write_text(landing.replace("<!--THEMES-->", card if theme_gallery(out) else ""), encoding="utf-8")
    put_theme(out / "index.html")
    print(f"site: {out}: webapp/installer ({len(html)} B), webapp/editor, firmware/{name} ({len(raw)} B, {product})")


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a not in ("--studio-guide", "--beta")]
    if len(args) != 3:
        sys.exit(__doc__)
    main(*args)
