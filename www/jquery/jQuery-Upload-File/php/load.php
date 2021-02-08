<?php
$dir="uploads";
$files = scandir($dir);

$ret= array();
foreach($files as $file)
{
	if($file == "." || $file == "..")
		continue;
	$filePath=$dir."/".$file;
	$details = array();
	$details['name']=$file;
	$details['path']=$filePath;
	$details['size']=filesize($filePath);
	$ret[] = $details;

}

echo json_encode($ret);
?>