<?php

// Load Centreon Build library.
$centreon_build_dir = dirname(__FILE__) . DIRECTORY_SEPARATOR . '..';
require_once($centreon_build_dir . DIRECTORY_SEPARATOR . 'script' . DIRECTORY_SEPARATOR . 'common.php');
// require_once($centreon_build_dir . DIRECTORY_SEPARATOR . 'script' . DIRECTORY_SEPARATOR . 'get_packages.php');

# Check arguments.
if ($argc <= 2) {
    echo "USAGE: $0 <centos6|centos7> <3.4|18.9>\n";
    exit(1);
}
$distrib = $argv[1];
$version = $argv[2];

# Prepare Dockerfile.
$content = file_get_contents(xpath($centreon_build_dir . '/containers/web/' . $version . '/dev.Dockerfile.in'));
$content = str_replace('@DISTRIB@', $distrib, $content);
$dockerfile = xpath($centreon_build_dir . '/containers/web/' . $version . '/dev.' . $distrib . '.Dockerfile');
file_put_contents($dockerfile, $content);

# Get Engine and Broker packages.
// getPackages($distrib, $version);

# Build web image.
xrmdir(xpath($centreon_build_dir . '/containers/centreon'));
xcopy('.', xpath($centreon_build_dir . '/containers/centreon'));
passthru(
    'docker build -t mon-web-' . $version . '-dev:' . $distrib . ' -f ' . $dockerfile . ' ' . xpath($centreon_build_dir . '/containers'),
    $return
);

exit($return);