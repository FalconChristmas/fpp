# Virtual Display Preview Unavailable
FPP could not start the network listener for the browser-based Virtual Display preview, so the live preview page will not load. Pixel output to your real controllers is unaffected.

The most common cause is the preview's port already being in use (for example a leftover process, or two virtual-display outputs configured on the same port).

1. If you do not use the Virtual Display preview, you can ignore this — or remove the Virtual Display output on the [Channel Outputs](../../channeloutputs.php) page.
2. Make sure only one Virtual Display (and one 3D Virtual Display) output is configured, each on its own port.
3. Reboot to clear a leftover process holding the port.
4. Check `fppd.log` for the underlying socket error.
