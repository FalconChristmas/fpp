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
        $media_filesize = real_filesize($settings['musicDirectory'] . "/" . $mediaName);
        $cache_duration = media_duration_cache($mediaName, null, $media_filesize);
        //cache duration will be null if not in cache, then retrieve it
        if ($cache_duration == null) {
            $resp = GetMetaDataFromFFProbe($mediaName);
            //cache it
            media_duration_cache($mediaName, $resp['format']['duration'], $media_filesize);
            $total_duration = $resp['format']['duration'] + $total_duration;
        } else {
            $total_duration = $cache_duration + $total_duration;
        }
    } else if (file_exists($settings['videoDirectory'] . "/" . $mediaName)) {
        //Check video directory

        //Check the cache for the media name first
        $media_filesize = real_filesize($settings['videoDirectory'] . "/" . $mediaName);
        $cache_duration = media_duration_cache($mediaName, null, $media_filesize);
        //cache duration will be null if not in cache, then retrieve it
        if ($cache_duration == null) {
            $resp = GetMetaDataFromFFProbe($mediaName);

            //cache it
            media_duration_cache($mediaName, $resp['format']['duration'], $media_filesize);
            $total_duration = $resp['format']['duration'] + $total_duration;
        } else {
            $total_duration = $cache_duration + $total_duration;
        }

    } else if (file_exists($settings['sequenceDirectory'] . "/" . $mediaName)) {
        //Check Sequence directory
        $sequence_info = get_sequence_file_info($mediaName);
//        $media_filesize =$sequence_info['seqFileSize'];

        if (array_key_exists('seqDuration', $sequence_info)) {
            $total_duration = $sequence_info['seqDuration'];

        } else {
            $total_duration = 0;
        }

        //This doesn't take very long so no need to cache
        //        media_duration_cache($mediaName, $total_duration, $media_filesize);
    } else {
        error_log("getMediaDurationInfo:: Could not find media file - " . $mediaName);
        if ($returnArray == false) {
            $returnStr[$mediaName]['duration'] = "-00:01";
        } else {
            $returnStr[$mediaName]['duration'] = -1;
        }
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

function GetMetaDataFromFFProbe($filename)
{
    global $settings;

    if (file_exists($settings['musicDirectory'] . "/$filename")) {
        $filename = $settings['musicDirectory'] . "/$filename";
    } else if (file_exists($settings['videoDirectory'] . "/$filename")) {
        $filename = $settings['videoDirectory'] . "/$filename";
    } else {
        $result = array();
        $result[$filename] = array();
        return $result;
    }

    $cmd = 'ffprobe -hide_banner -loglevel fatal -show_error -show_format -show_streams -show_programs -show_chapters -show_private_data -print_format json ' . escapeshellarg($filename);
    exec($cmd, $output);
    $jsonStr = join(' ', $output);
    return json_decode($jsonStr, true);
}
