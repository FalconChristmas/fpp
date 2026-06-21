# Effect Could Not Be Started
FPP tried to play an effect but the effect file could not be opened or is not valid, so the effect did nothing. The message names the file and the problem:

- **Unable to open effect** — the `.eseq`/`.fseq` file is missing or could not be read.
- **Effect file not a correct eseq file** / **must have at least one model range** — the file is not a valid effect sequence.

1. Confirm the effect file exists in the effects/sequences folder (see the [File Manager](../../filemanager.php)).
2. If the effect was created in xLights, re-export it and copy it back to FPP.
3. Check `fppd.log` for the exact file name that failed.

This warning clears automatically after a short time once the problem stops recurring.
