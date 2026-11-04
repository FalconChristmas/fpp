# Plugin Modified FPP's Files
A plugin changed files inside FPP's own installation (`/opt/fpp`) while it was installing or upgrading. The message names the plugin. Plugins are supposed to keep to their own directory; editing FPP's files stops FPP updates from applying cleanly, and it means crash reports describe code that is not what FPP shipped.

**To clear it:** go to **Help → [About](../../about.php)** and run an FPP update (the **Update FPP** button on the warning goes there). The update sets the plugin's edits aside before it applies, so the update itself is not blocked. The edits are not thrown away: a copy is saved as a patch in the `logs/fpp_tree_patches` folder, and they are also in `git stash list` inside `/opt/fpp`. The warning goes away within a couple of minutes of the update.

Setting the edits aside undoes what the plugin changed, so the plugin may stop working correctly. If it does, **report it to the plugin's author** and include the patch. `logs/fpp_tree_changes.log` records which plugin changed which files, and when.

If the warning comes back after the next install or upgrade of that plugin, the plugin is still making the edits.
