# Spec / Prompt for xLights: native `xlights-submodels.json` export to FPP

This document is intended to be handed to the **xLights** developers. It
specifies a new file that xLights FPP Connect should upload to FPP so that FPP
can display submodels under their parent model and run effects on a submodel
through its standard overlay commands.

It is written so it can be used directly as an implementation prompt.

---

## Background / why

FPP already consumes two digested files that xLights FPP Connect produces from
`xlights_rgbeffects.xml` during an upload:

- `model-overlays.json` — top-level model channel definitions.
- `virtualdisplaymap` — per-pixel `x,y,z,channel,...` for the model preview.

Neither carries **submodels**. FPP has now implemented submodel support on its
side (see `docs/PixelOverlaySubModels.md`) and, as a bridge, ships a PHP
converter (`scripts/genXlightsSubmodels.php`) that parses a raw uploaded
`rgbeffects.xml` into a digested `config/xlights-submodels.json`.

We would like xLights FPP Connect to **produce `xlights-submodels.json`
directly** and upload it alongside `model-overlays.json`, so that:

1. The multi-MB `rgbeffects.xml` never has to be parsed on the FPP device
   (often a Pi Zero / BeagleBone with little RAM/CPU).
2. xLights remains the single source of truth for node ordering / numbering,
   rather than FPP reimplementing xLights layout logic.

FPP already accepts this exact file format today (the converter is the reference
implementation). xLights producing it natively is a drop-in replacement for the
bridge converter.

---

## What to produce

One JSON file, uploaded to `<FPP media>/config/xlights-submodels.json`, using
the same FPP Connect upload mechanism already used for `model-overlays.json`.

### Top-level structure

```jsonc
{
  "source": "xlights",
  "version": 1,
  "submodels": [ /* one entry per submodel of every model being uploaded */ ]
}
```

### Per-submodel entry

```jsonc
{
  "Name": "Ally_Window_Panel_1_Horizontals",
  "DisplayName": "Horizontals",
  "Type": "Sub",
  "SubType": "grid",
  "Parent": "Ally_Window_Panel_1",
  "ParentStartChannel": 24250,
  "ChannelCountPerNode": 3,
  "Width": 11,
  "Height": 27,
  "Orientation": "horizontal",
  "StartCorner": "TL",
  "StringCount": 27,
  "StrandsPerString": 1,
  "Grid": "1,72,71,70,69,68,67,66,65,64,63;;...;27,28,29,30,31,32,33,34,35,36,37"
}
```

### Field-by-field

| Field | Type | Meaning / how to derive |
|-------|------|--------------------------|
| `Name` | string | Globally unique submodel id. Use exactly `sanitize(Parent) + "_" + sanitize(subModelName)`. `sanitize()` = the **same** name sanitization xLights FPP Connect applies to model names for `model-overlays.json` (spaces → `_`, `/` → `_`). FPP additionally replaces `/`→`_` on load, so avoid `/`. |
| `DisplayName` | string | The raw xLights submodel name, for UI display under the parent. |
| `Type` | string | Always the literal `"Sub"`. |
| `SubType` | string | Always the literal `"grid"` for `type="ranges"` submodels (see below). |
| `Parent` | string | The parent model's **sanitized** name — must match the `Name` of the parent entry in `model-overlays.json`. |
| `ParentStartChannel` | int | The parent model's 1-based start channel (the channel of the parent's node 1). Same value as the parent's `StartChannel` in `model-overlays.json`. |
| `ChannelCountPerNode` | int | The parent model's channels-per-node (3 for RGB, 4 for RGBW). Same as the parent's `ChannelCountPerNode`. |
| `Width` | int | Number of columns in the grid (see below). |
| `Height` | int | Number of rows in the grid. |
| `Orientation` | string | Always `"horizontal"`. |
| `StartCorner` | string | Always `"TL"`. |
| `StringCount` | int | Set equal to `Height`. |
| `StrandsPerString` | int | Always `1`. |
| `Grid` | string | The submodel buffer as a grid of **parent node numbers**. See encoding below. |

> `ParentStartChannel` + `ChannelCountPerNode` let FPP resolve each node to an
> absolute channel as `(ParentStartChannel - 1) + (node - 1) * ChannelCountPerNode`.
> This is the only channel math FPP does; everything else (which node is which
> physical pixel) stays in xLights.

---

## The `Grid` encoding (the important part)

This is the digested form of an xLights `<subModel type="ranges">` element's
`line0..lineN` attributes.

xLights stores a submodel's buffer as rows `line0, line1, ... line(H-1)` where
each `lineN` is a comma-separated list of cells, and each cell is either:

- empty (a hole — no pixel there),
- a single node number (e.g. `27`), or
- an inclusive node range (e.g. `1-27` meaning `1,2,3,...,27`, or `27-1` meaning
  `27,26,...,1`).

`Grid` is produced by:

1. For each `lineN`, split on `,`; expand every cell token (single value as-is,
   `a-b` expanded to the inclusive ascending/descending sequence, empty → hole).
   The result is that line's row of node numbers (holes included).
2. `Width` = the maximum expanded row length across all lines. Pad every row on
   the right with holes to `Width`.
3. **Row order:** xLights `line0` is the *bottom* buffer row. Reverse the line
   order so the first row of `Grid` is the visual *top* (FPP buffer row 0 is the
   top). `Height` = number of lines.
4. Serialize: rows joined by `;`, cells within a row joined by `,`, a hole
   encoded as an empty string.

So a 27×11 vertical-edge submodel whose only populated columns are the first and
last becomes rows like `27,,,,,,,,,,37`.

> Trailing empty cells in a row may be omitted (FPP treats a short row as
> hole-padded to `Width`), but emitting the full padded row is preferred for
> clarity.

### Reference implementation

`scripts/genXlightsSubmodels.php` in the FPP repo implements exactly this
transformation (functions `expandToken`, `expandLine`, and the grid build loop).
It is the authoritative reference for byte-for-byte compatibility; xLights output
should match what that script produces from the same `rgbeffects.xml`.

---

## Scope / phasing

- **Phase 1 (required):** support `type="ranges"` submodels — these are
  effectively 100% of real-world submodels today. Emit `SubType: "grid"` as
  above.
- **Phase 2 (optional, later):** `type="subbuffer"` submodels. Define a second
  `SubType` and coordinate the schema with the FPP team before implementing;
  FPP does not yet consume it.

## Notes for the xLights side

- Emit submodels for **every** model included in the FPP Connect upload set, so
  FPP's parent lookup (`Parent` → `model-overlays.json` `Name`) always resolves.
- A submodel whose parent is not in the upload set should be skipped (FPP will
  skip it too, but better not to ship dangling entries).
- The file can be large (thousands of entries) but that is fine: FPP loads it
  **lazily** and only when a submodel is actually referenced, so there is no
  device-side cost for users who do not use submodels. Keeping the `Grid` as a
  compact string (not nested arrays) keeps both the file and FPP's in-memory
  index small — please retain the string encoding.
- Re-uploading replaces the file wholesale; FPP drops and rebuilds its submodel
  index on reload, so no merge semantics are needed on the xLights side.

---

## Companion file: `xlights-modelgroups.json` (model groups)

FPP also supports xLights `<modelGroup>`s, rendered as a single combined buffer
an effect runs across (the "moving across a symmetrical group" look). The bridge
converter `scripts/genXlightsModelGroups.php` produces `xlights-modelgroups.json`
today; xLights producing it natively is the Phase 2 goal here too.

### What to produce

```jsonc
{
  "source": "xlights",
  "version": 1,
  "modelgroups": [
    {
      "Name": "-Grp_-_Big_Rectangle_Ally_Window", // sanitize(group name)
      "DisplayName": "-Grp - Big Rectangle Ally Window",
      "Type": "Sub",
      "SubType": "channelgrid",
      "IsGroup": true,
      "ChannelCountPerNode": 3,
      "Width": 24,
      "Height": 14,
      "Orientation": "horizontal",
      "StartCorner": "TL",
      "StringCount": 14,            // == Height
      "StrandsPerString": 1,
      "MemberCount": 6,
      "PixelCount": 142,
      "Grid": "24328,24331&24334,...;..."
    }
  ]
}
```

### The `Grid` for a group

Same row/column encoding as a submodel, but each **cell** is a `&`-separated
list of **1-based absolute start channels** (not parent node numbers). A cell
lists every member pixel binned into it; FPP scatters that cell's color to all
of them. Empty cell = hole.

To build it (reference: `genXlightsModelGroups.php`):

1. Resolve every member of the group to concrete channels. Members are
   `Model/SubModel` (use the submodel's nodes), plain `Model` (use all the
   model's nodes), or a nested group name (resolve recursively, guard cycles).
2. For each member pixel, take its **world X/Y** (xLights already has this — it
   is what `virtualdisplaymap` encodes) and its absolute start channel.
3. Bin pixels into a `Width x Height` grid by world position (higher world Y =
   top row). Choose a resolution that scales with pixel count and is bounded for
   memory. Multiple pixels may land in one cell — list all their channels in
   that cell.

> xLights is far better placed than FPP to emit the *exact* per-buffer-style
> combined layout (Default, Per-Model, Overlay-Centered, etc.). If you export
> the concrete buffer for a chosen style, emit it in this same `Grid` form and
> FPP will render it as-is. Phase 1 in FPP only implements the Default
> world-position arrangement.

Group notes:
- Groups render RGB; if a member is RGBW, mapping its R/G/B start channel is
  fine (the W channel is simply not driven).
- Skip groups that resolve to zero pixels.

---

## How FPP consumes it (for context)

- On the first reference to any submodel (an overlay command, the C++
  `getModel()` API, or a UI discovery request), FPP parses this file once into a
  name→config index, then materializes individual submodels on demand.
- A materialized submodel is a first-class overlay model of `Type:"Sub"`. It is
  addressable by `Name` through every existing overlay command
  (`Overlay Model Effect`, `Overlay Model Fill`, `Overlay Model Clear`,
  `Overlay Model State`, `Overlay Model Text`) and via the REST API at
  `api/overlays/model/<Name>`.
- FPP renders a submodel by scattering its buffer onto only the parent channels
  the grid references, leaving sibling pixels untouched.
