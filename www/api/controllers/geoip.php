<?
require_once(__DIR__ . "/../../config.php");

/**
 * GeoIP lookup
 *
 * Server-side proxy for ipapi.co's IP geolocation lookup, used by the
 * Timezone/GeoLocation "Lookup"/"Detect" buttons on settings.php. ipapi.co
 * does not send Access-Control-Allow-Origin, so the browser can't call it
 * directly from FPP's UI (blocked by the Same Origin Policy) - PHP isn't
 * subject to that, so we fetch it here and hand back the same JSON.
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
 * @queryParam country string Two-letter code substituted for the one the lookup
 *                            would report, so the mapping can be exercised for a
 *                            region you are not in.  Diagnostic only -- it changes
 *                            what is suggested, never what is stored, and the
 *                            response is marked with `fppCountryOverride`.
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
    // Diagnostic override.  The country normally comes from the caller's public
    // IP, which makes the interesting cases -- an EU box, an EEA box, somewhere
    // with no matching locale -- untestable from wherever you happen to be
    // sitting, and unreachable through the browser's location sensor because
    // this lookup never consults it.  ?country=XX substitutes the country and
    // nothing else, so the real mapping runs over the real data files and the
    // real fields fill in.  It changes only what is SUGGESTED; nothing is saved
    // here, and the caller was always free to pick these values by hand.
    $override = isset($_GET['country']) ? strtoupper($_GET['country']) : '';
    if ($override !== '' && !preg_match('/^[A-Z]{2}$/', $override)) {
        http_response_code(400);
        return json(['error' => 'country must be a two-letter code']);
    }

    $data = false;
    if (function_exists('curl_init')) {
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

    $j['fpp'] = GeoIPSuggestedSettings($j);

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
