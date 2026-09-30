<?php
$wrapped = 0;
$skipJSsettings = 1;
require_once 'common.php';
DisableOutputBuffering();

/**
 * SSRF guard for vendor firmware downloads: the URL must use http(s) with no
 * credentials, no off-default port, and must resolve ONLY to public IPs.
 * Anything loopback/private/link-local (incl. cloud metadata 169.254.169.254),
 * multicast, reserved, or unresolvable is refused. Legitimate vendor firmware
 * lives on public hosts, so this is transparent to real downloads.
 */
function capeFirmwareUrlIsPublic($url)
{
    if (!is_string($url)) {
        return false;
    }
    $parts = parse_url($url);
    if ($parts === false || !isset($parts['host'])) {
        return false;
    }
    $scheme = strtolower($parts['scheme'] ?? '');
    if ($scheme !== 'http' && $scheme !== 'https') {
        return false;
    }
    if (isset($parts['user']) || isset($parts['pass'])) {
        return false;
    }
    $host = $parts['host'];
    // parse_url keeps IPv6 literals bracketed ([::1]); unwrap for validation.
    if (strlen($host) > 2 && $host[0] === '[' && substr($host, -1) === ']') {
        $host = substr($host, 1, -1);
    }
    if (isset($parts['port'])) {
        $defaultPort = ($scheme === 'https') ? 443 : 80;
        if ((int)$parts['port'] !== $defaultPort) {
            return false;
        }
    }
    // Literal IP: validate directly. Hostname: every resolved A/AAAA record
    // must be public (fail closed on unresolvable or record-less answers,
    // which also defeats decimal/hex IP obfuscation like http://2130706433/).
    $ips = array();
    if (filter_var($host, FILTER_VALIDATE_IP)) {
        $ips[] = $host;
    } else {
        $recs = @dns_get_record($host, DNS_A + DNS_AAAA);
        if (!is_array($recs) || count($recs) === 0) {
            return false;
        }
        foreach ($recs as $r) {
            if (isset($r['ip'])) {
                $ips[] = $r['ip'];
            }
            if (isset($r['ipv6'])) {
                $ips[] = $r['ipv6'];
            }
        }
        if (count($ips) === 0) {
            return false;
        }
    }
    foreach ($ips as $ip) {
        if (!is_string($ip)) {
            return false;
        }
        if (strpos($ip, ':') === false) {
            // IPv4: match octets explicitly rather than trusting filter flags
            // alone (multicast/reserved coverage varies by build). Rejects
            // this-network, loopback, private, link-local, IETF/test/benchmark
            // ranges, multicast, reserved, and broadcast. String-form checks
            // stay correct on 32-bit PHP builds.
            $o = explode('.', $ip);
            if (count($o) !== 4) {
                return false;
            }
            $o0 = (int)$o[0];
            $o1 = (int)$o[1];
            $o2 = (int)$o[2];
            $bad = ($o0 === 0 || $o0 === 10 || $o0 === 127 ||
                ($o0 === 100 && $o1 >= 64 && $o1 <= 127) ||
                ($o0 === 172 && $o1 >= 16 && $o1 <= 31) ||
                ($o0 === 192 && $o1 === 168) ||
                ($o0 === 169 && $o1 === 254) ||
                ($o0 === 192 && $o1 === 0) ||
                ($o0 === 192 && $o1 === 88 && $o2 === 99) ||
                ($o0 === 198 && ($o1 === 18 || $o1 === 19)) ||
                ($o0 === 198 && $o1 === 51 && $o2 === 100) ||
                ($o0 === 203 && $o1 === 0 && $o2 === 113) ||
                ($o0 >= 224));
            if ($bad || $ip === '0.0.0.0') {
                return false;
            }
        } else {
            // IPv6: compare packed bytes -- ::1, fe80::/10 (link-local),
            // fc00::/7 (unique-local). ::ffff:0:0/96 (v4-mapped) is validated
            // as its embedded v4 address, since the filter flags below do not
            // see through the mapping.
            $p = @inet_pton($ip);
            if ($p === false || strlen($p) !== 16) {
                return false;
            }
            if ($p === str_repeat("\0", 15) . "\1") {
                return false; // ::1
            }
            if (substr($p, 0, 12) === "\0\0\0\0\0\0\0\0\0\0\xff\xff") {
                $mapped = @inet_ntop(substr($p, 12, 4));
                if (!is_string($mapped) || $mapped === '0.0.0.0' || str_starts_with($mapped, '127.') ||
                    !filter_var($mapped, FILTER_VALIDATE_IP, FILTER_FLAG_NO_PRIV_RANGE | FILTER_FLAG_NO_RES_RANGE)) {
                    return false;
                }
                continue;
            }
            $b0 = ord($p[0]);
            $b1 = ord($p[1]);
            if ($b0 === 0xFE && ($b1 & 0xC0) === 0x80) {
                return false; // fe80::/10
            }
            if (($b0 & 0xFE) === 0xFC) {
                return false; // fc00::/7
            }
        }
        if (!filter_var($ip, FILTER_VALIDATE_IP, FILTER_FLAG_NO_PRIV_RANGE | FILTER_FLAG_NO_RES_RANGE)) {
            return false;
        }
    }
    return true;
}

$force = "";
if (isset($_GET['force']) && $_GET['force'] == 'true') {
    $force = "force ";
}

$resetDefaults = "";
if (isset($_GET['resetDefaults']) && $_GET['resetDefaults'] == 'true') {
    $resetDefaults = "resetDefaults ";
} else if (isset($_POST['resetDefaults']) && $_POST['resetDefaults'] == 'true') {
    $resetDefaults = "resetDefaults ";
}

$action = 'Upgrading';
$file = '';
$unlink = 1;
if (isset($_FILES['firmware']) && isset($_FILES['firmware']['tmp_name'])) {
    $file = $_FILES["firmware"]["tmp_name"];
} else if (isset($_POST['filename']) || isset($_GET['filename'])) {
    $unlink = 0;

    $fnRaw = isset($_GET['filename']) ? $_GET['filename'] : $_POST['filename'];
    $fn = is_string($fnRaw) ? $fnRaw : '';

    // Zero-risk traversal block: reject .. and null bytes without allow-list
    if (strpos($fn, '..') !== false || strpos($fn, "\0") !== false) {
        echo "Invalid filename: traversal not allowed\n";
        $file = '';
    } else if (preg_match('/^http/', $fn)) {
        // Vendor firmware downloads stay working, but the URL must target a
        // public host: without this check any link-local/metadata/intranet URL
        // would be fetched server-side and flashed to the cape EEPROM below.
        if (!capeFirmwareUrlIsPublic($fn)) {
            echo "Invalid firmware URL: host must be a public http(s) address\n";
            $file = '';
        } else {
            $file = '/home/fpp/media/tmp/tmp-eeprom.bin';
            echo "Downloading " . htmlspecialchars($fn) . " to $file \n\n";
        system("/usr/bin/wget -O " . escapeshellarg($file) . " " . escapeshellarg($fn) . " 2>&1");
        if (!file_exists($file) || filesize($file) < 72) {
            echo "\n\nProblems downloading firmware.  Check above errors for details.\n";
            echo "You may be able to manually download the file and do the upgrade directly with the file.\n";
            unlink($file);
            $file = '';
        }
        $unlink = 1;
        }
    } else if (preg_match('/\/opt\/fpp\/capes/', $fn)) {
        // Ensure the resolved path stays under /opt/fpp/capes
        $realBase = realpath('/opt/fpp/capes');
        $realPath = realpath($fn);
        // realpath returns false if file doesn't exist yet — fall back to string prefix check
        if ($realPath) {
            if (!$realBase || strpos($realPath, $realBase) !== 0) {
                echo "Invalid cape file path\n";
                $file = '';
            } else {
                $file = $fn;
            }
        } else {
            // File may not exist yet; ensure the string starts with the base and has no .. (already checked)
            if (strpos($fn, '/opt/fpp/capes/') !== 0) {
                echo "Invalid cape file path\n";
                $file = '';
            } else {
                $file = $fn;
            }
        }

        if ($file !== '' && file_exists('/home/fpp/media/config/cape-eeprom.bin')) {
            unlink('/home/fpp/media/config/co-bbbStrings.json');
            unlink('/home/fpp/media/config/co-pixelStrings.json');
        }

    } else {
        // For uploads, only allow a plain filename (no directory) to stay within uploadDirectory
        $baseName = basename($fn);
        if ($baseName !== $fn || $baseName === '' || strpos($baseName, '/') !== false || strpos($baseName, '\\') !== false) {
            echo "Invalid filename\n";
            $file = '';
        } else {
            $file = $uploadDirectory . '/' . $baseName;
            // Extra containment: ensure parent dir is still uploadDirectory
            $realBase = realpath($uploadDirectory);
            $realParent = realpath(dirname($file));
            if ($realBase && $realParent && strpos($realParent, $realBase) !== 0) {
                echo "Invalid filename\n";
                $file = '';
            }
        }
    }
}

if ($file != '') {
    echo "Upgrading firmware.....\n";
    echo "\n";
    flush();
    // Wrap the file path as a single shell arg so the filename/URL cannot inject
    // additional commands. $force/$resetDefaults are fixed constant strings.
    system("sudo /opt/fpp/scripts/upgradeCapeFirmware " . $force . $resetDefaults . escapeshellarg($file), $retval);
    WriteSettingToFile('rebootFlag', 1);
    flush();

    if ($unlink && file_exists($file)) {
        unlink($file);
    }

    echo "\n";
} else {
    echo "\nERROR: No firmware file specified.\n";
}
