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
	global $args;
    $json = strval(file_get_contents('php://input'));
    $input = json_decode($json, true);

	SendCommand(sprintf("SetTestMode,%s", $json));
    return json(json_decode(array("status" => "OK")));
}

?>
