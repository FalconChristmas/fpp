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
var sass = require('gulp-sass');
var babel = require('gulp-babel');
var postcss = require('gulp-postcss');
var touch = require('gulp-touch-fd');
var rename = require('gulp-rename');
var concat = require('gulp-concat');
var uglify = require('gulp-uglify');
var ignore = require('gulp-ignore');
var rimraf = require('gulp-rimraf');
var browserSync = require('browser-sync').create();
var sourcemaps = require('gulp-sourcemaps');
var cleanCSS = require('gulp-clean-css');
var autoprefixer = require('autoprefixer');
const webpack = require('webpack-stream');
const named = require('vinyl-named');

var cfg = {
	"browserSyncOptions": {
        "server": {
			"baseDir": "./",
			"index": "components.html"
        },
		"notify": false
	},
	"browserSyncWatchFiles": [
		"./css/fpp-bootstrap/dist/*.min.css",
		"./js/fpp-bootstrap/dist/*.min.js",
		"./components.html"
		//"./**/*.html"
	]
}

var paths = {
	scss: "./css/fpp-bootstrap/src",
	css: "./css/fpp-bootstrap/dist",
	jssrc:"./js/fpp-bootstrap/src",
	js:"./js/fpp-bootstrap/dist",

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
	gulp.watch(
		[
			`${paths.jssrc}/*.js`,
			`!${paths.jssrc}/fpp-bootstrap.bundle.js`
		],
		gulp.series('bundle-js')
	);
});


gulp.task('cssnano', function () {
	return gulp
		.src(paths.css + '/fpp-bootstrap.css')
		.pipe(sourcemaps.init({ loadMaps: true }))
		.pipe(
			plumber({
				errorHandler: function (err) {
					console.log(err);
					this.emit('end');
				}
			})
		)
		.pipe(rename({ suffix: '.min' }))
		.pipe(cssnano({ discardComments: { removeAll: true } }))
		.pipe(sourcemaps.write('./'))
		.pipe(gulp.dest(paths.css))
		.pipe(touch());
});

gulp.task('minifycss', function () {

	return gulp
		.src([
			`${paths.css}/fpp-bootstrap.css`,
		])
		.pipe(sourcemaps.init({
			loadMaps: true
		}))
		.pipe(cleanCSS({
			compatibility: '*'
		}))
		.pipe(
			plumber({
				errorHandler: function (err) {
					console.log(err);
					this.emit('end');
				}
			})
		)
		.pipe(rename({ suffix: '.min' }))
		.pipe(sourcemaps.write('./'))
		.pipe(gulp.dest(paths.css))
		.pipe(touch());
});

gulp.task('cleancss', function () {
	return gulp
		.src(`${paths.css}/*.min.css`, { read: false }) 
		.pipe(ignore('fpp-bootstrap.css'))
		.pipe(rimraf());
});

gulp.task('styles', function (callback) {
	return gulp.series('sass', 'minifycss')(callback);
});

gulp.task('bundle-js', function() {
	return gulp.src([`${paths.jssrc}/fpp-bootstrap.js`])
		.pipe(named(function(file){
			return 'fpp-bootstrap.min'
		}))
		.pipe(webpack({
			mode:"production"
		}))
		.on('error', function handleError() {
			this.emit('end'); // Recover from errors
		})
		.pipe(gulp.dest(paths.js));
});


gulp.task('scripts', function () {
	var scripts = [

		// `${paths.dev}/js/bootstrap4/bootstrap.bundle.js`,
		// `${paths.dev}/js/themejs/*.js`,
		// `${paths.dev}/js/skip-link-focus-fix.js`,
		`${paths.jssrc}/js/fpp-bootstrap.bundle.js`
	];
	gulp
		.src(scripts, { allowEmpty: true })
		.pipe(babel(
			{
			presets: ['@babel/preset-env']
			}
		))
		.pipe(concat('fpp-bootstrap.min.js'))
		.pipe(uglify())
		.pipe(gulp.dest(paths.js));

	return gulp
		.src(scripts, { allowEmpty: true })
		.pipe(babel())
		.pipe(concat('fpp-bootstrap.js'))
		.pipe(gulp.dest(paths.js));
});

gulp.task('browser-sync', function () {
	browserSync.init(cfg.browserSyncWatchFiles, cfg.browserSyncOptions);
});

// Run:
// gulp watch-bs
// Starts watcher and launches component page with browsersync for live previewing.

gulp.task('watch-bs', gulp.parallel('browser-sync', 'watch'));

gulp.task('default', gulp.series('watch'));