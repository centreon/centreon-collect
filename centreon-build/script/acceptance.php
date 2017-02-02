<?php

// Load Centreon Build library.
$centreon_build_dir = dirname(__FILE__) . DIRECTORY_SEPARATOR . '..';
require_once($centreon_build_dir . DIRECTORY_SEPARATOR . 'script' . DIRECTORY_SEPARATOR . 'common.php');

// Replace all the elements of a file
function replace_in_file($in, $out, $to_replace) {
  $str = file_get_contents($in);
  foreach ($to_replace as $from => $to) {
    $str = str_replace($from, $to, $str);
  }
  file_put_contents($out, $str);
  return (true);
}

function call_exit(int $signo)
{
    exit(1);
}

// Set signal handlers.
if (function_exists('pcntl_signal')) {
    pcntl_signal(SIGTERM, 'call_exit');
    pcntl_signal(SIGINT, 'call_exit');
}

// Parse the options.
$opts = getopt("cd:ghs");
array_shift($argv);
if (isset($opts['h'])) {
    echo "USAGE: acceptance.php [-h] [-g] [-s] [-c] [-d distrib] [feature1 [feature2 [...] ] ]\n";
    echo "\n";
    echo "  Description:\n";
    echo "    Feature files are optional. By default all of them will be run.\n";
    echo "    Log files and screenshots will be saved in system temporary\n";
    echo "    directory in case of an error in a scenario.\n";
    echo "\n";
    echo "  Arguments:\n";
    echo "    -h  Print this help.\n";
    echo "    -c  Use images from the continuous integration instead of locally generated images.\n";
    echo "    -d  Distribution used to run tests. Can be one of centos6 (default) or centos7.\n";
    echo "    -g  Only generate files and images. Do not run tests.\n";
    echo "    -s  Synchronize with registry. Pull all images from ci.int.centreon.com registry.\n";
    echo "\n";
    echo "  Prerequisites:\n";
    echo "    - *Docker* (connected to Docker Machine on Windows or MacOS)\n";
    echo "    - *Docker Compose*\n";
    echo "    - *PHP*\n";
    echo "    - *PDO* extension for PHP\n";
    echo "    - *PDO MySQL* extension for PHP\n";
    echo "    - *composer*\n";
    echo "    - the following hosts must be resolved to the corresponding IP addresses:\n";
    echo "        crm.int.centreon.com 10.24.11.73\n";
    echo "        support.centreon.com 10.30.2.62\n";
    return (0);
}
if (isset($opts['c'])) {
    $ci = true;
    array_shift($argv);
} else {
    $ci = false;
}
if (isset($opts['d'])) {
    $distrib = $opts['d'];
    array_shift($argv);
    array_shift($argv);
} else {
    $distrib = 'centos6';
}
if (isset($opts['g'])) {
    $only_generate = true;
    array_shift($argv);
} else {
    $only_generate = false;
}
if (isset($opts['s'])) {
    $synchronize = true;
    array_shift($argv);
} else {
    $synchronize = false;
}
$source_dir = realpath('.');

//
// SYNCHRONIZE IMAGES
//
if ($synchronize) {
    $images = array(
        '/mon-phantomjs:latest',
        '/mon-lm:centos6',
        '/mon-lm:centos7',
        '/mon-middleware:latest',
        '/mon-mediawiki:latest',
        '/mon-openldap:latest',
        '/mon-squid-simple:latest',
        '/mon-squid-basic-auth:latest',
        '/mon-ppe:centos6',
        '/mon-ppe:centos7',
        '/mon-ppe1:centos6',
        '/mon-ppe1:centos7',
        '/mon-ppm:centos6',
        '/mon-ppm:centos7',
        '/mon-ppm1:centos6',
        '/mon-ppm1:centos7',
        '/mon-automation:centos6',
        '/mon-automation:centos7',
        '/mon-web-fresh:centos6',
        '/mon-web-fresh:centos7',
        '/mon-web:centos6',
        '/mon-web:centos7',
        '/mon-web-stable:centos6',
        '/mon-web-stable:centos7',
        '/des-bam:centos6',
        '/des-bam:centos7',
        '/des-bam-server:centos6',
        '/des-bam-server:centos7',
        '/des-map-web:centos6',
        '/des-map-web:centos7',
        'redis:latest'
    );
    $count = count($images);
    $i = 0;
    foreach ($images as $image) {
        ++$i;
        echo '[' . $i . '/' . $count . '] Pulling ' . $image . "\n";
        if ($image[0] == '/') {
            $image = 'ci.int.centreon.com:5000' . $image;
        }
        passthru('docker pull ' . $image);
    }
}

//
// RUN TESTS
//
else {
    // Load configuration file.
    echo "[1/4] Loading configuration...\n";
    require_once(xpath($centreon_build_dir . '/conf/acceptance.conf.php'));
    if (!defined('_GITHUB_TOKEN_') || _GITHUB_TOKEN_ == "") {
        echo "Please fill your GitHub token in acceptance.conf.php file.\n";
        return (1);
    }
    $project = basename($source_dir);
    switch ($project) {
    case 'centreon-license-manager':
    case 'centreon-lm':
        $project = 'lm';
        break ;
    case 'centreon-imp-portal-api':
    case 'centreon-middleware':
        $project = 'middleware';
        break ;
    case 'centreon-export':
    case 'centreon-ppe':
        $project = 'ppe';
        break ;
    case 'centreon-import':
    case 'centreon-pp-manager':
        $project = 'ppm';
        break ;
    case 'centreon-automation':
        $project = 'automation';
        break ;
    case 'centreon':
    case 'centreon-web':
        $project = 'web';
        break ;
    case 'centreon-bam':
        $project = 'bam';
        break ;
    case 'centreon-studio-server':
    case 'centreon-studio-desktop-client':
        $project = 'map';
        break ;
    default:
        echo 'Unknown project ' . $project . ": perhaps you are not running acceptance.php from the root of a supported project ?\n";
        return (1);
    };

    // Replace the compose .yml.in.
    echo "[2/4] Preparing for execution...\n";
    replace_in_file(
        xpath($centreon_build_dir . '/containers/middleware/docker-compose-web.yml.in'),
        xpath('mon-lm-dev.yml'),
        array(
            '@WEB_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/mon-lm:' : 'mon-lm-dev:') . $distrib,
            '@MIDDLEWARE_IMAGE@' => 'ci.int.centreon.com:5000/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/mediawiki/docker-compose.yml.in'),
        xpath('mon-web-kb-dev.yml'),
        array(
            '@WEB_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/mon-web:' : 'mon-web-dev:') . $distrib,
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/openldap/docker-compose.yml.in'),
        xpath('mon-web-openldap-dev.yml'),
        array(
            '@WEB_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/mon-web:' : 'mon-web-dev:') . $distrib,
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/squid/simple/docker-compose.yml.in'),
        xpath('mon-web-squid-simple-dev.yml'),
        array(
            '@WEB_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/mon-web:' : 'mon-web-dev:') . $distrib,
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/squid/simple/docker-compose-middleware.yml.in'),
        xpath('mon-ppm-squid-simple-dev.yml'),
        array(
            '@WEB_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/mon-ppm:' : 'mon-ppm-dev:') . $distrib,
            '@MIDDLEWARE_IMAGE@' => 'ci.int.centreon.com:5000/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/squid/simple/docker-compose-middleware.yml.in'),
        xpath('mon-lm-squid-simple-dev.yml'),
        array(
            '@WEB_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/mon-lm:' : 'mon-lm-dev:') . $distrib,
            '@MIDDLEWARE_IMAGE@' => 'ci.int.centreon.com:5000/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/squid/basic-auth/docker-compose.yml.in'),
        xpath('mon-web-squid-basic-auth-dev.yml'),
        array(
            '@WEB_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/mon-web:' : 'mon-web-dev:') . $distrib,
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/squid/basic-auth/docker-compose-middleware.yml.in'),
        xpath('mon-ppm-squid-basic-auth-dev.yml'),
        array(
            '@WEB_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/mon-ppm:' : 'mon-ppm-dev:') . $distrib,
            '@MIDDLEWARE_IMAGE@' => 'ci.int.centreon.com:5000/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/squid/basic-auth/docker-compose-middleware.yml.in'),
        xpath('mon-lm-squid-basic-auth-dev.yml'),
        array(
            '@WEB_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/mon-lm:' : 'mon-lm-dev:') . $distrib,
            '@MIDDLEWARE_IMAGE@' => 'ci.int.centreon.com:5000/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/docker-compose-influxdb.yml.in'),
        xpath('mon-web-influxdb.yml'),
        array(
            '@WEB_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/mon-web:' : 'mon-web-dev:') . $distrib,
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/middleware/docker-compose-standalone.yml.in'),
        xpath('mon-middleware-dev.yml'),
        array('@MIDDLEWARE_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/mon-middleware:latest' : 'mon-middleware-dev:latest'))
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/docker-compose.yml.in'),
        xpath('mon-ppe-dev.yml'),
        array('@WEB_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/mon-ppe:' : 'mon-ppe-dev:') . $distrib)
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/docker-compose.yml.in'),
        xpath('mon-ppe1-dev.yml'),
        array('@WEB_IMAGE@' => 'ci.int.centreon.com:5000/mon-ppe1:' . $distrib)
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/docker-compose.yml.in'),
        xpath('mon-ppm-dev.yml'),
        array(
            '@WEB_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/mon-ppm:' : 'mon-ppm-dev:') . $distrib,
            '@MIDDLEWARE_IMAGE@' => 'ci.int.centreon.com:5000/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/docker-compose.yml.in'),
        xpath('mon-ppm1-dev.yml'),
        array('@WEB_IMAGE@' => 'ci.int.centreon.com:5000/mon-ppm1:' . $distrib)
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/docker-compose.yml.in'),
        xpath('mon-automation-dev.yml'),
        array(
            '@WEB_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/mon-automation:' : 'mon-automation-dev:') . $distrib,
            '@MIDDLEWARE_IMAGE@' => 'ci.int.centreon.com:5000/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/docker-compose.yml.in'),
        xpath('mon-web-dev.yml'),
        array('@WEB_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/mon-web:' : 'mon-web-dev:') . $distrib)
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/docker-compose.yml.in'),
        xpath('mon-web-fresh-dev.yml'),
        array('@WEB_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/mon-web-fresh:' : 'mon-web-fresh-dev:') . $distrib)
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/docker-compose.yml.in'),
        xpath('des-bam-dev.yml'),
        array('@WEB_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/des-bam:' : 'des-bam-dev:') . $distrib)
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/map/docker-compose.yml.in'),
        xpath('des-map-dev.yml'),
        array(
            '@MAP_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/des-map-server:' : 'des-map-server:') . $distrib,
            '@WEB_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/des-map-web:' : 'des-map-web:') . $distrib
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/mbi/docker-compose.yml.in'),
        xpath('des-mbi-dev.yml'),
        array(
            '@WEB_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/des-mbi-web:' : 'des-mbi-web:') . $distrib
        )
    );

    // Execute the dev container script.
    echo "[3/4] Building development container from current sources...\n";
    if ($ci) {
        echo "Continuous integration mode (-c), step not needed\n";
    } else {
        passthru('php ' . xpath($centreon_build_dir . '/script/' . $project . '-dev.php') . ' ' . $distrib, $return_var);
        if ($return_var != 0) {
            echo 'Could not build development container of ' . $project . "\n";
            return (1);
        }
    }

    // Start acceptance tests.
    echo "[4/4] Finally running acceptance tests...\n";
    if ($only_generate) {
        echo "Image generation only mode (-g), step not needed\n";
    } else {
        $cmd = xpath("./vendor/bin/behat --strict");
        if (empty($argv)) {
            $argv[] = '';
        }
        foreach ($argv as $feature) {
            passthru($cmd . ' ' . $feature, $return_var);
        }
    }
}
