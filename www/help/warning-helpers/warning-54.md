# Network/DMX Input Error
FPP could not set up an input that feeds channel data into this device, so incoming data on that input will not be received.

- **Socket bind failed for ArtNet** — FPP could not bind the ArtNet input port, usually because another program on this device is already using it.
- **Could not open DMX input device `<device>`** — the configured serial/USB DMX input device is not present or could not be opened.

1. Open [Channel Inputs](../../channelinputs.php) and review the configured inputs.
2. For the DMX input error, confirm the USB/serial DMX adapter is plugged in and the device name is correct.
3. For the ArtNet bind error, make sure no other program or plugin on this device is bound to the ArtNet port.
4. Check `fppd.log` for the underlying error.
