# Could Not Load Plugin
FPP had a problem loading and initializing the named plugin, so its features are unavailable.

The most common cause is an **FPP upgrade**: a plugin built for the previous FPP version is not automatically rebuilt, so it fails to load on the new version.

1. Open the [Plugin Manager](../../plugins.php) (under Content Setup) and **update/reinstall** the plugin so it is rebuilt for this FPP version.
2. If updating does not help, **uninstall and reinstall** the plugin.
3. Check `fppd.log` for the specific load/initialization error from the plugin.
4. Confirm the plugin is still maintained and compatible with your FPP version; a plugin abandoned by its author may not work on newer releases.
