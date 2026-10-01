<sub>[SkyEmu OMEGA](../README.md) › [Docs](README.md) › HTTP control server</sub>

# HTTP control server

SkyEmu contains a small web server with a REST-like API, so other programs and scripts can drive the emulator:
load games, read the screen, read and write memory, step frames, press buttons and change settings.

It is available in all native builds (not in the web build). Enable it in **Menu → Advanced → Enable HTTP
Control Server** and pick the port (8080 by default). Try it from a browser:

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
| [`/run`](#run) | Play at normal speed | `ok` |
| [`/step`](#step) | Advance a number of frames | `ok` |
| [`/screen`](#screen) | Screenshot of the emulated screen | PNG, JPG or BMP |
| [`/input`](#input) | Press or release inputs | `ok` |
| [`/read_byte`](#read_byte) | Read memory | Hex bytes |
| [`/write_byte`](#write_byte) | Write memory | `ok` |
| [`/save`](#save) · [`/load`](#load) | Save or load a save state file | `ok` / `failed` |
| [`/cheats`](#cheats) · [`/edit_cheat`](#edit_cheat) · [`/remove_cheat`](#remove_cheat) | Manage cheats | Text |
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

<sub>The `inputs` object is shortened here, the real response lists every input.</sub>

### `/load_rom`

| Parameter | Description |
|---|---|
| `path` | Path of the ROM on the machine running SkyEmu |
| `pause` | `1` to load the game paused |

```
http://localhost:8080/load_rom?path=/tmp/rom.gba&pause=1
→ ok
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

Lists the cheats and whether they are enabled.

```
http://localhost:8080/cheats
→ 0 - My first cheat: 12345678 AABBCCDD (enabled)
  1 - My second cheat: 12345678 90ABCDEF (disabled)
```

### `/edit_cheat`

Adds or changes a cheat. All parameters are optional, but at least one besides `id` is required. At least 32
cheat slots are available.

| Parameter | Description |
|---|---|
| `id` | Slot to change. Without it, the first free slot is used. |
| `name` | Name shown in the GUI |
| `code` | Action Replay code |
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

### `/settings`

Returns every setting as JSON, including `screen_shader`, `design_system`, `color_scheme`, `contrast`,
`use_custom_accent`, `custom_accent` and `use_bundled_font`.

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
| `nds_layout` | DS screen layout, `0` auto |
| `gb_palette_0` … `gb_palette_3` | Game Boy palette colors |
| `force_dmg_mode` | Run Game Boy Color games as original Game Boy |
| `gui_scale_factor` | GUI scale |
| `menu`, `menu_bar` | Open the menu, keep the menu bar visible |
| `ui_type` | `DESKTOP`, `ANDROID`, `IOS` or `WEB` layout |
| `load_slot`, `capture_slot` | Restore or capture save state slot 0–3 |

Touch control, RetroAchievements and other options use their `/settings` names (for example
`touch_controls_opacity`, `hardcore_mode`, `enable_download_cache`).

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
