<?php

// Load Centreon Build library.
$centreon_build_dir = dirname(__FILE__) . DIRECTORY_SEPARATOR . '..';
require_once($centreon_build_dir . DIRECTORY_SEPARATOR . 'script' . DIRECTORY_SEPARATOR . 'common.php');
// require_once($centreon_build_dir . DIRECTORY_SEPARATOR . 'script' . DIRECTORY_SEPARATOR . 'get_packages.php');

# Check arguments.
if ($argc <= 2) {
    echo "USAGE: $0 <centos7> <3.4|18.10>\n";
    exit(1);
}
$distrib = $argv[1];
$version = $argv[2];

# Prepare Dockerfile.
$content = file_get_contents(xpath($centreon_build_dir . '/containers/bam/' . $version . '/dev.Dockerfile.in'));
$content = str_replace('@DISTRIB@', $distrib, $content);
$dockerfile = xpath($centreon_build_dir . '/containers/bam/bam-' . $version . '-dev.' . $distrib . '.Dockerfile');
file_put_contents($dockerfile, $content);

# Get Engine and Broker packages.
// getPackages($distrib, $version);

# Build BAM image.
xrmdir(xpath($centreon_build_dir . '/containers/centreon-bam-server'));
xcopy('.', xpath($centreon_build_dir . '/containers/centreon-bam-server'));
passthru('docker build -t des-bam-' . $version . '-dev:' . $distrib . ' -f ' . $dockerfile . ' ' . xpath($centreon_build_dir . '/containers'));
