<?php
// Shared helpers for the WLED-compatible JSON shim under /json/.
// Translates FPP state (system status, overlay models, overlay effects)
// into WLED-shaped JSON so the WLED-iOS app and other WLED clients
// see FPP as a (very minimal) WLED node.

function fpp_curl_json($path, $timeout = 2)
{
    $ch = curl_init('http://127.0.0.1' . $path);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_TIMEOUT, $timeout);
    curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 1);
    $body = curl_exec($ch);
    curl_close($ch);
    if ($body === false || $body === '') {
        return null;
    }
    $j = json_decode($body, true);
    return is_array($j) ? $j : null;
}

function fpp_primary_mac()
{
    // Try /sys first — cheapest and authoritative.
    foreach (['eth0', 'wlan0', 'end0'] as $iface) {
        $f = "/sys/class/net/$iface/address";
        if (is_readable($f)) {
            $m = trim(@file_get_contents($f));
            if ($m && $m !== '00:00:00:00:00:00') {
                return $m;
            }
        }
    }
    // Fall back to first non-loopback interface from FPP's status payload.
    $status = fpp_curl_json('/api/system/status');
    if ($status && !empty($status['interfaces'])) {
        foreach ($status['interfaces'] as $i) {
            if (!empty($i['address']) && $i['address'] !== '00:00:00:00:00:00') {
                return $i['address'];
            }
        }
    }
    return '02:00:00:00:00:00';
}

function fpp_primary_ip()
{
    $status = fpp_curl_json('/api/system/status');
    if ($status && !empty($status['advancedView']['IPs'][0])) {
        return $status['advancedView']['IPs'][0];
    }
    if ($status && !empty($status['interfaces'])) {
        foreach ($status['interfaces'] as $i) {
            foreach (($i['addr_info'] ?? []) as $a) {
                if (($a['family'] ?? '') === 'inet') {
                    return $a['local'];
                }
            }
        }
    }
    return '127.0.0.1';
}

// FPP overlay models become WLED "segments". One model -> one segment.
function fpp_overlay_models()
{
    $models = fpp_curl_json('/api/overlays/models');
    return is_array($models) ? $models : [];
}

function fpp_overlay_effects()
{
    $eff = fpp_curl_json('/api/overlays/effects');
    return is_array($eff) ? $eff : [];
}

function fpp_overlay_running()
{
    $r = fpp_curl_json('/api/overlays/running');
    return is_array($r) ? $r : [];
}

// Build the /json/info payload. Includes the minimum fields the WLED-iOS
// app reads: mac, name, ver, leds.count, ip, brand, product.
function build_wled_info()
{
    $status = fpp_curl_json('/api/system/status') ?: [];
    $models = fpp_overlay_models();
    $effects = fpp_overlay_effects();

    $totalPixels = 0;
    foreach ($models as $m) {
        $w = intval($m['width']  ?? 0);
        $h = intval($m['height'] ?? 0);
        $totalPixels += max(1, $w) * max(1, $h);
    }
    if ($totalPixels === 0) {
        $totalPixels = 1; // WLED-iOS doesn't like zero
    }

    $mac = strtolower(str_replace(':', '', fpp_primary_mac()));
    $hostname = $status['host_name'] ?? 'FPP';
    // Prefer HostDescription only if it's a real label; the FPP default
    // is the literal string "FPP", which is less informative than the
    // actual hostname the device announces over mDNS.
    $hd = $status['host_description'] ?? '';
    if ($hd !== '' && $hd !== 'FPP') {
        $name = $hd;
    } else {
        $name = $hostname;
    }
    $version = $status['version'] ?? '0.0.0';
    $platform = $status['platform'] ?? 'FPP';

    return [
        'ver'      => $version,
        'vid'      => intval(date('ymd') . '0'),
        'cn'       => 'FPP',
        'release'  => $version,
        'leds' => [
            'count'  => $totalPixels,
            'pwr'    => 0,
            'fps'    => 40,
            'maxpwr' => 0,
            'maxseg' => max(1, count($models)),
            'bootps' => 0,
            'rgbw'   => false,
            'wv'     => 0,
            'cct'    => false,
            'lc'     => 1,
            'seglc'  => array_fill(0, max(1, count($models)), 1),
        ],
        'str'      => false,
        'name'     => $name,
        'udpport'  => 21324,
        'live'     => false,
        'liveseg'  => -1,
        'lm'       => '',
        'lip'      => '',
        'ws'       => -1,                 // we do not (yet) speak WebSocket
        'fxcount'  => count($effects),
        'palcount' => 1,                  // single placeholder palette
        'cpalcount'=> 0,
        'cpalmax'  => 0,
        'maps'     => [['id' => 0]],
        'wifi'     => [
            'bssid'   => '00:00:00:00:00:00',
            'rssi'    => 0,
            'signal'  => 0,
            'channel' => 0,
            'ap'      => false,
        ],
        'fs'       => ['u' => 0, 't' => 0, 'pmt' => 0],
        'ndc'      => -1,
        'arch'     => $platform,
        'core'     => $version,
        'lwip'     => 0,
        'freeheap' => 0,
        'uptime'   => intval($status['uptimeTotalSeconds'] ?? 0),
        'time'     => $status['time'] ?? '',
        'opt'      => 79,
        'brand'    => 'FPP',
        'product'  => 'FalconPlayer',
        'mac'      => $mac,
        'ip'       => fpp_primary_ip(),
    ];
}

// Build a /json/state payload, faking one segment per FPP overlay model.
function build_wled_state()
{
    $models = fpp_overlay_models();
    $running = fpp_overlay_running();
    $runningByName = [];
    foreach ($running as $r) {
        $name = $r['model']['Name'] ?? ($r['model']['name'] ?? null);
        if ($name) {
            $runningByName[$name] = $r;
        }
    }

    $segs = [];
    $idx = 0;
    $effects = fpp_overlay_effects();
    $effectIndex = array_flip($effects);
    $anyOn = false;

    foreach ($models as $m) {
        $name = $m['Name'] ?? ($m['name'] ?? "model$idx");
        $w = max(1, intval($m['width']  ?? 1));
        $h = max(1, intval($m['height'] ?? 1));
        $len = $w * $h;
        $isOn = !empty($m['effectRunning']) || (intval($m['isActive'] ?? 0) > 0);
        if ($isOn) { $anyOn = true; }

        $fxIdx = 0;
        if (!empty($m['effectName']) && isset($effectIndex[$m['effectName']])) {
            $fxIdx = intval($effectIndex[$m['effectName']]);
        }

        $segs[] = [
            'id'    => $idx,
            'start' => 0,
            'stop'  => $len,
            'len'   => $len,
            'grp'   => 1,
            'spc'   => 0,
            'of'    => 0,
            'on'    => $isOn,
            'frz'   => false,
            'bri'   => 255,
            'cct'   => 127,
            'set'   => 0,
            'n'     => $name,
            'col'   => [[255,255,255,0],[0,0,0,0],[0,0,0,0]],
            'fx'    => $fxIdx,
            'sx'    => 128,
            'ix'    => 128,
            'pal'   => 0,
            'c1'    => 0,
            'c2'    => 0,
            'c3'    => 0,
            'sel'   => $idx === 0,
            'rev'   => false,
            'mi'    => false,
            'o1'    => false,
            'o2'    => false,
            'o3'    => false,
            'si'    => 0,
            'm12'   => 0,
        ];
        $idx++;
    }

    if (empty($segs)) {
        $segs[] = [
            'id' => 0, 'start' => 0, 'stop' => 1, 'len' => 1, 'grp' => 1, 'spc' => 0,
            'of' => 0, 'on' => false, 'frz' => false, 'bri' => 255, 'cct' => 127,
            'set' => 0, 'n' => 'Placeholder',
            'col' => [[0,0,0,0],[0,0,0,0],[0,0,0,0]],
            'fx' => 0, 'sx' => 128, 'ix' => 128, 'pal' => 0,
            'c1' => 0, 'c2' => 0, 'c3' => 0,
            'sel' => true, 'rev' => false, 'mi' => false,
            'o1' => false, 'o2' => false, 'o3' => false, 'si' => 0, 'm12' => 0,
        ];
    }

    return [
        'on'         => $anyOn,
        'bri'        => 255,
        'transition' => 7,
        'ps'         => -1,
        'pl'         => -1,
        'ledmap'     => 0,
        'nl' => ['on' => false, 'dur' => 60, 'mode' => 1, 'tbri' => 0, 'rem' => -1],
        'udpn' => ['send' => false, 'recv' => false, 'sgrp' => 1, 'rgrp' => 1],
        'lor'      => 0,
        'mainseg'  => 0,
        'seg'      => $segs,
    ];
}

function send_wled_json($payload)
{
    header('Content-Type: application/json');
    header('Access-Control-Allow-Origin: *');
    header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
    header('Access-Control-Allow-Headers: Content-Type');
    header('Cache-Control: no-store');
    echo json_encode($payload, JSON_UNESCAPED_SLASHES);
}
