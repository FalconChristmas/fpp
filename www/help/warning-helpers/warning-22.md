# Invalid Interface For UDP Broadcast/Multicast
A UDP output is set to broadcast or multicast on a network interface that does not exist or is down, so the output cannot send.

1. Open [Channel Outputs](../../channeloutputs.php) and pick a valid, active interface for the output.
2. Confirm the chosen interface is up and has an IP address (see the Status page / network configuration).
3. For unicast outputs this is not needed — set a specific destination IP instead.

Note: on a device that has a cape or hat installed, the network/UDP outputs are hidden while **User Interface Level** is set to **Basic**. Set it to **Advanced** on the [System](../../settings.php#settings-system) settings page to see and edit them.
