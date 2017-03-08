<?php
/*
 * MERETHIS
 *
 * Source Copyright 2005-2012 MERETHIS
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more information : contact@merethis.com
 *
 */
require_once("../../../../centreon-bam-server.conf.php");
if (file_exists($centreon_path . "www/class/Oreon.class.php")) {
    require_once($centreon_path . "www/class/Oreon.class.php");
}
require_once("../../../common/header.php");
require_once("../../../common/functions.php");

if (!isset($_GET['sid'])) {
    exit();
}

$db = new CentreonBam_Db();
$buffer = new CentreonBam_Xml();
$utils = new CentreonBam_Utils($db);
$boolean = new CentreonBam_Boolean($db, new CentreonBam_Db('centstorage'));

$statusArr = array(0 =>'OK', 1 => 'Warning', 2 => 'Critical', 3 => 'Unknown');

$buffer->startElement("root");
$buffer->writeElement('simtitle', _('Status simulation'));
$buffer->writeElement('simModeLabeL', _('Simulation mode'));

try {
    $mode = 0;
    $simStatus = array();
    if (isset($_GET['sim'])) {
        $mode = 1;
        $simStatus = $_GET['sim'];
    }
    $result = $boolean->evalExpr($_GET['expr'], $simStatus, true);
    $buffer->writeElement('result', $result ? 'true' : 'false');
    $buffer->writeElement('valid', 'true');
    $buffer->writeElement('mode', $mode);
    $resources = $boolean->getResources();
    $i = 0;
    foreach ($resources as $resource => $status) {
        $buffer->startElement('resource');
        $buffer->writeAttribute('name', $resource, false);
        $buffer->writeAttribute('status', $status);
        $buffer->writeAttribute('statuslabel', $statusArr[$status]);
        $buffer->writeAttribute('trStyle', ($i % 2) ? 'list_two' : 'list_one');
        $buffer->endElement();
        $i++;
    }
} catch (Exception $e) {
    $buffer->writeElement('result', $e->getMessage(), false);
    $buffer->writeElement('valid', 'false');
}
$buffer->endElement();

header('Content-Type: text/xml');
header('Pragma: no-cache');
header('Expires: 0');
header('Cache-Control: no-cache, must-revalidate');
$buffer->output();
?>
