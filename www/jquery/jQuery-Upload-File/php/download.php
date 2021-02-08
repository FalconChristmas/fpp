<?php
if(isset($_GET['filename']))
{
$fileName=$_GET['filename'];
$fileName=str_replace("..",".",$fileName); //required. if somebody is trying parent folder files
$file = "uploads/".$fileName;
$file = str_replace("..","",$file);
if (file_exists($file)) {
	$fileName =str_replace(" ","",$fileName);
    header('Content-Description: File Transfer');
    header('Content-Disposition: attachment; filename='.$fileName);
    header('Content-Transfer-Encoding: binary');
    header('Expires: 0');
    header('Cache-Control: must-revalidate');
    header('Pragma: public');
    header('Content-Length: ' . filesize($file));
    ob_clean();
    flush();
    readfile($file);
    exit;
}

}
?>
