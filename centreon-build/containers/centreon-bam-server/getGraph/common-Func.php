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

function bam_getDefaultDS($metric_name)	{
	global $pearDB;
	$ds = array();
	if ($metric_name == "BA_Downtime") {
		$filter = "BA_Downtime";
	} else if ($metric_name == "BA_Level") {
		$filter = "BA_Level";
	}

	$DBRESULT = $pearDB->query("SELECT compo_id FROM giv_components_template WHERE ds_name LIKE '".$filter."' LIMIT 1");
	if (PEAR::isError($DBRESULT)) {
		print "DB Error : ".$DBRESULT->getDebugInfo()."<br />";
	}
	if ($DBRESULT->numRows())	{
		$ds = $DBRESULT->fetchRow();
		return $ds["compo_id"];
	} else {
		$DBRESULT = $pearDB->query("SELECT compo_id FROM giv_components_template WHERE default_tpl1 = '1' LIMIT 1");
		if (PEAR::isError($DBRESULT)) {
			print "DB Error : ".$DBRESULT->getDebugInfo()."<br />";
		}
		if ($DBRESULT->numRows()) {
			$ds = $DBRESULT->fetchRow();
			return $ds["compo_id"];
		}
	}
	return NULL;
}
