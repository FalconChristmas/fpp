# MultiSync Network Socket Error
FPP could not open or bind one of the UDP sockets it uses for MultiSync, so it cannot send or receive sync packets with other FPP devices.

- **Could not open broadcast / control socket** — FPP could not create the socket used to send sync and command packets.
- **Could not open receive socket** — FPP could not create the socket that listens for sync packets from a master.
- **Could not bind receive socket** — the MultiSync UDP port (32320) is already in use. This usually means another copy of `fppd` is already running on this device.

1. Make sure this device actually has a working network connection (an interface with an IP address).
2. Confirm MultiSync is set up the way you expect on the [MultiSync](../../multisync.php) page.
3. To check at the system level, open **Help → [Troubleshooting Commands](../../troubleshooting.php)** and run **Networking → Interfaces** to confirm an interface is up with a valid IP address.
4. Check `fppd.log` for the underlying socket error.
