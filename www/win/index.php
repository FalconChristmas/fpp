<?php
// /win — legacy WLED HTTP API. Returns the XML status frame WLED clients
// expect. We accept a subset of the query params (T= for power) and proxy
// them through to FPP's overlay model state.
require_once __DIR__ . '/../json/wled_common.inc.php';

// Apply minimal commands. T=0 off, T=1 on, T=2 toggle, A=brightness (ignored).
if (isset($_REQUEST['T'])) {
    $t = intval($_REQUEST['T']);
    foreach (fpp_overlay_models() as $m) {
        $modelName = $m['Name'] ?? ($m['name'] ?? null);
        if (!$modelName) continue;
        $current = !empty($m['effectRunning']) || (intval($m['isActive'] ?? 0) > 0);
        $new = ($t === 2) ? (!$current ? 1 : 0) : ($t === 1 ? 1 : 0);
        $url = 'http://127.0.0.1/api/overlays/model/' . rawurlencode($modelName) . '/state';
        $payload = json_encode(['State' => $new]);
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
    }
}

$state = build_wled_state();
$info  = build_wled_info();

header('Content-Type: text/xml; charset=utf-8');
header('Access-Control-Allow-Origin: *');
header('Cache-Control: no-store');

$bri = intval($state['bri']);
$on  = !empty($state['on']);
$ac  = $on ? $bri : 0;
$seg0 = $state['seg'][0];
$col0 = $seg0['col'][0];
$col1 = $seg0['col'][1];
$ds = htmlspecialchars($info['name'], ENT_XML1);

echo '<?xml version="1.0" ?>' . "\n";
echo "<vs>\n";
echo "  <ac>$ac</ac>\n";
echo "  <cl>{$col0[0]}</cl><cl>{$col0[1]}</cl><cl>{$col0[2]}</cl>\n";
echo "  <cs>{$col1[0]}</cs><cs>{$col1[1]}</cs><cs>{$col1[2]}</cs>\n";
echo "  <ns>0</ns><nr>0</nr><nl>0</nl><nf>0</nf>\n";
echo "  <nd>60</nd><nt>0</nt>\n";
echo "  <fx>{$seg0['fx']}</fx>\n";
echo "  <sx>{$seg0['sx']}</sx>\n";
echo "  <ix>{$seg0['ix']}</ix>\n";
echo "  <fp>{$seg0['pal']}</fp>\n";
echo "  <wv>-1</wv><ws>0</ws>\n";
echo "  <ps>0</ps><cy>0</cy>\n";
echo "  <ds>$ds</ds>\n";
echo "  <ss>0</ss>\n";
echo "</vs>\n";
