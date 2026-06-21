# WLED Audio Sync Error
FPP's WLED Audio Sync feature could not start, so FFT/volume audio data will not be exchanged with WLED sound-reactive devices. The message names the problem:

- **Could not open the send / receive socket** — FPP could not create the UDP socket for WLED audio sync.
- **Invalid destination address** — the configured destination is not a valid IP address.

1. Open the [Audio/Video](../../settings.php#settings-av) settings and find **WLED Audio Sync** under General Audio. These controls are only shown when **User Interface Level** is **Advanced** or higher.
2. Check the **WLED Audio Sync Address** — it must be a valid unicast IP (e.g. `192.168.1.70`), a broadcast address, or WLED's default multicast group `239.0.0.1`.
3. Confirm the device has a working network connection.
4. Check `fppd.log` for the underlying socket error.
