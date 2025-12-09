# Sending data to myself
Check that you do not have this controller configured as a UDP output here:
[channeloutputs.php](Channel Outputs)

This is often caused by this controller being set as 'Active' in xLights instead of 'xLights Only' and uploading UDP outputs to FPP with FPP Connect.

You will have to manually remove the offending entry, because even if you correct the settings in xLights, FPP Connect won't delete the entries.
