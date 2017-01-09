<?php

// Connect to database.
require_once '/etc/centreon/centreon.conf.php';
$dsn = 'mysql:dbname=' . $conf_centreon['dbcstg'] . ';host=' . $conf_centreon['hostCentstorage'] . ';port=' . $conf_centreon['port'];
$db = new \PDO(
    $dsn,
    $conf_centreon['user'],
    $conf_centreon['password']
);
$db->setAttribute(\PDO::ATTR_ERRMODE, \PDO::ERRMODE_EXCEPTION);

// Load services and their states.
$services = array();
$query =
    'SELECT h.instance_id, h.name, s.description, s.state' .
    '  FROM services AS s' .
    '  LEFT JOIN hosts AS h' .
    '  ON s.host_id=h.host_id' .
    '  WHERE s.enabled=1';
foreach ($db->query($query) as $row) {
    $services[] = array(
        'poller' => $row['instance_id'],
        'host' => $row['name'],
        'service' => $row['description'],
        'state' => $row['state']
    );
}

// Randomly change services' states.
$keys = array_rand($services, 15);
foreach ($keys as $key) {
    // Generate new state.
    do {
        $newstate = rand(0, 3);
    } while ($newstate == $services[$key]);

    // Send service state change command.
    echo
        $services[$key]['poller'] . ';PROCESS_SERVICE_CHECK_RESULT;' .
        $services[$key]['host'] . ';' . $services[$key]['service'] .
        ';' . $newstate . ';svcpassivechange (benchmark cron)' . "\n";
}
