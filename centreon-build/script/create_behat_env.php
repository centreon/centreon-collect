<?php

// Parse the options.
$opts = getopt('p:a:s:d::f::');
if (!isset($opts["p"]) || !isset($opts["s"]) || !isset($opts["a"])) {
  echo "usage: create_behat_env [-d a_docker_machine] -f[feature_file] -p project_name -a centos6|centos7 -s source_directory\n";
  return (0);
}
$centreon_build_directory = realpath('centreon-build');
$source_directory = realpath($opts['s']);
$source_directory_name = $opts['s'];
// Remove ./ as the docker mount dislikes this.
if (0 === strpos($source_directory_name, './'))
  $source_directory_name = substr($source_directory_name, strlen('./'));

// Create tmp directory.
echo "creating tmp directory...\n";
$tmp_directory = sys_get_temp_dir() . '/behat_docker';
mkdir($tmp_directory);

// Build and execute behat container.
echo "creating behat docker image...\n";
copy('./centreon-build/containers/behat/behat.Dockerfile', "$tmp_directory/behat.Dockerfile");
exec("docker build -t behat -f $tmp_directory/behat.Dockerfile $tmp_directory");
echo "starting bethat docker image...\n";
exec("docker run -v /var/run/docker.sock:/var/run/docker.sock -v $centreon_build_directory:/tmp/centreon-build -v $source_directory:/tmp/$source_directory_name -ti -d behat", $output, $return_var);
if ($return_var != 0) {
  echo "couldn't launch behat: " . implode('\n', $output) . "\n";
  return (-1);
}
$id = $output[0];

// Launch acceptance script.
echo "starting acceptance script...\n";
array_shift($argv);
passthru("docker exec $id sh -c 'cd /tmp/; php /tmp/centreon-build/script/launch_acceptance_test.php " . implode(' ', $argv) . "'");

exec("docker stop $id");
?>
