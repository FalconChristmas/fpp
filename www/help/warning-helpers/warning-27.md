# Cannot Reach Output Controller
FPP could not reach a controller it is configured to send pixel data to. After several failed pings (and an HTTP check), FPP marks that output invalid and stops sending to it. It will automatically resume — and clear this warning — once the controller responds again.

1. Confirm the controller is powered on and connected to the network.
2. Check the controller's IP address and that it is on the same subnet as FPP; verify cabling/switch and Wi-Fi.
3. If the address is wrong, correct it on the [Channel Outputs](../../channeloutputs.php) page.
4. A controller using a static IP that changed, or DHCP handing out a new address, is a common cause.

Note: on a device that has a cape or hat installed, the network/UDP outputs are hidden while **User Interface Level** is set to **Basic**. Set it to **Advanced** on the [System](../../settings.php#settings-system) settings page to see and edit them.
