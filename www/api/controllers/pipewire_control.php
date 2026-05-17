<?php
/////////////////////////////////////////////////////////////////////////////
// PipeWire Control Facade — 3rd-party REST API
//
// A clean, stable, ID-addressed control layer over the PipeWire audio
// backend. Where the existing /api/pipewire/audio/* endpoints are coupled
// to the UI (they require callers to know internal PipeWire node names and
// only return the saved config), these /api/pipewire/control/* endpoints:
//
//   * Address resources by friendly IDs (group id, ALSA card id, stream
//     slot, input-group id) — callers never need internal node names.
//   * Return live runtime state (actual volume / mute / running) queried
//     from PipeWire at request time, not just the saved config.
//   * Persist changes back into the config JSON so they survive a reboot
//     (boot-time restore reads group['volume'] / member['volume']).
//
// Endpoint map:
//   GET  /api/pipewire/control/status
//   GET  /api/pipewire/control/groups
//   GET  /api/pipewire/control/groups/:id
//   POST /api/pipewire/control/groups/:id/volume               { "volume": 0-150 }
//   POST /api/pipewire/control/groups/:id/mute                 { "mute": bool } | { "toggle": true }
//   POST /api/pipewire/control/groups/:id/members/:cardId/volume   { "volume": 0-150 }
//   POST /api/pipewire/control/groups/:id/members/:cardId/mute     { "mute": bool } | { "toggle": true }
//   GET  /api/pipewire/control/input-groups
//   GET  /api/pipewire/control/input-groups/:id
//   POST /api/pipewire/control/input-groups/:id/members/:memberIndex/volume  { "volume": 0-100 }
//   POST /api/pipewire/control/input-groups/:id/members/:memberIndex/mute    { "mute": bool } | { "toggle": true }
//   GET  /api/pipewire/control/streams
//   POST /api/pipewire/control/streams/:slot/volume            { "volume": 0-100 }
//   GET  /api/pipewire/control/routing
//   POST /api/pipewire/control/routing/:inputGroupId/:outputGroupId/volume  { "volume": 0-100 }
//   POST /api/pipewire/control/routing/:inputGroupId/:outputGroupId/mute    { "mute": bool } | { "toggle": true }
//
// Volume semantics:
//   * Output groups + their member sound cards are real PipeWire sinks
//     (fpp_group_*, fpp_fx_g*). Their volume/mute is read LIVE via pactl
//     and is reported with "volumeSource": "live".
//   * Input-group members, stream slots and routing paths are controlled
//     via pw-cli channelmix on loopback / combine-stream internal nodes,
//     which cannot be read back reliably. These report the persisted
//     value with "volumeSource": "config" plus a "running" flag.
/////////////////////////////////////////////////////////////////////////////

require_once '../commandsocket.php';

/////////////////////////////////////////////////////////////////////////////
// Internal helpers
/////////////////////////////////////////////////////////////////////////////

function pwctl_env()
{
    return "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp PULSE_RUNTIME_PATH=/run/pipewire-fpp/pulse";
}

// Normalise a name/id the same way the config generator does when it builds
// PipeWire node names. Must stay in sync with GeneratePipeWireGroupsConfig /
// RestorePipeWireGroupVolumes in pipewire.php.
function pwctl_norm($s)
{
    return preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($s));
}

function pwctl_group_node_name($group)
{
    $name = isset($group['name']) ? $group['name'] : 'Group';
    return 'fpp_group_' . pwctl_norm($name);
}

function pwctl_member_fx_node_name($groupId, $cardId)
{
    return 'fpp_fx_g' . intval($groupId) . '_' . pwctl_norm($cardId);
}

function pwctl_output_groups_file()
{
    global $settings;
    return $settings['mediaDirectory'] . "/config/pipewire-audio-groups.json";
}

function pwctl_input_groups_file()
{
    global $settings;
    return $settings['mediaDirectory'] . "/config/pipewire-input-groups.json";
}

function pwctl_load_output_groups()
{
    $f = pwctl_output_groups_file();
    if (!file_exists($f))
        return array('groups' => array());
    $d = json_decode(file_get_contents($f), true);
    if (!is_array($d) || !isset($d['groups']))
        return array('groups' => array());
    return $d;
}

function pwctl_load_input_groups()
{
    $f = pwctl_input_groups_file();
    if (!file_exists($f))
        return array('inputGroups' => array());
    $d = json_decode(file_get_contents($f), true);
    if (!is_array($d) || !isset($d['inputGroups']))
        return array('inputGroups' => array());
    return $d;
}

function pwctl_backend_mode()
{
    global $settings;
    $b = isset($settings['MediaBackend']) ? $settings['MediaBackend'] : '';
    if ($b === '') {
        // Fall back to the settings file when not exported to PHP $settings.
        $sf = isset($settings['settingsFile']) ? $settings['settingsFile'] : '/home/fpp/media/settings';
        if (file_exists($sf)) {
            $txt = file_get_contents($sf);
            if (preg_match('/^MediaBackend\s*=\s*"?([^"\n]+)"?/m', $txt, $m))
                $b = trim($m[1]);
        }
    }
    return $b;
}

function pwctl_is_pipewire()
{
    $b = pwctl_backend_mode();
    return ($b === 'pipewire' || $b === 'pipewire-simple');
}

// Parse `pactl list sinks` into a map keyed by node name:
//   name => array(index, state, mute(bool), volume(int %), description)
function pwctl_live_sinks()
{
    global $SUDO;
    $sinks = array();
    $raw = shell_exec($SUDO . " " . pwctl_env() . " pactl list sinks 2>/dev/null");
    if (empty($raw))
        return $sinks;

    // Split into per-sink blocks on "Sink #N" lines.
    $blocks = preg_split('/^Sink #\d+/m', $raw, -1, PREG_SPLIT_NO_EMPTY);
    foreach ($blocks as $block) {
        if (!preg_match('/^\s*Name:\s*(.+)$/m', $block, $nm))
            continue;
        $name = trim($nm[1]);
        $state = '';
        if (preg_match('/^\s*State:\s*(\S+)/m', $block, $sm))
            $state = trim($sm[1]);
        $mute = false;
        if (preg_match('/^\s*Mute:\s*(\w+)/m', $block, $mm))
            $mute = (strtolower(trim($mm[1])) === 'yes');
        $vol = null;
        // Volume: front-left: 42951 /  66% / -10.50 dB, ...  → take first %
        if (preg_match('/^\s*Volume:[^\n]*?(\d+)%/m', $block, $vm))
            $vol = intval($vm[1]);
        $desc = '';
        if (preg_match('/^\s*Description:\s*(.+)$/m', $block, $dm))
            $desc = trim($dm[1]);

        $sinks[$name] = array(
            'state' => $state,
            'mute' => $mute,
            'volume' => $vol,
            'description' => $desc,
        );
    }
    return $sinks;
}

// Return the set of node.name values currently present in the PipeWire graph.
function pwctl_live_node_names()
{
    global $SUDO;
    $names = array();
    $raw = shell_exec($SUDO . " " . pwctl_env() . " pw-dump 2>/dev/null");
    if (empty($raw))
        return $names;
    $objs = json_decode($raw, true);
    if (!is_array($objs))
        return $names;
    foreach ($objs as $obj) {
        if (!isset($obj['type']) || $obj['type'] !== 'PipeWire:Interface:Node')
            continue;
        $props = isset($obj['info']['props']) ? $obj['info']['props'] : array();
        if (isset($props['node.name']))
            $names[$props['node.name']] = true;
    }
    return $names;
}

function pwctl_clamp($v, $min, $max)
{
    $v = intval($v);
    if ($v < $min)
        return $min;
    if ($v > $max)
        return $max;
    return $v;
}

function pwctl_body()
{
    $b = json_decode(file_get_contents('php://input'), true);
    return is_array($b) ? $b : array();
}

// Resolve a desired mute value from the request body, supporting either an
// explicit { "mute": bool } or { "toggle": true } against $current.
function pwctl_resolve_mute($body, $current)
{
    if (isset($body['toggle']) && $body['toggle'])
        return !$current;
    if (isset($body['mute']))
        return (bool) $body['mute'];
    return null;
}

function pwctl_set_sink_volume($node, $pct)
{
    global $SUDO;
    $out = array();
    $ret = 0;
    exec($SUDO . " " . pwctl_env() . " pactl set-sink-volume " . escapeshellarg($node) . " " . intval($pct) . "% 2>&1", $out, $ret);
    return $ret === 0;
}

function pwctl_set_sink_mute($node, $mute)
{
    global $SUDO;
    $out = array();
    $ret = 0;
    exec($SUDO . " " . pwctl_env() . " pactl set-sink-mute " . escapeshellarg($node) . " " . ($mute ? "1" : "0") . " 2>&1", $out, $ret);
    return $ret === 0;
}

// Set channelmix.volume on every PipeWire node whose node.name matches one of
// $nodeNames (loopback modules expose input.X / output.X sub-nodes, so the
// caller passes all acceptable names). Returns true if at least one node was
// updated.
function pwctl_set_channelmix_volume($nodeNames, $linear)
{
    global $SUDO;
    $env = pwctl_env();
    $raw = shell_exec($SUDO . " " . $env . " pw-dump 2>/dev/null");
    if (empty($raw))
        return false;
    $objs = json_decode($raw, true);
    if (!is_array($objs))
        return false;
    $ok = false;
    foreach ($objs as $obj) {
        if (!isset($obj['type']) || $obj['type'] !== 'PipeWire:Interface:Node')
            continue;
        $props = isset($obj['info']['props']) ? $obj['info']['props'] : array();
        $nm = isset($props['node.name']) ? $props['node.name'] : '';
        if (in_array($nm, $nodeNames, true)) {
            $o = shell_exec($SUDO . " " . $env . " pw-cli set-param " . intval($obj['id']) . " Props '{ channelmix.volume: " . floatval($linear) . " }' 2>&1");
            if (strpos((string) $o, 'Error') === false)
                $ok = true;
        }
    }
    return $ok;
}

function pwctl_persist_output_groups($data)
{
    file_put_contents(pwctl_output_groups_file(), json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));
}

function pwctl_persist_input_groups($data)
{
    file_put_contents(pwctl_input_groups_file(), json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));
}

function pwctl_not_pipewire_response()
{
    http_response_code(409);
    return json(array(
        "status" => "ERROR",
        "message" => "PipeWire backend is not active (MediaBackend=" . pwctl_backend_mode() . ")"
    ));
}

/////////////////////////////////////////////////////////////////////////////
/**
 * PipeWire control status
 *
 * Reports whether the PipeWire backend is active, the systemd service health
 * of the PipeWire/WirePlumber/pulse units, and configured group counts. Use
 * this to discover capability before issuing control calls.
 *
 * @route GET /api/pipewire/control/status
 * @response 200 PipeWire backend status
 * ```json
 * {"status":"OK","backend":"pipewire","pipewireActive":true,"simpleMode":false,"services":{"fpp-pipewire":"active","fpp-wireplumber":"active","fpp-pipewire-pulse":"active"},"outputGroupCount":2,"outputGroupsEnabled":2,"inputGroupCount":1,"inputGroupsEnabled":1}
 * ```
 */
function PWCtl_GetStatus()
{
    global $SUDO;

    $mode = pwctl_backend_mode();
    $isPw = ($mode === 'pipewire' || $mode === 'pipewire-simple');

    $services = array();
    foreach (array('fpp-pipewire', 'fpp-wireplumber', 'fpp-pipewire-pulse') as $svc) {
        $st = trim((string) shell_exec($SUDO . " /usr/bin/systemctl is-active " . escapeshellarg($svc . ".service") . " 2>/dev/null"));
        $services[$svc] = ($st === '') ? 'unknown' : $st;
    }

    $og = pwctl_load_output_groups();
    $ig = pwctl_load_input_groups();

    $enabledOut = 0;
    foreach ($og['groups'] as $g) {
        if (isset($g['enabled']) && $g['enabled'])
            $enabledOut++;
    }
    $enabledIn = 0;
    foreach ($ig['inputGroups'] as $g) {
        if (isset($g['enabled']) && $g['enabled'])
            $enabledIn++;
    }

    return json(array(
        "status" => "OK",
        "backend" => $mode,
        "pipewireActive" => $isPw,
        "simpleMode" => ($mode === 'pipewire-simple'),
        "services" => $services,
        "outputGroupCount" => count($og['groups']),
        "outputGroupsEnabled" => $enabledOut,
        "inputGroupCount" => count($ig['inputGroups']),
        "inputGroupsEnabled" => $enabledIn,
    ));
}

/////////////////////////////////////////////////////////////////////////////
// Output groups
/////////////////////////////////////////////////////////////////////////////

function pwctl_build_group_view($group, $liveSinks)
{
    $groupId = isset($group['id']) ? intval($group['id']) : 0;
    $node = pwctl_group_node_name($group);
    $live = isset($liveSinks[$node]) ? $liveSinks[$node] : null;

    $members = array();
    if (isset($group['members']) && is_array($group['members'])) {
        foreach ($group['members'] as $m) {
            $cardId = isset($m['cardId']) ? $m['cardId'] : '';
            $fxNode = pwctl_member_fx_node_name($groupId, $cardId);
            $mLive = isset($liveSinks[$fxNode]) ? $liveSinks[$fxNode] : null;
            $members[] = array(
                'cardId' => $cardId,
                'channels' => isset($m['channels']) ? intval($m['channels']) : 2,
                'nodeName' => $fxNode,
                'configVolume' => isset($m['volume']) ? intval($m['volume']) : 100,
                'configMute' => isset($m['mute']) ? (bool) $m['mute'] : false,
                'liveVolume' => $mLive ? $mLive['volume'] : null,
                'liveMute' => $mLive ? $mLive['mute'] : null,
                'running' => $mLive !== null,
                'volumeSource' => $mLive ? 'live' : 'config',
            );
        }
    }

    return array(
        'id' => $groupId,
        'name' => isset($group['name']) ? $group['name'] : ('Group ' . $groupId),
        'enabled' => isset($group['enabled']) ? (bool) $group['enabled'] : false,
        'channels' => isset($group['channels']) ? intval($group['channels']) : 2,
        'nodeName' => $node,
        'configVolume' => isset($group['volume']) ? intval($group['volume']) : 100,
        'configMute' => isset($group['mute']) ? (bool) $group['mute'] : false,
        'liveVolume' => $live ? $live['volume'] : null,
        'liveMute' => $live ? $live['mute'] : null,
        'running' => $live !== null,
        'state' => $live ? $live['state'] : 'not-running',
        'volumeSource' => $live ? 'live' : 'config',
        'members' => $members,
    );
}

/**
 * List output groups with live state
 *
 * Returns every configured audio output group together with its member
 * sound cards. Volume/mute for groups and member cards are read live from
 * PipeWire (`volumeSource: "live"` when the sink is running, else the saved
 * config value with `volumeSource: "config"`).
 *
 * @route GET /api/pipewire/control/groups
 * @response 200 Output groups with live runtime state
 * ```json
 * {"status":"OK","groups":[{"id":1,"name":"Front","enabled":true,"channels":2,"nodeName":"fpp_group_front","configVolume":100,"configMute":false,"liveVolume":80,"liveMute":false,"running":true,"state":"RUNNING","volumeSource":"live","members":[{"cardId":"S3","channels":2,"nodeName":"fpp_fx_g1_s3","configVolume":100,"liveVolume":75,"liveMute":false,"running":true,"volumeSource":"live"}]}]}
 * ```
 */
function PWCtl_GetGroups()
{
    $data = pwctl_load_output_groups();
    $liveSinks = pwctl_live_sinks();
    $out = array();
    foreach ($data['groups'] as $g) {
        $out[] = pwctl_build_group_view($g, $liveSinks);
    }
    return json(array("status" => "OK", "groups" => $out));
}

/**
 * Get one output group with live state
 *
 * Returns a single audio output group (by numeric group id) with its member
 * sound cards and live runtime volume/mute.
 *
 * @route GET /api/pipewire/control/groups/{id}
 * @response 200 Output group with live runtime state
 * ```json
 * {"status":"OK","group":{"id":1,"name":"Front","enabled":true,"nodeName":"fpp_group_front","liveVolume":80,"liveMute":false,"running":true,"members":[]}}
 * ```
 * @response 404 Output group not found
 */
function PWCtl_GetGroup()
{
    $id = intval(params('id'));
    $data = pwctl_load_output_groups();
    foreach ($data['groups'] as $g) {
        if ((isset($g['id']) ? intval($g['id']) : 0) === $id) {
            return json(array("status" => "OK", "group" => pwctl_build_group_view($g, pwctl_live_sinks())));
        }
    }
    http_response_code(404);
    return json(array("status" => "ERROR", "message" => "Output group $id not found"));
}

/**
 * Set output group volume
 *
 * Sets the master volume (0-150%) of an audio output group's combined
 * PipeWire sink. The value is applied live and persisted to the group
 * config so it survives a reboot.
 *
 * @route POST /api/pipewire/control/groups/{id}/volume
 * @body {"volume": 80}
 * @response 200 Volume applied and persisted
 * ```json
 * {"status":"OK","groupId":1,"nodeName":"fpp_group_front","volume":80,"applied":true,"message":"Volume set to 80%"}
 * ```
 * @response 400 Missing 'volume'
 * @response 404 Output group not found
 * @response 409 PipeWire backend not active
 */
function PWCtl_SetGroupVolume()
{
    if (!pwctl_is_pipewire())
        return pwctl_not_pipewire_response();

    $id = intval(params('id'));
    $body = pwctl_body();
    if (!isset($body['volume'])) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Missing 'volume'"));
    }
    $vol = pwctl_clamp($body['volume'], 0, 150);

    $data = pwctl_load_output_groups();
    $found = false;
    $node = '';
    foreach ($data['groups'] as &$g) {
        if ((isset($g['id']) ? intval($g['id']) : 0) === $id) {
            $found = true;
            $node = pwctl_group_node_name($g);
            $g['volume'] = $vol;
            break;
        }
    }
    unset($g);
    if (!$found) {
        http_response_code(404);
        return json(array("status" => "ERROR", "message" => "Output group $id not found"));
    }

    $applied = pwctl_set_sink_volume($node, $vol);
    pwctl_persist_output_groups($data);

    return json(array(
        "status" => "OK",
        "groupId" => $id,
        "nodeName" => $node,
        "volume" => $vol,
        "applied" => $applied,
        "message" => $applied ? "Volume set to {$vol}%" : "Volume saved; sink not running (will apply on next start)",
    ));
}

/**
 * Mute or unmute an output group
 *
 * Sets the mute state of an output group's combined sink. Provide an
 * explicit `mute` boolean, or `toggle: true` to flip the current state.
 * Applied live and persisted.
 *
 * @route POST /api/pipewire/control/groups/{id}/mute
 * @body {"mute": true}
 * @response 200 Mute state applied and persisted
 * ```json
 * {"status":"OK","groupId":1,"nodeName":"fpp_group_front","mute":true,"applied":true}
 * ```
 * @response 400 Provide 'mute' (bool) or 'toggle': true
 * @response 404 Output group not found
 * @response 409 PipeWire backend not active
 */
function PWCtl_SetGroupMute()
{
    if (!pwctl_is_pipewire())
        return pwctl_not_pipewire_response();

    $id = intval(params('id'));
    $body = pwctl_body();

    $data = pwctl_load_output_groups();
    $found = false;
    $node = '';
    $cur = false;
    foreach ($data['groups'] as &$g) {
        if ((isset($g['id']) ? intval($g['id']) : 0) === $id) {
            $found = true;
            $node = pwctl_group_node_name($g);
            $cur = isset($g['mute']) ? (bool) $g['mute'] : false;
            break;
        }
    }
    unset($g);
    if (!$found) {
        http_response_code(404);
        return json(array("status" => "ERROR", "message" => "Output group $id not found"));
    }

    $mute = pwctl_resolve_mute($body, $cur);
    if ($mute === null) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Provide 'mute' (bool) or 'toggle': true"));
    }

    foreach ($data['groups'] as &$g) {
        if ((isset($g['id']) ? intval($g['id']) : 0) === $id) {
            $g['mute'] = $mute;
            break;
        }
    }
    unset($g);

    $applied = pwctl_set_sink_mute($node, $mute);
    pwctl_persist_output_groups($data);

    return json(array(
        "status" => "OK",
        "groupId" => $id,
        "nodeName" => $node,
        "mute" => $mute,
        "applied" => $applied,
    ));
}

/**
 * Set member sound-card volume
 *
 * Sets the volume (0-150%) of an individual sound card within an output
 * group, addressed by its stable ALSA card id (e.g. `S3`). Applied live to
 * the member filter-chain sink and persisted.
 *
 * @route POST /api/pipewire/control/groups/{id}/members/{cardId}/volume
 * @body {"volume": 75}
 * @response 200 Member volume applied and persisted
 * ```json
 * {"status":"OK","groupId":1,"cardId":"S3","nodeName":"fpp_fx_g1_s3","volume":75,"applied":true,"message":"Volume set to 75%"}
 * ```
 * @response 400 Missing 'volume'
 * @response 404 Card not found in group
 * @response 409 PipeWire backend not active
 */
function PWCtl_SetMemberVolume()
{
    if (!pwctl_is_pipewire())
        return pwctl_not_pipewire_response();

    $id = intval(params('id'));
    $cardId = params('cardId');
    $body = pwctl_body();
    if (!isset($body['volume'])) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Missing 'volume'"));
    }
    $vol = pwctl_clamp($body['volume'], 0, 150);

    $data = pwctl_load_output_groups();
    $found = false;
    foreach ($data['groups'] as &$g) {
        if ((isset($g['id']) ? intval($g['id']) : 0) !== $id)
            continue;
        if (!isset($g['members']))
            continue;
        foreach ($g['members'] as &$m) {
            if ((isset($m['cardId']) ? $m['cardId'] : '') === $cardId) {
                $found = true;
                $m['volume'] = $vol;
                break;
            }
        }
        unset($m);
        break;
    }
    unset($g);
    if (!$found) {
        http_response_code(404);
        return json(array("status" => "ERROR", "message" => "Card '$cardId' not found in group $id"));
    }

    $node = pwctl_member_fx_node_name($id, $cardId);
    $applied = pwctl_set_sink_volume($node, $vol);
    pwctl_persist_output_groups($data);

    return json(array(
        "status" => "OK",
        "groupId" => $id,
        "cardId" => $cardId,
        "nodeName" => $node,
        "volume" => $vol,
        "applied" => $applied,
        "message" => $applied ? "Volume set to {$vol}%" : "Volume saved; member sink not running (EQ/filter chain may be inactive)",
    ));
}

/**
 * Mute or unmute a member sound card
 *
 * Sets the mute state of an individual sound card within an output group,
 * addressed by its ALSA card id. Provide `mute` (bool) or `toggle: true`.
 * Applied live and persisted.
 *
 * @route POST /api/pipewire/control/groups/{id}/members/{cardId}/mute
 * @body {"toggle": true}
 * @response 200 Member mute state applied and persisted
 * ```json
 * {"status":"OK","groupId":1,"cardId":"S3","nodeName":"fpp_fx_g1_s3","mute":true,"applied":true}
 * ```
 * @response 400 Provide 'mute' (bool) or 'toggle': true
 * @response 404 Card not found in group
 * @response 409 PipeWire backend not active
 */
function PWCtl_SetMemberMute()
{
    if (!pwctl_is_pipewire())
        return pwctl_not_pipewire_response();

    $id = intval(params('id'));
    $cardId = params('cardId');
    $body = pwctl_body();

    $data = pwctl_load_output_groups();
    $found = false;
    $cur = false;
    foreach ($data['groups'] as &$g) {
        if ((isset($g['id']) ? intval($g['id']) : 0) !== $id)
            continue;
        if (!isset($g['members']))
            continue;
        foreach ($g['members'] as &$m) {
            if ((isset($m['cardId']) ? $m['cardId'] : '') === $cardId) {
                $found = true;
                $cur = isset($m['mute']) ? (bool) $m['mute'] : false;
                break;
            }
        }
        unset($m);
        break;
    }
    unset($g);
    if (!$found) {
        http_response_code(404);
        return json(array("status" => "ERROR", "message" => "Card '$cardId' not found in group $id"));
    }

    $mute = pwctl_resolve_mute($body, $cur);
    if ($mute === null) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Provide 'mute' (bool) or 'toggle': true"));
    }

    foreach ($data['groups'] as &$g) {
        if ((isset($g['id']) ? intval($g['id']) : 0) !== $id)
            continue;
        foreach ($g['members'] as &$m) {
            if ((isset($m['cardId']) ? $m['cardId'] : '') === $cardId) {
                $m['mute'] = $mute;
                break;
            }
        }
        unset($m);
        break;
    }
    unset($g);

    $node = pwctl_member_fx_node_name($id, $cardId);
    $applied = pwctl_set_sink_mute($node, $mute);
    pwctl_persist_output_groups($data);

    return json(array(
        "status" => "OK",
        "groupId" => $id,
        "cardId" => $cardId,
        "nodeName" => $node,
        "mute" => $mute,
        "applied" => $applied,
    ));
}

/////////////////////////////////////////////////////////////////////////////
// Input groups (mix buses)
/////////////////////////////////////////////////////////////////////////////

function pwctl_input_member_loopback_names($groupId, $member)
{
    $type = isset($member['type']) ? $member['type'] : '';
    if ($type === 'fppd_stream') {
        $sourceId = isset($member['sourceId']) ? $member['sourceId'] : 'fppd_stream_1';
        $slug = pwctl_norm($sourceId);
        $base = "fpp_loopback_ig{$groupId}_{$slug}";
    } else {
        $mbrName = isset($member['name']) ? $member['name'] : 'Member';
        $base = "fpp_loopback_ig{$groupId}_" . pwctl_norm($mbrName);
    }
    return array($base, "input.$base", "output.$base");
}

function pwctl_build_input_group_view($ig, $liveNodes)
{
    $igId = isset($ig['id']) ? intval($ig['id']) : 0;
    $members = array();
    if (isset($ig['members']) && is_array($ig['members'])) {
        foreach ($ig['members'] as $idx => $m) {
            $names = pwctl_input_member_loopback_names($igId, $m);
            $running = false;
            foreach ($names as $n) {
                if (isset($liveNodes[$n])) {
                    $running = true;
                    break;
                }
            }
            $members[] = array(
                'index' => $idx,
                'type' => isset($m['type']) ? $m['type'] : '',
                'sourceId' => isset($m['sourceId']) ? $m['sourceId'] : '',
                'name' => isset($m['name']) ? $m['name'] : ('Member ' . $idx),
                'configVolume' => isset($m['volume']) ? intval($m['volume']) : 100,
                'configMute' => isset($m['mute']) ? (bool) $m['mute'] : false,
                'running' => $running,
                'volumeSource' => 'config',
            );
        }
    }
    return array(
        'id' => $igId,
        'name' => isset($ig['name']) ? $ig['name'] : ('Input ' . $igId),
        'enabled' => isset($ig['enabled']) ? (bool) $ig['enabled'] : false,
        'channels' => isset($ig['channels']) ? intval($ig['channels']) : 2,
        'outputs' => isset($ig['outputs']) ? array_values($ig['outputs']) : array(),
        'members' => $members,
    );
}

/**
 * List input groups (mix buses)
 *
 * Returns every configured input group (mix bus) with its members and the
 * output groups it routes to. Member volume/mute reflect the saved config
 * (`volumeSource: "config"`) plus a live `running` flag indicating whether
 * the loopback node is currently active.
 *
 * @route GET /api/pipewire/control/input-groups
 * @response 200 Input groups with member state
 * ```json
 * {"status":"OK","inputGroups":[{"id":1,"name":"Main Mix","enabled":true,"channels":2,"outputs":[1,2],"members":[{"index":0,"type":"fppd_stream","sourceId":"fppd_stream_1","name":"FPP Media","configVolume":100,"configMute":false,"running":true,"volumeSource":"config"}]}]}
 * ```
 */
function PWCtl_GetInputGroups()
{
    $data = pwctl_load_input_groups();
    $liveNodes = pwctl_live_node_names();
    $out = array();
    foreach ($data['inputGroups'] as $ig) {
        $out[] = pwctl_build_input_group_view($ig, $liveNodes);
    }
    return json(array("status" => "OK", "inputGroups" => $out));
}

/**
 * Get one input group (mix bus)
 *
 * Returns a single input group by numeric id with its members and routing
 * targets.
 *
 * @route GET /api/pipewire/control/input-groups/{id}
 * @response 200 Input group with member state
 * ```json
 * {"status":"OK","inputGroup":{"id":1,"name":"Main Mix","enabled":true,"channels":2,"outputs":[1],"members":[]}}
 * ```
 * @response 404 Input group not found
 */
function PWCtl_GetInputGroup()
{
    $id = intval(params('id'));
    $data = pwctl_load_input_groups();
    foreach ($data['inputGroups'] as $ig) {
        if ((isset($ig['id']) ? intval($ig['id']) : 0) === $id) {
            return json(array("status" => "OK", "inputGroup" => pwctl_build_input_group_view($ig, pwctl_live_node_names())));
        }
    }
    http_response_code(404);
    return json(array("status" => "ERROR", "message" => "Input group $id not found"));
}

// Shared resolver for input-group member by index.
function pwctl_find_input_member(&$data, $id, $memberIndex)
{
    foreach ($data['inputGroups'] as &$ig) {
        if ((isset($ig['id']) ? intval($ig['id']) : 0) === $id) {
            if (isset($ig['members'][$memberIndex])) {
                return array(&$ig, &$ig['members'][$memberIndex]);
            }
            return null;
        }
    }
    return null;
}

/**
 * Set input-group member volume
 *
 * Sets the volume (0-100%) of a member within an input group (mix bus),
 * addressed by its zero-based member index. Applied live to the member
 * loopback via channelmix and persisted. Note: the primary fppd stream's
 * volume is governed by fppd itself, not a loopback.
 *
 * @route POST /api/pipewire/control/input-groups/{id}/members/{memberIndex}/volume
 * @body {"volume": 60}
 * @response 200 Member volume applied and persisted
 * ```json
 * {"status":"OK","inputGroupId":1,"memberIndex":0,"volume":60,"applied":true,"message":"Volume set to 60%"}
 * ```
 * @response 400 Missing 'volume'
 * @response 404 Member not found in input group
 * @response 409 PipeWire backend not active
 */
function PWCtl_SetInputMemberVolume()
{
    if (!pwctl_is_pipewire())
        return pwctl_not_pipewire_response();

    $id = intval(params('id'));
    $memberIndex = intval(params('memberIndex'));
    $body = pwctl_body();
    if (!isset($body['volume'])) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Missing 'volume'"));
    }
    $vol = pwctl_clamp($body['volume'], 0, 100);
    $linear = round($vol / 100.0, 3);

    $data = pwctl_load_input_groups();
    $ref = pwctl_find_input_member($data, $id, $memberIndex);
    if ($ref === null) {
        http_response_code(404);
        return json(array("status" => "ERROR", "message" => "Member $memberIndex not found in input group $id"));
    }
    $ig = &$ref[0];
    $member = &$ref[1];

    $type = isset($member['type']) ? $member['type'] : '';
    $names = pwctl_input_member_loopback_names($id, $member);
    $applied = pwctl_set_channelmix_volume($names, $linear);

    $member['volume'] = $vol;
    $member['mute'] = false;
    pwctl_persist_input_groups($data);

    $msg = $applied
        ? "Volume set to {$vol}%"
        : (($type === 'fppd_stream')
            ? "Volume saved; primary fppd stream volume is governed by fppd, not a loopback"
            : "Volume saved; loopback not running (Apply input groups to activate)");

    return json(array(
        "status" => "OK",
        "inputGroupId" => $id,
        "memberIndex" => $memberIndex,
        "volume" => $vol,
        "applied" => $applied,
        "message" => $msg,
    ));
}

/**
 * Mute or unmute an input-group member
 *
 * Sets the mute state of a member within an input group, addressed by
 * zero-based member index. Loopback nodes have no mute property, so mute is
 * implemented by driving channelmix volume to 0 and unmute restores the
 * saved member volume. Provide `mute` (bool) or `toggle: true`.
 *
 * @route POST /api/pipewire/control/input-groups/{id}/members/{memberIndex}/mute
 * @body {"mute": true}
 * @response 200 Member mute state applied and persisted
 * ```json
 * {"status":"OK","inputGroupId":1,"memberIndex":0,"mute":true,"applied":true}
 * ```
 * @response 400 Provide 'mute' (bool) or 'toggle': true
 * @response 404 Member not found in input group
 * @response 409 PipeWire backend not active
 */
function PWCtl_SetInputMemberMute()
{
    if (!pwctl_is_pipewire())
        return pwctl_not_pipewire_response();

    $id = intval(params('id'));
    $memberIndex = intval(params('memberIndex'));
    $body = pwctl_body();

    $data = pwctl_load_input_groups();
    $ref = pwctl_find_input_member($data, $id, $memberIndex);
    if ($ref === null) {
        http_response_code(404);
        return json(array("status" => "ERROR", "message" => "Member $memberIndex not found in input group $id"));
    }
    $ig = &$ref[0];
    $member = &$ref[1];

    $cur = isset($member['mute']) ? (bool) $member['mute'] : false;
    $mute = pwctl_resolve_mute($body, $cur);
    if ($mute === null) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Provide 'mute' (bool) or 'toggle': true"));
    }

    // Loopback nodes have no mute prop — mute by driving channelmix.volume to
    // 0, unmute by restoring the saved member volume.
    $names = pwctl_input_member_loopback_names($id, $member);
    $savedVol = isset($member['volume']) ? intval($member['volume']) : 100;
    $linear = $mute ? 0.0 : round($savedVol / 100.0, 3);
    $applied = pwctl_set_channelmix_volume($names, $linear);

    $member['mute'] = $mute;
    pwctl_persist_input_groups($data);

    return json(array(
        "status" => "OK",
        "inputGroupId" => $id,
        "memberIndex" => $memberIndex,
        "mute" => $mute,
        "applied" => $applied,
    ));
}

/////////////////////////////////////////////////////////////////////////////
// Stream slots
/////////////////////////////////////////////////////////////////////////////

/**
 * List fppd stream slots
 *
 * Returns the status of all 5 fppd media stream slots (idle/playing). Slot 1
 * additionally reports the currently playing media filename and timing from
 * fppd.
 *
 * @route GET /api/pipewire/control/streams
 * @response 200 Stream slot status
 * ```json
 * {"status":"OK","streams":[{"slot":1,"nodeName":"fppd_stream_1","status":"playing","mediaFilename":"show.mp4","secondsElapsed":12,"secondsRemaining":48},{"slot":2,"nodeName":"fppd_stream_2","status":"idle","mediaFilename":""}]}
 * ```
 */
function PWCtl_GetStreams()
{
    global $SUDO;

    $ctx = stream_context_create(array('http' => array('timeout' => 3)));
    $raw = @file_get_contents('http://127.0.0.1:32322/api/fppd/status', false, $ctx);
    $fpp = (!empty($raw)) ? json_decode($raw, true) : array();

    $liveNodes = pwctl_live_node_names();

    $slots = array();
    for ($slot = 1; $slot <= 5; $slot++) {
        $node = "fppd_stream_$slot";
        $running = isset($liveNodes[$node]);
        $info = array(
            "slot" => $slot,
            "nodeName" => $node,
            "status" => $running ? "playing" : "idle",
            "mediaFilename" => "",
        );
        if ($slot === 1 && is_array($fpp) && !empty($fpp['media_filename'])) {
            $info['status'] = 'playing';
            $info['mediaFilename'] = $fpp['media_filename'];
            if (isset($fpp['seconds_elapsed']))
                $info['secondsElapsed'] = intval($fpp['seconds_elapsed']);
            if (isset($fpp['seconds_remaining']))
                $info['secondsRemaining'] = intval($fpp['seconds_remaining']);
        }
        $slots[] = $info;
    }
    return json(array("status" => "OK", "streams" => $slots));
}

/**
 * Set fppd stream slot volume
 *
 * Sets the volume (0-100%) of an fppd media stream slot (1-5). Slot 1 uses
 * fppd's built-in volume control; slots 2-5 are set live via PipeWire
 * channelmix on the stream node. The response `control` field indicates
 * which path was used.
 *
 * @route POST /api/pipewire/control/streams/{slot}/volume
 * @body {"volume": 90}
 * @response 200 Stream volume applied
 * ```json
 * {"status":"OK","slot":1,"volume":90,"applied":true,"control":"fppd"}
 * ```
 * @response 400 Missing 'volume'
 * @response 409 PipeWire backend not active
 */
function PWCtl_SetStreamVolume()
{
    if (!pwctl_is_pipewire())
        return pwctl_not_pipewire_response();

    $slot = pwctl_clamp(params('slot'), 1, 5);
    $body = pwctl_body();
    if (!isset($body['volume'])) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Missing 'volume'"));
    }
    $vol = pwctl_clamp($body['volume'], 0, 100);

    if ($slot === 1) {
        // Slot 1 is fppd's primary playback — use fppd's own volume control.
        $ctx = stream_context_create(array('http' => array(
            'method' => 'POST',
            'header' => 'Content-Type: application/json',
            'content' => json_encode(array('command' => 'Volume Set', 'args' => array(strval($vol)))),
            'timeout' => 3,
        )));
        @file_get_contents('http://127.0.0.1:32322/api/command', false, $ctx);
        return json(array("status" => "OK", "slot" => 1, "volume" => $vol, "applied" => true, "control" => "fppd"));
    }

    $linear = round($vol / 100.0, 3);
    $applied = pwctl_set_channelmix_volume(array("fppd_stream_$slot"), $linear);
    return json(array(
        "status" => $applied ? "OK" : "ERROR",
        "slot" => $slot,
        "volume" => $vol,
        "applied" => $applied,
        "control" => "pipewire",
        "message" => $applied ? "Volume set to {$vol}%" : "Stream slot $slot not active",
    ));
}

/////////////////////////////////////////////////////////////////////////////
// Routing matrix (input group → output group paths)
/////////////////////////////////////////////////////////////////////////////

/**
 * Get the routing matrix
 *
 * Returns the full input-group to output-group routing matrix. For every
 * input group, each possible output-group path is listed with its
 * connected state, per-path volume and mute. Values reflect the saved
 * config (`volumeSource: "config"`).
 *
 * @route GET /api/pipewire/control/routing
 * @response 200 Routing matrix
 * ```json
 * {"status":"OK","volumeSource":"config","matrix":[{"inputGroupId":1,"inputGroupName":"Main Mix","enabled":true,"paths":[{"outputGroupId":1,"outputGroupName":"Front","connected":true,"volume":100,"mute":false},{"outputGroupId":2,"outputGroupName":"Rear","connected":false,"volume":75,"mute":false}]}]}
 * ```
 */
function PWCtl_GetRouting()
{
    $ig = pwctl_load_input_groups();
    $og = pwctl_load_output_groups();

    $ogNames = array();
    foreach ($og['groups'] as $g) {
        $ogNames[intval($g['id'])] = isset($g['name']) ? $g['name'] : ('Group ' . $g['id']);
    }

    $matrix = array();
    foreach ($ig['inputGroups'] as $g) {
        $igId = isset($g['id']) ? intval($g['id']) : 0;
        $outputs = isset($g['outputs']) ? $g['outputs'] : array();
        $routing = isset($g['routing']) ? $g['routing'] : array();
        $paths = array();
        foreach ($ogNames as $ogId => $ogName) {
            $key = strval($ogId);
            $paths[] = array(
                'outputGroupId' => $ogId,
                'outputGroupName' => $ogName,
                'connected' => in_array($ogId, $outputs),
                'volume' => isset($routing[$key]['volume']) ? intval($routing[$key]['volume']) : 100,
                'mute' => isset($routing[$key]['mute']) ? (bool) $routing[$key]['mute'] : false,
            );
        }
        $matrix[] = array(
            'inputGroupId' => $igId,
            'inputGroupName' => isset($g['name']) ? $g['name'] : ('Input ' . $igId),
            'enabled' => isset($g['enabled']) ? (bool) $g['enabled'] : false,
            'paths' => $paths,
        );
    }
    return json(array("status" => "OK", "volumeSource" => "config", "matrix" => $matrix));
}

// Resolve the internal combine-stream node names that carry input group $igId
// to output group $ogId, and apply $linear via channelmix on each.
function pwctl_apply_routing_volume($igData, $ogData, $igId, $ogId, $linear)
{
    global $SUDO;

    $igName = '';
    $hasEffects = false;
    foreach ($igData['inputGroups'] as $ig) {
        if ((isset($ig['id']) ? intval($ig['id']) : 0) === $igId) {
            $igName = isset($ig['name']) ? $ig['name'] : '';
            $hasEffects = isset($ig['effects']['eq']['enabled']) && $ig['effects']['eq']['enabled']
                && isset($ig['effects']['eq']['bands']) && !empty($ig['effects']['eq']['bands']);
            break;
        }
    }
    $ogNode = '';
    foreach ($ogData['groups'] as $og) {
        if ((isset($og['id']) ? intval($og['id']) : 0) === $ogId) {
            $ogNode = 'fpp_group_' . pwctl_norm(isset($og['name']) ? $og['name'] : 'Group');
            break;
        }
    }
    if ($ogNode === '')
        return false;

    $routingNode = $hasEffects ? "fpp_route_ig_$igId" : ("fpp_input_" . pwctl_norm($igName));

    $raw = shell_exec($SUDO . " " . pwctl_env() . " pw-dump 2>/dev/null");
    if (empty($raw))
        return false;
    $objs = json_decode($raw, true);
    if (!is_array($objs))
        return false;

    $ok = false;
    foreach ($objs as $obj) {
        if (!isset($obj['type']) || $obj['type'] !== 'PipeWire:Interface:Node')
            continue;
        $props = isset($obj['info']['props']) ? $obj['info']['props'] : array();
        $nm = isset($props['node.name']) ? $props['node.name'] : '';
        $target = isset($props['node.target']) ? $props['node.target'] : '';
        if (($nm === $routingNode || strpos($nm, $routingNode . '.') === 0) && $target === $ogNode) {
            $o = shell_exec($SUDO . " " . pwctl_env() . " pw-cli set-param " . intval($obj['id']) . " Props '{ channelmix.volume: " . floatval($linear) . " }' 2>&1");
            if (strpos((string) $o, 'Error') === false)
                $ok = true;
        }
    }
    return $ok;
}

function pwctl_persist_routing(&$igData, $igId, $ogId, $volume, $mute)
{
    foreach ($igData['inputGroups'] as &$ig) {
        if ((isset($ig['id']) ? intval($ig['id']) : 0) === $igId) {
            if (!isset($ig['routing']))
                $ig['routing'] = array();
            $key = strval($ogId);
            if (!isset($ig['routing'][$key]))
                $ig['routing'][$key] = array();
            if ($volume !== null)
                $ig['routing'][$key]['volume'] = $volume;
            if ($mute !== null)
                $ig['routing'][$key]['mute'] = $mute;
            break;
        }
    }
    unset($ig);
}

/**
 * Set routing-path volume
 *
 * Sets the volume (0-100%) of a single input-group to output-group routing
 * path. Applied live to the routing combine-stream and persisted to the
 * input-group routing config.
 *
 * @route POST /api/pipewire/control/routing/{inputGroupId}/{outputGroupId}/volume
 * @body {"volume": 50}
 * @response 200 Route volume applied and persisted
 * ```json
 * {"status":"OK","inputGroupId":1,"outputGroupId":2,"volume":50,"applied":true,"message":"Route volume set to 50%"}
 * ```
 * @response 400 Missing 'volume'
 * @response 409 PipeWire backend not active
 */
function PWCtl_SetRoutingVolume()
{
    if (!pwctl_is_pipewire())
        return pwctl_not_pipewire_response();

    $igId = intval(params('inputGroupId'));
    $ogId = intval(params('outputGroupId'));
    $body = pwctl_body();
    if (!isset($body['volume'])) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Missing 'volume'"));
    }
    $vol = pwctl_clamp($body['volume'], 0, 100);

    $igData = pwctl_load_input_groups();
    $ogData = pwctl_load_output_groups();
    $applied = pwctl_apply_routing_volume($igData, $ogData, $igId, $ogId, round($vol / 100.0, 3));
    pwctl_persist_routing($igData, $igId, $ogId, $vol, null);
    pwctl_persist_input_groups($igData);

    return json(array(
        "status" => "OK",
        "inputGroupId" => $igId,
        "outputGroupId" => $ogId,
        "volume" => $vol,
        "applied" => $applied,
        "message" => $applied ? "Route volume set to {$vol}%" : "Saved; route not active (Apply input groups)",
    ));
}

/**
 * Mute or unmute a routing path
 *
 * Sets the mute state of a single input-group to output-group routing path.
 * Mute drives the routing channelmix volume to 0; unmute restores the saved
 * per-path volume. Provide `mute` (bool) or `toggle: true`. Persisted to the
 * input-group routing config.
 *
 * @route POST /api/pipewire/control/routing/{inputGroupId}/{outputGroupId}/mute
 * @body {"toggle": true}
 * @response 200 Route mute state applied and persisted
 * ```json
 * {"status":"OK","inputGroupId":1,"outputGroupId":2,"mute":true,"applied":true}
 * ```
 * @response 400 Provide 'mute' (bool) or 'toggle': true
 * @response 409 PipeWire backend not active
 */
function PWCtl_SetRoutingMute()
{
    if (!pwctl_is_pipewire())
        return pwctl_not_pipewire_response();

    $igId = intval(params('inputGroupId'));
    $ogId = intval(params('outputGroupId'));
    $body = pwctl_body();

    $igData = pwctl_load_input_groups();
    $cur = false;
    $savedVol = 100;
    foreach ($igData['inputGroups'] as $ig) {
        if ((isset($ig['id']) ? intval($ig['id']) : 0) === $igId) {
            $key = strval($ogId);
            if (isset($ig['routing'][$key]['mute']))
                $cur = (bool) $ig['routing'][$key]['mute'];
            if (isset($ig['routing'][$key]['volume']))
                $savedVol = intval($ig['routing'][$key]['volume']);
            break;
        }
    }

    $mute = pwctl_resolve_mute($body, $cur);
    if ($mute === null) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Provide 'mute' (bool) or 'toggle': true"));
    }

    $ogData = pwctl_load_output_groups();
    $linear = $mute ? 0.0 : round($savedVol / 100.0, 3);
    $applied = pwctl_apply_routing_volume($igData, $ogData, $igId, $ogId, $linear);
    pwctl_persist_routing($igData, $igId, $ogId, null, $mute);
    pwctl_persist_input_groups($igData);

    return json(array(
        "status" => "OK",
        "inputGroupId" => $igId,
        "outputGroupId" => $ogId,
        "mute" => $mute,
        "applied" => $applied,
    ));
}
