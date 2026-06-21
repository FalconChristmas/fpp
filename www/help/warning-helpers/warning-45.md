# AES67 PTP Clock Sync Failed
AES67 needs an accurate shared clock, which FPP provides by running the `ptp4l` (PTP) daemon. This warning means PTP clock sync could not start, so AES67 audio may not stay in sync (or may not play at all). The message names the problem:

- **Could not write PTP configuration file** — FPP could not write its temporary PTP config to `/tmp`.
- **PTP clock sync could not start on the configured interface** — `ptp4l` exited even after FPP retried with software timestamping. The network interface may not support PTP.

1. On the [AES67](../../aes67-config.php) configuration page, confirm the PTP interface is set to a real, connected network interface.
2. To check whether the interface supports hardware timestamping, open **Help → [Troubleshooting Commands](../../troubleshooting.php)** and run **Networking → Wired** (the `ethtool` output lists the interface's timestamping capabilities).
3. Check `fppd.log` for the underlying `ptp4l` error.
