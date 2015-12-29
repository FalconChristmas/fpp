<?php namespace FPP\Providers;

use FPP\Menu\Menu;
use Illuminate\Support\ServiceProvider as BaseServiceProvider;

class MenuServiceProvider extends BaseServiceProvider {

	/**
	 * Indicates if loading of the provider is deferred.
	 *
	 * @var bool
	 */
	//protected $defer = true;

	/**
	 * Bootstrap the application events.
	 *
	 * @return void
	 */
	public function boot()
	{

		\Menu::make('control', function($menu){

			$menu->add('Dashboard', ['route' => 'dashboard']);
			$menu->add('Display Testing', ['route' => 'testing']);
			$menu->add('Events', '#');
			$menu->add('Effects',  '#');

		});

		\Menu::make('content', function($menu){

			$menu->add('File Manager', ['route' => 'files']);
			$menu->add('Playlists',    ['route' => 'playlists']);
			$menu->add('Schedule', ['route' => 'schedule']);
			$menu->add('Plugins',  ['route' => 'plugins']);

		});

		\Menu::make('settings', function($menu){

			$menu->add('General', ['route' => 'settings']);
			$menu->add('Network', ['route' => 'settings.network']);
			$menu->add('Channel Outputs', ['route' => 'outputs']);
			$menu->add('Overlay Models',  ['route' => 'models']);

		});

        
	}

	/**
	 * Register the service provider.
	 *
	 * @return void
	 */
	public function register()
	{
		$this->app->singleton('menu', function($app) {
			return new Menu();
		});

//		$this->app['menu'] = $this->app->share(function($app){
//
//		 		return new Menu();
//		 });
	}


	/**
	 * Get the services provided by the provider.
	 *
	 * @return array
	 */
	public function provides()
	{
		return array('menu');
	}

}
