# Duplicate System UUID

Another device on your network is advertising the same System UUID as this one,
and this device could not replace its own. FPP Connect and xLights use the UUID
to tell controllers apart, so with two devices sharing one, one of them can show
as unavailable.

## Why FPP could not fix it automatically

FPP tries first: when a duplicate is spotted, it asks for a new UUID and, if the
UUID is generated rather than hardware-derived, replaces it and carries on. That
did not work here because this device's UUID comes from hardware that FPP will
not rewrite — a CPU or board serial, or an OS identity that is not FPP's to
change.

## What to do

Set **Override UUID** on one of the two devices:

1. FPP Settings → Advanced → **Override UUID**.
2. Enter any value you like — it only has to differ from the other device's. A
   UUID with the dashes removed, or any string of 6 or more letters and digits,
   is fine.
3. Restart FPPD, or reboot, so the new value is used everywhere.
4. In xLights, refresh the FPP Connect list so the controller is seen under its
   new UUID.

This warning clears itself once no device on the network advertises this UUID.

## Why duplicates happen

The usual cause is two devices built from the same SD card image, or a card
cloned after it had already booted: they inherit the same OS-level identity and
report it as their UUID. Restoring one device's backup onto another can do the
same.

Re-imaging a card from a fresh FPP image gives it a new identity on first boot.
An in-place OS upgrade deliberately keeps the existing identity, so it will not
clear a duplicate that is already there.
