#!/usr/bin/php
<?php
/*
 * genXlightsModelGroups.php
 *
 * Bridge converter: parse xLights <modelGroup> definitions and emit
 * config/xlights-modelgroups.json -- a combined "channel grid" buffer per group
 * that FPP renders effects across (Tier B / Default world-position layout).
 * See docs/PixelOverlaySubModels.md.
 *
 * A group is built as a 2D buffer where member pixels are binned by their
 * real-world coordinates (from virtualdisplaymap), so an effect sweeping across
 * the buffer sweeps across the group in physical space -- the way xLights runs
 * effects over symmetrical groups of (sub)models.  Each buffer cell maps to the
 * absolute channels of whichever member pixels fall in it; FPP scatters the
 * cell color to all of them.
 *
 * Runs OFF the daemon.  Long term xLights FPP Connect should emit this file (or
 * concrete per-buffer-style layouts) directly; this is the bridge.
 *
 * Usage:
 *   genXlightsModelGroups.php [rgbeffects.xml] [configDir] [out.json]
 */

function configDir()
{
    $media = getenv('FPPMEDIA');
    if (!$media || $media === '') {
        $media = '/home/fpp/media';
        if (!is_dir($media) && is_dir('/opt/fpp/media')) {
            $media = '/opt/fpp/media';
        }
    }
    return $media . '/config';
}

$cfg = $argv[2] ?? configDir();
$xmlPath = $argv[1] ?? ($cfg . '/xlights_rgbeffects.xml');
$outPath = $argv[3] ?? ($cfg . '/xlights-modelgroups.json');
$overlaysPath = $cfg . '/model-overlays.json';
$subsPath = $cfg . '/xlights-submodels.json';
$vdmPath = $cfg . '/virtualdisplaymap';

foreach ([$xmlPath, $overlaysPath, $vdmPath] as $p) {
    if (!file_exists($p)) {
        fwrite(STDERR, "Required file not found: $p\n");
        exit(1);
    }
}

function normName($n)
{
    return strtolower(preg_replace('/[^a-z0-9]/i', '', $n));
}
function sanitizeName($n)
{
    return str_replace(['/', ' '], ['_', '_'], trim($n));
}

// --- model-overlays: normalized name -> channel info -------------------------
$overlays = json_decode(file_get_contents($overlaysPath), true);
$models = [];
foreach (($overlays['models'] ?? []) as $m) {
    if (!isset($m['Name'])) {
        continue;
    }
    $models[normName($m['Name'])] = [
        'Start' => (int) ($m['StartChannel'] ?? 1),       // 1-based
        'Count' => (int) ($m['ChannelCount'] ?? 0),
        'cpn' => (int) ($m['ChannelCountPerNode'] ?? 3),
    ];
}

// --- submodels: norm(parent)/norm(display) -> list of 0-based start channels --
$subChannels = [];
if (file_exists($subsPath)) {
    $subs = json_decode(file_get_contents($subsPath), true);
    foreach (($subs['submodels'] ?? []) as $sm) {
        $parentStart0 = ((int) ($sm['ParentStartChannel'] ?? 1)) - 1;
        $cpn = (int) ($sm['ChannelCountPerNode'] ?? 3);
        $chans = [];
        foreach (explode(';', $sm['Grid'] ?? '') as $row) {
            foreach (explode(',', $row) as $cell) {
                $cell = trim($cell);
                if ($cell === '' || !ctype_digit($cell)) {
                    continue;
                }
                $node = (int) $cell;
                if ($node > 0) {
                    $chans[$parentStart0 + ($node - 1) * $cpn] = true; // dedupe
                }
            }
        }
        // Key by normalized parent + display name (group refs use raw names).
        $key = normName($sm['Parent'] ?? '') . '/' . normName($sm['DisplayName'] ?? '');
        $subChannels[$key] = array_keys($chans);
    }
}

// --- virtualdisplaymap: 0-based channel -> [x,y] -----------------------------
$coord = [];
$fh = fopen($vdmPath, 'r');
while (($line = fgets($fh)) !== false) {
    $line = trim($line);
    if ($line === '' || $line[0] === '#') {
        continue;
    }
    $p = explode(',', $line);
    if (count($p) >= 4) {
        $coord[(int) $p[3]] = [(float) $p[0], (float) $p[1]];
    }
}
fclose($fh);

// --- pass 1: read all groups (name + member list) ----------------------------
$groupsRaw = []; // normName -> ['name'=>, 'members'=>[]]
$reader = new XMLReader();
$reader->open($xmlPath);
while ($reader->read()) {
    if ($reader->nodeType === XMLReader::ELEMENT && $reader->name === 'modelGroup') {
        $gname = (string) $reader->getAttribute('name');
        $mstr = (string) $reader->getAttribute('models');
        $members = array_filter(array_map('trim', explode(',', $mstr)), function ($v) {
            return $v !== '';
        });
        $groupsRaw[normName($gname)] = ['name' => $gname, 'members' => array_values($members)];
    }
}
$reader->close();

// Resolve a single member reference to a list of [x,y,channel0] pixels.
// Handles "Model/SubModel", plain "Model", and nested group names (recursively).
function resolveMember($ref, &$models, &$subChannels, &$coord, &$groupsRaw, &$seen)
{
    $pixels = [];
    if (strpos($ref, '/') !== false) {
        $parts = explode('/', $ref, 2);
        $key = normName($parts[0]) . '/' . normName($parts[1]);
        if (isset($subChannels[$key])) {
            foreach ($subChannels[$key] as $ch0) {
                if (isset($coord[$ch0])) {
                    $pixels[] = [$coord[$ch0][0], $coord[$ch0][1], $ch0];
                }
            }
        }
        return $pixels;
    }
    $nn = normName($ref);
    if (isset($models[$nn])) {
        $m = $models[$nn];
        $start0 = $m['Start'] - 1;
        $end0 = $start0 + $m['Count'];
        for ($ch0 = $start0; $ch0 < $end0; $ch0 += $m['cpn']) {
            if (isset($coord[$ch0])) {
                $pixels[] = [$coord[$ch0][0], $coord[$ch0][1], $ch0];
            }
        }
        return $pixels;
    }
    // Nested group (guard against cycles).
    if (isset($groupsRaw[$nn]) && !isset($seen[$nn])) {
        $seen[$nn] = true;
        foreach ($groupsRaw[$nn]['members'] as $sub) {
            $pixels = array_merge($pixels, resolveMember($sub, $models, $subChannels, $coord, $groupsRaw, $seen));
        }
    }
    return $pixels;
}

// --- pass 2: build a combined channel-grid buffer for each group -------------
$out = [];
$skipped = 0;
foreach ($groupsRaw as $nn => $g) {
    $seen = [$nn => true];
    $pixels = [];
    foreach ($g['members'] as $ref) {
        $pixels = array_merge($pixels, resolveMember($ref, $models, $subChannels, $coord, $groupsRaw, $seen));
    }
    if (count($pixels) === 0) {
        $skipped++;
        continue;
    }

    $minX = INF;
    $maxX = -INF;
    $minY = INF;
    $maxY = -INF;
    foreach ($pixels as $px) {
        $minX = min($minX, $px[0]);
        $maxX = max($maxX, $px[0]);
        $minY = min($minY, $px[1]);
        $maxY = max($maxY, $px[1]);
    }
    $spanX = max(1e-6, $maxX - $minX);
    $spanY = max(1e-6, $maxY - $minY);

    // Resolution scales with pixel count (density), bounded for memory.
    $n = count($pixels);
    $maxDim = max(16, min(96, (int) round(sqrt($n) * 2)));
    if ($spanX >= $spanY) {
        $W = $maxDim;
        $H = max(1, (int) round($maxDim * $spanY / $spanX));
    } else {
        $H = $maxDim;
        $W = max(1, (int) round($maxDim * $spanX / $spanY));
    }

    // Bin: cell index -> set of 1-based start channels.
    $cells = [];
    foreach ($pixels as $px) {
        $col = ($W <= 1) ? 0 : (int) round(($px[0] - $minX) / $spanX * ($W - 1));
        $row = ($H <= 1) ? 0 : (int) round(($maxY - $px[1]) / $spanY * ($H - 1)); // higher y = top
        $idx = $row * $W + $col;
        $cells[$idx][$px[2] + 1] = true; // store 1-based, dedupe
    }

    // Serialize: rows ';' , columns ',', multiple channels in a cell '&', hole "".
    $rowStrs = [];
    for ($r = 0; $r < $H; $r++) {
        $colStrs = [];
        for ($c = 0; $c < $W; $c++) {
            $idx = $r * $W + $c;
            if (isset($cells[$idx])) {
                $colStrs[] = implode('&', array_keys($cells[$idx]));
            } else {
                $colStrs[] = '';
            }
        }
        $rowStrs[] = implode(',', $colStrs);
    }

    $out[] = [
        'Name' => sanitizeName($g['name']),
        'DisplayName' => $g['name'],
        'Type' => 'Sub',
        'SubType' => 'channelgrid',
        'IsGroup' => true,
        'ChannelCountPerNode' => 3,
        'Width' => $W,
        'Height' => $H,
        'Orientation' => 'horizontal',
        'StartCorner' => 'TL',
        'StringCount' => $H,
        'StrandsPerString' => 1,
        'MemberCount' => count($g['members']),
        'PixelCount' => $n,
        'Grid' => implode(';', $rowStrs),
    ];
}

$result = [
    'source' => 'xlights',
    'version' => 1,
    'modelgroups' => $out,
];
if (file_put_contents($outPath, json_encode($result, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES)) === false) {
    fwrite(STDERR, "Failed to write $outPath\n");
    exit(1);
}
fwrite(STDERR, sprintf("Wrote %d model groups to %s (skipped %d empty)\n", count($out), $outPath, $skipped));
exit(0);
