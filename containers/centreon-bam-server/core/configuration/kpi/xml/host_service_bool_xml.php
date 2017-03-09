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

require_once("../../../../centreon-bam-server.conf.php");
if (file_exists($centreon_path . "www/class/Oreon.class.php")) {
    require_once($centreon_path . "www/class/Oreon.class.php");
}
require_once("../../../common/header.php");
require_once("../../../common/functions.php");

if (!isset($_GET['sid']) || !isset($_GET['host_name']) || !$_GET['host_name']) {
    exit();
}

$host_name = $_GET['host_name'];

$pearDB = new CentreonBam_Db();
$buffer = new CentreonBam_Xml();

$buffer->startElement("root");

/*
 * Get HostGroupList Where host is included
 */
$HGID = array();
$DBRESULT = $pearDB->query("SELECT hostgroup_hg_id 
                                    FROM hostgroup_relation hgr, host h
                                    WHERE hgr.host_host_id = h.host_id
                                    AND h.host_name = '" . $pearDB->escape($host_name) . "'");
while ($row = $DBRESULT->fetchRow()) {
    $HGID[] = $row["hostgroup_hg_id"];
}
$DBRESULT->free();
unset($row);

$query = "SELECT service_id, service_description FROM service svc, host_service_relation hsr, host h " .
        "WHERE svc.service_id = hsr.service_service_id " .
        "AND h.host_name = '" . $pearDB->escape($host_name) . "'".
        "AND ((hsr.host_host_id = h.host_id ".
        "AND hostgroup_hg_id IS NULL)";
if (count($HGID)) {
    $str = "";
    foreach ($HGID as $hg_id) {
        if ($str != "") {
            $str .= ',';
        }
        $str .= "'".$hg_id."'";
    }
    $query .= " OR (hostgroup_hg_id IN ($str) AND host_host_id IS NULL)";
}
$query .= ") AND svc.service_register = '1' ".
    "ORDER BY service_description ASC";


$DBRESULT = $pearDB->query($query);
while ($row = & $DBRESULT->fetchRow()) {
    $buffer->startElement("svc");
    $buffer->writeElement("name", slashConvert($row['service_description']), false);
    $buffer->endElement();
}

$buffer->endElement();

header('Content-Type: text/xml');
header('Pragma: no-cache');
header('Expires: 0');
header('Cache-Control: no-cache, must-revalidate');
$buffer->output();
?>