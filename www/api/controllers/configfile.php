<?

/////////////////////////////////////////////////////////////////////////////
function GetFilesInDir($dir, $subdir = '')
{
	$result = Array();

	if ($subdir != '')
		$subdir .= '/';

	foreach( scandir($dir . '/' . $subdir) as $file)
	{
		if($file != '.' && $file != '..')
		{
			if (is_dir($dir . '/' . $subdir . $file))
				$result = array_merge($result, GetFilesInDir($dir, $subdir . $file));
			else
				array_push($result, $subdir . $file);
		}
	}

	return $result;
}

function GetConfigFileList($dir = '')
{
	global $settings;

	$origDir = $dir;
	if ($dir == '')
		$dir = $settings['configDirectory'];
	else
		$dir = $settings['configDirectory'] . '/' . $dir;

	$result = Array();

	$files = GetFilesInDir($dir);

	$result['Path'] = $origDir;
	$result['ConfigFiles'] = $files;

	return json($result);
}



/////////////////////////////////////////////////////////////////////////////
function DownloadConfigFile()
{
	global $settings;

	$fileName = params(0);

	if (is_dir($settings['configDirectory'] . '/' . $fileName))
		return GetConfigFileList($fileName);

	$fileName = $settings['configDirectory'] . '/' . $fileName;

	render_file($fileName);
}

/////////////////////////////////////////////////////////////////////////////
function UploadConfigFile()
{
	global $settings;
	$result = Array();

	// Content-Type: text/html is unprocessed by limonade
	$contents = $GLOBALS['HTTP_RAW_POST_DATA'];
	$fileName = params(0);

	if (preg_match('/\//', $fileName))
	{
		// FileName contains a subdir, so create if needed
		$subDir = $settings['configDirectory'] . '/' . $fileName;
		$subDir = preg_replace('/\/[^\/]*$/', '', $subDir);

		mkdir($subDir, 0755, true);
	}

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
function DeleteConfigFile()
{
	global $settings;
	$result = Array();

	$fileName = params(0);

	$fullFile = $settings['configDirectory'] . '/' . $fileName;

	if (is_file($fullFile))
	{
		unlink($fullFile);
		if (is_file($fullFile))
		{
			$result['Status'] = 'Error';
			$result['Message'] = 'Unable to delete ' . $fileName;
		}
		else
		{
			$result['Status'] = 'OK';
			$result['Message'] = '';
		}
	}
	else
	{
		$result['Status'] = 'Error';
		$result['Message'] = $fileName . ' is not a regular file';
	}

	return json($result);
}

/////////////////////////////////////////////////////////////////////////////
?>
