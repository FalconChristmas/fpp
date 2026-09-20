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
//   [ { "package": "vlc", "requestedBy": ["user", "fpp-plugin-Foo"] },
//     { "package": "libfoo", "requestedBy": ["user"], "via": ["vlc"] }, ... ]
// "via" names the package(s) whose install pulled this one in as a
// dependency; the Package Manager folds such rows under their parent.
// Legacy schema (still read): a plain array of package-name strings, treated as
//   requestedBy ["user"]. Saving always rewrites in the new object schema.
//
// A requester is either the literal string "user" (installed via the Package
// Manager UI) or a plugin's repoName (installed as that plugin's dependency).
//
// A package that is already installed and unknown to the manifest when it is
// first requested is NOT recorded: FPP didn't put it there, so FPP never
// removes it. (One FPP did install gains every later requester as usual, and
// one that is only there as another package's dependency -- apt's "auto"
// mark -- is claimed, since it would otherwise be lost the day its parent
// goes.) Every package an install newly adds is recorded against the
// requester, so a plugin's footprint comes out whole on uninstall. Without that
// rule a plugin declaring an already-present package (fontconfig, zip, ...)
// had it apt-removed on uninstall -- and 'apt-get remove' takes every reverse
// dependency with it (fontconfig -> libpango -> librsvg2 -> libavcodec ->
// ffmpeg, gstreamer1.0-libav). Manifests written before this rule existed can
// still carry such a claim; the dry-run in RemoveSystemPackageRequester()
// catches the cascading case, but a stand-alone image package (jq, zip) that
// an older FPP recorded under a plugin's name is still removed with it.
//
// These guards protect against accidents, not adversaries: a plugin's own
// fpp_install.sh runs with passwordless sudo and can do whatever it likes.

if (!defined('FPP_PACKAGES_INC')) {
    define('FPP_PACKAGES_INC', 1);

require_once __DIR__ . '/oplog.inc.php';

/////////////////////////////////////////////////////////////////////////////
// Output: progress text goes to the caller only when it asked for a stream
/////////////////////////////////////////////////////////////////////////////

// Every message here used to be a bare echo. That is right for the Package
// Manager page and for ?stream=true plugin installs, but a plugin API caller
// without ?stream gets a JSON body, and text in front of it made it
// unparseable ("Installed 'x'.\n{"Status":"OK"}"). Callers that return JSON
// call PackagesSetStreaming(false) first. Messages are always written to
// fpp_plugin_manager.log so a silent failure stays diagnosable; $owner names
// the plugin (or 'user') the operation is for, so log lines can be tied to
// the install/uninstall they belong to.
//
// $GLOBALS rather than a bare assignment: the API loads controllers from
// inside a function (limonade require_once_dir), where a top-level
// assignment here would be a local.
if (!isset($GLOBALS['FPP_PACKAGES_STREAMING'])) {
    $GLOBALS['FPP_PACKAGES_STREAMING'] = true;
    $GLOBALS['FPP_PACKAGES_OWNER'] = '';
}

function PackagesSetStreaming($streaming, $owner = '')
{
    // Accept the ?stream= convention too ("false" is off).
    $GLOBALS['FPP_PACKAGES_STREAMING'] = (bool) $streaming && $streaming !== 'false';
    $GLOBALS['FPP_PACKAGES_OWNER'] = $owner;
}

function PackagesStreaming()
{
    return !empty($GLOBALS['FPP_PACKAGES_STREAMING']);
}

function PackagesMsg($msg)
{
    if (PackagesStreaming()) {
        echo "\n" . $msg . "\n";
        flush();
    }
    OpLog('fpp_plugin_manager.log', 'packages', $GLOBALS['FPP_PACKAGES_OWNER'] ?? '', $msg);
}

/////////////////////////////////////////////////////////////////////////////
// Manifest lock
/////////////////////////////////////////////////////////////////////////////

// Every manifest update is read-modify-write. Two plugin installs (or an
// install and an uninstall) running at once could each read the same file
// and the second save would drop the first one's claim -- which later means
// a package gets removed while something still needs it. AddPackageRequester
// holds the lock for its own read-modify-write; RemoveSystemPackageRequester
// holds it across the whole decision and the apt call so the claim is not
// dropped before the removal is real. Readers never lock.
//
// Not re-entrant: flock on a second descriptor of the same file blocks in
// the same process, so $fn must not call AddPackageRequester() or
// RemoveSystemPackageRequester(). The depth guard turns that mistake into a
// plain nested call rather than a hung apache worker.
function WithUserPackagesLock($fn)
{
    static $depth = 0;
    if ($depth > 0) {
        return $fn();
    }
    $lockFile = UserPackagesFile() . '.lock';
    $fh = @fopen($lockFile, 'c');
    if ($fh !== false) {
        @chmod($lockFile, 0664);
    }
    if ($fh === false || !flock($fh, LOCK_EX)) {
        // Can't lock (read-only config dir, root-owned lock file): run
        // unlocked rather than refuse, but say so.
        if ($fh !== false) {
            fclose($fh);
        }
        OpLog('fpp_plugin_manager.log', 'packages', '', "Warning: could not lock $lockFile; continuing unlocked.");
        return $fn();
    }
    $depth++;
    try {
        return $fn();
    } finally {
        $depth--;
        flock($fh, LOCK_UN);
        fclose($fh);
    }
}

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
    $GLOBALS['FPP_PACKAGES_VIA'] = array(); // rebuilt from disk each load
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
            if (!ValidPackageName($entry)) {
                continue;
            }
            if (!isset($map[$entry])) {
                $map[$entry] = array();
            }
            if (!in_array('user', $map[$entry])) {
                $map[$entry][] = 'user';
            }
        } else if (is_array($entry) && isset($entry['package']) && ValidPackageName($entry['package'])) {
            // Only well-formed values get in: the Package Manager page renders
            // these names, and the file is writable through the config API and
            // backup restore. A requester is 'user' or a repoName (same rule as
            // InstallPlugin's allow-list).
            $pkg = $entry['package'];
            if (!isset($map[$pkg])) {
                $map[$pkg] = array();
            }
            $reqs = (isset($entry['requestedBy']) && is_array($entry['requestedBy'])) ? $entry['requestedBy'] : array();
            foreach ($reqs as $r) {
                if (is_string($r) && preg_match('/^[A-Za-z0-9_.-]+$/', $r) && !in_array($r, $map[$pkg])) {
                    $map[$pkg][] = $r;
                }
            }
            if (!count($map[$pkg])) {
                // No valid requester (hand-edited file): not tracked, so it
                // can't become a removal candidate.
                unset($map[$pkg]);
                continue;
            }
            if (isset($entry['via']) && is_array($entry['via'])) {
                foreach ($entry['via'] as $v) {
                    if (ValidPackageName($v)) {
                        SetPackageVia($pkg, $v);
                    }
                }
            }
        }
    }
    return $map;
}

// The packages whose install pulled $package in as a dependency ("via").
// Kept beside the requester map rather than in it so the many callers that
// only care about requesters stay as they are; Load fills it, Save writes it.
function PackageVia($package)
{
    return isset($GLOBALS['FPP_PACKAGES_VIA'][$package]) ? $GLOBALS['FPP_PACKAGES_VIA'][$package] : array();
}

function SetPackageVia($package, $via)
{
    if (!isset($GLOBALS['FPP_PACKAGES_VIA'][$package])) {
        $GLOBALS['FPP_PACKAGES_VIA'][$package] = array();
    }
    if ($via !== $package && !in_array($via, $GLOBALS['FPP_PACKAGES_VIA'][$package])) {
        $GLOBALS['FPP_PACKAGES_VIA'][$package][] = $via;
    }
}

// Persists the normalized map back to disk in the new object schema. Packages
// whose requester list is empty are dropped from the file. Written to a temp
// file and renamed so an unlocked reader (the Package Manager page) never sees
// a truncated file.
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
        $entry = array('package' => $pkg, 'requestedBy' => $reqs);
        // Only parents that are still tracked are worth naming.
        $via = array_values(array_filter(PackageVia($pkg), function ($v) use ($map) {
            return isset($map[$v]);
        }));
        if (count($via)) {
            $entry['via'] = $via;
        }
        $out[] = $entry;
    }
    $file = UserPackagesFile();
    $tmp = $file . '.tmp';
    if (file_put_contents($tmp, json_encode($out, JSON_PRETTY_PRINT)) === false || !rename($tmp, $file)) {
        @unlink($tmp);
        file_put_contents($file, json_encode($out, JSON_PRETTY_PRINT));
    }
}

// Returns the list of requesters currently recorded for a package.
function GetPackageRequesters($package)
{
    $map = LoadUserPackages();
    return isset($map[$package]) ? $map[$package] : array();
}

function AddPackageRequester($package, $requester)
{
    AddPackageRequesters(array($package), $requester);
}

// Records $requester against every package in $packages under one lock.
// $via, when given, is the package whose install pulled the others in; it is
// recorded on every package in the list other than itself.
function AddPackageRequesters($packages, $requester, $via = '')
{
    WithUserPackagesLock(function () use ($packages, $requester, $via) {
        $map = LoadUserPackages();
        foreach ($packages as $package) {
            if (!isset($map[$package])) {
                $map[$package] = array();
            }
            if (!in_array($requester, $map[$package])) {
                $map[$package][] = $requester;
            }
            if ($via !== '' && $via !== $package) {
                SetPackageVia($package, $via);
            }
        }
        SaveUserPackages($map);
    });
}

// The tracked packages that came in as dependencies of $package (one level:
// an install records every package it added against the top-level one).
function PackagesInstalledVia($package)
{
    $children = array();
    foreach (array_keys(LoadUserPackages()) as $pkg) {
        if (in_array($package, PackageVia($pkg))) {
            $children[] = $pkg;
        }
    }
    return $children;
}

/////////////////////////////////////////////////////////////////////////////
// Package name checks
/////////////////////////////////////////////////////////////////////////////

// A package name apt/dpkg will treat as exactly one package: Debian policy
// characters, optionally ":arch". This is what keeps apt's own argument syntax
// out of the sudo apt-get calls below -- "pkg-" means REMOVE to apt-get
// install, "*"/"?"/"^" are globs and regexes, "pkg=ver" / "pkg/suite" pin,
// and a leading "-" is an option. escapeshellarg() stops none of those. A
// trailing "+" is allowed (g++, libsigc++): apt resolves an exact name before
// treating "+" as a modifier. Names come from third-party pluginInfo.json
// files and from the Package Manager form, so every entry point validates
// before touching apt.
function ValidPackageName($package)
{
    return is_string($package)
        && strlen($package) <= 255
        && preg_match('/^[a-z0-9][a-z0-9+.-]*[a-z0-9+](:[a-z0-9-]+)?$/D', $package) === 1;
}

// "libfoo:armhf" -> "libfoo"
function PackageBaseName($package)
{
    return preg_replace('/:[^:]+$/', '', $package);
}

// Whether dpkg currently has the package fully installed.
function PackageIsInstalled($package)
{
    $status = trim(shell_exec("dpkg-query -W -f='\${db:Status-Status}' " . escapeshellarg($package) . " 2>/dev/null") ?? '');
    return $status === 'installed';
}

// Installed packages that depend on $package, other than those in $except.
function InstalledReverseDependencies($package, $except = array())
{
    exec('apt-cache rdepends --installed --important ' . escapeshellarg(PackageBaseName($package)) . ' 2>/dev/null', $lines, $rc);
    $rdeps = array();
    if ($rc !== 0) {
        return $rdeps;
    }
    $except = array_map('PackageBaseName', $except);
    $except[] = PackageBaseName($package); // its own foreign-arch twin (fontconfig:armhf) is listed too
    foreach ($lines as $line) {
        if (!preg_match('/^\s+\|?(\S+)/', $line, $m)) {
            continue; // the package's own name and the "Reverse Depends:" header
        }
        $name = PackageBaseName($m[1]);
        if (!in_array($name, $except) && !in_array($name, $rdeps)) {
            $rdeps[] = $name;
        }
    }
    return $rdeps;
}

// Whether apt knows a package by exactly this name. Needed on the install
// side because an unknown name containing "." or "+" falls back to an
// unanchored regex in apt-get: 'zlib1.' (a typo) installs zlib1g and
// zlib1g-dev. 'apt-cache pkgnames <prefix>' lists exact names only; do not
// use 'apt-cache policy', it has the same fallback.
function PackageKnownToApt($package)
{
    $base = PackageBaseName($package);
    exec('apt-cache pkgnames ' . escapeshellarg($base) . ' 2>/dev/null', $names, $rc);
    return $rc === 0 && in_array($base, $names, true);
}

// Parse apt-get -s output into what it would do beyond the packages named
// in $named: 'remove' = other packages it would remove, 'change' = packages
// it would install or upgrade on the way (apt's resolver can decide a remove
// needs a libc6 upgrade), 'new' = packages it would newly install (an
// "Inst name (ver ...)" line with no bracketed old version). Names are
// arch-stripped. null means the simulation itself failed.
function AptSimulate($args, $named)
{
    exec('apt-get -s ' . $args . ' 2>&1', $lines, $rc);
    if ($rc !== 0) {
        return null;
    }
    $named = array_map('PackageBaseName', $named);
    $result = array('remove' => array(), 'change' => array(), 'new' => array());
    foreach ($lines as $line) {
        if (!preg_match('/^(Remv|Inst|Conf) (\S+)(.*)$/', $line, $m)) {
            continue;
        }
        $name = PackageBaseName($m[2]);
        // "Inst name [old] (new ...)" is an upgrade; "Inst name (new ...)" is new.
        if ($m[1] === 'Inst' && !preg_match('/^\s*\[/', $m[3]) && !in_array($name, $result['new'])) {
            $result['new'][] = $name;
        }
        if (in_array($name, $named)) {
            continue;
        }
        $key = $m[1] === 'Remv' ? 'remove' : 'change';
        if (!in_array($name, $result[$key])) {
            $result[$key][] = $name;
        }
    }
    return $result;
}

// Dry-run removing $packages together. Both 'remove' and 'change' empty means
// the removal is self-contained. No sudo: apt-get -s needs no lock as a
// normal user, and as root it would block behind a concurrent apt run.
function PackagesRemovedWith($packages)
{
    $packages = is_array($packages) ? $packages : array($packages);
    return AptSimulate('remove -y ' . implode(' ', array_map('escapeshellarg', $packages)), $packages);
}

// Dry-run installing $package: the packages apt would newly add, the package
// itself included. null if apt cannot resolve it.
function PackagesNewlyInstalledBy($package)
{
    $sim = AptSimulate('install -y ' . escapeshellarg($package), array($package));
    return $sim === null ? null : $sim['new'];
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

// The sudo prefix every apt/dpkg call uses. DEBIAN_FRONTEND has to be set
// AFTER sudo (via env): sudoers env_reset drops anything set in front of it,
// and a debconf prompt would otherwise be able to hang the request.
define('FPP_SUDO_APT', 'sudo env DEBIAN_FRONTEND=noninteractive apt-get -o DPkg::Lock::Timeout=60');
define('FPP_SUDO_DPKG', 'sudo env DEBIAN_FRONTEND=noninteractive dpkg');

// Runs a shell command, streaming its combined stdout+stderr to the client as
// it goes (when streaming), and returns the process exit code (0 == success).
// The full transcript is written to fpp_plugin_manager.log either way.
function RunAptStreaming($cmd)
{
    // The command line is logged up front and the transcript when it ends,
    // so the two stamps bracket the run.
    OpLog('fpp_plugin_manager.log', 'apt', $GLOBALS['FPP_PACKAGES_OWNER'] ?? '', "\$ $cmd");
    $proc = proc_open($cmd . ' 2>&1', array(1 => array('pipe', 'w')), $pipes);
    if (!is_resource($proc)) {
        PackagesMsg("ERROR: failed to start: $cmd");
        return 255;
    }
    $transcript = '';
    while (!feof($pipes[1])) {
        $chunk = fread($pipes[1], 4096);
        if ($chunk === false) {
            break;
        }
        $transcript .= $chunk;
        if (PackagesStreaming()) {
            echo $chunk;
            flush();
        }
    }
    fclose($pipes[1]);
    $rc = proc_close($proc);
    OpLog('fpp_plugin_manager.log', 'apt', $GLOBALS['FPP_PACKAGES_OWNER'] ?? '',
        trim($transcript) . "\n(exit $rc)");
    return $rc;
}

// Refreshes package lists. Mirrors the robust boot path: waits for the dpkg
// lock instead of failing instantly, and retries a few times. Returns true on
// success.
function AptGetUpdate()
{
    if (!AptAvailable()) {
        PackagesMsg("This platform does not support system packages; skipping.");
        return false;
    }
    for ($i = 1; $i <= 3; $i++) {
        if (RunAptStreaming(FPP_SUDO_APT . " update") === 0) {
            return true;
        }
        PackagesMsg("Warning: 'apt-get update' failed (attempt $i), retrying...");
        sleep(5);
    }
    return false;
}

// An interrupted dpkg run (power loss mid-upgrade is the usual cause) leaves
// packages unpacked or half-configured, and apt then refuses every operation
// with "dpkg was interrupted, you must manually run 'dpkg --configure -a'".
// Finishing that configuration is the one repair that decides nothing new --
// it only completes what was already started -- so do it rather than fail
// the install. Anything beyond this (-f install, autoremove) is deliberately
// left to the user. Only the two states --configure can actually fix trigger
// it; 'dpkg --audit' also reports half-installed / missing-file states that
// would otherwise warn on every operation forever.
function FinishInterruptedDpkg()
{
    if (!AptAvailable()) {
        return true;
    }
    $audit = shell_exec('LC_ALL=C dpkg --audit 2>/dev/null') ?? '';
    if (strpos($audit, 'half configured') === false && strpos($audit, 'unpacked but not yet configured') === false) {
        return true;
    }
    PackagesMsg("Finishing interrupted package configuration before continuing...");
    $rc = RunAptStreaming(FPP_SUDO_DPKG . " --configure -a");
    if ($rc !== 0) {
        PackagesMsg("Warning: 'dpkg --configure -a' exited $rc; package operations may fail until this is resolved.");
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////
// Install / remove with ownership tracking
/////////////////////////////////////////////////////////////////////////////

// Installs a system package on behalf of $requester ("user" or a plugin
// repoName), recording it and every package apt newly adds for it (see the
// header). Only updated when apt actually succeeds, so a failed install is
// never recorded (and never wrongly replayed on the next OS upgrade).
//
// A package that is already installed is left exactly as it is -- no apt
// call (which would upgrade it). If the manifest knows it, $requester is
// added; if not, it came with the image or was installed by hand, and FPP
// does not take it over -- unless apt has it marked "auto" (present only as
// some other package's dependency): then the claim is recorded, mainly so
// the post-fppos-upgrade replay of the manifest brings it back.
//
// Pass $doUpdate=false to skip 'apt-get update' when the caller has already
// refreshed the lists. Returns true when the package is installed afterwards.
function InstallSystemPackage($package, $requester, $doUpdate = true)
{
    if (!ValidPackageName($package)) {
        PackagesMsg("ERROR: refusing to install '$package': not a valid package name.");
        return false;
    }
    // Manifest keys are arch-less (that is how apt reports what it installs);
    // the apt calls below still get the name as declared.
    $key = PackageBaseName($package);
    if (!AptAvailable()) {
        PackagesMsg("ERROR: cannot install package '$package' -- this platform does not support system packages. Install it from the plugin's fpp_install.sh instead.");
        return false;
    }
    if (PackageIsInstalled($package)) {
        $owners = GetPackageRequesters($key);
        if (in_array($requester, $owners)) {
            PackagesMsg("Package '$package' is already installed (required by $requester); nothing to do.");
        } else if (count($owners) > 0) {
            AddPackageRequester($key, $requester);
            PackagesMsg("Package '$package' is already installed; recorded $requester as also requiring it.");
        } else if (PackageIsAutoInstalled($package)) {
            AddPackageRequester($key, $requester);
            PackagesMsg("Package '$package' was already installed as a dependency of another package; recorded $requester as requiring it.");
        } else {
            PackagesMsg("Package '$package' is already installed; leaving it as it is (it will not be removed if $requester is uninstalled).");
        }
        return true;
    }
    FinishInterruptedDpkg();
    if ($doUpdate && !AptGetUpdate()) {
        PackagesMsg("ERROR: 'apt-get update' did not succeed; not installing '$package'.");
        return false;
    }
    if (!PackageKnownToApt($package)) {
        PackagesMsg("ERROR: no package named '$package' is available; check the name (requested by $requester).");
        return false;
    }
    // What this install will add, decided before apt runs: afterwards
    // "already there" and "just installed" look the same.
    $new = PackagesNewlyInstalledBy($package);
    if ($new === null) {
        $new = array($package);
    }

    PackagesMsg("Installing package '$package' (requested by " . ($requester === 'user' ? 'the Package Manager' : $requester) . ")...");
    $rc = RunAptStreaming(FPP_SUDO_APT . " install -y " . escapeshellarg($package));
    if ($rc !== 0) {
        PackagesMsg("ERROR: failed to install '$package' (apt exit $rc).");
        return false;
    }

    if (!in_array($key, $new)) {
        $new[] = $key;
    }
    AddPackageRequesters($new, $requester, $key);
    $deps = array_values(array_diff($new, array($key)));
    PackagesMsg("Installed '$package'" . (count($deps) ? " (with " . implode(', ', $deps) . ")" : "") . ".");
    return true;
}

// Whether apt has the package marked auto-installed, i.e. it is on the box
// only because something else depended on it.
function PackageIsAutoInstalled($package)
{
    exec('apt-mark showauto ' . escapeshellarg(PackageBaseName($package)) . ' 2>/dev/null', $lines, $rc);
    return $rc === 0 && in_array(PackageBaseName($package), array_map('trim', $lines), true);
}

// Reinstalls a system package via 'apt-get install --reinstall'. The manifest
// is not modified -- reinstall simply refreshes the files for an already
// tracked/installed package (useful for repairing a broken install). Pass
// $doUpdate=false to skip 'apt-get update' when the caller has already
// refreshed the lists. Returns true on success.
function ReinstallSystemPackage($package, $doUpdate = true)
{
    if (!ValidPackageName($package)) {
        PackagesMsg("ERROR: refusing to reinstall '$package': not a valid package name.");
        return false;
    }
    if (!AptAvailable()) {
        PackagesMsg("ERROR: cannot reinstall package '$package' -- this platform does not support system packages.");
        return false;
    }
    FinishInterruptedDpkg();
    if ($doUpdate && !AptGetUpdate()) {
        PackagesMsg("ERROR: 'apt-get update' did not succeed; not reinstalling '$package'.");
        return false;
    }
    if (!PackageKnownToApt($package)) {
        PackagesMsg("ERROR: no package named '$package' is available; check the name.");
        return false;
    }

    PackagesMsg("Reinstalling package '$package'...");
    $rc = RunAptStreaming(FPP_SUDO_APT . " install --reinstall -y " . escapeshellarg($package));
    if ($rc !== 0) {
        PackagesMsg("ERROR: failed to reinstall '$package' (apt exit $rc).");
        return false;
    }

    PackagesMsg("Reinstalled '$package'.");
    return true;
}

// Drops $requester from one package. See ReleasePackageClaims().
function RemoveSystemPackageRequester($package, $requester)
{
    $removed = ReleasePackageClaims(array($package), $requester);
    return count($removed) === 1;
}

// Drops $requester from every package in $packages. Whatever is left with no
// requester is apt-removed -- in ONE apt call, because a plugin's footprint
// (the package it declared plus the dependencies that install pulled in) can
// only come out together: removing the dependency alone is refused (its
// parent depends on it) and removing the parent alone strands the
// dependency. The manifest entries are dropped only once the removal has
// actually happened (or a package turns out not to be installed), so a
// refusal or a failed apt run leaves the entries -- and the user's rows in
// the Package Manager -- exactly as they were. Returns the list of packages
// actually apt-removed.
function ReleasePackageClaims($packages, $requester)
{
    return WithUserPackagesLock(function () use ($packages, $requester) {
        $map = LoadUserPackages();
        $candidates = array();
        foreach ($packages as $package) {
            if (!ValidPackageName($package)) {
                PackagesMsg("Ignoring invalid package name '$package'.");
                continue;
            }
            $package = PackageBaseName($package); // manifest keys are arch-less
            if (!isset($map[$package])) {
                // Never in the manifest -> FPP didn't install it (it was
                // already on the box, or its install failed), so FPP doesn't
                // get to remove it.
                PackagesMsg("Package '$package' was not installed by FPP; skipping.");
                continue;
            }
            $reqs = array_values(array_filter($map[$package], function ($r) use ($requester) {
                return $r !== $requester;
            }));
            if (count($reqs) > 0) {
                // Still wanted by someone else -> keep the package installed.
                $map[$package] = $reqs;
                $labels = array_map(function ($r) {
                    return $r === 'user' ? 'the Package Manager' : $r;
                }, $reqs);
                PackagesMsg("Package '$package' is still required by: " . implode(', ', $labels) . " - leaving it installed.");
                continue;
            }
            if (!AptAvailable()) {
                unset($map[$package]);
                PackagesMsg("Dropped '$package' from the package list (this platform does not support system packages).");
                continue;
            }
            if (!PackageIsInstalled($package)) {
                unset($map[$package]);
                PackagesMsg("Package '$package' is not installed; nothing to remove.");
                continue;
            }
            $candidates[] = $package;
        }
        SaveUserPackages($map);
        if (!count($candidates)) {
            return array();
        }

        FinishInterruptedDpkg();

        // 'apt-get remove' also removes every package that depends on the
        // target (see the header), so refuse anything that isn't
        // self-contained. This is what protects a manifest written before
        // "already installed -> not ours" existed. Slow on a Pi Zero /
        // BeagleBone (apt loads its whole cache), hence the progress line.
        $list = implode(', ', $candidates);
        PackagesMsg("Checking what apt would remove along with $list...");
        $others = PackagesRemovedWith($candidates);
        if ($others !== null && count($others['remove']) > 0) {
            // Something outside the batch depends on one of these (plugin 1
            // brought in B for A; C, installed later by hand or as an
            // already-present dependency, needs B too). Keep B, still remove
            // the rest: drop every candidate with an installed reverse
            // dependency outside the batch and simulate again.
            $kept = array();
            $usedBy = array();
            foreach ($candidates as $package) {
                $rdeps = InstalledReverseDependencies($package, $candidates);
                if (count($rdeps) > 0) {
                    $kept[] = $package;
                    $usedBy[$package] = $rdeps;
                }
            }
            if (count($kept) > 0) {
                foreach ($kept as $package) {
                    PackagesMsg("Package '$package' is still used by " . implode(', ', $usedBy[$package]) . " - leaving it installed" . ($requester !== 'user' ? " and no longer tracking it" : "") . ".");
                    if ($requester !== 'user') {
                        unset($map[$package]);
                    }
                }
                SaveUserPackages($map);
                $candidates = array_values(array_diff($candidates, $kept));
                if (!count($candidates)) {
                    return array();
                }
                $list = implode(', ', $candidates);
                $others = PackagesRemovedWith($candidates);
            }
        }
        if ($others === null) {
            PackagesMsg("Warning: could not simulate removing $list; leaving installed.");
            return array();
        }
        if (count($others['remove']) > 0 || count($others['change']) > 0) {
            $msg = "Not removing $list: apt would also";
            if (count($others['remove']) > 0) {
                $msg .= " remove " . count($others['remove']) . " other package(s) ("
                    . implode(', ', array_slice($others['remove'], 0, 8)) . (count($others['remove']) > 8 ? ', ...' : '') . ")";
            }
            if (count($others['change']) > 0) {
                $msg .= (count($others['remove']) > 0 ? " and" : "") . " install or upgrade " . count($others['change']) . " package(s)";
            }
            if ($requester !== 'user') {
                // The plugin is going away (or no longer declares these), and
                // the packages are demonstrably not FPP's to remove: drop the
                // claims so they aren't re-simulated on every later upgrade
                // and don't sit under a plugin that is no longer installed.
                // The not-in-manifest guard protects them from here on. A
                // user's rows are kept so they can see why they stayed.
                foreach ($candidates as $package) {
                    unset($map[$package]);
                }
                SaveUserPackages($map);
                PackagesMsg($msg . ". Leaving installed and no longer tracking.");
            } else {
                PackagesMsg($msg . ". Leaving installed.");
            }
            return array();
        }

        // Note we deliberately do NOT run 'apt-get autoremove' here. It works
        // off apt's own auto/manual marks, not our manifest, and those marks
        // don't know what FPP or a plugin's fpp_install.sh actually uses: a
        // library pulled in as somebody's dependency and then used directly
        // by another plugin (or by core) is "auto" and gets removed the moment
        // its original parent goes. We only ever remove what we were asked
        // about.
        PackagesMsg("Removing package(s) $list (no longer required)...");
        $rc = RunAptStreaming(FPP_SUDO_APT . " remove -y " . implode(' ', array_map('escapeshellarg', $candidates)));
        if ($rc !== 0) {
            PackagesMsg("Warning: 'apt-get remove' exited $rc; keeping $list in the package list so it can be retried.");
            return array();
        }
        foreach ($candidates as $package) {
            unset($map[$package]);
        }
        SaveUserPackages($map);
        return $candidates;
    });
}

} // FPP_PACKAGES_INC
