# Colorlight Link Down
The Network link between FPP and your colorlight receiver is down.

Ensure your Colorlight receivers are plugged in and powered on before starting your FPP.  Alternatively, Disable this check by unselecting the 'Check Ethernet Link' setting on the [LED Panels](../../channeloutputs.php#tab-LEDPanels)
output page. Note: This will not stop this warning being present, but it will stop FPP from deliberately disabling the Colorlight output.

At boot the network port can still be connecting when FPP starts (a few seconds on a Raspberry Pi 4). With 'Check Ethernet Link' unselected, FPP checks the link again 10 seconds later and clears this warning if the link is up by then. A link that comes up later than that leaves the warning showing until the LED Panels output is saved again or FPPD is restarted.

With 'Check Ethernet Link' selected, the warning ends in "(output disabled ...)": the Colorlight output stays off, even once the link comes up, until the LED Panels output is saved again or FPPD is restarted. If you see this after a reboot while the link light is on, unselect 'Check Ethernet Link'.

If the interface goes down while FPP is running (a USB network adapter unplugged or reset, the interface taken down), the warning reads "Unable to send on interface ...". The other outputs keep running, and the warning clears by itself as soon as frames reach the interface again.
