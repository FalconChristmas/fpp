<?

/////////////////////////////////////////////////////////////////////////////
// GET /api/backups/list
function GetAvailableBackups() {
    $backupDir = '/home/fpp/media/backups/';
    $found = 0;
    $excludeList = Array();
    $dirs = Array();

    foreach(scandir('/home/fpp/media') as $fileName) {
        if (($fileName != '.') &&
            ($fileName != '..')) {
            array_push($excludeList, $fileName);
        }
    }

    foreach (scandir($backupDir) as $fileName) {
        if (($fileName != '.') &&
            ($fileName != '..') &&
            (!in_array($fileName, $excludeList)) &&
            (is_dir($backupDir . '/' . $fileName))) {
            $found = 1;
            array_push($dirs, $fileName);

            foreach (scandir($backupDir . '/' . $fileName) as $subfileName) {
                if (($subfileName != '.') &&
                    ($subfileName != '..') &&
                    (!in_array($subfileName, $excludeList))) {
                    array_push($dirs, $fileName . '/' . $subfileName);
                }
            }
        }
    }

    return json($dirs);
}

?>
