<?php

Route::get('/', function () {
    return view('index');
});

/*
|--------------------------------------------------------------------------
| Application Routes
|--------------------------------------------------------------------------
|
| This route group applies the "web" middleware group to every route
| it contains. The "web" middleware group is defined in your HTTP
| kernel and includes session state, CSRF protection, and more.
|
*/


Route::group(['middleware' => ['api']], function ($router) {


	$router->get('/', ['as' => 'dashboard', function() { return view('index'); } ]);

	/**
	 * Playlists
	 */

	$router->get('/playlists', ['as' => 'playlists', 'uses' => 'Content\PlaylistController@index']);


	/**
	 * Scheduler
	 */
	$router->get('/schedule', ['as' => 'schedule', 'uses' => 'Content\ScheduleController@index']);


	/**
	 * Content
	 */

	$router->get('/files', ['as' => 'files', 'uses' => 'Content\FileController@index']);
	$router->post('/files/upload', ['as' => 'files.upload', 'uses' => 'Content\FileController@store']);

	/**
	 * Plugins
	 */

	$router->get('/plugins', ['as' => 'plugins', 'uses' => 'Content\PluginController@index']);
	$router->put('/plugins/disable', ['as' => 'plugins.disable', 'uses' => 'Content\PluginController@disablePlugin']);
	$router->put('/plugins/enable', ['as' => 'plugins.enable', 'uses' => 'Content\PluginController@enablePlugin']);

	/**
	 * Outputs
	 */

	$router->get('/outputs', ['as' => 'outputs', 'uses' => 'IO\E131Controller@index']);
	$router->get('/outputs/pixelnet', ['as' => 'outputs.pixelnet', 'uses' => 'IO\PixelnetController@index']);
	$router->get('/outputs/other', ['as' => 'outputs.other', 'uses' => 'IO\ChannelController@index']);
	$router->get('/outputs/remap', ['as' => 'outputs.remap', 'uses' => 'IO\ChannelController@remap']);


	/**
	 * Settings
	 */

	$router->get('/settings',		 ['as' => 'settings', 		 		 'uses' => 'Settings\SettingsController@index']);
	$router->post('/settings',		 ['as' => 'settings.store', 	     'uses' => 'Settings\SettingsController@store']);

	$router->get('/settings/network', ['as' => 'settings.network', 		 'uses' => 'Settings\NetworkController@index']);
	$router->post('/settings/network',['as' => 'settings.network.store',  'uses' => 'Settings\NetworkController@store']);

	$router->get('/settings/network/dns', ['as' => 'settings.network.dns', 		 'uses' => 'Settings\NetworkController@dns']);
	$router->post('/settings/network/dns',['as' => 'settings.network.dns.store',  'uses' => 'Settings\NetworkController@storeDNS']);


	$router->get('/settings/logs',	 ['as' => 'settings.logs',    		 'uses' => 'Settings\LogsController@index']);
	$router->post('/settings/logs',	 ['as' => 'settings.logs.store',     'uses' => 'Settings\LogsController@store']);

	$router->get('/settings/email',	 ['as' => 'settings.email',   		 'uses' => 'Settings\EmailController@index']);
	$router->post('/settings/email',	 ['as' => 'settings.email.store',    'uses' => 'Settings\EmailController@store']);

	$router->get('/settings/date',	 ['as' => 'settings.date',   		 'uses' => 'Settings\DateTimeController@index']);
	$router->post('/settings/date',	 ['as' => 'settings.date.store',    'uses' => 'Settings\DateTimeController@store']);


	/**
	 * Models
	 */

	$router->get('/models', ['as' => 'models', 'uses' => 'IO\ModelsController@index']);



	/**
	 * Testing
	 */

	$router->get('/testing', ['as' => 'testing', 'uses' => 'Testing\OutputTestingController@index']);
	$router->get('/testing/sequence', ['as' => 'testing.sequence', 'uses' => 'Testing\SequenceTestingController@index']);

});


/*
|--------------------------------------------------------------------------
| API Routes
|--------------------------------------------------------------------------
|
| 
|
*/
Route::group(['middleware' => ['api'], 'namespace' => 'Api', 'prefix' => 'api'], function ($router) {
   
    $router->get('status', function() {
    	return response()->json([
    		'data' => [
    			]
    		]);
    });

    $router->get('/fppd/start', 'FPPDController@start');
	$router->get('/fppd/stop', 'FPPDController@stop');
	$router->get('/fppd/restart', 'FPPDController@restart');
	$router->get('/fppd/mode', 'FPPDController@getMode');
	$router->get('/fppd/status', 'FPPDController@status');
	$router->get('/fppd/fstatus', 'FPPDController@fstatus');

	$router->get('/playlists', 'PlaylistController@getPlaylists');
	$router->get('/playlists/{playlist}', 'PlaylistController@getPlaylist');

	$router->get('/schedule', 'ScheduleController@getSchedule');

	$router->get('/files', 'MediaController@getAllFiles');
	$router->get('/files/music', 'MediaController@getMusicFiles');
	$router->get('/files/video', 'MediaController@getVideoFiles');
	$router->get('/files/sequence', 'MediaController@getSequenceFiles');

	$router->get('/universes', 'UniverseController@universes');
	$router->get('/universes/{universe}', 'UniverseController@getUniverse');

	$router->get('/settings', 'SettingController@getAllSettings');
	$router->get('/settings/{setting}', 'SettingController@getSetting');
	$router->put('/settings/{setting}', 'SettingController@update');

});
