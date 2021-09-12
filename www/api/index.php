<?
require_once 'lib/limonade.php';

$skipJSsettings = 1;
require_once '../config.php';
require_once '../common.php';

dispatch_get('/backups/list', 'GetAvailableBackups');
dispatch_get('/backups/list/:DeviceName', 'GetAvailableBackupsOnDevice');
dispatch_get('/backups/devices', 'GetAvailableBackupsDevices');

dispatch_get('/cape', 'GetCapeInfo');

dispatch_get('/channel/input/stats', 'channel_input_get_stats');
dispatch_delete('/channel/input/stats', 'channel_input_delete_stats');
dispatch_get('/channel/output/processors', 'channel_get_output_processors');
dispatch_post('/channel/output/processors', 'channel_save_output_processors');
dispatch_get('/channel/output/PixelnetDMX', 'channel_get_pixelnetDMX');
dispatch_post('/channel/output/PixelnetDMX', 'channel_put_pixelnetDMX');
dispatch_get('/channel/output/:file', 'channel_get_output');
dispatch_post('/channel/output/:file', 'channel_save_output');

dispatch_get('/configfile', 'GetConfigFileList');
dispatch_get('/configfile/**', 'DownloadConfigFile');
dispatch_post('/configfile/**', 'UploadConfigFile');
dispatch_delete('/configfile/**', 'DeleteConfigFile');

dispatch_get('/effects', 'effects_list');
dispatch_get('/effects/ALL', 'effects_list_ALL');

dispatch_post('/email/configure', 'ConfigureEmail');
dispatch_post('/email/test', 'SendTestEmail');

dispatch_get('/events', 'events_list');
dispatch_get('/events/:eventId', 'event_get');
dispatch_get('/events/:eventId/trigger', 'event_trigger');

dispatch_get('/files/:DirName', 'GetFiles');
dispatch_get('/file/move/:fileName', 'MoveFile'); // keep above file/:DirName
dispatch_get('/files/zip/:DirName', 'GetZipDir');
dispatch_post('/file/:DirName/copy/:source/:dest', 'files_copy');
dispatch_post('/file/:DirName/rename/:source/:dest', 'files_rename');
dispatch_get('/file/:DirName/:Name', 'GetFile');
dispatch_delete('/file/:DirName/:Name', 'DeleteFile');

dispatch_get('/git/originLog', 'GetGitOriginLog');
dispatch_get('/git/releases/os', 'GitOSReleases');
dispatch_get('/git/reset', 'GitReset');
dispatch_get('/git/status', 'GitStatus');

dispatch_get('/media', 'GetMedia');
dispatch_get('/media/:MediaName/duration', 'GetMediaDuration');
dispatch_get('/media/:MediaName/meta', 'GetMediaMetaData');

dispatch_get('/network/dns', 'network_get_dns');
dispatch_post('/network/dns', 'network_save_dns');
dispatch_get('/network/interface', 'network_list_interfaces');
dispatch_get('/network/interface/:interface', 'network_get_interface');
dispatch_post('/network/interface/:interface', 'network_set_interface');
dispatch_post('/network/interface/:interface/apply', 'network_apply_interface');

dispatch_delete('/network/presisentNames', 'network_presisentNames_delete');
dispatch_post('/network/presisentNames', 'network_presisentNames_create');
dispatch_get('/network/wifi/scan/:interface', 'network_wifi_scan');
dispatch_get('/network/wifi/strength', 'network_wifi_strength');
dispatch_get('/network/wifi_strength', 'network_wifi_strength'); // Legacy mapping

dispatch_get('/options/:SettingName', 'GetOptions');

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
dispatch_post('/playlist/:PlaylistName', 'playlist_update');
dispatch_delete('/playlist/:PlaylistName', 'playlist_delete');
dispatch_post('/playlist/:PlaylistName/:SectionName/item', 'PlaylistSectionInsertItem');

dispatch_get('/plugin', 'GetInstalledPlugins');
dispatch_post('/plugin', 'InstallPlugin');
dispatch_get('/plugin/:RepoName', 'GetPluginInfo');
dispatch_delete('/plugin/:RepoName', 'UninstallPlugin');
dispatch_get('/plugin/:RepoName/settings/:SettingName', 'PluginGetSetting');
dispatch_post('/plugin/:RepoName/settings/:SettingName', 'PluginSetSetting');
dispatch_post('/plugin/:RepoName/updates', 'CheckForPluginUpdates');
dispatch_get('/plugin/:RepoName/upgrade', 'UpgradePlugin');
dispatch_post('/plugin/:RepoName/upgrade', 'UpgradePlugin');
// NOTE: Plugins may also implement their own /plugin/:RepoName/* endpoints
// which are added after the above endpoints via addPluginEndpoints() below.

dispatch_get('/proxies', 'GetProxies');
dispatch_post('/proxies/:ProxyIp', 'AddProxy');
dispatch_delete('/proxies/:ProxyIp', 'DeleteProxy');

dispatch_get(array('/proxy/*/**', array("Ip", "urlPart")), 'GetProxiedURL');

dispatch_get('/remotes', 'GetRemotes');

dispatch_get('/sequence', 'GetSequences');
dispatch_get('/sequence/current/step', 'GetSequenceStep');
dispatch_get('/sequence/current/stepBack', 'GetSequenceStepBack');
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
dispatch_get('/settings/:SettingName', 'GetSetting');
dispatch_get('/settings/:SettingName/options', 'GetOptions');
dispatch_put('/settings/:SettingName', 'PutSetting');

dispatch_get('/scripts', 'scripts_list');
dispatch_get('/scripts/installRemote/:category/:filename', 'scripts_install_remote');
dispatch_get('/scripts/viewRemote/:category/:filename', 'scripts_view_remote');
dispatch_get('/scripts/:scriptName', 'script_get');
dispatch_post('/scripts/:scriptName', 'script_save');
dispatch_get('/scripts/:scriptName/run', 'script_run');

dispatch_get('/statistics/usage', 'stats_get_last_file');
dispatch_post('/statistics/usage', 'stats_publish_stats_file');
dispatch_delete('/statistics/usage', 'stats_delete_last_file');

dispatch_get('/system/fppd/restart', 'RestartFPPD');
dispatch_get('/system/fppd/start', 'StartFPPD');
dispatch_get('/system/fppd/stop', 'StopFPPD');
dispatch_get('/system/reboot', 'RebootDevice');
dispatch_get('/system/releaseNotes/:version', 'ViewReleaseNotes');
dispatch_get('/system/shutdown', 'SystemShutdownOS');
dispatch_get('/system/status', 'SystemGetStatus');
dispatch_get('/system/volume', 'SystemGetAudio');
dispatch_post('/system/volume', 'SystemSetAudio');

dispatch_get('/testmode', 'testMode_Get');
dispatch_post('/testmode', 'testMode_Set');

dispatch_get('/time', 'GetTime');

addPluginEndpoints();

run();

///////////////////////////////////////////////////////////////////////////

function addPluginEndpoints()
{
    $baseDir = '/home/fpp/media/plugins/';
    if ($dir = opendir($baseDir)) {
        while (($file = readdir($dir)) !== false) {
            if (!in_array($file, array('.', '..')) && is_dir($baseDir . $file) && is_file($baseDir . $file . '/api.php')) {
                require_once $baseDir . $file . '/api.php';

                $sfile = preg_replace('/-/', '', $file);

                $endpoints = call_user_func("getEndpoints$sfile");
                foreach ($endpoints as $ep) {
                    if ($ep['method'] == 'GET') {
                        dispatch_get('/plugin/' . $file . '/' . $ep['endpoint'], $ep['callback']);
                    } else if ($ep['method'] == 'POST') {
                        dispatch_post('/plugin/' . $file . '/' . $ep['endpoint'], $ep['callback']);
                    } else if ($ep['method'] == 'PUT') {
                        dispatch_put('/plugin/' . $file . '/' . $ep['endpoint'], $ep['callback']);
                    } else if ($ep['method'] == 'DELETE') {
                        dispatch_delete('/plugin/' . $file . '/' . $ep['endpoint'], $ep['callback']);
                    }
                }
            }
        }
    }
}
