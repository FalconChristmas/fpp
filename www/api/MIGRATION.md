# API Migration Guide

This document records breaking changes to the FPP REST API surface. Each section covers
one phase of the ongoing API review. Entries are cumulative — newer phases are added at the
top.

---

## HTTP Verb Corrections

**Background:** HTTP `GET` is defined as a safe, idempotent method. Browsers, proxies,
prefetchers, and link crawlers routinely follow GET links speculatively and without user
intent. The routes listed in this section all mutate system state — they start or stop
playback, reboot hardware, run scripts, trigger events, or modify files — and were therefore
incorrectly exposed as `GET`. All have been moved to `POST` for the `/api/v2/` surface.

The legacy `/api/` surface remains backward compatible where the merged router intentionally
preserves historical GET behavior. New clients should target `/api/v2/`.

Unless noted otherwise, the only required client change is updating the HTTP method.
Parameters that were previously passed as URL query strings remain in the query string.
No request body is required.

---

### System Control

| Old | New |
| --- | --- |
| `GET /api/system/reboot` | `POST /api/v2/system/reboot` |
| `GET /api/system/shutdown` | `POST /api/v2/system/shutdown` |
| `GET /api/system/fppd/start` | `POST /api/v2/system/fppd/start` |
| `GET /api/system/fppd/stop` | `POST /api/v2/system/fppd/stop` |
| `GET /api/system/fppd/restart` | `POST /api/v2/system/fppd/restart` |

### Playlist Control

| Old | New |
| --- | --- |
| `GET /api/playlists/stop` | `POST /api/v2/playlists/stop` |
| `GET /api/playlists/pause` | `POST /api/v2/playlists/pause` |
| `GET /api/playlists/resume` | `POST /api/v2/playlists/resume` |
| `GET /api/playlists/stopgracefully` | `POST /api/v2/playlists/stopgracefully` |
| `GET /api/playlists/stopgracefullyafterloop` | `POST /api/v2/playlists/stopgracefullyafterloop` |
| `GET /api/playlist/{PlaylistName}/start` | `POST /api/v2/playlist/{PlaylistName}/start` |
| `GET /api/playlist/{PlaylistName}/start/{Repeat}` | `POST /api/v2/playlist/{PlaylistName}/start/{Repeat}` |
| `GET /api/playlist/{PlaylistName}/start/{Repeat}/{ScheduleProtected}` | `POST /api/v2/playlist/{PlaylistName}/start/{Repeat}/{ScheduleProtected}` |

### Sequence Control

| Old | New |
| --- | --- |
| `GET /api/sequence/{SequenceName}/start/{startSecond}` | `POST /api/v2/sequence/{SequenceName}/start/{startSecond}` |
| `GET /api/sequence/current/step` | `POST /api/v2/sequence/current/step` |
| `GET /api/sequence/current/togglePause` | `POST /api/v2/sequence/current/togglePause` |
| `GET /api/sequence/current/stop` | `POST /api/v2/sequence/current/stop` |

### Event Triggering

| Old | New |
| --- | --- |
| `GET /api/events/{eventId}/trigger` | `POST /api/v2/events/{eventId}/trigger` |

### File Operations

| Old | New |
| --- | --- |
| `GET /api/file/move/{fileName}` | `POST /api/v2/file/move/{fileName}` |
| `GET /api/file/onUpload/{ext}/**` | `POST /api/v2/file/onUpload/{ext}/**` |

### Network Configuration

| Old | New |
| --- | --- |
| `GET /api/network/interface/add/{interface}` | `POST /api/v2/network/interface/add/{interface}` |

### Git Operations

| Old | New |
| --- | --- |
| `GET /api/git/reset` | `POST /api/v2/git/reset` |

### Script Execution

| Old | New |
| --- | --- |
| `GET /api/scripts/{scriptName}/run` | `POST /api/v2/scripts/{scriptName}/run` |
| `GET /api/scripts/installRemote/{category}/{filename}` | `POST /api/v2/scripts/installRemote/{category}/{filename}` |

### Plugin Management

| Old | New |
| --- | --- |
| `GET /api/plugin/{RepoName}/upgrade` | `POST /api/v2/plugin/{RepoName}/upgrade` |

### Remote Proxy

| Old | New |
| --- | --- |
| `GET /api/remoteAction?ip=192.168.1.100&action=reboot` | `POST /api/v2/remoteAction` with JSON body |
