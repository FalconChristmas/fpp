<?php
/**
 * Helpers for the per-output virtual display preview selection.
 *
 * The selected preview is stored in co-other.json as "previewFile".  An empty
 * value means the traditional default: config/default.vdm when it exists,
 * otherwise config/virtualdisplaymap, with images/default-bg.jpg taking
 * priority over images/virtualdisplaybackground.jpg.
 *
 * A named preview is a plain .vdm basename from the config directory, and its
 * background is the same basename with "-bg.jpg" in the images directory.
 *
 * $settings must already be available (config.php is included by every caller).
 */

/**
 * Return true when $name is a usable .vdm basename.
 *
 * This deliberately rejects any path component so a hand-edited config cannot
 * make the map reader or the background lookup escape their directories.
 */
function IsValidVirtualDisplayPreviewFile($name)
{
    if (!is_string($name) || strlen($name) <= 4) {
        return false;
    }

    // NUL bytes truncate paths in the underlying C filesystem calls, so reject
    // them explicitly (strpbrk() cannot see one: it ends the character list).
    if (strpos($name, "\0") !== false || strpbrk($name, "/\\") !== false) {
        return false;
    }

    if (strpos($name, '..') !== false) {
        return false;
    }

    return preg_match('/\.vdm$/i', $name) === 1;
}

/**
 * List the .vdm files that can be offered in the Preview dropdown.
 *
 * default.vdm is not listed: it is what the "Default" entry resolves to.
 */
function GetVirtualDisplayPreviewFiles()
{
    global $settings;

    $configDir = rtrim($settings['configDirectory'], '/');
    $files = array();

    if (!is_dir($configDir)) {
        return $files;
    }

    $entries = @scandir($configDir);
    if ($entries === false) {
        return $files;
    }

    foreach ($entries as $entry) {
        if ($entry === '.' || $entry === '..') {
            continue;
        }

        if (!IsValidVirtualDisplayPreviewFile($entry)) {
            continue;
        }

        if (strcasecmp($entry, 'default.vdm') === 0) {
            continue;
        }

        $path = $configDir . '/' . $entry;

        // Symlinks are skipped so the dropdown only shows files that really
        // live in the config directory.
        if (!is_file($path) || is_link($path)) {
            continue;
        }

        $files[] = $entry;
    }

    sort($files, SORT_NATURAL | SORT_FLAG_CASE);

    return $files;
}

/**
 * Resolve a previewFile value into the map and background to use.
 *
 * Returns an array with:
 *   previewFile  the normalized selection ('' for Default)
 *   mapFile      absolute path to the .vdm/map file ('' when invalid)
 *   mapName      basename of the map, for messages
 *   mapExists    whether the map file is present
 *   background   background image basename, or '' for no background
 *   error        human-readable problem, or '' when everything is usable
 */
function GetVirtualDisplayPreviewInfo($previewFile)
{
    global $settings;

    $info = array(
        'previewFile' => '',
        'mapFile' => '',
        'mapName' => '',
        'mapExists' => false,
        'background' => '',
        'error' => '',
    );

    $configDir = rtrim($settings['configDirectory'], '/');
    $imageDir = rtrim($settings['imageDirectory'], '/');

    $legacyMap = $configDir . '/virtualdisplaymap';
    $defaultMap = $configDir . '/default.vdm';
    $legacyBackground = 'virtualdisplaybackground.jpg';
    $defaultBackground = 'default-bg.jpg';

    $name = is_string($previewFile) ? trim($previewFile) : '';

    if ($name === '' || strcasecmp($name, 'default.vdm') === 0) {
        // Default preview.  default.vdm/default-bg.jpg each override their
        // legacy counterpart independently.
        $info['mapFile'] = is_file($defaultMap) ? $defaultMap : $legacyMap;
        $info['mapName'] = basename($info['mapFile']);
        $info['mapExists'] = is_file($info['mapFile']);
        $info['background'] = is_file($imageDir . '/' . $defaultBackground)
            ? $defaultBackground
            : $legacyBackground;

        if (!$info['mapExists']) {
            $info['error'] = "Virtual display map file '" . $info['mapName'] . "' was not found.";
        }

        return $info;
    }

    $info['previewFile'] = $name;

    if (!IsValidVirtualDisplayPreviewFile($name)) {
        $info['error'] = "Invalid virtual display preview '" . $name . "'.";
        return $info;
    }

    $info['mapFile'] = $configDir . '/' . $name;
    $info['mapName'] = $name;
    $info['mapExists'] = is_file($info['mapFile']);

    // Only use a background that actually exists.  A named preview without
    // its matching -bg.jpg gets no background rather than the default image,
    // which would not line up with a different layout.
    $background = preg_replace('/\.vdm$/i', '', $name) . '-bg.jpg';
    if (is_file($imageDir . '/' . $background)) {
        $info['background'] = $background;
    }

    if (!$info['mapExists']) {
        $info['error'] = "Virtual display preview '" . $name . "' was not found.";
    }

    return $info;
}

/**
 * Read the width/height header from a .vdm-style map file.
 *
 * Returns array('width' => int, 'height' => int), or false when the file
 * cannot be read or the header is missing/not two positive numbers.  Callers
 * use this before dividing by the dimensions, so a damaged or hand-edited map
 * shows an error instead of a PHP division-by-zero page.
 */
function GetVirtualDisplayMapDimensions($mapFile)
{
    if (!is_string($mapFile) || $mapFile === '' || !is_file($mapFile)) {
        return false;
    }

    $f = @fopen($mapFile, 'r');
    if ($f === false) {
        return false;
    }

    $line = fgets($f);
    while ($line !== false && preg_match('/^#/', $line)) {
        $line = fgets($f);
    }
    fclose($f);

    if ($line === false) {
        return false;
    }

    $parts = explode(',', trim($line));
    if (count($parts) < 2) {
        return false;
    }

    $width = trim($parts[0]);
    $height = trim($parts[1]);
    if (!is_numeric($width) || !is_numeric($height)) {
        return false;
    }

    $width = (int) $width;
    $height = (int) $height;
    if ($width <= 0 || $height <= 0) {
        return false;
    }

    return array('width' => $width, 'height' => $height);
}
