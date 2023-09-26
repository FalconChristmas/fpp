<?
require_once '../commandsocket.php';

/////////////////////////////////////////////////////////////////////////////
// GET /oled
function GetOLEDStatus()
{
    global $settings;

    return "OLED Status";
}

/////////////////////////////////////////////////////////////////////////////
// GET /oled/action/options
function GetOLEDMenuActions()
{
    global $settings;
    $menu_options = Array();
    $menu_options = ["Up","Down","Back","Right","Enter","Test","Mode","Test Multisync"];

    return json($menu_options);
}

/////////////////////////////////////////////////////////////////////////////
// GET /oled/action/up
function SetOLEDActionUp()
{
    global $settings;
    SendCommand(sprintf("SetOLEDCommand,Up"));
    return "OLED Up";
}

/////////////////////////////////////////////////////////////////////////////
// GET /oled/action/down

function SetOLEDActionDown()
{
    global $settings;
    SendCommand(sprintf("SetOLEDCommand,Down"));
    return "OLED Down";
}

/////////////////////////////////////////////////////////////////////////////
// GET /oled/action/back

function SetOLEDActionBack()
{
    global $settings;
    SendCommand(sprintf("SetOLEDCommand,Back"));
    return "OLED Back";
}

/////////////////////////////////////////////////////////////////////////////
// GET /oled/action/right

function SetOLEDActionRight()
{
    global $settings;

    SendCommand(sprintf("SetOLEDCommand,Right"));

    return "OLED Right";
}

/////////////////////////////////////////////////////////////////////////////
// GET /oled/action/enter

function SetOLEDActionEnter()
{
    global $settings;

	SendCommand(sprintf("SetOLEDCommand,Enter"));
    //need to put SetOLEDCommand structure into command.cpp
    //return json(json_decode(array("status" => "OLED Enter Received")));

    return "OLED Enter";
}

/////////////////////////////////////////////////////////////////////////////
// GET /oled/action/test

function SetOLEDActionTest()
{
    global $settings;

    return "OLED Test";
}

/////////////////////////////////////////////////////////////////////////////
// GET /oled/action/mode

function SetOLEDActionMode()
{
    global $settings;
    SendCommand(sprintf("SetOLEDCommand,Mode"));
    return "OLED Mode";
}

/////////////////////////////////////////////////////////////////////////////
// GET /oled/action/testmultisync

function SetOLEDActionTestMultiSync()
{
    global $settings;
    SendCommand(sprintf("SetOLEDCommand,TestMultisync"));
    return "OLED Test Multisync";
}


?>