# FPP Player

### with the folling dependencies
- Slim Framework v4 Skeleton
- Gulp
- Twig
- Stimulus

### Install
If you checkout the project for the first time you'll need to run these commands in the directory `www/new`;

This will install all php dependencies via [composer](https://getcomposer.org/doc/00-intro.md)

`$ composer install`


This will install css/js dependencies via [yarn](https://yarnpkg.com/)

`$ yarn`

When the dependencies have been installed you need to pack all the scss and js files together with a simple command.

`$ gulp` or `$ node_modules/gulp/bin/gulp.js`

You can also run `$ gulp watch` or `$ node_modules/gulp/bin/gulp.js watch`. This keeps Gulp watching the scss and js files and compiles them when a change has been done.

The js and css are already present by default. This is so a freshly baked version of FPP does not have to compile it on the fly.


### Starting the built-in server of Slim Skeleton

The Slim Skeleton framework has a simple php server which we can start to develop the frontend easily.
Run the following command (the port can vary if you like);

`$ php -S localhost:8080 -t public public/index.php`

### Component generator
To use the Gulp component generator:

`$ gulp generate --name "here-youre-name"`

The string is not escaped, use a string that has dashes ([kebab-case](https://wiki.c2.com/?KebabCase))

Optional parameters can be added like;<br>
(When such a parameter is used the specific file isn't generated)

* `--nojs`
* `--noscss`
* `--notwig`
