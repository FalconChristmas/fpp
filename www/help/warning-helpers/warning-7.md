# Colorlight Link Down
The Network link between FPP and your colorlight receiver is down.

Ensure your Colorlight receivers are plugged in and powered on before starting your FPP.  Alternatively, Disable this check by unselecting the 'Check Ethernet Link' setting on the [LED Panels](../../channeloutputs.php#tab-LEDPanels)
output page. Note: This will not stop this warning being present, but it will stop FPP from deliberately disabling the Colorlight output.

If the interface goes down while FPP is running (a USB network adapter unplugged or reset, the interface taken down), the warning reads "Unable to send on interface ...". The other outputs keep running, and the warning clears by itself as soon as frames reach the interface again.
