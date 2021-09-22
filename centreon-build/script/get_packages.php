<?php

$expectedDirectories = array(
    'broker',
    'engine'
);

$expectedFiles = array(
    'centreon-broker-3',
    'centreon-broker-cbd-3',
    'centreon-broker-cbmod-3',
    'centreon-broker-core-3',
    'centreon-broker-storage-3',
    'centreon-broker-influxdb-3',
    'centreon-broker-graphite-3',
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
function getIfNotExists($filename, $newFile, $url) {
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
 * @param $distrib  Distribution name. Can be only be 'centos7'.
 * @param $version  Centreon version (3.4 or 18.10)
 * @throws Exception
 */
function getPackages($distrib, $version) {
    global $expectedDirectories, $expectedFiles;
    echo "Searching latest Engine and Broker packages...\n";
    if ($distrib == 'centos7') {
        $url = 'http://srvi-repo.int.centreon.com/yum/internal/' . $version . '/el7/x86_64/';
    } else {
        throw new \Exception('Unknown distribution ' . $distrib);
    }
    if (!is_dir($distrib)) {
        mkdir($distrib);
    }

    chdir($distrib);

    foreach ($expectedDirectories as $expectedDirectory) {
        $html = @file_get_contents($url . '/' . $expectedDirectory);
        if (preg_match_all('/<a href="(.*'. $expectedDirectory . '.+)">[^<]*<\/a>/', $html, $matches)) {
            rsort($matches[1]);
            $tmpUrl = $url . '/' . $expectedDirectory . '/' . $matches[1][0];
            $html = file_get_contents($tmpUrl);
            $count = preg_match_all('/<a href="([^"]+\.rpm)">[^<]*<\/a>/i', $html, $remoteFiles);
            foreach ($expectedFiles as $expectedFile) {
                $latestPackage = '';
                for ($i = 0; $i < $count; ++$i) {
                    $filename = $remoteFiles[1][$i];
                    if ((substr($filename, 0, strlen($expectedFile)) === $expectedFile)
                        && (strcmp($latestPackage, $filename) < 0)
                    ) {
                        $latestPackage = $filename;
                    }
                }
                if (!empty($latestPackage)) {
                    getIfNotExists($expectedFile, $latestPackage, $tmpUrl . '/' . $latestPackage);
                }
            }
        }
    }

    chdir('..');
}

?>
