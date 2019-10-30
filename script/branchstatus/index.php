<!doctype html>
<html lang="en">
  <head>
    <!-- Required meta tags -->
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, shrink-to-fit=no">

    <!-- Bootstrap CSS -->
    <link rel="stylesheet" type="text/css" href="bootstrap-4.1.3.min.css">

    <title>Branch Status</title>
  </head>
  <body>
    <div class="container container-fluid">
      <h1>Branch Status</h1>

      <h2>20.04</h2>

      <table class="table table-hover table-sm" style="margin-top:50px">
        <thead class="thead-dark">
          <th scope="col">Project</th>
          <th scope="col">Branch</th>
          <th scope="col">Status</th>
        </thead>
        <tbody>
<?php

function printProjectStatus($projects)
{
  foreach ($projects as $project) {
    // Retrieve project status from Jenkins.
    $ch = curl_init();
    curl_setopt(
      $ch,
      CURLOPT_URL,
      'https://jenkins.centreon.com/job/' . $project['name'] . '/job/' .
      $project['branch'] . '/lastBuild/api/json'
    );
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_USERPWD, 'mkermagoret:1137c11b1b163d762610e4250e078582d7');
    $job = json_decode(curl_exec($ch), true);

    // Print project line.
    $lineStyle = ($job['result'] === 'SUCCESS') ? 'alert-success' : 'alert-danger';
    $jobLink = 'https://jenkins.centreon.com/job/' . $project['name'] . '/job/' . $project['branch'];
    echo '<tr class="' . $lineStyle . '">' .
         '  <td><a href="' . $jobLink . '">' . $project['name'] . '</a></td>' .
         '  <td>' . $project['branch'] . '</td>' .
         '  <td>' . $job['result'] . '</td>' .
         '</tr>';

    // Clone connection.
    curl_close($ch);
  }
}

$projects2004 = array(
  array('name' => 'centreon-autodiscovery', 'branch' => 'master'),
  array('name' => 'centreon-awie', 'branch' => 'master'),
  array('name' => 'centreon-broker', 'branch' => 'master'),
  array('name' => 'centreon-clib', 'branch' => 'master'),
  array('name' => 'centreon-connector', 'branch' => 'master'),
  array('name' => 'centreon-dsm', 'branch' => 'master'),
  array('name' => 'centreon-engine', 'branch' => 'master'),
  array('name' => 'centreon-export', 'branch' => 'master'),
  array('name' => 'centreon-license-manager', 'branch' => 'master'),
  array('name' => 'centreon-open-tickets', 'branch' => 'master'),
  array('name' => 'centreon-pp-manager', 'branch' => 'master'),
  array('name' => 'centreon-react-components', 'branch' => 'master'),
  array('name' => 'centreon-web', 'branch' => 'master'),
  array('name' => 'centreon-bam', 'branch' => 'master'),
  array('name' => 'centreon-map', 'branch' => 'master'),
  array('name' => 'centreon-bi-engine', 'branch' => 'master'),
  array('name' => 'centreon-bi-etl', 'branch' => 'master'),
  array('name' => 'centreon-bi-report', 'branch' => 'master'),
  array('name' => 'centreon-bi-reporting-server', 'branch' => 'master'),
  array('name' => 'centreon-bi-server', 'branch' => 'master')
);
printProjectStatus($projects2004);

?>
        </tbody>
      </table>

      <h2>19.10</h2>

      <table class="table table-hover table-sm" style="margin-top:50px">
        <thead class="thead-dark">
          <th scope="col">Project</th>
          <th scope="col">Branch</th>
          <th scope="col">Status</th>
        </thead>
        <tbody>
<?php

function printProjectStatus($projects)
{
  foreach ($projects as $project) {
    // Retrieve project status from Jenkins.
    $ch = curl_init();
    curl_setopt(
      $ch,
      CURLOPT_URL,
      'https://jenkins.centreon.com/job/' . $project['name'] . '/job/' .
      $project['branch'] . '/lastBuild/api/json'
    );
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_USERPWD, 'mkermagoret:1137c11b1b163d762610e4250e078582d7');
    $job = json_decode(curl_exec($ch), true);

    // Print project line.
    $lineStyle = ($job['result'] === 'SUCCESS') ? 'alert-success' : 'alert-danger';
    $jobLink = 'https://jenkins.centreon.com/job/' . $project['name'] . '/job/' . $project['branch'];
    echo '<tr class="' . $lineStyle . '">' .
         '  <td><a href="' . $jobLink . '">' . $project['name'] . '</a></td>' .
         '  <td>' . $project['branch'] . '</td>' .
         '  <td>' . $job['result'] . '</td>' .
         '</tr>';

    // Clone connection.
    curl_close($ch);
  }
}

$projects1910 = array(
  array('name' => 'centreon-autodiscovery', 'branch' => '19.10.x'),
  array('name' => 'centreon-awie', 'branch' => '19.10.x'),
  array('name' => 'centreon-broker', 'branch' => '19.10.x'),
  array('name' => 'centreon-clib', 'branch' => '19.10.x'),
  array('name' => 'centreon-connector', 'branch' => '19.10.x'),
  array('name' => 'centreon-dsm', 'branch' => '19.10.x'),
  array('name' => 'centreon-engine', 'branch' => '19.10.x'),
  array('name' => 'centreon-export', 'branch' => '19.10.x'),
  array('name' => 'centreon-license-manager', 'branch' => '19.10.x'),
  array('name' => 'centreon-open-tickets', 'branch' => '19.10.x'),
  array('name' => 'centreon-pp-manager', 'branch' => '19.10.x'),
  array('name' => 'centreon-react-components', 'branch' => '19.10.x'),
  array('name' => 'centreon-web', 'branch' => '19.10.x'),
  array('name' => 'centreon-bam', 'branch' => '19.10.x'),
  array('name' => 'centreon-map', 'branch' => '19.10.x'),
  array('name' => 'centreon-bi-engine', 'branch' => '19.10.x'),
  array('name' => 'centreon-bi-etl', 'branch' => '19.10.x'),
  array('name' => 'centreon-bi-report', 'branch' => '19.10.x'),
  array('name' => 'centreon-bi-reporting-server', 'branch' => '19.10.x'),
  array('name' => 'centreon-bi-server', 'branch' => '19.10.x')
);
printProjectStatus($projects1910);

?>
        </tbody>
      </table>

      <h2>19.04</h2>

      <table class="table table-hover table-sm" style="margin-top:50px">
        <thead class="thead-dark">
          <th scope="col">Project</th>
          <th scope="col">Branch</th>
          <th scope="col">Status</th>
        </thead>
        <tbody>
<?php

$projects1904 = array(
  array('name' => 'centreon-autodiscovery', 'branch' => '19.04.x'),
  array('name' => 'centreon-awie', 'branch' => '19.04.x'),
  array('name' => 'centreon-broker', 'branch' => '19.04.x'),
  array('name' => 'centreon-clib', 'branch' => '19.04'),
  array('name' => 'centreon-connector', 'branch' => '19.04.x'),
  array('name' => 'centreon-dsm', 'branch' => '19.04.x'),
  array('name' => 'centreon-engine', 'branch' => '19.04.x'),
  array('name' => 'centreon-export', 'branch' => '19.04.x'),
  array('name' => 'centreon-license-manager', 'branch' => '19.04.x'),
  array('name' => 'centreon-open-tickets', 'branch' => '19.04.x'),
  array('name' => 'centreon-pp-manager', 'branch' => '19.04.x'),
  array('name' => 'centreon-react-components', 'branch' => '19.04.x'),
  array('name' => 'centreon-web', 'branch' => '19.04.x'),
  array('name' => 'centreon-bam', 'branch' => '19.04.x'),
  array('name' => 'centreon-map', 'branch' => '19.04.x'),
  array('name' => 'centreon-bi-engine', 'branch' => '19.04.x'),
  array('name' => 'centreon-bi-etl', 'branch' => '19.04.x'),
  array('name' => 'centreon-bi-report', 'branch' => '19.04.x'),
  array('name' => 'centreon-bi-reporting-server', 'branch' => '19.04.x'),
  array('name' => 'centreon-bi-server', 'branch' => '19.04.x')
);
printProjectStatus($projects1904);

?>
        </tbody>
      </table>

      <h2>18.10</h2>

      <table class="table table-hover table-sm" style="margin-top:50px">
        <thead class="thead-dark">
          <th scope="col">Project</th>
          <th scope="col">Branch</th>
          <th scope="col">Status</th>
        </thead>
        <tbody>
<?php

$projects1810 = array(
  array('name' => 'centreon-autodiscovery', 'branch' => '18.10.x'),
  array('name' => 'centreon-awie', 'branch' => '18.10.x'),
  array('name' => 'centreon-broker', 'branch' => '18.10.x'),
  array('name' => 'centreon-clib', 'branch' => '18.10'),
  array('name' => 'centreon-connector', 'branch' => '18.10.x'),
  array('name' => 'centreon-engine', 'branch' => '18.10.x'),
  array('name' => 'centreon-export', 'branch' => '18.10.x'),
  array('name' => 'centreon-license-manager', 'branch' => '18.10.x'),
  array('name' => 'centreon-open-tickets', 'branch' => '18.10.x'),
  array('name' => 'centreon-pp-manager', 'branch' => '18.10.x'),
  array('name' => 'centreon-web', 'branch' => '18.10.x'),
  array('name' => 'centreon-bam', 'branch' => '18.10.x'),
  array('name' => 'centreon-map', 'branch' => '18.10.x'),
  array('name' => 'centreon-bi-engine', 'branch' => '18.10.x'),
  array('name' => 'centreon-bi-etl', 'branch' => '18.10.x'),
  array('name' => 'centreon-bi-report', 'branch' => '18.10.x'),
  array('name' => 'centreon-bi-reporting-server', 'branch' => '18.10.x'),
  array('name' => 'centreon-bi-server', 'branch' => '18.10.x')
);
printProjectStatus($projects1810);

?>
        </tbody>
      </table>

      <h2>3.4</h2>

      <table class="table table-hover table-sm" style="margin-top:50px">
        <thead class="thead-dark">
          <th scope="col">Project</th>
          <th scope="col">Branch</th>
          <th scope="col">Status</th>
        </thead>
        <tbody>
<?php

$projects34 = array(
  array('name' => 'centreon-autodiscovery', 'branch' => '2.4.x'),
  array('name' => 'centreon-awie', 'branch' => '1.0.x'),
  array('name' => 'centreon-broker', 'branch' => '3.0.x'),
  array('name' => 'centreon-clib', 'branch' => '1.4'),
  array('name' => 'centreon-engine', 'branch' => '1.8.x'),
  array('name' => 'centreon-export', 'branch' => '2.3.x'),
  array('name' => 'centreon-license-manager', 'branch' => '1.2.x'),
  array('name' => 'centreon-open-tickets', 'branch' => '1.2.x'),
  array('name' => 'centreon-poller-display', 'branch' => 'master'),
  array('name' => 'centreon-pp-manager', 'branch' => '2.4.x'),
  array('name' => 'centreon-web', 'branch' => '2.8.x'),
  array('name' => 'centreon-bam', 'branch' => '3.6.x'),
  array('name' => 'centreon-map', 'branch' => '4.4.x')
);
printProjectStatus($projects34);

?>
        </tbody>
      </table>

      <h2>Common</h2>

      <table class="table table-hover table-sm" style="margin-top:50px">
        <thead class="thead-dark">
          <th scope="col">Project</th>
          <th scope="col">Branch</th>
          <th scope="col">Status</th>
        </thead>
        <tbody>
<?php

$projectsCommon = array(
  array('name' => 'centreon-hub', 'branch' => 'master'),
  array('name' => 'centreon-imp-portal-api', 'branch' => 'master'),
  array('name' => 'centreon-plugins', 'branch' => 'master'),
  array('name' => 'centreon-plugin-packs', 'branch' => 'master'),
  array('name' => 'centreon-vmware', 'branch' => 'master')
);
printProjectStatus($projectsCommon);

?>
    </div>
  </body>
</html>
