# Virtual Display Map Missing
An HTTP Virtual Display output is configured, but its pixel-map file (`virtualdisplaymap`) is missing from FPP's configuration directory or could not be read, so the Virtual Display has nothing to render.

xLights generates this map and uploads it to FPP automatically via **FPP Connect** when the **Models** checkbox is ticked for this device.

1. In xLights, open **FPP Connect**, tick the **Models** checkbox for this device, and upload.
2. Make sure the **HTTP Virtual Display** output is added on the **'Other'** tab of the [Channel Outputs](../../channeloutputs.php) page.
3. If you are not using a Virtual Display, remove that output.
