<?php

/**
 * GET /api/plugin/headerIndicators
 * 
 * Returns an array of header indicator configurations from all installed plugins
 * that have registered a header indicator endpoint.
 * 
 * Each plugin can provide a /api/plugin/<plugin-name>/headerIndicator endpoint
 * that returns an indicator configuration object.
 */
function GetPluginHeaderIndicators()
{
    $indicators = array();
    
    // Get list of installed plugins
    $pluginDir = '/home/fpp/media/plugins';
    if (!is_dir($pluginDir)) {
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
    
    return json($indicators);
}

