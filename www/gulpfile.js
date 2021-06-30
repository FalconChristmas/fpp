/*
This is a gulp script for UI development.

It is used only during UI development to re-compile the fpp-bootstrap CSS framework.

Usage from the www directory:

$ npm install gulp-cli -g
$ npm install

//then:
$ gulp

// or:
$gulp watch-bs

*/

var gulp = require('gulp');
var plumber = require('gulp-plumber');
var sass = require('gulp-sass')(require('node-sass'));
var postcss = require('gulp-postcss');
var touch = require('gulp-touch-fd');
var browserSync = require('browser-sync').create();
var autoprefixer = require('autoprefixer');
var sourcemaps = require('gulp-sourcemaps');
var cfg = {
	"browserSyncOptions": {
        "server": {
			"baseDir": "./",
			"index": "components.html"
        },
		"notify": false,
		ui: {
			port: 3181
		},
		port: 3180

	},

	"browserSyncWatchFiles": [
		"./css/fpp-bootstrap/dist/fpp-bootstrap.css",
		"./css/fpp.css",
		"./components.html"
		//"./**/*.html"
	]
}

var paths = {
	scss: "./css/fpp-bootstrap/src",
	css: "./css/fpp-bootstrap/dist",

}

// Run:
// gulp sass
// Compiles SCSS files in CSS
gulp.task('sass', function () {
	var stream = gulp
		.src(paths.scss + '/*.scss')
		.pipe(
			plumber({
				errorHandler: function (err) {
					console.log(err);
					this.emit('end');
				}
			})
		)
		.pipe(sourcemaps.init({ loadMaps: true }))
		.pipe(sass({ errLogToConsole: true }))
		.pipe(postcss([autoprefixer()]))
		.pipe(gulp.dest(paths.css))
		.pipe(touch());
	return stream;
});

// Run:
// gulp watch
// Starts watcher. Watcher runs gulp sass task on changes
gulp.task('watch', function () {
	gulp.watch([`${paths.scss}/**/*.scss`, `${paths.scss}/*.scss`], gulp.series('styles'));

});




gulp.task('styles', function (callback) {
	return gulp.series('sass')(callback);
});


gulp.task('browser-sync', function () {
	browserSync.init(cfg.browserSyncWatchFiles, cfg.browserSyncOptions);
});

// Run:
// gulp watch-bs
// Starts watcher and launches component page with browsersync for live previewing.

gulp.task('watch-bs', gulp.parallel('browser-sync', 'watch'));


gulp.task('default', gulp.series('styles'));
