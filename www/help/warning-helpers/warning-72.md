# Network Input Port Unavailable

FPP could not open the UDP port it listens on for E1.31 (port 5568) or DDP (port 4048) data, almost always because another program on this device already has it. FPP keeps running, but that input is off: data sent to FPP with that protocol is ignored.

1. Find what holds the port: log in over SSH and run `ss -ulpn | grep -E ':5568|:4048'`.
2. Stop or reconfigure that program so it no longer listens on the port.
3. Save the [Channel Inputs](../../channelinputs.php) page again, or restart FPPD, to reopen the input.

This warning clears when the input is reopened successfully.
