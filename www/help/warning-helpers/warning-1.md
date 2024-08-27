# FPPD Not Running

The FPPD executable (the operational brain) is not currently running... this could be the result of several things...

1. Check if fppd file exist -- If not, may have been failed upgrade
2. Check fppd.log file.  If quickly crashing after restart, try and flag error
3. Configuring xLights with the wrong type of controller, typically  selecting an F instead of K series controller (or vice versa)  or one of  the other similar controllers.
   Fix-Configure xLights correctly  and go to FPP Settings>System and reset Channels, re-upload from xLights
