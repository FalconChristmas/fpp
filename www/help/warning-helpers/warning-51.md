# GPIO Pin Configuration Error
FPP could not find or configure a GPIO pin. Depending on what the pin was for, an output may not turn on, a fuse/power monitor may not work, or a GPIO input/event may not be detected. The message names the pin and what it was needed for.

- **Pin not found** — the configured pin name does not exist on this hardware (often a cape/hat configured here is not the one actually installed).
- **Could not open GPIO chip / configure pin / request events** — the pin could not be claimed, usually because another process already holds it or the kernel GPIO driver is unavailable.

1. Open [Channel Outputs](../../channeloutputs.php) and confirm the configured cape/hat matches the hardware actually installed, and that pin assignments are correct.
2. For GPIO inputs/outputs configured directly, review the pin settings on the [GPIO](../../gpio.php) page.
3. Make sure no other output or process is using the same pin.
4. Reboot after correcting wiring or cape selection, then check `fppd.log` for the exact pin error.
5. If the cape/hat is correctly installed and seated but the pin still isn't found, the cape's EEPROM (its pin map) may be outdated or incorrect for this board or FPP version. **Contact your cape/hat vendor for an updated EEPROM image.**
