<?php

function should_use_docker_machine() {
  $os = php_uname('s');

  if ($os == 'Windows NT' || $os == 'Darwin')
    return (true);
  else
    return (false);
}

// Copy directory recursively
function xcopy($source, $dest) {
  exec("cp -r '$source' '$dest'", $output, $return);
  if ($return == 0)
    return (true);
  else
    return (false);
}

// Replace all the elements of a file
function replace_in_file($in, $out, $to_replace) {
  $str = file_get_contents($in);
  foreach ($to_replace as $content) {
    $str = str_replace($content["from"], $content["to"], $str);
  }
  file_put_contents($out, $str);

  return (true);
}

function get_project_files($project_name) {
  global $distrib;
  global $centreon_build_dir;

  $project_files["web"]["dev"] = "$centreon_build_dir/jobs/containers/mon-containers-web-dev.sh";
  $project_files["web"]["input_directory"] = "centreon";
  $project_files["web"]["compose-in"] = "$centreon_build_dir/containers/web/docker-compose.yml.in";
  $project_files["web"]["compose-out"] = "mon-web-dev.yml";
  $project_files["web"]["compose-replace"][0]["from"] = "@WEB_IMAGE@";
  $project_files["web"]["compose-replace"][0]["to"] = "mon-web-dev:$distrib";

  $project_files["lm"]["dev"] = "$centreon_build_dir/jobs/containers/mon-containers-lm-dev.sh";
  $project_files["lm"]["input_directory"] = "centreon-license-manager";
  $project_files["lm"]["compose-in"] = "$centreon_build_dir/containers/middleware/docker-compose-web.yml.in";
  $project_files["lm"]["compose-out"] = "mon-lm-dev.yml";
  $project_files["lm"]["compose-replace"][0]["from"] = "@WEB_IMAGE@";
  $project_files["lm"]["compose-replace"][0]["to"] = "mon-lm-dev:$distrib";
  $project_files["lm"]["compose-replace"][1]["from"] = "@MIDDLEWARE_IMAGE@";
  $project_files["lm"]["compose-replace"][1]["to"] = "ci.int.centreon.com:5000/mon-middleware:$distrib";

  $project_files["ppm"]["dev"] = "$centreon_build_dir/jobs/containers/mon-containers-ppm-dev.sh";
  $project_files["ppm"]["input_directory"] = "centreon-import";
  $project_files["ppm"]["compose-in"] = "$centreon_build_dir/containers/middleware/docker-compose-web.yml.in";
  $project_files["ppm"]["compose-out"] = "mon-ppm-dev.yml";
  $project_files["ppm"]["compose-replace"][0]["from"] = "@WEB_IMAGE@";
  $project_files["ppm"]["compose-replace"][0]["to"] = "mon-ppm-dev:$distrib";
  $project_files["ppm"]["compose-replace"][1]["from"] = "@MIDDLEWARE_IMAGE@";
  $project_files["ppm"]["compose-replace"][1]["to"] = "ci.int.centreon.com:5000/mon-middleware:$distrib";

  $project_files["middleware"]["dev"] = "$centreon_build_dir/jobs/containers/mon-containers-middleware-dev.sh";
  $project_files["middleware"]["input_directory"] = "centreon-imp-portal-api";
  $project_files["middleware"]["compose-in"] = "$centreon_build_dir/containers/middleware/docker-compose-standalone.yml.in";
  $project_files["middleware"]["compose-out"] = "mon-middleware-dev.yml";
  $project_files["middleware"]["compose-replace"][0]["from"] = "@MIDDLEWARE_IMAGE@";
  $project_files["middleware"]["compose-replace"][0]["to"] = "mon-middleware-dev:$distrib";

  $project_files["ppe"]["dev"] = "$centreon_build_dir/jobs/containers/mon-containers-ppe-dev.sh";
  $project_files["ppe"]["input_directory"] = "centreon-export";
  $project_files["ppe"]["compose-in"] = "$centreon_build_dir/containers/web/docker-compose.yml.in";
  $project_files["ppe"]["compose-out"] = "mon-ppe-dev.yml";
  $project_files["ppe"]["compose-replace"][0]["from"] = "@WEB_IMAGE@";
  $project_files["ppe"]["compose-replace"][0]["to"] = "mon-ppe-dev:$distrib";
  $project_files["ppe"]["compose-replace"][1]["from"] = "@MIDDLEWARE_IMAGE@";
  $project_files["ppe"]["compose-replace"][1]["to"] = "ci.int.centreon.com:5000/mon-middleware:$distrib";

  return ($project_files[$project_name]);
}

function call_exit(int $signo)
{
    exit(1);
}

// Set signal handlers.
pcntl_signal(SIGTERM, 'call_exit');
pcntl_signal(SIGINT, 'call_exit');

// Parse the options.
$opts = getopt("d:h");
array_shift($argv);
if (isset($opts['h'])) {
    echo "USAGE: acceptance.php [-h] [-d distrib] [feature1 [feature2 [...] ] ]\n";
    echo "\n";
    echo "    -h  Print this help.\n";
    echo "    -d  Distribution used to run tests. Can be one of centos6 (default) or centos7.\n";
    echo "    Feature files are optional. By default all of them will be run.\n";
    return (0);
}
if (isset($opts['d'])) {
    $distrib = $opts['d'];
    array_shift($argv);
    array_shift($argv);
}
else {
    $distrib = 'centos6';
}
$centreon_build_dir = dirname(__FILE__) . '/..';
$source_dir = realpath('.');

// Load configuration file.
echo "[1/5] Loading configuration file...\n";
require_once($centreon_build_dir . '/conf/acceptance.conf.php');
if (!defined('_GITHUB_TOKEN_') || _GITHUB_TOKEN_ == "") {
    echo "Please fill your GitHub token in acceptance.conf.php file.\n";
    return (1);
}

// Load project settings.
echo "[2/5] Loading project settings...\n";
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
case 'centreon-ppm':
    $project = 'ppm';
    break ;
case 'centreon':
case 'centreon-web':
    $project = 'web';
    break ;
};
$project_files = get_project_files($project);
if (!isset($project_files)) {
  echo "Project $project not supported: supported projects are 'web', 'lm', 'ppe', 'ppm', 'middleware'.\n";
  return (1);
}

// Replace the compose .yml.in.
echo "[3/5] Generating execution files...\n";
if (!replace_in_file($project_files["compose-in"], $project_files["input_directory"] . "/" . $project_files["compose-out"], $project_files["compose-replace"])) {
    echo "Couldn't replace in the file " . $project_files["compose"] . "\n";
    return (1);
}

// Execute the dev container script.
echo "[4/5] Building development container from current sources...\n";
passthru($project_files["dev"] . " " . $distrib, $return_var);
if ($return_var != 0) {
    echo 'Could not build development container of ' . $project . "\n";
    return (1);
}

// Start acceptance tests.
echo "[5/5] Finally running acceptance tests...\n";
passthru("ls 'features'/*.feature | parallel -j1 -u ./vendor/bin/behat --strict \"{}\"", $return_var);

?>
