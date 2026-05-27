<?

require_once '../commandsocket.php';

/**
 * Get Test Mode state
 *
 * Returns the current Test Mode configuration for this instance.
 *
 * @badge "FPP REQUIRED" critical
 * @route-v1 GET /testmode
 * @route-v2 GET /testmode
 * @response 200 Current Test Mode configuration
 * ```json
 * {
 *   "mode": "RGBChase",
 *   "subMode": "RGBChase-RGB",
 *   "cycleMS": 1000,
 *   "colorPattern": "FF000000FF000000FF",
 *   "enabled": 1,
 *   "channelSet": "1-520",
 *   "channelSetType": "channelRange"
 * }
 * ```
 */
function testMode_Get()
{
	return json(json_decode(SendCommand("GetTestMode")));
}

/**
 * Set Test Mode configuration
 *
 * Sets the current Test Mode configuration for this instance.
 *
 * @badge "FPP REQUIRED" critical
 * @route-v1 POST /testmode
 * @route-v2 POST /testmode
 * @body {"mode": "RGBChase", "subMode": "RGBChase-RGB", "cycleMS": 1000, "colorPattern": "FF000000FF000000FF", "enabled": 1, "channelSet": "1-520", "channelSetType": "channelRange"}
 * @response 200 Test mode updated successfully
 * ```json
 * { "status": "OK" }
 * ```
 */
function testMode_Set()
{
    $json = strval(file_get_contents('php://input'));

    // the body is forwarded to fppd as-is, so make sure it is at least a JSON
    // object before handing it over
    if (!is_array(json_decode($json, true))) {
        return json(array("status" => "ERROR: Invalid JSON body"));
    }

    SendCommand(sprintf("SetTestMode,%s", $json));
    return json(array("status" => "OK"));
}

?>
