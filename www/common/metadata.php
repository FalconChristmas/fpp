<?

/**
 * Returns duration info about the specified media file
 *
 * @param string $mediaName
 * @param bool $returnArray
 * @return array|string
 * @throws getid3_exception
 */
function getMediaDurationInfo($mediaName = "", $returnArray = false)
{
	//Get Info
	global $args;
	global $settings;

	$returnStr = array();
	$total_duration = 0;

	if (empty($mediaName)) {
		$mediaName = $args['media'];
	}

	check($mediaName, "mediaName", __FUNCTION__);

	if (file_exists($settings['musicDirectory'] . "/" . $mediaName)) {
		//Music Directory

		//Check the cache for the media name first
		$media_filesize = filesize($settings['musicDirectory'] . "/" . $mediaName);
		$cache_duration = media_duration_cache($mediaName, null, $media_filesize);
		//cache duration will be null if not in cache, then retrieve it
		if ($cache_duration == NULL) {
			//Include our getid3 library for media
            if (file_exists('./lib/getid3/getid3.php'))
    			require_once('./lib/getid3/getid3.php');
            else
    			require_once('../lib/getid3/getid3.php');

			//Instantiate getID3 object
			$getID3 = new getID3;

			$ThisFileInfo = $getID3->analyze($settings['musicDirectory'] . "/" . $mediaName);
			//cache it
			media_duration_cache($mediaName, $ThisFileInfo['playtime_seconds'], $media_filesize);
		} else {
			$ThisFileInfo['playtime_seconds'] = $cache_duration;
		}

		$total_duration = $ThisFileInfo['playtime_seconds'] + $total_duration;
	} else if (file_exists($settings['videoDirectory'] . "/" . $mediaName)) {
		//Check video directory

		//Check the cache for the media name first
		$media_filesize = filesize($settings['videoDirectory'] . "/" . $mediaName);
		$cache_duration = media_duration_cache($mediaName, null, $media_filesize);
		//cache duration will be null if not in cache, then retrieve it
		if ($cache_duration == NULL) {
			//Include our getid3 library for media
			if (file_exists('./lib/getid3/getid3.php'))
				require_once('./lib/getid3/getid3.php');
			else
				require_once('../lib/getid3/getid3.php');

			//Instantiate getID3 object
			$getID3 = new getID3;

			$ThisFileInfo = $getID3->analyze($settings['videoDirectory'] . "/" . $mediaName);
			//cache it
			media_duration_cache($mediaName, $ThisFileInfo['playtime_seconds'], $media_filesize);
		} else {
			$ThisFileInfo['playtime_seconds'] = $cache_duration;
		}

		$total_duration = $ThisFileInfo['playtime_seconds'] + $total_duration;
	} else if (file_exists($settings['sequenceDirectory'] . "/" . $mediaName)) {
		//Check Sequence directory
		$sequence_info = get_sequence_file_info($mediaName);
//		$media_filesize =$sequence_info['seqFileSize'];

		if (array_key_exists('seqDuration', $sequence_info)) {
			$total_duration = $sequence_info['seqDuration'];

		} else {
			$total_duration = 0;
		}

		//This doesn't take very long so no need to cache
//		media_duration_cache($mediaName, $total_duration, $media_filesize);
	} else {
		error_log("getMediaDurationInfo:: Could not find media file - " . $mediaName);
	}

	if ($total_duration !== 0) {
		//If return array is false then return the human readable duration, else raw seconds
		if ($returnArray == false) {
			$returnStr[$mediaName]['duration'] = human_playtime($total_duration);
		} else {
			$returnStr[$mediaName]['duration'] = $total_duration;
		}
	}

	if ($returnArray == true) {
		return $returnStr;
	} else {
		returnJSON($returnStr);
	}
}

/**
 * Returns sequence header info
 *
 * @return array
 */
function GetSequenceInfo()
{
	global $args;
	global $settings;
	$return_arr = array();

	$sequence = $args['seq'];
	//if the file extension is missing, add it on
	if (strpos($sequence, '.fseq') === FALSE) {
		$sequence = $sequence . ".fseq";
	}

	if (file_exists($settings['sequenceDirectory'] . '/' . $sequence)) {
		$return_arr = get_sequence_file_info($sequence);
	} else {
		$return_arr[$sequence]['error'] = "GetSequenceInfo:: Unable find sequence :: " . $sequence;
		error_log("GetSequenceInfo:: Unable find sequence :: " . $sequence);
	}

	returnJSON($return_arr);
}

function GetMetaDataFromFFprobe($filename)
{
    global $settings;

    if (file_exists($settings['musicDirectory'] . "/$filename")) {
        $filename = $settings['musicDirectory'] . "/$filename";
    } else if (file_exists($settings['videoDirectory'] . "/$filename")) {
        $filename = $settings['videoDirectory'] . "/$filename";
    } else {
        $result = Array();
        $result[$filename] = Array();
        return $result;
    }

    exec('ffprobe -hide_banner -loglevel fatal -show_error -show_format -show_streams -show_programs -show_chapters -show_private_data -print_format json ' . $filename, $output);
    $jsonStr = join(' ', $output);

    return json_decode($jsonStr, true);
}

?>
