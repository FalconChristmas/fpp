# Core Infrastructure

## EPollManager (`src/EPollManager.h/cpp`)

Singleton wrapping epoll (Linux) or kqueue (macOS) for the main event loop. Manages file descriptor callbacks via `addFileDescriptor()`/`removeFileDescriptor()` and a single wakeup timer (timerfd on Linux, `EVFILT_TIMER` on macOS). The timer is armed via `armTimer(deadlineMS)` with an absolute `GetTimeMS()` deadline, which causes `waitForEvents()` to return precisely when the deadline is reached. `setTimerCallback()` registers the function invoked on timer expiry. Used by `Timers` to ensure timer deadlines wake the main loop without reducing `sleepms`.

## Timers (`src/Timers.h/cpp`)

Singleton timer system supporting one-shot (`addTimer`) and periodic (`addPeriodicTimer`) timers. Fires callbacks or command presets. `fireTimers()` is called each main loop iteration and also via the EPollManager timer callback for precise wakeup. When the next timer deadline changes in `updateTimers()`, it calls `EPollManager::INSTANCE.armTimer(nextTimer)` so epoll/kqueue wakes at exactly the right time. Used by GPIO debounce, eFuse retry, Twinkly token refresh, MQTT disconnect, FileMonitor, and command presets.

## MultiSync (`src/MultiSync.h/cpp`)

Master/slave synchronization across multiple FPP instances. UDP port 32320. Packet types: SYNC (playback sync with frame number), PING, FPPCOMMAND, BLANK, PLUGIN. Knows about FPP variants (Pi models, BBB variants) and Falcon controllers (F16v2-v5, F48v4-v5, etc.).

## Scheduler (`src/Scheduler.h/cpp`)

Schedule-based playback with day patterns (EVERYDAY, WEEKDAYS, WEEKEND, M_W_F, T_TH, custom bitmask). Priority-based scheduling with start/end times.

## Events (`src/Events.h/cpp`)

Publish/subscribe event system. `EventHandler` base with Publish/RegisterCallback. `EventNotifier` for frequency-based notifications.

## Player (`src/Player.h/cpp`)

Singleton managing playlist playback lifecycle. `StartPlaylist()`, `StopNow()`, `StopGracefully()`, `Pause()`, `Resume()`, `Process()`. HTTP resource for status queries.

## Sequence (`src/Sequence.h/cpp`)

FSEQ playback engine. `FPPD_MAX_CHANNELS = 8MB`. `OpenSequenceFile()`, `ProcessSequenceData()`, `SendSequenceData()`, `SeekSequenceFile()`. Bridge data support for E1.31/ArtNet input with expiry.

## FSEQ File Format (`src/fseq/`)

Frame-based sequence format for LED channel data.

| Version | Compression | Features |
| --- | --- | --- |
| V1 | None | Fixed header, raw sequential frames |
| V2 | zstd/zlib/none | Variable headers, sparse channel reading, frame offset indexing, extended blocks |
| V2E (ESEQ) | None | Special 'E' header, 50ms fixed step time |

Variable Headers: 2-byte codes for metadata. Common: `"mf"` = media filename (associated audio/video).

Key API: `FSEQFile::openFSEQFile(filename)`, `FSEQFile::createFSEQFile(filename, version, compression)`, `prepareRead(ranges, startFrame)`, `getFrame(frame)` -> `FrameData::readFrame()`, `addFrame()` / `finalize()`.

## MQTT (`src/mqtt.h/cpp`)

`MosquittoClient` wrapping mosquitto library. SSL/TLS support, message caching, QoS 0/1, retain flags, topic prefix hierarchy. MQTT command for playlist/schedule integration.

## Network Monitor (`src/NetworkMonitor.h/cpp`)

Linux netlink (NETLINK_ROUTE) for interface events (NEW_LINK, DEL_LINK, NEW_ADDR, DEL_ADDR). Callback registration. macOS: graceful no-op.

## Network Controller (`src/NetworkController.h/cpp`)

Detects and classifies network LED controllers: FPP, Falcon, SanDevices, ESPixelStick, Baldrick, AlphaPixel, HinksPixel, DIYLE, WLED.

## Output Monitor (`src/OutputMonitor.h/cpp`)

Physical port/pin management with eFuse protection, current monitoring, pixel count tracking, smart receiver callbacks, port grouping.

## Warnings (`src/Warnings.h/cpp`)

Warning system with timeouts, listener callbacks, persistence to file. Used for power supply warnings, hardware issues, etc.

Warnings have no expiry unless one is requested and cannot be dismissed from the UI, so
one that is never retracted persists until fppd restarts. `RemoveWarning()` matches the id
*and* the exact message text. See [WARNINGS.md](WARNINGS.md) before adding or changing a
warning.

## HTTP API (`src/httpAPI.h/cpp`)

RESTful API on port 32322 (localhost only). Player status, playlist control, effects, log levels, GPIO, output config, schedule, MultiSync stats, E1.31 byte counters.

## Logging (`src/log.h/cpp`)

Facility-based logging with per-module levels. Facilities: General, ChannelOut, ChannelData, Command, E131Bridge, Effect, MediaOut, Playlist, Schedule, Sequence, Settings, Control, Sync, Plugin, GPIO, HTTP. Levels: ERR, WARN, INFO, DEBUG, EXCESSIVE. Complex level strings: `"debug:schedule,player;excess:mqtt"`.
