# Network/DMX Input Error
FPP could not set up an input that feeds channel data into this device, or the ArtNet port it shares between input and output.

- **ArtNet is off: could not open UDP port 6454** — another program on this device (a second player, xLights, a plugin) already holds the ArtNet port. ArtNet input, ArtPoll replies and ArtNet output that must come from port 6454 are off. FPP tries the port again every few seconds and clears this warning once it gets it.
- **ArtNet input is off and ArtNet output is sent from another port … Restart FPPD once the port is free** — the same cause, but FPP's ArtNet outputs could still send from a different port. Most controllers accept that. FPP does not try port 6454 again after this, so ArtNet input and ArtPoll replies stay off, and this warning stays up, until FPPD is restarted with the port free.
- **Could not open DMX input device `<device>`** — the configured serial/USB DMX input device is not present or could not be opened. It clears the next time the DMX inputs are saved and the device opens.
- **Channel Inputs: only the first 65536 input universes are used** — the input universe rows add up to more universes than FPP accepts, so the rest are ignored. Reduce the universe counts on [Channel Inputs](../../channelinputs.php); the warning clears when the saved configuration fits.

1. For the ArtNet warnings, find what holds the port: log in over SSH and run `ss -ulpn | grep ':6454'`, then stop or reconfigure that program.
2. For the DMX input error, confirm the USB/serial DMX adapter is plugged in and the device name is correct on [Channel Inputs](../../channelinputs.php).
3. Check `fppd.log` for the underlying error.
