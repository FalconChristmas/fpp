<?
require_once 'lib/limonade.php';

$skipJSsettings = 1;
require_once '../config.php';
require_once '../common.php';

dispatch_get('/', 'ServeApiDocs');
dispatch_get('/api.html', 'ServeApiHtml');
dispatch_get('/openapi.yaml', 'ServeOpenApiSpec');
dispatch_get('/openapi.json', 'ServeOpenApiSpec');

dispatch_get('/backups/list', 'GetAvailableBackups');
dispatch_get('/backups/list/:DeviceName', 'GetAvailableBackupsOnDevice');
dispatch_get('/backups/devices', 'RetrieveAvailableBackupsDevices');
dispatch_post('/backups/devices/mount/:DeviceName/:MountLocation', 'MountDevice');
dispatch_post('/backups/devices/unmount/:DeviceName/:MountLocation', 'UnmountDevice');
dispatch_post('/backups/configuration', 'MakeJSONBackup');
dispatch_get('/backups/configuration/list', 'GetAvailableJSONBackups');
dispatch_get('/backups/configuration/list/:DeviceName', 'GetAvailableJSONBackupsOnDevice');
dispatch_post('/backups/configuration/restore/:Directory/:BackupFilename', 'RestoreJsonBackup');
dispatch_get('/backups/configuration/:Directory/:BackupFilename', 'DownloadJsonBackup');
dispatch_delete('/backups/configuration/:Directory/:BackupFilename', 'DeleteJsonBackup');

dispatch_get('/cape', 'GetCapeInfo');
dispatch_post('/cape/eeprom/voucher', 'RedeemVoucher');
dispatch_post('/cape/eeprom/sign/:key/:order', 'SignEEPROM');
dispatch_get('/cape/eeprom/signingData/:key/:order', 'GetSigningData');
dispatch_get('/cape/eeprom/signingFile/:key/:order', 'GetSigningFile');
dispatch_post('/cape/eeprom/signingData', 'PostSigningData');
dispatch_get('/cape/options', 'GetCapeOptions');
dispatch_get('/cape/strings', 'GetCapeStringOptions');
dispatch_get('/cape/panel', 'GetCapePanelOptions');
dispatch_get('/cape/strings/:key', 'GetCapeStringConfig');
dispatch_get('/cape/panel/:key', 'GetCapePanelConfig');

dispatch_get('/channel/input/stats', 'channel_input_get_stats');
dispatch_delete('/channel/input/stats', 'channel_input_delete_stats');
dispatch_get('/channel/output/processors', 'channel_get_output_processors');
dispatch_post('/channel/output/processors', 'channel_save_output_processors');
dispatch_get('/channel/output/:file', 'channel_get_output');
dispatch_post('/channel/output/:file', 'channel_save_output');

dispatch_get('/configfile', 'GetConfigFileList');
dispatch_get('/configfile/**', 'DownloadConfigFile');
dispatch_post('/configfile/**', 'UploadConfigFile');
dispatch_delete('/configfile/**', 'DeleteConfigFile');

dispatch_post('/dir/:DirName/:SubDir', 'CreateDir');
dispatch_delete('/dir/:DirName/:SubDir', 'DeleteDir');

dispatch_get('/effects', 'effects_list');
dispatch_get('/effects/ALL', 'effects_list_ALL');

dispatch_post('/email/configure', 'ConfigureEmail');
dispatch_post('/email/test', 'SendTestEmail');

dispatch_get('/events', 'events_list');
dispatch_get('/events/:eventId', 'event_get');
dispatch_get('/events/:eventId/trigger', 'event_trigger');

dispatch_get('/files/Sequences/fps', 'GetSequenceFPS'); // keep above files/:DirName
dispatch_get('/files/:DirName', 'GetFiles');
dispatch_get('/file/info/:plugin/:ext/**', 'GetPluginFileInfo'); // keep above file/:DirName
dispatch_get('/file/onUpload/:ext/**', 'PluginFileOnUpload'); // keep above file/:DirName
dispatch_get('/file/move/:fileName', 'MoveFile'); // keep above file/:DirName
dispatch_get('/files/zip/:DirNames', 'GetZipDir');
dispatch_post('/file/:DirName/copy/:source/:dest', 'files_copy');
dispatch_post('/file/:DirName/rename/:source/:dest', 'files_rename');
dispatch_get('/file/:DirName/tailfollow/**', 'TailFollowFile');
dispatch_get('/file/:DirName/**', 'GetFile');
dispatch_delete('/file/:DirName/**', 'DeleteFile');
dispatch_post('/file/:DirName', 'PatchFile');
dispatch_patch('/file/:DirName', 'PatchFile');
dispatch_post('/file/:DirName/:Name', 'PostFile');

dispatch_get('/git/originLog', 'GetGitOriginLog');
dispatch_get('/git/releases/os/:All', 'GitOSReleases');
dispatch_get('/git/releases/notes/:tag', 'GitOSReleaseNotes');
dispatch_get('/git/releases/sizes', 'GitOSReleaseSizes');
dispatch_get('/git/reset', 'GitReset');
dispatch_get('/git/status', 'GitStatus');
dispatch_get('/git/branches', 'GitBranches');

dispatch_get('/media', 'GetMedia');
dispatch_get('/media/:MediaName/duration', 'GetMediaDuration');
dispatch_get('/media/:MediaName/meta', 'GetMediaMetaData');

dispatch_get('/network/dns', 'network_get_dns');
dispatch_post('/network/dns', 'network_save_dns');
dispatch_get('/network/gateway', 'network_get_gateway');
dispatch_post('/network/gateway', 'network_save_gateway');
dispatch_get('/network/interface', 'network_list_interfaces');
dispatch_get('/network/interface/:interface', 'network_get_interface');
dispatch_get('/network/interface/add/:interface', 'network_add_interface');
dispatch_post('/network/interface/:interface', 'network_set_interface');
dispatch_post('/network/interface/:interface/apply', 'network_apply_interface');

dispatch_delete('/network/presisentNames', 'network_persistentNames_delete');
dispatch_post('/network/presisentNames', 'network_persistentNames_create');
dispatch_delete('/network/persistentNames', 'network_persistentNames_delete');
dispatch_post('/network/persistentNames', 'network_persistentNames_create');
dispatch_get('/network/wifi/scan/:interface', 'network_wifi_scan');
dispatch_get('/network/wifi/status/:interface', 'network_wifi_status');
dispatch_get('/network/wifi/strength', 'network_wifi_strength');

dispatch_get('/options/:SettingName', 'GetOptions');

dispatch_get('/audio/cardaliases', 'GetAudioCardAliases');
dispatch_post('/audio/cardaliases', 'SaveAudioCardAliases');

dispatch_get('/pipewire/audio/groups', 'GetPipeWireAudioGroups');
dispatch_post('/pipewire/audio/groups', 'SavePipeWireAudioGroups');
dispatch_post('/pipewire/audio/groups/apply', 'ApplyPipeWireAudioGroups');
dispatch_get('/pipewire/audio/sinks', 'GetPipeWireSinks');
dispatch_get('/pipewire/audio/cards', 'GetPipeWireAudioCards');
dispatch_get('/pipewire/audio/usb-check', 'GetUsbAudioBandwidthCheck');
dispatch_get('/pipewire/audio/sources', 'GetPipeWireAudioSources');
dispatch_get('/pipewire/audio/plugin-sources', 'GetPipeWirePluginSources');
dispatch_get('/pipewire/audio/input-groups', 'GetPipeWireInputGroups');
dispatch_post('/pipewire/audio/input-groups', 'SavePipeWireInputGroups');
dispatch_post('/pipewire/audio/input-groups/apply', 'ApplyPipeWireInputGroups');
dispatch_post('/pipewire/audio/input-groups/volume', 'SetInputGroupMemberVolume');
dispatch_post('/pipewire/audio/input-groups/effects', 'SaveInputGroupEffects');
dispatch_post('/pipewire/audio/input-groups/eq/update', 'UpdateInputGroupEQRealtime');
dispatch_get('/pipewire/audio/routing', 'GetRoutingMatrix');
dispatch_post('/pipewire/audio/routing', 'SaveRoutingMatrix');
dispatch_post('/pipewire/audio/routing/volume', 'SetRoutingPathVolume');
dispatch_get('/pipewire/audio/routing/presets', 'GetRoutingPresets');
dispatch_get('/pipewire/audio/routing/presets/names', 'GetRoutingPresetNames');
dispatch_post('/pipewire/audio/routing/presets', 'SaveRoutingPreset');
dispatch_post('/pipewire/audio/routing/presets/load', 'LoadRoutingPreset');
dispatch_post('/pipewire/audio/routing/presets/live-apply', 'LiveApplyRoutingPreset');
dispatch_delete('/pipewire/audio/routing/presets/:name', 'DeleteRoutingPreset');
dispatch_post('/pipewire/audio/stream/volume', 'SetStreamSlotVolume');
dispatch_get('/pipewire/audio/stream/status', 'GetStreamSlotStatus');
dispatch_post('/pipewire/audio/group/volume', 'SetPipeWireGroupVolume');
dispatch_post('/pipewire/audio/eq/update', 'UpdatePipeWireEQRealtime');
dispatch_post('/pipewire/audio/delay/update', 'UpdatePipeWireDelayRealtime');
dispatch_post('/pipewire/audio/sync/start', 'StartSyncCalibration');
dispatch_post('/pipewire/audio/sync/stop', 'StopSyncCalibration');
dispatch_post('/pipewire/audio/services/restart', 'RestartPipeWireServices');
dispatch_get('/pipewire/video/groups', 'GetPipeWireVideoGroups');
dispatch_post('/pipewire/video/groups', 'SavePipeWireVideoGroups');
dispatch_post('/pipewire/video/groups/apply', 'ApplyPipeWireVideoGroups');
dispatch_post('/pipewire/simple/apply', 'ApplyPipeWireSimpleConfig');
dispatch_get('/pipewire/video/connectors', 'GetVideoOutputTargets');
dispatch_get('/pipewire/video/routing', 'GetVideoRoutingMatrix');
dispatch_post('/pipewire/video/routing', 'SaveVideoRoutingMatrix');
dispatch_get('/pipewire/video/input-sources', 'GetPipeWireVideoInputSources');
dispatch_post('/pipewire/video/input-sources', 'SavePipeWireVideoInputSources');
dispatch_post('/pipewire/video/input-sources/apply', 'ApplyPipeWireVideoInputSources');
dispatch_get('/pipewire/video/input-sources/v4l2-devices', 'GetV4L2Devices');

dispatch_get('/pipewire/aes67/instances', 'GetAES67Instances');
dispatch_post('/pipewire/aes67/instances', 'SaveAES67Instances');
dispatch_post('/pipewire/aes67/apply', 'ApplyAES67Instances');
dispatch_get('/pipewire/aes67/status', 'GetAES67Status');
dispatch_get('/pipewire/aes67/interfaces', 'GetAES67NetworkInterfaces');

dispatch_get('/pipewire/opusrtp/instances', 'GetOpusRTPInstances');
dispatch_post('/pipewire/opusrtp/instances', 'SaveOpusRTPInstances');
dispatch_post('/pipewire/opusrtp/apply', 'ApplyOpusRTPInstances');
dispatch_get('/pipewire/opusrtp/status', 'GetOpusRTPStatus');
dispatch_get('/pipewire/opusrtp/interfaces', 'GetOpusRTPNetworkInterfaces');
dispatch_get('/pipewire/graph', 'GetPipeWireGraph');

// PipeWire control facade — clean, ID-addressed, live-state 3rd-party API
dispatch_get('/pipewire/control/status', 'PWCtl_GetStatus');
dispatch_get('/pipewire/control/groups', 'PWCtl_GetGroups');
dispatch_get('/pipewire/control/groups/:id', 'PWCtl_GetGroup');
dispatch_post('/pipewire/control/groups/:id/volume', 'PWCtl_SetGroupVolume');
dispatch_post('/pipewire/control/groups/:id/mute', 'PWCtl_SetGroupMute');
dispatch_post('/pipewire/control/groups/:id/members/:cardId/volume', 'PWCtl_SetMemberVolume');
dispatch_post('/pipewire/control/groups/:id/members/:cardId/mute', 'PWCtl_SetMemberMute');
dispatch_get('/pipewire/control/input-groups', 'PWCtl_GetInputGroups');
dispatch_get('/pipewire/control/input-groups/:id', 'PWCtl_GetInputGroup');
dispatch_post('/pipewire/control/input-groups/:id/members/:memberIndex/volume', 'PWCtl_SetInputMemberVolume');
dispatch_post('/pipewire/control/input-groups/:id/members/:memberIndex/mute', 'PWCtl_SetInputMemberMute');
dispatch_get('/pipewire/control/streams', 'PWCtl_GetStreams');
dispatch_post('/pipewire/control/streams/:slot/volume', 'PWCtl_SetStreamVolume');
dispatch_get('/pipewire/control/routing', 'PWCtl_GetRouting');
dispatch_post('/pipewire/control/routing/:inputGroupId/:outputGroupId/volume', 'PWCtl_SetRoutingVolume');
dispatch_post('/pipewire/control/routing/:inputGroupId/:outputGroupId/mute', 'PWCtl_SetRoutingMute');

dispatch_get('/playlists', 'playlist_list');
dispatch_post('/playlists', 'playlist_insert');
dispatch_get('/playlists/playable', 'playlist_playable');
dispatch_get('/playlists/validate', 'playlist_list_validate');
dispatch_get('/playlists/stop', 'playlist_stop');
dispatch_get('/playlists/pause', 'playlist_pause');
dispatch_get('/playlists/resume', 'playlist_resume');
dispatch_get('/playlists/stopgracefully', 'playlist_stopgracefully');
dispatch_get('/playlists/stopgracefullyafterloop', 'playlist_stopgracefullyafterloop');
dispatch_get('/playlist/:PlaylistName', 'playlist_get');
dispatch_get('/playlist/:PlaylistName/start', 'playlist_start');
dispatch_get('/playlist/:PlaylistName/start/:Repeat', 'playlist_start_repeat');
dispatch_get('/playlist/:PlaylistName/start/:Repeat/:ScheduleProtected', 'playlist_start_repeat_protected');
dispatch_post('/playlist/:PlaylistName', 'playlist_update');
dispatch_delete('/playlist/:PlaylistName', 'playlist_delete');
dispatch_post('/playlist/:PlaylistName/:SectionName/item', 'PlaylistSectionInsertItem');

dispatch_get('/plugin/headerIndicators', 'GetPluginHeaderIndicators');
dispatch_get('/plugin', 'GetInstalledPlugins');
dispatch_post('/plugin', 'InstallPlugin');
dispatch_post('/plugin/fetchInfo', 'FetchPluginInfoProxy');
dispatch_get('/plugin/popularity', 'GetPluginPopularity'); // keep above /plugin/:RepoName
dispatch_get('/plugin/githubStats', 'GetPluginGitHubStats'); // keep above /plugin/:RepoName
dispatch_get('/plugin/source', 'GetPluginSource'); // keep above /plugin/:RepoName
dispatch_get('/plugin/:RepoName', 'GetPluginInfo');
dispatch_get('/plugin/:RepoName/icon', 'PluginServeIcon');
dispatch_get('/plugin/:RepoName/page', 'GetPluginPageUrl');
dispatch_delete('/plugin/:RepoName', 'UninstallPlugin');
dispatch_get('/plugin/:RepoName/settings/:SettingName', 'PluginGetSetting');
dispatch_put('/plugin/:RepoName/settings/:SettingName', 'PluginSetSetting');
dispatch_post('/plugin/:RepoName/settings/:SettingName', 'PluginSetSetting');
dispatch_post('/plugin/:RepoName/updates', 'CheckForPluginUpdates');
dispatch_get('/plugin/:RepoName/upgrade', 'UpgradePlugin');
dispatch_post('/plugin/:RepoName/upgrade', 'UpgradePlugin');
// NOTE: Plugins may also implement their own /plugin/:RepoName/* endpoints
// which are added after the above endpoints via addPluginEndpoints() below.

dispatch_get('/proxies', 'GetProxies');
dispatch_post('/proxies', 'PostProxies');
dispatch_delete('/proxies', 'DeleteAllProxies');
dispatch_post('/proxies/:ProxyIp', 'AddProxy');
dispatch_delete('/proxies/:ProxyIp', 'DeleteProxy');

dispatch_get(array('/proxy/*/**', array("Ip", "urlPart")), 'GetProxiedURL');

dispatch_get('/remotes', 'GetRemotes');
dispatch_get('/remoteAction', 'remoteAction');

dispatch_get('/geoip', 'GetGeoIP');

dispatch_get('/sequence', 'GetSequences');
dispatch_get('/sequence/current/step', 'GetSequenceStep');
dispatch_get('/sequence/current/stop', 'GetSequenceStop');
dispatch_get('/sequence/current/togglePause', 'GetSequenceTogglePause');
dispatch_get('/sequence/:SequenceName', 'GetSequence');
dispatch_get('/sequence/:SequenceName/meta', 'GetSequenceMetaData');
dispatch_get('/sequence/:SequenceName/start/:startSecond', 'GetSequenceStart');
dispatch_post('/sequence/:SequenceName', 'PostSequence');
dispatch_delete('/sequence/:SequenceName', 'DeleteSequence');

dispatch_post('/schedule/reload', 'ReloadSchedule');
dispatch_get('/schedule', 'GetSchedule');
dispatch_post('/schedule', 'SaveSchedule');

dispatch_get('/settings', 'GetSettings');
dispatch_post('/settings/fanThermal/reset', 'ResetFanThermalTrips');
dispatch_get('/settings/:SettingName', 'GetSetting');
dispatch_get('/settings/:SettingName/options', 'GetOptions');
dispatch_put('/settings/:SettingName', 'PutSetting');
dispatch_put('/settings/:SettingName/jsonValueUpdate', 'UpdateJSONValueSetting');

dispatch_get('/scripts', 'scripts_list');
dispatch_get('/scripts/:scriptName', 'script_get');
dispatch_post('/scripts/:scriptName', 'script_save');
dispatch_get('/scripts/:scriptName/run', 'script_run');

dispatch_get('/statistics/usage', 'stats_get_last_file');
dispatch_post('/statistics/usage', 'stats_publish_stats_file');
dispatch_delete('/statistics/usage', 'stats_delete_last_file');

dispatch_get('/system/fppd/restart', 'RestartFPPD');
dispatch_get('/system/fppd/start', 'StartFPPD');
dispatch_get('/system/fppd/stop', 'StopFPPD');
dispatch_post('/system/fppd/skipBootDelay', 'SkipBootDelay');
dispatch_get('/system/reboot', 'RebootDevice');
dispatch_get('/system/releaseNotes/:version', 'ViewReleaseNotes');
dispatch_get('/system/shutdown', 'SystemShutdownOS');
dispatch_get('/system/status', 'SystemGetStatus');
dispatch_get('/system/updateStatus', 'GetUpdateStatus');
dispatch_get('/system/info', 'SystemGetInfo');
dispatch_get('/system/volume', 'SystemGetAudio');
dispatch_post('/system/volume', 'SystemSetAudio');
dispatch_post('/system/proxies', 'PostProxies');
dispatch_get('/system/proxies', 'GetProxies');
dispatch_get('/system/packages', 'GetOSpackages');
dispatch_get('/system/packages/info/:packageName', 'GetOSpackageInfo');

dispatch_get('/testmode', 'testMode_Get');
dispatch_post('/testmode', 'testMode_Set');

dispatch_get('/time', 'GetTime');

// Load FPP's own controllers BEFORE any plugin's api.php.
//
// limonade normally defers this to autoload_controller() inside run(), which
// would put plugin code first: collectPluginEndpoints() require_once's every
// installed plugin's api.php right here, at line 290, and run() only loads
// www/api/controllers/ afterwards. That ordering is what makes a plugin able to
// break or take over the API rather than merely fail on its own -- a plugin
// declaring a name a controller also declares wins the race, and the
// controller's later declaration is then the fatal one, killing every route
// served by that file and every file loaded after it.
//
// Loading them first costs nothing (autoload_controller loads the whole
// directory on any matched route anyway, and require_once makes the second call
// a no-op) and means FPP's own definitions always exist first, so
// PluginApiFunctionConflicts() below can see a conflict coming.
require_once_dir(__DIR__ . '/controllers');

addPluginEndpoints();

run();

///////////////////////////////////////////////////////////////////////////

/**
 * Function names a plugin's api.php must not declare, beyond anything FPP has
 * already declared (which is checked dynamically).
 *
 * These are limonade's request-handling hooks. It either calls them when a
 * global of that name happens to exist (call_if_exists: configure, initialize,
 * before, autorender, before_exit, before_sending_header) or defines its own
 * only when one does not (after, route_missing, autoload_controller, not_found,
 * server_error). Either way a plugin declaring one silently takes over part of
 * request handling for EVERY API call, not just its own endpoints --
 * autoload_controller() in particular decides whether FPP's controllers get
 * loaded at all. None of them are defined yet when plugin code is included, so
 * function_exists() cannot catch them; they have to be named.
 *
 * A function rather than a global: declarations in the bottom half of this file
 * are hoisted, but a top-level assignment would not run until after
 * addPluginEndpoints() had already been called near the top.
 */
function PluginApiReservedFunctions()
{
    return array(
        'configure', 'initialize', 'before', 'autorender', 'before_exit',
        'before_sending_header', 'after', 'route_missing', 'autoload_controller',
        'not_found', 'server_error',
    );
}

/**
 * Top-level function names declared by a PHP file.
 *
 * Uses the tokenizer rather than a regex so that the word "function" inside a
 * comment or a string can't produce a false hit. Methods are skipped (they live
 * in a class's namespace and cannot collide), as are functions inside a
 * namespace declaration, and closures have no name to collect.
 */
function PluginApiDeclaredFunctions($file)
{
    $src = @file_get_contents($file);
    if ($src === false || $src === '') {
        return array();
    }
    $tokens = @token_get_all($src);
    if (!is_array($tokens)) {
        return array();
    }

    $names = array();
    $depth = 0;          // current brace depth
    $classDepth = -1;    // brace depth the enclosing class body sits at, or -1
    $namespaced = false;
    $count = count($tokens);

    for ($i = 0; $i < $count; $i++) {
        $t = $tokens[$i];

        if (is_string($t)) {
            if ($t === '{') {
                $depth++;
            } else if ($t === '}') {
                $depth--;
                if ($classDepth >= 0 && $depth <= $classDepth) {
                    $classDepth = -1;
                }
            }
            continue;
        }

        if ($t[0] === T_NAMESPACE) {
            // Anything declared from here on is namespaced and cannot collide
            // with FPP's global functions.
            $namespaced = true;
            continue;
        }

        if ($t[0] === T_CLASS || $t[0] === T_INTERFACE || $t[0] === T_TRAIT) {
            // Skip the "::class" constant, which is not a class declaration.
            $prev = $i - 1;
            while ($prev >= 0 && is_array($tokens[$prev]) && $tokens[$prev][0] === T_WHITESPACE) {
                $prev--;
            }
            if ($prev >= 0 && is_array($tokens[$prev]) && $tokens[$prev][0] === T_DOUBLE_COLON) {
                continue;
            }
            $classDepth = $depth;
            continue;
        }

        if ($t[0] !== T_FUNCTION) {
            continue;
        }

        // The name, if any, is the next token that isn't whitespace, a comment,
        // or the "&" of a by-reference return. No name means a closure.
        //
        // The ampersand is a plain string token on older PHP but its own token
        // type from 8.1, so both forms have to be skipped or "function &foo()"
        // reads as anonymous and its name is missed.
        $skip = array(T_WHITESPACE, T_COMMENT, T_DOC_COMMENT);
        foreach (array('T_AMPERSAND_FOLLOWED_BY_VAR_OR_VARARG',
                       'T_AMPERSAND_NOT_FOLLOWED_BY_VAR_OR_VARARG') as $amp) {
            if (defined($amp)) {
                $skip[] = constant($amp);
            }
        }
        $j = $i + 1;
        while ($j < $count) {
            if (is_string($tokens[$j])) {
                if ($tokens[$j] !== '&') {
                    break;
                }
            } else if (!in_array($tokens[$j][0], $skip, true)) {
                break;
            }
            $j++;
        }
        if ($j < $count && is_array($tokens[$j]) && $tokens[$j][0] === T_STRING) {
            if ($classDepth < 0 && !$namespaced) {
                $names[] = $tokens[$j][1];
            }
        }
    }

    return $names;
}

/**
 * The names in a plugin's api.php that would collide with something FPP relies
 * on. Empty means the file is safe to include.
 */
function PluginApiFunctionConflicts($file)
{
    $conflicts = array();
    foreach (PluginApiDeclaredFunctions($file) as $name) {
        if (in_array(strtolower($name), PluginApiReservedFunctions(), true)) {
            $conflicts[] = $name . ' (reserved)';
        } else if (function_exists($name)) {
            $conflicts[] = $name;
        }
    }
    return $conflicts;
}

// Returns an array of all plugin endpoints: [['plugin'=>..., 'method'=>..., 'endpoint'=>..., 'callback'=>...], ...]
//
// Memoized: this is called once from addPluginEndpoints() at bootstrap and
// again from ServeOpenApiSpec() when /openapi.json is requested. Recomputing
// it the second time is wrong, not just wasteful -- PluginApiFunctionConflicts()
// treats function_exists() as a signal, and by the second call every plugin's
// own functions exist because the first call just require_once'd them. That
// reads as every plugin conflicting with itself, so a naive second pass
// silently drops all plugin paths from the served spec while the routes
// (registered by the first pass) keep working.
function collectPluginEndpoints()
{
    static $collected = null;
    if ($collected !== null) {
        return $collected;
    }

    global $pluginDirectory;
    $collected = array();
    $baseDir = $pluginDirectory . '/';
    if ($dir = opendir($baseDir)) {
        while (($file = readdir($dir)) !== false) {
            if (!in_array($file, array('.', '..')) && is_dir($baseDir . $file) && is_file($baseDir . $file . '/api.php')) {
                // Including a file that redeclares an existing function is a
                // fatal error, and it would happen here -- taking down every
                // route in the API, not just this plugin's. Check first and skip
                // the plugin instead. Losing one plugin's endpoints is a bad
                // outcome; losing the API is a much worse one.
                $conflicts = PluginApiFunctionConflicts($baseDir . $file . '/api.php');
                if (!empty($conflicts)) {
                    error_log("FPP: skipping plugin API for '$file' -- api.php declares "
                        . "function(s) FPP already provides: " . implode(', ', $conflicts));
                    continue;
                }

                $functionsBefore = get_defined_functions();
                $userFunctionsBefore = isset($functionsBefore['user']) ? $functionsBefore['user'] : array();

                // A syntax error in a plugin's api.php is a ParseError thrown at
                // require_once time -- catchable, unlike a redeclaration fatal
                // (PHP never turned "cannot redeclare" into a catchable Error, so
                // that one still has to be prevented before this point rather than
                // caught here; see PluginApiFunctionConflicts() above). Confirmed
                // live: an uncaught ParseError here previously took down every
                // route in the API, not just this plugin's, exactly like the
                // conflict case -- catch it and skip the plugin instead.
                try {
                    require_once $baseDir . $file . '/api.php';
                } catch (\Throwable $e) {
                    error_log("FPP: skipping plugin API for '$file' -- api.php failed to load: "
                        . get_class($e) . ': ' . $e->getMessage());
                    continue;
                }

                $functionsAfter = get_defined_functions();
                $userFunctionsAfter = isset($functionsAfter['user']) ? $functionsAfter['user'] : array();
                $newUserFunctions = array_diff($userFunctionsAfter, $userFunctionsBefore);

                $sfile = preg_replace('/-/', '', $file);
                $endpointFunction = "getEndpoints$sfile";

                if (!is_callable($endpointFunction)) {
                    foreach ($newUserFunctions as $funcName) {
                        if (stripos($funcName, 'getEndpoints') === 0) {
                            $endpointFunction = $funcName;
                            break;
                        }
                    }
                }

                if (!is_callable($endpointFunction)) {
                    error_log("Skipping plugin API endpoint registration for '$file': no callable getEndpoints* function found");
                    continue;
                }

                foreach (call_user_func($endpointFunction) as $ep) {
                    if (!isset($ep['callback'])) {
                        error_log("Skipping plugin endpoint for '$file': callback missing");
                        continue;
                    }
                    $collected[] = array(
                        'plugin'   => $file,
                        'method'   => $ep['method'],
                        'endpoint' => $ep['endpoint'],
                        'callback' => $ep['callback'],
                    );
                }
            }
        }
    }
    return $collected;
}

function addPluginEndpoints()
{
    foreach (collectPluginEndpoints() as $ep) {
        if (!is_callable($ep['callback'])) {
            error_log("Skipping plugin endpoint for '{$ep['plugin']}': callback not callable");
            continue;
        }
        $path = '/plugin/' . $ep['plugin'] . '/' . $ep['endpoint'];
        if ($ep['method'] == 'GET') {
            dispatch_get($path, $ep['callback']);
        } else if ($ep['method'] == 'POST') {
            dispatch_post($path, $ep['callback']);
        } else if ($ep['method'] == 'PUT') {
            dispatch_put($path, $ep['callback']);
        } else if ($ep['method'] == 'DELETE') {
            dispatch_delete($path, $ep['callback']);
        }
    }
}

function ServeApiDocs() {
    set_include_path(get_include_path() . PATH_SEPARATOR . dirname(__DIR__));
    extract($GLOBALS);
    include __DIR__ . '/api.php';
    exit;
}

function ServeApiHtml() {
    header('Content-Type: text/html; charset=utf-8');
    readfile(__DIR__ . '/api.html');
    exit;
}

function ServeOpenApiSpec() {
    $spec = json_decode(file_get_contents(__DIR__ . '/openapi.json'), true);

    foreach (collectPluginEndpoints() as $ep) {
        $method = strtolower($ep['method']);
        // Every other path in this spec is the full external path (@route
        // docblocks are written with /api/... baked in, and
        // MergeUndocumentedFppdRoutes() below does the same) -- match that,
        // rather than the bare limonade-relative path these are dispatched
        // at internally. servers.url is "/", not "/api", so leaving this one
        // bare made every generated request URL for a PHP plugin route
        // (Scalar's "Try it", copy-as-curl, etc.) 404.
        $path   = '/api/plugin/' . $ep['plugin'] . '/' . $ep['endpoint'];
        if (!isset($spec['paths'][$path])) {
            $spec['paths'][$path] = array();
        }
        if (!isset($spec['paths'][$path][$method])) {
            $spec['paths'][$path][$method] = array(
                'summary'  => $ep['plugin'] . ' - ' . $ep['endpoint'],
                'tags'     => array('Plugins', $ep['plugin']),
                'responses' => array('200' => array('description' => 'Success')),
            );
        }
    }

    MergeUndocumentedFppdRoutes($spec);

    header('Content-Type: application/json; charset=utf-8');
    echo json_encode($spec, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
    exit;
}

// fppd's /internal/pluginApiRoutes lists every route Drogon has registered,
// FPP's own included -- Drogon's registry doesn't record who registered a
// route, so that's everything it can tell us. For each one, try both
// candidate external paths (direct Apache proxy, and the generic
// plugin-apis passthrough that works for literally any fppd path); if
// either is already documented via an @route docblock (PHP or C++), skip
// it. Otherwise report it as undocumented at whichever candidate its
// namespace actually resolves through. In practice the survivors are
// mostly C++ plugin routes (e.g. fpp-brightness's /Brightness) -- but
// nothing here can prove a route is plugin-owned (confirmed live: several
// of FPP's own bare/catch-all routes have no individual docblock either),
// so this is tagged "Undocumented", not "Plugins". Regex artifacts
// (catch-all patterns like "/fppd/.*") are skipped -- not callable paths,
// just how FPP's own routing overlaps get expressed.
function MergeUndocumentedFppdRoutes(&$spec)
{
    // Local, not top-level define()/$GLOBALS -- run() is called earlier in
    // this file's top-to-bottom execution than this point, so a top-level
    // assignment here would never have run by the time a request actually
    // dispatches here: function declarations hoist, but top-level statements
    // don't, so PHP would never reach a define() placed after run(). Function
    // bodies execute lazily on call, so declaring these locally is safe.

    // Bare paths that are registered with Drogon but have no real content on
    // ANY method -- fppd itself 404s on them regardless (confirmed directly
    // against localhost:32322), they exist only because a wildcard sibling
    // needs a parent match. Nothing useful to document, so skip outright.
    $nonFunctionalBareRoutes = array('/fppd', '/command');

    // Bare path + method combos where the handler requires a sub-path
    // segment (a pin/name/id/etc, already documented at that sub-path) and
    // 404s or 400s on the bare form -- unlike the routes above, GET on these
    // same paths IS real (list/status), so this has to be per-method, not
    // per-path. Every one of these confirmed live against a running fppd;
    // don't add to this list, or remove from it, without checking behavior
    // directly rather than assuming from the handler code alone (this
    // session got bitten twice: PixelOverlayManager::render_PUT only
    // handles p1=="overlays", not "models", despite the shared handler
    // function making them look symmetric; Player::render_POST/PUT are
    // unconditionally dead code regardless of path).
    $nonFunctionalMethodPaths = array(
        'POST /gpio', 'POST /commands', 'POST /commandPresets',
        'POST /player', 'PUT /player',
        'PUT /models',
        'POST /variables', 'DELETE /variables',
    );

    // fppd registers this at /fppdws, an entirely separate websocket proxy
    // (see etc/apache2.site's ProxyPass /fppdws ws://...) -- not a REST path
    // under /api/ at all, so it can't be expressed as one here. And this
    // function's own data source lists itself along with everything else --
    // deliberately undocumented (see the comment on its registerHandler()
    // call in httpAPI.cpp), so exclude it explicitly rather than relying on
    // it happening to fall outside $directProxyNamespaces.
    $nonApiRoutes = array('/fppdws', '/internal/pluginApiRoutes');

    // Namespaces Apache proxies directly from /api/<name>... straight to
    // fppd (see the RewriteRule list in etc/apache2.site). Anything NOT in
    // this list is only reachable externally via the generic
    // /api/plugin-apis/<path> passthrough (RewriteRule
    // ^plugin-apis/(.*)$ ... -- proxies any fppd path verbatim, confirmed
    // against fpp-brightness's /Brightness and a real third-party plugin's
    // /FPPMon, neither of which resolve at bare /api/*).
    $directProxyNamespaces = array(
        'fppd', 'overlays', 'models', 'commands', 'command', 'commandPresets',
        'player', 'gpio', 'variables', 'aes67', 'opusrtp',
    );

    $ctx = stream_context_create(array('http' => array('timeout' => 1)));
    $body = @file_get_contents('http://localhost:32322/internal/pluginApiRoutes', false, $ctx);
    if ($body === false) {
        return;
    }
    $routes = json_decode($body, true);
    if (!is_array($routes)) {
        return;
    }

    foreach ($routes as $route) {
        if (!isset($route['path']) || !isset($route['method'])) {
            continue;
        }
        $rawPath = $route['path'];
        if (strpbrk($rawPath, '.*()|\\') !== false) {
            continue; // regex catch-all, not a real callable path
        }
        if (in_array($rawPath, $nonFunctionalBareRoutes, true)
            || in_array($rawPath, $nonApiRoutes, true)) {
            continue;
        }
        if (in_array($route['method'] . ' ' . $rawPath, $nonFunctionalMethodPaths, true)) {
            continue;
        }

        $namespace = ltrim(strtok($rawPath, '/'), '/');
        $directPath = '/api' . $rawPath;
        $pluginApiPath = '/api/plugin-apis' . $rawPath;
        $method = strtolower($route['method']);

        if (isset($spec['paths'][$directPath][$method])) {
            continue; // already documented, whether direct-proxied or PHP-mediated at the same path
        }
        if (isset($spec['paths'][$pluginApiPath][$method])) {
            continue;
        }
        // Drogon handlers answer HEAD as a side effect of handling GET
        // (req->isHead() checks throughout httpAPI.cpp); it's not a
        // separately meaningful operation, so don't report it as its own
        // undocumented route once GET is covered either way.
        if ($method === 'head' && (isset($spec['paths'][$directPath]['get']) || isset($spec['paths'][$pluginApiPath]['get']))) {
            continue;
        }

        $path = in_array($namespace, $directProxyNamespaces, true) ? $directPath : $pluginApiPath;
        if (!isset($spec['paths'][$path])) {
            $spec['paths'][$path] = array();
        }
        $spec['paths'][$path][$method] = array(
            'summary'   => $rawPath . ' (undocumented - see plugin documentation)',
            'tags'      => array('Undocumented'),
            'responses' => array('200' => array('description' => 'Success')),
        );
    }
}
