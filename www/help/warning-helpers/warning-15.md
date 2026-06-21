# Raspberry Pi Voltage Too Low
The Pi's firmware reported an under-voltage condition. This causes instability, dropped frames, freezes, SD card corruption, and corrupt pixel output.

**When this appears:** FPP polls the Pi's power/throttle status periodically while idle. The warning is only raised after the under-voltage condition is seen on **more than 5 consecutive checks**, so a momentary dip (for example a large inrush when outputs first turn on) will not trigger it — it indicates a sustained power problem.

**It will not clear on its own:** once raised, this warning stays until FPP (`fppd`) is restarted, even if the voltage later recovers. After fixing the power problem below, restart FPP to confirm the warning does not return.

How to fix:

1. Use a quality power supply rated for your Pi model (5.1V, adequate amperage — e.g. the official supply for your board).
2. Replace thin or long USB power cables — they drop voltage under load and are the most common cause.
3. Do not power pixels, relays, or other peripherals from the Pi's 5V rail; use a dedicated supply for them.
4. Check for a loose or worn power connector at the Pi.
5. To confirm it at the hardware level, open **Help → [Troubleshooting Commands](../../troubleshooting.php)** and run **RPI Utils → RPI vclog util** (or **RPI Info**); look for under-voltage / throttled messages.
