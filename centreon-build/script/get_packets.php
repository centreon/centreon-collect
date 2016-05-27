<?php

$expectedFiles = array(
    'centreon-broker-2',
    'centreon-broker-cbd-2',
    'centreon-broker-cbmod-2',
    'centreon-broker-core-2',
    'centreon-broker-storage-2',
    'centreon-clib-1',
    'centreon-engine-1',
    'centreon-engine-bench-1',
    'centreon-engine-daemon-1',
    'centreon-engine-debuginfo-1',
    'centreon-engine-extcommands-1'
);

/**
 *  Get a file from a url if it does not already exist.
 * 
 *  @param type $filename  The filename of the file to check for existence.
 *  @param type $newFile   The new file.
 *  @param type $url       The url of the new file.
 */
function getIfNotExists($filename, $newFile, $url) {
    $existingFiles = glob("$filename*");
    $existingFile = '';
    if (!empty($existingFiles)) {
        $existingFile = $existingFiles[0];
    }
    if (!empty($existingFile)) {
        if ($existingFile != $newFile) {
            echo "found new packet $newFile, erasing $existingFile\n";
            unlink($existingFile);
        }
        else {
            return;
        }
    }
    echo "getting new packet $newFile\n";
    file_put_contents($newFile, file_get_contents($url));
}

/**
 *  Get engine/broker packets.
 */
function getPackets() {
    global $expectedFiles;
    echo "searching for engine/broker packets...\n";
    $url = 'http://srvi-ces-repository.int.centreon.com/repos/standard/3.0/unstable/x86_64/RPMS/';
    $html = file_get_contents($url);
    $count = preg_match_all('/<a href="([^"]+)">[^<]*<\/a>/i', $html, $remoteFiles);
    for ($i = 0; $i < $count; ++$i) {
       $filename = $remoteFiles[1][$i];
       foreach ($expectedFiles as $expectedFile) {
           if (substr($filename, 0, strlen($expectedFile)) === $expectedFile) {
               getIfNotExists($expectedFile, $filename, $url . $filename);
               break ;
           }
       }
   }
}
?>