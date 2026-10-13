# Configuration Entry Skipped
A configuration file in FPP's `config` folder has an entry FPP could not read, usually a value of the wrong type: a number saved as text (`"1"` instead of `1`), `true` written as `"true"`, a list where a single value belongs, or an empty value. FPP skipped that one entry and loaded the rest.

The warning names the file and what was wrong with the entry.

1. If you edited the file by hand, or a script or another program wrote it, correct the value (or remove the entry) in the file named in the warning.
2. If the file is managed by an FPP page (Channel Inputs, GPIO Inputs, Recurring Tasks, Sensors, Audio Groups), open that page and save it again to rewrite the file in the expected form.
3. Restart FPPD, or save the page again, to load the corrected entry.

This warning clears by itself an hour after the file was last read with the bad entry in it.
