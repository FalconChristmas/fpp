# Media File Could Not Be Played
FPP tried to play an audio/video file but could not. The message names the file and the specific problem:

- **Media file not found** — the file is missing from this device.
- **No media output handler / unsupported file type** — the file format is not supported (or its extension is wrong).
- **No media files found to play** — a random/sequential media entry has an empty folder or prefix.
- **Could not start media** — the file exists but failed to start (corrupt file, missing codec, or busy audio/video device).

1. Confirm the file is uploaded via the [File Manager](../../filemanager.php) and the name matches exactly (case-sensitive).
2. Use a supported format (e.g. `.mp3`/`.wav`/`.ogg` audio, `.mp4` video) and re-encode if the file is unusual or corrupt.
3. For random-media playlist entries, check the folder/prefix actually contains matching files — open the entry in the [Playlists](../../playlists.php) editor.
4. Check the audio/video output device on the [Audio/Video](../../settings.php#settings-av) settings page; see also the **Dummy Audio** warning if no audio device is found.

This warning clears automatically after a short time once the problem stops recurring.
