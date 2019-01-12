<?
require_once 'lib/limonade.php';

$skipJSsettings = 1;
require_once '../config.php';
require_once '../common.php';

send_header("Access-Control-Allow-Origin: *");

dispatch_get ('/help', 'help_help');

dispatch_get ('/playlists', 'playlist_list');
dispatch_post('/playlists', 'playlist_insert');
dispatch_get ('/playlist/:PlaylistName', 'playlist_get');
dispatch_post('/playlist/:PlaylistName', 'playlist_update');
dispatch_delete('/playlist/:PlaylistName', 'playlist_delete');
dispatch_post('/playlist/:PlaylistName/:SectionName/item', 'PlaylistSectionInsertItem');

dispatch_post ('/models/raw', 'post_models_raw');


run();
?>
