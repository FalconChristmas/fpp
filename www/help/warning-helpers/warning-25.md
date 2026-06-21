# Sequence File Does Not Exist
FPP tried to play a sequence (`.fseq`) file that is not present on this device.

On a **remote**, this most often means the sequence has not been synced/uploaded from the master, or the file name does not match what the master is asking for.

1. Upload the sequence to this device via the [File Manager](../../filemanager.php), or re-run FPP Connect from xLights to push it.
2. Confirm the file name matches exactly (names are case-sensitive).
3. On a remote, you can place a `fallback.fseq` in the sequences folder — FPP plays it automatically when the requested sequence is missing.
