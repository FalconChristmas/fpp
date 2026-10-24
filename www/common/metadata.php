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
            // The same probe tells the Music tab this file's sample rate and
            // bit rate; keep them so it does not probe the file a second time.
            $audioInfo = AudioInfoFromProbe($resp);
            if (AudioInfoIsCacheable($resp, $audioInfo)) {
                media_audio_info_cache($mediaName, $audioInfo, $media_filesize);
            }
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
 * The sample rates FPP's audio system is using.
 *
 *   engine      default.clock.rate: the rate the audio graph really runs at.
 *   configured  the "# configured rate:" tag in 95-fpp-alsa-sink.conf: the rate
 *               the AudioFormat setting asked for.  The two differ when a card
 *               cannot clock as low as the request and refines it upward (a
 *               PCM5102A cape runs a 44.1 kHz request at 88.2 kHz).  With no
 *               tag the configured rate is the engine rate.
 *
 * 95-fpp-alsa-sink.conf is generated from the audio setting and overrides the
 * base default in 90-fpp.conf, so it is read first.
 *
 * @return array ['engine' => int|null, 'configured' => int|null, 'source' => string];
 *               nulls where there is no PipeWire configuration (macOS, Docker)
 */
function GetAudioRates()
{
    $confd = '/etc/pipewire/pipewire.conf.d';
    $sink = @file_get_contents($confd . '/95-fpp-alsa-sink.conf');
    $configured = null;
    if ($sink !== false && preg_match('/^#\s*configured rate:\s*(\d+)/m', $sink, $m)) {
        $configured = (int) $m[1];
    }
    $sources = array('95-fpp-alsa-sink.conf' => $sink, '90-fpp.conf' => @file_get_contents($confd . '/90-fpp.conf'));
    foreach ($sources as $file => $text) {
        if ($text !== false && preg_match('/default\.clock\.rate\s*=\s*(\d+)/', $text, $m)) {
            $engine = (int) $m[1];
            return array('engine' => $engine, 'configured' => ($configured !== null ? $configured : $engine), 'source' => $file);
        }
    }
    return array('engine' => null, 'configured' => null, 'source' => '');
}

/**
 * How a file's sample rate sits against the audio system's rates.
 *
 *   'ok'        at the engine rate: nothing to do
 *   'hardware'  at the configured rate, with the hardware running a whole
 *               multiple of it: the card upsamples, so there is no CPU cost
 *   'stuck'     at the configured rate, but the hardware does not run a whole
 *               multiple of it, so it is resampled anyway and converting the
 *               file to the configured rate would change nothing
 *   'resample'  any other rate: resampled on the CPU while it plays; convert it
 *               to the configured rate
 *   ''          not known (no sample rate, or no audio configuration)
 *
 * Same grouping as the Media Files report on the Troubleshooting page.
 *
 * @param int $rate the file's sample rate in Hz
 * @param array $rates the result of GetAudioRates()
 * @return string
 */
function AudioRateStatus($rate, $rates)
{
    $rate = (int) $rate;
    if ($rate <= 0 || empty($rates['engine'])) {
        return '';
    }
    $engine = (int) $rates['engine'];
    $configured = !empty($rates['configured']) ? (int) $rates['configured'] : $engine;
    if ($rate === $engine) {
        return 'ok';
    }
    if ($rate === $configured) {
        return ($engine > $configured && $engine % $configured === 0) ? 'hardware' : 'stuck';
    }
    return 'resample';
}

/**
 * Pull the facts the Music tab shows out of an ffprobe structure: the first
 * audio stream's sample rate, channels and codec, and its bit rate (falling back
 * to the container's for formats that do not carry one per stream, such as VBR
 * MP3).  A file with no audio stream comes back with a sample rate of 0.
 *
 * @param array $resp the structure from GetMetaDataFromFFProbe()
 * @return array ['sampleRate' => int, 'bitRate' => int, 'channels' => int, 'codec' => string]
 */
function AudioInfoFromProbe($resp)
{
    $streams = (isset($resp['streams']) && is_array($resp['streams'])) ? $resp['streams'] : array();
    foreach ($streams as $s) {
        if (isset($s['codec_type']) && $s['codec_type'] === 'audio') {
            $bitRate = isset($s['bit_rate']) ? (int) $s['bit_rate'] : 0;
            if ($bitRate <= 0 && isset($resp['format']['bit_rate'])) {
                $bitRate = (int) $resp['format']['bit_rate'];
            }
            return array(
                'sampleRate' => isset($s['sample_rate']) ? (int) $s['sample_rate'] : 0,
                'bitRate' => $bitRate,
                'channels' => isset($s['channels']) ? (int) $s['channels'] : 0,
                'codec' => isset($s['codec_name']) ? (string) $s['codec_name'] : '',
            );
        }
    }
    return array('sampleRate' => 0, 'bitRate' => 0, 'channels' => 0, 'codec' => '');
}

/**
 * Whether a probe's audio info is worth caching.  It is when the probe found a
 * sample rate, or when ffprobe itself ran and listed the file's streams without
 * an audio one (a video, a picture).  It is not when the probe failed -- ffprobe
 * missing, timed out, or only GStreamer's discoverer answered, which never
 * reports a sample rate -- because a cached "no audio" would stand for 30 days.
 * The file is simply probed again on the next listing.
 *
 * @param array $resp the structure from GetMetaDataFromFFProbe()
 * @param array $info the result of AudioInfoFromProbe($resp)
 * @return bool
 */
function AudioInfoIsCacheable($resp, $info)
{
    if ($info['sampleRate'] > 0) {
        return true;
    }
    // format_name is ffprobe's; the GStreamer fallback does not set it.
    return isset($resp['format']['format_name']) && isset($resp['streams']) && is_array($resp['streams']);
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
