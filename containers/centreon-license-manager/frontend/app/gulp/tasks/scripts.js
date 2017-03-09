 'use strict';

// Base dependencies
var gulp = require('gulp');
var gulpif = require('gulp-if');
var source = require('vinyl-source-stream');
var buffer = require('vinyl-buffer');
var browserify = require('browserify');
var stringify = require('stringify');
var uglify = require('gulp-uglify');

// Config
var config = require('../config').scripts;

function concatScripts(uglifyOutput) {
    return browserify({
        entries: config.entryPoint,
        debug: !uglifyOutput
    })
    .transform(stringify({
        extensions: ['.html', '.json']
    }))
    .bundle()
    .pipe(source(config.destFileName))
    .pipe(buffer())
        /*
    .pipe(gulpif(
        uglifyOutput,
        uglify()
    ))
    */
    .pipe(gulp.dest(config.dest));
}

gulp.task('scripts', function () {
    concatScripts(true);
});

gulp.task('scripts:dev', function () {
    concatScripts(false);
});

gulp.task('scripts:watch', function () {
    gulp.watch(config.toWatch, ['scripts:dev']);
});