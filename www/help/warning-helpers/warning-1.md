# FPPD Is Not Running

The FPPD executable (the operational brain of FPP) is not currently running. This can have several causes:

1. The `fppd` file may be missing, which can happen after a failed upgrade.
2. Check the `fppd.log` file. If FPPD crashes shortly after each restart, the log should show the error.
3. xLights may be configured with the wrong controller type — for example selecting an F series instead of a K series controller (or vice versa). To fix, configure xLights correctly, then open the [System](/settings.php#settings-system) settings page and use **Reset FPP Config** to reset the Channel Outputs, then re-upload from xLights. The **Reset FPP Config** button is only shown when **User Interface Level** (near the top of the System page) is set to **Advanced** or higher.
