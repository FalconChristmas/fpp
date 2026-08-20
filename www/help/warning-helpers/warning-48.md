# Schedule Location Not Set
A schedule uses a dawn/dusk (sunrise/sunset) time, but this device's **Latitude/Longitude** are unset or are not plain decimal numbers (`38.938524` / `-104.600945` - no `N`/`W` letters, no degree symbols). FPP fell back to default coordinates (Falcon, Colorado), so those schedules will trigger at the wrong times for your location.

1. Open the [Localization settings](../../settings.php#settings-localization) and fill in **Latitude** and **Longitude** for this device's location, then save.
2. Re-check your dawn/dusk schedule times on the [Scheduler](../../scheduler.php) page.

Once a valid latitude and longitude are saved, sunrise/sunset schedules will use this site's real times.
