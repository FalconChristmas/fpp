# Frame Skips - Likely Slow Network
This remote repeatedly had to skip frames early in playback, which usually means sequence data is not arriving from the master fast enough.

1. Prefer wired Ethernet over Wi-Fi for both master and remotes.
2. Keep show traffic off congested/uplinked switches and avoid mixing it with heavy general network traffic.
3. Verify the master can read its sequences fast enough (see slow-storage warnings).
