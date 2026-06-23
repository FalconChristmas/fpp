# Duplicate System UUID Detected

FPP has detected another device on the network advertising the **same System UUID** as one already known to this instance. Each FPP device must have a unique UUID — it is how FPP Connect, xLights, MultiSync, and statistics tell devices apart. When two devices share a UUID, tools like xLights treat them as a single device and one will appear to be missing or unavailable.

This is rare. It happens when two devices were built from the same OS image and both ended up with the same underlying identifier instead of a unique one.

## How to fix it

Re-image the SD card of **one** of the conflicting devices with a fresh FPP OS image:

1. Back up the device's configuration first (**Content Setup → File Manager / Backup**, or the **Backup** page).
2. Write a **fresh FPP OS image** to the SD card (e.g. with the Raspberry Pi Imager or `dd`). A fresh image generates a new, unique identifier on first boot, which resolves the collision.
3. Restore your backup onto the freshly imaged card.

> **Important:** Use a *fresh image write* — **not** an in-place "FPP OS Upgrade" or reinstall. The OS upgrade path intentionally preserves the existing `machine-id`, so it will **not** change the UUID or clear the duplicate.

You only need to do this on one of the two devices; the other can stay as it is.
