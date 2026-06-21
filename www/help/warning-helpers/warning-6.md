# Sending data to myself
Check that you do not have this controller configured as a UDP output here:
[Channel Outputs](../../channeloutputs.php)

If a cape or hat is installed, the network/UDP outputs are hidden while **User Interface Level** is
set to **Basic**.  Set it to **Advanced** on the [System](../../settings.php#settings-system) settings
page so the UDP outputs are shown.

This is often caused by this controller being set as 'Active' in xLights instead of 'xLights Only' and uploading UDP outputs to FPP with FPP Connect.

You will have to manually remove the offending entry, because even if you correct the settings in xLights, FPP Connect won't delete the entries.
