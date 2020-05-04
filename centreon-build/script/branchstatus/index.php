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

      <table class="table table-hover table-sm" style="margin-top:50px">
        <thead class="thead-dark">
          <th scope="col">Project</th>
<?php

$versions = array('20.04.x', '19.10.x', '19.04.x', '3.4.x');
foreach ($versions as $version) {
  echo '<th scope="col">' . $version . '</th>';
}

?>
        </thead>
        <tbody>
<?php

$projects = array(
  'centreon-autodiscovery' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04.x',
    '3.4.x' => '2.4.x'
  ),
  'centreon-awie' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04.x',
    '3.4.x' => '1.0.x'
  ),
  'centreon-bam' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04.x',
    '3.4.x' => '3.6.x'
  ),
  'centreon-bi-engine' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04.x',
  ),
  'centreon-bi-etl' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04.x',
  ),
  'centreon-bi-report' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04.x',
  ),
  'centreon-bi-reporting-server' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04.x',
  ),
  'centreon-bi-server' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04.x',
  ),
  'centreon-broker' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04.x',
    '3.4.x' => '3.0.x'
  ),
  'centreon-clib' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04',
    '3.4.x' => '1.4'
  ),
  'centreon-connector' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04.x',
  ),
  'centreon-documentation' => array(
    '20.04.x' => 'master'
  ),
  'centreon-dsm' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04.x'
  ),
  'centreon-engine' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04.x',
    '3.4.x' => '1.8.x'
  ),
  'centreon-gorgone' => array(
    '20.04.x' => 'master'
  ),
  'centreon-hub' => array(
    '20.04.x' => 'master',
    '19.10.x' => 'master',
    '19.04.x' => 'master',
    '3.4.x' => 'master'
  ),
  'centreon-imp-portal-api' => array(
    '20.04.x' => 'master',
    '19.10.x' => 'master',
    '19.04.x' => 'master',
    '3.4.x' => 'master'
  ),
  'centreon-license-manager' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04.x',
    '3.4.x' => '1.2.x'
  ),
  'centreon-map' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04.x',
    '3.4.x' => '4.4.x'
  ),
  'centreon-open-tickets' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04.x',
    '3.4.x' => '1.2.x'
  ),
  'centreon-plugins' => array(
    '20.04.x' => 'master',
    '19.10.x' => 'master',
    '19.04.x' => 'master',
    '3.4.x' => 'master'
  ),
  'centreon-plugin-packs' => array(
    '20.04.x' => 'master',
    '19.10.x' => 'master',
    '19.04.x' => 'master',
    '3.4.x' => 'master'
  ),
  'centreon-poller-display' => array(
    '3.4.x' => 'master'
  ),
  'centreon-pp-manager' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04.x',
    '3.4.x' => '2.4.x'
  ),
  'centreon-ui' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04.x'
  ),
  'centreon-vmware' => array(
    '20.04.x' => 'master',
    '19.10.x' => 'master',
    '19.04.x' => 'master',
    '3.4.x' => 'master'
  ),
  'centreon-web' => array(
    '20.04.x' => 'master',
    '19.10.x' => '19.10.x',
    '19.04.x' => '19.04.x',
    '3.4.x' => '2.8.x'
  )
);

foreach ($projects as $project => $branches) {
  // Line start.
  echo '<tr><td>' . $project . '</td>';

  foreach ($versions as $version) {
    // Check if project has this version.
    if (array_key_exists($version, $branches)) {
      // Retrieve project status from Jenkins.
      $ch = curl_init();
      curl_setopt(
        $ch,
        CURLOPT_URL,
        'https://jenkins.centreon.com/job/' . $project . '/job/' .
        $branches[$version] . '/lastBuild/api/json'
      );
      curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
      curl_setopt($ch, CURLOPT_USERPWD, 'mkermagoret:1137c11b1b163d762610e4250e078582d7');
      $job = json_decode(curl_exec($ch), true);

      // Print project/version cell.
      if (empty($job['result'])) {
        $cellStyle = 'alert-warning';
        $jobResult = 'RUNNING';
      } else if ($job['result'] === 'SUCCESS') {
        $cellStyle = 'alert-success';
        $jobResult = 'SUCCESS';
      } else {
        $cellStyle = 'alert-danger';
        $jobResult = $job['result'];
      }
      $jobLink = 'https://jenkins.centreon.com/job/' . $project . '/job/' . $branches[$version];
      echo '<td class="' . $cellStyle . '"><a href="' . $jobLink . '">' . $jobResult . '</a></td>';

      // Clone connection.
      curl_close($ch);
    } else {
      echo '<td>N/A</td>';
    }
  }

  // Line end.
  echo '</tr>';
}

?>
    </div>
  </body>
</html>
