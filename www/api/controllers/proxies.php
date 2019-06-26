<?

function LoadProxyList() {
    global $settings;
    $mediaDirectory = $settings['mediaDirectory'];
    if (file_exists("$mediaDirectory/config/proxies")) {
        $hta = file("$mediaDirectory/config/proxies", FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    } else {
        $hta = array();
    }
    $proxies = array();
    foreach ($hta as $line) {
        if (strpos($line, 'http://') !== false) {
            $parts = preg_split("/[\s]+/", $line);
            $host = preg_split("/[\/]+/", $parts[2])[1];
            $proxies[] = $host;
        }
    }
    return $proxies;
}

function WriteProxyFile($proxies) {
    global $settings;
    $mediaDirectory = $settings['mediaDirectory'];

    $newht = "RewriteEngine on\nRewriteBase /proxy/\n\n";
    foreach( $proxies as $host ) {
        $newht = $newht . "RewriteRule ^" . $host . "$  " . $host . "/  [R,L]\n";
        $newht = $newht . "RewriteRule ^" . $host . "/(.*)$  http://" . $host . "/$1  [P,L]\n\n";
    }
    file_put_contents("$mediaDirectory/config/proxies", $newht);
}
/////////////////////////////////////////////////////////////////////////////
// GET /api/proxies
function GetProxies() {
    $proxies = LoadProxyList();
    return json_encode($proxies, true);
}


function AddProxy() {
    $pip = params('ProxyIp');
    $proxies = LoadProxyList();
    if ($in_array($pip, $proxies)) {
        $proxies[] = $pip;
    }
    WriteProxyFile($proxies);
    return json_encode($proxies);
}


function DeleteProxy() {
    $pip = params('ProxyIp');
    $proxies = LoadProxyList();
    $proxies = array_diff($proxies, array($pip));
    WriteProxyFile($proxies);
    return json_encode($proxies);
}

?>
