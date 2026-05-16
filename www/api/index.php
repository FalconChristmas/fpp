<?
require_once 'lib/limonade.php';

$skipJSsettings = 1;
require_once '../config.php';
require_once '../common.php';
require_once 'controllers/helpers.php';

$apiVersionPrefixes = ['', '/v2'];

function dispatch_all(string $path, string $method, string $fn): void {
    global $apiVersionPrefixes;
    foreach ($apiVersionPrefixes as $prefix) {
        call_user_func("dispatch_{$method}", $prefix . $path, $fn);
    }
}

// Spec / docs — v1 reads from www/api/v1/, v2 reads from www/api/v2/
dispatch_get('/',             'ServeApiDocs_v1');
dispatch_get('/api.html',     'ServeApiHtml_v1');
dispatch_get('/openapi.yaml', 'ServeOpenApiSpec_v1');
dispatch_get('/openapi.json', 'ServeOpenApiSpec_v1');
dispatch_get('/v1/',             'ServeApiDocs_v1');
dispatch_get('/v1/api.html',     'ServeApiHtml_v1');
dispatch_get('/v1/openapi.yaml', 'ServeOpenApiSpec_v1');
dispatch_get('/v1/openapi.json', 'ServeOpenApiSpec_v1');
dispatch_get('/v2/',             'ServeApiDocs_v2');
dispatch_get('/v2/api.html',     'ServeApiHtml_v2');
dispatch_get('/v2/openapi.yaml', 'ServeOpenApiSpec_v2');
dispatch_get('/v2/openapi.json', 'ServeOpenApiSpec_v2');

// Backups
dispatch_all('/backups/list', 'get', 'GetAvailableBackups');
dispatch_all('/backups/list/:DeviceName', 'get', 'GetAvailableBackupsOnDevice');
dispatch_all('/backups/devices', 'get', 'RetrieveAvailableBackupsDevices');
dispatch_all('/backups/devices/mount/:DeviceName/:MountLocation', 'post', 'MountDevice');
dispatch_all('/backups/devices/unmount/:DeviceName/:MountLocation', 'post', 'UnmountDevice');
dispatch_all('/backups/configuration', 'post', 'MakeJSONBackup');
dispatch_all('/backups/configuration/list', 'get', 'GetAvailableJSONBackups');
dispatch_all('/backups/configuration/list/:DeviceName', 'get', 'GetAvailableJSONBackupsOnDevice');
dispatch_all('/backups/configuration/restore/:Directory/:BackupFilename', 'post', 'RestoreJsonBackup');
dispatch_all('/backups/configuration/:Directory/:BackupFilename', 'get', 'DownloadJsonBackup');
dispatch_all('/backups/configuration/:Directory/:BackupFilename', 'delete', 'DeleteJsonBackup');

// Cape
dispatch_all('/cape', 'get', 'GetCapeInfo');
dispatch_all('/cape/eeprom/voucher', 'post', 'RedeemVoucher');
dispatch_all('/cape/eeprom/sign/:key/:order', 'post', 'SignEEPROM');
dispatch_all('/cape/eeprom/signingData/:key/:order', 'get', 'GetSigningData');
dispatch_all('/cape/eeprom/signingFile/:key/:order', 'get', 'GetSigningFile');
dispatch_all('/cape/eeprom/signingData', 'post', 'PostSigningData');
dispatch_all('/cape/options', 'get', 'GetCapeOptions');
dispatch_all('/cape/strings', 'get', 'GetCapeStringOptions');
dispatch_all('/cape/panel', 'get', 'GetCapePanelOptions');
dispatch_all('/cape/strings/:key', 'get', 'GetCapeStringConfig');
dispatch_all('/cape/panel/:key', 'get', 'GetCapePanelConfig');

// Channel
dispatch_all('/channel/input/stats', 'get', 'ChannelInputGetStats');
dispatch_all('/channel/input/stats', 'delete', 'ChannelInputDeleteStats');
dispatch_all('/channel/output/processors', 'get', 'ChannelGetOutputProcessors');
dispatch_all('/channel/output/processors', 'post', 'ChannelSaveOutputProcessors');
dispatch_all('/channel/output/:file', 'get', 'ChannelGetOutput');
dispatch_all('/channel/output/:file', 'post', 'ChannelSaveOutput');

// Config files
dispatch_all('/configfile', 'get', 'GetConfigFileList');
dispatch_all('/configfile/**', 'get', 'DownloadConfigFile');
dispatch_all('/configfile/**', 'post', 'UploadConfigFile');
dispatch_all('/configfile/**', 'delete', 'DeleteConfigFile');

// Directories
dispatch_all('/dir/:DirName/:SubDir', 'post', 'CreateDir');
dispatch_all('/dir/:DirName/:SubDir', 'delete', 'DeleteDir');

// Effects
dispatch_all('/effects', 'get', 'EffectsList');
dispatch_all('/effects/ALL', 'get', 'EffectsListAll');

// Email
dispatch_all('/email/configure', 'post', 'ConfigureEmail');
dispatch_all('/email/test', 'post', 'SendTestEmail');

// Events
dispatch_all('/events', 'get', 'EventsList');
dispatch_all('/events/:eventId', 'get', 'EventGet');
dispatch_get('/events/:eventId/trigger', 'EventTrigger');         // v1: GET
dispatch_post('/v2/events/:eventId/trigger', 'EventTrigger');     // v2: POST

// Files — more-specific routes must precede /file/:DirName/**
dispatch_all('/files/:DirName', 'get', 'GetFiles');
dispatch_all('/file/info/:plugin/:ext/**', 'get', 'GetPluginFileInfo'); // keep above /file/:DirName
dispatch_get('/file/onUpload/:ext/**', 'PluginFileOnUpload');           // v1: GET; keep above /file/:DirName
dispatch_post('/v2/file/onUpload/:ext/**', 'PluginFileOnUpload');       // v2: POST
dispatch_get('/file/move/:fileName', 'MoveFile');                       // v1: GET; keep above /file/:DirName
dispatch_post('/v2/file/move/:fileName', 'MoveFile');                   // v2: POST
dispatch_all('/files/zip/:DirNames', 'get', 'GetZipDir');
dispatch_all('/file/:DirName/copy/:source/:dest', 'post', 'FilesCopy');
dispatch_all('/file/:DirName/rename/:source/:dest', 'post', 'FilesRename');
dispatch_all('/file/:DirName/tailfollow/**', 'get', 'TailFollowFile');
dispatch_all('/file/:DirName/**', 'get', 'GetFile');
dispatch_all('/file/:DirName/**', 'delete', 'DeleteFile');
dispatch_all('/file/:DirName', 'post', 'PatchFile');
dispatch_all('/file/:DirName', 'patch', 'PatchFile');
dispatch_all('/file/:DirName/:Name', 'post', 'PostFile');

// Git
dispatch_all('/git/originLog', 'get', 'GetGitOriginLog');
dispatch_all('/git/releases/os/:All', 'get', 'GitOSReleases');
dispatch_all('/git/releases/sizes', 'get', 'GitOSReleaseSizes');
dispatch_get('/git/reset', 'GitReset');                           // v1: GET
dispatch_post('/v2/git/reset', 'GitReset');                       // v2: POST
dispatch_all('/git/status', 'get', 'GitStatus');
dispatch_all('/git/branches', 'get', 'GitBranches');

// Media
dispatch_all('/media', 'get', 'GetMedia');
dispatch_all('/media/:MediaName/duration', 'get', 'GetMediaDuration');
dispatch_all('/media/:MediaName/meta', 'get', 'GetMediaMetaData');

// Network
dispatch_all('/network/dns', 'get', 'NetworkGetDNS');
dispatch_all('/network/dns', 'post', 'NetworkSaveDNS');
dispatch_all('/network/gateway', 'get', 'NetworkGetGateway');
dispatch_all('/network/gateway', 'post', 'NetworkSaveGateway');
dispatch_all('/network/interface', 'get', 'NetworkListInterfaces');
dispatch_all('/network/interface/:interface', 'get', 'NetworkGetInterface');
dispatch_get('/network/interface/add/:interface', 'NetworkAddInterface');    // v1: GET
dispatch_post('/v2/network/interface/add/:interface', 'NetworkAddInterface'); // v2: POST
dispatch_all('/network/interface/:interface', 'post', 'NetworkSetInterface');
dispatch_all('/network/interface/:interface/apply', 'post', 'NetworkApplyInterface');
dispatch_all('/network/presisentNames', 'delete', 'NetworkPersistentNamesDelete');
dispatch_all('/network/presisentNames', 'post', 'NetworkPersistentNamesCreate');
dispatch_all('/network/persistentNames', 'delete', 'NetworkPersistentNamesDelete');
dispatch_all('/network/persistentNames', 'post', 'NetworkPersistentNamesCreate');
dispatch_all('/network/wifi/scan/:interface', 'get', 'NetworkWiFiScan');
dispatch_all('/network/wifi/strength', 'get', 'NetworkWiFiStrength');

// Options
dispatch_all('/options/:SettingName', 'get', 'GetOptions');

// Audio
dispatch_all('/audio/cardaliases', 'get', 'GetAudioCardAliases');
dispatch_all('/audio/cardaliases', 'post', 'SaveAudioCardAliases');

// PipeWire
dispatch_all('/pipewire/audio/groups', 'get', 'GetPipeWireAudioGroups');
dispatch_all('/pipewire/audio/groups', 'post', 'SavePipeWireAudioGroups');
dispatch_all('/pipewire/audio/groups/apply', 'post', 'ApplyPipeWireAudioGroups');
dispatch_all('/pipewire/audio/sinks', 'get', 'GetPipeWireSinks');
dispatch_all('/pipewire/audio/cards', 'get', 'GetPipeWireAudioCards');
dispatch_all('/pipewire/audio/sources', 'get', 'GetPipeWireAudioSources');
dispatch_all('/pipewire/audio/input-groups', 'get', 'GetPipeWireInputGroups');
dispatch_all('/pipewire/audio/input-groups', 'post', 'SavePipeWireInputGroups');
dispatch_all('/pipewire/audio/input-groups/apply', 'post', 'ApplyPipeWireInputGroups');
dispatch_all('/pipewire/audio/input-groups/volume', 'post', 'SetInputGroupMemberVolume');
dispatch_all('/pipewire/audio/input-groups/effects', 'post', 'SaveInputGroupEffects');
dispatch_all('/pipewire/audio/input-groups/eq/update', 'post', 'UpdateInputGroupEQRealtime');
dispatch_all('/pipewire/audio/routing', 'get', 'GetRoutingMatrix');
dispatch_all('/pipewire/audio/routing', 'post', 'SaveRoutingMatrix');
dispatch_all('/pipewire/audio/routing/volume', 'post', 'SetRoutingPathVolume');
dispatch_all('/pipewire/audio/routing/presets', 'get', 'GetRoutingPresets');
dispatch_all('/pipewire/audio/routing/presets/names', 'get', 'GetRoutingPresetNames');
dispatch_all('/pipewire/audio/routing/presets', 'post', 'SaveRoutingPreset');
dispatch_all('/pipewire/audio/routing/presets/load', 'post', 'LoadRoutingPreset');
dispatch_all('/pipewire/audio/routing/presets/live-apply', 'post', 'LiveApplyRoutingPreset');
dispatch_all('/pipewire/audio/routing/presets/:name', 'delete', 'DeleteRoutingPreset');
dispatch_all('/pipewire/audio/stream/volume', 'post', 'SetStreamSlotVolume');
dispatch_all('/pipewire/audio/stream/status', 'get', 'GetStreamSlotStatus');
dispatch_all('/pipewire/audio/group/volume', 'post', 'SetPipeWireGroupVolume');
dispatch_all('/pipewire/audio/eq/update', 'post', 'UpdatePipeWireEQRealtime');
dispatch_all('/pipewire/audio/delay/update', 'post', 'UpdatePipeWireDelayRealtime');
dispatch_all('/pipewire/audio/sync/start', 'post', 'StartSyncCalibration');
dispatch_all('/pipewire/audio/sync/stop', 'post', 'StopSyncCalibration');
dispatch_all('/pipewire/video/groups', 'get', 'GetPipeWireVideoGroups');
dispatch_all('/pipewire/video/groups', 'post', 'SavePipeWireVideoGroups');
dispatch_all('/pipewire/video/groups/apply', 'post', 'ApplyPipeWireVideoGroups');
dispatch_all('/pipewire/simple/apply', 'post', 'ApplyPipeWireSimpleConfig');
dispatch_all('/pipewire/video/connectors', 'get', 'GetVideoOutputTargets');
dispatch_all('/pipewire/video/routing', 'get', 'GetVideoRoutingMatrix');
dispatch_all('/pipewire/video/routing', 'post', 'SaveVideoRoutingMatrix');
dispatch_all('/pipewire/video/input-sources', 'get', 'GetPipeWireVideoInputSources');
dispatch_all('/pipewire/video/input-sources', 'post', 'SavePipeWireVideoInputSources');
dispatch_all('/pipewire/video/input-sources/apply', 'post', 'ApplyPipeWireVideoInputSources');
dispatch_all('/pipewire/video/input-sources/v4l2-devices', 'get', 'GetV4L2Devices');
dispatch_all('/pipewire/aes67/instances', 'get', 'GetAES67Instances');
dispatch_all('/pipewire/aes67/instances', 'post', 'SaveAES67Instances');
dispatch_all('/pipewire/aes67/apply', 'post', 'ApplyAES67Instances');
dispatch_all('/pipewire/aes67/status', 'get', 'GetAES67Status');
dispatch_all('/pipewire/aes67/interfaces', 'get', 'GetAES67NetworkInterfaces');
dispatch_all('/pipewire/graph', 'get', 'GetPipeWireGraph');

// Playlists
dispatch_all('/playlists', 'get', 'PlaylistList');
dispatch_all('/playlists', 'post', 'PlaylistInsert');
dispatch_all('/playlists/playable', 'get', 'PlaylistPlayable');
dispatch_all('/playlists/validate', 'get', 'PlaylistListValidate');
dispatch_get('/playlists/stop', 'PlaylistStop');                                               // v1: GET
dispatch_post('/v2/playlists/stop', 'PlaylistStop');                                           // v2: POST
dispatch_get('/playlists/pause', 'PlaylistPause');                                             // v1: GET
dispatch_post('/v2/playlists/pause', 'PlaylistPause');                                         // v2: POST
dispatch_get('/playlists/resume', 'PlaylistResume');                                           // v1: GET
dispatch_post('/v2/playlists/resume', 'PlaylistResume');                                       // v2: POST
dispatch_get('/playlists/stopgracefully', 'PlaylistStopGracefully');                           // v1: GET
dispatch_post('/v2/playlists/stopgracefully', 'PlaylistStopGracefully');                       // v2: POST
dispatch_get('/playlists/stopgracefullyafterloop', 'PlaylistStopGracefullyAfterLoop');         // v1: GET
dispatch_post('/v2/playlists/stopgracefullyafterloop', 'PlaylistStopGracefullyAfterLoop');     // v2: POST
dispatch_all('/playlist/:PlaylistName', 'get', 'PlaylistGet');
dispatch_get('/playlist/:PlaylistName/start', 'PlaylistStart');                                // v1: GET
dispatch_post('/v2/playlist/:PlaylistName/start', 'PlaylistStart');                            // v2: POST
dispatch_get('/playlist/:PlaylistName/start/:Repeat', 'PlaylistStartRepeat');                  // v1: GET
dispatch_post('/v2/playlist/:PlaylistName/start/:Repeat', 'PlaylistStartRepeat');              // v2: POST
dispatch_get('/playlist/:PlaylistName/start/:Repeat/:ScheduleProtected', 'PlaylistStartRepeatProtected'); // v1: GET
dispatch_post('/v2/playlist/:PlaylistName/start/:Repeat/:ScheduleProtected', 'PlaylistStartRepeatProtected'); // v2: POST
dispatch_all('/playlist/:PlaylistName', 'post', 'PlaylistUpdate');
dispatch_all('/playlist/:PlaylistName', 'delete', 'PlaylistDelete');
dispatch_all('/playlist/:PlaylistName/:SectionName/item', 'post', 'PlaylistSectionInsertItem');

// Plugins
dispatch_all('/plugin/headerIndicators', 'get', 'GetPluginHeaderIndicators');
dispatch_all('/plugin', 'get', 'GetInstalledPlugins');
dispatch_all('/plugin', 'post', 'InstallPlugin');
dispatch_all('/plugin/fetchInfo', 'post', 'FetchPluginInfoProxy');
dispatch_all('/plugin/:RepoName', 'get', 'GetPluginInfo');
dispatch_all('/plugin/:RepoName', 'delete', 'UninstallPlugin');
dispatch_all('/plugin/:RepoName/settings/:SettingName', 'get', 'PluginGetSetting');
dispatch_all('/plugin/:RepoName/settings/:SettingName', 'put', 'PluginSetSetting');
dispatch_all('/plugin/:RepoName/settings/:SettingName', 'post', 'PluginSetSetting');
dispatch_all('/plugin/:RepoName/updates', 'post', 'CheckForPluginUpdates');
dispatch_get('/plugin/:RepoName/upgrade', 'UpgradePlugin');              // v1: GET (backward compat only)
dispatch_all('/plugin/:RepoName/upgrade', 'post', 'UpgradePlugin');
// NOTE: Plugins may also implement their own /plugin/:RepoName/* endpoints
// which are added after the above endpoints via addPluginEndpoints() below.

// Proxies
dispatch_all('/proxies', 'get', 'GetProxies');
dispatch_all('/proxies', 'post', 'PostProxies');
dispatch_all('/proxies', 'delete', 'DeleteAllProxies');
dispatch_all('/proxies/:ProxyIp', 'post', 'AddProxy');
dispatch_all('/proxies/:ProxyIp', 'delete', 'DeleteProxy');

foreach (['', '/v2'] as $prefix) {
    dispatch_get(array($prefix . '/proxy/*/**', array("Ip", "urlPart")), 'GetProxiedURL');
}

// Remotes
dispatch_all('/remotes', 'get', 'GetRemotes');
dispatch_get('/remoteAction', 'RemoteAction_v1');       // v1: GET + query params
dispatch_post('/v2/remoteAction', 'RemoteAction');      // v2: POST + JSON body

// Sequences
dispatch_all('/sequence', 'get', 'GetSequences');
dispatch_get('/sequence/current/step', 'GetSequenceStep');               // v1: GET
dispatch_post('/v2/sequence/current/step', 'GetSequenceStep');           // v2: POST
dispatch_get('/sequence/current/stop', 'GetSequenceStop');               // v1: GET
dispatch_post('/v2/sequence/current/stop', 'GetSequenceStop');           // v2: POST
dispatch_get('/sequence/current/togglePause', 'GetSequenceTogglePause'); // v1: GET
dispatch_post('/v2/sequence/current/togglePause', 'GetSequenceTogglePause'); // v2: POST
dispatch_all('/sequence/:SequenceName', 'get', 'GetSequence');
dispatch_all('/sequence/:SequenceName/meta', 'get', 'GetSequenceMetaData');
dispatch_get('/sequence/:SequenceName/start/:startSecond', 'GetSequenceStart');    // v1: GET
dispatch_post('/v2/sequence/:SequenceName/start/:startSecond', 'GetSequenceStart'); // v2: POST
dispatch_all('/sequence/:SequenceName', 'post', 'PostSequence');
dispatch_all('/sequence/:SequenceName', 'delete', 'DeleteSequence');

// Schedule
dispatch_all('/schedule/reload', 'post', 'ReloadSchedule');
dispatch_all('/schedule', 'get', 'GetSchedule');
dispatch_all('/schedule', 'post', 'SaveSchedule');

// Settings
dispatch_all('/settings', 'get', 'GetSettings');
dispatch_all('/settings/:SettingName', 'get', 'GetSetting');
dispatch_all('/settings/:SettingName/options', 'get', 'GetOptions');
dispatch_all('/settings/:SettingName', 'put', 'PutSetting');
dispatch_all('/settings/:SettingName/jsonValueUpdate', 'put', 'UpdateJSONValueSetting');

// Scripts
dispatch_all('/scripts', 'get', 'ScriptsList');
dispatch_get('/scripts/installRemote/:category/:filename', 'ScriptsInstallRemote');     // v1: GET
dispatch_post('/v2/scripts/installRemote/:category/:filename', 'ScriptsInstallRemote'); // v2: POST
dispatch_all('/scripts/viewRemote/:category/:filename', 'get', 'ScriptsViewRemote');
dispatch_all('/scripts/:scriptName', 'get', 'ScriptGet');
dispatch_all('/scripts/:scriptName', 'post', 'ScriptSave');
dispatch_get('/scripts/:scriptName/run', 'ScriptRun');                   // v1: GET
dispatch_post('/v2/scripts/:scriptName/run', 'ScriptRun');               // v2: POST

// Statistics
dispatch_all('/statistics/usage', 'get', 'StatsGetLastFile');
dispatch_all('/statistics/usage', 'post', 'StatsPublishStatsFile');
dispatch_all('/statistics/usage', 'delete', 'StatsDeleteLastFile');

// System
dispatch_get('/system/fppd/restart', 'RestartFPPD');                    // v1: GET
dispatch_post('/v2/system/fppd/restart', 'RestartFPPD');                 // v2: POST
dispatch_get('/system/fppd/start', 'StartFPPD');                         // v1: GET
dispatch_post('/v2/system/fppd/start', 'StartFPPD');                     // v2: POST
dispatch_get('/system/fppd/stop', 'StopFPPD');                           // v1: GET
dispatch_post('/v2/system/fppd/stop', 'StopFPPD');                       // v2: POST
dispatch_all('/system/fppd/skipBootDelay', 'post', 'SkipBootDelay');
dispatch_get('/system/reboot', 'RebootDevice');                          // v1: GET
dispatch_post('/v2/system/reboot', 'RebootDevice');                      // v2: POST
dispatch_all('/system/releaseNotes/:version', 'get', 'ViewReleaseNotes');
dispatch_get('/system/shutdown', 'SystemShutdownOS');                    // v1: GET
dispatch_post('/v2/system/shutdown', 'SystemShutdownOS');                // v2: POST
dispatch_all('/system/status', 'get', 'SystemGetStatus');
dispatch_all('/system/updateStatus', 'get', 'GetUpdateStatus');
dispatch_all('/system/info', 'get', 'SystemGetInfo');
dispatch_all('/system/volume', 'get', 'SystemGetAudio');
dispatch_all('/system/volume', 'post', 'SystemSetAudio');
dispatch_all('/system/proxies', 'post', 'PostProxies');
dispatch_all('/system/proxies', 'get', 'GetProxies');
dispatch_all('/system/packages', 'get', 'GetOSPackages');
dispatch_all('/system/packages/info/:packageName', 'get', 'GetOSPackageInfo');

// Test mode
dispatch_all('/testmode', 'get', 'TestModeGet');
dispatch_all('/testmode', 'post', 'TestModeSet');

// Time
dispatch_all('/time', 'get', 'GetTime');

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
        dispatch_all($path, strtolower($ep['method']), $ep['callback']);
    }
}

function ServeApiDocs_v1()
{
    set_include_path(get_include_path() . PATH_SEPARATOR . dirname(__DIR__));
    extract($GLOBALS);
    include __DIR__ . '/api.php';
    exit;
}

function ServeApiDocs_v2()
{
    set_include_path(get_include_path() . PATH_SEPARATOR . dirname(__DIR__));
    extract($GLOBALS);
    include __DIR__ . '/api.php';
    exit;
}

function ServeApiHtml_v1()
{
    header('Content-Type: text/html; charset=utf-8');
    readfile(__DIR__ . '/v1/api.html');
    exit;
}

function ServeApiHtml_v2()
{
    header('Content-Type: text/html; charset=utf-8');
    readfile(__DIR__ . '/v2/api.html');
    exit;
}

function ServeOpenApiSpec_v1()
{
    $spec = json_decode(file_get_contents(__DIR__ . '/v1/openapi.json'), true);

    foreach (collectPluginEndpoints() as $ep) {
        $method = strtolower($ep['method']);
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

    header('Content-Type: application/json; charset=utf-8');
    echo json_encode($spec, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
    exit;
}

function ServeOpenApiSpec_v2()
{
    $spec = json_decode(file_get_contents(__DIR__ . '/v2/openapi.json'), true);

    foreach (collectPluginEndpoints() as $ep) {
        $method = strtolower($ep['method']);
        $path   = '/api/v2/plugin/' . $ep['plugin'] . '/' . $ep['endpoint'];
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
