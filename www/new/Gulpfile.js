const autoprefixer = require('autoprefixer');
const concat = require('gulp-concat');
const del = require('del');
const generate = require('./gulp/component-generator');
const hash = require('gulp-hash');
const livereload = require('gulp-livereload');
const cleanCSS = require('gulp-clean-css');
const mode = require('gulp-mode')();
const plugins = require('gulp-load-plugins')();
const postcss = require('gulp-postcss');
const {parallel, series, src, dest, watch} = require('gulp');
const rename = require('gulp-rename');
const sass = require('gulp-sass');
const terser = require('gulp-terser');
const webpack = require('webpack-stream');
const fs = require('fs');
const path = require('path');
const {config, resourcesPath, publicPath} = require('./gulp/config');

function processJs (cb) {
    return src([
        'node_modules/@babel/polyfill/dist/polyfill.js',
        'node_modules/@stimulus/polyfills/index.js',
        `${resourcesPath}/**/*.js`,
        `!${resourcesPath}/**/_template.js`,
    ])
        .pipe(mode.development(new webpack({
            config: require(`./webpack.development.config.js`)
        })))
        .pipe(mode.production(new webpack({
            config: require(`./webpack.production.config.js`)
        })))
        .pipe(concat('main.js'))
        .pipe(rename({basename: 'main', suffix: '.min'}))
        .pipe((mode.production(terser())))
        // .pipe(hash())
        .pipe(dest(config.jsDstPath))
        .pipe(livereload({start: true}))
        .on('error', plugins.util.log)
        .on('end', () => {
            cb(); // Give the complete signal to Gulp
        });
}

function processStyles (cb) {
    return src([
        `${config.stylesSrcPath}/package.scss`,
    ])
        .pipe(sass().on('error', sass.logError))
        .pipe(rename({basename: 'main', suffix: '.min'}))
        .pipe(postcss([autoprefixer({grid: 'autoplace'})]))
        .pipe((mode.production(cleanCSS())))
        // .pipe(hash())
        .pipe(dest(config.stylesDstPath))
        .pipe(livereload({start: true}))
        .on('error', plugins.util.log)
        .on('end', () => {
            cb(); // Give the complete signal to Gulp
        });
}

exports.generate = generate;
exports.watch = function () {
    series(
        clean,
        parallel(processJs, processStyles),
        moveImages,
    );

    livereload.listen({
        key: fs.readFileSync(path.join(__dirname, 'livereload.key'), 'utf-8'),
        cert: fs.readFileSync(path.join(__dirname, 'livereload.crt'), 'utf-8')
    });

    watch([`${resourcesPath}/**/*.scss`], series(parallel(processStyles)));
    watch([`${resourcesPath}/**/*.js`], series(parallel(processJs)));
    watch([`${resourcesPath}/**/*.html.twig`], series(parallel(processStyles, processJs)));
};

exports.default = series(
    clean,
    parallel(processJs, processStyles),
    moveImages,
    stop,
);

exports.copy = series(
    moveImages,
);

function stop (cb) {
    cb();
    process.exit(); // cut off the process
}

function clean () {
    process.on('uncaughtException', err => {
        console.log(err);
    });

    return del([
        config.stylesDstPath,
        config.jsDstPath
    ]);
}

function moveImages () {
    return src([
        `${config.componentsDstPath}/**/*.{gif,jpg,png,svg}`,
        `${config.emailDstPath}/**/*.{gif,jpg,png,svg}`
    ], {base: `${resourcesPath}`})
        .pipe(dest('./public/assets/'))
        .on('error', plugins.util.log);
}
