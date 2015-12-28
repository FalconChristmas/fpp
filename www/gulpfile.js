var elixir = require('laravel-elixir');
require('laravel-elixir-vueify');


elixir(function(mix) {
    mix.sass('app.scss');
    mix.browserify('main.js');

    mix.copy('resources/assets/img', 'public/img')
        .copy('resources/assets/fonts/Lato/*.ttf', 'public/fonts')
        .copy('resources/assets/fonts/Montserrat/*.ttf', 'public/fonts')
        .copy('resources/assets/fonts/Source_Sans_Pro/*.ttf', 'public/fonts');

    mix.version(['css/app.css', 'js/main.js']);
});
