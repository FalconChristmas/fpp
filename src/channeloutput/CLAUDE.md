# Channel Data Pipeline

This documents the per-frame data flow from FSEQ file to channel output plugins.

## Data Flow Overview

```
FSEQ File on Disk
       |
       v
ReadFramesLoop()              <-- background thread
  FSEQFile::getFrame()             decompresses zstd/zlib
  caches into frameCache
       |
       v
ReadSequenceData()            <-- channel output thread (channeloutputthread.cpp)
  data->readFrame()
  writes into m_seqData            (8MB mmap'd buffer, FPPD_MAX_CHANNELS)
       |
       v
ProcessSequenceData()
  1. Bridge/E1.31 merge        memcpy non-expired m_bridgeData ranges into m_seqData
  2. PluginManager::modifySequenceData()
  3. OverlayEffects()          legacy effects
  4. SDLOutput::ProcessVideoOverlay()
  5. PixelOverlayManager::doOverlays()
     - flush dirty buffers
     - sub-models (children first)
     - regular models (opaque/transparent blend)
  6. ChannelTester::OverlayTestData()
  7. PluginManager::modifyChannelData()
  8. PrepareChannelData()
     - OutputProcessors chain (in order):
         Remap -> SetValue -> Brightness+Gamma -> ColorOrder ->
         HoldValue -> ThreeToFour -> OverrideZero -> Fold ->
         ClampValue -> ScaleValue
     - Each output's PrepData()
       |
       v
SendSequenceData()
  Execute frame-triggered commands from FSEQ variable headers
  SendChannelData(m_seqData)
    for each ChannelOutput:
      output->SendData(m_seqData + startChannel)
       |
       v
  41 channel output plugins (UDPOutput, BBB48String, RGBMatrix, etc.)
```

## Overlay Model Effect Update and Buffer Flushing

### Two Buffers Per Model

Each `PixelOverlayModel` has two shared-memory buffers:

1. **Overlay Buffer** (`overlayBufferData`, shm `/FPP-Model-Overlay-Buffer-{name}`):
   Full RGB image (width * height * 3). Effects render into this. Dirty flag is bit 0 of
   `overlayBufferData->flags` (visible to external processes via shared memory).

2. **Channel Data Buffer** (`channelData`, shm `/FPP-Model-Data-{name}`):
   Sparse channel-mapped output. This is what `doOverlay()` copies into `m_seqData`.
   Dirty flag is `volatile bool dirtyBuffer`.

### Effect Update Thread

A dedicated background thread ("FPP-OverlayME") runs `doOverlayModelEffects()`:
- Created lazily on first effect via `addPeriodicUpdate()`
- Sleeps on a condition variable, wakes at precise ms deadlines
- Calls `model->updateRunningEffects()` which calls `RunningEffect::update()`

`RunningEffect::update()` return values:
- **> 0**: Reschedule update in N milliseconds
- **0** (`EFFECT_DONE`): Effect finished, delete it
- **-1** (`EFFECT_AFTER_NEXT_OUTPUT`): Defer next update to phase 5 of `doOverlays()`

### How Effects Write Data

Effects write to the overlay buffer, then flush to channel data:
```
effect->update()
  -> model->fillOverlayBuffer(r, g, b)   // write to overlay buffer
  -> model->flushOverlayBuffer()          // copy overlay -> channelData via channelMap
     -> sets dirtyBuffer = true
```

Some simple effects skip the overlay buffer and write `channelData` directly via
`model->setPixelValue()` or `model->fillData()`.

### doOverlays() Phases (called from ProcessSequenceData, step 5)

```
Phase 1: Flush externally-dirtied overlay buffers
         for each active model: if overlayBufferIsDirty() -> flushOverlayBuffer()

Phase 2: Apply sub-models (type "Sub") to m_seqData
         sub-models have X/Y offset within a parent

Phase 3: Apply non-sub models to m_seqData
         copy channelData -> m_seqData based on overlay state:
           Enabled(1)=opaque, Transparent(2)=non-zero only,
           TransparentRGB(3)=all-RGB-non-zero only

Phase 4: Apply activeRanges (channel range overwrites)

Phase 5: Process AFTER_NEXT_OUTPUT effects (after lock release)
         calls updateRunningEffects() for deferred models
```

### Effect Lifecycle Example (Color Fade)

```
T+0ms   setRunningEffect(fadeEffect, 1)  -> queued at T+1ms
T+1ms   background thread: update() -> fill + flush -> dirtyBuffer=true
        returns 25 -> rescheduled at T+26ms
T+26ms  background thread: update() -> recalculate fade -> flush
T+50ms  main loop: doOverlays() phase 1 checks dirty, phase 3 copies to m_seqData
        ...repeats until fade complete...
T+Nms   update() returns EFFECT_DONE(0) -> effect deleted
```

### Effect Synchronization

| Lock | Protects |
|------|----------|
| `effectLock` per model (recursive_mutex) | `runningEffect` pointer during update/replace |
| `threadLock` (mutex) | `updates` map, `afterOverlayModels` list |
| `activeModelsLock` (recursive_mutex) | `activeModels` list during doOverlays phases 1-4 |

## Key Details

- All transforms happen **in-place** on the single `m_seqData` buffer.
- Bridge/E1.31 input is merged first, before any overlays.
- Test data overlays after pixel overlays but before output processors.
- Output processors modify the buffer right before sending (final wire format).
- `ThreadedChannelOutput` subclasses double-buffer and send asynchronously; others send synchronously.

## Key Files

| File | Role |
|------|------|
| `../Sequence.cpp` | ReadSequenceData, ProcessSequenceData, SendSequenceData, bridge merge |
| `channeloutputthread.cpp` | Main output loop (50ms idle, 10ms playing) |
| `ChannelOutputSetup.cpp` | PrepareChannelData, SendChannelData, output initialization |
| `ChannelOutput.h` | Base class: Init, SendData, PrepData, GetRequiredChannelRanges |
| `ThreadedChannelOutput.h/cpp` | Async double-buffered output base class |
| `processors/OutputProcessor.h` | Output processor chain base |
| `../overlays/PixelOverlay.cpp` | PixelOverlayManager::doOverlays |
| `../channeltester/ChannelTester.cpp` | Test pattern overlay |

## Synchronization

| Lock | Protects |
|------|----------|
| `m_sequenceLock` (recursive_mutex) | FSEQ file operations |
| `frameCacheLock` (mutex) | Frame cache between read and output threads |
| `m_bridgeRangesLock` (mutex) | Bridge data ranges and expiry |
| `outputProcessors.processorsLock` (mutex) | Processor list during execution |
| `activeModelsLock` (recursive_mutex) | Active overlay model list |
