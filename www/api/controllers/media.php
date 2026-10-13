<?

// This path is relative to the api directory, not the controllers directory
require_once '../common/metadata.php';

/**
 * List all media files
 *
 * Returns a list of media files (includes both music and video files).
 *
 * @route GET /api/media
 * @response 200 List of media filenames
 * ```json
 * ["Frosty.mp4", "Jingle_Bells.mp3"]
 * ```
 */
function GetMedia()
{
    global $settings;
    $files = array();

    // Use scandir instead of glob for better performance
    $dirs = [$settings['musicDirectory'], $settings['videoDirectory']];
    foreach ($dirs as $dir) {
        if (is_dir($dir)) {
            foreach (scandir($dir) as $file) {
                if ($file !== '.' && $file !== '..' && is_file($dir . '/' . $file)) {
                    $files[] = $file;
                }
            }
        }
    }

    sort($files);

    return json($files);
}

/**
 * Get duration of media item
 *
 * Returns the duration of a media item.
 *
 * @route GET /api/media/{MediaName}/duration
 * @response 200 Media duration
 * ```json
 * {
 *   "1min_720p29_2014-10-01.mp4": {
 *     "duration": 60.010666666667
 *   }
 * }
 * ```
 * @response 404 Media file not found
 * ```text
 * Not found: {MediaName}
 * ```
 */
function GetMediaDuration()
{
    global $settings;
    $resp = array();

    $file = params('MediaName');

    $resp = getMediaDurationInfo($file, true);

    if ($resp[$file]['duration'] < 0) {
        halt(404, "Not found: " . $file);
    } else {
        return json($resp);
    }

}

/**
 * Get metadata for media item
 *
 * Returns metadata streams, codecs, profiles, type for a specific media file.
 *
 * @route GET /api/media/{MediaName}/meta
 * @response 200 Media file metadata
 * ```json
 * {
 *   "programs": [],
 *   "streams": [
 *     {
 *       "index": 0,
 *       "codec_name": "h264",
 *       "codec_long_name": "H.264 / AVC / MPEG-4 AVC / MPEG-4 part 10",
 *       "profile": "High",
 *       "codec_type": "video",
 *       "codec_time_base": "500/29971"
 *     }
 *   ]
 * }
 * ```
 */
function GetMediaMetaData()
{
    global $settings;
    $resp = array();

    $file = params('MediaName');

    $resp = GetMetaDataFromFFProbe($file);

    return json($resp);
}

/**
 * Get audio info for a music file
 *
 * Returns the sample rate, bit rate, channels and codec of a music file, and how
 * its sample rate sits against the audio system's engine and configured rates.
 * Probes the file once and caches the result, so the Music tab can fill its
 * Sample Rate and Bit Rate columns without waiting on the first listing.
 *
 * @route GET /api/media/{MediaName}/audio
 * @response 200 Audio info
 * ```json
 * {
 *   "sampleRate": 48000,
 *   "bitRate": 192000,
 *   "channels": 2,
 *   "codec": "mp3",
 *   "status": "resample",
 *   "engineRate": 44100,
 *   "configuredRate": 44100
 * }
 * ```
 * `status` is `ok`, `hardware`, `stuck`, `resample`, or empty when unknown.
 * @response 404 No such music file
 */
function GetMediaAudioInfo()
{
    global $settings;

    $file = params('MediaName');
    $path = $settings['musicDirectory'] . '/' . $file;
    if (strpos($file, '/') !== false || strpos($file, '..') !== false || !is_file($path)) {
        http_response_code(404);
        return json(array('error' => 'No such music file'));
    }

    $size = real_filesize($path);
    $info = media_audio_info_cache($file, null, $size);
    if ($info === null) {
        $resp = GetMetaDataFromFFProbe($file);
        $info = AudioInfoFromProbe($resp);
        media_audio_info_cache($file, $info, $size);
        // The probe already has the duration; keep it so the Duration column
        // does not probe the file again.
        if (isset($resp['format']['duration'])) {
            media_duration_cache($file, $resp['format']['duration'], $size);
        }
    }

    $rates = GetAudioRates();
    return json(array_merge($info, array(
        'status' => AudioRateStatus($info['sampleRate'], $rates),
        'engineRate' => $rates['engine'],
        'configuredRate' => $rates['configured'],
    )));
}
