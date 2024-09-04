<?

function LoadProxyList()
{
    global $settings;
    $mediaDirectory = $settings['mediaDirectory'];
    if (file_exists("$mediaDirectory/config/proxies")) {
        $hta = file("$mediaDirectory/config/proxies", FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    } else {
        $hta = array();
    }
    $proxies = array();
    $description = "";
    foreach ($hta as $line) {
        if (strpos($line, 'http://') !== false) {
            $parts = preg_split("/[\s]+/", $line);
            $host = preg_split("/[\/]+/", $parts[2])[1];
            if ($host != "$1") {
                $proxies[] = array("host" => $host, "description" => $description);
                $description = "";
            }
        } else if (strpos($line, '# D:') !== false) {
            $description = substr($line, 4);
        }
    }
    return $proxies;
}

function PostProxies()
{
    $proxies = $_POST;
    if (!empty($proxies)) {
        WriteProxyFile($proxies);
        //Trigger a JSON Configuration Backup
        GenerateBackupViaAPI('Proxies were added.');
    
        return json($proxies);
    } else {
        http_response_code(400); // Bad Request
    }
}

function WriteProxyFile($proxies)
{
    global $settings;
    $mediaDirectory = $settings['mediaDirectory'];

    $newht = "RewriteEngine on\nRewriteBase /proxy/\n\n";

    $newht = $newht . "RewriteCond %{HTTP:Upgrade}     \"websocket\" [NC]\n";
    $newht = $newht . "RewriteCond %{HTTP:Connection}  \"Upgrade\" [NC]\n";
    $newht = $newht . "RewriteRule ^(.*)/(.*)$ \"ws://$1/$2\" [P]\n";
    $newht = $newht . "\n";

    foreach ($proxies as $item) {
        $host = $item['host'];
        $description = $item['description'];
        if ($description != "") {
            $newht = $newht . "# D:" . $description . "\n";
        }
        $newht = $newht . "RewriteRule ^" . $host . "$  " . $host . "/  [R,L]\n";
        $newht = $newht . "RewriteRule ^" . $host . "/(.*)$  http://" . $host . "/$1  [P,L]\n\n";
    }
    $newht = $newht . "RewriteRule ^(.*)/(.*)$  http://$1/$2  [P,L]\n";
    $newht = $newht . "RewriteRule ^(.*)$  $1/  [R,L]\n\n";
    file_put_contents("$mediaDirectory/config/proxies", $newht);
}
/////////////////////////////////////////////////////////////////////////////
// GET /api/proxies
function GetProxies()
{
    $proxies = LoadProxyList();
    return json($proxies, true);
}

function AddProxy()
{
    $pip = params('ProxyIp');
    $pdesp = '';
    $proxies = LoadProxyList();
    if (!in_array($pip, $proxies)) {
        $proxies[] = array("host" => $pip, "description" => $pdesp);
    }
    WriteProxyFile($proxies);

	//Trigger a JSON Configuration Backup
	GenerateBackupViaAPI('Proxy ' . $pip . ' was added.');

    return json($proxies);
}

function DeleteProxy()
{
    $pip = params('ProxyIp');
    $proxies = LoadProxyList();
    //$proxies = array_diff($proxies, array($pip));
    
    $newproxies = [];
    foreach($json as $key => $host) {
            if($host->host != pip) {
                    $newproxies[] = $host;
            }
    }

    //print_r(json_encode($newproxies));
    WriteProxyFile($newproxies);

	//Trigger a JSON Configuration Backup
	GenerateBackupViaAPI('Proxy ' . $pip . ' was deleted.');

    return json($newproxies);
}

function GetRemotes()
{
    $curl = curl_init('http://localhost:32322/fppd/multiSyncSystems');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);

    $remotes = array();
    $j = json_decode($request_content, true);
    foreach ($j["systems"] as $host) {
        if ($host["address"] != $host["hostname"]) {
            $remotes[$host["address"]] = $host["address"] . " - " . $host["hostname"];
        } else {
            $remotes[$host["address"]] = $host["address"];
        }
    }
    return json($remotes);
}

function GetProxiedURL()
{
    $ip = params('Ip');
    $urlPart = params('urlPart');

    $url = "http://$ip/$urlPart";

    $data = file_get_contents($url);

    echo $data;

    return;
}
