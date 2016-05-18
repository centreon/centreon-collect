<?php

require_once(dirname(__FILE__) . '/../conf/acceptance.conf.php');
$centreon_build_directory = dirname(__FILE__) . '/..';

if (!defined('_GITHUB_TOKEN_') || _GITHUB_TOKEN_ == "") {
    echo "Please fill your github token in acceptance.conf.php file\n";
    return (-1);
}

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
  global $arch;
  global $centreon_build_directory;

  $project_files["web"]["dev"] = "$centreon_build_directory/jobs/containers/mon-containers-web-dev.sh";
  $project_files["web"]["input_directory"] = "centreon";
  $project_files["web"]["compose-in"] = "$centreon_build_directory/containers/web/docker-compose.yml.in";
  $project_files["web"]["compose-out"] = "$centreon_build_directory/containers/web/docker-compose.yml";
  $project_files["web"]["compose-replace"][0]["from"] = "@WEB_IMAGE@";
  $project_files["web"]["compose-replace"][0]["to"] = "mon-web-dev:centos$arch";

  $project_files["lm"]["dev"] = "$centreon_build_directory/jobs/containers/mon-containers-lm-dev.sh";
  $project_files["lm"]["input_directory"] = "centreon-license-manager";
  $project_files["lm"]["compose-in"] = "$centreon_build_directory/containers/middleware/docker-compose-web.yml.in";
  $project_files["lm"]["compose-out"] = "mon-lm-dev.yml";
  $project_files["lm"]["compose-replace"][0]["from"] = "@WEB_IMAGE@";
  $project_files["lm"]["compose-replace"][0]["to"] = "mon-lm-dev:centos$arch";
  $project_files["lm"]["compose-replace"][1]["from"] = "@MIDDLEWARE_IMAGE@";
  $project_files["lm"]["compose-replace"][1]["to"] = "ci.int.centreon.com:5000/mon-middleware:centos$arch";

  $project_files["ppm"]["dev"] = "$centreon_build_directory/jobs/containers/mon-containers-ppm-dev.sh";
  $project_files["ppm"]["input_directory"] = "centreon-import";
  $project_files["ppm"]["compose-in"] = "$centreon_build_directory/containers/middleware/docker-compose-web.yml.in";
  $project_files["ppm"]["compose-out"] = "mon-ppm-dev.yml";
  $project_files["ppm"]["compose-replace"][0]["from"] = "@WEB_IMAGE@";
  $project_files["ppm"]["compose-replace"][0]["to"] = "mon-ppm-dev:centos$arch";
  $project_files["ppm"]["compose-replace"][1]["from"] = "@MIDDLEWARE_IMAGE@";
  $project_files["ppm"]["compose-replace"][1]["to"] = "ci.int.centreon.com:5000/mon-middleware:centos$arch";

  $project_files["middleware"]["dev"] = "$centreon_build_directory/jobs/containers/mon-containers-middleware-dev.sh";
  $project_files["middleware"]["input_directory"] = "centreon-imp-portal-api";
  $project_files["middleware"]["compose-in"] = "$centreon_build_directory/containers/middleware/docker-compose-standalone.yml.in";
  $project_files["middleware"]["compose-out"] = "mon-middleware-dev.yml";
  $project_files["middleware"]["compose-replace"][0]["from"] = "@MIDDLEWARE_IMAGE@";
  $project_files["middleware"]["compose-replace"][0]["to"] = "mon-middleware-dev:centos$arch";

  $project_files["ppe"]["dev"] = "$centreon_build_directory/jobs/containers/mon-containers-ppe-dev.sh";
  $project_files["ppe"]["input_directory"] = "centreon-export";
  $project_files["ppe"]["compose-in"] = "$centreon_build_directory/containers/web/docker-compose.yml.in";
  $project_files["ppe"]["compose-out"] = "mon-ppe-dev.yml";
  $project_files["ppe"]["compose-replace"][0]["from"] = "@WEB_IMAGE@";
  $project_files["ppe"]["compose-replace"][0]["to"] = "mon-ppe-dev:centos$arch";
  $project_files["ppe"]["compose-replace"][1]["from"] = "@MIDDLEWARE_IMAGE@";
  $project_files["ppe"]["compose-replace"][1]["to"] = "ci.int.centreon.com:5000/mon-middleware:centos$arch";

  return ($project_files[$project_name]);
}

// Parse the options.
$opts = getopt("p:a:s:f::");
if (!isset($opts["p"]) || !isset($opts["s"]) || !isset($opts["a"])) {
  echo "usage: launch_acceptance_test -f[feature_file] -p project_name -a centos6|centos7 -s source_directory\n";
  return (0);
}

// Get absolute paths.
$opts["s"] = realpath($opts["s"]);
if (isset($opts["f"])) {
  $opts["f"] = realpath($opts["f"]);
}

// Get the feature file[s]
$feature;
if (isset($opts["f"])) {
  $pathinfo = pathinfo($opts["f"]);
  $feature = $pathinfo['basename'];
} else {
  $feature = "*.feature";
}

// Get the architecture.
$archs["centos6"] = "6";
$archs["centos7"] = "7";
$arch = $archs[$opts["a"]];
if (!isset($arch)) {
  echo "unrecognized architecture '$arch': 'centos6' or 'centos7' allowed";
  return (-1);
}

// Get the project files.
$project_name = $opts["p"];
$source_directory = $opts["s"];
$project_files = get_project_files($project_name);
if (!isset($project_files)) {
  echo "project $project_name not supported: supported projects are 'web', 'lm', 'ppe', 'ppm', 'middleware'\n";
  return (0);
}

// Get tmp directory.
$tmp_directory = sys_get_temp_dir() . '/launch_acceptance';
echo "creating tmp directory...\n";
exec("rm -rf $tmp_directory");
mkdir($tmp_directory);

// Copy the source directory to the tmp directory.
echo "copying sources to tmp directory...\n";
if (!xcopy($source_directory, $tmp_directory . "/" . $project_files["input_directory"])) {
  echo "couldn't copy \"$source_directory\" to \"" . $tmp_directory . "/" . $project_files["input_directory"] . "\"\n";
  return (-1);
}

// Copy centreon-build to the tmp directory.
echo "copying centreon build to tmp directory...\n";
if (!xcopy($centreon_build_directory, $tmp_directory . "/centreon-build")) {
  echo "couldn't copy \"centreon-build\" to \"$tmp_directory\"";
  return (-1);
}

// Chdir to tmp directory.
if (!chdir($tmp_directory)) {
  echo "couldn't chdir into $tmp_directory";
  return (-1);
}

// Replace the compose .yml.in.
if (isset($project_files["compose-in"])) {
  echo "replacing compose.yml\n";
  if (!replace_in_file($project_files["compose-in"], $project_files["input_directory"] . "/" . $project_files["compose-out"], $project_files["compose-replace"])) {
    echo "couldn't replace in the file " . $project_files["compose"] . "\n";
    return (-1);
  }
}

echo "Building dev container...\n";

// Execute the dev container script.
passthru($project_files["dev"] . " " . $arch, $return_var);
if ($return_var != 0) {
  echo $project_files["dev"] . " error: " . $return_var . "\n";
  return (-1);
}

echo "Running composer install...\n";

// Start acceptance test.
chdir($project_files["input_directory"]);
exec('composer config github-oauth.github.com ' . _GITHUB_TOKEN_);
exec('composer install');
exec('composer update');
echo "Starting acceptance tests...\n";
passthru("ls 'features'/$feature | parallel -j1 -u ./vendor/bin/behat --strict \"{}\"", $return_var);
if ($return_var != 0) {
  echo "acceptance error: " . $return_var . "\n";
  return (-1);
}
?>
