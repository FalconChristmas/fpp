# Playlist Could Not Be Loaded
FPP could not load or play a playlist. The message names the playlist and the specific problem, which is usually one of:

- **Could not load playlist** — the playlist file is missing or its JSON is corrupt.
- **Playlist is invalid** — the file exists but contains a formatting error.
- **Playlist is empty. Nothing to play.** — the playlist has no lead-in, main, or lead-out items.
- **fseq "…" lists a media file of "…" but it can not be found** — the sequence expects an audio/video file that is not uploaded.
- **Unknown playlist entry type** — the playlist contains an entry of a type FPP does not recognize.
- **Channel-remap entry is missing a field** — a "remap" step is missing a required setting (action/source/destination/count/loops).
- **Recursive playlist** — a playlist includes itself (directly or through sub-playlists) too many levels deep.

1. Open the [Playlists](../../playlists.php) editor, re-open the named playlist, fix or re-add its items, and save it again.
2. If a media file is missing, upload it via the [File Manager](../../filemanager.php) (or re-run FPP Connect from xLights with the media checkbox ticked).
3. For an empty playlist, add at least one sequence/media entry or remove the playlist.
4. If the playlist file itself is corrupt and will not open in the editor, delete the playlist from the [Playlists](../../playlists.php) page (deleting works even when the playlist cannot be opened) and recreate it. (Reset FPP Config does not remove playlists, so deleting the broken one is the way to clear it.)
