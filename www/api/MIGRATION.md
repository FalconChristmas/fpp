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
incorrectly exposed as `GET`. All have been moved to `POST` (or, in one case, corrected from
`POST` to `GET`).

Unless noted otherwise, the only required client change is updating the HTTP method.
Parameters that were previously passed as URL query strings remain in the query string.
No request body is required.

---

### System Control

| Old | New |
| --- | --- |
| `GET /api/system/reboot` | `POST /api/system/reboot` |
| `GET /api/system/shutdown` | `POST /api/system/shutdown` |
| `GET /api/system/fppd/start` | `POST /api/system/fppd/start` |
| `GET /api/system/fppd/stop` | `POST /api/system/fppd/stop` |
| `GET /api/system/fppd/restart` | `POST /api/system/fppd/restart` |

**Required change:** Replace `GET` with `POST`. No request body is required.

---

### Playlist Control

| Old | New |
| --- | --- |
| `GET /api/playlists/stop` | `POST /api/playlists/stop` |
| `GET /api/playlists/pause` | `POST /api/playlists/pause` |
| `GET /api/playlists/resume` | `POST /api/playlists/resume` |
| `GET /api/playlists/stopgracefully` | `POST /api/playlists/stopgracefully` |
| `GET /api/playlists/stopgracefullyafterloop` | `POST /api/playlists/stopgracefullyafterloop` |
| `GET /api/playlist/{PlaylistName}/start` | `POST /api/playlist/{PlaylistName}/start` |
| `GET /api/playlist/{PlaylistName}/start/{Repeat}` | `POST /api/playlist/{PlaylistName}/start/{Repeat}` |
| `GET /api/playlist/{PlaylistName}/start/{Repeat}/{ScheduleProtected}` | `POST /api/playlist/{PlaylistName}/start/{Repeat}/{ScheduleProtected}` |

**Required change:** Replace `GET` with `POST`. The optional `scheduleProtected` query
parameter remains in the URL query string. No request body is required.

---

### Sequence Control

| Old | New |
| --- | --- |
| `GET /api/sequence/{SequenceName}/start/{startSecond}` | `POST /api/sequence/{SequenceName}/start/{startSecond}` |
| `GET /api/sequence/current/step` | `POST /api/sequence/current/step` |
| `GET /api/sequence/current/togglePause` | `POST /api/sequence/current/togglePause` |
| `GET /api/sequence/current/stop` | `POST /api/sequence/current/stop` |

**Required change:** Replace `GET` with `POST`. No request body is required.

---

### Event Triggering

| Old | New |
| --- | --- |
| `GET /api/events/{eventId}/trigger` | `POST /api/events/{eventId}/trigger` |

**Required change:** Replace `GET` with `POST`. No request body is required.

---

### File Operations

| Old | New |
| --- | --- |
| `GET /api/file/move/{fileName}` | `POST /api/file/move/{fileName}` |
| `GET /api/file/onUpload/{ext}/**` | `POST /api/file/onUpload/{ext}/**` |

**Required change:** Replace `GET` with `POST`. No request body is required.

---

### Network Configuration

| Old | New |
| --- | --- |
| `GET /api/network/interface/add/{interface}` | `POST /api/network/interface/add/{interface}` |

**Required change:** Replace `GET` with `POST`. No request body is required.

---

### Git Operations

| Old | New |
| --- | --- |
| `GET /api/git/reset` | `POST /api/git/reset` |

**Required change:** Replace `GET` with `POST`. No request body is required.

---

### Script Execution

| Old | New |
| --- | --- |
| `GET /api/scripts/{scriptName}/run` | `POST /api/scripts/{scriptName}/run` |
| `GET /api/scripts/installRemote/{category}/{filename}` | `POST /api/scripts/installRemote/{category}/{filename}` |

**Required change:** Replace `GET` with `POST`. No request body is required.

---

### Plugin Management

| Old | New |
| --- | --- |
| `GET /api/plugin/{RepoName}/upgrade` | `POST /api/plugin/{RepoName}/upgrade` |

**`/api/plugin/{RepoName}/upgrade`:** The `GET` alias has been removed. Both verbs were
previously accepted; only `POST` remains. Replace `GET` with `POST`. No request body
is required.

---

### Remote Proxy

| Old | New |
| --- | --- |
| `GET /api/remoteAction` | `POST /api/remoteAction` |

**Required change:** Replace `GET` with `POST`. Move `ip` and `action` from the URL query
string into a JSON request body.

```text
Before:
  GET /api/remoteAction?ip=192.168.1.100&action=reboot

After:
  POST /api/remoteAction
  Content-Type: application/json
  {"ip": "192.168.1.100", "action": "reboot"}
```
