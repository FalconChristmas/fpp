#!/usr/bin/php
<?php
/*
 * genXlightsSubmodels.php
 *
 * Bridge converter: parse an xLights rgbeffects xml export plus FPP's
 * model-overlays.json and emit config/xlights-submodels.json, the digested
 * file fppd consumes lazily.  See docs/PixelOverlaySubModels.md.
 *
 * This runs OFF the daemon (CLI/PHP) so the multi-MB xml is never parsed by
 * fppd.  Long term xLights FPP Connect produces xlights-submodels.json
 * directly (see docs/xLights-SubModel-Export-Spec.md) and this script is only
 * the bridge.
 *
 * Usage:
 *   genXlightsSubmodels.php [rgbeffects.xml] [model-overlays.json] [out.json]
 *
 * Defaults are taken from the FPP media/config directory when arguments are
 * omitted.
 */

function configDir()
{
    // Mirror FPP's media directory resolution loosely; allow override via env.
    $media = getenv('FPPMEDIA');
    if (!$media || $media === '') {
        $media = '/home/fpp/media';
        if (!is_dir($media) && is_dir('/opt/fpp/media')) {
            $media = '/opt/fpp/media';
        }
    }
    return $media . '/config';
}

$cfg = configDir();
$xmlPath = $argv[1] ?? ($cfg . '/xlights_rgbeffects.xml');
$overlaysPath = $argv[2] ?? ($cfg . '/model-overlays.json');
$outPath = $argv[3] ?? ($cfg . '/xlights-submodels.json');

if (!file_exists($xmlPath)) {
    fwrite(STDERR, "xLights rgbeffects file not found: $xmlPath\n");
    exit(1);
}
if (!file_exists($overlaysPath)) {
    fwrite(STDERR, "model-overlays.json not found: $overlaysPath\n");
    exit(1);
}

// Normalize a model name the same way the UI does so xml model names
// (spaces) match model-overlays.json names (underscores).
function normName($n)
{
    return strtolower(preg_replace('/[^a-z0-9]/i', '', $n));
}

// FPP model-name sanitization for the generated submodel Name. FPP's model
// ctor only converts "/" -> "_"; model-overlays names already collapse spaces
// to "_", so mirror that for consistency.
function sanitizeName($n)
{
    return str_replace(['/', ' '], ['_', '_'], trim($n));
}

// Build parent lookup: normalized name -> [Name, StartChannel, ChannelCountPerNode]
$overlays = json_decode(file_get_contents($overlaysPath), true);
$parents = [];
if (isset($overlays['models']) && is_array($overlays['models'])) {
    foreach ($overlays['models'] as $m) {
        if (!isset($m['Name'])) {
            continue;
        }
        $parents[normName($m['Name'])] = [
            'Name' => $m['Name'],
            'StartChannel' => isset($m['StartChannel']) ? (int) $m['StartChannel'] : 1,
            'ChannelCountPerNode' => isset($m['ChannelCountPerNode']) ? (int) $m['ChannelCountPerNode'] : 3,
        ];
    }
}

// Expand one xLights "ranges" cell token into a list of node numbers.
// Tokens: "" (hole -> [null]), "5" (node), "1-27" / "27-1" (inclusive range).
function expandToken($tok)
{
    $tok = trim($tok);
    if ($tok === '') {
        return [null];
    }
    if (strpos($tok, '-') !== false && preg_match('/^(\d+)-(\d+)$/', $tok, $mm)) {
        $a = (int) $mm[1];
        $b = (int) $mm[2];
        $out = [];
        if ($a <= $b) {
            for ($i = $a; $i <= $b; $i++) {
                $out[] = $i;
            }
        } else {
            for ($i = $a; $i >= $b; $i--) {
                $out[] = $i;
            }
        }
        return $out;
    }
    if (ctype_digit($tok)) {
        return [(int) $tok];
    }
    return [null];
}

// Expand a full lineN value into a row of node numbers (null = hole).
function expandLine($val)
{
    $row = [];
    foreach (explode(',', $val) as $tok) {
        foreach (expandToken($tok) as $n) {
            $row[] = $n;
        }
    }
    return $row;
}

$reader = new XMLReader();
if (!$reader->open($xmlPath)) {
    fwrite(STDERR, "Unable to open xml: $xmlPath\n");
    exit(1);
}

$submodels = [];
$currentModel = '';
$missingParents = [];
$skipped = 0;

while ($reader->read()) {
    if ($reader->nodeType !== XMLReader::ELEMENT) {
        continue;
    }
    if ($reader->name === 'model') {
        $currentModel = (string) $reader->getAttribute('name');
        continue;
    }
    if ($reader->name !== 'subModel') {
        continue;
    }

    $smName = (string) $reader->getAttribute('name');
    $smType = (string) $reader->getAttribute('type');

    // Collect lineN attributes keyed by numeric index (attribute order is
    // lexicographic: line0,line1,line10,line2,... so we must sort numerically).
    $lines = [];
    if ($reader->moveToFirstAttribute()) {
        do {
            if (preg_match('/^line(\d+)$/', $reader->name, $mm)) {
                $lines[(int) $mm[1]] = $reader->value;
            }
        } while ($reader->moveToNextAttribute());
        $reader->moveToElement();
    }

    if (empty($lines) || $smType !== 'ranges') {
        // Only ranges submodels are supported in phase 1.
        $skipped++;
        continue;
    }
    ksort($lines, SORT_NUMERIC);

    // Build the grid. xLights line0 is the bottom buffer row; flip so the
    // first FPP buffer row (top) is the visual top, matching the designer view.
    $rowsXl = [];
    $width = 0;
    foreach ($lines as $idx => $val) {
        $row = expandLine($val);
        $width = max($width, count($row));
        $rowsXl[] = $row;
    }
    $rowsXl = array_reverse($rowsXl); // line0 (bottom) -> last FPP row
    $height = count($rowsXl);
    if ($width < 1 || $height < 1) {
        $skipped++;
        continue;
    }

    $pkey = normName($currentModel);
    if (!isset($parents[$pkey])) {
        $missingParents[$currentModel] = true;
        $skipped++;
        continue;
    }
    $parent = $parents[$pkey];

    // Serialize the grid: rows separated by ';', columns by ',', hole = "".
    $rowStrs = [];
    foreach ($rowsXl as $row) {
        $cells = [];
        for ($x = 0; $x < $width; $x++) {
            $n = $row[$x] ?? null;
            $cells[] = ($n === null) ? '' : (string) $n;
        }
        $rowStrs[] = implode(',', $cells);
    }
    $gridStr = implode(';', $rowStrs);

    $fullName = sanitizeName($parent['Name']) . '_' . sanitizeName($smName);

    $submodels[] = [
        'Name' => $fullName,
        'DisplayName' => $smName,
        'Type' => 'Sub',
        'SubType' => 'grid',
        'Parent' => $parent['Name'],
        'ParentStartChannel' => $parent['StartChannel'],
        'ChannelCountPerNode' => $parent['ChannelCountPerNode'],
        'Width' => $width,
        'Height' => $height,
        // Explicit orientation/corner so the FPP model ctor never reads empty
        // strings (it indexes StartCorner[1] before the non-Channel override).
        'Orientation' => 'horizontal',
        'StartCorner' => 'TL',
        'StringCount' => $height,
        'StrandsPerString' => 1,
        'Grid' => $gridStr,
    ];
}
$reader->close();

$out = [
    'source' => 'xlights',
    'version' => 1,
    'submodels' => $submodels,
];

if (file_put_contents($outPath, json_encode($out, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES)) === false) {
    fwrite(STDERR, "Failed to write $outPath\n");
    exit(1);
}

fwrite(STDERR, sprintf(
    "Wrote %d submodels to %s (skipped %d, %d parents unmatched)\n",
    count($submodels),
    $outPath,
    $skipped,
    count($missingParents)
));
exit(0);
