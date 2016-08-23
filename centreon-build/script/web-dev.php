<?php

// Load Centreon Build library.
$centreon_build_dir = dirname(__FILE__) . DIRECTORY_SEPARATOR . '..';
require_once($centreon_build_dir . DIRECTORY_SEPARATOR . 'script' . DIRECTORY_SEPARATOR . 'common.php');
require_once($centreon_build_dir . DIRECTORY_SEPARATOR . 'script' . DIRECTORY_SEPARATOR . 'get_packages.php');

# Check arguments.
if ($argc <= 1) {
    echo "USAGE: $0 <centos6|centos7>\n";
    exit(1);
}
$distrib = $argv[1];

# Prepare fresh Dockerfile.
$content = file_get_contents(xpath($centreon_build_dir . '/containers/web/fresh-dev.Dockerfile.in'));
$content = str_replace('@DISTRIB@', $distrib, $content);
$dockerfile_fresh = xpath($centreon_build_dir . '/containers/web/fresh-dev.' . $distrib . '.Dockerfile');
file_put_contents($dockerfile_fresh, $content);

# Prepare standard Dockerfile.
$content = file_get_contents(xpath($centreon_build_dir . '/containers/web/dev.Dockerfile.in'));
$content = str_replace('@DISTRIB@', $distrib, $content);
$dockerfile = xpath($centreon_build_dir . '/containers/web/dev.' . $distrib . '.Dockerfile');
file_put_contents($dockerfile, $content);

# Get Engine and Broker packages.
getPackages($distrib);

# Copy local sources.
xrmdir(xpath($centreon_build_dir . '/containers/centreon'));
xcopy('.', xpath($centreon_build_dir . '/containers/centreon'));

# Build web fresh image.
passthru('docker build -t mon-web-fresh-dev:' . $distrib . ' -f ' . $dockerfile_fresh . ' ' . xpath($centreon_build_dir . '/containers'));

# Build web image.
passthru('docker build -t mon-web-dev:' . $distrib . ' -f ' . $dockerfile . ' ' . xpath($centreon_build_dir . '/containers'));

?>
