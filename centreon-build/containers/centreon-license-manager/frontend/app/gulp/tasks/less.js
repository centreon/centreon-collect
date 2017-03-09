 'use strict';

require('es6-promise').polyfill();

// Base dependencies
var gulp = require('gulp');
var gulpif = require('gulp-if');
var rename = require("gulp-rename");
var less = require('gulp-less');
var sourcemaps = require('gulp-sourcemaps');

// Less Plugins
var cleanCSSPlugin = require('less-plugin-clean-css');
var cleanless = new cleanCSSPlugin(
    {advanced: true}
);
var autoPrefixerPlugin = require('less-plugin-autoprefix');
var autoprefix = new autoPrefixerPlugin(
    {browsers: ["last 2 versions"]}
);

 //Css Plugins
 var postcss = require('gulp-postcss');
 var autoprefixer = require('autoprefixer');
 var concat = require('gulp-concat');
 var cleancss = require('gulp-clean-css');
 var removeFiles = require('gulp-clean');

 var Cssautoprefix = new autoprefixer(
     {browsers: ["last 2 versions"]}
 );

// Config
var config = require('../config').less;

gulp.task('compileLess', function() {
    return gulp.src(config.src)
        .pipe(less({
            plugins: [
                autoprefix,
                cleanless
            ]
        }))
        .pipe(rename(config.destLessFileName))
        .pipe(gulp.dest(config.dest));
});

gulp.task('compileCss', function() {
     return gulp.src(config.srcCss)
         .pipe(concat(config.destCssFileName))
         .pipe(postcss(
             [Cssautoprefix]
         ))
         .pipe(cleancss({compatibility: 'ie11'}))
         .pipe(gulp.dest(config.dest));
});

gulp.task('less', ['compileLess', 'compileCss'], function () {

    gulp.src([config.destCssFileName, config.destLessFileName])
        .pipe(concat(config.destFileName))
        .pipe(gulp.dest(config.dest));
});
 
gulp.task('less:watch', function () {
    gulp.watch(config.toWatch, ['less:dev']);
});
