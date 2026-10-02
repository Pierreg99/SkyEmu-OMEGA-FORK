<sub>[SkyEmu OMEGA](../README.md) › [Docs](README.md) › HTTP control server</sub>

# HTTP control server

SkyEmu contains a small web server with a REST-like API, so other programs and scripts can drive the emulator:
load games, read the screen, read and write memory, step frames, press buttons and change settings.

It is available in all native builds (not in the web build). Turn it on or off in **Menu → Advanced → Enable HTTP
Control Server** (or **Menu → Streaming**) and pick the port (8080 by default). It is on in new installations, and
every device on your network can reach it; turn it off on networks you don't trust. While you play in RetroAchievements
Hardcore Mode it only answers the commands that don't change the game: [`/achievements`](#achievements), the
[recording](#record--recording) commands, the [streams](#streammjpg--streamwav), the [pages](#remote--overlay) and
[`/input_state`](#input_state). Try it from a browser:

```
http://localhost:8080/ping
```

> [!TIP]
> For bots and automated testing, start SkyEmu headless. It then skips the GUI and runs as fast as possible
> instead of in real time:
>
> ```sh
> ./SkyEmu http_server <port> <path/to/rom>
> ```

## Commands

| Command | Purpose | Returns |
|---|---|---|
| [`/ping`](#ping) | Check that the server is up | `pong` |
| [`/status`](#status) | Emulator state and HTTP inputs | JSON |
| [`/load_rom`](#load_rom) | Load a game | `ok` |
| [`/load_patch`](#load_patch) | Add an IPS, UPS or BPS patch to the game | `ok` or the reason |
| [`/run`](#run) | Play at normal speed | `ok` |
| [`/step`](#step) | Advance a number of frames | `ok` |
| [`/screen`](#screen) | Screenshot of the emulated screen | PNG, JPG or BMP |
| [`/input`](#input) | Press or release inputs | `ok` |
| [`/read_byte`](#read_byte) | Read memory | Hex bytes |
| [`/write_byte`](#write_byte) | Write memory | `ok` |
| [`/save`](#save) · [`/load`](#load) | Save or load a save state file | `ok` / `failed` |
| [`/cheats`](#cheats) · [`/edit_cheat`](#edit_cheat) · [`/remove_cheat`](#remove_cheat) | Manage cheats | Text or JSON |
| [`/cheat_search`](#cheat_search) · [`/make_cheat`](#make_cheat) | Find values in memory and make codes for them | JSON |
| [`/achievements`](#achievements) | RetroAchievements user, game and achievements | JSON |
| [`/record`](#record--recording) · [`/recording`](#record--recording) | Start or stop recording a video or sound, recording state | JSON |
| [`/save_screenshot`](#save_screenshot--save_replay) · [`/save_replay`](#save_screenshot--save_replay) | Save a screenshot or the replay buffer | JSON |
| [`/stream.mjpg`](#streammjpg--streamwav) · [`/stream.wav`](#streammjpg--streamwav) | Live video and sound | MJPEG, WAV |
| [`/remote`](#remote--overlay) · [`/overlay`](#remote--overlay) | Remote Play page, overlay for streaming software | HTML |
| [`/input_state`](#input_state) | Console buttons held in the last frame | JSON |
| [`/settings`](#settings) | All settings | JSON |
| [`/setting`](#setting) | Change settings | `ok` |
| [`/show_ui`](#show_ui--hide_ui) · [`/hide_ui`](#show_ui--hide_ui) | Show or hide the GUI | Empty |
| [`/stretch_on`](#stretch_on--stretch_off) · [`/stretch_off`](#stretch_on--stretch_off) | Stretch the screen to the window | Empty |
| [`/load_html`](#load_html--indexhtml) · [`/index.html`](#load_html--indexhtml) | Serve a custom web page | HTML |
| [`/external_menu`](#external_menu) | Ask the host app to open its menu | Empty |

Parameters are passed as a query string: `http://localhost:<port>/<command>?<name>=<value>&...`.

---

### `/ping`

Returns `pong`. Use it to check that the server is running.

```
http://localhost:8080/ping
→ pong
```

When SkyEmu is [embedded](EMBEDDING.md), the host app is notified of every ping.

### `/status`

Returns JSON with the emulator state and the value of every input that the HTTP server holds. The `inputs` keys
are the valid names for [`/input`](#input).

```
http://localhost:8080/status
```

```json
{
  "emulator": "SkyEmu (6af0053049aa689a79ae653a949ed70e517ce2e1)",
  "run-mode": "RUN",
  "rom-loaded": true,
  "rom-path": "/Users/sky/roms/gba/varooom-3d.gba",
  "save-path": "/Users/sky/roms/gba/varooom-3d.sav",
  "patch": { "path": "/Users/sky/roms/gba/varooom-3d.ips", "applied": true, "status": "varooom-3d.ips (IPS)" },
  "rewind-info": { "entries-used": 953, "capacity": 1048576, "percent_full": 0.1 },
  "inputs": {
    "A": 0.000000,
    "B": 0.000000,
    "Up": 0.000000,
    "Start": 0.000000,
    "Capture State 0": 0.000000,
    "Toggle Full Screen": 0.000000
  }
}
```

<sub>The `inputs` object is shortened here, the real response lists every input. `patch` describes the
[ROM patch](CHEATS_AND_PATCHES.md#rom-patches) of the game; its `path` is empty when there is none.</sub>

### `/load_rom`

| Parameter | Description |
|---|---|
| `path` | Path of the ROM on the machine running SkyEmu |
| `pause` | `1` to load the game paused |

```
http://localhost:8080/load_rom?path=/tmp/rom.gba&pause=1
→ ok
```

### `/load_patch`

Adds an IPS, UPS or BPS patch to the running game, like **Add Patch** in the menu: it is copied next to the save
file under the ROM's name and the game restarts with it. A patch made for a different ROM is refused, not kept,
and the reason is returned. See [ROM patches](CHEATS_AND_PATCHES.md#rom-patches).

| Parameter | Description |
|---|---|
| `path` | Path of the patch on the machine running SkyEmu |

```
http://localhost:8080/load_patch?path=/tmp/translation.ips
→ ok

http://localhost:8080/load_patch?path=/tmp/other-version.bps
→ other-version.bps: The patch was made for a different ROM
```

### `/run`

Unpauses the emulator and runs it at 1× speed.

```
http://localhost:8080/run
→ ok
```

### `/step`

Advances the emulator and then pauses it.

| Parameter | Description |
|---|---|
| `frames` | Number of frames to emulate, 1 by default |

```
http://localhost:8080/step?frames=100
→ ok
```

### `/screen`

Returns an image of the emulated screen.

| Parameter | Description |
|---|---|
| `format` | `png` (default), `jpg` or `bmp` |
| `embed_state` | `1` to embed a save state in the PNG, like the files `/save` writes |

```
http://localhost:8080/screen?format=jpg
→ <jpg image>
```

### `/input`

Sets inputs to a value between `0` (released) and `1` (pressed). An input keeps its value until a later
`/input` changes it. Every input that has a keybind in the GUI can be set, several per request.

```
http://localhost:8080/input?A=1&Up=1     A and Up are pressed
http://localhost:8080/input?B=1&Up=0     A and B are pressed, Up is released
http://localhost:8080/input?A=0&B=0      nothing is pressed
```

Hotkeys work the same way. To capture save state slot 0:

```
http://localhost:8080/input?Capture State 0=1
http://localhost:8080/step
http://localhost:8080/input?Capture State 0=0
```

### `/read_byte`

Reads bytes from the emulated system's memory. Repeat `addr` to read several bytes; they are returned as one hex
string in request order.

| Parameter | Description |
|---|---|
| `addr` | Address in hex |
| `map` | Address map for the following `addr` parameters. `0` by default; on the DS `7` is the ARM7 and `0` or `9` the ARM9 map. Resets on every request. |

```
http://localhost:8080/read_byte?addr=02000004
→ f0

http://localhost:8080/read_byte?addr=02000004&map=7&addr=02000005&addr=02000006
→ f0bf01          map 0 at 0x02000004, then map 7 at 0x02000005 and 0x02000006
```

### `/write_byte`

Writes bytes. Each parameter is `<address>=<value>` in hex. `map` works as for [`/read_byte`](#read_byte).

```
http://localhost:8080/write_byte?02000000=ff&02000001=ee
→ ok             mem[0x02000000] = 0xff, mem[0x02000001] = 0xee
```

### `/save`

Writes a save state to `path` on the machine running SkyEmu.

```
http://localhost:8080/save?path=/tmp/save.png
→ ok
```

### `/load`

Restores the save state stored at `path`.

```
http://localhost:8080/load?path=/tmp/save.png
→ ok
```

### `/cheats`

Lists the cheats and whether they are enabled. `format=json` returns them as JSON.

```
http://localhost:8080/cheats
→ 0 - My first cheat: 12345678 AABBCCDD (enabled)
  1 - My second cheat: 12345678 90ABCDEF (disabled)

http://localhost:8080/cheats?format=json
→ [
    {"id": 0, "name": "My first cheat", "enabled": true, "code": "12345678 AABBCCDD"},
    {"id": 1, "name": "My second cheat", "enabled": false, "code": "12345678 90ABCDEF"}
  ]
```

### `/edit_cheat`

Adds or changes a cheat. All parameters are optional, but at least one besides `id` is required. There are 128
cheat slots. Changes are saved to the game's `.code` file, like changes made in the menu.

| Parameter | Description |
|---|---|
| `id` | Slot to change. Without it, the first free slot is used. |
| `name` | Name shown in the GUI |
| `code` | Action Replay code (GameShark for the Game Boy) |
| `enabled` | `1` (default) or `0` |

```
http://localhost:8080/edit_cheat?name=My cheat&code=12345678AABBCCDD&id=0&enabled=1
→ ok             0 - My cheat: 12345678 AABBCCDD (enabled)
```

### `/remove_cheat`

Removes the cheats with the given `id`s.

```
http://localhost:8080/remove_cheat?id=0&id=1
→ ok
```

### `/cheat_search`

The [cheat finder](CHEATS_AND_PATCHES.md#cheat-finder): searches the game's RAM for a value again and again while
it changes in the game, until only its address is left. Every request returns the state of the search and its
first results. The menu shows the same search.

| Parameter | Description |
|---|---|
| `start` | `1` starts a new search with every address |
| `size` | With `start`: value size in bytes, `1` (default), `2` or `4` |
| `signed` | With `start`: `1` for values that can be negative |
| `compare` | Keeps the addresses whose value is `equal`, `not_equal`, `greater` or `less` than `value`, `changed`, `unchanged`, `increased` or `decreased` since the last search, or `increased_by` / `decreased_by` exactly `value` |
| `value` | Number to compare with, decimal or `0x` hex |
| `reset` | `1` ends the search |
| `offset`, `count` | Which results to return, 50 from the first by default (256 at most) |

```
http://localhost:8080/cheat_search?start=1&size=2
http://localhost:8080/cheat_search?compare=increased_by&value=10
→ {"active": true, "value_size": 2, "signed": false, "searches": 1, "count": 1, "first": 0,
   "results": [{"address": "0x03000200", "value": 1030, "previous": 1030}]}
```

`value` is the value now, `previous` the value at the last search. Errors are returned as `{"error": "…"}`.

### `/make_cheat`

Adds an enabled code that keeps a value at an address, in the format of the console (encrypted Action Replay v3
for the GBA, Action Replay for the DS, GameShark for the Game Boy), and returns it.

| Parameter | Description |
|---|---|
| `address` | Address, decimal or `0x` hex. For Game Boy cartridge RAM, bits 16–23 are the RAM bank. |
| `value` | Value to keep |
| `size` | `1`, `2` or `4` bytes. The size of the current search by default. |
| `name` | Name of the code. The address and the value by default. |

```
http://localhost:8080/make_cheat?address=0x02000100&value=99&size=1&name=Infinite Lives
→ {"id": 0, "name": "Infinite Lives", "code": "69E24E1F 0BA154FB"}
```

### `/achievements`

Returns the [RetroAchievements](RETROACHIEVEMENTS.md) user, the modes, the game with its rich presence, and every
achievement with its unlock state, progress and rarity. It only reads, so it also answers in Hardcore Mode. See
[the example response](RETROACHIEVEMENTS.md#from-another-app).

```
http://localhost:8080/achievements
→ {"available": true, "logged_in": true, "user": {...}, "game": {...}, "summary": {...}, "achievements": [...]}
```

### `/record` · `/recording`

`/record` starts (`1`) or stops (`0`) recording a `video` (AVI) or `audio` (WAV), see
[Recording](RECORDING_AND_STREAMING.md#recording). Both return the recording state:

```
http://localhost:8080/record?video=1
http://localhost:8080/recording
→ {
    "video": {"recording": true, "file": "/roms/Game 2026-10-02 14-05-33.avi", "seconds": 12.4, "frames": 741,
              "bytes": 21233664, "width": 480, "height": 320},
    "audio": {"recording": false},
    "replay": {"seconds": 30, "held": 30.0},
    "last_file": "/roms/Game 2026-10-02 14-04-10.png",
    "message": "Recording video: Game 2026-10-02 14-05-33.avi",
    "error": false
  }
```

`seconds` is the length recorded so far (paused time does not count). After a recording stops, `last_file` is the
saved file and `message` says whether it worked.

### `/save_screenshot` · `/save_replay`

Save a PNG screenshot, or the [replay buffer](RECORDING_AND_STREAMING.md#replay-buffer) as a video, and return the
same JSON as `/recording` with the new file in `last_file`. `/save_replay` reports an error in `message` when the
replay buffer is off or empty.

### `/stream.mjpg` · `/stream.wav`

Endless live streams of the game: MJPEG video (`multipart/x-mixed-replace`, the frames as JPEG) and 48 kHz 16 bit
stereo WAV sound. Browsers show `/stream.mjpg` in an `<img>`, and VLC, ffmpeg and OBS Media Sources play both.
Frames are only encoded while someone watches, at most four of each stream can be open. Size and frame rate are
the `stream_scale` and `stream_fps` [settings](#setting). See
[Streaming](RECORDING_AND_STREAMING.md#streaming-and-remote-play).

### `/remote` · `/overlay`

`/remote` is the [Remote Play](RECORDING_AND_STREAMING.md#remote-play) page: the game with its sound and a touch,
keyboard and game controller input. `/overlay` is a transparent page for an OBS Browser Source with the buttons
held, the game and the REC badge ([options](RECORDING_AND_STREAMING.md#obs-and-other-streaming-software)).

### `/input_state`

The console buttons held in the last frame, whatever they came from (keyboard, controller, touch or `/input`):

```
http://localhost:8080/input_state
→ {"system": "GBA", "game": "Pokemon Emerald", "running": true,
   "inputs": {"A": 1, "B": 0, "X": 0, "Y": 0, "Up": 0, "Down": 0, "Left": 0, "Right": 1, "L": 0, "R": 0, "Start": 0, "Select": 0}}
```

### `/settings`

Returns every setting as JSON, including `screen_shader`, `design_system`, `color_scheme`, `contrast`,
`use_custom_accent`, `custom_accent`, `use_bundled_font`, `touch_controller`, `touch_controls_show_speed`,
`controller_face_layout`, `soft_patching`, `hardcore_mode`, `ra_unofficial`, `ra_spectator`, `record_scale`,
`record_format`, `record_audio`, `replay_seconds`, `screenshot_scale`, `stream_scale` and `stream_fps`.

```
http://localhost:8080/settings
```

### `/setting`

Changes one or more settings. It answers `ok` when a game is loaded and `Failed to load ROM` otherwise, but the
settings are applied either way.

| Parameter | Values |
|---|---|
| `design` | `0` platform native, `1` SkyEmu classic, `2` Material 3, `3` Fluent, `4` Adwaita ([details](DESIGN_SYSTEMS.md)) |
| `color_scheme` | `0` follow system, `1` light, `2` dark, `3` black |
| `contrast` | `0` follow system, `1` standard, `2` high ([details](DESIGN_SYSTEMS.md#high-contrast)) |
| `accent` | Accent color as hex `RRGGBB`, or `system` |
| `system_font` | `1` platform UI font, `0` bundled font |
| `theme` | Classic skin: `0` dark, `1` light, `2` black, `3` custom |
| `language` | Language code such as `en`, `de` or `ja` (locales like `de_DE` work too) |
| `volume` | `0.0` – `1.0` |
| `shader` | `0` pixelate, `1` bilinear, `2` LCD, `3` LCD & subpixels, `4` xBRZ, `5` CRT, `6` scanlines ([details](GRAPHICS.md#screen-shaders)) |
| `screen_rotation` | `0`–`3` for 0°, 90°, 180°, 270° |
| `integer_scaling`, `ghosting`, `color_correction` | Screen options |
| `gba_color_correction_mode` | `0` SkyEmu, `1` Higan |
| `nds_layout` | DS screen layout, `0` auto … `10` bottom screen only ([details](GRAPHICS.md#ds-screen-layouts)) |
| `nds_swap_screens` | `1` swaps the DS screens |
| `nds_screen_gap` | Space between the DS screens, `0`–`96` DS pixels |
| `nds_small_screen` | Size of the small DS screen, `25`–`100` percent |
| `gb_palette_0` … `gb_palette_3` | Game Boy palette colors |
| `force_dmg_mode` | Run Game Boy Color games as original Game Boy |
| `gui_scale_factor` | GUI scale |
| `menu`, `menu_bar` | Open the menu, keep the menu bar visible |
| `ui_type` | `DESKTOP`, `ANDROID`, `IOS` or `WEB` layout |
| `load_slot`, `capture_slot` | Restore or capture save state slot 0–3 |
| `touch_controller` | On-screen controller: `1` shown, `0` off ([details](CONTROLLERS.md)) |
| `touch_controls_show_speed` | `1` shows Rewind and Fast Forward on the on-screen controller |
| `touch_layout_editor` | `1` opens the on-screen controller layout editor, `0` closes it |
| `reset_touch_layout` | `1` restores the default portrait and landscape layouts |
| `controller_face_layout` | Game controller face buttons: `0` match the labels, `1` match the GBA positions |
| `soft_patching` | `1` applies [ROM patches](CHEATS_AND_PATCHES.md#rom-patches) when games load, `0` loads the original ROM |
| `hardcore_mode` | RetroAchievements [Hardcore Mode](RETROACHIEVEMENTS.md#modes) |
| `ra_unofficial` | `1` also loads unofficial RetroAchievements |
| `ra_spectator` | `1` turns on RetroAchievements spectator mode: unlocks are shown but not sent |
| `record_scale` | Video size, `1`–`4` times the console screen ([details](RECORDING_AND_STREAMING.md#files)) |
| `record_format` | Video quality: `0` high, `1` standard (smaller), `2` lossless |
| `record_audio` | `1` records sound in videos |
| `replay_seconds` | Replay buffer: `0` off, `15`, `30`, `60` or `120` |
| `screenshot_scale` | Screenshot size, `1`–`8` |
| `stream_scale`, `stream_fps` | Live video stream: size `1`–`3`, `60` or `30` frames per second |

Touch control, RetroAchievements and other options use their `/settings` names (for example
`touch_controls_opacity`, `draw_notifications`, `enable_download_cache`).

```
http://localhost:8080/setting?design=2&color_scheme=1&contrast=2&accent=3584e4
http://localhost:8080/setting?shader=5
```

### `/show_ui` · `/hide_ui`

Shows or hides the whole SkyEmu GUI, leaving only the game screen.

### `/stretch_on` · `/stretch_off`

Turns *Stretch Screen to Fit* on or off.

### `/load_html` · `/index.html`

`/load_html?path=<file>` loads an HTML file and returns it. Afterwards `/index.html` serves that page, which lets
a host ship a web remote control for SkyEmu. `pause=1` also pauses the emulator.

### `/external_menu`

Asks the [host app](EMBEDDING.md) to open its own menu (Android, iOS, macOS and the Windows DLL).
