# Assets

This document describes the current `swos-port` graphics asset pipeline. For the original SWOS file and
palette formats, see [`SWOS/sprites.txt`](SWOS/sprites.txt). For preparing replacement player artwork with
image generators, see [`player-sprite-ai-workflow.md`](player-sprite-ai-workflow.md).

## Source layout

Source artwork lives under `assets` and is committed to the repository:

```text
assets/
  sprites/
    menu/                       menu sprites and font glyphs
    game/                       fixed in-game sprites
      player/                   recolourable outfield-player layers
      goalkeeper/               recolourable goalkeeper layers
      bench/                    recolourable bench-player layers
      stadium/                  stadium components
  pitches/
    pitch1/ ... pitch6/         pitch tiles and layout matrices
  compileAssets.py              atlas and metadata generator
```

Sprite source files are PNG images named `sprNNNN.png`, where `NNNN` is the numeric sprite index used by the
game. A sprite with a non-zero centre/anchor has a matching `sprNNNN.txt`. The metadata file contains exactly
two decimal integers on separate lines: x centre followed by y centre, expressed in original-game pixels.

Source PNGs represent the 4K asset set. Their dimensions should be divisible by 12 so they map exactly to the
original coordinate system. The compiler produces these resolutions:

| Output | Coordinate multiplier | Scale from source |
|---|---:|---:|
| `4k` | 12 | 100% |
| `hd` | 6 | 50% |
| `low-res` | 3 | 25% |

The names refer to the game's asset tiers, not to a requirement that every individual image fill a particular
screen resolution.

## Fixed and variable sprites

Fixed menu and game sprites are packed as complete RGBA images. They are read from `sprites/menu` and PNGs in
the root of `sprites/game`.

Players, goalkeepers and bench players are assembled from aligned layers. The currently supported layer
directories are:

- player: `background`, `skin`, `hair`, `shirt`, `shorts`, `socks`;
- goalkeeper: `background`, `skin`, `hair`, `shorts`, `socks`;
- bench: `background`, `shirt`.

Every layer belonging to a pose must use the same canvas, sprite number and anchor. Background layers are not
trimmed or deduplicated because the game uses them as composition destinations. Rotation is disabled for all
variable layers.

The existing variable layers are neutral/grayscale artwork that the game colours and composites. Shirt layers
can encode two independently selectable kit colours. This avoids storing complete sprites for every combination
of team colours, skin, hair, shorts and socks.

For redesigned high-resolution players, retain this composable model. A useful authoring representation is:

- neutral RGBA base artwork containing outlines, lighting and material detail;
- semantic masks for skin, hair, primary shirt, secondary shirt, shorts and socks;
- overlays where the shape changes, such as shirt patterns, sleeve styles, hairstyles or goalkeeper equipment;
- shared canvas and anchor metadata for every pose and direction.

A neutral base need not be an indexed 8-bit image. Ordinary RGBA PNGs provide 256 levels per channel and full
alpha. Masks identify which material is recoloured; they do not limit the final rendered sprite to a 256-colour
palette. Colour ramps or shader-based tinting can preserve coloured shadows and highlights better than simple
grayscale multiplication.

Do not generate and store every finished kit/appearance combination. That produces a large, difficult-to-maintain
asset set and makes consistency across animation frames much harder.

## Fonts and menu constraints

Font glyphs are menu sprites and therefore follow the same `sprNNNN.png` numbering. Menus retain layout and
spacing inherited from the original game, so visible glyph bounds, bearings and advance widths matter. Empty
columns accidentally left inside a glyph can cause premature text ellipsis even when the artwork itself looks
correct.

Replacement fonts should be validated in actual menus, including narrow fields, editable-name cursors, numbers,
punctuation and the additional `ŠĐČĆŽ` characters. Reference-atlas and font-rendering utilities are located in
`assets` and write their inspection output below `tmp/assets`.

## Pitches

Each `assets/pitches/pitchN` directory contains numbered pitch-pattern PNGs and `pitchN.txt`. The text file is the
tile-index matrix used to reconstruct the pitch. The asset compiler packs the patterns without rotation and emits
the pitch layout and texture locations for all three resolutions.

Sprites 225 (the in-play match pitch diagram) and 226 (the edit-tactics pitch) are special: their atlas entries
contain dimensions and anchors, but the images themselves are drawn procedurally by the renderer. This avoids
placing two large, mostly empty pitch diagrams in an atlas.

## Compilation and generated files

Run the compiler directly from the repository root with:

```powershell
python assets\compileAssets.py
```

Python dependencies are listed in `assets/requirements.txt`. The patched PyTexturePacker used by the build is
vendored in `3rd-party/PyTexturePacker`.

The compiler packs textures up to 2048 x 2048 pixels with padding, creates scaled atlases, and writes generated
data under `tmp/assets`:

```text
tmp/assets/
  4k/
  hd/
  low-res/
  spriteDatabase.h
  variableSprites.h
  pitchDatabase.h
  stadiumSprites.h
  assets.stamp
```

The generated headers and atlases must not be edited manually. Change source PNGs, metadata or the compiler and
rebuild instead.

Visual Studio and Meson both integrate asset compilation incrementally. Assets are regenerated when an output is
missing or an asset/compiler input is newer than the outputs. If a changed source appears to remain up to date,
check its timestamp and the `assets` utility/custom target dependencies.

If the `SWOS_DIR` environment variable is defined, a successful asset build copies `4k`, `hd` and `low-res` into
`%SWOS_DIR%\assets`, overwriting files with the same names. If it is not defined, deployment is skipped.

## Validation checklist

After changing assets:

1. Run or build the asset compiler and confirm it reports success.
2. Check for invalid dimensions or missing sprite entries in its output.
3. Confirm the intended files were regenerated under `tmp/assets`.
4. Test all three asset resolutions when dimensions, anchors or trimming changed.
5. For recolourable sprites, test contrasting light, dark and saturated kit colours plus every skin/hair variant.
6. For fonts and menu artwork, test narrow menu fields and editable text, not only atlas previews.
7. Keep source files and metadata in version control; treat `tmp/assets` as generated output.

