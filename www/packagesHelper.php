<?php
$skipJSsettings = 1;
require_once "config.php";
require_once "common.php";
DisableOutputBuffering();

$userPackagesFile = $settings['configDirectory'] . '/userpackages.json';
$userPackages = [];
if (file_exists($userPackagesFile)) {
    $userPackages = json_decode(file_get_contents($userPackagesFile), true);
    if (!is_array($userPackages)) {
        $userPackages = array_values((array)$userPackages); // Convert objects to array of values if needed
    }
}

// Handle backend actions
$action = $_POST['action'] ?? $_GET['action'] ?? null;
$packageName = $_POST['package'] ?? $_GET['package'] ?? null;

if ($action) {
    if ($action === 'install' && !empty($packageName)) {
        $packageName = escapeshellarg($packageName);
        header('Content-Type: text/plain');

        $process = popen("sudo apt-get update;sudo apt-get install -y $packageName 2>&1", 'r');
        if (is_resource($process)) {
            while (!feof($process)) {
                echo fread($process, 1024);
                flush();
            }
            pclose($process);
        }

        // Add package to user-installed packages if not already added
        $packageName = trim($packageName, "'");
        if (!in_array($packageName, $userPackages)) {
            $userPackages[] = $packageName;
            file_put_contents($userPackagesFile, json_encode($userPackages, JSON_PRETTY_PRINT));
        }

        exit;
    }

    if ($action === 'uninstall' && !empty($packageName)) {
        $packageName = escapeshellarg($packageName);
        header('Content-Type: text/plain');

        $process = popen("sudo apt-get remove -y $packageName 2>&1", 'r');
        if (is_resource($process)) {
            while (!feof($process)) {
                echo fread($process, 1024);
                flush();
            }
            pclose($process);
        }

        // Remove package from user-installed packages
        $packageName = trim($packageName, "'");
        $userPackages = array_filter($userPackages, function ($pkg) use ($packageName) {
            return $pkg !== $packageName;
        });
        file_put_contents($userPackagesFile, json_encode(array_values($userPackages), JSON_PRETTY_PRINT));

        exit;
    }
}

header('Content-Type: application/json');
echo json_encode($userPackages);


