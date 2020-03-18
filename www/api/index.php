<?
require_once 'lib/limonade.php';

$skipJSsettings = 1;
require_once '../config.php';
require_once '../common.php';

dispatch_get   ('/backups/list', 'GetAvailableBackups');
dispatch_get   ('/backups/list/:DeviceName', 'GetAvailableBackupsOnDevice');
dispatch_get   ('/backups/devices', 'GetAvailableBackupsDevices');

dispatch_get   ('/cape', 'GetCapeInfo');

dispatch_get   ('/configfile', 'GetConfigFileList');
dispatch_get   ('/configfile/**', 'DownloadConfigFile');
dispatch_post  ('/configfile/**', 'UploadConfigFile');
dispatch_delete('/configfile/**', 'DeleteConfigFile');

dispatch_get   ('/effects', 'effects_list');
dispatch_get   ('/effects/ALL', 'effects_list_ALL');

dispatch_post  ('/email/configure', 'ConfigureEmail');
dispatch_post  ('/email/test', 'SendTestEmail');

dispatch_get   ('/events', 'events_list');
dispatch_get   ('/events/:eventId', 'event_get');
dispatch_get   ('/events/:eventId/trigger', 'event_trigger');

dispatch_get   ('/files/:DirName', 'GetFiles');

dispatch_get   ('/media', 'GetMedia');
dispatch_get   ('/media/:MediaName/duration', 'GetMediaDuration');
dispatch_get   ('/media/:MediaName/meta', 'GetMediaMetaData');

dispatch_get   ('/options/:SettingName', 'GetOptions');

dispatch_get   ('/playlists', 'playlist_list');
dispatch_post  ('/playlists', 'playlist_insert');
dispatch_get   ('/playlists/stop', 'playlist_stop');
dispatch_get   ('/playlists/stopgracefully', 'playlist_stopgracefully');
dispatch_get   ('/playlist/:PlaylistName', 'playlist_get');
dispatch_get   ('/playlist/:PlaylistName/start', 'playlist_start');
dispatch_get   ('/playlist/:PlaylistName/start/:Repeat', 'playlist_start_repeat');
dispatch_post  ('/playlist/:PlaylistName', 'playlist_update');
dispatch_delete('/playlist/:PlaylistName', 'playlist_delete');
dispatch_post  ('/playlist/:PlaylistName/:SectionName/item', 'PlaylistSectionInsertItem');

dispatch_get   ('/plugin', 'GetInstalledPlugins');
dispatch_post  ('/plugin', 'InstallPlugin');
dispatch_get   ('/plugin/:RepoName', 'GetPluginInfo');
dispatch_delete('/plugin/:RepoName', 'UninstallPlugin');
dispatch_post  ('/plugin/:RepoName/updates', 'CheckForPluginUpdates');
dispatch_post  ('/plugin/:RepoName/upgrade', 'UpgradePlugin');

dispatch_get   ('/proxies', 'GetProxies');
dispatch_post  ('/proxies/:ProxyIp', 'AddProxy');
dispatch_delete('/proxies/:ProxyIp', 'DeleteProxy');

dispatch_get   ('/remotes', 'GetRemotes');

dispatch_get   ('/sequence', 'GetSequences');
dispatch_get   ('/sequence/:SequenceName', 'GetSequence');
dispatch_get   ('/sequence/:SequenceName/meta', 'GetSequenceMetaData');
dispatch_post  ('/sequence/:SequenceName', 'PostSequence');
dispatch_delete('/sequence/:SequenceName', 'DeleteSequence');

dispatch_post  ('/schedule/reload', 'ReloadSchedule');
dispatch_get   ('/schedule', 'GetSchedule');
dispatch_post  ('/schedule', 'SaveSchedule');

dispatch_get   ('/settings', 'GetSettings');
dispatch_get   ('/settings/:SettingName', 'GetSetting');
dispatch_get   ('/settings/:SettingName/options', 'GetOptions');
dispatch_put   ('/settings/:SettingName', 'PutSetting');

dispatch_get   ('/scripts', 'scripts_list');
dispatch_get   ('/scripts/:scriptName', 'script_get');
dispatch_get   ('/scripts/:scriptName/run', 'script_run');

dispatch_get   ('/time', 'GetTime');

run();
?>
