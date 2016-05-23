<?php

// Copy directory recursively
function xcopy($source, $dest) {
    if (defined(PHP_WINDOWS_VERSION_MAJOR)) {
        $cmd = "xcopy /E '$source' '$dest'";
    }
    else {
        $cmd = "cp -r '$source' '$dest'";
    }
    exec($cmd, $output, $return);
    if ($return == 0)
        return (true);
    else
        return (false);
}

// Replace slashes with platform-specific directory separator.
function xpath($path) {
    return str_replace('/', DIRECTORY_SEPARATOR, $path);
}

// Remove directory recursively.
function xrmdir($dir) {
    if (is_dir($dir)) {
        $objects = scandir($dir);
        foreach ($objects as $object) {
            if ($object != "." && $object != "..") {
                if (is_dir($dir."/".$object))
                    xrmdir($dir."/".$object);
                else
                    unlink($dir."/".$object);
            }
        }
        rmdir($dir);
    }
}

# Check arguments.
$centreon_build_dir = xpath(dirname(__FILE__) . '/../..');
if ($argc <= 1) {
    echo "USAGE: $0 <centos6|centos7>\n";
    exit(1);
}
$distrib = $argv[1];

# Prepare Dockerfile.
$content = file_get_contents(xpath($centreon_build_dir . '/containers/ppe/ppe-dev.Dockerfile.in'));
$content = str_replace('@DISTRIB@', $distrib, $content);
$dockerfile = xpath($centreon_build_dir . '/containers/ppe/ppe-dev.' . $distrib . '.Dockerfile');
file_put_contents($dockerfile, $content);

# Build middleware image.
xrmdir(xpath($centreon_build_dir . '/containers/centreon-export'));
xcopy(xpath('www/modules/centreon-export'), xpath($centreon_build_dir . '/containers/centreon-export'));
passthru('docker build -t mon-ppe-dev:' . $distrib . ' -f ' . $dockerfile . ' ' . xpath($centreon_build_dir . '/containers'));

?>
