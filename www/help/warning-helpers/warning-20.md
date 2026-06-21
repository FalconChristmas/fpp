# Unable To Start PRU - Reboot Required
FPP could not start the BeagleBone PRU used to drive this output. The output will not work until the PRU starts.

1. Reboot the device — this clears most transient PRU start failures.
2. If it persists after reboot, confirm the correct cape/string config is selected for your hardware.
3. Check `fppd.log` for the underlying PRU error.
