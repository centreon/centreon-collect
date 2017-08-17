<?php

// Load Centreon Build library.
$centreon_build_dir = dirname(__FILE__) . DIRECTORY_SEPARATOR . '..';
require_once($centreon_build_dir . DIRECTORY_SEPARATOR . 'script' . DIRECTORY_SEPARATOR . 'common.php');

// Build an image name.
function build_image_name($base) {
    // Global variables.
    global $ci;
    global $distrib;
    global $version;

    if ($ci) {
        $name = 'ci.int.centreon.com:5000/' . $base;
        if (!empty($version)) {
            $name .= '-' . $version;
        }
        $name .= ':' . $distrib;
    } elseif (!empty($version)) {
        $name = $base . '-' . $version . '-dev:' . $distrib;
    } else {
        $name = $base . '-dev:' . $distrib;
    }
    return $name;
}

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
$opts = getopt("cd:ghsv:i:");
array_shift($argv);
if (isset($opts['h'])) {
    echo "USAGE: acceptance.php [-h] [-g] [-s] [-c] [-v version] [-d distrib] [feature1 [feature2 [...] ] ]\n";
    echo "\n";
    echo "  Description:\n";
    echo "    Feature files are optional. By default all of them will be run.\n";
    echo "    Log files and screenshots will be saved in system temporary\n";
    echo "    directory in case of an error in a scenario.\n";
    echo "\n";
    echo "  Arguments:\n";
    echo "    -h  Print this help.\n";
    echo "    -c  Use images from the continuous integration instead of locally generated images.\n";
    echo "    -v  Use precise version (can be use with -c for CI).\n";
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


if (isset($opts['v'])) {
    $version = $opts['v'];
    array_shift($argv);
    array_shift($argv);
} else {
    $version = '3.5';
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
        '/mon-phantomjs' => array(),
        '/mon-middleware' => array(),
        '/mon-mediawiki' => array(),
        '/mon-openldap' => array(),
        '/mon-squid-simple' => array(),
        '/mon-squid-basic-auth' => array(),
        'influxdb' => array(),
        'selenium/standalone-chrome' => array(),
        'redis' => array(),
        '/mon-lm' => array(
            'distribution' => array('centos6', 'centos7'),
            'version' => array('3.4', '3.5')
        ),
        '/mon-poller-display-central' => array(
            'distribution' => array('centos6', 'centos7'),
            'version' => array('3.4', '3.5')
        ),
        '/mon-poller-display' => array(
            'distribution' => array('centos6', 'centos7'),
            'version' => array('3.4', '3.5')
        ),
        '/mon-ppe' => array(
            'distribution' => array('centos6', 'centos7'),
            'version' => array('3.4', '3.5')
        ),
        '/mon-ppe1' => array(
            'distribution' => array('centos6', 'centos7')
        ),
        '/mon-ppm' => array(
            'distribution' => array('centos6', 'centos7'),
            'version' => array('3.4', '3.5')
        ),
        '/mon-ppm1' => array(
            'distribution' => array('centos6', 'centos7')
        ),
        '/mon-automation' => array(
            'distribution' => array('centos6', 'centos7'),
            'version' => array('3.4', '3.5')
        ),
        '/mon-web-fresh' => array(
            'distribution' => array('centos6', 'centos7'),
            'version' => array('3.4', '3.5')
        ),
        '/mon-web' => array(
            'distribution' => array('centos6', 'centos7'),
            'version' => array('3.4', '3.5')
        ),
        '/mon-web-widgets' => array(
            'distribution' => array('centos6', 'centos7'),
            'version' => array('3.4', '3.5')
        ),
        '/mon-web-stable' => array(
            'distribution' => array('centos6', 'centos7'),
            'version' => array('3.4', '3.5')
        ),
        '/des-bam' => array(
            'distribution' => array('centos6', 'centos7'),
            'version' => array('3.4', '3.5')
        ),
        '/des-map-server' => array(
            'distribution' => array('centos6', 'centos7'),
            'version' => array('3.4', '3.5')
        ),
        '/des-map-web' => array(
            'distribution' => array('centos6', 'centos7'),
            'version' => array('3.4', '3.5')
        ),
        '/des-mbi-server' => array(
            'distribution' => array('centos6', 'centos7'),
            'version' => array('3.4', '3.5')
        ),
        '/des-mbi-web' => array(
            'distribution' => array('centos6', 'centos7'),
            'version' => array('3.4', '3.5')
        )
    );

    $finalImages = array();
    foreach ($images as $image => $parameters) {
        if ($image[0] == '/') {
            $image = 'ci.int.centreon.com:5000' . $image;
        }
        $tmpImages = array();
        if (isset($parameters['version'])) {
            foreach ($parameters['version'] as $version) {
                $tmpImages[] = $image . '-' . $version;
            }
        } else {
            $tmpImages[] = $image;
        }
        foreach ($tmpImages as $tmpImage) {
            if (isset($parameters['distribution'])) {
                foreach ($parameters['distribution'] as $distribution) {
                    $finalImages[] = $tmpImage . ':' . $distribution;
                }
            } else {
                $finalImages[] = $tmpImage . ':latest';
            }
        }
    }

    $count = count($finalImages);
    $i = 0;
    foreach ($finalImages as $finalImage) {
        $i++;
        echo '[' . $i . '/' . $count . '] Pulling ' . $finalImage . "\n";
        passthru('docker pull ' . $finalImage);
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
    case 'centreon-poller-display':
        $project = 'poller-display';
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
            '@WEB_IMAGE@' => build_image_name('mon-lm'),
            '@MIDDLEWARE_IMAGE@' => 'ci.int.centreon.com:5000/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/mediawiki/docker-compose.yml.in'),
        xpath('mon-web-kb-dev.yml'),
        array('@WEB_IMAGE@' => build_image_name('mon-web'))
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/openldap/docker-compose.yml.in'),
        xpath('mon-web-openldap-dev.yml'),
        array('@WEB_IMAGE@' => build_image_name('mon-web'))
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/squid/simple/docker-compose.yml.in'),
        xpath('mon-web-squid-simple-dev.yml'),
        array('@WEB_IMAGE@' => build_image_name('mon-web'))
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/squid/simple/docker-compose-middleware.yml.in'),
        xpath('mon-ppm-squid-simple-dev.yml'),
        array(
            '@WEB_IMAGE@' => build_image_name('mon-ppm'),
            '@MIDDLEWARE_IMAGE@' => 'ci.int.centreon.com:5000/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/squid/simple/docker-compose-middleware.yml.in'),
        xpath('mon-lm-squid-simple-dev.yml'),
        array(
            '@WEB_IMAGE@' => build_image_name('mon-lm'),
            '@MIDDLEWARE_IMAGE@' => 'ci.int.centreon.com:5000/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/squid/basic-auth/docker-compose.yml.in'),
        xpath('mon-web-squid-basic-auth-dev.yml'),
        array('@WEB_IMAGE@' => build_image_name('mon-web'))
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/squid/basic-auth/docker-compose-middleware.yml.in'),
        xpath('mon-ppm-squid-basic-auth-dev.yml'),
        array(
            '@WEB_IMAGE@' => build_image_name('mon-ppm'),
            '@MIDDLEWARE_IMAGE@' => 'ci.int.centreon.com:5000/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/squid/basic-auth/docker-compose-middleware.yml.in'),
        xpath('mon-lm-squid-basic-auth-dev.yml'),
        array(
            '@WEB_IMAGE@' => build_image_name('mon-lm'),
            '@MIDDLEWARE_IMAGE@' => 'ci.int.centreon.com:5000/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/3.5/docker-compose-influxdb.yml.in'),
        xpath('mon-web-influxdb.yml'),
        array('@WEB_IMAGE@' => build_image_name('mon-web'))
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/middleware/docker-compose-standalone.yml.in'),
        xpath('mon-middleware-dev.yml'),
        array('@MIDDLEWARE_IMAGE@' => ($ci ? 'ci.int.centreon.com:5000/mon-middleware:latest' : 'mon-middleware-dev:latest'))
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/poller-display/3.5/docker-compose.yml.in'),
        xpath('mon-poller-display-dev.yml'),
        array(
            '@WEB_IMAGE@' => build_image_name('mon-poller-display-central'),
            '@POLLER_IMAGE@' => build_image_name('mon-poller-display')
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/3.5/docker-compose.yml.in'),
        xpath('mon-ppe-dev.yml'),
        array('@WEB_IMAGE@' => build_image_name('mon-ppe'))
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/3.5/docker-compose.yml.in'),
        xpath('mon-ppe1-dev.yml'),
        array('@WEB_IMAGE@' => 'ci.int.centreon.com:5000/mon-ppe1:' . $distrib)
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/3.5/docker-compose.yml.in'),
        xpath('mon-ppm-dev.yml'),
        array(
            '@WEB_IMAGE@' => build_image_name('mon-ppm'),
            '@MIDDLEWARE_IMAGE@' => 'ci.int.centreon.com:5000/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/3.5/docker-compose.yml.in'),
        xpath('mon-ppm1-dev.yml'),
        array('@WEB_IMAGE@' => 'ci.int.centreon.com:5000/mon-ppm1:' . $distrib)
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/3.5/docker-compose.yml.in'),
        xpath('mon-automation-dev.yml'),
        array(
            '@WEB_IMAGE@' => build_image_name('mon-automation'),
            '@MIDDLEWARE_IMAGE@' => 'ci.int.centreon.com:5000/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/3.5/docker-compose.yml.in'),
        xpath('mon-web-dev.yml'),
        array('@WEB_IMAGE@' => build_image_name('mon-web'))
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/3.5/docker-compose.yml.in'),
        xpath('mon-web-fresh-dev.yml'),
        array('@WEB_IMAGE@' => 'ci.int.centreon.com:5000/mon-web-fresh:' . $distrib)
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/3.5/docker-compose.yml.in'),
        xpath('mon-web-widgets-dev.yml'),
        array('@WEB_IMAGE@' => build_image_name('mon-web-widgets'))
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/3.5/docker-compose.yml.in'),
        xpath('des-bam-dev.yml'),
        array('@WEB_IMAGE@' => build_image_name('des-bam'))
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/map/docker-compose.yml.in'),
        xpath('des-map-dev.yml'),
        array(
            '@MAP_IMAGE@' => build_image_name('des-map-server'),
            '@WEB_IMAGE@' => build_image_name('des-map-web')
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/mbi/docker-compose.yml.in'),
        xpath('des-mbi-dev.yml'),
        array(
            '@MBI_IMAGE@' => build_image_name('des-mbi-server'),
            '@WEB_IMAGE@' => build_image_name('des-mbi-web')
        )
    );

    // Execute the dev container script.
    echo "[3/4] Building development container from current sources...\n";
    if ($ci) {
        echo "Continuous integration mode (-c), step not needed\n";
    } else {
        passthru(
            'php ' . xpath($centreon_build_dir . '/script/' . $project . '-dev.php') . ' ' .
            $distrib . ' ' . $version, $return_var
        );
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
        $cmd = xpath("./vendor/bin/behat --strict --no-colors --no-interaction");
        if (empty($argv)) {
            $argv[] = '';
        }
        passthru(
            'docker-compose -f ' . $centreon_build_dir . '/containers/webdrivers/docker-compose.yml.in ' .
            '-p webdriver up -d',
            $return_var
        );
        foreach ($argv as $feature) {
            passthru($cmd . ' ' . $feature, $return_var);
        }
        passthru(
            'docker-compose -f ' . $centreon_build_dir . '/containers/webdrivers/docker-compose.yml.in ' .
            '-p webdriver down -v',
            $return_var
        );
    }
}
