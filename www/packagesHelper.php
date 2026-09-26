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
// Streams text (the page's progress dialog); log lines are tagged as the
// user's own actions.
PackagesSetStreaming(true, 'user');
$action = $_POST['action'] ?? $_GET['action'] ?? null;
$packageName = $_POST['package'] ?? $_GET['package'] ?? null;

// The same rules the page enforces in its dialog, applied here so a direct
// request can't do what the UI won't offer.
if (in_array($action, array('install', 'reinstall', 'uninstall')) && !ValidPackageName($packageName)) {
    header('Content-Type: text/plain');
    echo "\nInvalid package name.\nFailed";
    exit;
}

if ($action === 'install' && !empty($packageName)) {
    header('Content-Type: text/plain');
    if (PackageIsInstalled($packageName)) {
        // The page only offers Reinstall for an installed package: FPP never
        // takes ownership of something that was already on the box.
        echo "\nPackage '$packageName' is already installed; use Reinstall instead.\nFailed";
        exit;
    }
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
    // The page only shows Uninstall when "user" is among the requesters.
    $owners = GetPackageRequesters($packageName);
    if (!in_array('user', $owners)) {
        echo count($owners)
            ? "\nPackage '$packageName' was not installed from this page; uninstall the owning plugin instead.\nFailed"
            : "\nPackage '$packageName' is not managed by FPP; nothing to uninstall.\nFailed";
        exit;
    }
    // Drops the "user" requester from the package and from the dependencies
    // its install pulled in; each is only apt-removed if nothing else still
    // needs it.
    $removed = ReleasePackageClaims(array_merge(array($packageName), PackagesInstalledVia($packageName)), 'user');
    echo count($removed) ? "\nCompleted" : "\nNothing removed";
    exit;
}

// Default: return the current package -> requesters map as JSON.
header('Content-Type: application/json');
echo json_encode(LoadUserPackages());
