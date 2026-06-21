# Network Discovery (mDNS) Unavailable
FPP uses mDNS (via the Avahi daemon) to automatically find other FPP and WLED devices on your network. This warning means discovery failed, so other devices may not appear automatically in the systems list. Sync and manual configuration still work — only automatic discovery is affected.

- **Could not browse for FPP / WLED devices** — FPP could not start the mDNS browser.
- **mDNS/Avahi client failure** — the connection to the Avahi daemon was lost or never established.

1. If discovery still fails, reboot the device — Avahi normally starts automatically at boot.
2. You can always add remote devices by IP/hostname manually on the [MultiSync](../../multisync.php) page even when discovery is unavailable.
3. Check `fppd.log` for the underlying Avahi error message.
