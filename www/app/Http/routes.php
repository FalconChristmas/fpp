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
    	$status = [
    		'fppd' => 'running',
    		'mode' => 'standalone',
    		'status' => 0,
    		'volume' => 100,
    		'playlist' => 'No playlist scheduled.',
    		'currentDate' => Carbon\Carbon::now(),
    		'repeatMode' => 0,
    		'version' => fpp_version(),
    		'repeatMode' => 0,
    		'restartFlag' => false,
    		'rebootFlag' => false,
    		'updateFlag' => false,
    	];
    	return response()->json([
    		'data' => $status
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

	$router->get('/outputs', function() {
		return response()->json([
    		'data' => [
    			'e131' => [
    				'enabled' => true,
    				'outputs' => [
	    					[
	    						'active' => true,
	    						'universe' => 1,
	    						'start' => 1,
	    						'size' => 150,
	    						'type' => 'unicast',
	    						'address' => '10.10.10.10'

	    					],
	    					[
	    						'active' => true,
	    						'universe' => 2,
	    						'start' => 151,
	    						'size' => 150,
	    						'type' => 'unicast',
	    						'address' => '10.10.10.10'

	    					]
	    				]
    			],

				'pdmx'     => [
					'enabled' => false,
					'outputs' => []
				],
				'panels'   => [
					'enabled' => false,
					'outputs' => []
				],
				'bbb'      => [
					'enabled' => false,
					'outputs' => []
				],
				'lor'      => [
					'enabled' => false,
					'outputs' => []
				],
				'serial'   => [
					'enabled' => false,
					'outputs' => []
				],
				'renard'   => [
					'enabled' => false,
					'outputs' => []
				],
				'pixelnet' => [
					'enabled' => false,
					'outputs' => []
				],
				'triks'    => [
					'enabled' => false,
					'outputs' => []
				],
				'virtual'  => [
					'enabled' => false,
					'outputs' => []
				]


    		]

		]);
	});
	$router->get('/outputs/e131', function() {
		return response()->json([
    		'data' => [
	    			[
	    				'active' => true,
	    				'universe' => 1,
	    				'start' => 1,
	    				'size' => 150,
	    				'type' => 'unicast',
	    				'address' => '10.10.10.10'

	    			],
	    			[
	    				'active' => true,
	    				'universe' => 2,
	    				'start' => 151,
	    				'size' => 150,
	    				'type' => 'unicast',
	    				'address' => '10.10.10.10'

	    			]
    			]
    		]);
	});
});

Route::group(['middleware' => []], function ($router) {

	 $router->get('/{vue_capture?}', ['as' => 'dashboard', function () {
            return view('index');
        }])->where('vue_capture', '[\/\w\.-]*');


});