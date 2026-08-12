<?php

function getFPPVersion()
{
    return "Unknown";
}
function getFPPBranch()
{
    return "Unknown";
}
function getFPPVersionFloat()
{
    return 99.999;
}
function getFPPVersionFloatStr()
{
    return "99.999";
}
function getFPPMajorVersion()
{
    return "99";
}
function getFPPMinorVersion()
{
    return "999";
}
function getFPPPatchVersion()
{
    return "";
}
function getFPPVersionTriplet()
{
    return "99.999";
}

// Mirrors the generated www/fppversion.php: emits its own <script> block, so
// callers (plugins.php, packages.php) must not wrap it in one.
function writeFPPVersionJavascriptFunctions()
{
    ?>
    <script>
        function getFPPVersion() { return "Unknown"; }
        function getFPPBranch() { return "Unknown"; }
        function getFPPVersionFloat() { return 99.999; }
        function getFPPVersionFloatStr() { return "99.999"; }
        function getFPPMajorVersion() { return "99"; }
        function getFPPMinorVersion() { return "999"; }
        function getFPPPatchVersion() { return ""; }
        function getFPPVersionTriplet() { return "99.999"; }
    </script>
    <?
}
?>