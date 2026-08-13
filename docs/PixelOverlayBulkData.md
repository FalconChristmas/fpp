# Bulk pixel data for Pixel Overlay models

Two ways to move a whole frame of pixels in and out of an overlay model in a
single HTTP request, instead of one request per pixel:

| | |
|---|---|
| `PUT /api/overlays/model/{model}/data` | write raw pixels, an encoded image, or base64-in-JSON |
| `GET /api/overlays/model/{model}/data/raw` | read the buffer back as raw bytes |

Before this existed the only options were `PUT .../pixel` (one HTTP round trip
per pixel) or the shared-memory buffer, which needs `shm_open`/`mmap` and only
works from a process on the player itself. These endpoints work from anything
that can make an HTTP request, including a remote machine and `curl` in a shell
script.

## Quick start

```sh
# a 32x32 PNG, scaled to fit the model
curl -X PUT --data-binary @logo.png -H 'Content-Type: image/png' \
  "http://fpp/api/overlays/model/Matrix/data?autoEnable=true"

# raw RGB bytes, whole model
head -c $((32*32*3)) /dev/urandom |
  curl -X PUT --data-binary @- -H 'Content-Type: application/octet-stream' \
    "http://fpp/api/overlays/model/Matrix/data"

# read it back
curl -s "http://fpp/api/overlays/model/Matrix/data/raw" | xxd | head
```

## Body formats

FPP works out which of three shapes the body is:

**Raw pixel bytes** — `Content-Type: application/octet-stream` (or anything
unrecognised). Row-major from the top-left of the region, `w * h * fmt` bytes
exactly. A body of the wrong length is a `400` and writes nothing at all; the
error names the byte count it expected.

**An encoded image** — PNG, JPEG, GIF, BMP, TIFF or WebP, selected by
`Content-Type: image/*` or `?fmt=image`. Decoded and scaled on the player. A
body that is not the right length for the raw geometry but does start with an
image magic number is treated as an image too, so `--data-binary @foo.png` works
with no `Content-Type` at all.

Note the order: **length first, magic second**. Raw pixel data can legitimately
begin with any byte sequence — `FF D8 FF` is an ordinary reddish pixel, not
necessarily a JPEG — so sniffing is only ever consulted once the body has
already failed to match the raw geometry.

**JSON** — `Content-Type: application/json`, or a body whose first non-space
character is `{`:

```json
{"X": 0, "Y": 0, "W": 32, "H": 32, "Format": "rgb", "Data": "<base64>"}
```

`Format: "image"` makes `Data` a base64 encoded image. Members given in the JSON
override the equivalent query arguments.

## Query arguments

| arg | default | meaning |
|---|---|---|
| `x`, `y` | `0`, `0` | where the region's top-left corner lands. May be negative. |
| `w`, `h` | to the far edge | size of the region. For an image, the box it is scaled into. |
| `fmt` | `rgb` | `rgb`, `rgbw`, `mono` (one byte per pixel, replicated), or `image` |
| `blend` | `opaque` | `opaque`, `transparent` (skip zero bytes), `transparentrgb` (skip fully black pixels) |
| `scale` | `fit` | images only: `fit`, `fill`, `stretch`, `none` |
| `target` | `channel` | `channel` writes the output channel data; `overlay` composites into the mmapped buffer shared with external clients |
| `autoEnable` | *(off)* | `true`, `Enabled`, `Transparent`, `TransparentRGB` — enables the model only if it is currently Disabled |
| `stopEffect` | `true` | evict a running effect on the model first |

`blend` is deliberately not called `state`: `PUT .../state` sets the model's
*enable* state, which is a different thing.

### Clipping

A region that hangs off an edge draws the part that is visible; it is not
rejected. Only a region entirely outside the model is a `400`. This is what lets
you move something around by reposting it at changing offsets:

```sh
for x in $(seq -8 40); do
  curl -sX PUT --data-binary @sprite.rgb \
    "http://fpp/api/overlays/model/Matrix/data?x=$x&y=4&w=8&h=8&blend=transparent"
done
```

The response reports what happened, which is handy from a shell:

```json
{"Status":"OK","Message":"","Model":"Matrix","X":12,"Y":12,"W":8,"H":8,
 "Target":"channel","BytesPerPixel":3,"Clipped":true}
```

### Formats and model geometry

The model's own pixel size is 3 bytes (RGB) or 4 (RGBW), from its
`ChannelCountPerNode`. Conversion is automatic:

- `rgb` onto an RGBW model leaves the white channel at 0.
- `rgbw` onto an RGB model folds the white into R, G and B (clamped at 255),
  matching what `getOverlayPixelValue()` does, so white dims rather than
  vanishing.
- `mono` replicates its single byte to R, G and B.

`GET .../data/raw` returns `width * height * bytesPerPixel` bytes and repeats
the geometry in `X-FPP-Model-Width`, `X-FPP-Model-Height` and
`X-FPP-Model-BytesPerPixel` response headers, so a script does not have to fetch
the model definition separately.

## Why the binary form is worth it

Measured on a Pi 5, through Apache, against a 128x96 model (12,288 pixels,
36,864 bytes per frame):

| operation | rate | per request | fppd CPU |
|---|---|---|---|
| `PUT .../data` full frame, raw | 1380 frames/s | 0.72 ms | 37% |
| `PUT .../data` full frame, PNG | 1010 frames/s | 0.99 ms | 51% |
| `PUT .../data` 8x8 rect | 2023 req/s | 0.49 ms | 27% |
| `GET .../data/raw` full frame | 1694 frames/s | 0.59 ms | 34% |
| `GET .../data` full frame, JSON | 32 frames/s | 30.9 ms | 98% |
| `PUT .../pixel` | 2023 req/s | 0.49 ms | 27% |

A full frame is one bulk request against 12,288 per-pixel ones — about
8,400x fewer. On the read side the binary form is ~52x the JSON array's
throughput at a third of the CPU, because the JSON form spends 4-6 wire bytes
encoding each payload byte.

These are Pi 5 numbers. A BeagleBone has one core and far less memory; the
relative gain should hold but the absolute rates will be much lower.

## Notes and limits

**Request body limits.** `fppd` raises drogon's defaults (`src/httpAPI.cpp`):
the maximum body goes from 1MB to 32MB, and the in-memory limit from 64KB to
4MB. The second one matters more than it looks: below it, drogon buffers the
body in memory; above it, drogon spools the body to a temp file under
`./uploads/tmp/` relative to its working directory — the SD card. At the stock
64KB threshold every frame over about 150x150 would have hit the disk, which
would make this endpoint slower than the per-pixel API it replaces. Bodies
larger than 4MB still work, they just take that slower path.

**Tearing.** The blit runs on an HTTP thread while the channel output thread
reads the same buffer, so a frame can in principle be output half-updated. This
is not new — `.../pixel` and `.../fill` have always behaved this way — but it
is worth knowing if you are pushing frames at a high rate.

**Running effects.** By default a bulk write evicts any effect running on the
model, because an effect's next tick would overwrite what you just drew. Pass
`stopEffect=false` to draw over a running effect instead. This differs from
`.../pixel` and `.../fill`, which leave effects alone.

**Submodels.** These endpoints work on xLights submodels, including lazily
materialized ones. Note that *enabling* a Sub model concurrently with other
overlay API traffic can trip a pre-existing lock-order problem in FPP that is
unrelated to these endpoints; see the note in `plans/`.

**Compression.** Not supported on the request body. For photographic or flat
content, posting a PNG is usually a bigger win anyway — in the benchmark above
the same frame was 318x smaller as a PNG than as raw bytes.
