<?

require_once '../commandsocket.php';

function testMode_Get()
{
	return json(json_decode(SendCommand("GetTestMode")));
}

function testMode_Set()
{
	global $args;
    $json = strval(file_get_contents('php://input'));
    $input = json_decode($json, true);

	SendCommand(sprintf("SetTestMode,%s", $json));
    return json(json_decode(array("status" => "OK")));
}


?>