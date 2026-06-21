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

    $result = ProbeMediaWithFFProbe($filename);

    //Fall back to GStreamer's discoverer when ffprobe is unavailable or could
    //not determine the duration. This keeps duration/metadata working on
    //gstreamer-based installs that do not ship the ffmpeg/ffprobe CLI binary.
    if (!isset($result['format']['duration'])) {
        $gstResult = ProbeMediaWithGstDiscoverer($filename);
        if (isset($gstResult['format']['duration'])) {
            $result = $gstResult;
        }
    }

    return $result;
}

/**
 * Probe a media file with ffprobe and return the decoded JSON structure.
 *
 * @param string $filename absolute path to the media file
 * @return array ffprobe's format/streams structure, or empty array on failure
 */
function ProbeMediaWithFFProbe($filename)
{
    $binary = MediaProbeFindBinary('ffprobe');
    if ($binary === null) {
        return array();
    }

    $cmd = escapeshellarg($binary) . ' -hide_banner -loglevel fatal -show_error -show_format -show_streams -show_programs -show_chapters -show_private_data -print_format json ' . escapeshellarg($filename) . ' 2>/dev/null';
    $output = array();
    exec($cmd, $output);
    $decoded = json_decode(join(' ', $output), true);
    return is_array($decoded) ? $decoded : array();
}

/**
 * Probe a media file with GStreamer's gst-discoverer-1.0 tool and map the
 * output into an ffprobe-like structure (format.duration + streams[]) so that
 * existing callers and the "Video Info" modal keep working unchanged.
 *
 * @param string $filename absolute path to the media file
 * @return array format/streams structure, or empty array on failure
 */
function ProbeMediaWithGstDiscoverer($filename)
{
    $binary = MediaProbeFindBinary('gst-discoverer-1.0');
    if ($binary === null) {
        return array();
    }

    $cmd = escapeshellarg($binary) . ' -v ' . escapeshellarg($filename) . ' 2>/dev/null';
    $output = array();
    exec($cmd, $output);
    if (empty($output)) {
        return array();
    }

    $result = array('format' => array('filename' => $filename), 'streams' => array());

    foreach ($output as $line) {
        $trim = trim($line);

        // Duration: 0:03:46.010000000
        if (preg_match('/^Duration:\s*(\d+):(\d{2}):(\d{2})(?:\.(\d+))?/', $trim, $m)) {
            $seconds = ($m[1] * 3600) + ($m[2] * 60) + $m[3];
            if (isset($m[4]) && $m[4] !== '') {
                $seconds += floatval('0.' . $m[4]);
            }
            $result['format']['duration'] = (string) $seconds;
        }

        // Topology lines, e.g.:
        //   "container #0: video/quicktime, variant=(string)iso"
        //   "video #1: video/x-h264, stream-format=(string)avc, ..."
        //   "audio #2: audio/mpeg, mpegversion=(int)4, ..."
        // The caps string is comma-separated; the first segment is the media
        // type which makes a reasonable codec name for display.
        if (preg_match('/^(container|audio|video|subtitle|text)(?:\s*#\d+)?:\s*(.+)$/', $trim, $m)) {
            $type = $m[1];
            $caps = explode(',', trim($m[2]));
            $codec = trim($caps[0]);
            if ($type === 'container') {
                $result['format']['format_long_name'] = $codec;
            } else {
                $result['streams'][] = array(
                    'codec_type' => $type,
                    'codec_name' => $codec,
                );
            }
        }
    }

    return $result;
}

/**
 * Locate a media probe helper binary, checking PATH first and then a few
 * common install locations (covers cases where the web server's PATH is
 * minimal, e.g. apache/php-fpm).
 *
 * @param string $name binary name to locate
 * @return string|null absolute path to the binary, or null if not found
 */
function MediaProbeFindBinary($name)
{
    $path = trim((string) shell_exec('command -v ' . escapeshellarg($name) . ' 2>/dev/null'));
    if ($path !== '' && is_executable($path)) {
        return $path;
    }

    foreach (array('/usr/bin/', '/usr/local/bin/', '/opt/homebrew/bin/', '/bin/') as $dir) {
        if (is_executable($dir . $name)) {
            return $dir . $name;
        }
    }

    return null;
}
