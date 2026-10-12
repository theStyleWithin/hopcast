#!/usr/bin/env python3
"""HopCast UI map: turn a UI tour (hopcast/scripts/ui-tour.sh) into documentation.

  hopcast/scripts/ui-map.py <tour-dir> [--page OUT.html]

* writes hopcast/docs/ui-map/<section>/<screen>.webp (click spots marked in pink)
* writes hopcast/docs/ui-map/README.md (what each click opens, per section)
* with --page, also writes a self-contained HTML page (hopcast/docs/ui-map/page-template.html)

Captions live in SECTIONS below; screen names match the scenarios in hopcast/tests/ui-tour.txt
(without the number prefix). GPL-2.0-or-later.
"""
import argparse, base64, html, pathlib, re, shutil

from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parents[2]
DOCS = ROOT / "hopcast" / "docs" / "ui-map"
TOUR = ROOT / "hopcast" / "tests" / "ui-tour.txt"

H, C = "hopcast", "change"  # flags: HopCast-specific screen

# (section id, title, intro, [(screen, clicks, opens, note, flags)])
SECTIONS = [
    ("start", "Opening HopCast", "What you see the moment the app starts.", [
        ("main", ["Open HopCast"], "The studio, straight away",
         "No sign-in screen. The channel bar holds only Custom RTMP. The sidebar on the right has studio mode, dual output, music, draw pen, virtual camera, settings and notifications.", [H]),
    ]),
    ("channels", "Channels and stream keys", "Adding a Custom RTMP channel, from the first click to where the key is stored.", [
        ("my-channel-menu", ["My Channel ⋮"], "Channel menu", "Add channels · Reset live info.", []),
        ("custom-rtmp-form", ["Custom RTMP"], "Custom Channel form",
         "URL, Stream Key and Stream Name. “Auto-paste stream key only” is on by default. No links to PRISM’s output guide or help pages.", [H]),
        ("custom-rtmp", ["Custom RTMP", "URL dropdown"], "Connection type",
         "Only User Input (RTMP), (SRT) and (RIST). No list of platforms.", [H]),
        ("custom-rtmp-filled", ["Custom RTMP", "Fill in the form"], "Filled-in form",
         "Example values: rtmp://live.example.com/app and a made-up key. RTMP also offers optional ID and password fields.", []),
        ("storage-prompt", ["Save (first stream key)"], "Where to keep your stream keys",
         "Asked once: Choose Folder… or Use HopCast’s Folder. The unlock key always stays in ~/.local/share/hopcast.", [H]),
        ("folder-picker", ["Choose Folder…"], "Folder picker",
         "Pick any folder; it starts in Documents. In this test the picker was cancelled, so HopCast’s own folder was used.", [H]),
        ("after-channel-added", ["After saving"], "Channel in the bar",
         "The saved channel appears next to My Channel. It reads “Custom RT…” here because the name field already said “Custom RTMP”.", []),
        ("channel-in-bar-menu", ["Click the saved channel"], "Channel controls",
         "An on/off switch for including it when you go live, a share button and a ⋮ menu.", []),
        ("tools-stream-key-storage", ["Tools", "Stream Key Storage…"], "Stream key file location",
         "Shows where the encrypted file is and offers to move it: Choose Folder… or Close.", [H]),
    ]),
    ("menus", "Menu bar", "The eight menus under the channel bar.", [
        ("menu-settings", ["Settings"], "Settings menu",
         "Settings · Show Settings Folder · Exit (Ctrl+Q) · Show Videos Folder · Remux Recordings.", []),
        ("menu-scene-collection", ["Scene Collection"], "Scene Collection menu", "New · Export · Import · Scene Set Management.", []),
        ("menu-edit-source", ["Edit Source"], "Edit Source menu",
         "Undo/Redo, Copy, Paste (Reference/Duplicate), Copy/Paste Filters, Transform, Order, Preview Scaling, Lock Preview, Advanced Audio Properties.", []),
        ("menu-view", ["View"], "View menu",
         "Reset UI · Fullscreen Interface (F11) · Source Toolbar · Source Icons · Stats · Studio Mode · Multiview · Always On Top.", []),
        ("menu-docks", ["Docks"], "Docks menu",
         "Lock Docks · Full-Height Docks · Reset Docks, plus show/hide for Scenes, Sources, Audio Mixer and Music Playlist.", []),
        ("menu-profile", ["Profile"], "Profile menu", "New · Export · Import, and the current profile (Untitled).", []),
        ("menu-tools", ["Tools"], "Tools menu", "Stream Key Storage… is the only entry.", [H]),
        ("menu-help", ["Help"], "Help menu", "Program Info · Open Source License. No links to outside websites.", [H]),
        ("help-program-info", ["Help", "Program Info"], "About HopCast",
         "Name, version and the GPL notice crediting PRISM Live Studio and OBS Studio. No update check.", [H]),
    ]),
    ("scenes", "Scenes and sources", "The two panels under the preview.", [
        ("scenes-add", ["Scenes +"], "New scene", "“Please enter the name of the scene”, with OK and Cancel.", []),
        ("scenes-transition", ["Transition button"], "Scene transition", "Switch to scene: pick the transition (Cut).", []),
        ("scenes-more", ["Scenes ⋮"], "Scenes menu", "Detach · Export scene set · Display Method.", []),
        ("sources-add", ["Sources +"], "Add Source window",
         "Search, Video Capture Device (V4L2), Audio Input/Output Capture (PulseAudio), Image, Image Slide Show, Media and more. A plain hint until you pick one.", [H]),
        ("sources-more", ["Sources ⋮"], "Sources menu", "Detach.", []),
    ]),
    ("settings", "Settings", "The gear next to “1280x720 · 30fps” opens Settings on the Video page.", [
        ("settings-video", ["Gear"], "Video", "Base canvas 1600x900, output 1280x720, downscale filter, 30 FPS, dual output.", []),
        ("settings-general", ["Gear", "General"], "General", "Language, recording options, source snapping, projectors.", []),
        ("settings-output", ["Gear", "Output"], "Output", "Simple mode: 2500 kbps video, 160 kbps audio, software x264, encoder preset, recording.", []),
        ("settings-audio", ["Gear", "Audio"], "Audio", "48 kHz stereo, desktop and mic/aux devices, meters.", []),
        ("settings-hotkeys", ["Gear", "Hotkeys"], "Hotkeys", "Start/stop streaming and recording, pause, replay buffer, chapter markers.", []),
        ("settings-accessibility", ["Gear", "Accessibility"], "Accessibility", "Colour presets for source borders and mixer meters.", []),
        ("settings-advanced", ["Gear", "Advanced"], "Advanced", "Colour format NV12 / Rec. 709, HDR levels, recording file names, replay buffer.", []),
    ]),
    ("sidebar", "Right sidebar", "The column of buttons on the right edge.", []),  # filled in from SIDEBAR below
    ("bottom", "Bottom bar", "Going live and recording.", [
        ("performance", ["CPU ^"], "Performance panel",
         "CPU and memory, frame rate, dropped frames, stream and recording stats, free disk space.", []),
        ("go-live", ["Go Live"], "Empty-scene check",
         "“It looks like you haven’t added any video sources yet…” (the test scene was empty). With no channel switched on it says “You must select a channel first.”", []),
        ("record", ["Rec"], "Same check before recording", "Yes starts recording anyway; No goes back.", []),
    ]),
]

SIDEBAR = [
    ("sidebar-studio-mode", ["Studio mode button"], "Studio mode",
     "Splits the preview into Edit and Live. It stays on in the following screenshots.", []),
    ("sidebar-dual-output", ["Dual output button"], "Dual output",
     "Streams a horizontal and a vertical layout at once. With studio mode on it first asks: “Dual output is not available in the studio mode. Turn off the studio mode?”", []),
    ("sidebar-music", ["Music button"], "Music playlist",
     "Player controls and an Add Source button for a music playlist source.", []),
    ("sidebar-draw-pen", ["Draw pen button"], "Draw pen",
     "A drawing toolbar appears under the preview (pens, shapes, colours, undo, clear).", []),
    ("sidebar-virtual-camera", ["Virtual camera button"], "Virtual camera",
     "Starts the virtual camera; with an empty scene it first asks whether to continue.", []),
    ("sidebar-settings", ["Settings (gear)"], "Settings", "Opens Settings on the General page.", []),
    ("notifications", ["Bell"], "Notifications", "“No Alert.”", []),
]


def scenario_clicks():
    """screen name -> click positions leading to its screenshot, from ui-tour.txt."""
    clicks = {}
    for line in TOUR.read_text().splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        name, acts = [x.strip() for x in line.split("|", 1)]
        seq = []
        for act in acts.split(";"):
            act = act.strip()
            if m := re.match(r"r?click (\d+) (\d+)", act):
                seq.append((int(m[1]), int(m[2])))
            elif m := re.match(r"shot (\S+)", act):
                clicks[m[1]] = list(seq)
        clicks[name] = seq
    # sub-shots of the Custom RTMP save scenario: mark only the click that led there
    if "custom-rtmp-filled" in clicks:
        clicks["custom-rtmp-filled"] = clicks["custom-rtmp-filled"][:1]
    if "storage-prompt" in clicks:
        clicks["storage-prompt"] = clicks["storage-prompt"][-1:]
    clicks["folder-picker"] = []
    return clicks


def find_png(tour_dir, screen):
    exact = tour_dir / f"{screen}.png"
    if exact.exists():
        return exact
    hits = sorted(tour_dir.glob(f"[0-9][0-9]-{screen}.png"))
    return hits[0] if hits else None


def annotate(src, dst, points):
    im = Image.open(src).convert("RGB")
    d = ImageDraw.Draw(im)
    try:
        font = ImageFont.truetype("DejaVuSans-Bold.ttf", 15)
    except OSError:
        font = ImageFont.load_default()
    for i, (x, y) in enumerate(points, 1):
        d.ellipse((x - 17, y - 17, x + 17, y + 17), outline=(255, 64, 129), width=4)
        d.ellipse((x - 4, y - 4, x + 4, y + 4), fill=(255, 64, 129))
        if len(points) > 1:
            bx, by = x + 14, y - 30
            d.rounded_rectangle((bx, by, bx + 22, by + 22), radius=6, fill=(255, 64, 129))
            d.text((bx + 6, by + 2), str(i), fill="white", font=font)
    dst.parent.mkdir(parents=True, exist_ok=True)
    im.save(dst, "WEBP", quality=82, method=6)


def sections():
    out = []
    for sid, title, intro, items in SECTIONS:
        out.append((sid, title, intro, SIDEBAR if sid == "sidebar" else items))
    return out


def write_readme(entries):
    lines = ["# HopCast UI map", "",
             "Every button and menu in HopCast and what it opens, captured by the automated UI tour",
             "(`hopcast/scripts/ui-tour.sh`, scenarios in `hopcast/tests/ui-tour.txt`) on a virtual",
             "1600×900 screen without a window manager. Pink rings mark where the tour clicked; numbers give",
             "the order. Regenerate with `hopcast/scripts/ui-map.py <tour-dir>`.", ""]
    lines += ["Sections: " + " · ".join(f"[{t}](#{sid})" for sid, t, _, _ in entries), ""]
    for sid, title, intro, items in entries:
        lines += [f'<a id="{sid}"></a>', f"## {title}", "", intro, ""]
        for screen, clicks, opens, note, flags, rel in items:
            path = " › ".join(f"`{c}`" for c in clicks)
            tag = " *(HopCast change)*" if H in flags else ""
            lines += [f"### {opens}{tag}", "", f"**Click:** {path}", ""]
            if note:
                lines += [note, ""]
            lines += [f"![{opens}]({rel})", ""]
    (DOCS / "README.md").write_text("\n".join(lines))


def write_page(entries, out):
    tpl = (DOCS / "page-template.html").read_text()

    def card(screen, clicks, opens, note, flags, rel):
        steps = '<span class="arrow" aria-hidden="true">›</span>'.join(f"<kbd>{html.escape(c)}</kbd>" for c in clicks)
        alt = f"{' then '.join(clicks)} opens: {opens}"
        data = base64.b64encode((DOCS / rel).read_bytes()).decode()
        flag = '<span class="flag flag-hop">HopCast change</span>' if H in flags else ""
        return (f'<article class="card"><button class="shot" type="button" aria-label="Enlarge: {html.escape(alt)}">'
                f'<img src="data:image/webp;base64,{data}" alt="{html.escape(alt)}" loading="lazy" width="1600" height="900"></button>'
                f'<div class="meta"><div class="path">{steps}</div><h3><span class="opens">opens</span> {html.escape(opens)}</h3>'
                f'<p>{html.escape(note or "")}</p><div class="flags">{flag}</div></div></article>')

    nav = "".join(f'<a href="#{sid}">{html.escape(t)}</a>' for sid, t, _, _ in entries)
    secs = "".join(f'<section id="{sid}" aria-labelledby="h-{sid}"><header class="sec-head"><h2 id="h-{sid}">{html.escape(t)}</h2>'
                   f'<p>{html.escape(i)}</p></header><div class="grid">{"".join(card(*it) for it in items)}</div></section>'
                   for sid, t, i, items in entries)
    count = sum(len(items) for *_, items in entries)
    page = tpl.replace("{{NAV}}", nav).replace("{{SECTIONS}}", secs).replace("{{COUNT}}", str(count))
    out.write_text(page)
    print(f"page: {out} ({len(page) / 1e6:.2f} MB)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tour_dir", type=pathlib.Path)
    ap.add_argument("--page", type=pathlib.Path)
    args = ap.parse_args()

    clicks = scenario_clicks()
    for sid, *_ in SECTIONS:  # start clean so removed screens disappear
        shutil.rmtree(DOCS / sid, ignore_errors=True)
    entries, missing = [], []
    for sid, title, intro, items in sections():
        kept = []
        for screen, cl, opens, note, flags in items:
            src = find_png(args.tour_dir, screen)
            if not src:
                missing.append(screen)
                continue
            rel = f"{sid}/{screen}.webp"
            annotate(src, DOCS / rel, clicks.get(screen, []))
            kept.append((screen, cl, opens, note, flags, rel))
        if kept:
            entries.append((sid, title, intro, kept))
    write_readme(entries)
    print(f"{sum(len(e[3]) for e in entries)} screens written to {DOCS}")
    if missing:
        print("missing screenshots:", ", ".join(missing))
    if args.page:
        write_page(entries, args.page)


if __name__ == "__main__":
    main()
