<?

/**
 * Get effects
 *
 * Returns a list of effect (`*.eseq`) files available in the effects directory.
 *
 * @route-v1 GET /effects
 * @route-v2 GET /effects
 * @response 200 List of effect filenames
 * ```json
 * ["rainbow", "twinkle"]
 * ```
 */
function effects_list()
{
    global $effectDirectory;

    $effects = array();
    if ($d = opendir($effectDirectory)) {
        while (($file = readdir($d)) !== false) {
            if (preg_match('/\.eseq$/', $file)) {
                $file = preg_replace('/\.eseq$/', '', $file);
                array_push($effects, $file);
            }
        }
        closedir($d);
    }
    sort($effects);
    return json($effects);
}

/**
 * Get all effects
 *
 * Returns a combined list of all effect (`*.eseq`) files from both the effects directory
 * and the sequences directory.
 *
 * @route-v1 GET /effects/ALL
 * @route-v2 GET /effects/ALL
 * @response 200 Combined list of effect and sequence filenames
 * ```json
 * ["rainbow", "twinkle", "MySequence"]
 * ```
 */
function effects_list_ALL()
{
    global $effectDirectory;
    global $sequenceDirectory;

    $effects = array();
    if ($d = opendir($effectDirectory)) {
        while (($file = readdir($d)) !== false) {
            if (preg_match('/\.eseq$/', $file)) {
                $file = preg_replace('/\.eseq$/', '', $file);
                array_push($effects, $file);
            }
        }
        closedir($d);
    }
    if ($d = opendir($sequenceDirectory)) {
        while (($file = readdir($d)) !== false) {
            if (preg_match('/\.fseq$/', $file)) {
                $file = preg_replace('/\.fseq$/', '', $file);
                array_push($effects, $file);
            }
        }
        closedir($d);
    }
    sort($effects);
    return json($effects);
}
?>
