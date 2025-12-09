# Colorlight Link Speed
The Network link between FPP and your colorlight receiver is not running at 1000Mbps.  This could be caused by a faulty cable.  Check the cable and restart FPPD.

Colorlight receivers require a 1000Mbps connection.  If you are running a strange setup eg 100Mbps between FPP and a switch, then 1000Mbps between the switch and the Colorlight receiver, you can disable this check by unselecting the 'Check Ethernet Link' setting on the [channeloutputs.php#tab-LEDPanels](LED Panels) output page.  Note: This will not stop this warning being present, but it will stop FPP from deliberately disabling the Colorlight output. 
