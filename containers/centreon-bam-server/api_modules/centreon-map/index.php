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

if (!isset($authorization) || $authorization == FALSE)
	exit();

try {
    require_once ($centreon_path . "www/modules/centreon-bam-server/api_modules/centreon-map/functions.php");
    /* For displaying radar */
    //require_once ($centreon_path . "www/modules/centreon-bam-server/api_modules/centreon-map/lib/BamExport/BamExport.php");
    require_once ($centreon_path . "www/modules/centreon-bam-server/api_modules/centreon-map/lib/BamExport/Utils.php");
}
catch (Exception $e) {
    $buffer->startElement("error");
	$buffer->writeAttribute("code", "20004");
	$buffer->text("Problem during ojbect initialization : " . $e->getMessage());
	$buffer->endElement();
}

foreach ($xmlString->children() as $child) {
	if ($child->getName() == "request") {
		switch ($child['type']) {
			case "getTree" :
			    $serviceGraphed = array();
			    $child_ba = array();
				getTree($child);
                break;
			case "getStatus" :
				getStatus($child);
                break;
			case "getGraph" :
				getGraph($child);
                break;
			case "getRadar" :
			    getRadar($child);
                break;
			default :
                break;
		}
	}
}
?>
