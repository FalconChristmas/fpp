# Smart Receiver Warning
A smart receiver or cape reported a problem with one of its outputs. The message names the affected port. There are two common causes:

**Pixel count mismatch** ("configured for X pixels but Y pixels detected")
1. Compare the configured pixel count for that port with the strings actually connected.
2. Update the model/output configuration, or fix the wiring, so the counts agree.
3. A large mismatch can also indicate a broken pixel, a bad connection, or power-injection issues partway down the string.

**eFUSE triggered** ("eFUSE Triggered for ...")
1. Power down and inspect the wiring on the listed output for shorts or reversed polarity.
2. Confirm the pixel count/current draw is within the port and power-injection limits.
3. After fixing the fault, restart FPP (or power-cycle the receiver) to clear the fuse.
