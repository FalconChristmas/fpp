<?


/////////////////////////////////////////////////////////////////////////////
// GET /api/cape
function GetCapeInfo() {
    global $settings;
    if (IsSet($settings['cape-info'])) {
        return json($settings['cape-info']);
    }
    
    halt(404, "No Cape!");
}

?>
