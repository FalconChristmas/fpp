<?
/////////////////////////////////////////////////////////////////////////////
// Audio Card Aliases — user-defined friendly names for sound cards.
//
// Aliases are keyed on the ALSA card ID (e.g. "S3", "ICUSBAUDIO7D") which
// is stable across reboots and USB hot-plug ordering. They are surfaced in
// every audio card selector in both Hardware Direct (ALSA) and PipeWire
// media backends. See issue #2586.
//
// Storage: $configDirectory/audio-card-aliases.json
//   { "aliases": { "<cardId>": "<alias>", ... } }
/////////////////////////////////////////////////////////////////////////////

function GetAudioCardAliasesFile()
{
    global $configDirectory;
    return $configDirectory . "/audio-card-aliases.json";
}

// Returns associative array mapping ALSA card ID -> user alias.
// Empty array if no aliases configured.
function LoadAudioCardAliases()
{
    $file = GetAudioCardAliasesFile();
    if (!file_exists($file)) {
        return array();
    }
    $contents = @file_get_contents($file);
    if ($contents === false || $contents === '') {
        return array();
    }
    $data = json_decode($contents, true);
    if (!is_array($data) || !isset($data['aliases']) || !is_array($data['aliases'])) {
        return array();
    }
    $out = array();
    foreach ($data['aliases'] as $cardId => $alias) {
        $cardId = trim((string) $cardId);
        $alias = trim((string) $alias);
        if ($cardId !== '' && $alias !== '') {
            $out[$cardId] = $alias;
        }
    }
    return $out;
}

// Returns a label suitable for display in a select option.
// If an alias exists for $cardId, returns "<alias> (<original>)".
// Otherwise returns $original unchanged.
function ApplyAudioCardAlias($cardId, $original, $aliases = null)
{
    if ($aliases === null) {
        $aliases = LoadAudioCardAliases();
    }
    if ($cardId !== '' && isset($aliases[$cardId]) && $aliases[$cardId] !== '') {
        return $aliases[$cardId] . ' (' . $original . ')';
    }
    return $original;
}

/////////////////////////////////////////////////////////////////////////////
// Enumerate all detected sound cards so the modal can present them.
// Returns: [{ cardNum, cardId, cardName, alias }, ...]
function GetAudioCardAliases()
{
    global $SUDO, $settings;

    $aliases = LoadAudioCardAliases();
    $cards = array();

    if (isset($settings["Platform"]) && $settings["Platform"] == "MacOS") {
        // macOS: list audio devices via system_profiler
        exec("system_profiler SPAudioDataType", $output);
        $curCard = "";
        foreach ($output as $line) {
            $tline = trim($line);
            if ($tline != "" && $tline != "Audio:" && $tline != "Device:") {
                if (str_ends_with($tline, ":")) {
                    $curCard = rtrim($tline, ":");
                } else {
                    $values = explode(':', $tline);
                    $key = trim($values[0]);
                    if ($key == "Output Source" && $curCard != "") {
                        $cards[] = array(
                            "cardNum" => -1,
                            "cardId" => $curCard,
                            "cardName" => $curCard,
                            "alias" => isset($aliases[$curCard]) ? $aliases[$curCard] : ""
                        );
                    }
                }
            }
        }
        return json(array("cards" => $cards, "aliases" => $aliases));
    }

    // Linux: parse /proc/asound/cards for stable ALSA card IDs and names.
    // Format: "  N [ID             ]: <driver> - <longname>"
    $cardIdMap = array(); // cardNum -> cardId
    $cardNameMap = array(); // cardNum -> long name
    $cardsFile = @file_get_contents('/proc/asound/cards');
    if ($cardsFile) {
        $lines = explode("\n", $cardsFile);
        $lastNum = -1;
        foreach ($lines as $line) {
            if (preg_match('/^\s*(\d+)\s*\[([^\]]+)\]\s*:\s*(.*)$/', $line, $m)) {
                $lastNum = intval($m[1]);
                $cardIdMap[$lastNum] = trim($m[2]);
                $cardNameMap[$lastNum] = trim($m[3]);
            } elseif ($lastNum >= 0 && trim($line) !== '') {
                // Continuation line — usually contains the human-friendly long name
                if (!isset($cardNameMap[$lastNum]) || $cardNameMap[$lastNum] === '') {
                    $cardNameMap[$lastNum] = trim($line);
                } else {
                    // Prefer the second line which typically has the longer description
                    $longer = trim($line);
                    if (strlen($longer) > strlen($cardNameMap[$lastNum])) {
                        $cardNameMap[$lastNum] = $longer;
                    }
                }
            }
        }
    }

    // Also pull short bracketed names from `aplay -l` so the modal shows the
    // same display names users see elsewhere.
    $shortNameMap = array(); // cardNum -> short bracketed name
    exec($SUDO . " aplay -l 2>/dev/null | grep '^card'", $aplayOut);
    foreach ($aplayOut as $line) {
        if (preg_match('/^card\s+(\d+):\s*[^[]*\[([^\]]+)\]/', $line, $m)) {
            $cn = intval($m[1]);
            if (!isset($shortNameMap[$cn])) {
                $shortNameMap[$cn] = trim($m[2]);
            }
        }
    }

    foreach ($cardIdMap as $cardNum => $cardId) {
        // Only include cards that have PCM playback devices — same filter used by
        // other audio dropdowns (aplay -l). MIDI-only and other non-audio USB
        // devices appear in /proc/asound/cards but not in aplay -l output.
        if (!isset($shortNameMap[$cardNum])) {
            continue;
        }
        $name = isset($shortNameMap[$cardNum]) ? $shortNameMap[$cardNum] : '';
        if ($name === '' && isset($cardNameMap[$cardNum])) {
            $name = $cardNameMap[$cardNum];
        }
        if ($name === '') {
            $name = $cardId;
        }
        $cards[] = array(
            "cardNum" => $cardNum,
            "cardId" => $cardId,
            "cardName" => $name,
            "alias" => isset($aliases[$cardId]) ? $aliases[$cardId] : ""
        );
    }

    // Include any aliases for cards not currently detected (so users can see
    // and clean up stale entries).
    $seenIds = array();
    foreach ($cards as $c) {
        $seenIds[$c['cardId']] = true;
    }
    foreach ($aliases as $cardId => $alias) {
        if (!isset($seenIds[$cardId])) {
            $cards[] = array(
                "cardNum" => -1,
                "cardId" => $cardId,
                "cardName" => "(not currently connected)",
                "alias" => $alias
            );
        }
    }

    return json(array("cards" => $cards, "aliases" => $aliases));
}

/////////////////////////////////////////////////////////////////////////////
// POST body: { "aliases": { "<cardId>": "<alias>", ... } }
// Empty alias values remove the entry. Returns { "status": "ok" } on success.
function SaveAudioCardAliases()
{
    $raw = file_get_contents('php://input');
    $data = json_decode($raw, true);
    if (!is_array($data)) {
        halt(400, json(array("status" => "error", "message" => "Invalid JSON body")));
        return;
    }
    $aliases = isset($data['aliases']) && is_array($data['aliases']) ? $data['aliases'] : array();

    $clean = array();
    foreach ($aliases as $cardId => $alias) {
        $cardId = trim((string) $cardId);
        $alias = trim((string) $alias);
        // Reject empty card IDs and excessively long values
        if ($cardId === '' || strlen($cardId) > 128) {
            continue;
        }
        if (strlen($alias) > 128) {
            $alias = substr($alias, 0, 128);
        }
        if ($alias === '') {
            continue; // empty alias removes the entry
        }
        $clean[$cardId] = $alias;
    }

    $file = GetAudioCardAliasesFile();
    $dir = dirname($file);
    if (!is_dir($dir)) {
        @mkdir($dir, 0755, true);
    }

    $payload = json_encode(array("aliases" => $clean), JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
    if (@file_put_contents($file, $payload) === false) {
        halt(500, json(array("status" => "error", "message" => "Failed to write alias file")));
        return;
    }

    return json(array("status" => "ok", "count" => count($clean)));
}
