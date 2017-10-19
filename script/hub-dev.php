<?php

// Load Centreon Build library.
$centreon_build_dir = dirname(__FILE__) . DIRECTORY_SEPARATOR . '..';
require_once($centreon_build_dir . DIRECTORY_SEPARATOR . 'script' . DIRECTORY_SEPARATOR . 'common.php');

// Build release sources.
passthru('npm client:build');

// Build hub image.
xrmdir($centreon_build_dir . '/containers/centreon-hub-ui');
xcopy(xpath('build'), xpath($centreon_build_dir . '/containers/centreon-hub-ui'));
passthru(
    'docker build -t hub-dev:latest -f ' . $centreon_build_dir .
    '/containers/hub/Dockerfile ' . xpath($centreon_build_dir . '/containers')
);
