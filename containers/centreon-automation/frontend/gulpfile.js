var gulp = require('gulp');
var inject = require('gulp-inject');
var mainBowerFiles = require('gulp-main-bower-files');
var handlebars = require('gulp-handlebars');
var wrap = require('gulp-wrap');
var declare = require('gulp-declare');
var concat = require('gulp-concat');
var less = require('gulp-less');
var es = require('event-stream');
var polyglot = require('node-polyglot');
var notify = require('gulp-notify');
var replace = require('gulp-replace');
var scp = require('gulp-scp2');

gulp.task('build', ['img', 'js', 'templates', 'bower-files', 'less-automation', 'watch'], function () {
    gulp.src('./src/index.html')
        .pipe(inject(gulp.src('./app/lib/css/*.css', {read: false}), {ignorePath: '/app'}))
        .pipe(inject(gulp.src(['./app/lib/js/libs.js'], {read: false}),{ignorePath: '/app'}))
        .pipe(inject(gulp.src(['./app/lib/js/app.js'], {read: false}), {ignorePath: '/app'}))
        .pipe(inject(gulp.src(['./app/lib/js/libs.js', './app/lib/js/app.js', './app/lib/js/*.js'], {read: false}), {ignorePath: '/app'}))
        .pipe(gulp.dest('./app'))
        .pipe(notify({message: 'Build complete'}));
});

gulp.task('build-centreon', ['img', 'js', 'templates', 'bower-files', 'less-automation'], function () {
    gulp.src('./app/index.ihtml')
        .pipe(inject(gulp.src('./app/lib/css/*.css', {read: false}), {addPrefix:'{$webPath}', ignorePath: '/app/'}))
        .pipe(inject(gulp.src(['./app/lib/js/libs.js'], {read: false}), {addPrefix:'{$webPath}',ignorePath: '/app/'}))
        .pipe(inject(gulp.src(['./app/lib/js/app.js'], {read: false}), {addPrefix:'{$webPath}',ignorePath: '/app/'}))
        .pipe(inject(gulp.src(['./app/lib/js/libs.js', './app/lib/js/app.js', './app/lib/js/*.js'],{read: false}), {addPrefix:'{$webPath}',ignorePath: '/app/'}))
        .pipe(gulp.dest('./app'))
        .pipe(notify({message: 'Build complete'}));
});

gulp.task('less-automation', function () {
    return gulp.src('src/less/main.less')
        .pipe(less())
        .pipe(concat('lib/css/main.css'))
        .pipe(gulp.dest('./app'));
});

gulp.task('js', function () {
    gulp.src(['src/js/app.js'])
        .pipe(replace('jQuery', '$Automation'))
        .pipe(gulp.dest('./app/lib/js'));
    gulp.src(['!src/js/app.js',
        'src/js/*',
        'src/js/test.js',
        'src/js/views/homepage.js',
        'src/js/**/*.js'])
        .pipe(concat('lib/js/main.js'))
        .pipe(replace('jQuery', '$Automation'))
        .pipe(gulp.dest('./app'));
});

gulp.task('templates', function(){
    gulp.src('src/templates/**/*.hbs')
        .pipe(handlebars())
        .pipe(wrap('Handlebars.template(<%= contents %>)'))
        .pipe(declare({
            namespace: 'Automation.Templates',
            noRedeclare: true, // Avoid duplicate declarations
        }))
        .pipe(concat('lib/js/templates.js'))
        .pipe(gulp.dest('./app'));
});

gulp.task('img', function () {
    gulp.src(['src/img/**/*'])
        .pipe(gulp.dest('./app/img'));
});

gulp.task('bower-files', function() {
    return gulp.src('./bower.json')
        .pipe(mainBowerFiles('**/*.js'))
        .pipe(concat('lib/js/libs.js'))
        .pipe(gulp.dest('./app'));
});

gulp.task('watch', function() {
   gulp.watch('src/**/*', ['build', 'build-centreon']);
});

gulp.task('default', ['build', 'build-centreon', 'watch']);