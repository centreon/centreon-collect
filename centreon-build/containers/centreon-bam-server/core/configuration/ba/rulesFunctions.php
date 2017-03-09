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

function testBAExistence() {				
		global $pearDB;
		global $form;
		
		if (!isset($form))
			return false;
					
		$id = $form->getSubmitValue('ba_id');
		$name = $form->getSubmitValue('ba_name');
		
		$query = "SELECT * FROM mod_bam WHERE name = '".$pearDB->escape($name)."'";
		if (isset($id)) {
			$query .= " AND ba_id != '".$id."'";
		}
		$DBRES = $pearDB->query($query);
		if ($DBRES->numRows())
			return false;
		return true;
}

function checkThresholds() {
	global $form;
		
	if (!isset($form))
		return false;
				
	$warning = $form->getSubmitValue('ba_warning');
	$critical = $form->getSubmitValue('ba_critical');
	if ($warning < $critical)
		return false;
	if ($warning < 0 || $warning > 100 || $critical < 0 || $critical > 100)
		return false;
	return true;	
}

function checkCircularDependencies() {
	global $form;
	
	if (!isset($form))
		return false;
		
	$parents = array();
	$childs = array();
	if ($form->getSubmitValue('dep_bamParents'))
		$parents = $form->getSubmitValue('dep_bamParents');
	if ($form->getSubmitValue('dep_bamChilds'))
		$childs = $form->getSubmitValue('dep_bamChilds');
	$childs = array_flip($childs);
	
	foreach ($parents as $parent)
		if (array_key_exists($parent, $childs))
			return false;
	return true;
}

/**
 * Checks if percentage is in correct range
 *
 * @param int $value
 */
function checkPercentage($value)
{
	if (!is_numeric($value)) {
		return false;
	}
	if ($value < 0 || $value > 100) {
		return false;
	}
	return true;
}

function checkInstanceService()
{
    global $form;
    global $pearDB;
    
    if (!isset($form) || !$form->getSubmitValue('ba_id')) {
        return true;
    }

    if ($form->getSubmitValue('additional_poller') > 0) {
        $instance = $form->getSubmitValue('additional_poller');
    } else {
         return true;
    }
    
    $iIdBa = $form->getSubmitValue('ba_id');
    
    $oKpi = new CentreonBam_Kpi($pearDB);
    $rep = $oKpi->instanceService($iIdBa, $instance);
    return $rep;
}
