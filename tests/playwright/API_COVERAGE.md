# API v2 Test Coverage

This document is the source of truth for route coverage classification. The `check-api-coverage` script diffs this file against the router to detect gaps.

## Tier Legend

| Tier | CI | Manual (container) | Manual (hardware) | Description |
| --- | --- | --- | --- | --- |
| FULL | ✓ | ✓ | ✓ | Deterministic — structure and values assertable |
| SCHEMA | ✓ | ✓ | ✓ | Shape assertable; values are runtime-dependent |
| STATE | — | ✓ | ✓ | Test owns its setup/teardown |
| HARDWARE | — | — | ✓ | Requires specific physical hardware |
| DESTRUCTIVE | — | opt-in | opt-in | Kills fppd, reboots, or shuts down the device |

## Hardware Tag Reference

| Tag | Required hardware |
| --- | --- |
| `@hardware:cape` | Physical cape with EEPROM (Pi/BBB only) |
| `@hardware:wifi` | WiFi network interface |
| `@hardware:audio` | Audio device (sound card, USB audio) |
| `@hardware:pipewire` | PipeWire daemon running with connected audio/video hardware |

## Routes

| Method | Path | Tier | Tag | Notes |
| --- | --- | --- | --- | --- |
| GET | /backups/list | SCHEMA | | Returns array of available backups |
| GET | /backups/list/:DeviceName | STATE | @state | Requires mounted external device |
| GET | /backups/devices | SCHEMA | | Returns array of backup devices |
| POST | /backups/devices/mount/:DeviceName/:MountLocation | STATE | @state | Mounts a device |
| POST | /backups/devices/unmount/:DeviceName/:MountLocation | STATE | @state | Unmounts a device |
| POST | /backups/configuration | STATE | @state | Creates a backup; cleanup: delete it |
| GET | /backups/configuration/list | SCHEMA | | Returns array of backups |
| GET | /backups/configuration/list/:DeviceName | STATE | @state | Requires mounted device |
| POST | /backups/configuration/restore/:Directory/:BackupFilename | STATE | @state | Requires backup to exist first |
| GET | /backups/configuration/:Directory/:BackupFilename | STATE | @state | Requires backup to exist |
| DELETE | /backups/configuration/:Directory/:BackupFilename | STATE | @state | Self-contained with create first |
| GET | /cape | HARDWARE | @hardware:cape | Requires physical cape |
| POST | /cape/eeprom/voucher | HARDWARE | @hardware:cape | Requires physical cape |
| POST | /cape/eeprom/sign/:key/:order | HARDWARE | @hardware:cape | Requires physical cape |
| GET | /cape/eeprom/signingData/:key/:order | HARDWARE | @hardware:cape | Requires physical cape |
| GET | /cape/eeprom/signingFile/:key/:order | HARDWARE | @hardware:cape | Requires physical cape |
| POST | /cape/eeprom/signingData | HARDWARE | @hardware:cape | Requires physical cape |
| GET | /cape/options | SCHEMA | | May return empty object without cape |
| GET | /cape/strings | SCHEMA | | May return empty array without cape |
| GET | /cape/panel | SCHEMA | | May return empty array without cape |
| GET | /cape/strings/:key | HARDWARE | @hardware:cape | Requires physical cape |
| GET | /cape/panel/:key | HARDWARE | @hardware:cape | Requires physical cape |
| GET | /channel/input/stats | SCHEMA | | Object with numeric counters |
| DELETE | /channel/input/stats | STATE | @state | Resets stats |
| GET | /channel/output/processors | SCHEMA | | Returns array |
| POST | /channel/output/processors | STATE | @state | Saves output processors |
| GET | /channel/output/:file | SCHEMA | | Use `channeloutputs` as file param |
| POST | /channel/output/:file | STATE | @state | Saves channel output config |
| GET | /configfile | SCHEMA | | Returns array of directory names |
| GET | /configfile/** | SCHEMA | | Use a known path like `settings` |
| POST | /configfile/** | STATE | @state | Upload then delete |
| DELETE | /configfile/** | STATE | @state | Self-contained with upload first |
| POST | /dir/:DirName/:SubDir | STATE | @state | Create sequences/ci-test-dir |
| DELETE | /dir/:DirName/:SubDir | STATE | @state | Self-contained with create first |
| GET | /effects | SCHEMA | | Returns array |
| GET | /effects/ALL | SCHEMA | | Returns more items than /effects |
| POST | /email/configure | STATE | @state | Sets email config |
| POST | /email/test | STATE | @state | Sends actual email |
| GET | /events | SCHEMA | | Returns array |
| GET | /events/:eventId | STATE | @state | Requires event file to exist |
| POST | /events/:eventId/trigger | STATE | @state | Requires event to exist |
| GET | /files/:DirName | SCHEMA | | Use `sequences` as DirName |
| GET | /files/zip/:DirNames | SCHEMA | | Binary zip response |
| GET | /file/info/:plugin/:ext/** | STATE | @state | Requires plugin with a file |
| POST | /file/onUpload/:ext/** | STATE | @state | Requires plugin |
| POST | /file/move/:fileName | STATE | @state | Requires file to exist |
| POST | /file/:DirName/copy/:source/:dest | STATE | @state | Self-contained |
| POST | /file/:DirName/rename/:source/:dest | STATE | @state | Self-contained |
| GET | /file/:DirName/tailfollow/** | STATE | @state | Streaming endpoint |
| GET | /file/:DirName/** | STATE | @state | Requires file to exist |
| DELETE | /file/:DirName/** | STATE | @state | Self-contained with upload first |
| POST | /file/:DirName | STATE | @state | File upload |
| PATCH | /file/:DirName | STATE | @state | File patch (alias of POST) |
| POST | /file/:DirName/:Name | STATE | @state | File upload with explicit name |
| GET | /git/originLog | SCHEMA | | Returns array of commits |
| GET | /git/releases/os/:All | SCHEMA | | Returns array; use `0` as param |
| GET | /git/releases/sizes | SCHEMA | | Returns object |
| GET | /git/status | SCHEMA | | Returns object with branch info |
| GET | /git/branches | SCHEMA | | Returns array |
| POST | /git/reset | DESTRUCTIVE | @destructive | Resets git state |
| GET | /media | SCHEMA | | Returns array |
| GET | /media/:MediaName/duration | STATE | @state | Requires a media file |
| GET | /media/:MediaName/meta | STATE | @state | Requires a media file |
| GET | /network/dns | SCHEMA | | Object with nameservers array |
| POST | /network/dns | STATE | @state | Saves DNS config |
| GET | /network/gateway | SCHEMA | | Returns object |
| POST | /network/gateway | STATE | @state | Saves gateway config |
| GET | /network/interface | SCHEMA | | Returns array of interface objects |
| GET | /network/interface/:interface | SCHEMA | | Use `lo` as safe interface |
| POST | /network/interface/add/:interface | STATE | @state | Adds network interface |
| POST | /network/interface/:interface | STATE | @state | Sets interface config |
| POST | /network/interface/:interface/apply | DESTRUCTIVE | @destructive | Applies network config live |
| DELETE | /network/presisentNames | STATE | @state | Legacy typo alias; DELETE persistent names |
| POST | /network/presisentNames | STATE | @state | Legacy typo alias; POST persistent names |
| DELETE | /network/persistentNames | STATE | @state | Deletes persistent interface names |
| POST | /network/persistentNames | STATE | @state | Creates persistent interface names |
| GET | /network/wifi/scan/:interface | HARDWARE | @hardware:wifi | Requires WiFi interface |
| GET | /network/wifi/strength | HARDWARE | @hardware:wifi | Requires WiFi interface |
| GET | /options/:SettingName | SCHEMA | | Use `fppMode`; returns array of option objects |
| GET | /audio/cardaliases | HARDWARE | @hardware:audio | Requires audio device |
| POST | /audio/cardaliases | HARDWARE | @hardware:audio | Requires audio device |
| GET | /pipewire/audio/groups | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/audio/groups | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/audio/groups/apply | HARDWARE | @hardware:pipewire | Requires PipeWire |
| GET | /pipewire/audio/sinks | HARDWARE | @hardware:pipewire | Requires PipeWire |
| GET | /pipewire/audio/cards | HARDWARE | @hardware:pipewire | Requires PipeWire |
| GET | /pipewire/audio/sources | HARDWARE | @hardware:pipewire | Requires PipeWire |
| GET | /pipewire/audio/input-groups | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/audio/input-groups | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/audio/input-groups/apply | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/audio/input-groups/volume | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/audio/input-groups/effects | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/audio/input-groups/eq/update | HARDWARE | @hardware:pipewire | Requires PipeWire |
| GET | /pipewire/audio/routing | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/audio/routing | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/audio/routing/volume | HARDWARE | @hardware:pipewire | Requires PipeWire |
| GET | /pipewire/audio/routing/presets | HARDWARE | @hardware:pipewire | Requires PipeWire |
| GET | /pipewire/audio/routing/presets/names | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/audio/routing/presets | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/audio/routing/presets/load | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/audio/routing/presets/live-apply | HARDWARE | @hardware:pipewire | Requires PipeWire |
| DELETE | /pipewire/audio/routing/presets/:name | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/audio/stream/volume | HARDWARE | @hardware:pipewire | Requires PipeWire |
| GET | /pipewire/audio/stream/status | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/audio/group/volume | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/audio/eq/update | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/audio/delay/update | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/audio/sync/start | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/audio/sync/stop | HARDWARE | @hardware:pipewire | Requires PipeWire |
| GET | /pipewire/video/groups | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/video/groups | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/video/groups/apply | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/simple/apply | HARDWARE | @hardware:pipewire | Requires PipeWire |
| GET | /pipewire/video/connectors | HARDWARE | @hardware:pipewire | Requires PipeWire |
| GET | /pipewire/video/routing | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/video/routing | HARDWARE | @hardware:pipewire | Requires PipeWire |
| GET | /pipewire/video/input-sources | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/video/input-sources | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/video/input-sources/apply | HARDWARE | @hardware:pipewire | Requires PipeWire |
| GET | /pipewire/video/input-sources/v4l2-devices | HARDWARE | @hardware:pipewire | Requires PipeWire |
| GET | /pipewire/aes67/instances | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/aes67/instances | HARDWARE | @hardware:pipewire | Requires PipeWire |
| POST | /pipewire/aes67/apply | HARDWARE | @hardware:pipewire | Requires PipeWire |
| GET | /pipewire/aes67/status | HARDWARE | @hardware:pipewire | Requires PipeWire |
| GET | /pipewire/aes67/interfaces | HARDWARE | @hardware:pipewire | Requires PipeWire |
| GET | /pipewire/graph | HARDWARE | @hardware:pipewire | Requires PipeWire |
| GET | /playlists | SCHEMA | | Returns array |
| POST | /playlists | STATE | @state | Create ci-test-playlist; cleanup: delete it |
| GET | /playlists/playable | SCHEMA | | Returns array |
| GET | /playlists/validate | SCHEMA | | Returns object or array |
| POST | /playlists/stop | STATE | @state | Requires running playlist |
| POST | /playlists/pause | STATE | @state | Requires running playlist |
| POST | /playlists/resume | STATE | @state | Requires paused playlist |
| POST | /playlists/stopgracefully | STATE | @state | Requires running playlist |
| POST | /playlists/stopgracefullyafterloop | STATE | @state | Requires running playlist |
| GET | /playlist/:PlaylistName | STATE | @state | Requires playlist to exist |
| POST | /playlist/:PlaylistName | STATE | @state | Upsert; self-contained |
| DELETE | /playlist/:PlaylistName | STATE | @state | Self-contained with create first |
| POST | /playlist/:PlaylistName/start | STATE | @state | Requires playlist to exist |
| POST | /playlist/:PlaylistName/start/:Repeat | STATE | @state | Requires playlist to exist |
| POST | /playlist/:PlaylistName/start/:Repeat/:ScheduleProtected | STATE | @state | Requires playlist to exist |
| POST | /playlist/:PlaylistName/:SectionName/item | STATE | @state | Requires playlist to exist |
| GET | /plugin/headerIndicators | SCHEMA | | Returns array |
| GET | /plugin | SCHEMA | | Returns array |
| POST | /plugin | STATE | @state | Installs plugin from URL |
| POST | /plugin/fetchInfo | STATE | @state | Fetches plugin metadata from URL |
| GET | /plugin/:RepoName | STATE | @state | Requires plugin installed |
| DELETE | /plugin/:RepoName | STATE | @state | Self-contained with install first |
| GET | /plugin/:RepoName/settings/:SettingName | STATE | @state | Requires plugin installed |
| PUT | /plugin/:RepoName/settings/:SettingName | STATE | @state | Requires plugin installed |
| POST | /plugin/:RepoName/settings/:SettingName | STATE | @state | Requires plugin installed |
| POST | /plugin/:RepoName/updates | STATE | @state | Requires plugin installed |
| POST | /plugin/:RepoName/upgrade | STATE | @state | Requires plugin installed |
| GET | / | SCHEMA | | Serves the API docs index |
| GET | /api.html | SCHEMA | | Serves the API HTML UI |
| GET | /openapi.yaml | SCHEMA | | Serves the OpenAPI YAML spec |
| GET | /openapi.json | SCHEMA | | Serves the OpenAPI JSON spec |
| GET | /proxies | FULL | | Returns `[]` on fresh instance |
| POST | /proxies | STATE | @state | Sets full proxy list; self-contained |
| DELETE | /proxies | STATE | @state | Deletes all; self-contained with setup first |
| POST | /proxies/:ProxyIp | STATE | @state | Add 192.0.2.1; cleanup DELETE it |
| DELETE | /proxies/:ProxyIp | STATE | @state | Self-contained with add first |
| GET | /proxy/*/** | STATE | @state | Proxies a request to a remote FPP instance; requires reachable remote at a known IP |
| GET | /remotes | SCHEMA | | Returns array |
| POST | /remoteAction | STATE | @state | Requires a reachable remote FPP instance |
| GET | /schedule | SCHEMA | | Object with scheduleEntries array |
| POST | /schedule | STATE | @state | Overwrites entire schedule |
| POST | /schedule/reload | STATE | @state | Reloads schedule from disk |
| GET | /scripts | SCHEMA | | Returns array |
| POST | /scripts/installRemote/:category/:filename | STATE | @state | Requires internet and valid remote script |
| GET | /scripts/viewRemote/:category/:filename | STATE | @state | Requires internet |
| GET | /scripts/:scriptName | STATE | @state | Requires script to exist |
| POST | /scripts/:scriptName | STATE | @state | Saves script |
| POST | /scripts/:scriptName/run | STATE | @state | Requires script to exist |
| GET | /sequence | SCHEMA | | Returns array |
| POST | /sequence/current/step | STATE | @state | Requires paused sequence |
| POST | /sequence/current/stop | STATE | @state | Requires running sequence |
| POST | /sequence/current/togglePause | STATE | @state | Requires running sequence |
| GET | /sequence/:SequenceName | STATE | @state | Download; requires file |
| GET | /sequence/:SequenceName/meta | STATE | @state | Requires file |
| POST | /sequence/:SequenceName/start/:startSecond | STATE | @state | Requires file |
| POST | /sequence/:SequenceName | STATE | @state | Upload |
| DELETE | /sequence/:SequenceName | STATE | @state | Self-contained with upload first |
| GET | /settings | SCHEMA | | Large settings object |
| GET | /settings/:SettingName | SCHEMA | | Use `fppMode`; string value |
| GET | /settings/:SettingName/options | SCHEMA | | Use `fppMode`; array |
| PUT | /settings/:SettingName | STATE | @state | Change a safe setting; restore in cleanup |
| PUT | /settings/:SettingName/jsonValueUpdate | STATE | @state | JSON value update |
| GET | /statistics/usage | SCHEMA | | Returns object or null |
| POST | /statistics/usage | STATE | @state | Publishes stats |
| DELETE | /statistics/usage | STATE | @state | Deletes stats file |
| GET | /system/info | SCHEMA | | Object with Version, Hostname, etc. |
| GET | /system/status | SCHEMA | | Object with status string |
| GET | /system/updateStatus | SCHEMA | | Returns object |
| GET | /system/releaseNotes/:version | SCHEMA | | Use `current` as version param |
| GET | /system/packages | SCHEMA | | Returns array |
| GET | /system/packages/info/:packageName | SCHEMA | | Use `fpp` as packageName |
| POST | /system/fppd/skipBootDelay | STATE | @state | Skips boot delay |
| GET | /system/volume | HARDWARE | @hardware:audio | Requires audio device |
| POST | /system/volume | HARDWARE | @hardware:audio | Requires audio device |
| POST | /system/fppd/restart | DESTRUCTIVE | @destructive | Kills and restarts fppd |
| POST | /system/fppd/start | DESTRUCTIVE | @destructive | Starts fppd (may fail if already running) |
| POST | /system/fppd/stop | DESTRUCTIVE | @destructive | Kills fppd |
| POST | /system/reboot | DESTRUCTIVE | @destructive | Reboots the OS |
| POST | /system/shutdown | DESTRUCTIVE | @destructive | Shuts down the OS |
| GET | /system/proxies | SCHEMA | | Alias of /proxies |
| POST | /system/proxies | STATE | @state | Alias of POST /proxies |
| GET | /testmode | FULL | | Returns `{ enabled: false }` on fresh instance |
| POST | /testmode | STATE | @state | Enable test mode; cleanup: disable it |
| GET | /time | SCHEMA | | Object with utcDate, localDate, utcOffset |

## Known Gaps (in openapi.json but not in router)

| Verb | Endpoint | Notes |
| --- | --- | --- |
| n/a | /proxy/:Ip/:urlPart | Human-readable alias for GET /proxy/*/** which is dispatched via PHP array form; covered in Routes table |
