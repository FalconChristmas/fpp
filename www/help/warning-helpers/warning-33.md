# Dynamic Playlist Data Invalid
A dynamic playlist could not be built because the data it loaded (from a file, URL, or plugin) was missing or invalid. The message names the specific problem:

- **Source file does not exist** — the configured data file is missing.
- **Data is not valid JSON** — the returned content could not be parsed.
- **Invalid entry type / entry failed to initialize** — the data referenced a playlist entry type FPP does not recognize, or one that could not start.
- **Contained no valid entries** — the data produced an empty playlist.

1. Open the [Playlists](../../playlists.php) editor and check the dynamic playlist's source (file path, URL, or plugin).
2. If it loads from a file, confirm the file exists (upload via the [File Manager](../../filemanager.php)) and contains valid playlist JSON.
3. If it loads from a URL/plugin, verify that endpoint returns the expected JSON with supported entry types.
4. Check `fppd.log` for the exact parse/initialization error.

This warning clears automatically after a short time once the problem stops recurring.
