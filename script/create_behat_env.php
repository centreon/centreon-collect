<?php

// Parse the options.
$opts = getopt('p:a:s:d::f::');
if (!isset($opts["p"]) || !isset($opts["s"]) || !isset($opts["a"])) {
  echo "usage: create_behat_env [-d a_docker_machine] -f[feature_file] -p project_name -a centos6|centos7 -s source_directory\n";
  return (0);
}

// Create tmp directory.
$tmp_directory = sys_get_temp_dir() . '/behat_docker';
mkdir($tmp_directory);

// Build and execute behat container.
copy('./centreon-build/containers/behat/behat.Dockerfile', "$tmp_directory/behat.Dockerfile");
exec("docker build -t behat -f $tmp_directory/behat.Dockerfile $tmp_directory");
exec("docker run -v /var/run/docker.sock:/var/run/docker.sock -ti -d behat", $output, $return_var);
if ($return_var != 0) {
  echo "couldn't launch behat: " . implode('\n', $output) . "\n";
  return (-1);
}
$id = $output[0];

// Copy files
exec("docker cp ./centreon-build $id:/tmp/");
exec("docker cp " . $opts['s'] . " $id:/tmp/");

// Launch acceptance script.
array_shift($argv);
passthru("docker exec $id sh -c 'cd /tmp/; php /tmp/centreon-build/script/launch_acceptance_test.php " . implode(' ', $argv) . "'");

exec("docker stop $id");
?>
