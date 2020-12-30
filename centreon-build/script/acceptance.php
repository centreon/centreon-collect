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
        $name = 'registry.centreon.com/' . $base;
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
function replace_in_file($in, $out, $toReplace)
{
    $content = file_get_contents($in);
    foreach ($toReplace as $from => $to) {
        $content = str_replace($from, $to, $content);
    }

    $opts = getopt('v:', ['api']);
    $yaml = yaml_parse($content);
    if (isset($opts['api'])) {
        unset($yaml['services']['webdriver']);
    }
    file_put_contents($out, yaml_emit($yaml));
    return true;
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
$opts = getopt('cd:ghsv:i:t:', ['api']);
array_shift($argv);
if (isset($opts['h'])) {
    echo "USAGE: acceptance.php [-h] ";
    echo "[--api] [-g] [-s] [-c] [-v version] [-d distrib] [feature1 [feature2 [...] ] ]\n";
    echo "\n";
    echo "  Description:\n";
    echo "    Feature files are optional. By default all of them will be run.\n";
    echo "    Log files and screenshots will be saved in system temporary\n";
    echo "    directory in case of an error in a scenario.\n";
    echo "\n";
    echo "  Arguments:\n";
    echo "    -h     Print this help.\n";
    echo "    --api  API tests (end to end tests if not set).\n";
    echo "    -c     Use images from the continuous integration instead of locally generated images.\n";
    echo "    -v     Use precise version (can be use with -c for CI).\n";
    echo "    -d     Distribution used to run tests. Default is centos7.\n";
    echo "    -g     Only generate files and images. Do not run tests.\n";
    echo "    -t     Filter features or scenarios by tags.\n";
    echo "    -s     Synchronize with registry. Pull all images from ci.int.centreon.com registry.\n";
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

$apiTest = false;
if (isset($opts['api'])) {
    $apiTest = true;
    array_shift($argv);
}

if (isset($opts['v'])) {
    $version = $opts['v'];
    array_shift($argv);
    array_shift($argv);
} else {
    $version = '21.04';
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
    $distrib = 'centos7';
}
if (isset($opts['g'])) {
    $only_generate = true;
    array_shift($argv);
} else {
    $only_generate = false;
}
if (isset($opts['t'])) {
    $tags = $opts['t'];
    array_shift($argv);
    array_shift($argv);
} else {
    $tags = null;
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
        '/mon-middleware' => array(),
        '/hub' => array(),
        '/mon-mediawiki-3.4' => array(),
        '/mon-mediawiki-19.04' => array(),
        '/mon-mediawiki-19.10' => array(),
        '/mon-mediawiki-20.04' => array(),
        '/mon-mediawiki-20.10' => array(),
        '/mon-mediawiki-21.04' => array(),
        '/mon-openldap' => array(),
        '/mon-squid-simple' => array(),
        '/mon-squid-basic-auth' => array(),
        'influxdb' => array(),
        'selenium/hub' => array(),
        'selenium/node-chrome' => array(),
        'redis' => array(),
        '/mon-lm' => array(
            'version' => array('3.4', '19.04', '19.10', '20.04', '20.10', '21.04')
        ),
        '/mon-poller-display-central' => array(
            'version' => array('3.4')
        ),
        '/mon-poller-display' => array(
            'version' => array('3.4')
        ),
        '/mon-ppe' => array(
            'version' => array('3.4', '19.04', '19.10', '20.04', '20.10', '21.04')
        ),
        '/mon-ppm' => array(
            'version' => array('3.4', '19.04', '19.10', '20.04', '20.10', '21.04')
        ),
        '/mon-awie' => array(
            'version' => array('3.4', '19.04', '19.10', '20.04', '20.10', '21.04')
        ),
        '/mon-ppm-autodisco' => array(
            'version' => array('3.4', '19.04', '19.10', '20.04', '20.10', '21.04')
        ),
        '/mon-ppm1' => array(
            'distribution' => array('centos7')
        ),
        '/mon-web-fresh' => array(
            'version' => array('3.4', '19.04', '19.10', '20.04', '20.10', '21.04')
        ),
        '/mon-web' => array(
            'version' => array('3.4', '19.04', '19.10', '20.04', '20.10', '21.04')
        ),
        '/mon-web-widgets' => array(
            'version' => array('3.4', '19.04', '19.10', '20.04', '20.10', '21.04')
        ),
        '/mon-web-stable' => array(
            'version' => array('3.4')
        ),
        '/des-bam' => array(
            'version' => array('3.4', '19.04', '19.10', '20.04', '20.10', '21.04')
        ),
        '/des-map-server' => array(
            'version' => array('3.4', '19.04', '19.10', '20.04', '20.10', '21.04')
        ),
        '/des-map-web' => array(
            'version' => array('3.4', '19.04', '19.10', '20.04', '20.10', '21.04')
        ),
        '/des-mbi-server' => array(
            'version' => array('3.4', '19.04', '19.10', '20.04', '20.10', '21.04')
        ),
        '/des-mbi-web' => array(
            'version' => array('3.4', '19.04', '19.10', '20.04', '20.10', '21.04')
        )
    );

    $finalImages = array();
    foreach ($images as $image => $parameters) {
        if ($image[0] == '/') {
            $image = 'registry.centreon.com' . $image;
        }
        if (isset($parameters['version'])) {
            foreach ($parameters['version'] as $version) {
                if ($version == '3.4') {
                    $distributions = array('centos7');
                } else if ($version == '20.10' || $version == '21.04') {
                    $distributions = array('centos7', 'centos8');
                } else {
                    $distributions = array('centos7');
                }
                foreach ($distributions as $distribution) {
                    $finalImages[] = $image . '-' . $version . ':' . $distribution;
                }
            }
        } else if (isset($parameters['distribution'])) {
            foreach ($parameters['distribution'] as $distribution) {
                $finalImages[] = $image . ':' . $distribution;
            }
        } else {
            $finalImages[] = $image . ':latest';
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
    case 'centreon':
    case 'centreon-web':
        $project = 'web';
        break ;
    case 'centreon-bam':
        $project = 'bam';
        break ;
    case 'centreon-awie':
        $project = 'awie';
        break ;
    case 'centreon-map':
        $project = 'map';
        break ;
    case 'centreon-hub-ui':
        $project = 'hub';
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
            '@MIDDLEWARE_IMAGE@' => 'registry.centreon.com/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/middleware/docker-compose-web.yml.in'),
        xpath('hub-dev.yml'),
        array(
            '@WEB_IMAGE@' => 'hub-dev:latest',
            '@MIDDLEWARE_IMAGE@' => 'registry.centreon.com/mon-middleware-dataset:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/mediawiki/19.04/docker-compose.yml.in'),
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
            '@MIDDLEWARE_IMAGE@' => 'registry.centreon.com/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/squid/simple/docker-compose-middleware.yml.in'),
        xpath('mon-lm-squid-simple-dev.yml'),
        array(
            '@WEB_IMAGE@' => build_image_name('mon-lm'),
            '@MIDDLEWARE_IMAGE@' => 'registry.centreon.com/mon-middleware:latest'
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
            '@MIDDLEWARE_IMAGE@' => 'registry.centreon.com/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/squid/basic-auth/docker-compose-middleware.yml.in'),
        xpath('mon-lm-squid-basic-auth-dev.yml'),
        array(
            '@WEB_IMAGE@' => build_image_name('mon-lm'),
            '@MIDDLEWARE_IMAGE@' => 'registry.centreon.com/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/19.04/docker-compose-influxdb.yml.in'),
        xpath('mon-web-influxdb-dev.yml'),
        array('@WEB_IMAGE@' => build_image_name('mon-web'))
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/middleware/docker-compose-standalone.yml.in'),
        xpath('mon-middleware-dev.yml'),
        array('@MIDDLEWARE_IMAGE@' => ($ci ? 'registry.centreon.com/mon-middleware:latest' : 'mon-middleware-dev:latest'))
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/poller-display/3.4/docker-compose.yml.in'),
        xpath('mon-poller-display-dev.yml'),
        array(
            '@WEB_IMAGE@' => build_image_name('mon-poller-display-central'),
            '@POLLER_IMAGE@' => build_image_name('mon-poller-display')
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/19.04/docker-compose.yml.in'),
        xpath('mon-ppe-dev.yml'),
        array('@WEB_IMAGE@' => build_image_name('mon-ppe'))
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/19.04/docker-compose.yml.in'),
        xpath('mon-ppm-dev.yml'),
        array(
            '@WEB_IMAGE@' => build_image_name('mon-ppm'),
            '@MIDDLEWARE_IMAGE@' => 'registry.centreon.com/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/19.04/docker-compose.yml.in'),
        xpath('mon-ppm-autodisco-dev.yml'),
        array(
            '@WEB_IMAGE@' => build_image_name('mon-ppm-autodisco'),
            '@MIDDLEWARE_IMAGE@' => 'registry.centreon.com/mon-middleware:latest'
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/19.04/docker-compose.yml.in'),
        xpath('mon-awie-dev.yml'),
        array('@WEB_IMAGE@' => build_image_name('mon-awie'))
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/19.04/docker-compose.yml.in'),
        xpath('mon-ppm1-dev.yml'),
        array('@WEB_IMAGE@' => 'registry.centreon.com/mon-ppm1:' . $distrib)
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/19.04/docker-compose.yml.in'),
        xpath('mon-web-dev.yml'),
        array('@WEB_IMAGE@' => build_image_name('mon-web'))
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/19.04/docker-compose.yml.in'),
        xpath('mon-web-fresh-dev.yml'),
        array('@WEB_IMAGE@' => 'registry.centreon.com/mon-web-fresh-' . $version . ':' . $distrib)
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/19.04/docker-compose.yml.in'),
        xpath('mon-web-widgets-dev.yml'),
        array('@WEB_IMAGE@' => build_image_name('mon-web-widgets'))
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/web/19.04/docker-compose.yml.in'),
        xpath('des-bam-dev.yml'),
        array('@WEB_IMAGE@' => build_image_name('des-bam'))
    );
    replace_in_file(
        xpath(
            $centreon_build_dir . '/containers/map/' .
            ($ci ? 'docker-compose.yml.in' : 'docker-compose-dev.yml.in')
        ),
        xpath('des-map-dev.yml'),
        array(
            '@MAP_IMAGE@' => 'registry.centreon.com/des-map-server-' . $version . ':' . $distrib,
            '@WEB_IMAGE@' => 'registry.centreon.com/des-map-web-' . $version . ':' . $distrib,
            '@SOURCE_DIR@' => xpath(realpath('web/build'))
        )
    );
    replace_in_file(
        xpath($centreon_build_dir . '/containers/mbi/19.04/docker-compose.yml.in'),
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
        $cmdAttr = '';

        if ($apiTest === true) {
            $cmdAttr .= " --config tests/api/behat.yml";
        }

        if ($tags !== null) {
            $cmdAttr .= " --tags '{$tags}'";
        }

        $cmd = xpath("./vendor/bin/behat --strict --no-colors --no-interaction{$cmdAttr}");
        if (empty($argv)) {
            $argv[] = '';
        }

        foreach ($argv as $feature) {
            passthru($cmd . ' ' . $feature, $return_var);
        }
    }
}
