<?php
/*
 * The PHP half of etc/jurisdictions.json.
 *
 * The policy itself lives in that file, once, and this reads it.  The C++ half
 * is src/non-gpl/CapeUtils/CapeUtils.cpp (inferJurisdiction / priorOptInRequired)
 * and the two MUST answer identically, because they decide the same thing about
 * the same box at different moments: cape detection decides at boot whether an
 * EEPROM may set a transmitting default, and the setup wizard decides whether
 * the privacy step starts unanswered or pre-filled.  A disagreement would show
 * up as a setting the wizard offers and the device then refuses.
 *
 * Three behaviours are load-bearing and are the ones to check if the halves
 * ever drift:
 *
 *   - Longest prefix wins.  'Europe/London' resolves to UK, not to EU via the
 *     shorter 'Europe/' entry.
 *   - A recorded answer always beats an inferred one, and inference is never
 *     written back to the settings file.
 *   - An unreadable policy file means prior opt-in IS required.  That is the
 *     recoverable direction: withholding a permissive default leaves the user
 *     free to set it, where wrongly allowing one cannot be taken back.  Note
 *     this is NOT the same as an unrecognised jurisdiction in a file that did
 *     load -- that takes priorOptInDefault, which ships false on purpose.
 */

function jurisdictionConfig()
{
    global $settings;
    static $cfg = null;
    if ($cfg !== null) {
        return $cfg;
    }
    $cfg = false;
    $f = $settings['fppDir'] . '/etc/jurisdictions.json';
    if (is_readable($f)) {
        $j = json_decode(file_get_contents($f), true);
        if (is_array($j) && isset($j['jurisdictions']) && is_array($j['jurisdictions'])) {
            $cfg = $j;
        }
    }
    return $cfg;
}

/**
 * Label => code map for the LegalJurisdiction setting's dropdown.
 *
 * Presentation is deliberately geographic and unordered by "recommendation":
 * steering the answer would be worse than not asking it.  The list is whatever
 * the policy file declares, so adding a region stays a data change.
 */
function jurisdictionOptions()
{
    $cfg = jurisdictionConfig();
    $opts = array();
    if ($cfg === false) {
        return $opts;
    }
    foreach ($cfg['jurisdictions'] as $code => $j) {
        $label = isset($j['description']) ? $j['description'] : $code;
        $opts[$label] = $code;
    }
    return $opts;
}

/**
 * The jurisdiction this box should be treated as being in.
 *
 * A recorded LegalJurisdiction wins outright.  Otherwise the time zone is used,
 * which every box already has -- so an unanswered box in Europe still gets the
 * strict path rather than waiting for a prompt.  Returns '' when nothing is
 * recorded and nothing can be inferred.
 */
function inferJurisdiction()
{
    global $settings;

    if (!empty($settings['LegalJurisdiction'])) {
        return $settings['LegalJurisdiction'];
    }
    $cfg = jurisdictionConfig();
    if ($cfg === false || empty($settings['TimeZone']) || !isset($cfg['timezonePrefix'])) {
        return '';
    }
    $tz = $settings['TimeZone'];
    $best = '';
    $bestPrefix = '';
    foreach ($cfg['timezonePrefix'] as $prefix => $code) {
        if (strncmp($tz, $prefix, strlen($prefix)) === 0 && strlen($prefix) > strlen($bestPrefix)) {
            $bestPrefix = $prefix;
            $best = $code;
        }
    }
    return $best;
}

/**
 * Does this jurisdiction require consent BEFORE the device transmits?
 *
 * $code defaults to the inferred jurisdiction; pass one to ask about a value
 * the user has picked in the wizard but not yet saved.
 */
function jurisdictionRequiresPriorOptIn($code = null)
{
    $cfg = jurisdictionConfig();
    if ($cfg === false) {
        return true;
    }
    $dflt = isset($cfg['priorOptInDefault']) ? (bool) $cfg['priorOptInDefault'] : false;
    if ($code === null) {
        $code = inferJurisdiction();
    }
    if ($code === '' || !isset($cfg['jurisdictions'][$code])) {
        return $dflt;
    }
    $j = $cfg['jurisdictions'][$code];
    return isset($j['priorOptIn']) ? (bool) $j['priorOptIn'] : $dflt;
}

/**
 * The jurisdiction a country code falls under.
 *
 * Used by the setup wizard's "Lookup" buttons, which the user presses
 * deliberately and whose answer they can see and change before anything is
 * saved.  $inEu is the geoip provider's own EU flag and is only a fallback for
 * a country the policy file does not list -- the list wins, because it also
 * carries the EEA states (Iceland, Liechtenstein, Norway) that those flags
 * leave out and the GDPR does not.
 *
 * Returns '' when nothing matches, so the caller can leave the field alone
 * rather than assert a wrong answer.
 */
function jurisdictionForCountry($country, $inEu = false)
{
    $cfg = jurisdictionConfig();
    if ($cfg === false || $country === '') {
        return '';
    }
    $country = strtoupper($country);
    foreach ($cfg['jurisdictions'] as $code => $j) {
        if (!empty($j['countryCodes']) && in_array($country, $j['countryCodes'], true)) {
            return $code;
        }
    }
    if ($inEu) {
        return 'EU';
    }
    return '';
}

/**
 * The Locale (etc/locale/<name>.json) a country code belongs to.
 *
 * Locale selects HOLIDAYS, which is not the same question as which law covers
 * you -- someone in Germany may well want Global.  It gets its own mapping,
 * declared in the locale files themselves so that adding a locale stays a data
 * change, and it is only ever a seed.
 *
 * Returns '' when no locale claims the country; Global is the fallback and
 * deliberately claims none, so it is never asserted over a user's choice.
 */
function localeForCountry($country)
{
    global $settings;

    if ($country === '') {
        return '';
    }
    $country = strtoupper($country);
    $dir = $settings['fppDir'] . '/etc/locale';
    foreach (scandir($dir) as $file) {
        if (!preg_match('/\.json$/', $file)) {
            continue;
        }
        $j = json_decode(file_get_contents($dir . '/' . $file), true);
        if (is_array($j) && !empty($j['countryCodes']) && in_array($country, $j['countryCodes'], true)) {
            return preg_replace('/\.json$/', '', $file);
        }
    }
    return '';
}
