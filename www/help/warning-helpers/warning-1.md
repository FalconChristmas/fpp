# FPPD Has Crashed
The FPPD process (the operational brain of FPP) crashed. FPP normally restarts it automatically, but the crash was recorded so it can be investigated. You may also see this if FPPD is not currently running at all.

**Find the crash report:** open the [File Manager](../../filemanager.php) and select the **Crash Reports** tab. Each crash is saved there as a `.zip` (in `<media>/crashes/`) with passwords redacted, and is helpful when asking for support. The `fppd.log` will also show the error leading up to the crash.

**Make sure the crash report is submitted:** when FPP catches a crash it automatically uploads the report to the FPP developers (keyed to this device's **System UUID**) so it can be diagnosed. For it to succeed:

1. The device must have **working internet access** at the time of the crash. If it was offline, the report is still saved locally — open the **Crash Reports** tab in the [File Manager](../../filemanager.php), download the `.zip`, and share it manually with a developer.
2. Uploads are rate-limited — if several crashes happen in quick succession, only the first is sent automatically; grab the rest from the Crash Reports tab if needed.
3. For more useful reports, set **Share Statistics** to **Enabled** on the [System](../../settings.php#settings-system) settings page — when enabled, anonymous hardware/feature stats are bundled with the crash report to give the developers extra context.
4. When asking for help, reference the **crash report file name** (it contains your platform, FPP version, and System UUID) so the developers can locate your uploaded report.

Common causes:

1. **A repeating crash loop** — if FPPD crashes shortly after each restart, the crash report and `fppd.log` should point at the cause (a bad output config, a plugin, or a sequence/media file).
2. **Video output crash** — on the Pi, a video thread can crash (SIGBUS / DRM-KMS buffer issue). Audio and pixel output continue, but video may be unavailable until restart; check your video output settings and source files.
3. **Wrong controller type in xLights** — e.g. selecting an F series instead of a K series controller (or vice versa). Configure xLights correctly, then open the [System](../../settings.php#settings-system) settings page and use **Reset FPP Config** to reset the Channel Outputs, then re-upload from xLights. The **Reset FPP Config** button is only shown when **User Interface Level** (near the top of the System page) is set to **Advanced** or higher.
4. **Missing or corrupt `fppd` binary** — can happen after a failed upgrade; re-run the FPP upgrade/install.

If crashes persist, attach the crash report when reporting the issue to the FPP community.
