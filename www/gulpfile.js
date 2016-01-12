var elixir = require('laravel-elixir');
require('laravel-elixir-vueify');

var gutils = require('gulp-util');
var sassOpts = { 
        includePaths: [
            'node_modules/breakpoint-sass/stylesheets/',
            'node_modules/normalize-scss/sass/',
            'node_modules/support-for/sass/'
            ] 
        };

if(elixir.config.production == true){
    process.env.NODE_ENV = 'production';
}

if(gutils.env._.indexOf('watch') > -1 && elixir.config.production != true){
    elixir.config.js.browserify.plugins.push({
        name: "browserify-hmr",
        options : {}
    });
}

elixir(function(mix) {
    mix.sass('app.scss','public/css/app.css', sassOpts)
        .sass('vendor.scss', 'public/css/vendor.css', sassOpts);
   
    mix.browserify('main.js');

    mix.copy('resources/assets/img', 'public/img')
        .copy('resources/assets/fonts/Lato/*.ttf', 'public/fonts')
        .copy('resources/assets/fonts/Montserrat/*.ttf', 'public/fonts')
        .copy('resources/assets/fonts/Source_Sans_Pro/*.ttf', 'public/fonts');

    mix.version([
        'css/app.css', 
        'css/vendor.css', 
        'js/main.js'
    ]);

    if(gutils.env._.indexOf('watch') > -1 && elixir.config.production != true){
        mix.browserSync({
           proxy: 'fpp2.dev',
           files: [
               elixir.config.appPath + '/**/*.php',
               elixir.config.get('public.css.outputFolder') + '/**/*.css',
               elixir.config.get('public.versioning.buildFolder') + '/rev-manifest.json',
               'resources/views/**/*.php'
           ],
           
        });
    }

});
