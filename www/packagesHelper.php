<?php
$skipJSsettings = 1;
require_once "config.php";
require_once "common.php";
require_once "common/packages.inc.php";
DisableOutputBuffering();

// Backend for the Package Manager UI. Install/uninstall stream apt output live
// into the progress modal. The heavy lifting (hardened apt, ownership tracking)
// lives in common/packages.inc.php so the plugin dependency installer can reuse
// it. Everything the UI does is on behalf of the "user" requester.
$action = $_POST['action'] ?? $_GET['action'] ?? null;
$packageName = $_POST['package'] ?? $_GET['package'] ?? null;

if ($action === 'install' && !empty($packageName)) {
    header('Content-Type: text/plain');
    $ok = InstallSystemPackage($packageName, 'user');
    echo $ok ? "\nCompleted" : "\nFailed";
    exit;
}

if ($action === 'reinstall' && !empty($packageName)) {
    header('Content-Type: text/plain');
    $ok = ReinstallSystemPackage($packageName, true);
    echo $ok ? "\nCompleted" : "\nFailed";
    exit;
}

if ($action === 'uninstall' && !empty($packageName)) {
    header('Content-Type: text/plain');
    // Drops the "user" requester; the package is only apt-removed if no plugin
    // still depends on it.
    RemoveSystemPackageRequester($packageName, 'user');
    echo "\nCompleted";
    exit;
}

// Default: return the current package -> requesters map as JSON.
header('Content-Type: application/json');
echo json_encode(LoadUserPackages());
