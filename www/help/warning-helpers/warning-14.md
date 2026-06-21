# Could Not Resolve Host Name
A network output is configured with a host name that DNS could not resolve, so that output has been disabled.

1. Open [Channel Outputs](../../channeloutputs.php) and check the address of each network output (E1.31, DDP, ArtNet, KiNet, Twinkly).
2. Prefer a numeric IP address, or make sure the host name resolves on your network.
3. If the controller is offline or renamed, fix or remove the entry — output stays disabled until the name resolves.

Note: on a device that has a cape or hat installed, the network/UDP outputs are hidden while **User Interface Level** is set to **Basic**. Set it to **Advanced** on the [System](../../settings.php#settings-system) settings page to see and edit them.
