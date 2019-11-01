<?
require_once 'lib/limonade.php';

$skipJSsettings = 1;
require_once '../config.php';
require_once '../common.php';

dispatch_get   ('/help', 'help_help');

dispatch_get   ('/configfile', 'GetConfigFileList');
dispatch_get   ('/configfile/**', 'DownloadConfigFile');
dispatch_post  ('/configfile/**', 'UploadConfigFile');
dispatch_delete('/configfile/**', 'DeleteConfigFile');

dispatch_get   ('/playlists', 'playlist_list');
dispatch_post  ('/playlists', 'playlist_insert');
dispatch_get  ('/playlists/stop', 'playlist_stop');
dispatch_get  ('/playlists/stopgracefully', 'playlist_stopgracefully');
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

dispatch_get   ('/sequence', 'GetSequences');
dispatch_get   ('/sequence/:SequenceName', 'GetSequence');
dispatch_get   ('/sequence/:SequenceName/meta', 'GetSequenceMetaData');
dispatch_post  ('/sequence/:SequenceName', 'PostSequence');
dispatch_delete('/sequence/:SequenceName', 'DeleteSequence');

dispatch_get   ('/cape', 'GetCapeInfo');

dispatch_get   ('/scripts', 'scripts_list');
dispatch_get   ('/scripts/:scriptName', 'script_get');
dispatch_get   ('/scripts/:scriptName/run', 'script_run');

dispatch_get   ('/effects', 'effects_list');
dispatch_get   ('/effects/ALL', 'effects_list_ALL');

dispatch_get   ('/events', 'events_list');
dispatch_get   ('/events/:eventId', 'event_get');
dispatch_get   ('/events/:eventId/trigger', 'event_trigger');


dispatch_get   ('/proxies', 'GetProxies');
dispatch_post  ('/proxies/:ProxyIp', 'AddProxy');
dispatch_delete('/proxies/:ProxyIp', 'DeleteProxy');


run();
?>
