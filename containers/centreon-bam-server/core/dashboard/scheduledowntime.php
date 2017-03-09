<?php
/*
 * Centreon
 *
 * Source Copyright 2005-2016 Centreon
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more informations : contact@centreon.com
 *
 */

global $pearDB;

if (!isset($_POST['ba']) || !isset($_POST['sid']) || !isset($_POST['start']) ||
    !isset($_POST['end']) || !isset($_POST['comments'])) {
    exit;
}

require_once("../../centreon-bam-server.conf.php");
require_once '../common/header.php';
require_once '../common/functions.php';

if (!isset($varlib) || $varlib == "") {
    $varlib = "/var/lib/centreon";
}

if  ($_POST['sid'] != session_id()) {
    exit;
}

$ba = explode(',', $_POST['ba']);
$datetimeExp = '/(\d+)\/(\d+)\/(\d+)\ (\d+):(\d+)/';

if (preg_match($datetimeExp, $_POST['start'], $matches)) {
    list($tmp, $year, $month, $day, $hour, $minute) = $matches;    
    $start = mktime($hour, $minute, "0", $month, $day, $year, -1);    
} else {
    exit;
}

if (preg_match($datetimeExp, $_POST['end'], $matches)) {
    list($tmp, $year, $month, $day, $hour, $minute) = $matches;    
    $end = mktime($hour, $minute, "0", $month, $day, $year, -1);
} else {
    exit;
}

$fixed = 0;
if (isset($_POST['fixed']) && $_POST['fixed'] == 'true') {
    $fixed = 1;
}

$duration = 0;
if (isset($_POST['duration'])) {
    $duration = $_POST['duration'];
}
$author = $oreon->user->get_alias();
$comments = $_POST['comments'];

$hosts = getCentreonBaHostNamesWithLocation($pearDB);

foreach ($hosts as $host) {
    foreach($ba as $baId) {
        $cmd = " SCHEDULE_SVC_DOWNTIME;" . $host['host_name'] . ";ba_" . $baId . ";$start;$end;$fixed;0;$duration;$author;$comments";
        if ($host['localhost'] == '1') {
            $str = "echo '[" . time() . "]" . $cmd . "\n' >> " . $oreon->Nagioscfg["command_file"];
        } else {
            $str = "echo 'EXTERNALCMD:" . $host['poller_id'] . ":[" . time() . "]" . $cmd . "\n' >> " . $varlib . "/centcore.cmd";
        }
        passthru($str);
    }
}
