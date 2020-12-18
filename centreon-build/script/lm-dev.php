<?php

// Load Centreon Build library.
$centreon_build_dir = dirname(__FILE__) . DIRECTORY_SEPARATOR . '..';
require_once($centreon_build_dir . DIRECTORY_SEPARATOR . 'script' . DIRECTORY_SEPARATOR . 'common.php');

# Check arguments.
if ($argc <= 2) {
    echo "USAGE: $0 <centos7> <3.4|18.10>\n";
    exit(1);
}
$distrib = $argv[1];
$version = $argv[2];

# Prepare Dockerfile.
$content = file_get_contents(xpath($centreon_build_dir . '/containers/lm/' . $version . '/lm-dev.Dockerfile.in'));
$content = str_replace('@DISTRIB@', $distrib, $content);
$dockerfile = xpath($centreon_build_dir . '/containers/lm/lm-' . $version . '-dev.' . $distrib . '.Dockerfile');
file_put_contents($dockerfile, $content);

# Build middleware image.
xrmdir(xpath($centreon_build_dir . '/containers/centreon-license-manager'));
xcopy(xpath('www/modules/centreon-license-manager'), xpath($centreon_build_dir . '/containers/centreon-license-manager'));
passthru('docker build -t mon-lm-' . $version . '-dev:' . $distrib . ' -f ' . $dockerfile . ' ' . xpath($centreon_build_dir . '/containers'));
