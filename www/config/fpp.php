<?php

return [

    /*
	|--------------------------------------------------------------------------
	| FPP Debug Mode
	|--------------------------------------------------------------------------
	|
	|
	*/
    'debug'     => false,
    /*
    |--------------------------------------------------------------------------
    | FPP Setting Directory
    |--------------------------------------------------------------------------
    |
    |
    */
    'settings'  => [
        'settings_file' => '/home/fpp/media/settings',
        'defaults'      => [
            'fppdMode'               => 'player',
            'alwaysTransmit'         => false,
            'PiFaceDetected'         => false,
            'FalconHardwareDetected' => false,
            'restartFlag'            => false,
            'E131Enabled'            => true,
            'rebootFlag'             => false,
            'restartFlag'            => false,
            'screensaver'            => false,
            'forceLocalAudio'        => false,
            'E131Bridging'           => false,
            'emailenable'            => false,
            'piRTC'                  => 'N',
            'volume'                 => 75,
            'hostname'               => 'FPP',

        ]
    ],
    /*
    |--------------------------------------------------------------------------
    | FPP Media Directories & files
    |--------------------------------------------------------------------------
    |
    |
    */
    'media'     => env('MEDIA_LOCATION', '/home/fpp/media'),
    'music'     => '/music',
    'sequences' => '/sequences',
    'playlists' => '/playlists',
    'events'    => '/events',
    'videos'    => '/videos',
    'effects'   => '/effects',
    'scripts'   => '/scripts',
    'logs'      => '/logs',
    'upload'    => '/upload',
    'universes' => '/universes',
    'pixelnet'  => '/pixelnetDMX',
    'schedule'  => '/schedule',
    'bytes'     => '/bytesReceived',
    'remap'     => '/channelremap',
    'exim'      => '/exim4',
    'docs'      => fpp_dir() . '/docs',

    /*
    |--------------------------------------------------------------------------
    | FPP Email Settings
    |--------------------------------------------------------------------------
    |
    |
    */
    'email'     => [

        'enable'   => false,
        'username' => '',
        'password' => '',
        'from'     => '',
        'to'       => '',
    ],



    'menu' => [
            'default' => [
                'auto_activate'    => true,
                'activate_parents' => true,
                'active_class'     => 'active',
                'restful'          => false,
                'cascade_data'     => true,
                'rest_base'        => '',      // string|array
                'active_element'   => 'item',  // item|link
            ]
    ],


    'plugins' => [
    /*
   |--------------------------------------------------------------------------
   | Plugins Paths
   |--------------------------------------------------------------------------
   |
   | Here are the default plugin paths for the application. If the
   | same plugin (determined by the plugin's slug) is found in multiple
   | paths, the later plugin will be used. Order is important.
   |
   */

        'paths' => array(
            __DIR__.'/../plugins',
            __DIR__.'/../workbench',
        ),

        /*
        |--------------------------------------------------------------------------
        | Auto Register
        |--------------------------------------------------------------------------
        |
        | Here you may specify if the plugins are registered when the service
        | provider is booted. This will locate all plugins and register them.
        |
        | Supported: true, false.
        |
        */

        'auto_register' => true,

        /*
        |--------------------------------------------------------------------------
        | Auto Boot
        |--------------------------------------------------------------------------
        |
        | Here you may specify if the plugins are booted when all plugins
        | have been registered, similar to Laravel service providers. It allows you
        | to fire a callback once all plugins are available.
        |
        | Plugins must be auto-registered to be auto-booted.
        |
        | Supported: true, false.
        |
        */

        'auto_boot' => true,
    ]

];
