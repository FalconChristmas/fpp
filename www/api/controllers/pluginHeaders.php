<?php

// Seconds a collected set of indicators is served for before it is refreshed.
// The UI polls status every few seconds, so this only ever drops sub-requests
// that would have returned the same badges.
define('FPP_PLUGIN_INDICATOR_TTL', 5);

/**
 * Get header indicators
 *
 * Returns header indicator data (e.g., notification badges) from all installed plugins
 * that define a `headerIndicators.php` file.
 *
 * @route GET /api/plugin/headerIndicators
 * @response 200 Plugin header indicator data
 * ```json
 * [
 *   {
 *     "pluginName": "fpp-matrixtools",
 *     "label": "1",
 *     "color": "red"
 *   }
 * ]
 * ```
 */
function GetPluginHeaderIndicators()
{
    global $settings;

    // Collecting the indicators means one HTTP request per plugin back into this
    // same web server, from inside a request that is already holding a php-fpm
    // worker.  /api/system/status calls this on every poll of every open page, so
    // without a brake N clients need N * (1 + plugins) workers at once: the pool
    // fills with outer status requests, the inner per-plugin requests have no
    // worker left to run on, and the UI stops loading for everyone until php-fpm
    // is restarted.  Serving a short-lived cache, and letting only one request at
    // a time refresh it, keeps the fan-out at one set of sub-requests per
    // FPP_PLUGIN_INDICATOR_TTL regardless of how many clients are polling.
    $mediaDir = isset($settings['mediaDirectory']) ? $settings['mediaDirectory'] : '/home/fpp/media';
    $cacheFile = $mediaDir . '/tmp/plugin-header-indicators.json';
    $cached = FreshPluginHeaderIndicatorCache($cacheFile, FPP_PLUGIN_INDICATOR_TTL);
    if ($cached !== null) {
        return json($cached);
    }

    // Non-blocking: if another request is already refreshing, serve what we have
    // (even if stale) rather than queue up behind it holding a worker.
    $lock = @fopen($cacheFile . '.lock', 'c');
    if ($lock === false || !flock($lock, LOCK_EX | LOCK_NB)) {
        if ($lock !== false) {
            fclose($lock);
        }
        $stale = ReadPluginHeaderIndicatorCache($cacheFile);
        return json($stale === null ? array() : $stale);
    }

    $indicators = array();

    // Get list of installed plugins
    $pluginDir = $mediaDir . '/plugins';
    if (!is_dir($pluginDir)) {
        WritePluginHeaderIndicatorCache($cacheFile, $indicators);
        flock($lock, LOCK_UN);
        fclose($lock);
        return json($indicators);
    }

    $plugins = array_filter(glob($pluginDir . '/*'), 'is_dir');

    foreach ($plugins as $pluginPath) {
        $pluginName = basename($pluginPath);

        // Check if plugin has a headerIndicator endpoint by looking for api.php
        $apiFile = $pluginPath . '/api.php';
        if (!file_exists($apiFile)) {
            continue;
        }

        // Try to call the plugin's headerIndicator endpoint
        try {
            $url = "http://localhost/api/plugin/{$pluginName}/headerIndicator";

            $ch = curl_init($url);
            curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
            curl_setopt($ch, CURLOPT_TIMEOUT, 1);
            curl_setopt($ch, CURLOPT_CONNECTTIMEOUT_MS, 200);
            curl_setopt($ch, CURLOPT_FAILONERROR, false);

            $response = curl_exec($ch);
            $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
            curl_close($ch);

            // If endpoint exists and returns 200, parse the response
            if ($httpCode === 200 && $response) {
                $indicator = json_decode($response, true);
                if ($indicator && is_array($indicator)) {
                    // Add plugin name to the indicator for identification
                    $indicator['pluginName'] = $pluginName;
                    $indicators[] = $indicator;
                }
            }
        } catch (Exception $e) {
            // Silently skip plugins that don't have the endpoint
            continue;
        }
    }

    WritePluginHeaderIndicatorCache($cacheFile, $indicators);
    flock($lock, LOCK_UN);
    fclose($lock);

    return json($indicators);
}


function ReadPluginHeaderIndicatorCache($cacheFile)
{
    if (!file_exists($cacheFile)) {
        return null;
    }
    $data = @file_get_contents($cacheFile);
    if ($data === false) {
        return null;
    }
    $decoded = json_decode($data, true);
    return is_array($decoded) ? $decoded : null;
}

function FreshPluginHeaderIndicatorCache($cacheFile, $ttl)
{
    clearstatcache(true, $cacheFile);
    if (!file_exists($cacheFile) || (time() - filemtime($cacheFile)) >= $ttl) {
        return null;
    }
    return ReadPluginHeaderIndicatorCache($cacheFile);
}

function WritePluginHeaderIndicatorCache($cacheFile, $indicators)
{
    // Write then rename so a reader never sees a half-written file.
    $tmp = $cacheFile . '.' . getmypid() . '.tmp';
    if (@file_put_contents($tmp, json_encode($indicators)) !== false) {
        @rename($tmp, $cacheFile);
    }
}
