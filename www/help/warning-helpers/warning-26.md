# Channel Output Configuration Error
FPP could not load or start one of your channel outputs, so that output will not send data. The message names the output type and the specific problem:

- **Could not parse `<file>`** — the channel output configuration file is corrupt (often from an interrupted save or upload).
- **Invalid start channel** — an output has a start channel of 0 or negative.
- **Could not initialize / create output type `<type>`** — the output could not start; the FPPD log has the details.

1. Open [Channel Outputs](../../channeloutputs.php) and review the affected output's settings (start channel, channel count, address/interface).
2. If a config file is corrupt, re-upload your outputs from xLights (FPP Connect), or use **Reset FPP Config** on the [System](../../settings.php#settings-system) settings page and re-upload. **Reset FPP Config** is only shown when **User Interface Level** is **Advanced** or higher.
3. Check `fppd.log` for the detailed reason behind an initialize/create failure.

Note: on a device with a cape or hat installed, network/UDP outputs are hidden while **User Interface Level** is **Basic** — set it to **Advanced** to see and edit them.
