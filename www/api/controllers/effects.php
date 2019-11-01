<?

function effects_list()
{
    global $effectDirectory;
    
    $effects = array();
    if ($d = opendir($effectDirectory)) {
        while (($file = readdir($d)) !== false) {
            if (preg_match('/\.eseq$/', $file)) {
                $file = preg_replace('/\.eseq$/', '', $file);
                array_push($effects, $file);
            }
        }
        closedir($d);
    }
    return json($effects);
}

function effects_list_ALL()
{
    global $effectDirectory;
    global $sequenceDirectory;
    
    $effects = array();
    if ($d = opendir($effectDirectory)) {
        while (($file = readdir($d)) !== false) {
            if (preg_match('/\.eseq$/', $file)) {
                $file = preg_replace('/\.eseq$/', '', $file);
                array_push($effects, $file);
            }
        }
        closedir($d);
    }
    if ($d = opendir($sequenceDirectory)) {
        while (($file = readdir($d)) !== false) {
            if (preg_match('/\.fseq$/', $file)) {
                $file = preg_replace('/\.fseq$/', '', $file);
                array_push($effects, $file);
            }
        }
        closedir($d);
    }
    return json($effects);
}
?>
