#Received bridging data while sequence is running. 

This is unexpected, and likely the sign of a configuration problem. 

Something is sending E1.31, DDP or Artnet data to FPP right now, whilst FPP is running a sequence.  It could be xLights test mode, xLights output to lights, another FPP or perhaps other software you are using.

Check that you do not have this controller configured as a UDP output on any other FPP devices.  If you do, xLights may have configured that for you.  If this FPP is a remote, double check that this controller is set to 'xLights Only' in Xlights and not 'Active'.  If you change this, you will have to manually remove the offending entries, because xLights/FPP Connect only adds them and never removes them.
