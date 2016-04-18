<?php

// Copy directory recursively
function xcopy($source, $dest) {
  exec("cp -r $source $dest", $output, $return);
  if ($return == 0)
    return (true);
  else
    return (false);
}

function get_project_files($project_name) {
  $project_files["web"]["acceptance"] = "./centreon-build/jobs/web/mon-web-acceptance.sh";
  $project_files["web"]["dev"] = "./centreon-build/jobs/containers/mon-containers-web-dev.sh";
  $project_files["web"]["input_directory"] = "centreon";

  return ($project_files[$project_name]);
}

// Parse the options.
$opts = getopt("p:a:s:d::");
if (!isset($opts["p"]) || !isset($opts["s"]) || !isset($opts["a"])) {
  echo "usage: launch_acceptance_test [-d a_docker_machine] -p project_name -a centos6|centos7 -s source_directory";
  return (0);
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
  echo "project $project_name not supported: supported projects are 'web', 'lm', 'ppe', 'ppm'";
  return (0);
}

// Get tmp directory.
$tmp_directory = sys_get_temp_dir() . '/launch_acceptance';
exec("rm -rf $tmp_directory");
mkdir($tmp_directory);

// Copy the source directory to the tmp directory.
if (!xcopy($source_directory, $tmp_directory . "/" . $project_files["input_directory"])) {
  echo "couldn't copy \"$source_directory\" to \"" . $tmp_directory . "/" . $project_files["input_directory"] . "\"";
  return (-1);
}

// Copy centreon-build to the tmp directory.
if (!xcopy("./centreon-build", $tmp_directory . "/centreon-build")) {
  echo "couldn't copy \"centreon-build\" to \"$tmp_directory\"";
  return (-1);
}

// Chdir to tmp directory.
if (!chdir($tmp_directory)) {
  echo "couldn't chdir into $tmp_directory";
  return (-1);
}

// Execute the dev container script.
exec($project_files["dev"] . " " . $arch, $output, $return_var);
if ($return_var != 0) {
  echo $project_files["dev"] . " error: " . $return_var;
  return (-1);
}

// Execute the acceptance script.
exec($project_files["acceptance"]  . " " . $arch, $output, $return_var);
if ($return_var != 0) {
  echo $project_files["acceptance"] . " error: " . $return_var;
  return (-1);
}

?>