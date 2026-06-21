# Pixel String Cape Configuration References Missing Hardware
FPP has a pixel string (or DPI) output configured, but the matching cape or hat was not detected.

This commonly happens after a cape or hat that contains an EEPROM (for example a Falcon PiCap) is
physically removed.  The string configuration it created is left behind on disk, so FPP keeps
trying to drive the output that is no longer present.

To clear the leftover configuration, open the [System](../../settings.php#settings-system) settings page.
The **Reset FPP Config** button is only shown when **User Interface Level** (near the top of that
page) is set to **Advanced** or higher, so change it if you are in **Basic** mode.  Then click
**Reset FPP Config**, tick **EEPROM / String Config**, and press **Reset**.  FPP will reboot.

If the cape or hat should still be present, instead check that it is firmly seated and powered, then
reboot.  If it is correctly installed but still isn't detected, its EEPROM may be corrupt or outdated —
**contact your cape/hat vendor for an updated EEPROM image.**
