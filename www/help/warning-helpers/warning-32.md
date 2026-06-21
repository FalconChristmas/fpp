# Playlist URL Request Failed
A playlist "URL" step (or a dynamic playlist fetching from a URL) could not reach its target, so that step did nothing.

1. Check the URL in the playlist step — open the [Playlists](../../playlists.php) editor and verify the address, method, and any data.
2. Confirm the target host is reachable from FPP (powered on, correct IP/hostname, on the network).
3. If the URL points at another device, make sure that device is up and the path is correct.
4. Check `fppd.log` for the underlying error (timeout, connection refused, DNS failure).

This warning clears automatically after a short time once the problem stops recurring.
