# Invalid Playlist Entry Setting
A playlist entry has a setting FPP cannot use, so it fell back to a safe default and that entry will not behave the way the playlist expects. The message names the entry and the setting.

Common causes:

- **Invalid start/end time** — a Branch entry's time is not in `HH:MM:SS` form (for example `8:00` instead of `08:00:00`).
- **Invalid iteration count** — a Branch entry's "every N loops" value is missing or zero.

These generally come from a playlist that was hand-edited or written through the API rather than saved from the playlist editor.

1. Open the [Playlists](../../playlists.php) page and edit the named playlist.
2. Re-set the flagged field on the entry and save the playlist — saving from the editor writes valid values.
3. Check `fppd.log` for the specific entry that triggered the error.
