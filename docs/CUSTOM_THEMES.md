<sub>[SkyEmu OMEGA](../README.md) › [Docs](README.md) › Custom themes</sub>

# Custom themes

The **SkyEmu Classic** design is drawn from a single PNG skin: menu bar, buttons, volume slider, touch
controls, screen bezels and five palette colors all come from one image. You can replace it with your own.

> [!NOTE]
> Custom skins apply to the classic design. The Material 3, Fluent and Adwaita designs draw their own widgets
> and colors, see [Design systems](DESIGN_SYSTEMS.md).

## Using a custom skin

1. **Menu → GUI → Design**: choose *SkyEmu Classic*.
2. **Theme**: choose *Custom*.
3. **Theme Path**: pick your `.png`. A path that fails to load is shown as `INVALID`.
4. Optionally pick a **Custom Font** (`.ttf`) and a **Font Scale** for the GUI text.

## Templates

Start from one of the templates in this folder. The `.pxd` files are the layered Pixelmator Pro sources.

| Template | Size | Files |
|---|---|---|
| Full | 5250 × 7400 | [`skyemu-theme-template.png`](skyemu-theme-template.png) · `skyemu-theme-template.pxd` |
| Mini | 3042 × 2835 | [`skyemu-theme-mini.png`](skyemu-theme-mini.png) · `skyemu-theme-mini.pxd` |
| Default skin | 3042 × 2835 | [`skyemu-theme-default.png`](skyemu-theme-default.png) · `skyemu-theme-default.pxd` |

SkyEmu recognizes the format by the image size, so keep the canvas exactly as it is.

<p align="center">
  <img src="skyemu-theme-default.png" width="520" alt="The default SkyEmu skin: palette, d-pad, buttons, menu bar and bezel regions">
</p>

The header row holds the **palette**, the theme **name** and the **author**, drawn as images. The five palette
swatches set the GUI colors:

| Swatch | Used for |
|---|---|
| 1 | Window and panel backgrounds |
| 2 | Text |
| 3 | Frames, buttons and borders |
| 4 | Title bars, headers, tabs and slider grabs |
| 5 | Accent: checkmarks and graphs |

A fully transparent swatch keeps SkyEmu's default for that role. The rest of the image contains one region per
widget and state (for example `A` and `A pressed`, the nine d-pad directions, the menu bar and the portrait and
landscape bezels). Regions left empty fall back to SkyEmu's built-in drawing.

## Control strips

Regions describe how they stretch with one pixel wide **control strips** along their top and left edges, 2
pixels outside the black outline of the region they control. Each color channel of a strip pixel encodes one
property; green and blue only matter for the bezels, which also place the game screen and the gamepad:

### Red: resize

| Value | Meaning |
|---|---|
| `#00xxxx` | Stretch |
| `#40xxxx` | Tile |
| `#80xxxx` | Fixed size |
| `#C0xxxx`, `#F0xxxx` | Reserved |

### Green: screen placement (bezels only)

| Value | Meaning |
|---|---|
| `#xx00xx` | Default |
| `#xx40xx` | Top screen cutout, the top screen is placed here |
| `#xx80xx` | Bottom screen cutout, empty outside DS mode |
| `#xxC0xx` | Both screens, when the theme does not lay out the DS screens itself |
| `#xxF0xx` | Reserved |

### Blue: gamepad placement (bezels only)

| Value | Meaning |
|---|---|
| `#xxxx00` | Default |
| `#xxxx40`, `#xxxx80`, `#xxxxC0`, `#xxxxF0` | Reserved for custom default button placements |
