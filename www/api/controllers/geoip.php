<?
require_once(__DIR__ . "/../../config.php");

/**
 * GeoIP lookup
 *
 * Server-side proxy for ipapi.co's IP geolocation lookup, used by the
 * Timezone/GeoLocation "Lookup"/"Detect" buttons.
 *
 * It is fetched here rather than from the browser so that only the PLAYER's
 * address is disclosed, and only when somebody presses the button -- and so
 * that no third-party host has to appear in the CSP, which is where the rest of
 * the UI has deliberately ended up.  (ipapi.co does send
 * Access-Control-Allow-Origin: *, so a browser-side call is not blocked by CORS
 * the way an older comment here claimed; connect-src is the only thing stopping
 * it, and that is ours to decide rather than a technical obstacle.)
 *
 * The response also carries an `fpp` object naming the settings the lookup
 * implies, so the mapping lives in one place instead of being reimplemented in
 * the browser.  Every one of them is a SEED: the buttons fill the fields in, the
 * user sees the result and can change it, and nothing is saved until they finish
 * setup.  Any field the lookup cannot determine is simply absent, so the caller
 * leaves that control alone rather than asserting a wrong answer.
 *
 * Note geoip resolves the PUBLIC IP: it says where the device reaches the
 * internet, not where the device is.  Fine for something shown and correctable,
 * which is the only way it is used.
 *
 * @route GET /api/geoip
 * @queryParam country string Two-letter code supplied in place of the one the
 *                            lookup would report -- used when the device has no
 *                            internet but the browser does, and to exercise a
 *                            region you are not in.  It changes what is
 *                            suggested, never what is stored, and the response
 *                            is marked with `fppCountryOverride`.
 * @queryParam nolookup int   With `country`, skip the network lookup entirely and
 *                            answer from local data.  For a caller whose own
 *                            lookup already failed.
 * @response 200 ipapi.co's JSON response, plus the settings it implies
 * ```json
 * {"ip": "1.2.3.4", "city": "Adelaide", "region": "South Australia", "timezone": "Australia/Adelaide",
 *  "latitude": -34.9, "longitude": 138.6, "country_code": "AU", "in_eu": false,
 *  "fpp": {"Locale": "Global", "LegalJurisdiction": "AU", "WifiRegulatoryDomain": "AU"}}
 * ```
 * @response 502 Lookup failed
 * ```json
 * {"error": "GeoIP lookup failed"}
 * ```
 */
function GetGeoIP()
{
    // The country normally comes from the caller's public IP.  ?country=XX
    // supplies it instead, for the two cases where the IP cannot:
    //
    //   - The device has no route to the internet but the browser does, which is
    //     an ordinary show-network layout.  The browser knows its own region
    //     without asking anyone (Intl), so it sends that and the mapping still
    //     runs here, against the same data files.
    //   - Testing a region you are not in, which is otherwise unreachable: the
    //     browser's location sensor is never consulted on this path, and cannot
    //     be anyway on a plain-HTTP LAN page.
    //
    // It changes only what is SUGGESTED.  Nothing is saved here, and the caller
    // was always free to pick these values by hand.
    $override = isset($_GET['country']) ? strtoupper($_GET['country']) : '';
    if ($override !== '' && !preg_match('/^[A-Z]{2}$/', $override)) {
        http_response_code(400);
        return json(['error' => 'country must be a two-letter code']);
    }

    // A caller that already knows the lookup will fail -- because its own just
    // did -- should not wait out the timeout again to be told so.  The mapping
    // is entirely local, so there is nothing to wait for.
    $noLookup = $override !== '' && !empty($_GET['nolookup']);

    // The browser's own time zone, for the offline fallback.  It is the one
    // thing a browser knows about where it is without asking anyone -- no
    // permission prompt, no network -- and it is enough to place the player
    // roughly on the map.
    $tzHint = isset($_GET['timezone']) ? $_GET['timezone'] : '';
    if ($tzHint !== '' && !preg_match('#^[A-Za-z][A-Za-z0-9_+/-]{0,63}$#', $tzHint)) {
        http_response_code(400);
        return json(['error' => 'invalid timezone']);
    }

    $data = false;
    if (!$noLookup && function_exists('curl_init')) {
        $ch = curl_init('https://ipapi.co/json/');
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 3);
        curl_setopt($ch, CURLOPT_TIMEOUT, 6);
        curl_setopt($ch, CURLOPT_FOLLOWLOCATION, true);
        curl_setopt($ch, CURLOPT_USERAGENT, 'FPP');
        $data = curl_exec($ch);
        curl_close($ch);
    }

    $j = json_decode($data === false ? '' : $data, true);
    if (!is_array($j)) {
        // An overridden country is still answerable with no network at all: the
        // mapping is local, and it is the half worth testing.  Everything the
        // lookup would have supplied is simply absent, which callers already
        // have to cope with.
        if ($override === '') {
            http_response_code(502);
            return json(['error' => 'GeoIP lookup failed']);
        }
        $j = array();
    }

    if ($override !== '') {
        $j['country_code'] = $override;
        $j['country'] = $override;
        // in_eu is the provider's own flag and cannot be trusted to agree with a
        // substituted country, so drop it and let the policy file's list decide.
        unset($j['in_eu']);
        $j['fppCountryOverride'] = true;
    }

    if ($tzHint !== '') {
        $j['timezone'] = $tzHint;
    }

    $j['fpp'] = GeoIPSuggestedSettingsWithCoords($j);
    if (isset($j['fpp']['Latitude']) && !isset($j['latitude'])) {
        // Flagged outside `fpp`, which is strictly settings to apply: these
        // coordinates are the time zone's reference city, and the caller should
        // be able to say so rather than presenting them as a fix.
        $j['fppCoordsFromTimeZone'] = true;
    }

    header('Content-Type: application/json');
    echo json_encode($j);
}

/**
 * Map a geoip response onto the settings the wizard's locale step holds.
 *
 * Only keys it can actually determine are returned.  Locale and jurisdiction
 * are separate lookups on purpose: Locale picks HOLIDAYS, jurisdiction picks
 * which privacy rules are assumed, and someone in Germany may legitimately want
 * the Global holiday set.
 *
 * @param array $j Decoded geoip response.
 * @return array Setting name => value, for whatever could be determined.
 */
function GeoIPSuggestedSettings($j)
{
    require_once __DIR__ . '/../../jurisdiction.inc';

    $out = array();
    $country = isset($j['country_code']) ? strtoupper($j['country_code']) : '';
    if ($country === '') {
        return $out;
    }

    $locale = localeForCountry($country);
    if ($locale !== '') {
        $out['Locale'] = $locale;
    }

    $jur = jurisdictionForCountry($country, !empty($j['in_eu']));
    if ($jur !== '') {
        $out['LegalJurisdiction'] = $jur;
    }

    // The regulatory domain is an ISO country code already, so it needs no
    // mapping -- but it does need checking against the list the setting offers,
    // since that list is not every country.
    $domains = SettingOptions('WifiRegulatoryDomain');
    if (in_array($country, $domains, true)) {
        $out['WifiRegulatoryDomain'] = $country;
    }

    return $out;
}

/**
 * Everything the response implies, including coordinates where they had to be
 * derived rather than looked up.
 *
 * Kept separate from GeoIPSuggestedSettings() so the mapping test harness keeps
 * testing the country mapping on its own.  Real coordinates from the lookup
 * always win: a zone's reference city is a fallback, not an improvement.
 *
 * @param array $j Decoded geoip response, possibly with a supplied timezone.
 * @return array Setting name => value.
 */
function GeoIPSuggestedSettingsWithCoords($j)
{
    $out = GeoIPSuggestedSettings($j);

    if (isset($j['latitude']) && $j['latitude'] !== '' && $j['latitude'] !== null) {
        return $out;
    }
    $tz = isset($j['timezone']) ? $j['timezone'] : '';
    $c = TimeZoneCoordinates($tz);
    if (!empty($c)) {
        $out['Latitude'] = (string) $c['latitude'];
        $out['Longitude'] = (string) $c['longitude'];
    }
    return $out;
}

/**
 * Approximate coordinates for an IANA time zone, from tzdata's own zone.tab.
 *
 * This is the offline half of the location lookup: the file ships with the
 * operating system, so a player with no route to the internet can still turn a
 * time zone into somewhere plausible on the map.  No network, no third party,
 * nothing added to the CSP.
 *
 * The accuracy is what zone.tab offers, which is the zone's reference city, not
 * the user's town -- good enough to stop the scheduler computing sunset for the
 * shipped default location, and not good enough to leave uncorrected if the
 * exact minute matters.  The caller says so.
 *
 * zone.tab's coordinates are ISO 6709 packed: +DDMM+DDDMM or +DDMMSS+DDDMMSS.
 *
 * @param string $tz IANA zone name, e.g. "America/New_York".
 * @return array ['latitude' => float, 'longitude' => float] or empty if unknown.
 */
function TimeZoneCoordinates($tz)
{
    if ($tz === '' || strpos($tz, '..') !== false) {
        return array();
    }
    $f = '/usr/share/zoneinfo/zone.tab';
    if (!is_readable($f)) {
        return array();
    }
    foreach (file($f) as $line) {
        if ($line === '' || $line[0] === '#') {
            continue;
        }
        $cols = preg_split('/\t+/', trim($line));
        if (count($cols) < 3 || $cols[2] !== $tz) {
            continue;
        }
        if (!preg_match('/^([+-])(\d{2})(\d{2})(\d{2})?([+-])(\d{3})(\d{2})(\d{2})?$/', $cols[1], $m)) {
            return array();
        }
        $lat = $m[2] + $m[3] / 60 + (isset($m[4]) && $m[4] !== '' ? $m[4] / 3600 : 0);
        $lon = $m[6] + $m[7] / 60 + (isset($m[8]) && $m[8] !== '' ? $m[8] / 3600 : 0);
        return array(
            'latitude' => round($m[1] === '-' ? -$lat : $lat, 4),
            'longitude' => round($m[5] === '-' ? -$lon : $lon, 4),
        );
    }
    return array();
}

/**
 * The values a select-type setting offers, from settings.json.
 *
 * @param string $name Setting name.
 * @return array List of permitted values.
 */
function SettingOptions($name)
{
    global $settings;

    $f = $settings['fppDir'] . '/www/settings.json';
    if (!is_readable($f)) {
        return array();
    }
    $j = json_decode(file_get_contents($f), true);
    if (!isset($j['settings'][$name]['options']) || !is_array($j['settings'][$name]['options'])) {
        return array();
    }
    return array_values(array_map('strval', $j['settings'][$name]['options']));
}
