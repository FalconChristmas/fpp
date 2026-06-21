# Serial Output Port Error
FPP could not open or configure a serial port used by a serial channel output (DMX, Renard, Pixelnet, etc.), so that output will not send data. The message names the device and problem:

- **invalid serial port mode** — the configured data/parity/stop setting is not valid.
- **could not set exclusive mode** — another process is using the port.
- **could not read/set port attributes or speed** — the adapter rejected the settings.

1. Open [Channel Outputs](../../channeloutputs.php) and confirm the serial output's device and settings (correct `/dev/tty…` device, baud rate, mode).
2. Make sure the USB-serial adapter is plugged in and nothing else is using it.
3. Try a different USB port/cable; reboot if the adapter was connected after FPP started.
4. To check at the system level, open **Help → [Troubleshooting Commands](../../troubleshooting.php)**, run **USB → USB Device List** to confirm the adapter is present, and **Boot → dmesg** to see which `/dev/ttyUSB*` it was assigned.
