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
 * @route GET /api/geoip
 * @response 200 ipapi.co's JSON response, passed through unmodified
 * ```json
 * {"ip": "1.2.3.4", "city": "Adelaide", "region": "South Australia", "timezone": "Australia/Adelaide", "latitude": -34.9, "longitude": 138.6}
 * ```
 * @response 502 Lookup failed
 * ```json
 * {"error": "GeoIP lookup failed"}
 * ```
 */
function GetGeoIP()
{
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

    if ($data === false || $data === null || $data === '') {
        http_response_code(502);
        return json(['error' => 'GeoIP lookup failed']);
    }

    header('Content-Type: application/json');
    echo $data;
}
