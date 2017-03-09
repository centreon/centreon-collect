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

function testBAGroupExistence() {				
		global $pearDB;
		global $form;
		
		if (!isset($form))
			return false;
					
		$id = $form->getSubmitValue('ba_group_id');
		$name = $form->getSubmitValue('ba_group_name');
		
		$query = "SELECT * FROM mod_bam_ba_groups WHERE ba_group_name = '".$pearDB->escape($name)."'";
		if (isset($id)) {
			$query .= " AND id_ba_group != '".$id."'";
		}
		$DBRES = $pearDB->query($query);
		if ($DBRES->numRows())
			return false;
		return true;
}

