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

require_once("../../../centreon-bam-server.conf.php");
require_once("../../common/functions.php");

if (file_exists($centreon_path."www/class/Session.class.php")) {
    require_once($centreon_path . "www/class/Session.class.php");
}
if (file_exists($centreon_path."www/class/Oreon.class.php")) {
    require_once($centreon_path . "www/class/Oreon.class.php");
}
if (file_exists($centreon_path."www/class/other.class.php")) {
    require_once($centreon_path . "www/class/other.class.php");
}
require_once("../../common/header.php");

if (!isset($varlib) || $varlib == "") {
    $varlib = "/var/lib/centreon";
}

if (!isset($_GET['sid']) || !isset($_GET['ba_list']) || !isset($_GET['action']) || !isset($oreon))
    exit();

$session_id = $_GET['sid'];
$ba_list_str = $_GET['ba_list'];
$action = $_GET['action'];

$ba_list = array();
$ba_list = explode(",", $ba_list_str);

$command_prefix = "";
switch ($action) {
    case "immediate_check" :
        $command_prefix .= "SCHEDULE_SVC_CHECK;";
	    break;
    case "force_immediate_check" :
        $command_prefix .= "SCHEDULE_FORCED_SVC_CHECK;";
	    break;
    case "acknowledge" :
        $command_prefix .= "ACKNOWLEDGE_SVC_PROBLEM;";
	    break;
    case "remove_acknowledge" :
        $command_prefix .= "REMOVE_SVC_ACKNOWLEDGEMENT;";
	    break;
    case "enable_notif" :
        $command_prefix .= "ENABLE_SVC_NOTIFICATIONS;";
	    break;
    case "disable_notif" :
        $command_prefix .= "DISABLE_SVC_NOTIFICATIONS;";
	    break;
    case "remove_downtime" :
        $command_prefix .= "DEL_SVC_DOWNTIME;";
        break;
}

$db = new CentreonBam_Db();
$utils = new CentreonBam_Utils($db);
$dbmon = new CentreonBam_Db('centstorage');

$hosts = getCentreonBaHostNamesWithLocation($db);

$str = '';
foreach ($hosts as $host) {
    foreach ($ba_list as $ba_id) {
    if ($action == "remove_downtime") {
        $sql = "SELECT internal_id 
                FROM downtimes d, services s, hosts h
                WHERE d.service_id = s.service_id
                AND s.host_id = h.host_id
                AND s.description = 'ba_{$ba_id}'
                AND h.name = '{$host['host_name']}'";
        $res = $dbmon->query($sql);
        while ($row = $res->fetchRow()) {
            $command = $command_prefix . $row['internal_id'];
            $command = "[".time()."] " . $command;
            $command = trim($command);
            if ($host['localhost'] == '1') {
                $str = "echo '" . $command . "\n' >> " . $oreon->Nagioscfg["command_file"];
            } else {
                $str = "echo 'EXTERNALCMD:" . $host['poller_id'] . ":" . $command . "\n' >> " . $varlib . "/centcore.cmd";
            }
            passthru($str);
        }    
    } else {
        $command_prefix2 = $command_prefix . $host['host_name'] . ';';
        $command = "ba_" . $ba_id . ";";
        if ($action == "immediate_check" || $action == "force_immediate_check") {
            $command .= time();
        } elseif ($action == "acknowledge") {
            $command .= "0;0;1;";
            $command .= $oreon->user->alias . ";Acknowledged by " . $oreon->user->alias;
        }
        $command = $command_prefix2 . $command;
        $command = trim('[' . time() . '] ' . $command);
        if ($host['localhost'] == '1') {
            $str = "echo '" . $command . "\n' >> " . $oreon->Nagioscfg["command_file"];
        } else {
            $str = "echo 'EXTERNALCMD:" . $host['poller_id'] . ":" . $command . "\n' >> " . $varlib . "/centcore.cmd";
        }
        passthru($str);
    }
    }
}

$buffer = new CentreonBAM_XML();
$buffer->startElement("root");
$buffer->writeElement("result", $str);
$buffer->endElement();

header('Content-Type: text/xml');
header('Pragma: no-cache');
header('Expires: 0');
header('Cache-Control: no-cache, must-revalidate');

// Display Data
$buffer->output();

