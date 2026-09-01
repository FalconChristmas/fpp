<?php
// Shared helpers for system (apt) package management with per-requester
// ownership tracking.
//
// The manifest lives at $settings['configDirectory'].'/userpackages.json'. It
// records every package FPP installed on the user's behalf together with WHO
// asked for it, so a package is only apt-removed once nothing needs it anymore.
// The same file is replayed after an fppos OS upgrade by the C++ boot code
// (src/boot/FPPINIT_Config.cpp installPackagesFromJson) to reinstall packages
// onto the freshly-flashed rootfs.
//
// On-disk schema (new):
//   [ { "package": "vlc", "requestedBy": ["user", "fpp-plugin-Foo"] }, ... ]
// Legacy schema (still read): a plain array of package-name strings, treated as
//   requestedBy ["user"]. Saving always rewrites in the new object schema.
//
// A requester is either the literal string "user" (installed via the Package
// Manager UI) or a plugin's repoName (installed as that plugin's dependency).

if (!defined('FPP_PACKAGES_INC')) {
    define('FPP_PACKAGES_INC', 1);

/////////////////////////////////////////////////////////////////////////////
// Manifest (userpackages.json) load / save + requester bookkeeping
/////////////////////////////////////////////////////////////////////////////

function UserPackagesFile()
{
    global $settings;
    return $settings['configDirectory'] . '/userpackages.json';
}

// Returns a normalized associative map: package name => array of requester
// strings. Accepts both the legacy string[] schema and the new object schema.
function LoadUserPackages()
{
    $file = UserPackagesFile();
    $map = array();
    if (!file_exists($file)) {
        return $map;
    }
    $data = json_decode(file_get_contents($file), true);
    if (!is_array($data)) {
        return $map;
    }
    foreach ($data as $entry) {
        if (is_string($entry)) {
            // Legacy entry: a bare package name installed via the UI.
            if (!isset($map[$entry])) {
                $map[$entry] = array();
            }
            if (!in_array('user', $map[$entry])) {
                $map[$entry][] = 'user';
            }
        } else if (is_array($entry) && isset($entry['package']) && is_string($entry['package'])) {
            $pkg = $entry['package'];
            if (!isset($map[$pkg])) {
                $map[$pkg] = array();
            }
            $reqs = (isset($entry['requestedBy']) && is_array($entry['requestedBy'])) ? $entry['requestedBy'] : array();
            foreach ($reqs as $r) {
                if (is_string($r) && $r !== '' && !in_array($r, $map[$pkg])) {
                    $map[$pkg][] = $r;
                }
            }
        }
    }
    return $map;
}

// Persists the normalized map back to disk in the new object schema. Packages
// whose requester list is empty are dropped from the file.
function SaveUserPackages($map)
{
    $out = array();
    foreach ($map as $pkg => $reqs) {
        $reqs = array_values(array_unique(array_filter($reqs, function ($r) {
            return is_string($r) && $r !== '';
        })));
        if (count($reqs) === 0) {
            continue;
        }
        $out[] = array('package' => $pkg, 'requestedBy' => $reqs);
    }
    file_put_contents(UserPackagesFile(), json_encode($out, JSON_PRETTY_PRINT));
}

// Returns the list of requesters currently recorded for a package.
function GetPackageRequesters($package)
{
    $map = LoadUserPackages();
    return isset($map[$package]) ? $map[$package] : array();
}

function AddPackageRequester($package, $requester)
{
    $map = LoadUserPackages();
    if (!isset($map[$package])) {
        $map[$package] = array();
    }
    if (!in_array($requester, $map[$package])) {
        $map[$package][] = $requester;
    }
    SaveUserPackages($map);
}

/////////////////////////////////////////////////////////////////////////////
// Hardened apt runner
/////////////////////////////////////////////////////////////////////////////

// Whether this platform has apt. FPP also runs on non-Debian platforms (Fedora,
// MacOS, UNKNOWN) where apt-get does not exist; on those, apt-based package
// management is simply not available. Callers use this to skip / refuse rather
// than shelling out to a missing apt-get.
function AptAvailable()
{
    // apt-get lives at /usr/bin/apt-get on every Debian-family platform (the
    // only ones that have it); non-Debian platforms (Fedora, MacOS) don't.
    return file_exists('/usr/bin/apt-get');
}

// Runs a shell command, streaming its combined stdout+stderr to the client as
// it goes, and returns the process exit code (0 == success). DEBIAN_FRONTEND is
// forced non-interactive so a package with a debconf prompt can never hang the
// request.
function RunAptStreaming($cmd)
{
    $full = 'DEBIAN_FRONTEND=noninteractive ' . $cmd . ' 2>&1';
    $proc = proc_open($full, array(1 => array('pipe', 'w')), $pipes);
    if (!is_resource($proc)) {
        echo "\nERROR: failed to start: $cmd\n";
        flush();
        return 255;
    }
    while (!feof($pipes[1])) {
        $chunk = fread($pipes[1], 4096);
        if ($chunk === false) {
            break;
        }
        echo $chunk;
        flush();
    }
    fclose($pipes[1]);
    return proc_close($proc);
}

// Refreshes package lists. Mirrors the robust boot path: waits for the dpkg
// lock instead of failing instantly, and retries a few times. Returns true on
// success.
function AptGetUpdate()
{
    if (!AptAvailable()) {
        echo "\nThis platform does not support system packages; skipping.\n";
        flush();
        return false;
    }
    for ($i = 1; $i <= 3; $i++) {
        if (RunAptStreaming("sudo apt-get -o DPkg::Lock::Timeout=60 update") === 0) {
            return true;
        }
        echo "\nWarning: 'apt-get update' failed (attempt $i), retrying...\n";
        flush();
        sleep(5);
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
// Install / remove with ownership tracking
/////////////////////////////////////////////////////////////////////////////

// Installs a system package on behalf of $requester ("user" or a plugin
// repoName). The manifest is only updated when apt actually succeeds, so a
// failed install is never recorded (and therefore never wrongly replayed on the
// next OS upgrade). Pass $doUpdate=false to skip 'apt-get update' when the
// caller has already refreshed the lists (e.g. installing several packages in a
// row). Returns true on success.
function InstallSystemPackage($package, $requester, $doUpdate = true)
{
    if (!AptAvailable()) {
        echo "\nERROR: cannot install package '$package' -- this platform does not support system packages. Install it from the plugin's fpp_install.sh instead.\n";
        flush();
        return false;
    }
    if ($doUpdate && !AptGetUpdate()) {
        echo "\nERROR: 'apt-get update' did not succeed; not installing '$package'.\n";
        flush();
        return false;
    }

    echo "\nInstalling package '$package' (requested by $requester)...\n";
    flush();
    $rc = RunAptStreaming("sudo apt-get -o DPkg::Lock::Timeout=60 install -y " . escapeshellarg($package));
    if ($rc !== 0) {
        echo "\nERROR: failed to install '$package' (apt exit $rc).\n";
        flush();
        return false;
    }

    AddPackageRequester($package, $requester);
    echo "\nInstalled '$package'.\n";
    flush();
    return true;
}

// Reinstalls a system package via 'apt-get install --reinstall'. The manifest
// is not modified -- reinstall simply refreshes the files for an already
// tracked/installed package (useful for repairing a broken install). Pass
// $doUpdate=false to skip 'apt-get update' when the caller has already
// refreshed the lists. Returns true on success.
// Keeps the same hardening/timeout/DEBIAN_FRONTEND handling as InstallSystemPackage.
function ReinstallSystemPackage($package, $doUpdate = true)
{
    if (!AptAvailable()) {
        echo "\nERROR: cannot reinstall package '$package' -- this platform does not support system packages.\n";
        flush();
        return false;
    }
    if ($doUpdate && !AptGetUpdate()) {
        echo "\nERROR: 'apt-get update' did not succeed; not reinstalling '$package'.\n";
        flush();
        return false;
    }

    echo "\nReinstalling package '$package'...\n";
    flush();
    $rc = RunAptStreaming("sudo apt-get -o DPkg::Lock::Timeout=60 install --reinstall -y " . escapeshellarg($package));
    if ($rc !== 0) {
        echo "\nERROR: failed to reinstall '$package' (apt exit $rc).\n";
        flush();
        return false;
    }

    echo "\nReinstalled '$package'.\n";
    flush();
    return true;
}

// Drops $requester from a package's requester list. If no requester remains the
// package is apt-removed; otherwise it is kept and left installed. Returns true
// if the package was actually apt-removed.
function RemoveSystemPackageRequester($package, $requester)
{
    $map = LoadUserPackages();
    $reqs = isset($map[$package]) ? $map[$package] : array();
    $reqs = array_values(array_filter($reqs, function ($r) use ($requester) {
        return $r !== $requester;
    }));

    if (count($reqs) > 0) {
        // Still wanted by someone else -> keep the package installed.
        $map[$package] = $reqs;
        SaveUserPackages($map);
        echo "\nPackage '$package' is still required by: " . implode(', ', $reqs) . " - leaving it installed.\n";
        flush();
        return false;
    }

    // Nobody needs it anymore -> remove it. Note we deliberately do NOT run a
    // blanket 'apt-get autoremove' here: under reference counting that could
    // pull out shared libraries another plugin still depends on.
    unset($map[$package]);
    SaveUserPackages($map);

    if (!AptAvailable()) {
        // Manifest claim dropped; nothing to apt-remove on a non-apt platform.
        echo "\nDropped '$package' from the package list (this platform does not support system packages).\n";
        flush();
        return false;
    }

    echo "\nRemoving package '$package' (no longer required)...\n";
    flush();
    $rc = RunAptStreaming("sudo apt-get -o DPkg::Lock::Timeout=60 remove -y " . escapeshellarg($package));
    if ($rc !== 0) {
        echo "\nWarning: 'apt-get remove' for '$package' exited $rc.\n";
        flush();
    }
    return $rc === 0;
}

} // FPP_PACKAGES_INC
