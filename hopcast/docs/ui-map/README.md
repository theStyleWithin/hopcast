# HopCast UI map

Every button and menu in HopCast and what it opens, captured by the automated UI tour
(`hopcast/scripts/ui-tour.sh`, scenarios in `hopcast/tests/ui-tour.txt`) on a virtual
1600×900 screen without a window manager. Pink rings mark where the tour clicked; numbers give
the order. Regenerate with `hopcast/scripts/ui-map.py <tour-dir>`.

Sections: [Opening HopCast](#start) · [Channels and stream keys](#channels) · [Menu bar](#menus) · [Scenes and sources](#scenes) · [Settings](#settings) · [Right sidebar](#sidebar) · [Bottom bar](#bottom)

<a id="start"></a>
## Opening HopCast

What you see the moment the app starts.

### The studio, straight away *(HopCast change)*

**Click:** `Open HopCast`

No sign-in screen. The channel bar holds only Custom RTMP. The sidebar on the right has studio mode, dual output, music, draw pen, virtual camera, settings and notifications.

![The studio, straight away](start/main.webp)

<a id="channels"></a>
## Channels and stream keys

Adding a Custom RTMP channel, from the first click to where the key is stored.

### Channel menu

**Click:** `My Channel ⋮`

Add channels · Reset live info.

![Channel menu](channels/my-channel-menu.webp)

### Custom Channel form *(HopCast change)*

**Click:** `Custom RTMP`

URL, Stream Key and Stream Name. “Auto-paste stream key only” is on by default. No links to PRISM’s output guide or help pages.

![Custom Channel form](channels/custom-rtmp-form.webp)

### Connection type *(HopCast change)*

**Click:** `Custom RTMP` › `URL dropdown`

Only User Input (RTMP), (SRT) and (RIST). No list of platforms.

![Connection type](channels/custom-rtmp.webp)

### Filled-in form

**Click:** `Custom RTMP` › `Fill in the form`

Example values: rtmp://live.example.com/app and a made-up key. RTMP also offers optional ID and password fields.

![Filled-in form](channels/custom-rtmp-filled.webp)

### Where to keep your stream keys *(HopCast change)*

**Click:** `Save (first stream key)`

Asked once: Choose Folder… or Use HopCast’s Folder. The unlock key always stays in ~/.local/share/hopcast.

![Where to keep your stream keys](channels/storage-prompt.webp)

### Folder picker *(HopCast change)*

**Click:** `Choose Folder…`

Pick any folder; it starts in Documents. In this test the picker was cancelled, so HopCast’s own folder was used.

![Folder picker](channels/folder-picker.webp)

### Channel in the bar

**Click:** `After saving`

The saved channel appears next to My Channel. It reads “Custom RT…” here because the name field already said “Custom RTMP”.

![Channel in the bar](channels/after-channel-added.webp)

### Channel controls

**Click:** `Click the saved channel`

An on/off switch for including it when you go live, a share button and a ⋮ menu.

![Channel controls](channels/channel-in-bar-menu.webp)

### Stream key file location *(HopCast change)*

**Click:** `Tools` › `Stream Key Storage…`

Shows where the encrypted file is and offers to move it: Choose Folder… or Close.

![Stream key file location](channels/tools-stream-key-storage.webp)

<a id="menus"></a>
## Menu bar

The eight menus under the channel bar.

### Settings menu

**Click:** `Settings`

Settings · Show Settings Folder · Exit (Ctrl+Q) · Show Videos Folder · Remux Recordings.

![Settings menu](menus/menu-settings.webp)

### Scene Collection menu

**Click:** `Scene Collection`

New · Export · Import · Scene Set Management.

![Scene Collection menu](menus/menu-scene-collection.webp)

### Edit Source menu

**Click:** `Edit Source`

Undo/Redo, Copy, Paste (Reference/Duplicate), Copy/Paste Filters, Transform, Order, Preview Scaling, Lock Preview, Advanced Audio Properties.

![Edit Source menu](menus/menu-edit-source.webp)

### View menu

**Click:** `View`

Reset UI · Fullscreen Interface (F11) · Source Toolbar · Source Icons · Stats · Studio Mode · Multiview · Always On Top.

![View menu](menus/menu-view.webp)

### Docks menu

**Click:** `Docks`

Lock Docks · Full-Height Docks · Reset Docks, plus show/hide for Scenes, Sources, Audio Mixer and Music Playlist.

![Docks menu](menus/menu-docks.webp)

### Profile menu

**Click:** `Profile`

New · Export · Import, and the current profile (Untitled).

![Profile menu](menus/menu-profile.webp)

### Tools menu *(HopCast change)*

**Click:** `Tools`

Stream Key Storage… is the only entry.

![Tools menu](menus/menu-tools.webp)

### Help menu *(HopCast change)*

**Click:** `Help`

Program Info · Open Source License. No links to outside websites.

![Help menu](menus/menu-help.webp)

### About HopCast *(HopCast change)*

**Click:** `Help` › `Program Info`

Name, version and the GPL notice crediting PRISM Live Studio and OBS Studio. No update check.

![About HopCast](menus/help-program-info.webp)

<a id="scenes"></a>
## Scenes and sources

The two panels under the preview.

### New scene

**Click:** `Scenes +`

“Please enter the name of the scene”, with OK and Cancel.

![New scene](scenes/scenes-add.webp)

### Scene transition

**Click:** `Transition button`

Switch to scene: pick the transition (Cut).

![Scene transition](scenes/scenes-transition.webp)

### Scenes menu

**Click:** `Scenes ⋮`

Detach · Export scene set · Display Method.

![Scenes menu](scenes/scenes-more.webp)

### Add Source window *(HopCast change)*

**Click:** `Sources +`

Search, Video Capture Device (V4L2), Audio Input/Output Capture (PulseAudio), Image, Image Slide Show, Media and more. A plain hint until you pick one.

![Add Source window](scenes/sources-add.webp)

### Sources menu

**Click:** `Sources ⋮`

Detach.

![Sources menu](scenes/sources-more.webp)

<a id="settings"></a>
## Settings

The gear next to “1280x720 · 30fps” opens Settings on the Video page.

### Video

**Click:** `Gear`

Base canvas 1600x900, output 1280x720, downscale filter, 30 FPS, dual output.

![Video](settings/settings-video.webp)

### General

**Click:** `Gear` › `General`

Language, recording options, source snapping, projectors.

![General](settings/settings-general.webp)

### Output

**Click:** `Gear` › `Output`

Simple mode: 2500 kbps video, 160 kbps audio, software x264, encoder preset, recording.

![Output](settings/settings-output.webp)

### Audio

**Click:** `Gear` › `Audio`

48 kHz stereo, desktop and mic/aux devices, meters.

![Audio](settings/settings-audio.webp)

### Hotkeys

**Click:** `Gear` › `Hotkeys`

Start/stop streaming and recording, pause, replay buffer, chapter markers.

![Hotkeys](settings/settings-hotkeys.webp)

### Accessibility

**Click:** `Gear` › `Accessibility`

Colour presets for source borders and mixer meters.

![Accessibility](settings/settings-accessibility.webp)

### Advanced

**Click:** `Gear` › `Advanced`

Colour format NV12 / Rec. 709, HDR levels, recording file names, replay buffer.

![Advanced](settings/settings-advanced.webp)

<a id="sidebar"></a>
## Right sidebar

The column of buttons on the right edge.

### Studio mode

**Click:** `Studio mode button`

Splits the preview into Edit and Live. It stays on in the following screenshots.

![Studio mode](sidebar/sidebar-studio-mode.webp)

### Dual output

**Click:** `Dual output button`

Streams a horizontal and a vertical layout at once. With studio mode on it first asks: “Dual output is not available in the studio mode. Turn off the studio mode?”

![Dual output](sidebar/sidebar-dual-output.webp)

### Music playlist

**Click:** `Music button`

Player controls and an Add Source button for a music playlist source.

![Music playlist](sidebar/sidebar-music.webp)

### Draw pen

**Click:** `Draw pen button`

A drawing toolbar appears under the preview (pens, shapes, colours, undo, clear).

![Draw pen](sidebar/sidebar-draw-pen.webp)

### Virtual camera

**Click:** `Virtual camera button`

Starts the virtual camera; with an empty scene it first asks whether to continue.

![Virtual camera](sidebar/sidebar-virtual-camera.webp)

### Settings

**Click:** `Settings (gear)`

Opens Settings on the General page.

![Settings](sidebar/sidebar-settings.webp)

### Notifications

**Click:** `Bell`

“No Alert.”

![Notifications](sidebar/notifications.webp)

<a id="bottom"></a>
## Bottom bar

Going live and recording.

### Performance panel

**Click:** `CPU ^`

CPU and memory, frame rate, dropped frames, stream and recording stats, free disk space.

![Performance panel](bottom/performance.webp)

### Empty-scene check

**Click:** `Go Live`

“It looks like you haven’t added any video sources yet…” (the test scene was empty). With no channel switched on it says “You must select a channel first.”

![Empty-scene check](bottom/go-live.webp)

### Same check before recording

**Click:** `Rec`

Yes starts recording anyway; No goes back.

![Same check before recording](bottom/record.webp)
