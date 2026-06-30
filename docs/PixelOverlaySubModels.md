# xLights SubModel Support in Pixel Overlay Models

## Goal

Allow FPP to display xLights **submodels** under their parent model on the
Pixel Overlay Models page, and to run effects/sequences on a submodel through
the **existing core overlay commands** so that FPP itself and any plugin can
target a submodel by name with no special-case code.

This must cost nothing (memory or parse time) on systems that never use
submodels, which is the explicit constraint that drove the design.

## Background: how the data flows today

xLights FPP Connect already converts its internal `xlights_rgbeffects.xml`
into two digested files and pushes them to FPP:

| File | Produced by | Consumed by | Contents |
|------|-------------|-------------|----------|
| `config/model-overlays.json` | xLights FPP Connect | `fppd` (`PixelOverlayManager::loadModelMap`) | Top-level models: start channel, channel count, orientation, etc. |
| `config/virtualdisplaymap` | xLights FPP Connect | `pixeloverlaymodels.php` (preview) | Per-pixel `x,y,z,channel,channelCount,colorOrder,pixelSize`, listed in node order |

FPP never parses `xlights_rgbeffects.xml` directly. Neither digested file
carries submodels: `model-overlays.json` has zero `Sub` entries, while a real
`rgbeffects.xml` contains thousands of `<subModel>` definitions
(`type="ranges"`).

## Design summary

1. **FPP owns a new digested file**, `config/xlights-submodels.json`, in the
   same spirit as `model-overlays.json`. It is the contract.
2. **A bridge converter** (`scripts/genXlightsSubmodels.php`) parses an
   uploaded `xlights_rgbeffects.xml` plus `model-overlays.json` and writes
   `xlights-submodels.json`. This runs *off the daemon* (PHP / CLI), so the
   multi-MB XML is never parsed by `fppd`. Long term this conversion moves
   into xLights FPP Connect (see `xLights-SubModel-Export-Spec.md`); the
   converter is the bridge until then.
3. **Submodels are first-class `PixelOverlayModel`s** of `Type:"Sub"`, resolved
   by name through the single existing chokepoint
   `PixelOverlayManager::getModelLocked()`. Because every overlay command
   (`Overlay Model Effect`, `Overlay Model Fill`, `Overlay Model Clear`,
   `Overlay Model State`, `Overlay Model Text`) resolves its target through
   that function, submodel support reaches the entire command surface with no
   per-command changes. Plugins use the identical `getModel()` /
   `CommandManager::INSTANCE.run("Overlay Model Effect", ...)` entry points.
4. **Lazy load.** `fppd` does not touch `xlights-submodels.json` at boot. On
   the first reference to any submodel (a command, the C++ API, or a UI
   discovery request) it parses the file once into a lightweight in-memory
   index (`subModelConfigs`: name → compact config). The heavy
   `PixelOverlayModel` object (shared-memory buffers etc.) is materialized
   **per submodel, on demand** — only the submodels actually used become live
   objects. Systems that never reference a submodel never open the file.

## Why a submodel is a "grid-mode Sub"

The base `PixelOverlayModel` constructor, for any non-`Channel` type, builds a
dense `width x height` buffer with a `channelMap` (buffer cell -> channelData)
and allocates `channelData`. This gives effects, `setData`, `getDataJson`,
text, fill, the web pixel editor, and image export **for free**.

What the base class cannot do for a submodel is output: its `doOverlay()` does a
bulk `memcpy(channels[startChannel], channelData, channelCount)`, which assumes
the model owns a *contiguous* channel range. A submodel is a *sparse subset* of
its parent's nodes (e.g. only the top and bottom rows of a window), so a bulk
copy would clobber sibling pixels.

Therefore the grid-mode `PixelOverlayModelSub`:

- lets the base class build the dense `width x height` buffer + `channelMap` +
  `channelData` (reuse everything on the input/effects side), and
- overrides `doOverlay()` to **scatter** each grid cell's color from
  `channelData` to the parent's resolved absolute output channel, honoring the
  overlay state (Opaque / Transparent / TransparentRGB).

The scatter target for grid cell with parent node `n` is:

```
absoluteChannel0 = (ParentStartChannel - 1) + (n - 1) * ChannelCountPerNode
```

`ParentStartChannel` and `ChannelCountPerNode` are embedded in the digested
submodel entry by the converter (it knows them from `model-overlays.json`), so
the submodel renders without needing a live pointer to the parent. Node ->
channel is trivial arithmetic, **not** xLights layout geometry; the geometry
(which physical pixel a node is, zig-zag, start corner) only matters for the
preview, which already uses `virtualdisplaymap`.

## `config/xlights-submodels.json` schema

```jsonc
{
  "source": "xlights",
  "version": 1,
  "submodels": [
    {
      "Name": "Ally_Window_Panel_1_Horizontals", // unique, "/" -> "_" sanitized
      "DisplayName": "Horizontals",              // shown in the UI under parent
      "Type": "Sub",
      "SubType": "grid",
      "Parent": "Ally_Window_Panel_1",           // sanitized parent model name
      "ParentStartChannel": 24250,               // 1-based, parent node 1's channel
      "ChannelCountPerNode": 3,
      "Width": 11,                               // grid columns
      "Height": 27,                              // grid rows
      // Grid of PARENT node numbers, rows separated by ';', columns by ','.
      // An empty cell ("") is a hole (no pixel). Row count == Height,
      // column count == Width.
      "Grid": "1,72,71,...,63;,,,...,;27,28,...,37"
    }
  ]
}
```

Notes:
- The grid is the xLights `<subModel type="ranges">` `line0..lineN` data with
  node ranges expanded to one node per cell. `line0` is the bottom row in
  xLights buffer terms; the converter preserves xLights row order so the
  preview and effects orientation match what the designer saw.
- The grid is stored as a compact **string** (not a nested JSON array) so the
  in-memory index stays small (~hundreds of bytes per submodel).

## Lazy-load lifecycle

```
boot:                 nothing read. subModelIndexLoaded = false.
first submodel touch: loadSubModelIndex() parses xlights-submodels.json once,
                      fills subModelConfigs (name -> compact Json::Value),
                      drops the root DOM. subModelIndexLoaded = true.
getModelLocked(miss): if name in subModelConfigs and not yet in models,
                      addModel(subModelConfigs[name]) -> materialize one Sub.
discovery list:       ensure index loaded, list subModelConfigs keys without
                      materializing.
reload (loadModelMap):clears models, modelNames, subModelConfigs, and the
                      loaded flag so a re-upload is picked up.
```

## Phases

- **Phase 1 (this work, FPP side):** schema, bridge converter, grid-mode Sub,
  lazy load, `api/models?submodels=true` discovery, UI grouping. Works today
  with a raw `rgbeffects.xml` a user uploads.
- **Phase 2 (xLights side):** xLights FPP Connect emits
  `xlights-submodels.json` directly during upload, so the conversion happens on
  the desktop and `fppd` only ever consumes the digested file. See
  `xLights-SubModel-Export-Spec.md` for the spec handed to the xLights
  developers.

## Model groups (Tier B, Default world-position layout)

xLights `<modelGroup>` elements group models and submodels (referenced by
`Model/SubModel` path) so an effect can run across a symmetrical arrangement as
one coherent canvas. FPP has no native group concept; this adds it by reusing
the same grid-scatter machinery.

A group is rendered as a **combined "channel grid" buffer**:

- The bridge converter `scripts/genXlightsModelGroups.php` resolves each group's
  members (submodels, plain models, and nested groups) to their absolute
  channels, looks up each member pixel's **real-world coordinate** from
  `virtualdisplaymap`, and bins those pixels by world position into a 2D buffer.
  Buffer resolution scales with pixel count and is bounded for memory.
- Each buffer cell maps to **one or more** absolute channels (the member pixels
  binned into it). The cell is encoded as a `&`-separated list of 1-based start
  channels; rows are `;`-separated, columns `,`-separated, holes empty.
- Because the layout follows real-world position, an effect sweeping across the
  buffer sweeps across the group in physical space — the xLights "moving across
  the group" look, using data FPP already has (no xLights geometry reimplemented
  beyond binning the coordinates xLights already exported).

Runtime: a group is a `Type:"Sub"`, `SubType:"channelgrid"` model. It shares the
lazy index (`xlights-modelgroups.json` is loaded alongside the submodels file)
and the same on-demand materialization. Its `doOverlay` scatters each cell's
color to every channel in that cell. So groups are addressable by name through
every overlay command exactly like submodels and top-level models.

### `config/xlights-modelgroups.json` schema

```jsonc
{
  "source": "xlights",
  "version": 1,
  "modelgroups": [
    {
      "Name": "-Grp_-_Big_Rectangle_Ally_Window", // sanitized, "/"->"_"
      "DisplayName": "-Grp - Big Rectangle Ally Window",
      "Type": "Sub",
      "SubType": "channelgrid",
      "IsGroup": true,
      "ChannelCountPerNode": 3,        // groups render RGB; member W (RGBW) is left undriven
      "Width": 24,
      "Height": 14,
      "Orientation": "horizontal",
      "StartCorner": "TL",
      "StringCount": 14,
      "StrandsPerString": 1,
      "MemberCount": 6,
      "PixelCount": 142,
      // cell = '&'-separated 1-based absolute start channels; ';' rows, ',' cols, ""=hole
      "Grid": "24328,24331&24334,...;..."
    }
  ]
}
```

### Group limitations (Phase 1)

- One arrangement ("Default" by world position), not all ~10 xLights buffer
  styles. The exact look in xLights also depends on the per-effect buffer style
  chosen in the sequence, which is not in `rgbeffects.xml`; FPP provides a
  sensible spatial default.
- Binning can place multiple member pixels in one cell (they share that cell's
  color). Resolution scales with pixel count to keep this reasonable.
- Groups render as RGB; an RGBW member's white channel is not driven.

## Out of scope for Phase 1

- Playing a full FSEQ remapped onto a submodel's channels (a channel-remap
  feature distinct from overlay effects). Pushing frame data into the
  submodel's buffer via `setData` / the overlay buffer already works today.
- Non-`ranges` xLights submodel types (`subbuffer`); the current real-world
  file is 100% `type="ranges"`.
