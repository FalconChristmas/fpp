# System Clock Not Set
This device's clock is set to a date/time before the FPP release date, so FPP has **delayed starting the scheduler** until the time is corrected. Until then, scheduled playlists will not run at the right time (or at all).

This normally happens on devices **without a real-time clock (RTC)** that boot before they can sync time from the network.

1. Make sure the device has a working network connection so it can reach an NTP time server.
2. Check the time zone and time settings on the [Localization settings](../../settings.php#settings-localization) page.
3. If this device is frequently offline, consider adding a hardware RTC module.

This warning clears automatically once the clock is corrected (FPP detects the time jump and starts the scheduler).
