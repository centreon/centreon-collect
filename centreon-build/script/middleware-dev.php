<?php

// Load Centreon Build library.
$centreon_build_dir = dirname(__FILE__) . DIRECTORY_SEPARATOR . '..';
require_once($centreon_build_dir . DIRECTORY_SEPARATOR . 'script' . DIRECTORY_SEPARATOR . 'common.php');

# Build middleware image.
xrmdir(xpath($centreon_build_dir . '/containers/centreon-imp-portal-api'));
xcopy('.', xpath($centreon_build_dir . '/containers/centreon-imp-portal-api'));
passthru('docker build -t mon-middleware-dev:latest -f ' . xpath($centreon_build_dir) . '/containers/middleware/dev.Dockerfile ' . xpath($centreon_build_dir . '/containers'));

?>
