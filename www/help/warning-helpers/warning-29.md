# Schedule Will Not Run In Remote Mode
A schedule entry is enabled, but this device is running in **Remote** mode rather than **Player** mode. Schedules only run in Player mode, so the entry is being skipped.

1. If this device should run its own schedule, set **FPPD Mode** to **Player** on the [System](../../settings.php#settings-system) settings page.
2. If this device is intentionally a remote (it plays sequences sent by a master), the schedule belongs on the **master** instead — disable or remove the schedule entry here.
