# USB DMX Adapter Problem
FPP could not start a USB DMX output. The message describes the specific problem:

- **No UDMX Device Found / Error Opening USB Device** — the adapter is not plugged in, not detected, or another process is using it.
- **Unknown dongle type** — the configured adapter type does not match the connected hardware.
- **Error Starting LibUSB** — FPP could not access USB on this system.
- **Channel count … exceeds the DMX universe size of 512** — a single DMX output cannot exceed 512 channels.

1. Confirm the USB DMX adapter is firmly plugged in; try a different USB port or cable.
2. On [Channel Outputs](../../channeloutputs.php), make sure the selected adapter/dongle type matches your hardware (uDMX, DMX-Open, or DMX-Pro).
3. Keep each DMX output's channel count at **512 or fewer**; split larger setups across multiple outputs/universes.
4. Reboot if the adapter was plugged in after FPP started, or if USB is not responding.
