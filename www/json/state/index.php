<?php
// /json/state — minimal GET implementation, reflects FPP overlay-model state.
// POST is accepted and best-effort translated to FPP overlay calls.
require_once __DIR__ . '/../wled_common.inc.php';

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    send_wled_json(['success' => true]);
    exit;
}

if ($_SERVER['REQUEST_METHOD'] === 'POST' || $_SERVER['REQUEST_METHOD'] === 'PUT') {
    $raw = file_get_contents('php://input');
    $body = json_decode($raw, true);
    $applied = [];

    if (is_array($body) && !empty($body['seg'])) {
        $models = fpp_overlay_models();
        foreach ($body['seg'] as $s) {
            $segId = intval($s['id'] ?? 0);
            if (!isset($models[$segId])) continue;
            $modelName = $models[$segId]['Name'] ?? ($models[$segId]['name'] ?? null);
            if (!$modelName) continue;

            // on/off → state Enabled (1) or Disabled (0)
            if (array_key_exists('on', $s)) {
                $newState = $s['on'] ? 1 : 0;
                $url = 'http://127.0.0.1/api/overlays/model/' . rawurlencode($modelName) . '/state';
                $payload = json_encode(['State' => $newState]);
                $ch = curl_init($url);
                curl_setopt_array($ch, [
                    CURLOPT_CUSTOMREQUEST => 'PUT',
                    CURLOPT_POSTFIELDS    => $payload,
                    CURLOPT_HTTPHEADER    => ['Content-Type: application/json'],
                    CURLOPT_RETURNTRANSFER=> true,
                    CURLOPT_TIMEOUT       => 2,
                ]);
                curl_exec($ch);
                curl_close($ch);
                $applied[] = "$modelName.state=$newState";
            }
        }
    }
    if (is_array($body) && array_key_exists('on', $body) && empty($body['seg'])) {
        // Whole-device on/off — apply to all models
        foreach (fpp_overlay_models() as $m) {
            $modelName = $m['Name'] ?? ($m['name'] ?? null);
            if (!$modelName) continue;
            $url = 'http://127.0.0.1/api/overlays/model/' . rawurlencode($modelName) . '/state';
            $payload = json_encode(['State' => $body['on'] ? 1 : 0]);
            $ch = curl_init($url);
            curl_setopt_array($ch, [
                CURLOPT_CUSTOMREQUEST => 'PUT',
                CURLOPT_POSTFIELDS    => $payload,
                CURLOPT_HTTPHEADER    => ['Content-Type: application/json'],
                CURLOPT_RETURNTRANSFER=> true,
                CURLOPT_TIMEOUT       => 2,
            ]);
            curl_exec($ch);
            curl_close($ch);
            $applied[] = "$modelName.state=" . ($body['on'] ? 1 : 0);
        }
    }
}

send_wled_json(build_wled_state());
