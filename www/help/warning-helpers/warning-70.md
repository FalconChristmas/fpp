# Script Outside the Scripts Directory
FPP was asked to run a script whose name leads outside the scripts folder (it contains a `..` path component), and refused. Only scripts in the scripts folder can be run, whether from a playlist Script entry, a dynamic playlist, or the **Run Script** command.

1. Check `fppd.log` for the exact script name that was refused.
2. If it came from a playlist or command preset, pick the script again from the list in the [Playlists](../../playlists.php) editor or the command editor.
3. If it came from a dynamic playlist (a file, URL, or plugin), fix the data that source returns so `scriptName` is a plain file name in the scripts folder.
4. Upload the script to the scripts folder with the [File Manager](../../filemanager.php) if it is not there.

This warning clears automatically after a short time once nothing asks for that script again.
