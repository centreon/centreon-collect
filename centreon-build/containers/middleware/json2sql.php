<?php

// Parse options.
if ($argc < 1) {
    echo "USAGE: json2sql <SQL file>\n";
    return (1);
}
$filename = $argv[1];

// Load content.
$db = new mysqli('localhost', 'root', '', 'imp');
$content = file_get_contents($filename);

// Perform replacements.
$dir = opendir('/etc/centreon/ppm');
while (($file = readdir($dir)) !== FALSE) {
    $json = mysqli_real_escape_string($db, file_get_contents('/etc/centreon/ppm/' . $file));
    $content = str_replace('@' . $file . '@', $json, $content);
}
closedir($dir);

// Print output.
echo $content;

?>
