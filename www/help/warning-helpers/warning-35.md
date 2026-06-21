# Invalid Pixel Overlay Model
Something is configured to use a Pixel Overlay Model that does not exist (or no model name was given), so it cannot display.

This usually means a model was renamed or deleted but something still references the old name — for example a playlist image step, an effect, or a matrix/virtual output.

1. Open [Pixel Overlay Models](../../pixeloverlaymodels.php) and confirm the model exists with the expected name (names are case-sensitive).
2. Fix the reference to point at an existing model, or recreate the missing model.
3. If the reference is in a playlist step, edit it in the [Playlists](../../playlists.php) editor.
