# Scheduled Playlist Or Sequence Does Not Exist
A schedule entry refers to a playlist or sequence that is not present on this device, so that entry has been disabled and will not run.

This usually happens when a playlist/sequence was renamed or deleted but the schedule still points at the old name, or when it was never uploaded to this device.

1. Open the [Schedule](../../scheduler.php) and check the entry — fix it to point at an existing playlist/sequence, or remove it.
2. If the item should exist, recreate it or upload it (e.g. via FPP Connect from xLights), then the warning clears on the next schedule reload.
3. Names are case-sensitive and must match exactly.
