<?php

// Load Centreon Build library.
$centreon_build_dir = dirname(__FILE__) . DIRECTORY_SEPARATOR . '..';
require_once($centreon_build_dir . DIRECTORY_SEPARATOR . 'script' . DIRECTORY_SEPARATOR . 'common.php');

# Check arguments.
if ($argc <= 1) {
    echo "USAGE: $0 <centos6|centos7>\n";
    exit(1);
}
$distrib = $argv[1];

# Prepare poller Dockerfile.
$content = file_get_contents(xpath($centreon_build_dir . '/containers/poller-display/poller-dev.Dockerfile.in'));
$content = str_replace('@DISTRIB@', $distrib, $content);
$dockerfile = xpath($centreon_build_dir . '/containers/poller-display/poller-dev.' . $distrib . '.Dockerfile');
file_put_contents($dockerfile, $content);

# Build poller image.
xrmdir(xpath($centreon_build_dir . '/containers/centreon-poller-display'));
xcopy(xpath('www/modules/centreon-poller-display'), xpath($centreon_build_dir . '/containers/centreon-poller-display'));
passthru('docker build -t mon-poller-display-dev:' . $distrib . ' -f ' . $dockerfile . ' ' . xpath($centreon_build_dir . '/containers'));

# Prepare central Dockerfile.
$content = file_get_contents(xpath($centreon_build_dir . '/containers/poller-display/central-dev.Dockerfile.in'));
$content = str_replace('@DISTRIB@', $distrib, $content);
$dockerfile = xpath($centreon_build_dir . '/containers/poller-display/central-dev.' . $distrib . '.Dockerfile');
file_put_contents($dockerfile, $content);

# Build central image.
xrmdir(xpath($centreon_build_dir . '/containers/centreon-poller-display-central'));
xcopy(xpath('www/modules/centreon-poller-display-central'), xpath($centreon_build_dir . '/containers/centreon-poller-display-central'));
passthru('docker build -t mon-poller-display-central-dev:' . $distrib . ' -f ' . $dockerfile . ' ' . xpath($centreon_build_dir . '/containers'));
