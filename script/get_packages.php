<?php

$expectedFiles = array(
    'centreon-broker-3',
    'centreon-broker-cbd-3',
    'centreon-broker-cbmod-3',
    'centreon-broker-core-3',
    'centreon-broker-storage-3',
    'centreon-clib-1',
    'centreon-engine-1',
    'centreon-engine-daemon-1',
    'centreon-engine-extcommands-1'
);

/**
 *  Get a file from a url if it does not already exist.
 *
 *  @param $filename  The filename of the file to check for existence.
 *  @param $newFile   The new file.
 *  @param $url       The url of the new file.
 *  @param $distrib   Distribution name.
 */
function getIfNotExists($filename, $newFile, $url, $distrib) {
    $existingFiles = glob("$filename*.rpm");
    foreach ($existingFiles as $existingFile) {
        if ($existingFile != $newFile) {
            echo "Erasing previous package $existingFile...\n";
            unlink($existingFile);
        }
    }
    if (!file_exists($newFile)) {
        echo "Getting new package $newFile...\n";
        file_put_contents($newFile, file_get_contents($url));
    }
}

/**
 *  Get Engine and Broker packages.
 *
 *  @param $distrib  Distribution name. Can be one of 'centos6' or 'centos7'.
 */
function getPackages($distrib) {
    global $expectedFiles;
    echo "Searching latest Engine and Broker packages...\n";
    if ($distrib == 'centos6') {
        $url = 'http://srvi-ces-repository.int.centreon.com/repos/standard/dev/el6/unstable/x86_64/RPMS/';
    }
    else if ($distrib == 'centos7') {
        $url = 'http://srvi-ces-repository.int.centreon.com/repos/standard/dev/el7/unstable/x86_64/RPMS/';
    }
    else {
        throw new \Exception('Unknown distribution ' . $distrib);
    }
    if (!is_dir($distrib)) {
        mkdir($distrib);
    }
    chdir($distrib);
    $html = file_get_contents($url);
    $count = preg_match_all('/<a href="([^"]+)">[^<]*<\/a>/i', $html, $remoteFiles);
    foreach ($expectedFiles as $expectedFile) {
        $latestPackage = '';
        for ($i = 0; $i < $count; ++$i) {
            $filename = $remoteFiles[1][$i];
            if ((substr($filename, 0, strlen($expectedFile)) === $expectedFile)
                && (strcmp($latestPackage, $filename) < 0)) {
                $latestPackage = $filename;
            }
        }
        getIfNotExists($expectedFile, $latestPackage, $url . $latestPackage, $distrib);
    }
    chdir('..');
}

?>
