# Pixel String Cape Configuration References Missing Hardware
FPP has a pixel string (or DPI) output configured, but the matching cape or hat was not detected.

This commonly happens after a cape or hat that contains an EEPROM (for example a Falcon PiCap) is
physically removed.  The string configuration it created is left behind on disk, so FPP keeps
trying to drive the output that is no longer present.

To clear the leftover configuration, open the [settings.php#settings-system](System) settings page,
click **Reset FPP Config**, tick **EEPROM / String Config**, and press **Reset**.  FPP will reboot.

If the cape or hat should still be present, instead check that it is firmly seated and powered, then
reboot.
