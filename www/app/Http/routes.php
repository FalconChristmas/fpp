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

Route::group(['middleware' => []], function ($router) {

	 $router->get('/{vue_capture?}', ['as' => 'dashboard', function () {
            return view('index');
        }])->where('vue_capture', '[\/\w\.-]*');


});
