<?php
  $jobData = json_decode(file_get_contents(__DIR__ . '/jobData.json'), true);
?>
<!doctype html>
<html lang="en">
<head>
  <!-- Required meta tags -->
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />

  <!-- CSS -->
  <link rel="stylesheet" type="text/css" href="materialize.min.css">

  <title><?php echo $jobData['project']['displayname']; ?> Build Artifacts</title>
</head>

<body>
  <div class="container">
    <h1><?php echo $jobData['project']['displayname']; ?> Artifacts</h1>
    <h3 class="blue-text text-darken-3">Informations</h3>
    <div class="row">
      <div class="col s6">
        <table id="project-informations" class="table striped">
          <thead class="teal lighten-2">
            <tr>
              <th>Info</th>
              <th>Value</th>
            </tr>
          </thead>
          <tbody>
<?php
  foreach ($jobData['project'] as $propKey => $propValue) {
?>
            <tr>
              <td><b><?php echo $propKey; ?></b></td><td><?php echo $propValue; ?></td>
            </tr>
<?php
  }
?>
          </tbody>
        </table>
      </div>

      <div class="col s6">
        <table id="version-informations" class="table striped">
          <thead class="teal lighten-2">
            <tr>
              <th>Info</th>
              <th>Value</th>
            </tr>
          </thead>
          <tbody>
<?php
  foreach ($jobData['version'] as $propKey => $propValue) {
?>
            <tr>
              <td><b><?php echo $propKey; ?></b></td><td><?php echo $propValue; ?></td>
            </tr>
<?php
  }
?>
          </tbody>
        </table>
      </div>
    </div>
    <h3 class="blue-text text-darken-3">Artifacts</h3>
    <div class="row">
      <div class="col s12">
        <table id="list-artifacts" class="table striped">
          <thead class="teal lighten-2">
            <th>Artifact</th>
            <th>Flavor</th>
            <th>Value</th>
            </tr>
          </thead>
          <tbody>
<?php
  foreach ($jobData['artifacts'] as $artifact) {
?>
            <tr>
              <td><b><?php echo $artifact['displayname']; ?></b></td>
              <td><?php echo $artifact['flavor']; ?></td>
	      <td><a href="<?php echo $artifact['url'] . '">' . $artifact['url']; ?></a></td>
	    </tr>
<?php
  }
?>
          </tbody>
        </table>
      </div>
    </div>
  </div>
  <script type="text/javascript" src="jquery.min.js"></script>
  <script type="text/javascript" src="app.js"></script>
</body>

</html>
