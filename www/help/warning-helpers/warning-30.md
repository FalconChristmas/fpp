# Media File Could Not Be Played
FPP tried to play an audio/video file but could not. The message names the file and the specific problem:

- **Media file not found** — the file is missing from this device.
- **No media output handler / unsupported file type** — the file format is not supported (or its extension is wrong).
- **No media files found to play** — a random/sequential media entry has an empty folder or prefix.
- **Could not start media** — the file exists but failed to start (corrupt file, missing codec, or busy audio/video device).
- **Can't play … the file looks damaged or is not a media file** — the file could not be unpacked or decoded; the text in brackets is the reason. Re-export or re-encode it, or upload it again if the copy may be incomplete.
- **Can't play … it uses a format this device can't decode** — the file may be fine, but this device has no decoder for it (for example HEVC/H.265 video on a Raspberry Pi 0–3). Re-encode it as H.264.
- **Can't play … the file could not be read** — the file is there but reading it failed; check the storage (SD card or USB drive).
- **No playable audio in … and no video output** — a video-only file on an item with no connected video output; set the item's Video Output to a connected display.
- **Playback of … stalled and was stopped** — the media stopped moving and FPP stopped it; check the audio device and the HDMI connection. When a video output failure caused it, that warning is shown instead.

1. Confirm the file is uploaded via the [File Manager](../../filemanager.php) and the name matches exactly (case-sensitive).
2. Use a supported format (e.g. `.mp3`/`.wav`/`.ogg` audio, `.mp4` video) and re-encode if the file is unusual or corrupt.
3. For random-media playlist entries, check the folder/prefix actually contains matching files — open the entry in the [Playlists](../../playlists.php) editor.
4. Check the audio/video output device on the [Audio/Video](../../settings.php#settings-av) settings page; see also the **Dummy Audio** warning if no audio device is found.

This warning clears automatically after a short time once the problem stops recurring.
