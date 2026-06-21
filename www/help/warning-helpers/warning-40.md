# nRF24 Radio Output Error
FPP could not start the nRF24L01 radio output, so wireless pixel data will not be sent. The message names the problem:

- **Invalid speed / invalid channel** — the configured radio data rate or channel is out of range (channel must be 0–125).
- **Could not create radio instance / radio not detected** — the nRF24L01 module was not found.

1. Open [Channel Outputs](../../channeloutputs.php) and check the nRF24 output's speed and channel settings.
2. Confirm the nRF24L01 module is wired correctly to the SPI pins and seated firmly.
3. Make sure SPI is enabled on this device.
4. Reboot after fixing wiring. To check at the system level, open **Help → [Troubleshooting Commands](../../troubleshooting.php)** and run **Boot → dmesg** (SPI/radio errors) and **OS, Kernel, and SD image → Kernel Modules** (is the SPI module loaded?).
