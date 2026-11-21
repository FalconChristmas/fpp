# Virtual Display Map Missing
An HTTP Virtual Display output is configured, but its pixel-map file is missing from FPP's configuration directory or could not be read, so the Virtual Display has nothing to render.

With the output's **Preview** dropdown left on **Default**, the map is `config/default.vdm` when it exists, otherwise `config/virtualdisplaymap`. A named preview (for example `christmas.vdm`) selects that file from the configuration directory; if the saved file no longer exists, the output reports it here instead of silently using a different map.

xLights generates this map and uploads it to FPP automatically via **FPP Connect** when the **Models** checkbox is ticked for this device.

1. In xLights, open **FPP Connect**, tick the **Models** checkbox for this device, and upload.
2. Make sure the **HTTP Virtual Display** output is added on the **'Other'** tab of the [Channel Outputs](../../channeloutputs.php) page.
3. If the Preview dropdown names a file that is no longer in the configuration directory, choose **Default** or upload the missing `.vdm` file.
4. If you are not using a Virtual Display, remove that output.
