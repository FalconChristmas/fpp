# LED Panel Output Failed
FPP could not initialize an RGB LED panel (matrix) output, so the connected panels will not light up. The message names the cause:

- **Cannot run with snd_bcm2835 enabled — please reboot** — the onboard audio driver conflicts with the panel output's use of the hardware. FPP has requested a reboot; reboot the device.
- **Does not work on Raspberry Pi 5** — the GPIO RGB-panel output (rpi-rgb-led-matrix) is not supported on the Raspberry Pi 5 / CM5.
- **channelCount exceeds maximum** — the panel layout requests more channels than FPP supports; reduce the number/size of panels.
- **Unable to create panel interleave handler** / **no (or unreadable) output pin configuration** — the panel's scan/interleave or the cape's pin map could not be loaded.
- **Unable to create RGBMatrix instance** — the panel library rejected the configured options.

1. Open [Channel Outputs](../../channeloutputs.php) and review the **LED Panels** configuration (panel size, layout, scan/interleave, and the selected cape/hat).
2. If the message asks you to reboot, reboot the device.
3. Confirm the configured cape/hat matches the hardware actually installed. If the cape is correct but its pin configuration can't be read, the cape's EEPROM may be outdated or incorrect — **contact your cape/hat vendor for an updated EEPROM image.**
4. Check `fppd.log` for the exact panel error.
