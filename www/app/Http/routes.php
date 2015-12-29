<?php

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

	 $router->get('/{vue_capture?}', ['as' => 'dashboard', function () {
            return view('index');
        }])->where('vue_capture', '[\/\w\.-]*');


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
