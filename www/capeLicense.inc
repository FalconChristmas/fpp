<?php
/*
 * A cape whose signature verifies but which is not attached to identifiable
 * hardware -- no i2c EEPROM and no device serial -- is what a signed EEPROM
 * image lifted off the board it was licensed to looks like.  Cape detection
 * records that in cape-info.json as `licenseCheck` and reports nothing, because
 * it runs before the network exists.
 *
 * Two things then report it, and they need both halves for different reasons.
 * The device (src/CapeLicenseNotify.cpp) covers a headless install nobody ever
 * opens a browser against; the browser covers an FPP with no route out that is
 * administered from a laptop that has one, which is an ordinary show-network
 * layout.  Either one succeeding is enough, so they share the state file below
 * and the same rules about when to send.
 *
 * Lawful basis is legitimate interests -- GDPR Art 6(1)(f), Recital 47 -- not
 * consent; src/CapeLicenseNotify.h carries the reasoning and the limits that
 * balance depends on, and both halves have to keep obeying them.
 *
 * This file is the browser half's server side: it decides whether a request is
 * due and records the outcome the page reports back.  It must agree with the
 * C++ on the URL, the fields, and the intervals -- if the two ever disagree the
 * symptom is a device that notifies twice, not one that notifies wrongly.
 */

// Retry until something is delivered, then leave it alone for a week.  The
// vendor wants to know the situation is still current, not to hear daily.
define('CAPE_LICENSE_RETRY_HOURS', 24);
define('CAPE_LICENSE_REPEAT_DAYS', 7);

function capeLicenseStateFile()
{
    global $settings;
    return $settings['configDirectory'] . "/cape_license_state.json";
}

function capeLicenseReadState()
{
    $f = capeLicenseStateFile();
    if (is_readable($f)) {
        $state = json_decode(file_get_contents($f), true);
        if (is_array($state)) {
            return $state;
        }
    }
    return array();
}

/**
 * Which cape this state refers to.  A different cape -- or the same one
 * re-signed -- is a new detection and reports at once rather than inheriting
 * the previous one's quiet period.  Must match capeIdentity() in the C++.
 */
function capeLicenseIdentity($capeInfo)
{
    $id = "";
    foreach (array('id', 'serialNumber', 'cs', 'verifiedKeyId') as $k) {
        $id .= isset($capeInfo[$k]) ? $capeInfo[$k] : "";
        $id .= "/";
    }
    return $id;
}

/**
 * Where the notification goes.
 *
 * `licenseCheckUrl` is the endpoint a vendor can declare in their EEPROM image
 * to receive this and nothing else.  Where it is absent -- every EEPROM in the
 * field today -- the logo URL is used, because that is the host the old
 * mechanism reached by hijacking the <img>, so it is known to be the vendor's
 * and known to be in the CSP.  Coverage does not regress because an EEPROM
 * predates the endpoint.
 */
function capeLicenseNotifyURL($capeInfo)
{
    if (!isset($capeInfo['vendor'])) {
        return "";
    }
    $vendor = $capeInfo['vendor'];
    $url = "";
    if (isset($vendor['licenseCheckUrl'])) {
        $url = $vendor['licenseCheckUrl'];
    } else if (isset($vendor['image'])) {
        $url = $vendor['image'];
    } else if (isset($vendor['url'])) {
        $url = $vendor['url'];
    }
    if ($url == "" || strpos($url, "http") !== 0) {
        return "";
    }

    // What the vendor needs to match this against their order book, and no
    // more.  It is the same set the hijacked logo fetch already sent, so naming
    // the request for what it is discloses nothing new.
    $params = array('lc' => '1');
    foreach (array('id' => 'id', 'serialNumber' => 'sn', 'cs' => 'cs', 'verifiedKeyId' => 'key') as $field => $param) {
        if (isset($capeInfo[$field]) && $capeInfo[$field] != "") {
            $params[$param] = $capeInfo[$field];
        }
    }
    return $url . (strpos($url, '?') === false ? '?' : '&') . http_build_query($params);
}

/**
 * The URL the browser should request, or "" when nothing is due.  Returns ""
 * for the overwhelmingly common case of no cape or a cape bound to real
 * hardware, so callers can ask unconditionally.
 */
function capeLicenseDueURL($capeInfo)
{
    if (!is_array($capeInfo) || !isset($capeInfo['licenseCheck'])) {
        return "";
    }
    // Deliberately NOT gated on hideExternalURLs: that setting is kiosk mode --
    // it hides links so somebody at the kiosk cannot navigate off the box -- and
    // it is not a statement about whether FPP may make a request.  It does still
    // blank the vendor link and e-mail address on this page, which is what it is
    // for.
    $state = capeLicenseReadState();
    if (isset($state['cape']) && $state['cape'] === capeLicenseIdentity($capeInfo)) {
        $lastSuccess = isset($state['lastSuccess']) ? intval($state['lastSuccess']) : 0;
        $lastAttempt = isset($state['lastAttempt']) ? intval($state['lastAttempt']) : 0;
        if ($lastSuccess && (time() - $lastSuccess) < (CAPE_LICENSE_REPEAT_DAYS * 24 * 60 * 60)) {
            return "";
        }
        if (!$lastSuccess && (time() - $lastAttempt) < (CAPE_LICENSE_RETRY_HOURS * 60 * 60)) {
            return "";
        }
    }
    return capeLicenseNotifyURL($capeInfo);
}

function capeLicenseRecord($capeInfo, $delivered)
{
    $state = capeLicenseReadState();
    $state['cape'] = capeLicenseIdentity($capeInfo);
    $state['lastAttempt'] = time();
    if ($delivered) {
        $state['lastSuccess'] = time();
        $state['source'] = 'browser';
    }
    @file_put_contents(capeLicenseStateFile(), json_encode($state), LOCK_EX);
}
