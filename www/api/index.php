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
dispatch_post('/events/:eventId/trigger', 'event_trigger');

dispatch_get('/files/:DirName', 'GetFiles');
dispatch_get('/file/info/:plugin/:ext/**', 'GetPluginFileInfo'); // keep above file/:DirName
dispatch_post('/file/onUpload/:ext/**', 'PluginFileOnUpload'); // keep above file/:DirName
dispatch_post('/file/move/:fileName', 'MoveFile'); // keep above file/:DirName
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
dispatch_get('/git/releases/sizes', 'GitOSReleaseSizes');
dispatch_post('/git/reset', 'GitReset');
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
dispatch_post('/network/interface/add/:interface', 'network_add_interface');
dispatch_post('/network/interface/:interface', 'network_set_interface');
dispatch_post('/network/interface/:interface/apply', 'network_apply_interface');

dispatch_delete('/network/presisentNames', 'network_persistentNames_delete');
dispatch_post('/network/presisentNames', 'network_persistentNames_create');
dispatch_delete('/network/persistentNames', 'network_persistentNames_delete');
dispatch_post('/network/persistentNames', 'network_persistentNames_create');
dispatch_get('/network/wifi/scan/:interface', 'network_wifi_scan');
dispatch_get('/network/wifi/strength', 'network_wifi_strength');

dispatch_get('/options/:SettingName', 'GetOptions');

dispatch_get('/playlists', 'playlist_list');
dispatch_post('/playlists', 'playlist_insert');
dispatch_get('/playlists/playable', 'playlist_playable');
dispatch_get('/playlists/validate', 'playlist_list_validate');
dispatch_post('/playlists/stop', 'playlist_stop');
dispatch_post('/playlists/pause', 'playlist_pause');
dispatch_post('/playlists/resume', 'playlist_resume');
dispatch_post('/playlists/stopgracefully', 'playlist_stopgracefully');
dispatch_post('/playlists/stopgracefullyafterloop', 'playlist_stopgracefullyafterloop');
dispatch_get('/playlist/:PlaylistName', 'playlist_get');
dispatch_post('/playlist/:PlaylistName/start', 'playlist_start');
dispatch_post('/playlist/:PlaylistName/start/:Repeat', 'playlist_start_repeat');
dispatch_post('/playlist/:PlaylistName/start/:Repeat/:ScheduleProtected', 'playlist_start_repeat_protected');
dispatch_post('/playlist/:PlaylistName', 'playlist_update');
dispatch_delete('/playlist/:PlaylistName', 'playlist_delete');
dispatch_post('/playlist/:PlaylistName/:SectionName/item', 'PlaylistSectionInsertItem');

dispatch_get('/plugin/headerIndicators', 'GetPluginHeaderIndicators');
dispatch_get('/plugin', 'GetInstalledPlugins');
dispatch_post('/plugin', 'InstallPlugin');
dispatch_post('/plugin/fetchInfo', 'FetchPluginInfoProxy');
dispatch_get('/plugin/:RepoName', 'GetPluginInfo');
dispatch_delete('/plugin/:RepoName', 'UninstallPlugin');
dispatch_get('/plugin/:RepoName/settings/:SettingName', 'PluginGetSetting');
dispatch_put('/plugin/:RepoName/settings/:SettingName', 'PluginSetSetting');
dispatch_post('/plugin/:RepoName/settings/:SettingName', 'PluginSetSetting');
dispatch_post('/plugin/:RepoName/updates', 'CheckForPluginUpdates');
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
dispatch_post('/remoteAction', 'remoteAction');

dispatch_get('/sequence', 'GetSequences');
dispatch_post('/sequence/current/step', 'GetSequenceStep');
dispatch_post('/sequence/current/stop', 'GetSequenceStop');
dispatch_post('/sequence/current/togglePause', 'GetSequenceTogglePause');
dispatch_get('/sequence/:SequenceName', 'GetSequence');
dispatch_get('/sequence/:SequenceName/meta', 'GetSequenceMetaData');
dispatch_post('/sequence/:SequenceName/start/:startSecond', 'GetSequenceStart');
dispatch_post('/sequence/:SequenceName', 'PostSequence');
dispatch_delete('/sequence/:SequenceName', 'DeleteSequence');

dispatch_post('/schedule/reload', 'ReloadSchedule');
dispatch_get('/schedule', 'GetSchedule');
dispatch_post('/schedule', 'SaveSchedule');

dispatch_get('/settings', 'GetSettings');
dispatch_get('/settings/:SettingName', 'GetSetting');
dispatch_get('/settings/:SettingName/options', 'GetOptions');
dispatch_put('/settings/:SettingName', 'PutSetting');
dispatch_put('/settings/:SettingName/jsonValueUpdate', 'UpdateJSONValueSetting');

dispatch_get('/scripts', 'scripts_list');
dispatch_post('/scripts/installRemote/:category/:filename', 'scripts_install_remote');
dispatch_get('/scripts/viewRemote/:category/:filename', 'scripts_view_remote');
dispatch_get('/scripts/:scriptName', 'script_get');
dispatch_post('/scripts/:scriptName', 'script_save');
dispatch_post('/scripts/:scriptName/run', 'script_run');

dispatch_get('/statistics/usage', 'stats_get_last_file');
dispatch_post('/statistics/usage', 'stats_publish_stats_file');
dispatch_delete('/statistics/usage', 'stats_delete_last_file');

dispatch_post('/system/fppd/restart', 'RestartFPPD');
dispatch_post('/system/fppd/start', 'StartFPPD');
dispatch_post('/system/fppd/stop', 'StopFPPD');
dispatch_post('/system/fppd/skipBootDelay', 'SkipBootDelay');
dispatch_post('/system/reboot', 'RebootDevice');
dispatch_get('/system/releaseNotes/:version', 'ViewReleaseNotes');
dispatch_post('/system/shutdown', 'SystemShutdownOS');
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

addPluginEndpoints();

run();

///////////////////////////////////////////////////////////////////////////

// Returns an array of all plugin endpoints: [['plugin'=>..., 'method'=>..., 'endpoint'=>..., 'callback'=>...], ...]
function collectPluginEndpoints()
{
    global $pluginDirectory;
    $collected = array();
    $baseDir = $pluginDirectory . '/';
    if ($dir = opendir($baseDir)) {
        while (($file = readdir($dir)) !== false) {
            if (!in_array($file, array('.', '..')) && is_dir($baseDir . $file) && is_file($baseDir . $file . '/api.php')) {
                $functionsBefore = get_defined_functions();
                $userFunctionsBefore = isset($functionsBefore['user']) ? $functionsBefore['user'] : array();

                require_once $baseDir . $file . '/api.php';

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
        $path   = '/plugin/' . $ep['plugin'] . '/' . $ep['endpoint'];
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

    header('Content-Type: application/json; charset=utf-8');
    echo json_encode($spec, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
    exit;
}
