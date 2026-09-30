<?

/**
 * Commands that fppd no longer has, and what replaced each one.  A playlist
 * saved on an older FPP still carries these names; fppd skips the entry at
 * play time with nothing but a transient warning, so validation calls them
 * out by name.
 */
function removedFPPCommands()
{
    return array(
        "Remote Trigger Command Preset" => "Trigger Command Preset",
        "Remote Trigger Command Preset Slot" => "Trigger Command Preset Slot",
        "Remote Effect Start" => "Effect Start",
        "Remote FSEQ Effect Start" => "FSEQ Effect Start",
        "Remote Effect Stop" => "Effect Stop",
        "Remote Playlist Start" => "Start Playlist",
        "Remote Run Script" => "Run Script",
    );
}

/**
 * Builds the lookup state shared by every playlist checked in one validation
 * pass: the known playlists, the command descriptions from fppd, and the
 * command preset names.
 *
 * `commands` is null when fppd could not be reached; command entries are then
 * not checked rather than all being reported as unknown.
 *
 * @return array Validation context, passed by reference to the validators.
 */
function validationContext()
{
    global $settings;

    $playlists = array();
    if ($d = opendir($settings['playlistDirectory'])) {
        while (($file = readdir($d)) !== false) {
            if (preg_match('/\.json$/', $file)) {
                $playlists[preg_replace('/\.json$/', '', $file)] = true;
            }
        }
        closedir($d);
    }

    $commands = null;
    $curl = curl_init('http://127.0.0.1:32322/commands');
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 1000);
    curl_setopt($curl, CURLOPT_TIMEOUT_MS, 3000);
    $body = curl_exec($curl);
    $status = curl_getinfo($curl, CURLINFO_HTTP_CODE);
    curl_close($curl);
    if ($body !== false && $status == 200) {
        $list = json_decode($body, true);
        if (is_array($list)) {
            $commands = array();
            foreach ($list as $c) {
                if (isset($c['name'])) {
                    $commands[$c['name']] = $c;
                }
            }
        }
    }

    $presets = array();
    $presetFile = $settings['configDirectory'] . '/commandPresets.json';
    if (file_exists($presetFile)) {
        $data = json_decode(file_get_contents($presetFile), true);
        if (isset($data['commands']) && is_array($data['commands'])) {
            foreach ($data['commands'] as $p) {
                if (isset($p['name'])) {
                    $presets[$p['name']] = true;
                }
            }
        }
    }

    return array(
        'playlists' => $playlists,
        'commands' => $commands,
        'presets' => $presets,
        'results' => array(), // playlist name => validation result, memoized
        'active' => array(),  // playlists being validated, for cycle detection
    );
}

/**
 * True when $name is a file (or, with $allowDir, a directory) under $dir.
 * Names may carry a sub-directory, as sequences in sub-folders do.
 */
function validationFileExists($dir, $name, $allowDir = false)
{
    if ($name === '') {
        return false;
    }
    $path = (substr($name, 0, 1) == '/') ? $name : $dir . '/' . $name;
    return is_file($path) || ($allowDir && is_dir($path));
}

/**
 * Media is looked up the way fppd does: an absolute path as given, otherwise
 * the music directory and then the video directory.
 */
function validationMediaExists($name)
{
    global $settings;
    if (substr($name, 0, 1) == '/') {
        return is_file($name);
    }
    return validationFileExists($settings['musicDirectory'], $name) ||
        validationFileExists($settings['videoDirectory'], $name);
}

/**
 * Checks a stored FPP command (a playlist "command" entry, a schedule entry,
 * a command preset, a GPIO action, ...) and its arguments, the way fppd will
 * run it on this host.
 *
 * @param array $cmd  Holds `command`, `args` and optionally `multisyncCommand`/`multisyncHosts`.
 * @param array $ctx  Validation context from validationContext().
 * @return array Problem descriptions; empty when the command looks runnable.
 */
function validateFPPCommand($cmd, &$ctx)
{
    global $settings;
    $rc = array();

    $name = isset($cmd['command']) ? $cmd['command'] : '';
    if ($name === '') {
        return array("No command selected");
    }

    // A multisync command sent to named hosts runs on those hosts, which have
    // their own commands and files.  One with no hosts is sent to every host
    // and run here too, so it is checked like any other.
    if (!empty($cmd['multisyncCommand']) && isset($cmd['multisyncHosts']) && $cmd['multisyncHosts'] !== '') {
        return $rc;
    }
    if ($ctx['commands'] === null) {
        return $rc;
    }

    if (!isset($ctx['commands'][$name])) {
        $removed = removedFPPCommands();
        if (isset($removed[$name])) {
            $rc[] = "Command '$name' was removed in FPP 10; use '" . $removed[$name] .
                "' with the Multisync option instead";
        } else {
            $rc[] = "Unknown command '$name' (it may come from a plugin that is not installed)";
        }
        return $rc;
    }

    $argDefs = isset($ctx['commands'][$name]['args']) ? $ctx['commands'][$name]['args'] : array();
    $args = (isset($cmd['args']) && is_array($cmd['args'])) ? array_values($cmd['args']) : array();
    for ($i = 0; $i < count($argDefs) && $i < count($args); $i++) {
        $def = $argDefs[$i];
        // Arguments after a subcommand depend on which subcommand was chosen.
        if (isset($def['type']) && $def['type'] == 'subcommand') {
            break;
        }
        if (!is_scalar($args[$i])) {
            continue;
        }
        // "If" and friends carry their commands as a JSON list in one argument.
        if (isset($def['type']) && $def['type'] == 'commandlist') {
            $list = json_decode((string) $args[$i], true);
            if (is_array($list)) {
                $label = isset($def['description']) ? $def['description'] : $def['name'];
                foreach (array_values($list) as $n => $sub) {
                    if (is_array($sub) && isset($sub['command']) && $sub['command'] !== '') {
                        foreach (validateFPPCommand($sub, $ctx) as $msg) {
                            $rc[] = "Command '$name', $label #" . ($n + 1) . ": $msg";
                        }
                    }
                }
            }
            continue;
        }
        if (!isset($def['contentListUrl'])) {
            continue;
        }
        $value = (string) $args[$i];
        // Empty means "not set"; %...% is substituted when the command runs.
        if ($value === '' || strpos($value, '%') !== false) {
            continue;
        }
        $label = isset($def['description']) ? $def['description'] : $def['name'];
        $missing = false;
        switch ($def['contentListUrl']) {
            case 'api/playlists':
                $missing = !isset($ctx['playlists'][$value]);
                break;
            case 'api/playlists/playable':
                $missing = !isset($ctx['playlists'][$value]) &&
                    !(preg_match('/\.fseq$/i', $value) && validationFileExists($settings['sequenceDirectory'], $value));
                break;
            case 'api/sequence':
                $missing = !validationFileExists($settings['sequenceDirectory'], $value) &&
                    !validationFileExists($settings['sequenceDirectory'], $value . '.fseq');
                break;
            case 'api/effects':
                $missing = !validationFileExists($settings['effectDirectory'], $value) &&
                    !validationFileExists($settings['effectDirectory'], $value . '.eseq');
                break;
            case 'api/media':
                $missing = !validationMediaExists($value);
                break;
            case 'api/scripts':
                $missing = !validationFileExists($settings['scriptDirectory'], $value);
                break;
            case 'api/commandPresets?names=true':
                $missing = !isset($ctx['presets'][$value]);
                break;
        }
        if ($missing) {
            $rc[] = "Command '$name': $label '$value' not found";
        }
    }
    return $rc;
}


/**
 * Validate FPP commands
 *
 * Checks a list of FPP commands the way fppd will run them on this host: the
 * command must exist, and arguments that name a playlist, sequence, media
 * file, effect, script or command preset must name one that exists.  Lists
 * of commands inside an argument (the "If" command) are checked too.  A
 * multisync command sent to named hosts is not checked, since it runs there.
 * If fppd cannot be reached, nothing is reported.
 *
 * @route POST /api/validate/commands
 * @body [{"command": "Start Playlist", "args": ["Show", "false", "false"], "multisyncCommand": false, "multisyncHosts": ""}]
 * @response 200 One list of problems per command, in the order given
 * ```json
 * [["Command 'Start Playlist': Playlist Name 'Show' not found"]]
 * ```
 */
function validate_commands()
{
    $cmds = json_decode(file_get_contents('php://input'), true);
    if (!is_array($cmds)) {
        halt(400, json(array("status" => "error", "message" => "Expected a JSON array of commands")));
    }
    $ctx = validationContext();
    $rc = array();
    foreach (array_values($cmds) as $cmd) {
        $rc[] = (is_array($cmd) && isset($cmd['command']) && $cmd['command'] !== '') ? validateFPPCommand($cmd, $ctx) : array();
    }
    return json($rc);
}
