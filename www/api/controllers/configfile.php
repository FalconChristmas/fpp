<?

/////////////////////////////////////////////////////////////////////////////
function DownloadConfigFile()
{
	global $settings;

	$fileName = params('FileName');
	$fileName = $settings['configDirectory'] . '/' . $fileName;

	render_file($fileName);
}

/////////////////////////////////////////////////////////////////////////////
function UploadConfigFile()
{
	global $settings;

	// Content-Type: text/html is unprocessed by limonade
	$contents = $GLOBALS['HTTP_RAW_POST_DATA'];
	$fileName = params('FileName');

	$fileName = $settings['configDirectory'] . '/' . $fileName;

	$f = fopen($fileName, "w");
	if ($f)
	{
		fwrite($f, $contents);
		fclose($f);

		$result['Status'] = 'OK';
		$result['Message'] = '';
	}
	else
	{
		$result['Status'] = 'Error';
		$result['Message'] = 'Unable to open file for writing';
	}

	return json($result);
}

/////////////////////////////////////////////////////////////////////////////
?>
