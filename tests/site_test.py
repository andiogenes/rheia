#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""SLOOP 2.6: the offline editor (web/make_site.py offline_editor): one file that needs nothing next to it; and the
site without internet (webapp/sw.js, the manifest): what sw.js keeps is all there and all it needs."""
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "web"))
import make_site  # noqa: E402

src = (ROOT / "web" / "editor.html").read_text(encoding="utf-8")
off = make_site.offline_editor(src)
bad = 0


def check(what, ok):
    global bad
    print(f"site: {what:<70} {'ok' if ok else 'FAIL'}")
    bad += not ok


check("the editor links the offline file (download)", src.count('download="sloop-editor.html"') == 1)
check("the installer page links it too",
      (ROOT / "web" / "index_pkg.html").read_text(encoding="utf-8").count('href="../editor/sloop-editor.html"') == 1)
check("offline: the icon font inlined", 'url("fukiai.ttf")' not in off and "font/ttf;base64," in off)
check("offline: no download link to itself", "<!--DL-->" not in off and 'id="offline"' not in off)
check("offline: the installer and the font licence online",
      'href="../installer/"' not in off and make_site.ONLINE + "webapp/installer/" in off
      and 'href="FUKIAI-LICENSE.txt"' not in off)
rel = [m for m in re.findall(r'(?:src|href)="([^"#]+)"', off) if not re.match(r"(https?:|data:|mailto:)", m)]
check("offline: no other file next to it (no relative src / href)", not rel)
check("offline: no url() to a file", not re.search(r'url\((?!data:)["\']?[A-Za-z]', off))
check("offline: no app, no service worker (a file has no site)",
      "serviceWorker" not in off and "webmanifest" not in off and 'id="install"' not in off)

# the site without internet: a site made from any package (a fake one with the identity make_site checks)
tmp = Path(tempfile.mkdtemp())
try:
    pkg = bytearray(4096)
    for i, ch in enumerate("FM-1_900"):
        pkg[i * make_site.BLK + make_site.KEEP] = (ord(ch) + i + 1) & 0xFF
    for i in range(8, make_site.BLOCKS):
        pkg[i * make_site.BLK + make_site.KEEP] = 0x7D
    pkg[2000:2016] = b"FELUCCA-LOADER-1"
    (tmp / "p.fwsc").write_bytes(bytes(pkg))
    make_site.main(tmp / "p.fwsc", "9.9", tmp / "site")
    app = tmp / "site" / "webapp"
    sw = (app / "sw.js").read_text(encoding="utf-8")
    files = json.loads(re.search(r"const FILES = (\[.*?\]);", sw).group(1))
    path = lambda f: (app / f / "index.html") if f.endswith("/") else (app / f)   # noqa: E731
    check("sw.js: stamped with the version", 'const STAMP = "9.9-' in sw and "/*" + "FILES*/" not in sw)
    check("sw.js keeps the installer, the editor, its font, the firmware",
          {"installer/", "editor/", "editor/fukiai.ttf", "../firmware/rheia-9.9.fwsc"} <= set(files))
    check("sw.js: every file it keeps is in the site", all(path(f).is_file() for f in files))
    m = json.loads((app / "sloop.webmanifest").read_text(encoding="utf-8"))
    check("manifest: standalone, starts at the editor, icons 192 and 512 (+ maskable)",
          m["display"] == "standalone" and m["start_url"] == "editor/" and
          {i["sizes"] for i in m["icons"]} == {"192x192", "512x512"} and
          any(i.get("purpose") == "maskable" for i in m["icons"]) and
          all((app / i["src"]).is_file() for i in m["icons"]))
    for page in ("editor", "installer"):
        h = (app / page / "index.html").read_text(encoding="utf-8")
        check(f"{page}: links the manifest, registers ../sw.js, an install button",
              'href="../sloop.webmanifest"' in h and 'register("../sw.js")' in h and "beforeinstallprompt" in h)
    landing = (tmp / "site" / "index.html").read_text(encoding="utf-8")
    shots = make_site.HERE.parent / "build" / "host" / "ui"
    if all((shots / f"theme-{t}-{k}.ppm").exists() for t in make_site.THEMES for k in make_site.SCREENS):
        g = (tmp / "site" / "themes" / "index.html").read_text(encoding="utf-8")
        imgs = re.findall(r'src="([^"]+\.png)"', g)
        check("(Rheia) themes/: every theme's screens, each image there, linked from the landing page",
              len(imgs) == len(make_site.THEMES) * len(make_site.SCREENS) and
              all((tmp / "site" / "themes" / i).is_file() for i in imgs) and 'href="themes/"' in landing and
              "<!--" + "THEMES-->" not in g)
    else:
        check("(Rheia) no screens drawn (build/host/ui): no themes/, no link to it",
              not (tmp / "site" / "themes").exists() and 'href="themes/"' not in landing)
    check("(Rheia) the landing page: no marker left", "<!--" + "THEMES-->" not in landing and "<!--VERSION-->" not in landing)
    # (Rheia) the site's colours: the firmware's themes, dark and light, legible; the script in each page
    themes = [dict(p, **make_site.theme_tokens(p)) for p in make_site.rheia_palettes()]
    check("(Rheia) the site's themes are the FM-1's: ASH FROST RUST BLOOD GAZE, palettes 5..9 (gfx.c)",
          [t["name"] for t in themes] == ["ASH", "FROST", "RUST", "BLOOD", "GAZE"] and
          [t["index"] for t in themes] == list(range(5, 10)))
    rgb = lambda h: tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))       # noqa: E731
    worst = []
    for t in themes:
        for m in ("dark", "light"):
            c = {k: rgb(v) for k, v in t[m].items()}
            for k, need in [("ink", 7), ("muted", 4.5), ("accent", 4.5), ("t1", 3), ("t2", 3), ("t3", 3),
                            ("t4", 3), ("red", 3)]:
                r = min(make_site.contrast(c[k], c["bg"]), make_site.contrast(c[k], c["card"]))
                if r < need:
                    worst.append(f"{t['name']} {m} {k} {r:.2f}")
    check("(Rheia) every theme, dark and light: text 7:1, labels and links 4.5:1, tracks and red 3:1 " +
          ("; ".join(worst) or ""), not worst)
    pages = [tmp / "site" / "index.html"] + [f for f in (tmp / "site" / "themes" / "index.html",
                                                          app / "emulator" / "index.html") if f.exists()]
    ok = True
    for f in pages:
        h = f.read_text(encoding="utf-8")
        ok &= "<!--THEME-->" not in h and "/*THEMES*/" not in h and "window.rheiaTheme" in h and \
            h.index("window.rheiaTheme") < h.index("</head>") and "data-theme-picker" in h
        if shutil.which("node"):
            js = h[h.rindex("<script>", 0, h.index("window.rheiaTheme")) + 8:h.index("</script>", h.index("window.rheiaTheme"))]
            (tmp / "t.js").write_text(js, encoding="utf-8")
            ok &= subprocess.run(["node", "--check", str(tmp / "t.js")], capture_output=True).returncode == 0
    check(f"(Rheia) the theme script in <head> and a picker: {', '.join(str(f.relative_to(tmp / 'site')) for f in pages)}", ok)
    stamp = sw.split('const STAMP = "')[1].split('"')[0]
    (tmp / "site2").mkdir()
    make_site.main(tmp / "p.fwsc", "9.9", tmp / "site2")
    same = (tmp / "site2" / "webapp" / "sw.js").read_text(encoding="utf-8").split('const STAMP = "')[1].split('"')[0]
    pkg[3000] ^= 1
    (tmp / "p.fwsc").write_bytes(bytes(pkg))
    make_site.main(tmp / "p.fwsc", "9.9", tmp / "site2")
    other = (tmp / "site2" / "webapp" / "sw.js").read_text(encoding="utf-8").split('const STAMP = "')[1].split('"')[0]
    check("sw.js: the same site, the same stamp; a file changed, a new one", stamp == same and stamp != other)
finally:
    shutil.rmtree(tmp, ignore_errors=True)
print("site:", "FAILED" if bad else "offline editor and offline site test passed")
sys.exit(1 if bad else 0)
