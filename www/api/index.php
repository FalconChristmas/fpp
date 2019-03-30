<?
require_once 'lib/limonade.php';

$skipJSsettings = 1;
require_once '../config.php';
require_once '../common.php';

send_header("Access-Control-Allow-Origin: *");

dispatch_get   ('/help', 'help_help');

dispatch_get   ('/configfile', 'GetConfigFileList');
dispatch_get   ('/configfile/**', 'DownloadConfigFile');
dispatch_post  ('/configfile/**', 'UploadConfigFile');
dispatch_delete('/configfile/**', 'DeleteConfigFile');

dispatch_get   ('/playlists', 'playlist_list');
dispatch_post  ('/playlists', 'playlist_insert');
dispatch_get   ('/playlist/:PlaylistName', 'playlist_get');
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

run();
?>
