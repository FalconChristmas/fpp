# Network Interface Dropping Packets
The player is discarding outgoing packets before they reach the network cable. The network
protocols that carry pixel data have no acknowledgements, so these losses are silent: the
controllers just miss frames, which shows up as flicker or lights that lag. This warning only
counts drops on this player. A switch or controller that drops packets further along cannot be
seen from here.

Things to check:

1. **The link.** Look at the network interface on the [Network](../../networkconfig.php) page.
   A cable or switch port that came up at 10 Mbps or half duplex, or keeps bouncing, causes
   this. Try another cable or switch port.
2. **Wi-Fi.** Sending pixel data over Wi-Fi is often more than the link can carry. Use a
   wired connection for show data where you can.
3. **Too much data for the link.** On the [Channel Outputs](../../channeloutputs.php#tab-e131)
   page, check the total being sent: the controller count, the channels per controller, and
   the sequence frame rate. Lowering the frame rate or switching controllers to DDP reduces
   the load.
4. **Other traffic on the same interface**, such as large file copies or video streaming
   during the show.

`fppd.log` has a line for each check that saw drops, with the counts. The warning clears on
the first check (about once a minute while output is running) that sees no new drops.
