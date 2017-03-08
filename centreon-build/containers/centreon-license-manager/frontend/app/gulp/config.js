'use strict';

var destBase = '.';
var srcBase  = './src';
var moduleName = 'license-manager';
var lessFile = 'all_less';
var nodeBase = './node_modules';
var cssFile = 'all_css';

module.exports = {
    less: {
        src: srcBase + '/index.less',
        dest: destBase,
        destFileName: moduleName + '.css',
        toWatch: srcBase + '/**/*.less',
        srcCss: nodeBase + '/**/dist/*.css',
        destLessFileName: lessFile + '.css',
        destCssFileName: cssFile + '.css'
    },
    scripts: {
        src: srcBase + '/**/!(index)*.js',
        dest: destBase,
        entryPoint: srcBase + '/index.js',
        destFileName: moduleName + '.js',
        toWatch: srcBase + '/**/*.{js,html}'
    }
};